#include <boost/graph/graphviz.hpp>
#include <io/dfg_io.hpp>

entity::DFG io::ReadDFGDotFile(std::string file_name) {
  entity::Graph graph;
  std::ifstream dot(file_name);

  boost::dynamic_properties dp(boost::ignore_other_properties);
  boost::read_graphviz(dot, graph, dp, "node_id");

  entity::DFG dfg(graph);
  return dfg;
}

void io::WriteDFGDotFile(std::string file_name, entity::DFG dfg) {
  std::ofstream output_file(file_name);
  boost::write_graphviz(output_file, dfg.GetGraph());

  return;
}