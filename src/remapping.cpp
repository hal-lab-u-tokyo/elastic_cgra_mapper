#include <time.h>

#include <entity/mapping.hpp>
#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/mapping_io.hpp>
#include <io/output_to_log_file.hpp>
#include <io/select_database.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/remapper.hpp>

io::MappingTransformOp ConvertMappingTransformOp(
    remapper::MappingTransformOp transform_op) {
  io::MappingTransformOp result;
  result.row = transform_op.row;
  result.column = transform_op.column;
  result.rotate_op = remapper::RotateOpToString(transform_op.rotate_op);
  return result;
}

int main(int argc, char* argv[]) {
  std::string database_dir_path = argv[1];
  std::string dfg_file_path = argv[2];
  std::string mrrg_file_path = argv[3];
  std::string output_dir = argv[4];
  remapper::RemappingMode mode =
      static_cast<remapper::RemappingMode>(std::stoi(argv[5]));
  const double timeout_s = std::stod(argv[6]);
  const int num_available_mappings = std::stoi(argv[7]);

  if (argc != 8) {
    std::cerr << "invalid arguments" << std::endl;
    abort();
  }

  assert(std::filesystem::path(database_dir_path).is_absolute());
  assert(std::filesystem::path(mrrg_file_path).is_absolute());
  assert(std::filesystem::path(output_dir).is_absolute());
  assert(std::filesystem::exists(database_dir_path));
  assert(std::filesystem::exists(mrrg_file_path));

  if (!std::filesystem::exists(output_dir)) {
    std::filesystem::create_directories(output_dir);
  };

  io::RemapperLogger logger;
  io::RemapperInput input;
  input.mapping_dir_path = database_dir_path;
  input.cgra_file_path = mrrg_file_path;
  input.dfg_file_path = dfg_file_path;
  input.output_dir_path = output_dir;
  input.remapper_mode = remapper::RemappingModeToString(mode);
  input.timeout_s = timeout_s;
  input.num_available_mappings = num_available_mappings;
  logger.LogRemapperInput(input);

  const auto mrrg_config =
      io::ReadMRRGFromJsonFile(mrrg_file_path).GetMRRGConfig();

  entity::Database database =
      io::select_database(database_dir_path, mrrg_config, input.dfg_file_path);
  database.LimitMappingNum(num_available_mappings);
  std::vector<entity::Mapping> mapping_vec = database.GetMappings();

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
