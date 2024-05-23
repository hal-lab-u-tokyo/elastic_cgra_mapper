#include <gtest/gtest.h>

#include <mapper/gurobi_mapper.hpp>

TEST(MapperTest, gurobi_mapper_test) {
  // create dfg
  std::vector<entity::Edge> edges = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}};
  int node_num = 6;
  entity::DFGGraph g;
  for (int i = 0; i < node_num; i++) {
    auto v1 = boost::add_vertex(g);
    g[v1].op = entity::OpType::ADD;
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

  auto mapper_ptr = mapper::GurobiILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  const auto result = mapper_ptr->Execution();

  EXPECT_EQ(result.is_success, true);
}
