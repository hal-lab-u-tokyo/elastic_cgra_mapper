#include <time.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>
#include <remapper/algorithm/dp_elastic_remapper.hpp>

std::vector<entity::MRRGConfig> GetMRRGOfSubCGRA(
    int dfg_node_num, int memory_access_node_num,
    const entity::MRRGConfig& target_mrrg_config, double utilization_min) {
  std::vector<entity::MRRGConfig> mrrg_config_vec;
  for (int context_size = 1; context_size <= target_mrrg_config.context_size;
       context_size++) {
    for (int row_num = 1; row_num <= target_mrrg_config.row; row_num++) {
      int column_num = std::ceil(dfg_node_num / (context_size * row_num));
      if (target_mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll &&
          column_num < row_num) {
        break;
      }

      while (1) {
        if (column_num > target_mrrg_config.column) break;

        double utilization = (double)dfg_node_num /
                             (double)(context_size * row_num * column_num);
        if (utilization < utilization_min) break;

        for (int memory_io_type = 0; memory_io_type < 2; memory_io_type++) {
          entity::MRRGConfig mrrg_config;
          mrrg_config.cgra_type = target_mrrg_config.cgra_type;
          mrrg_config.memory_io = target_mrrg_config.memory_io;
          if (target_mrrg_config.memory_io ==
                  entity::MRRGMemoryIOType::kBothEnds &&
              column_num < target_mrrg_config.column) {
            mrrg_config.memory_io = entity::MRRGMemoryIOType::kOneEnd;
          }

          if (target_mrrg_config.memory_io != entity::MRRGMemoryIOType::kAll) {
            int memory_access_pe_num = 0;
            if (mrrg_config.memory_io == entity::kBothEnds) {
              memory_access_pe_num = 2 * row_num * context_size;
            } else if (mrrg_config.memory_io == entity::kOneEnd) {
              memory_access_pe_num = row_num * context_size;
            }
            if (memory_access_pe_num < memory_access_node_num) {
              continue;
            }
          }

          mrrg_config.network_type = target_mrrg_config.network_type;
          mrrg_config.context_size = context_size;
          mrrg_config.row = row_num;
          mrrg_config.column = column_num;
          mrrg_config.local_reg_size = 1;

          mrrg_config_vec.push_back(mrrg_config);
        }

        column_num++;
      }
    }
  }

  return mrrg_config_vec;
}

std::vector<int> SortElementByFreqency(const std::vector<int>& vec) {
  std::unordered_map<int, int> mode_map;
  for (const auto& value : vec) {
    if (mode_map.find(value) == mode_map.end()) {
      mode_map[value] = 0;
    }
    mode_map[value]++;
  }

  std::vector<std::pair<int, int>> mode_vec;
  for (const auto& [key, value] : mode_map) {
    mode_vec.push_back(std::make_pair(key, value));
  }
  std::sort(mode_vec.begin(), mode_vec.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
              return a.second > b.second;
            });

  std::vector<int> result;
  for (const auto& [key, value] : mode_vec) {
    for (int i = 0; i < value; i++) {
      result.push_back(key);
    }
  }

  return result;
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
  double db_timeout_s = std::stod(argv[5]);

  constexpr double kMinUtilization = 0.7;

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

  const int dfg_node_num = dfg_ptr->GetNodeNum();
  int memory_access_node_num = 0;
  for (int node_id = 0; node_id < dfg_node_num; node_id++) {
    const auto node = dfg_ptr->GetNodeProperty(node_id);
    if (entity::IsMemoryAccessOperation(node.op)) {
      memory_access_node_num++;
    }
  }

  const auto mrrg_config_vec =
      GetMRRGOfSubCGRA(dfg_node_num, memory_access_node_num, target_mrrg_config,
                       kMinUtilization);
  std::vector<remapper::MappingMatrix> mapping_matrix_vec;
  for (size_t i = 0; i < mrrg_config_vec.size(); i++) {
    mapping_matrix_vec.push_back(
        remapper::MappingMatrix::CreateDummyMappingMatrix(mrrg_config_vec[i],
                                                          i));
  }

  remapper::CGRAMatrix cgra_matrix(target_mrrg_config);

  int target_parallel_num =
      (target_mrrg_config.row * target_mrrg_config.column *
       target_mrrg_config.context_size) /
      dfg_node_num;
  while (1) {
    const auto tmp_time = std::time(0);
    std::string remapper_log_file_path =
        log_file_dir + "log" + std::to_string(tmp_time) + ".log";
    std::ofstream remapper_log_file;
    remapper_log_file.open(remapper_log_file_path, std::ios::app);
    remapper_log_file << "-- remapper for create database --" << std::endl;
    remapper_log_file.close();
    const auto start_remapping_time = std::chrono::system_clock::now();
    const auto remapping_result =
        remapper::DPElasticRemapping(mapping_matrix_vec, cgra_matrix,
                                     target_parallel_num, remapper_log_file);
    const auto end_remapping_time = std::chrono::system_clock::now();

    const auto remapper_time_s =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            end_remapping_time - start_remapping_time)
            .count() /
        1000.0;
    db_timeout_s -= remapper_time_s;
    const auto sorted_mapping_id_vec =
        SortElementByFreqency(remapping_result.result_mapping_id_vec);

    for (const auto mapping_id : sorted_mapping_id_vec) {
      const auto tmp_mrrg_config = mrrg_config_vec[mapping_id];
      std::shared_ptr<entity::MRRG> mrrg_ptr =
          std::make_shared<entity::MRRG>(tmp_mrrg_config);

      const auto tmp_time = std::time(0);
      std::string tmp_mapping_log_file_path =
          log_file_dir + "log" + std::to_string(tmp_time) + ".log";
      std::string output_mapping_path =
          output_mapping_dir + "mapping_" + std::to_string(tmp_time) + ".json";

      std::ofstream tmp_mapping_log_file;
      tmp_mapping_log_file.open(tmp_mapping_log_file_path, std::ios::app);
      tmp_mapping_log_file << "-- mapping input --" << std::endl;
      tmp_mapping_log_file << "dfg file: " << dfg_dot_file_path << std::endl;
      tmp_mapping_log_file << "output mapping file: " << output_mapping_path
                           << std::endl;
      tmp_mapping_log_file << "log_file_path file: "
                           << tmp_mapping_log_file_path << std::endl;
      tmp_mapping_log_file << "timeout (s): " << db_timeout_s / 2 << std::endl;
      tmp_mapping_log_file << "parallel num: " << 1 << std::endl;
      tmp_mapping_log_file.close();
      mapper::GurobiILPMapper* mapper;
      mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
      mapper->SetLogFilePath(tmp_mapping_log_file_path);
      mapper->SetTimeOut(db_timeout_s / 2);

      bool is_success = false;
      std::shared_ptr<entity::Mapping> mapping_ptr =
          std::make_shared<entity::Mapping>();
      const auto start_mapping_time = std::chrono::system_clock::now();
      std::tie(is_success, *mapping_ptr) = mapper->Execution();
      const auto end_mapping_time = std::chrono::system_clock::now();
      const auto mapping_time_s =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              end_mapping_time - start_mapping_time)
              .count() /
          1000.0;
      db_timeout_s -= mapping_time_s;

      if (is_success) {
        io::WriteMappingFile(output_mapping_path, mapping_ptr,
                             mrrg_ptr->GetMRRGConfig());
      } else {
        break;
      }
    }
  }
}