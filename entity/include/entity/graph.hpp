#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_utility.hpp>

namespace entity {
template <typename NodeProperty, typename EdgeProperty, typename GraphProperty>
class BaseGraph
    : public boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                                   NodeProperty, EdgeProperty, GraphProperty> {
 public:
  using boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                              NodeProperty, EdgeProperty,
                              GraphProperty>::adjacency_list;
};
typedef std::pair<int, int> Edge;

template <typename NodeProperty, typename EdgeProperty, typename GraphProperty>
class BaseGraphClass {
 public:
  BaseGraphClass() {
    graph_ = BaseGraph<NodeProperty, EdgeProperty, GraphProperty>();
  }
  BaseGraphClass(BaseGraph<NodeProperty, EdgeProperty, GraphProperty> graph) {
    graph_ = graph;
  };
  BaseGraph<NodeProperty, EdgeProperty, GraphProperty> GetGraph() const {
    return graph_;
  };

  NodeProperty GetNodeProperty(int vertex_id) const {
    return graph_[vertex_id];
  };

  void SetNodeProperty(int vertex_id, NodeProperty node_property) {
    graph_[vertex_id] = node_property;
    return;
  }

  EdgeProperty GetEdgeProperty(
      typename boost::graph_traits<
          BaseGraph<NodeProperty, EdgeProperty, GraphProperty>>::edge_descriptor
          edge_id) const {
    return graph_[edge_id];
  }

  int GetNodeNum() { return boost::num_vertices(graph_); };

  std::vector<int> GetAdjacentNodeIdVec(int node_id) {
    std::vector<int> adjacent_node_id_vec;
    auto pair_it_and_end = boost::adjacent_vertices(node_id, graph_);
    auto vit = pair_it_and_end.first;
    auto vend = pair_it_and_end.second;
    for (vit; vit != vend; vit++) {
      adjacent_node_id_vec.push_back(*vit);
    }

    return adjacent_node_id_vec;
  };

  bool IsReachable(int src_node_id, int to_node_id) {
    std::vector<boost::default_color_type> color(GetNodeNum(),
                                                 boost::white_color);
    return boost::is_reachable(src_node_id, to_node_id, graph_, color.data());
  }

 protected:
  BaseGraph<NodeProperty, EdgeProperty, GraphProperty> graph_;
};
}  // namespace entity