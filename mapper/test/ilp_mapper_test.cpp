#include <gtest/gtest.h>

#include <algorithm>
#include <mapper/ilp/full_routing_ilp_mapper.hpp>
#include <mapper/ilp/placement2d_ilp_mapper.hpp>
#include <mapper/mapper_factory.hpp>

TEST(MapperTest, built_in_mapper_registry_test) {
  const auto types = mapper::GetRegisteredMapperTypes();
  const auto is_registered = [&](const std::string& type) {
    return std::find(types.begin(), types.end(), type) != types.end();
  };

  EXPECT_TRUE(is_registered("Placement2DCPUMappingYOTT"));
  EXPECT_TRUE(is_registered("Placement2DCPUMappingYOTTCoreRepair"));
  EXPECT_TRUE(is_registered("Placement2DFaithfulArrayYOTT"));
  EXPECT_TRUE(is_registered("ModuloYOTT"));
  EXPECT_TRUE(is_registered("FullRoutingILPMapper"));
}

TEST(MapperTest, placement2d_ilp_mapper_test) {
  // create dfg
  std::vector<entity::Edge> edges = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}};
  int node_num = 6;
  entity::DFGGraph g;
  for (int i = 0; i < node_num; i++) {
    auto v1 = boost::add_vertex(g);
    g[v1].op = entity::OpType::kAdd;
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

  auto mapper_ptr =
      mapper::Placement2DILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  const auto result = mapper_ptr->Execution();

  EXPECT_EQ(result.is_success, true);
}

TEST(MapperTest, full_routing_ilp_mapper_test) {
  // create dfg
  std::vector<entity::Edge> edges = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}};
  int node_num = 6;
  entity::DFGGraph g;
  for (int i = 0; i < node_num; i++) {
    auto v1 = boost::add_vertex(g);
    g[v1].op = entity::OpType::kAdd;
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

  auto mapper_ptr =
      mapper::FullRoutingILPMapper().CreateMapper(dfg_ptr, mrrg_ptr);
  const auto result = mapper_ptr->Execution();

  EXPECT_EQ(result.is_success, true);
}

TEST(MapperTest, algorithm_selects_engine_from_mrrg_test) {
  // Connected operations exercise placement and routing, not just registration.
  entity::DFGGraph graph;
  for (int i = 0; i < 3; ++i) {
    auto vertex = boost::add_vertex(graph);
    graph[vertex].op = entity::OpType::kAdd;
    graph[vertex].op_name = "add";
  }
  for (int i = 0; i < 2; ++i) {
    auto edge = boost::add_edge(i, i + 1, graph).first;
    graph[edge].operand = 0;
  }
  auto dfg = std::make_shared<entity::DFG>(graph);
  for (int contexts : {1, 2}) {
    entity::MRRGConfig config{};
    config.column = 3;
    config.row = 3;
    config.memory_io = entity::MRRGMemoryIOType::kAll;
    config.cgra_type = entity::MRRGCGRAType::kDefault;
    config.network_type = entity::MRRGNetworkType::kOrthogonal;
    config.local_reg_size = 3;
    config.context_size = contexts;
    auto mrrg = std::make_shared<entity::MRRG>(config);
    const std::vector<std::pair<std::string, std::string>> variants = {
        {"SA", contexts == 1 ? "Placement2DSA" : "ModuloSA"},
        {"PRISA", contexts == 1 ? "Placement2DPRISA" : "ModuloPhysicalPRISA"},
        {"YOTO", contexts == 1 ? "Placement2DYOTO" : "ModuloYOTO"},
        {"YOTT", contexts == 1 ? "Placement2DYOTT" : "ModuloYOTT"},
        {"ILP",
         contexts == 1 ? "Placement2DILPMapper" : "FullRoutingILPMapper"},
        {"ILPMapper",
         contexts == 1 ? "Placement2DILPMapper" : "FullRoutingILPMapper"}};
    for (const auto& variant : variants) {
      SCOPED_TRACE(variant.first + " contexts=" + std::to_string(contexts));
      mapper::MapperOptions options;
      options.timeout_s = 5;
      options.random_seed = 42;
      options.max_trials = 2;
      options.max_iterations = 30;
      options.seed_count = 1;
      auto automatic = mapper::CreateMapper(variant.first, dfg, mrrg);
      auto explicit_variant = mapper::CreateMapper(variant.second, dfg, mrrg);
      automatic->Configure(options);
      explicit_variant->Configure(options);
      const auto actual = automatic->Execution();
      const auto expected = explicit_variant->Execution();
      ASSERT_TRUE(expected.is_success);
      ASSERT_TRUE(actual.is_success);
      const auto actual_configs = actual.mapping_ptr->GetConfigMap();
      const auto expected_configs = expected.mapping_ptr->GetConfigMap();
      ASSERT_EQ(actual_configs.size(), expected_configs.size());
      for (const auto& entry : expected_configs) {
        ASSERT_EQ(actual_configs.count(entry.first), 1U);
        const auto& value = actual_configs.at(entry.first);
        EXPECT_EQ(value.operation_type, entry.second.operation_type);
        EXPECT_EQ(value.operation_name, entry.second.operation_name);
        EXPECT_EQ(value.const_value, entry.second.const_value);
        EXPECT_EQ(value.from_config_id_vec, entry.second.from_config_id_vec);
        EXPECT_EQ(value.to_config_id_vec, entry.second.to_config_id_vec);
      }
    }
  }
}

TEST(MapperTest, operation_enum_values_and_serialization_test) {
  // The public spelling changes; solver/Verilog numeric encodings stay stable.
  EXPECT_EQ(static_cast<int>(entity::OpType::kAdd), 0);
  EXPECT_EQ(static_cast<int>(entity::OpType::kLoad), 9);
  EXPECT_EQ(static_cast<int>(entity::OpType::kRoute), 13);
  EXPECT_EQ(static_cast<int>(entity::OpType::kSelect), 22);
  for (auto op : entity::GetAllOperations()) {
    EXPECT_EQ(entity::OpTypeFromString(entity::OpTypeToString(op)), op);
  }
}
