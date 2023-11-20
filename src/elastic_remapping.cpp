#include <time.h>

#include <entity/mapping.hpp>
#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/mapping_io.hpp>
#include <remapper/remapper.hpp>

int main(int argc, char* argv[]) {
  std::string mapping_dir_path = argv[1];
  std::string mrrg_file_path = argv[2];
  std::string output_mapping_dir = argv[3];
  std::string output_log_dir = argv[4];

  remapper::RemappingMode mode = remapper::RemappingMode::Naive;

  if (!std::filesystem::exists(output_mapping_dir)) {
    std::filesystem::create_directories(output_mapping_dir);
  };
  if (!std::filesystem::exists(output_log_dir)) {
    std::filesystem::create_directories(output_log_dir);
  }
  const auto tmp_time = std::time(0);
  std::string log_file_path = output_log_dir + "log" +
                              std::to_string(tmp_time) + "_mode" +
                              std::to_string(mode) + ".log";

  std::ofstream log_file;
  log_file.open(log_file_path);

  switch (mode) {
    case remapper::RemappingMode::FullSearch:
      log_file << "mode: FullSearch" << std::endl;
    case remapper::RemappingMode::Naive:
      log_file << "mode: Naive" << std::endl;
  }

  std::vector<entity::Mapping> mapping_vec;
  const auto mrrg_config =
      io::ReadMRRGFromJsonFile(mrrg_file_path).GetMRRGConfig();

  size_t max_config_num =
      mrrg_config.row * mrrg_config.column * mrrg_config.context_size;
  size_t min_mapping_op_num = max_config_num;
  for (const auto& file :
       std::filesystem::directory_iterator(mapping_dir_path)) {
    const auto mapping = io::ReadMappingFile(file.path());
    if (mapping.GetMRRGConfig().cgra_type != entity::MRRGCGRAType::kElastic) {
      continue;
    }
    if (mapping.GetMRRGConfig().context_size > mrrg_config.context_size) {
      continue;
    }

    int min_row_column_size = std::min(mrrg_config.row, mrrg_config.column);
    if (mapping.GetMRRGConfig().row > min_row_column_size ||
        mapping.GetMRRGConfig().column > min_row_column_size) {
      continue;
    }
    log_file << file.path() << std::endl;
    min_mapping_op_num = std::min(min_mapping_op_num, mapping.GetOpNum());
    mapping_vec.push_back(mapping);
  }

  size_t parallel_num = std::floor(max_config_num / min_mapping_op_num);
  // size_t parallel_num = 5;

  bool is_success = false;
  std::shared_ptr<entity::Mapping> result_mapping =
      std::make_shared<entity::Mapping>();

  while (parallel_num > 0) {
    log_file << "parallel num: " << parallel_num << std::endl;
    const auto start_time = clock();
    std::tie(is_success, *result_mapping) =
        remapper::Remapper::ElasticRemapping(mapping_vec, mrrg_config,
                                             parallel_num, log_file, mode);
    const auto end_time = clock();
    log_file << "total " << parallel_num << " parallel remapping time: "
             << ((double)end_time - start_time) / CLOCKS_PER_SEC << std::endl;

    if (is_success) break;

    parallel_num--;
  }

  if (is_success) {
    std::string output_mapping_path = output_mapping_dir + "mapping_" +
                                      std::to_string(tmp_time) + "_mode" +
                                      std::to_string(mode) + ".json";
    io::WriteMappingFile(output_mapping_path, result_mapping, mrrg_config);
  }
}