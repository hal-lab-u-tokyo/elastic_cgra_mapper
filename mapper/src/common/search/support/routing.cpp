#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

std::optional<RouteUsage> PlacementSearchEngine::TryRoutePlacement(
    const PlacementState& state) {
  std::vector<std::vector<DFGEdgeInfo>> route_orders;
  route_orders.push_back(dfg_edges_);

  route_orders.push_back(dfg_edges_);
  std::sort(route_orders.back().begin(), route_orders.back().end(),
            [&](const auto& a, const auto& b) {
              const int da = DirectedResourceDistance(
                  state.dfg_to_mrrg[a.source], state.dfg_to_mrrg[a.target]);
              const int db = DirectedResourceDistance(
                  state.dfg_to_mrrg[b.source], state.dfg_to_mrrg[b.target]);
              return da > db;
            });

  route_orders.push_back(route_orders.back());
  std::reverse(route_orders.back().begin(), route_orders.back().end());

  route_orders.push_back(dfg_edges_);
  std::sort(route_orders.back().begin(), route_orders.back().end(),
            [&](const auto& a, const auto& b) {
              const int degree_a =
                  annotation_.degree[a.source] + annotation_.degree[a.target];
              const int degree_b =
                  annotation_.degree[b.source] + annotation_.degree[b.target];
              if (degree_a != degree_b) return degree_a > degree_b;
              return a.id < b.id;
            });

  const int retry_count = RoutingRetryCount();
  while (static_cast<int>(route_orders.size()) < retry_count) {
    route_orders.push_back(dfg_edges_);
    std::shuffle(route_orders.back().begin(), route_orders.back().end(), rng_);
  }

  int attempts = 0;
  for (const auto& route_order : route_orders) {
    if (attempts >= retry_count) break;
    attempts++;
    auto usage = TryRoutePlacementInOrder(state, route_order);
    if (usage.has_value()) return usage;
  }
  return std::nullopt;
}

std::optional<RouteUsage> PlacementSearchEngine::TryRoutePlacementInOrder(
    const PlacementState& state,
    const std::vector<DFGEdgeInfo>& route_order) const {
  RouteUsage usage;
  usage.route_node_owner.assign(mrrg_.GetNodeNum(), -1);
  usage.route_edge_owner.assign(mrrg_.GetEdgeNum(), -1);
  usage.source_to_route_edges.assign(dfg_.GetNodeNum(), {});

  for (const auto& edge : route_order) {
    const int source_node = state.dfg_to_mrrg[edge.source];
    const int target_node = state.dfg_to_mrrg[edge.target];
    auto path = UsesManhattanRouting()
                    ? FindManhattanRoutePath(edge.source, source_node,
                                             target_node, state, usage)
                    : FindRoutePath(edge.source, source_node, target_node,
                                    state, usage);
    if (!path.has_value()) return std::nullopt;
    ReserveRoute(edge.source, source_node, target_node, *path, state, usage);
  }
  return usage;
}

std::optional<PathResult> PlacementSearchEngine::FindRoutePath(
    int source_dfg_node, int source_mrrg, int target_mrrg,
    const PlacementState& state, const RouteUsage& usage) const {
  if (source_mrrg != target_mrrg) {
    return FindRoutePathNonEmptyAllowed(source_dfg_node, source_mrrg,
                                        target_mrrg, state, usage, false);
  }

  // Self-loop/recurrent edge: force at least one MRRG edge.
  for (int first_edge : mrrg_.GetOutEdgeIdVec(source_mrrg)) {
    if (!CanUseEdge(first_edge, source_dfg_node, usage)) continue;
    auto edge_pair = mrrg_.GetEdgeSourceTarget(first_edge);
    int first_to = edge_pair.second;
    if (!CanUseNode(first_to, source_dfg_node, source_mrrg, target_mrrg, state,
                    usage)) {
      continue;
    }
    if (first_to == target_mrrg) {
      return PathResult{{first_edge}, {source_mrrg, target_mrrg}};
    }
    auto suffix = FindRoutePathNonEmptyAllowed(
        source_dfg_node, first_to, target_mrrg, state, usage, false);
    if (!suffix.has_value()) continue;
    PathResult result;
    result.edge_ids.push_back(first_edge);
    result.edge_ids.insert(result.edge_ids.end(), suffix->edge_ids.begin(),
                           suffix->edge_ids.end());
    result.node_ids.push_back(source_mrrg);
    result.node_ids.insert(result.node_ids.end(), suffix->node_ids.begin(),
                           suffix->node_ids.end());
    return result;
  }
  return std::nullopt;
}

int PlacementSearchEngine::ManhattanRouteScore(int from_mrrg_node,
                                               int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
  if (from_mrrg_node == to_mrrg_node) return 0;
  const auto from = mrrg_.GetNodeProperty(from_mrrg_node);
  const auto to = mrrg_.GetNodeProperty(to_mrrg_node);
  const int context_size = std::max(1, from.context_size);
  const int context_delta =
      (to.context_id - from.context_id + context_size) % context_size;
  return SpatialStepDistance(from_mrrg_node, to_mrrg_node) + context_delta;
}

std::optional<PathResult> PlacementSearchEngine::FindManhattanRoutePath(
    int source_dfg_node, int source_mrrg, int target_mrrg,
    const PlacementState& state, const RouteUsage& usage) const {
  if (source_mrrg != target_mrrg) {
    return FindManhattanRoutePathNonEmptyAllowed(
        source_dfg_node, source_mrrg, target_mrrg, state, usage, false);
  }

  // Recurrent/self edges still need a real route edge. Prefer the outgoing
  // step that best follows the target in context/mesh Manhattan distance.
  std::vector<int> first_edges = mrrg_.GetOutEdgeIdVec(source_mrrg);
  std::stable_sort(first_edges.begin(), first_edges.end(),
                   [&](int lhs, int rhs) {
                     const int lto = mrrg_.GetEdgeSourceTarget(lhs).second;
                     const int rto = mrrg_.GetEdgeSourceTarget(rhs).second;
                     return ManhattanRouteScore(lto, target_mrrg) <
                            ManhattanRouteScore(rto, target_mrrg);
                   });
  for (int first_edge : first_edges) {
    if (!CanUseEdge(first_edge, source_dfg_node, usage)) continue;
    auto edge_pair = mrrg_.GetEdgeSourceTarget(first_edge);
    int first_to = edge_pair.second;
    if (!CanUseNode(first_to, source_dfg_node, source_mrrg, target_mrrg, state,
                    usage)) {
      continue;
    }
    if (first_to == target_mrrg) {
      return PathResult{{first_edge}, {source_mrrg, target_mrrg}};
    }
    auto suffix = FindManhattanRoutePathNonEmptyAllowed(
        source_dfg_node, first_to, target_mrrg, state, usage, false);
    if (!suffix.has_value()) continue;
    PathResult result;
    result.edge_ids.push_back(first_edge);
    result.edge_ids.insert(result.edge_ids.end(), suffix->edge_ids.begin(),
                           suffix->edge_ids.end());
    result.node_ids.push_back(source_mrrg);
    result.node_ids.insert(result.node_ids.end(), suffix->node_ids.begin(),
                           suffix->node_ids.end());
    return result;
  }
  return std::nullopt;
}

std::optional<PathResult>
PlacementSearchEngine::FindManhattanRoutePathNonEmptyAllowed(
    int source_dfg_node, int source_mrrg, int target_mrrg,
    const PlacementState& state, const RouteUsage& usage,
    bool forbid_zero_length) const {
  if (!forbid_zero_length && source_mrrg == target_mrrg) {
    return PathResult{{}, {source_mrrg}};
  }

  struct QueueItem {
    int priority = 0;
    int steps = 0;
    int node = -1;
    bool operator<(const QueueItem& other) const {
      if (priority != other.priority) return priority > other.priority;
      return steps > other.steps;
    }
  };

  const int node_num = mrrg_.GetNodeNum();
  std::vector<int> parent_node(node_num, -1);
  std::vector<int> parent_edge(node_num, -1);
  std::vector<int> best_steps(node_num, kInfDistance);
  std::priority_queue<QueueItem> q;
  parent_node[source_mrrg] = source_mrrg;
  best_steps[source_mrrg] = 0;
  q.push({ManhattanRouteScore(source_mrrg, target_mrrg), 0, source_mrrg});

  while (!q.empty()) {
    const QueueItem item = q.top();
    q.pop();
    if (item.steps != best_steps[item.node]) continue;

    std::vector<int> edge_ids = mrrg_.GetOutEdgeIdVec(item.node);
    std::stable_sort(edge_ids.begin(), edge_ids.end(), [&](int lhs, int rhs) {
      const int lto = mrrg_.GetEdgeSourceTarget(lhs).second;
      const int rto = mrrg_.GetEdgeSourceTarget(rhs).second;
      return ManhattanRouteScore(lto, target_mrrg) <
             ManhattanRouteScore(rto, target_mrrg);
    });
    for (int edge_id : edge_ids) {
      if (!CanUseEdge(edge_id, source_dfg_node, usage)) continue;
      const auto edge_pair = mrrg_.GetEdgeSourceTarget(edge_id);
      int v = edge_pair.second;
      const int next_steps = item.steps + 1;
      if (next_steps >= best_steps[v]) continue;
      if (!CanUseNode(v, source_dfg_node, source_mrrg, target_mrrg, state,
                      usage)) {
        continue;
      }
      parent_node[v] = item.node;
      parent_edge[v] = edge_id;
      best_steps[v] = next_steps;
      if (v == target_mrrg) {
        PathResult result;
        int cur = target_mrrg;
        result.node_ids.push_back(cur);
        while (cur != source_mrrg) {
          int e = parent_edge[cur];
          result.edge_ids.push_back(e);
          cur = parent_node[cur];
          result.node_ids.push_back(cur);
        }
        std::reverse(result.edge_ids.begin(), result.edge_ids.end());
        std::reverse(result.node_ids.begin(), result.node_ids.end());
        return result;
      }
      q.push({next_steps + ManhattanRouteScore(v, target_mrrg), next_steps, v});
    }
  }
  return std::nullopt;
}

std::optional<PathResult> PlacementSearchEngine::FindRoutePathNonEmptyAllowed(
    int source_dfg_node, int source_mrrg, int target_mrrg,
    const PlacementState& state, const RouteUsage& usage,
    bool forbid_zero_length) const {
  if (!forbid_zero_length && source_mrrg == target_mrrg) {
    return PathResult{{}, {source_mrrg}};
  }

  const int node_num = mrrg_.GetNodeNum();
  std::vector<int> parent_node(node_num, -1);
  std::vector<int> parent_edge(node_num, -1);
  std::queue<int> q;
  parent_node[source_mrrg] = source_mrrg;
  q.push(source_mrrg);

  while (!q.empty()) {
    int u = q.front();
    q.pop();
    for (int edge_id : mrrg_.GetOutEdgeIdVec(u)) {
      if (!CanUseEdge(edge_id, source_dfg_node, usage)) continue;
      const auto edge_pair = mrrg_.GetEdgeSourceTarget(edge_id);
      int v = edge_pair.second;
      if (parent_node[v] != -1) continue;
      if (!CanUseNode(v, source_dfg_node, source_mrrg, target_mrrg, state,
                      usage)) {
        continue;
      }
      parent_node[v] = u;
      parent_edge[v] = edge_id;
      if (v == target_mrrg) {
        PathResult result;
        int cur = target_mrrg;
        result.node_ids.push_back(cur);
        while (cur != source_mrrg) {
          int e = parent_edge[cur];
          result.edge_ids.push_back(e);
          cur = parent_node[cur];
          result.node_ids.push_back(cur);
        }
        std::reverse(result.edge_ids.begin(), result.edge_ids.end());
        std::reverse(result.node_ids.begin(), result.node_ids.end());
        return result;
      }
      q.push(v);
    }
  }
  return std::nullopt;
}

bool PlacementSearchEngine::CanUseEdge(int edge_id, int source_dfg_node,
                                       const RouteUsage& usage) const {
  return usage.route_edge_owner[edge_id] == -1 ||
         usage.route_edge_owner[edge_id] == source_dfg_node;
}

bool PlacementSearchEngine::CanUseNode(int mrrg_node_id, int source_dfg_node,
                                       int source_mrrg, int target_mrrg,
                                       const PlacementState& state,
                                       const RouteUsage& usage) const {
  if (mrrg_node_id == source_mrrg || mrrg_node_id == target_mrrg) return true;
  if (state.mrrg_to_dfg[mrrg_node_id] != -1) return false;
  if (!SupportsOperation(mrrg_.GetNodeProperty(mrrg_node_id),
                         entity::OpType::ROUTE)) {
    return false;
  }
  return usage.route_node_owner[mrrg_node_id] == -1 ||
         usage.route_node_owner[mrrg_node_id] == source_dfg_node;
}

void PlacementSearchEngine::ReserveRoute(int source_dfg_node, int source_mrrg,
                                         int target_mrrg,
                                         const PathResult& path,
                                         const PlacementState& state,
                                         RouteUsage& usage) const {
  for (int edge_id : path.edge_ids) {
    if (usage.route_edge_owner[edge_id] == -1) {
      usage.route_edge_owner[edge_id] = source_dfg_node;
    }
    usage.source_to_route_edges[source_dfg_node].push_back(edge_id);
  }
  for (int node_id : path.node_ids) {
    if (node_id == source_mrrg || node_id == target_mrrg) continue;
    if (state.mrrg_to_dfg[node_id] != -1) continue;
    if (usage.route_node_owner[node_id] == -1) {
      usage.route_node_owner[node_id] = source_dfg_node;
    }
  }
}

int PlacementSearchEngine::RouteEdgeCount(const RouteUsage& usage) const {
  int result = 0;
  for (const auto& edges : usage.source_to_route_edges) {
    std::set<int> unique_edges(edges.begin(), edges.end());
    result += static_cast<int>(unique_edges.size());
  }
  return result;
}

}  // namespace mapper::detail::placement_search
