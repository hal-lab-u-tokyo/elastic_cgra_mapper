#include <gtest/gtest.h>

#include <io/mapping_io.hpp>
#include <mapper/gurobi_mapper.hpp>

TEST(IOTest, maping_io_test) {
  // create mapping
  // create dfg
  std::vector<entity::Edge> edges = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}};
  std::vector<entity::OpType> all_op = entity::GetAllOperations();
  int node_num = all_op.size();
  entity::DFGGraph g;
  for (int i = 0; i < node_num; i++) {
    auto v1 = boost::add_vertex(g);
    std::string node_name = "add" + std::to_string(i);
    g[v1].op_name = node_name;
    g[v1].op = all_op[i];
    g[v1].op_str = entity::OpTypeToString(all_op[i]);
    if (v1 < 5) {
      g[v1].const_value = i;
    }
  }

  boost::graph_traits<entity::DFGGraph>::edge_descriptor e;
  bool found;
  for (auto edge : edges) {
    boost::tie(e, found) = boost::add_edge(edge.first, edge.second, g);
    g[e].operand = edge.first % 2;
  }

  entity::DFG dfg(g);
  std::shared_ptr<entity::DFG> dfg_ptr = std::make_shared<entity::DFG>();
  *dfg_ptr = dfg;

  // create mrrg
  entity::MRRGConfig mrrg_config;

  mrrg_config.column = 4;
  mrrg_config.row = 4;
  mrrg_config.memory_io = entity::MRRGMemoryIOType::kAll;
  mrrg_config.cgra_type = entity::MRRGCGRAType::kElastic;
  mrrg_config.network_type = entity::MRRGNetworkType::kDiagonal;
  mrrg_config.local_reg_size = 3;
  mrrg_config.context_size = 2;

  std::shared_ptr<entity::MRRG> mrrg_ptr = std::make_shared<entity::MRRG>();
  *mrrg_ptr = entity::MRRG(mrrg_config);

  // exec mapping
  auto mapper_ptr = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  entity::Mapping input_mapping;
  bool is_succeed;
  const auto result = mapper_ptr->Execution();

  EXPECT_EQ(result.is_success, true);

  // write mapping
  std::string file_name = "./mapping_data.json";
  std::shared_ptr<entity::Mapping> input_mapping_ptr =
      std::make_shared<entity::Mapping>();
  *input_mapping_ptr = input_mapping;
  io::WriteMappingFile(file_name, input_mapping_ptr, mrrg_config);

  // read mapping
  entity::Mapping output_mapping = io::ReadMappingFile(file_name);

  // mapping test
  entity::ConfigMap input_config_map, output_config_map;
  input_config_map = input_mapping.GetConfigMap();
  output_config_map = output_mapping.GetConfigMap();

  for (auto key_value : input_config_map) {
    entity::CGRAConfig input_value = key_value.second;
    entity::CGRAConfig output_value = output_config_map[key_value.first];

    EXPECT_EQ(input_value.operation_type, output_value.operation_type);
    EXPECT_EQ(input_value.const_value, output_value.const_value);
  }
}