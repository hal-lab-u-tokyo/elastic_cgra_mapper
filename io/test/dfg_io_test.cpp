#include <gtest/gtest.h>

#include <io/dfg_io.hpp>
#include <string>

TEST(IOTest, dfg_io_test) {
  std::string file_path = "./test_dot_data.dot";

  std::vector<entity::Edge> edges = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}};
  int node_num = 6;
  entity::Graph g(edges.begin(), edges.end(), node_num);

  entity::DFG input_dfg(g);
  io::WriteDFGDotFile(file_path, input_dfg);

  entity::DFG output_dfg = io::ReadDFGDotFile(file_path);

  int output_node_num = 0;
  auto vertex_iter = boost::vertices(output_dfg.GetGraph());
  for (vertex_iter; vertex_iter.first != vertex_iter.second;
       ++vertex_iter.first) {
    output_node_num++;
  }

  EXPECT_EQ(output_node_num, node_num);
}