#include <boost/graph/graphviz.hpp>
#include <io/dfg_io.hpp>
#include <iostream>

entity::DFG io::ReadDFGDotFile(std::string file_name) {
  entity::DFGGraph graph;
  std::ifstream dot(file_name);

  boost::dynamic_properties dp(boost::ignore_other_properties);
  dp.property("opcode", boost::get(&entity::DFGNodeProperty::op_str, graph));
  dp.property("operand", boost::get(&entity::DFGEdgeProperty::operand, graph));
  dp.property("node_id", boost::get(&entity::DFGNodeProperty::op_name, graph));
  boost::read_graphviz(dot, graph, dp);

  for (size_t i = 0; i < boost::num_vertices(graph); i++) {
    graph[i].op = entity::OpTypeFromString(graph[i].op_str);
  }

  entity::DFG dfg(graph);
  return dfg;
}

class DFGVertexPropertyWriter {
 public:
  DFGVertexPropertyWriter(std::shared_ptr<entity::DFG> dfg_ptr)
      : dfg_ptr_(dfg_ptr){};

  template <class Vertex>
  void operator()(std::ostream& out, const Vertex& v) const {
    entity::OpType op = dfg_ptr_->GetNodeProperty(v).op;
    out << "[opcode=\"" << entity::OpTypeToString(op) << "\"]";
  }

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
};

class DFGEdgePropertyWriter {
 public:
  DFGEdgePropertyWriter(std::shared_ptr<entity::DFG> dfg_ptr)
      : dfg_ptr_(dfg_ptr){};

  template <class Edge>
  void operator()(std::ostream& out, const Edge& v) const {
    out << "[operand=\"" << dfg_ptr_->GetEdgeProperty(v).operand << "\"]";
  }

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
};

void io::WriteDFGDotFile(std::string file_name,
                         const std::shared_ptr<entity::DFG> dfg_ptr) {
  std::ofstream output_file(file_name);

  entity::DFGGraph graph = dfg_ptr->GetGraph();
  boost::dynamic_properties dp(boost::ignore_other_properties);
  dp.property("opcode", boost::get(&entity::DFGNodeProperty::op_str, graph));
  dp.property("operand", boost::get(&entity::DFGEdgeProperty::operand, graph));
  dp.property("node_id", boost::get(&entity::DFGNodeProperty::op_name, graph));

  boost::write_graphviz_dp(output_file, dfg_ptr->GetGraph(), dp);
  return;
}