#include <time.h>

#include <entity/mapping.hpp>
#include <filesystem>
#include <io/architecture_io.hpp>
#include <io/mapping_io.hpp>
#include <remapper/remapper.hpp>

int main(int argc, char* argv[]) {
  std::string mapping_dir_path = argv[1];
  std::string mrrg_file_path = argv[2];
  std::string output_mapping_dir = argv[3];

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
    std::cout << file.path() << std::endl;
    min_mapping_op_num = std::min(min_mapping_op_num, mapping.GetOpNum());
    mapping_vec.push_back(mapping);
  }

  // size_t parallel_num = std::floor(max_config_num / min_mapping_op_num);
  size_t parallel_num = 5;

  bool is_success = false;
  std::shared_ptr<entity::Mapping> result_mapping =
      std::make_shared<entity::Mapping>();

  while (parallel_num > 0) {
    std::cout << "parallel num: " << parallel_num << std::endl;
    std::tie(is_success, *result_mapping) =
        remapper::Remapper::ElasticRemapping(mapping_vec, mrrg_config,
                                             parallel_num);
    if (is_success) break;

    parallel_num--;
  }

  if (is_success) {
    const auto tmp_time = std::time(0);
    std::string output_mapping_path =
        output_mapping_dir + "mapping_" + std::to_string(tmp_time) + ".json";
    io::WriteMappingFile(output_mapping_path, result_mapping, mrrg_config);
  }
}