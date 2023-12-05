#include <time.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>

std::vector<entity::MRRGConfig> GetMRRGOfSubCGRA(
    int dfg_node_num, const entity::MRRGConfig& target_mrrg_config) {
  std::vector<entity::MRRGConfig> mrrg_config_vec;
  for (int context_size = 1; context_size <= target_mrrg_config.context_size;
       context_size++) {
    for (int row_num = 1; row_num <= target_mrrg_config.row; row_num++) {
      int column_num = std::ceil(dfg_node_num / (context_size * row_num));
      if (target_mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll &&
          column_num < row_num)
        break;
      if (column_num > target_mrrg_config.column) break;

      for (int memory_io_type = 0; memory_io_type < 2; memory_io_type++) {
        entity::MRRGConfig mrrg_config;
        mrrg_config.cgra_type = target_mrrg_config.cgra_type;
        mrrg_config.memory_io = target_mrrg_config.memory_io;
        if (target_mrrg_config.memory_io ==
                entity::MRRGMemoryIOType::kBothEnds &&
            column_num < target_mrrg_config.column) {
          mrrg_config.memory_io = entity::MRRGMemoryIOType::kOneEnd;
        }
        mrrg_config.network_type = target_mrrg_config.network_type;
        mrrg_config.context_size = context_size;
        mrrg_config.row = row_num;
        mrrg_config.column = column_num;
        mrrg_config.local_reg_size = 1;

        mrrg_config_vec.push_back(mrrg_config);
      }
    }
  }

  return mrrg_config_vec;
}

int main(int argc, char* argv[]) {
  if (argc != 6) {
    std::cerr << "invalid arguments" << std::endl;
    abort();
  }

  const std::string dfg_dot_file_path = argv[1];
  const std::string mrrg_file_path = argv[2];
  const std::string output_mapping_dir = argv[3];
  const std::string log_file_dir = argv[4];
  double timeout_s = std::stod(argv[5]);

  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  *dfg_ptr = io::ReadDFGDotFile(dfg_dot_file_path);
  std::shared_ptr<entity::MRRG> target_mrrg_ptr =
      std::make_shared<entity::MRRG>();
  *target_mrrg_ptr = io::ReadMRRGFromJsonFile(mrrg_file_path);
  const auto target_mrrg_config = target_mrrg_ptr->GetMRRGConfig();

  if (!std::filesystem::exists(output_mapping_dir)) {
    std::filesystem::create_directories(output_mapping_dir);
  };
  if (!std::filesystem::exists(log_file_dir)) {
    std::filesystem::create_directories(log_file_dir);
  }

  const auto mrrg_config_vec =
      GetMRRGOfSubCGRA(dfg_ptr->GetNodeNum(), target_mrrg_config);

  for (auto tmp_mrrg_config : mrrg_config_vec) {
    bool is_success = false;
    while (!is_success) {
      const auto tmp_time = std::time(0);

      std::shared_ptr<entity::MRRG> mrrg_ptr =
          std::make_shared<entity::MRRG>(tmp_mrrg_config);

      std::string log_file_path =
          log_file_dir + "log" + std::to_string(tmp_time) + ".log";
      std::string output_mapping_path =
          output_mapping_dir + "mapping_" + std::to_string(tmp_time) + ".json";

      std::ofstream log_file;
      log_file.open(log_file_path, std::ios::app);
      log_file << "-- mapping input --" << std::endl;
      log_file << "dfg file: " << dfg_dot_file_path << std::endl;
      log_file << "output mapping file: " << output_mapping_path << std::endl;
      log_file << "log_file_path file: " << log_file_path << std::endl;
      log_file << "timeout (s): " << timeout_s << std::endl;
      log_file << "parallel num: " << 1 << std::endl;
      log_file.close();
      mapper::GurobiILPMapper* mapper;
      mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
      mapper->SetLogFilePath(log_file_path);
      mapper->SetTimeOut(timeout_s);

      std::shared_ptr<entity::Mapping> mapping_ptr =
          std::make_shared<entity::Mapping>();
      std::tie(is_success, *mapping_ptr) = mapper->Execution();

      if (is_success) {
        io::WriteMappingFile(output_mapping_path, mapping_ptr,
                             mrrg_ptr->GetMRRGConfig());
      } else {
        tmp_mrrg_config.column++;
      }

      if (tmp_mrrg_config.column > target_mrrg_config.column) {
        break;
      }
    }
  }
}