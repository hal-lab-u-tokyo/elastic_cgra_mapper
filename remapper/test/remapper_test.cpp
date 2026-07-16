#include <gtest/gtest.h>

#include <filesystem>
#include <io/mapping_io.hpp>
#include <remapper/mapping_concater.hpp>
#include <remapper/remapper.hpp>

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

TEST(RemapperTest, dp_default_cgra_result_can_be_concatenated) {
  std::vector<entity::Mapping> mapping_vec = LoadMappingDBForTest();
  entity::MRRGConfig mrrg_config = GetMRRGConfig();
  mrrg_config.cgra_type = entity::MRRGCGRAType::kDefault;

  int max_config_num =
      mrrg_config.column * mrrg_config.row * mrrg_config.context_size;
  int min_mapping_op_num = 4;
  size_t parallel_num = max_config_num / min_mapping_op_num;

  std::ofstream log_file(std::filesystem::temp_directory_path() /
                         "test_remapper_default.log");

  const auto remapping_result = remapper::Remapper::Remapping(
      mapping_vec, mrrg_config, parallel_num, log_file,
      remapper::RemappingMode::kDP, 100);

  std::vector<entity::Mapping> result_mapping_vec;
  for (const auto mapping_id : remapping_result.result_mapping_id_vec) {
    result_mapping_vec.push_back(mapping_vec[mapping_id]);
  }

  const auto result_mapping = remapper::MappingConcater(
      result_mapping_vec, remapping_result.result_transform_op_vec,
      mrrg_config);

  EXPECT_EQ(result_mapping.GetMRRGConfig().cgra_type,
            entity::MRRGCGRAType::kDefault);
  EXPECT_EQ(remapping_result.result_mapping_id_vec.size(),
            remapping_result.result_transform_op_vec.size());
}
