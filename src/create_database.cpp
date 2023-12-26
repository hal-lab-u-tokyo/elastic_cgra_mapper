#include <time.h>

#include <filesystem>
#include <fstream>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <io/output_to_log_file.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>
#include <remapper/algorithm/dp_elastic_remapper.hpp>

std::vector<entity::MRRGConfig> GetMRRGOfSubCGRA(
    int dfg_node_num, int memory_access_node_num,
    const entity::MRRGConfig& target_mrrg_config,
    const double utilization_min) {
  std::vector<entity::MRRGConfig> mrrg_config_vec;
  for (int context_size = 1; context_size <= target_mrrg_config.context_size;
       context_size++) {
    for (int row_num = 1; row_num <= target_mrrg_config.row; row_num++) {
      int column_num =
          int(std::ceil(double(dfg_node_num) / (context_size * row_num)));

      if (target_mrrg_config.memory_io == entity::MRRGMemoryIOType::kAll &&
          column_num < row_num) {
        break;
      }

      while (1) {
        if (column_num > target_mrrg_config.column) break;

        double utilization = (double)dfg_node_num /
                             (double)(context_size * row_num * column_num);
        if (utilization < utilization_min) break;

        entity::MRRGConfig mrrg_config;
        mrrg_config.cgra_type = target_mrrg_config.cgra_type;
        mrrg_config.memory_io = target_mrrg_config.memory_io;
        if (target_mrrg_config.memory_io ==
            entity::MRRGMemoryIOType::kBothEnds) {
          if (column_num < target_mrrg_config.column) {
            mrrg_config.memory_io = entity::MRRGMemoryIOType::kOneEnd;
          } else {
            mrrg_config.memory_io = entity::MRRGMemoryIOType::kBothEnds;
          }
        }

        if (target_mrrg_config.memory_io != entity::MRRGMemoryIOType::kAll) {
          int memory_access_pe_num = 0;
          if (mrrg_config.memory_io == entity::kBothEnds) {
            memory_access_pe_num = 2 * row_num * context_size;
          } else if (mrrg_config.memory_io == entity::kOneEnd) {
            memory_access_pe_num = row_num * context_size;
          }
          if (memory_access_pe_num < memory_access_node_num) {
            column_num++;
            continue;
          }
        }

        mrrg_config.network_type = target_mrrg_config.network_type;
        mrrg_config.context_size = context_size;
        mrrg_config.row = row_num;
        mrrg_config.column = column_num;
        mrrg_config.local_reg_size = 1;

        mrrg_config_vec.push_back(mrrg_config);

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
    result.push_back(key);
  }

  return result;
}

int main(int argc, char* argv[]) {
  if (argc != 5) {
    std::cerr << "invalid arguments" << std::endl;
    abort();
  }

  const std::string dfg_dot_file_path = argv[1];
  const std::string mrrg_file_path = argv[2];
  const std::string output_dir = argv[3];
  const double db_timeout_s = std::stod(argv[4]);
  double creating_db_time_s = 0;

  assert(std::filesystem::path(dfg_dot_file_path).is_absolute());
  assert(std::filesystem::path(mrrg_file_path).is_absolute());
  assert(std::filesystem::path(output_dir).is_absolute());
  assert(std::filesystem::exists(dfg_dot_file_path));
  assert(std::filesystem::exists(mrrg_file_path));

  constexpr double kMinUtilization = 0.5;

  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  *dfg_ptr = io::ReadDFGDotFile(dfg_dot_file_path);
  std::shared_ptr<entity::MRRG> target_mrrg_ptr =
      std::make_shared<entity::MRRG>();
  *target_mrrg_ptr = io::ReadMRRGFromJsonFile(mrrg_file_path);
  const auto target_mrrg_config = target_mrrg_ptr->GetMRRGConfig();

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
  std::vector<int> evaluated_mapping_id_vec;

  io::CreateDatabaseLogger logger;
  logger.LogCreateDatabaseInput({dfg_dot_file_path, mrrg_file_path, output_dir,
                                 db_timeout_s, kMinUtilization});
  while (1) {
    std::vector<remapper::MappingMatrix> tmp_mapping_matrix_vec;
    for (const auto& mapping_matrix : mapping_matrix_vec) {
      bool is_evaluated = false;
      if (std::find(evaluated_mapping_id_vec.begin(),
                    evaluated_mapping_id_vec.end(),
                    mapping_matrix.id) != evaluated_mapping_id_vec.end()) {
        is_evaluated = true;
      }

      if (!is_evaluated) {
        tmp_mapping_matrix_vec.push_back(mapping_matrix);
      }
    }

    if (tmp_mapping_matrix_vec.size() == 0) {
      break;
    }

    std::ofstream remapper_log_file(logger.GetSelectionLogFilePath());
    const auto remapping_result =
        remapper::DPElasticRemapping(tmp_mapping_matrix_vec, cgra_matrix,
                                     target_parallel_num, remapper_log_file);

    creating_db_time_s += remapping_result.remapping_time_s;
    const auto sorted_mapping_id_vec =
        SortElementByFreqency(remapping_result.result_mapping_id_vec);

    for (const auto mapping_id : sorted_mapping_id_vec) {
      const auto tmp_mrrg_config = mrrg_config_vec[mapping_id];
      std::shared_ptr<entity::MRRG> mrrg_ptr =
          std::make_shared<entity::MRRG>(tmp_mrrg_config);

      double mapping_time_out_s = (db_timeout_s - creating_db_time_s) / 2;
      mapper::GurobiILPMapper* mapper;
      mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
      mapper->SetLogFilePath(logger.GetNextGurobiMappingPath(
          mapping_time_out_s, mrrg_ptr->GetMRRGConfig()));
      mapper->SetTimeOut(mapping_time_out_s);

      const auto mapping_result = mapper->Execution();
      creating_db_time_s += mapping_result.mapping_time_s;

      evaluated_mapping_id_vec.push_back(mapping_id);
      if (mapping_result.is_success) {
        io::MappingOutput mapping_output;
        mapping_output.mapping_time_s = mapping_result.mapping_time_s;
        mapping_output.is_success = mapping_result.is_success;
        mapping_output.mapping_ptr = mapping_result.mapping_ptr;
        mapping_output.mrrg_config = mrrg_ptr->GetMRRGConfig();

        logger.LogMapping(mapping_output);
      } else {
        break;
      }
    }
  }

  io::CreateDatabaseOutput db_output;
  db_output.creating_db_time_s = creating_db_time_s;
  logger.LogCreateDatabaseOutput(db_output);
}