#include <gtest/gtest.h>

#include <io/mapper_config_io.hpp>

TEST(IOTest, mapper_config_io_test) {
  entity::MapperConfig mapper_config;
  mapper_config.dfg_config.operation_name_label = "node_id";
  mapper_config.algorithm_config.type = "FullRoutingILPMapper";
  mapper_config.algorithm_config.accept_feasible_solution = false;
  mapper_config.algorithm_config.placement_only = true;
  mapper_config.algorithm_config.max_trials = 200;
  mapper_config.algorithm_config.seed_count = 4;
  mapper_config.algorithm_config.routing_retry_count = 8;
  mapper_config.algorithm_config.random_seed = 1234;
  mapper_config.algorithm_config.max_iterations = 20000;

  std::string file_name = "./mapper_config_data.json";
  io::WriteMapperConfigToJsonFile(file_name, mapper_config);

  entity::MapperConfig read_mapper_config =
      io::ReadMapperConfigFromJsonFile(file_name);

  EXPECT_EQ(mapper_config.dfg_config.operation_name_label,
            read_mapper_config.dfg_config.operation_name_label);
  EXPECT_EQ(mapper_config.algorithm_config.type,
            read_mapper_config.algorithm_config.type);
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
