#include <gtest/gtest.h>

#include <algorithm>
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
  input_mrrg_config.loop_controller_position_vec = {{0, 0}, {7, 7}};
  input_mrrg_config.tm_pe_position_vec = {{0, 7}, {7, 0}};

  std::shared_ptr<entity::MRRG> input_mrrg_ptr_ =
      std::make_shared<entity::MRRG>();
  *input_mrrg_ptr_ = entity::MRRG(input_mrrg_config);

  io::WriteMRRGToJsonFile(file_path, input_mrrg_ptr_);
  entity::MRRG output_mrrg = io::ReadMRRGFromJsonFile(file_path);

  EXPECT_EQ(input_mrrg_config.row, output_mrrg.GetMRRGConfig().row);
  EXPECT_EQ(input_mrrg_config.loop_controller_position_vec.size(),
            output_mrrg.GetMRRGConfig().loop_controller_position_vec.size());
  EXPECT_EQ(input_mrrg_config.tm_pe_position_vec.size(),
            output_mrrg.GetMRRGConfig().tm_pe_position_vec.size());

  auto has_op =
      [](const std::vector<entity::OpType>& ops, entity::OpType op) {
        return std::find(ops.begin(), ops.end(), op) != ops.end();
      };
  auto loop_node_id = output_mrrg.GetMRRGNodeId(0, 0, 0);
  auto tm_node_id = output_mrrg.GetMRRGNodeId(0, 7, 0);
  auto normal_node_id = output_mrrg.GetMRRGNodeId(1, 1, 0);

  EXPECT_TRUE(
      has_op(output_mrrg.GetNodeProperty(loop_node_id).supported_operations,
             entity::OpType::LOOP));
  EXPECT_TRUE(
      has_op(output_mrrg.GetNodeProperty(tm_node_id).supported_operations,
             entity::OpType::TM));
  EXPECT_FALSE(
      has_op(output_mrrg.GetNodeProperty(normal_node_id).supported_operations,
             entity::OpType::TM));
}
