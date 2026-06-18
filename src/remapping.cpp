#include <time.h>

#include <chrono>
#include <entity/mapping.hpp>
#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/mapping_io.hpp>
#include <io/output_to_log_file.hpp>
#include <io/select_database.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/remapper.hpp>

namespace {
double GetElapsedSeconds(
    const std::chrono::system_clock::time_point& start_time) {
  const auto end_time = std::chrono::system_clock::now();
  return static_cast<double>(
             std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                   start_time)
                 .count()) /
         1000.0;
}
}  // namespace

io::MappingTransformOp ConvertMappingTransformOp(
    remapper::MappingTransformOp transform_op) {
  io::MappingTransformOp result;
  result.row = transform_op.row;
  result.column = transform_op.column;
  result.rotate_op = remapper::RotateOpToString(transform_op.rotate_op);
  return result;
}

int main(int argc, char* argv[]) {
  if (argc != 8) {
    std::cerr << "invalid arguments" << std::endl;
    abort();
  }

  std::string database_dir_path = argv[1];
  std::string dfg_file_path = argv[2];
  std::string mrrg_file_path = argv[3];
  std::string output_dir = argv[4];
  remapper::RemappingMode mode =
      static_cast<remapper::RemappingMode>(std::stoi(argv[5]));
  const double timeout_s = std::stod(argv[6]);
  const int num_available_mappings = std::stoi(argv[7]);
  const auto start_time = std::chrono::system_clock::now();

  assert(std::filesystem::path(database_dir_path).is_absolute());
  assert(std::filesystem::path(mrrg_file_path).is_absolute());
  assert(std::filesystem::path(output_dir).is_absolute());
  assert(std::filesystem::exists(database_dir_path));
  assert(std::filesystem::exists(mrrg_file_path));

  if (!std::filesystem::exists(output_dir)) {
    std::filesystem::create_directories(output_dir);
  };

  const auto mrrg_config =
      io::ReadMRRGFromJsonFile(mrrg_file_path).GetMRRGConfig();

  entity::Database database =
      io::select_database(database_dir_path, mrrg_config, dfg_file_path);
  database.LimitMappingNum(num_available_mappings);
  std::vector<entity::Mapping> mapping_vec = database.GetMappings();

  io::RemapperLogger logger;
  io::RemapperInput input;
  input.database_dir_path = database.GetDatabaseDirPath();
  input.cgra_file_path = mrrg_file_path;
  input.dfg_file_path = dfg_file_path;
  input.output_dir_path = output_dir;
  input.remapper_mode = remapper::RemappingModeToString(mode);
  input.timeout_s = timeout_s;
  input.num_available_mappings = num_available_mappings;
  logger.LogRemapperInput(input);

  if (!database.Exist()) {
    const std::string error_message =
        "No valid database is found for the given MRRG config and DFG.";
    std::cerr << error_message << std::endl;
    logger.LogRemapperFailure(GetElapsedSeconds(start_time), error_message);
    return 0;
  }

  if (mapping_vec.empty()) {
    const std::string error_message =
        "No successful mapping is found in the database.";
    std::cerr << error_message << std::endl;
    logger.LogRemapperFailure(GetElapsedSeconds(start_time), error_message);
    return 0;
  }

  int max_config_num =
      mrrg_config.column * mrrg_config.row * mrrg_config.context_size;

  size_t parallel_num =
      std::floor(max_config_num / database.GetMinMappingOpNum());

  std::shared_ptr<entity::Mapping> result_mapping =
      std::make_shared<entity::Mapping>();

  std::ofstream remapper_exec_file =
      std::ofstream(logger.GetRemapperExecLogFilePath());
  const auto remapping_result =
      remapper::Remapper::Remapping(mapping_vec, mrrg_config, parallel_num,
                                    remapper_exec_file, mode, timeout_s);

  if (remapping_result.result_mapping_id_vec.empty()) {
    const std::string error_message = "Remapper produced no mapping result.";
    std::cerr << error_message << std::endl;
    logger.LogRemapperFailure(remapping_result.remapping_time_s, error_message);
    return 0;
  }

  std::set<int> result_mapping_id_set;
  std::vector<entity::Mapping> result_mapping_vec;
  for (const auto& mapping_id : remapping_result.result_mapping_id_vec) {
    result_mapping_id_set.insert(mapping_id);
    result_mapping_vec.push_back(mapping_vec[mapping_id]);
  }
  *result_mapping = remapper::MappingConcater(
      result_mapping_vec, remapping_result.result_transform_op_vec,
      mrrg_config);

  std::vector<io::MappingTransformOp> output_transform_op_vec;
  for (const auto& transform_op : remapping_result.result_transform_op_vec) {
    output_transform_op_vec.push_back(ConvertMappingTransformOp(transform_op));
  }

  std::unordered_map<int, entity::Mapping> mapping_id_to_mapping;
  for (const auto& mapping_id : result_mapping_id_set) {
    mapping_id_to_mapping[mapping_id] = mapping_vec[mapping_id];
  }

  io::RemapperOutput output;
  output.remapping_time_s = remapping_result.remapping_time_s;
  output.mapping_ptr = result_mapping;
  output.mrrg_config = mrrg_config;
  output.parallel_num = remapping_result.result_mapping_id_vec.size();
  output.mapping_type_num = result_mapping_id_set.size();
  output.mapping_id_to_mapping = mapping_id_to_mapping;
  output.mapping_id_vec = remapping_result.result_mapping_id_vec;
  output.transform_op_vec = output_transform_op_vec;
  logger.LogRemapperOutput(output);
}
