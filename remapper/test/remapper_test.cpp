#include <gtest/gtest.h>

#include <filesystem>
#include <io/mapping_io.hpp>
#include <remapper/remapper.hpp>

std::filesystem::path kDatabaseDirPath =
    "../../../remapper/test/data/database/";

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

  std::ofstream log_file(kDatabaseDirPath / "test_remapper.log");

  // test dynamic programming remapping
  const auto remapping_result_dp = remapper::Remapper::ElasticRemapping(
      mapping_vec, mrrg_config, parallel_num, log_file,
      remapper::RemappingMode::DP, 100);
  EXPECT_EQ(remapping_result_dp.result_mapping_id_vec.size(), 3);

  // test greedy remapping
  const auto remapping_result_greedy = remapper::Remapper::ElasticRemapping(
      mapping_vec, mrrg_config, parallel_num, log_file,
      remapper::RemappingMode::Greedy, 100);
  EXPECT_EQ(remapping_result_greedy.result_mapping_id_vec.size(), 3);

  // test full search remapping
  const auto remapping_result_full_search =
      remapper::Remapper::ElasticRemapping(
          mapping_vec, mrrg_config, parallel_num, log_file,
          remapper::RemappingMode::FullSearch, 100);
  EXPECT_EQ(remapping_result_full_search.result_mapping_id_vec.size(), 4);
}
