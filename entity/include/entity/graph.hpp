#pragma once

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graph_utility.hpp>
#include <map>

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

  int GetNodeNum() const { return boost::num_vertices(graph_); };

  int GetEdgeNum() const { return boost::num_edges(graph_); };

  typename boost::graph_traits<
      BaseGraph<NodeProperty, EdgeProperty, GraphProperty>>::vertex_descriptor
  AddNode(const NodeProperty property) {
    auto v = boost::add_vertex(graph_);
    graph_[v].op = property.op;
    graph_[v].op_name = property.op_name;
    graph_[v].op_str = property.op_str;
    graph_[v].const_value = property.const_value;

    return v;
  }

  std::vector<int> GetAdjacentNodeIdVec(int node_id) const {
    std::vector<int> adjacent_node_id_vec;
    auto pair_it_and_end = boost::adjacent_vertices(node_id, graph_);
    auto vit = pair_it_and_end.first;
    auto vend = pair_it_and_end.second;
    for (vit; vit != vend; vit++) {
      adjacent_node_id_vec.push_back(*vit);
    }

    return adjacent_node_id_vec;
  };

  std::vector<int> GetParentNodeIdVec(int node_id) {
    if (child_to_parent_map_.empty()) {
      SetChildToParentMap();
    }
    return child_to_parent_map_.at(node_id);
  }

  bool IsReachable(int src_node_id, int to_node_id) const {
    std::vector<boost::default_color_type> color(GetNodeNum(),
                                                 boost::white_color);
    return boost::is_reachable(src_node_id, to_node_id, graph_, color.data());
  }

 protected:
  BaseGraph<NodeProperty, EdgeProperty, GraphProperty> graph_;
  std::unordered_map<int, std::vector<int>> child_to_parent_map_;

  void SetChildToParentMap() {
    for (int node_id = 0; node_id < GetNodeNum(); node_id++) {
      child_to_parent_map_.insert({node_id, std::vector<int>()});
    }
    for (int node_id = 0; node_id < GetNodeNum(); node_id++) {
      std::vector<int> adj_node_id_vec = GetAdjacentNodeIdVec(node_id);
      for (int adj_node_id : adj_node_id_vec) {
        child_to_parent_map_.at(adj_node_id).push_back(node_id);
      }
    }
    return;
  }
};
}  // namespace entity
