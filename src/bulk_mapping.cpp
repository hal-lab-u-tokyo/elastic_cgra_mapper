#include <time.h>

#include <chrono>
#include <filesystem>
#include <io/architecture_io.hpp>
#include <io/dfg_io.hpp>
#include <io/mapping_io.hpp>
#include <io/output_to_log_file.hpp>
#include <iostream>
#include <mapper/gurobi_mapper.hpp>

std::vector<entity::MRRGConfig> GetMRRGToTest(int dfg_node_num) {
  std::vector<entity::MRRGConfig> mrrg_config_vec;
  int dfg_node_num_sqrt = static_cast<int>(std::sqrt(dfg_node_num));

  for (int context_size = 1; context_size < dfg_node_num; context_size++) {
    for (int row_num = 2; row_num <= dfg_node_num_sqrt; row_num++) {
      int column_num = std::ceil(dfg_node_num / (context_size * row_num));
      if (column_num < row_num) break;
      for (int memory_io_type = 0; memory_io_type < 2; memory_io_type++) {
        entity::MRRGConfig mrrg_config;
        mrrg_config.cgra_type = entity::MRRGCGRAType::kElastic;
        mrrg_config.memory_io =
            static_cast<entity::MRRGMemoryIOType>(memory_io_type);
        mrrg_config.network_type = entity::MRRGNetworkType::kOrthogonal;
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
  if (argc != 5) {
    std::cerr << "invalid arguments" << std::endl;
    abort();
  }

  const std::string dfg_dot_file_path(argv[1]);
  const std::string output_dir = argv[2];
  double timeout_s = std::stod(argv[3]);

  assert(std::filesystem::path(dfg_dot_file_path).is_absolute());
  assert(std::filesystem::path(output_dir).is_absolute());

  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  *dfg_ptr = io::ReadDFGDotFile(dfg_dot_file_path);

  const auto mrrg_config_vec = GetMRRGToTest(dfg_ptr->GetNodeNum());

  for (auto mrrg_config : mrrg_config_vec) {
    bool is_success = false;
    int count = 0;
    while (!is_success) {
      io::MappingLogger mapping_logger;
      io::MappingInput mapping_input;
      mapping_input.dfg_dot_file_path = dfg_dot_file_path;
      mapping_input.mrrg_config = mrrg_config;
      mapping_input.output_dir_path = output_dir;
      mapping_input.timeout_s = timeout_s;
      mapping_input.parallel_num = 1;
      mapping_logger.LogMappingInput(mapping_input);
      std::shared_ptr<entity::MRRG> mrrg_ptr =
          std::make_shared<entity::MRRG>(mrrg_config);

      mapper::GurobiILPMapper* mapper;
      mapper = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
      mapper->SetLogFilePath(mapping_logger.GetGurobiLogFilePath());
      mapper->SetTimeOut(timeout_s);

      const auto result = mapper->Execution();
      is_success = result.is_success;

      if (is_success) {
        io::MappingOutput mapping_output;
        mapping_output.mapping_time_s = result.mapping_time_s;
        mapping_output.is_success = result.is_success;
        mapping_output.mapping_ptr = result.mapping_ptr;
        mapping_output.mrrg_config = mrrg_config;
        mapping_logger.LogMappingOutput(mapping_output);
      } else {
        mrrg_config.column++;
      }
      count++;
      if (count == 5) break;
    }
  }
}
