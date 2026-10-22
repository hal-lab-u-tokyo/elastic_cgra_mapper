#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <mapper/placement_first/engine/placement_first_engine.hpp>
#include <queue>
#include <set>
#include <unordered_map>

namespace {

bool SupportsOperation(const entity::MRRGNodeProperty& mrrg_node,
                       entity::OpType op) {
  return std::find(mrrg_node.supported_operations.begin(),
                   mrrg_node.supported_operations.end(),
                   op) != mrrg_node.supported_operations.end();
}

int NodeDegree(entity::DFG& dfg, int dfg_node_id) {
  return static_cast<int>(dfg.GetAdjacentNodeIdVec(dfg_node_id).size() +
                          dfg.GetParentNodeIdVec(dfg_node_id).size());
}

std::vector<int> BuildPlacementOrder(entity::DFG& dfg) {
  const int node_num = dfg.GetNodeNum();
  std::vector<int> order;
  std::vector<bool> selected(node_num, false);
  order.reserve(node_num);

  while (static_cast<int>(order.size()) < node_num) {
    int best_node_id = -1;
    int best_selected_neighbor_count = -1;
    int best_degree = -1;

    for (int node_id = 0; node_id < node_num; node_id++) {
      if (selected[node_id]) {
        continue;
      }

      int selected_neighbor_count = 0;
      for (int parent_id : dfg.GetParentNodeIdVec(node_id)) {
        if (selected[parent_id]) {
          selected_neighbor_count++;
        }
      }
      for (int child_id : dfg.GetAdjacentNodeIdVec(node_id)) {
        if (selected[child_id]) {
          selected_neighbor_count++;
        }
      }

      int degree = NodeDegree(dfg, node_id);
      if (selected_neighbor_count > best_selected_neighbor_count ||
          (selected_neighbor_count == best_selected_neighbor_count &&
           degree > best_degree) ||
          (selected_neighbor_count == best_selected_neighbor_count &&
           degree == best_degree && node_id < best_node_id)) {
        best_node_id = node_id;
        best_selected_neighbor_count = selected_neighbor_count;
        best_degree = degree;
      }
    }

    selected[best_node_id] = true;
    order.push_back(best_node_id);
  }

  return order;
}

int ManhattanDistance(const entity::MRRGNodeProperty& a,
                      const entity::MRRGNodeProperty& b) {
  return std::abs(a.position_id.first - b.position_id.first) +
         std::abs(a.position_id.second - b.position_id.second);
}

int GetMrrgEdgeId(entity::MRRG& mrrg, int from_node_id, int to_node_id) {
  for (int edge_id : mrrg.GetOutEdgeIdVec(from_node_id)) {
    if (mrrg.GetEdgeSourceTarget(edge_id).second == to_node_id) {
      return edge_id;
    }
  }
  return -1;
}

std::vector<int> FindRoutePath(
    entity::MRRG& mrrg, int from_node_id, int to_node_id,
    const std::set<int>& occupied_op_nodes,
    const std::unordered_map<int, int>& route_node_owner,
    const std::unordered_map<int, int>& route_edge_owner, int source_dfg_id) {
  if (from_node_id == to_node_id) {
    int self_edge_id = GetMrrgEdgeId(mrrg, from_node_id, to_node_id);
    if (self_edge_id >= 0 &&
        (route_edge_owner.count(self_edge_id) == 0 ||
         route_edge_owner.at(self_edge_id) == source_dfg_id)) {
      return {from_node_id, to_node_id};
    }
  }

  std::queue<int> q;
  std::unordered_map<int, int> parent;
  q.push(from_node_id);
  parent[from_node_id] = -1;

  while (!q.empty()) {
    int current = q.front();
    q.pop();

    for (int edge_id : mrrg.GetOutEdgeIdVec(current)) {
      int next = mrrg.GetEdgeSourceTarget(edge_id).second;
      if (route_edge_owner.count(edge_id) > 0 &&
          route_edge_owner.at(edge_id) != source_dfg_id) {
        continue;
      }
      if (next != to_node_id && occupied_op_nodes.count(next) > 0) {
        continue;
      }
      if (next != to_node_id && route_node_owner.count(next) > 0 &&
          route_node_owner.at(next) != source_dfg_id) {
        continue;
      }
      if (parent.count(next) > 0) {
        continue;
      }
      parent[next] = current;
      if (next == to_node_id) {
        std::vector<int> path;
        for (int node = to_node_id; node != -1; node = parent[node]) {
          path.push_back(node);
        }
        std::reverse(path.begin(), path.end());
        return path;
      }
      q.push(next);
    }
  }

  return {};
}

}  // namespace

namespace mapper::placement_first {
PlacementFirstEngine::PlacementFirstEngine(
    const std::shared_ptr<entity::DFG>& dfg,
    const std::shared_ptr<entity::MRRG>& mrrg)
    : dfg_ptr_(dfg),
      mrrg_ptr_(mrrg),
      mrrg_config(mrrg->GetMRRGConfig()),
      dfg_node_num(dfg->GetNodeNum()),
      mrrg_node_num(mrrg->GetNodeNum()),
      dfg_node_to_mrrg_node(dfg_node_num, -1) {}
bool PlacementFirstEngine::PlaceOperations() {
  std::vector<int> order = BuildPlacementOrder(*dfg_ptr_);

  const double center_row = (mrrg_config.row - 1) / 2.0;
  const double center_col = (mrrg_config.column - 1) / 2.0;

  for (int dfg_node_id : order) {
    entity::OpType op = dfg_ptr_->GetNodeProperty(dfg_node_id).op;
    int best_mrrg_node_id = -1;
    double best_score = std::numeric_limits<double>::infinity();

    for (int mrrg_node_id = 0; mrrg_node_id < mrrg_node_num; mrrg_node_id++) {
      if (occupied_op_nodes.count(mrrg_node_id) > 0) {
        continue;
      }
      const auto& candidate = mrrg_ptr_->GetNodeProperty(mrrg_node_id);
      if (!SupportsOperation(candidate, op)) {
        continue;
      }

      double score =
          0.5 * (std::abs(candidate.position_id.first - center_row) +
                 std::abs(candidate.position_id.second - center_col));
      int placed_neighbor_count = 0;
      for (int parent_id : dfg_ptr_->GetParentNodeIdVec(dfg_node_id)) {
        if (dfg_node_to_mrrg_node[parent_id] >= 0) {
          int distance = ManhattanDistance(
              candidate,
              mrrg_ptr_->GetNodeProperty(dfg_node_to_mrrg_node[parent_id]));
          score += 10.0 * distance;
          if (distance <= 1) {
            score -= 4.0;
          }
          placed_neighbor_count++;
        }
      }
      for (int child_id : dfg_ptr_->GetAdjacentNodeIdVec(dfg_node_id)) {
        if (dfg_node_to_mrrg_node[child_id] >= 0) {
          int distance = ManhattanDistance(
              candidate,
              mrrg_ptr_->GetNodeProperty(dfg_node_to_mrrg_node[child_id]));
          score += 8.0 * distance;
          if (distance <= 1) {
            score -= 3.0;
          }
          placed_neighbor_count++;
        }
      }
      score -= 2.0 * placed_neighbor_count;

      if (score < best_score) {
        best_score = score;
        best_mrrg_node_id = mrrg_node_id;
      }
    }

    if (best_mrrg_node_id < 0) {
      return false;
    }
    dfg_node_to_mrrg_node[dfg_node_id] = best_mrrg_node_id;
    occupied_op_nodes.emplace(best_mrrg_node_id);
  }

  return true;
}
bool PlacementFirstEngine::RouteEdges() {
  dfg_output_to_mrrg_edge.assign(dfg_node_num, {});
  std::unordered_map<int, int> route_node_owner;
  std::unordered_map<int, int> route_edge_owner;

  for (int from_dfg_id = 0; from_dfg_id < dfg_node_num; from_dfg_id++) {
    int from_mrrg_node = dfg_node_to_mrrg_node[from_dfg_id];
    for (int to_dfg_id : dfg_ptr_->GetAdjacentNodeIdVec(from_dfg_id)) {
      int to_mrrg_node = dfg_node_to_mrrg_node[to_dfg_id];
      std::vector<int> path = FindRoutePath(
          *mrrg_ptr_, from_mrrg_node, to_mrrg_node, occupied_op_nodes,
          route_node_owner, route_edge_owner, from_dfg_id);
      if (path.size() < 2) {
        return false;
      }

      for (size_t i = 0; i + 1 < path.size(); i++) {
        int edge_id = GetMrrgEdgeId(*mrrg_ptr_, path[i], path[i + 1]);
        if (edge_id < 0) {
          return false;
        }
        dfg_output_to_mrrg_edge[from_dfg_id].push_back(edge_id);
        route_edge_owner[edge_id] = from_dfg_id;
        if (i + 1 < path.size() - 1) {
          route_node_owner[path[i + 1]] = from_dfg_id;
        }
      }
    }
  }

  return true;
}
MappingResult PlacementFirstEngine::Finish(
    std::chrono::system_clock::time_point start_time) {
  entity::Mapping mapping = entity::GenerateMappingFromRoutingResult(
      *mrrg_ptr_, *dfg_ptr_, dfg_node_to_mrrg_node, dfg_output_to_mrrg_edge);
  const auto end_time = std::chrono::system_clock::now();
  const double mapping_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                            start_time)
          .count() /
      1000.0;
  return mapper::MappingResult(true, mapping, mapping_time);
}
MappingResult PlacementFirstEngine::Failure(
    std::chrono::system_clock::time_point start_time) const {
  const double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now() - start_time)
                             .count() /
                         1000.0;
  return MappingResult(false, entity::Mapping(mrrg_config), elapsed);
}
}  // namespace mapper::placement_first
