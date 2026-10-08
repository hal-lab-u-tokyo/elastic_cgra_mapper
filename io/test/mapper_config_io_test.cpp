#include <gtest/gtest.h>

#include <io/mapper_config_io.hpp>

TEST(IOTest, ilp_mapper_config_io_test) {
  entity::MapperConfig mapper_config;
  mapper_config.dfg_config.operation_name_label = "node_id";
  mapper_config.algorithm_config.algorithm = entity::AlgorithmType::kILPMapper;

  std::string file_name = "./mapper_config_data.json";
  io::WriteMapperConfigToJsonFile(file_name, mapper_config);

  entity::MapperConfig read_mapper_config =
      io::ReadMapperConfigFromJsonFile(file_name);

  EXPECT_EQ(mapper_config.algorithm_config.accept_feasible_solution,
            read_mapper_config.algorithm_config.accept_feasible_solution);
  EXPECT_EQ(mapper_config.algorithm_config.placement_only,
            read_mapper_config.algorithm_config.placement_only);
}

TEST(IOTest, yott_mapper_config_io_test) {
  entity::MapperConfig mapper_config;
  mapper_config.dfg_config.operation_name_label = "node_id";
  mapper_config.algorithm_config.algorithm = entity::AlgorithmType::kYOTTMapper;
  mapper_config.algorithm_config.accept_feasible_solution = false;
  mapper::YOTTConfig yott_config;
  yott_config.placement_only = true;
  yott_config.max_trials = 200;
  yott_config.seed_count = 4;
  yott_config.routing_retry_count = 8;
  yott_config.random_seed = 1234;
  yott_config.max_iterations = 20000;
  mapper_config.algorithm_config = yott_config;

  std::string file_name = "./mapper_config_data.json";
  io::WriteMapperConfigToJsonFile(file_name, mapper_config);

  entity::MapperConfig read_mapper_config =
      io::ReadMapperConfigFromJsonFile(file_name);

  EXPECT_EQ(mapper_config.dfg_config.operation_name_label,
            read_mapper_config.dfg_config.operation_name_label);
  EXPECT_EQ(mapper_config.algorithm_config.algorithm,
            read_mapper_config.algorithm_config.algorithm);
  EXPECT_EQ(mapper_config.algorithm_config.accept_feasible_solution,
            read_mapper_config.algorithm_config.accept_feasible_solution);
  EXPECT_EQ(mapper_config.algorithm_config.placement_only,
            read_mapper_config.algorithm_config.placement_only);
  EXPECT_EQ(mapper_config.algorithm_config.max_trials,
            read_mapper_config.algorithm_config.max_trials);
  EXPECT_EQ(mapper_config.algorithm_config.seed_count,
            read_mapper_config.algorithm_config.seed_count);
  EXPECT_EQ(mapper_config.algorithm_config.routing_retry_count,
            read_mapper_config.algorithm_config.routing_retry_count);
  EXPECT_EQ(mapper_config.algorithm_config.random_seed,
            read_mapper_config.algorithm_config.random_seed);
  EXPECT_EQ(mapper_config.algorithm_config.max_iterations,
            read_mapper_config.algorithm_config.max_iterations);
}
