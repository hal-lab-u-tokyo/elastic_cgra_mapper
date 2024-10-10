#include <time.h>

#include <entity/mapping.hpp>
#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/mapping_io.hpp>
#include <io/output_to_log_file.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/remapper.hpp>

int main(int argc, char* argv[]) {
  std::string mapping_dir_path = argv[1];
  std::string mrrg_file_path = argv[2];
  std::string output_dir = argv[3];
  remapper::RemappingMode mode =
      static_cast<remapper::RemappingMode>(std::stoi(argv[4]));
  const double timeout_s = std::stod(argv[5]);
  const int db_num = std::stod(argv[6]);

  assert(std::filesystem::path(mapping_dir_path).is_absolute());
  assert(std::filesystem::path(mrrg_file_path).is_absolute());
  assert(std::filesystem::path(output_dir).is_absolute());
  assert(std::filesystem::exists(mapping_dir_path));
  assert(std::filesystem::exists(mrrg_file_path));

  if (!std::filesystem::exists(output_dir)) {
    std::filesystem::create_directories(output_dir);
  };

  io::RemapperLogger logger;
  io::RemapperInput input;
  input.mapping_dir_path = mapping_dir_path;
  input.cgra_file_path = mrrg_file_path;
  input.output_dir_path = output_dir;
  input.remapper_mode = remapper::RemappingModeToString(mode);
  input.timeout_s = timeout_s;
  logger.LogRemapperInput(input);

  std::vector<entity::Mapping> mapping_vec;
  const auto mrrg_config =
      io::ReadMRRGFromJsonFile(mrrg_file_path).GetMRRGConfig();

  size_t max_config_num =
      mrrg_config.row * mrrg_config.column * mrrg_config.context_size;
  size_t min_mapping_op_num = max_config_num;
  for (const auto& file :
       std::filesystem::directory_iterator(mapping_dir_path)) {
    const auto mapping = io::ReadMappingFile(file.path());
    if (mapping.GetMRRGConfig().context_size > mrrg_config.context_size) {
      continue;
    }

    int min_row_column_size = std::min(mrrg_config.row, mrrg_config.column);
    if (mapping.GetMRRGConfig().row > min_row_column_size ||
        mapping.GetMRRGConfig().column > min_row_column_size) {
      continue;
    }
    if (mapping.GetMRRGConfig().network_type != mrrg_config.network_type) {
      continue;
    }
    bool mapping_io_is_both_ends_or_one_end =
        mapping.GetMRRGConfig().memory_io ==
            entity::MRRGMemoryIOType::kBothEnds ||
        mapping.GetMRRGConfig().memory_io == entity::MRRGMemoryIOType::kOneEnd;
    if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kBothEnds &&
        !mapping_io_is_both_ends_or_one_end) {
      continue;
    }
    if (mrrg_config.memory_io == entity::MRRGMemoryIOType::kOneEnd &&
        mapping.GetMRRGConfig().memory_io !=
            entity::MRRGMemoryIOType::kOneEnd) {
      continue;
    }
    min_mapping_op_num = std::min(min_mapping_op_num, mapping.GetOpNum());
    mapping_vec.push_back(mapping);
  }

  size_t parallel_num = std::floor(max_config_num / min_mapping_op_num);

  std::shared_ptr<entity::Mapping> result_mapping =
      std::make_shared<entity::Mapping>();

  std::ofstream remapper_exec_file =
      std::ofstream(logger.GetRemapperExecLogFilePath());
  remapper::RemappingResult remapping_result;
  if (db_num == -1) {
    remapping_result = remapper::Remapper::ElasticRemapping(
        mapping_vec, mrrg_config, parallel_num, remapper_exec_file, mode,
        timeout_s);
  } else if (db_num == 1) {
    remapping_result = remapper::Remapper::ElasticRemapping(
        mapping_vec, mrrg_config, parallel_num, remapper_exec_file, mode,
        timeout_s, 1);
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

  io::RemapperOutput output;
  output.remapping_time_s = remapping_result.remapping_time_s;
  output.mapping_ptr = result_mapping;
  output.mrrg_config = mrrg_config;
  output.parallel_num = remapping_result.result_mapping_id_vec.size();
  output.mapping_type_num = result_mapping_id_set.size();
  output.db_num = db_num;
  logger.LogRemapperOutput(output);
}
