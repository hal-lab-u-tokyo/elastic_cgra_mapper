#include <gtest/gtest.h>

#include <io/architecture_io.hpp>
#include <string>

TEST(IOTest, mrrg_io_test) {
  std::string file_path = "./test_mrrg_data.dot";
  entity::MRRGConfig input_mrrg_config;

  input_mrrg_config.column = 8;
  input_mrrg_config.row = 8;
  input_mrrg_config.memory_io = entity::MRRGMemoryIOType::kAll;
  input_mrrg_config.cgra_type = entity::MRRGCGRAType::kElastic;
  input_mrrg_config.network_type = entity::MRRGNetworkType::kDiagonal;
  input_mrrg_config.local_reg_size = 3;
  input_mrrg_config.context_size = 3;

  std::shared_ptr<entity::MRRG> input_mrrg_ptr_ =
      std::make_shared<entity::MRRG>();
  *input_mrrg_ptr_ = entity::MRRG(input_mrrg_config);

  io::WriteMRRGToJsonFile(file_path, input_mrrg_ptr_);
  entity::MRRG output_mrrg = io::ReadMRRGFromJsonFile(file_path);

  EXPECT_EQ(input_mrrg_config.row, output_mrrg.GetMRRGConfig().row);
}
