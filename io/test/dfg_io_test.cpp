#include <gtest/gtest.h>

#include <io/dfg_io.hpp>
#include <string>

TEST(IOTest, dfg_io_test) {
  std::string file_path = "./test_dot_data.dot";

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
    g[v1].const_value = i;
  }

  boost::graph_traits<entity::DFGGraph>::edge_descriptor e;
  bool found;
  for (auto edge : edges) {
    boost::tie(e, found) = boost::add_edge(edge.first, edge.second, g);
    g[e].operand = edge.first % 2;
  }

  entity::DFG input_dfg(g);
  std::shared_ptr<entity::DFG> input_dfg_ptr = std::make_shared<entity::DFG>();
  *input_dfg_ptr = input_dfg;
  io::WriteDFGDotFile(file_path, input_dfg_ptr);

  entity::DFG output_dfg = io::ReadDFGDotFile(file_path);

  int output_node_num = 0;
  auto vertex_iter = boost::vertices(output_dfg.GetGraph());
  for (; vertex_iter.first != vertex_iter.second; ++vertex_iter.first) {
    output_node_num++;
  }

  EXPECT_EQ(output_node_num, node_num);  // check number of node
  for (int i = 0; i < output_node_num; i++) {
    EXPECT_EQ(output_dfg.GetNodeProperty(0).op,
              input_dfg.GetNodeProperty(0).op);  // check node property
    EXPECT_EQ(output_dfg.GetNodeProperty(0).const_value,
              input_dfg.GetNodeProperty(0).const_value);  // check node property
  }
}