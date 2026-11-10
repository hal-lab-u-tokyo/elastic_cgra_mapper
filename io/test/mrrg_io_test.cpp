#include <gtest/gtest.h>

#include <io/architecture_io.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <set>
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

  std::shared_ptr<entity::MRRG> input_mrrg_ptr_ =
      std::make_shared<entity::MRRG>();
  *input_mrrg_ptr_ = entity::MRRG(input_mrrg_config);

  io::WriteMRRGToJsonFile(file_path, input_mrrg_ptr_);
  entity::MRRG output_mrrg = io::ReadMRRGFromJsonFile(file_path);

  EXPECT_EQ(input_mrrg_config.row, output_mrrg.GetMRRGConfig().row);
  EXPECT_EQ(input_mrrg_config.loop_controller_position_vec.size(),
            output_mrrg.GetMRRGConfig().loop_controller_position_vec.size());
}


TEST(IOTest, one_hop_2d_canonical_and_legacy_input_test) {
  entity::MRRGConfig config{};
  config.column = 5;
  config.row = 5;
  config.memory_io = entity::MRRGMemoryIOType::kAll;
  config.cgra_type = entity::MRRGCGRAType::kDefault;
  config.network_type = entity::MRRGNetworkType::kOneHop2d;
  config.local_reg_size = 3;
  config.context_size = 1;
  auto mrrg = std::make_shared<entity::MRRG>(config);
  const std::string file = "./one_hop_2d_test.json";
  io::WriteMRRGToJsonFile(file, mrrg);
  boost::property_tree::ptree json;
  boost::property_tree::read_json(file, json);
  EXPECT_EQ(json.get<std::string>("network_type"), "one_hop_2d");
  EXPECT_EQ(entity::MRRGNetworkTypeFromString("one_hop_2d"),
            entity::MRRGNetworkType::kOneHop2d);
  for (const std::string name : {"one_hop_2d", "one_hop_axis2", "one_hop", "1hop"}) {
    SCOPED_TRACE(name);
    json.put("network_type", name);
    boost::property_tree::write_json(file, json);
    auto read = io::ReadMRRGFromJsonFile(file);
    EXPECT_EQ(read.GetMRRGConfig().network_type, entity::MRRGNetworkType::kOneHop2d);
    const int center = read.GetMRRGNodeId(2, 2, 0);
    std::set<std::pair<int, int>> neighbors;
    for (int node : read.GetAdjacentNodeIdVec(center)) {
      neighbors.insert(read.GetNodeProperty(node).position_id);
    }
    // The model also retains the same-PE next-context edge (a self edge at II=1).
    const std::set<std::pair<int, int>> expected = {
        {0, 2}, {1, 2}, {3, 2}, {4, 2}, {2, 0}, {2, 1}, {2, 2}, {2, 3}, {2, 4}};
    EXPECT_EQ(neighbors, expected);
  }
}

TEST(IOTest, invalid_memory_io_is_rejected_test) {
  entity::MRRGConfig config{};
  config.row = config.column = 2;
  config.context_size = 1;
  config.local_reg_size = 3;
  config.memory_io = static_cast<entity::MRRGMemoryIOType>(999);
  config.network_type = entity::MRRGNetworkType::kOrthogonal;
  config.cgra_type = entity::MRRGCGRAType::kDefault;
  EXPECT_DEATH({ entity::MRRG invalid(config); }, "");
}
