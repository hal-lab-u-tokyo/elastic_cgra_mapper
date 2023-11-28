#include <time.h>

#include <chrono>
#include <filesystem>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>

std::vector<entity::MRRGConfig> GetMRRGToTest(int dfg_node_num) {
  std::vector<entity::MRRGConfig> mrrg_config_vec;
  int dfg_node_num_sqrt = static_cast<int>(std::sqrt(dfg_node_num));

  for (int context_size = 1; context_size < dfg_node_num; context_size++) {
    for (int row_num = 2; row_num <= dfg_node_num_sqrt; row_num++) {
      int column_num = std::ceil(dfg_node_num / (context_size * row_num));
      if (column_num < row_num) break;
      for (int cgra_type = 0; cgra_type < 2; cgra_type++) {
        for (int memory_io_type = 0; memory_io_type < 2; memory_io_type++) {
          for (int network_type = 0; network_type < 2; network_type++) {
            entity::MRRGConfig mrrg_config;
            mrrg_config.cgra_type =
                static_cast<entity::MRRGCGRAType>(cgra_type);
            mrrg_config.memory_io =
                static_cast<entity::MRRGMemoryIOType>(memory_io_type);
            mrrg_config.network_type =
                static_cast<entity::MRRGNetworkType>(network_type);
            mrrg_config.context_size = context_size;
            mrrg_config.row = row_num;
            mrrg_config.column = column_num;
            mrrg_config.local_reg_size = 1;

            mrrg_config_vec.push_back(mrrg_config);
          }
        }
      }
    }
  }

  return mrrg_config_vec;
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cerr << "invalid arguments" << std::endl;
    abort();
  }

  const std::string dfg_dot_file_path = argv[1];
  const std::string output_mapping_dir = argv[2];
  const std::string log_file_dir = argv[3];

  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  *dfg_ptr = io::ReadDFGDotFile(dfg_dot_file_path);

  if (!std::filesystem::exists(output_mapping_dir)) {
    std::filesystem::create_directories(output_mapping_dir);
  };
  if (!std::filesystem::exists(log_file_dir)) {
    std::filesystem::create_directories(log_file_dir);
  }

  const auto mrrg_config_vec = GetMRRGToTest(dfg_ptr->GetNodeNum());

  for (auto mrrg_config : mrrg_config_vec) {
    bool is_success = false;
    int count = 0;
    while (!is_success) {
      const auto tmp_time = std::time(0);

      std::shared_ptr<entity::MRRG> mrrg_ptr =
          std::make_shared<entity::MRRG>(mrrg_config);

      std::string log_file_path =
          log_file_dir + "log" + std::to_string(tmp_time) + ".log";
      mapper::GurobiILPMapper* mapper;
      mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
      mapper->SetLogFilePath(log_file_path);

      std::shared_ptr<entity::Mapping> mapping_ptr =
          std::make_shared<entity::Mapping>();
      std::tie(is_success, *mapping_ptr) = mapper->Execution();

      std::string output_mapping_path =
          output_mapping_dir + "mapping_" + std::to_string(tmp_time) + ".json";
      if (is_success) {
        io::WriteMappingFile(output_mapping_path, mapping_ptr,
                             mrrg_ptr->GetMRRGConfig());
      } else {
        mrrg_config.column++;
      }
      count++;
      if (count == 5) break;
    }
  }
}