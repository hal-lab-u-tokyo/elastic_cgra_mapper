#pragma once

#include <boost/graph/adjacency_list.hpp>

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

  EdgeProperty GetEdgeProperty(
      typename boost::graph_traits<
          BaseGraph<NodeProperty, EdgeProperty, GraphProperty>>::edge_descriptor
          edge_id) const {
    return graph_[edge_id];
  }

  int GetNodeNum() { return boost::num_vertices(graph_); };

 protected:
  BaseGraph<NodeProperty, EdgeProperty, GraphProperty> graph_;
};
}  // namespace entity