#include <gtest/gtest.h>

#include <io/mapper_config_io.hpp>

TEST(IOTest, mapper_config_io_test) {
  entity::MapperConfig mapper_config;
  mapper_config.dfg_config.operation_name_label = "node_id";
  mapper_config.algorithm_config.algorithm = entity::AlgorithmType::kILPMapper;

  std::string file_name = "./mapper_config_data.json";
  io::WriteMapperConfigToJsonFile(file_name, mapper_config);

  entity::MapperConfig read_mapper_config =
      io::ReadMapperConfigFromJsonFile(file_name);

  EXPECT_EQ(mapper_config.dfg_config.operation_name_label,
            read_mapper_config.dfg_config.operation_name_label);
}
