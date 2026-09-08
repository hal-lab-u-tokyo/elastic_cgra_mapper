#include <gtest/gtest.h>

#include <filesystem>
#include <io/mapping_io.hpp>
#include <remapper/remapper.hpp>
#include <remapper/transform.hpp>

#ifndef REMAPPER_TEST_DATABASE_DIR
#define REMAPPER_TEST_DATABASE_DIR "../../../remapper/test/data/database/"
#endif

std::filesystem::path kDatabaseDirPath = REMAPPER_TEST_DATABASE_DIR;

// Create a mapping database for testing
// DFG: load0 -> add2 <- load1, add2 -> output3
// MRRG of mapping_a: 2x2 CGRA with 2 context size
// MRRG of mapping_b: 1x3 CGRA with 2 context size
std::vector<entity::Mapping> LoadMappingDBForTest() {
  std::vector<entity::Mapping> mapping_vec;
  mapping_vec.push_back(
      io::ReadMappingFile(kDatabaseDirPath / "mapping_a.json"));
  mapping_vec.push_back(
      io::ReadMappingFile(kDatabaseDirPath / "mapping_b.json"));
  return mapping_vec;
}

entity::MRRGConfig GetMRRGConfig() {
  entity::MRRGConfig mrrg_config;
  mrrg_config.column = 3;
  mrrg_config.row = 3;
  mrrg_config.context_size = 2;
  mrrg_config.memory_io = entity::MRRGMemoryIOType::kAll;
  mrrg_config.cgra_type = entity::MRRGCGRAType::kElastic;
  mrrg_config.network_type = entity::MRRGNetworkType::kOrthogonal;
  mrrg_config.local_reg_size = 1;

  return mrrg_config;
}

entity::MRRGConfig GetMRRGConfig(int row, int column, int context_size) {
  entity::MRRGConfig mrrg_config = GetMRRGConfig();
  mrrg_config.row = row;
  mrrg_config.column = column;
  mrrg_config.context_size = context_size;
  return mrrg_config;
}

entity::Mapping CreateMapping(int row, int column) {
  entity::ConfigMap config_map;
  config_map.emplace(entity::ConfigId(0, 0, 0),
                     entity::CGRAConfig(entity::OpType::kAdd, "add"));
  return entity::Mapping(GetMRRGConfig(row, column, 1), config_map);
}

// 3x3 CGRA with 2 context size
TEST(RemapperTest, remapper_test) {
  std::vector<entity::Mapping> mapping_vec = LoadMappingDBForTest();
  entity::MRRGConfig mrrg_config = GetMRRGConfig();

  int max_config_num =
      mrrg_config.column * mrrg_config.row * mrrg_config.context_size;
  int min_mapping_op_num = 4;
  size_t parallel_num = max_config_num / min_mapping_op_num;

  std::ofstream log_file(std::filesystem::temp_directory_path() /
                         "test_remapper.log");

  // test dynamic programming remapping
  const auto remapping_result_dp = remapper::Remapper::Remapping(
      mapping_vec, mrrg_config, parallel_num, log_file,
      remapper::RemappingMode::kDP, 100);
  EXPECT_EQ(remapping_result_dp.result_mapping_id_vec.size(), 3);

  // test greedy remapping
  const auto remapping_result_greedy = remapper::Remapper::Remapping(
      mapping_vec, mrrg_config, parallel_num, log_file,
      remapper::RemappingMode::kGreedy, 100);
  EXPECT_EQ(remapping_result_greedy.result_mapping_id_vec.size(), 3);

  // test full search remapping
  const auto remapping_result_full_search = remapper::Remapper::Remapping(
      mapping_vec, mrrg_config, parallel_num, log_file,
      remapper::RemappingMode::kFullSearch, 100);
  EXPECT_EQ(remapping_result_full_search.result_mapping_id_vec.size(), 4);
}

TEST(RemapperTest, full_search_returns_empty_when_mapping_cannot_fit) {
  std::vector<entity::Mapping> mapping_vec;
  mapping_vec.push_back(
      io::ReadMappingFile(kDatabaseDirPath / "mapping_a.json"));

  entity::MRRGConfig mrrg_config = GetMRRGConfig();
  mrrg_config.column = 1;
  mrrg_config.row = 1;

  std::ofstream log_file(std::filesystem::temp_directory_path() /
                         "test_remapper_unfit.log");

  const auto remapping_result =
      remapper::Remapper::Remapping(mapping_vec, mrrg_config, 1, log_file,
                                    remapper::RemappingMode::kFullSearch, 100);

  EXPECT_TRUE(remapping_result.result_mapping_id_vec.empty());
  EXPECT_TRUE(remapping_result.result_transform_op_vec.empty());
}

TEST(RemapperTest, MappingRotaterRotatesConfigIdsAndDimensions) {
  const entity::ConfigId source_id(0, 0, 0);
  const entity::ConfigId destination_id(1, 2, 0);
  entity::CGRAConfig source_config(entity::OpType::kAdd, "source");
  source_config.to_config_id_vec.push_back(destination_id);
  entity::CGRAConfig destination_config(entity::OpType::kOutput, "destination");
  destination_config.from_config_id_vec.push_back(source_id);
  entity::ConfigMap config_map = {{source_id, source_config},
                                  {destination_id, destination_config}};
  const entity::Mapping mapping(GetMRRGConfig(2, 3, 1), config_map);

  const entity::Mapping rotated =
      remapper::MappingRotater(mapping, remapper::RotateOp::kTopIsRight);

  EXPECT_EQ(rotated.GetMRRGConfig().row, 3);
  EXPECT_EQ(rotated.GetMRRGConfig().column, 2);
  const entity::ConfigId rotated_source_id(0, 1, 0);
  const entity::ConfigId rotated_destination_id(2, 0, 0);
  const auto rotated_source = rotated.GetConfig(rotated_source_id);
  const auto rotated_destination = rotated.GetConfig(rotated_destination_id);
  ASSERT_EQ(rotated_source.to_config_id_vec.size(), 1);
  ASSERT_EQ(rotated_destination.from_config_id_vec.size(), 1);
  EXPECT_EQ(rotated_source.to_config_id_vec[0], rotated_destination_id);
  EXPECT_EQ(rotated_destination.from_config_id_vec[0], rotated_source_id);
}

TEST(RemapperTest, GetRotatedItemSizeAllowsOnlyFittingQuarterTurn) {
  const std::vector<entity::Mapping> mappings = {CreateMapping(2, 3)};
  const entity::MRRGConfig target_config = GetMRRGConfig(3, 2, 1);
  std::ofstream log_file("/dev/null");

  const auto result = remapper::Remapper::Remapping(
      mappings, target_config, 1, log_file, remapper::RemappingMode::kDP, 100);

  ASSERT_EQ(result.result_mapping_id_vec.size(), 1);
  ASSERT_EQ(result.result_transform_op_vec.size(), 1);
  EXPECT_EQ(result.result_mapping_id_vec[0], 0);
  EXPECT_EQ(result.result_transform_op_vec[0].row, 0);
  EXPECT_EQ(result.result_transform_op_vec[0].column, 0);
  EXPECT_EQ(result.result_transform_op_vec[0].rotate_op,
            remapper::RotateOp::kTopIsRight);
}

TEST(RemapperTest, GetShiftedPlacementMovesPreviousItemBesideNewItem) {
  const std::vector<entity::Mapping> mappings = {CreateMapping(1, 1)};
  const entity::MRRGConfig target_config = GetMRRGConfig(1, 2, 1);
  std::ofstream log_file("/dev/null");

  const auto result = remapper::Remapper::Remapping(
      mappings, target_config, 2, log_file, remapper::RemappingMode::kDP, 100);

  ASSERT_EQ(result.result_mapping_id_vec.size(), 2);
  ASSERT_EQ(result.result_transform_op_vec.size(), 2);
  EXPECT_EQ(result.result_mapping_id_vec[0], 0);
  EXPECT_EQ(result.result_mapping_id_vec[1], 0);
  EXPECT_EQ(result.result_transform_op_vec[0].row, 0);
  EXPECT_EQ(result.result_transform_op_vec[0].column, 1);
  EXPECT_EQ(result.result_transform_op_vec[1].row, 0);
  EXPECT_EQ(result.result_transform_op_vec[1].column, 0);
}
