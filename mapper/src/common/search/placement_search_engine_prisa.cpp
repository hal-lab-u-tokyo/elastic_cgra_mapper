#include "placement_search_engine_internal.hpp"

namespace mapper::detail::placement_search {

std::optional<PlacementState> PlacementSearchEngine::RunPRISAMultiSeed(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> best_routeable;
  double best_routeable_cost = std::numeric_limits<double>::infinity();
  std::optional<PlacementState> best_unrouted;
  double best_unrouted_cost = std::numeric_limits<double>::infinity();
  int attempted_seeds = 0;

  for (int seed_index = 0; seed_index < SeedCount() && !HasTimedOut(start);
       seed_index++) {
    ResetSeed(seed_index);
    attempted_seeds++;
    auto placement = RunPRISA(start);
    if (!placement.has_value()) continue;
    const double cost = PlacementCost(*placement);
    if (cost < best_unrouted_cost) {
      best_unrouted = placement;
      best_unrouted_cost = cost;
    }
    if (options_.placement_only) continue;
    if (TryRoutePlacement(*placement).has_value() &&
        cost < best_routeable_cost) {
      best_routeable = placement;
      best_routeable_cost = cost;
    }
  }
  Log("prisa_search seeds=" + std::to_string(attempted_seeds) +
      " sis=" + (UsesPRISASIS() ? "1" : "0") +
      " routeable=" + (best_routeable.has_value() ? "1" : "0"));
  if (options_.placement_only) return best_unrouted;
  if (best_routeable.has_value()) return best_routeable;
  return best_unrouted;
}

int PlacementSearchEngine::PRISAMaxIterations() const {
  if (options_.max_iterations.has_value()) {
    return std::max(1, options_.max_iterations.value());
  }
  return std::max(1, dfg_.GetNodeNum() * 10);
}

const std::vector<int>& PlacementSearchEngine::Placement2DResourceOrder() const {
  if (IsPRISALike() && !prisa_resources_.empty()) return prisa_resources_;
  return placement2d_resources_;
}

const std::vector<int>& PlacementSearchEngine::ResourceOrderIndex() const {
  if (IsPRISALike() && !prisa_resource_order_index_.empty()) {
    return prisa_resource_order_index_;
  }
  return placement2d_resource_order_index_;
}

int PlacementSearchEngine::PRISAResourceSideLength() const {
  const auto& resources = Placement2DResourceOrder();
  if (resources.empty()) return 1;
  int min_row = std::numeric_limits<int>::max();
  int max_row = std::numeric_limits<int>::min();
  int min_column = std::numeric_limits<int>::max();
  int max_column = std::numeric_limits<int>::min();
  for (int resource : resources) {
    const auto position = mrrg_.GetNodeProperty(resource).position_id;
    min_row = std::min(min_row, position.first);
    max_row = std::max(max_row, position.first);
    min_column = std::min(min_column, position.second);
    max_column = std::max(max_column, position.second);
  }
  return std::max(max_row - min_row + 1, max_column - min_column + 1);
}

int PlacementSearchEngine::PRISAWeakPairCount(int resource_count) const {
  const int k = std::max(1, PRISAResourceSideLength());
  if (k < 2 || resource_count < 2) return 0;
  const long long kk = k;
  const long long numerator =
      kk * kk * kk * kk + 2 * kk * kk * kk - kk * kk - 2 * kk;
  const long long paper_weak_pairs = numerator / 12;
  const long long total_pairs =
      static_cast<long long>(resource_count) * (resource_count - 1) / 2;
  return static_cast<int>(
      std::max(0LL, std::min(paper_weak_pairs, total_pairs)));
}

void PlacementSearchEngine::BuildPRISADistanceRegions() {
  const auto& resources = Placement2DResourceOrder();
  const int resource_count = static_cast<int>(resources.size());
  prisa_distance_matrix_.assign(resource_count * resource_count, 0);
  prisa_weak_region_matrix_.assign(resource_count * resource_count, 0);
  prisa_weak_distance_threshold_ = kInfDistance;
  if (resource_count < 2) return;

  std::vector<int> pair_distances;
  pair_distances.reserve(resource_count * (resource_count - 1) / 2);
  for (int row = 0; row < resource_count; row++) {
    for (int column = 0; column < resource_count; column++) {
      if (row == column) continue;
      const int distance = DirectedDistanceCost(
          ResourceDistance(resources[row], resources[column]));
      prisa_distance_matrix_[row * resource_count + column] = distance;
      if (row < column) pair_distances.push_back(distance);
    }
  }

  const int weak_pair_count = PRISAWeakPairCount(resource_count);
  if (weak_pair_count <= 0 || pair_distances.empty()) return;
  // PRISA defines PR/WR on the resource distance matrix. WR consists of the
  // heavy-distance entries; SIS only supplies a low-bandwidth initial
  // permutation before this distance-matrix-guided search.
  std::sort(pair_distances.begin(), pair_distances.end(),
            std::greater<int>());
  const int threshold_index =
      std::min(weak_pair_count, static_cast<int>(pair_distances.size())) - 1;
  prisa_weak_distance_threshold_ = pair_distances[threshold_index];

  for (int row = 0; row < resource_count; row++) {
    for (int column = 0; column < resource_count; column++) {
      if (row == column) continue;
      prisa_weak_region_matrix_[row * resource_count + column] =
          prisa_distance_matrix_[row * resource_count + column] >=
          prisa_weak_distance_threshold_;
    }
  }
}

bool PlacementSearchEngine::IsPRISAWeakRegion(int row, int column, int resource_count) const {
  if (row == column) return false;
  if (row < 0 || column < 0 || row >= resource_count ||
      column >= resource_count) {
    return false;
  }
  if (static_cast<int>(prisa_weak_region_matrix_.size()) ==
      resource_count * resource_count) {
    return prisa_weak_region_matrix_[row * resource_count + column] != 0;
  }
  if (static_cast<int>(prisa_distance_matrix_.size()) ==
      resource_count * resource_count) {
    return prisa_distance_matrix_[row * resource_count + column] >=
           prisa_weak_distance_threshold_;
  }
  return false;
}

bool PlacementSearchEngine::IsPRISAPotentialRegion(int row, int column,
                            int resource_count) const {
  if (row == column) return false;
  return !IsPRISAWeakRegion(row, column, resource_count);
}

std::vector<int> PlacementSearchEngine::LowBandwidthNodeOrder() {
  const int n = dfg_.GetNodeNum();
  if (n == 0) return {};

  auto bfs_distance = [&](int root,
                          const std::vector<char>* allowed = nullptr) {
    std::vector<int> distance(n, -1);
    std::queue<int> q;
    distance[root] = 0;
    q.push(root);
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = OtherEndpoint(dfg_edges_[edge_id], u);
        if (v < 0 || distance[v] >= 0) continue;
        if (allowed != nullptr && !(*allowed)[v]) continue;
        distance[v] = distance[u] + 1;
        q.push(v);
      }
    }
    return distance;
  };

  std::vector<char> visited(n, 0);
  std::vector<std::vector<int>> components;
  for (int root = 0; root < n; root++) {
    if (visited[root]) continue;
    std::vector<int> component;
    std::queue<int> q;
    visited[root] = 1;
    q.push(root);
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      component.push_back(u);
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = OtherEndpoint(dfg_edges_[edge_id], u);
        if (v < 0 || visited[v]) continue;
        visited[v] = 1;
        q.push(v);
      }
    }
    components.push_back(std::move(component));
  }
  std::sort(components.begin(), components.end(),
            [&](const std::vector<int>& a, const std::vector<int>& b) {
              if (a.size() != b.size()) return a.size() > b.size();
              const auto max_degree = [&](const std::vector<int>& component) {
                int result = -1;
                for (int node : component) {
                  result = std::max(result, annotation_.degree[node]);
                }
                return result;
              };
              const int a_degree = max_degree(a);
              const int b_degree = max_degree(b);
              if (a_degree != b_degree) return a_degree > b_degree;
              return *std::min_element(a.begin(), a.end()) <
                     *std::min_element(b.begin(), b.end());
            });

  std::vector<int> order;
  order.reserve(n);
  for (const auto& component : components) {
    std::vector<char> in_component(n, 0);
    for (int node : component) in_component[node] = 1;

    int highest_degree = component.front();
    for (int node : component) {
      if (annotation_.degree[node] > annotation_.degree[highest_degree] ||
          (annotation_.degree[node] == annotation_.degree[highest_degree] &&
           node < highest_degree)) {
        highest_degree = node;
      }
    }

    const auto distance_from_high_degree =
        bfs_distance(highest_degree, &in_component);
    int max_distance = 0;
    std::vector<int> farthest_nodes;
    for (int node : component) {
      if (distance_from_high_degree[node] < 0) continue;
      if (distance_from_high_degree[node] > max_distance) {
        max_distance = distance_from_high_degree[node];
        farthest_nodes.clear();
      }
      if (distance_from_high_degree[node] == max_distance) {
        farthest_nodes.push_back(node);
      }
    }
    if (farthest_nodes.empty()) farthest_nodes.push_back(highest_degree);
    const int start = *std::min_element(
        farthest_nodes.begin(), farthest_nodes.end(), [&](int a, int b) {
          if (annotation_.degree[a] != annotation_.degree[b]) {
            return annotation_.degree[a] < annotation_.degree[b];
          }
          return a < b;
        });

    // Algorithm 2 is a low-bandwidth labeling. Keep the component-local BFS
    // visitation order instead of re-sorting entire levels, because the
    // latter breaks parent-neighbor locality on wide DFGs such as fir16.
    std::vector<char> labeled(n, 0);
    std::queue<int> q;
    labeled[start] = 1;
    q.push(start);
    while (!q.empty()) {
      const int u = q.front();
      q.pop();
      order.push_back(u);

      std::vector<int> neighbors;
      for (int edge_id : incident_edge_ids_[u]) {
        const int v = OtherEndpoint(dfg_edges_[edge_id], u);
        if (v < 0 || !in_component[v] || labeled[v]) continue;
        labeled[v] = 1;
        neighbors.push_back(v);
      }
      std::sort(neighbors.begin(), neighbors.end(), [&](int a, int b) {
        if (annotation_.degree[a] != annotation_.degree[b]) {
          return annotation_.degree[a] < annotation_.degree[b];
        }
        return a < b;
      });
      for (int neighbor : neighbors) q.push(neighbor);
    }

    for (int node : component) {
      if (!labeled[node]) order.push_back(node);
    }
  }
  return order;
}

std::optional<PlacementState> PlacementSearchEngine::ConstructPRISAInitialPlacementFromOrder(
    const std::vector<int>& node_order) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.empty()) return std::nullopt;
  PlacementState state;
  state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
  state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
  for (int label = 0; label < static_cast<int>(node_order.size()); label++) {
    const int node = node_order[label];
    int best_resource = -1;
    int best_distance = kInfDistance;
    for (int order_index = 0; order_index < static_cast<int>(resources.size());
         order_index++) {
      const int r = resources[order_index];
      if (!CanOccupyResource(node, r, state)) continue;
      const int label_distance = std::abs(order_index - label);
      if (label_distance < best_distance) {
        best_distance = label_distance;
        best_resource = r;
      }
    }
    if (best_resource < 0) return std::nullopt;
    state.dfg_to_mrrg[node] = best_resource;
    state.mrrg_to_dfg[best_resource] = node;
  }
  return state;
}

int PlacementSearchEngine::PRISAInitialSolutionSampleCount() const {
  if (!UsesPRISASIS()) return 1;
  return 1;
}

std::optional<PlacementState> PlacementSearchEngine::ConstructPRISAInitialPlacement() {
  if (!UsesPRISASIS()) return RandomLegalPlacement();
  std::optional<PlacementState> best;
  double best_cost = std::numeric_limits<double>::infinity();
  const int sample_count = PRISAInitialSolutionSampleCount();
  for (int sample = 0; sample < sample_count; sample++) {
    auto placement =
        ConstructPRISAInitialPlacementFromOrder(LowBandwidthNodeOrder());
    if (!placement.has_value()) continue;
    const double cost = PlacementCost(*placement);
    if (cost < best_cost) {
      best = placement;
      best_cost = cost;
    }
  }
  return best;
}

bool PlacementSearchEngine::CanSwapResources(const PlacementState& state, int ra, int rb) const {
  if (ra == rb || ra < 0 || rb < 0) return false;
  const int a = state.mrrg_to_dfg[ra];
  const int b = state.mrrg_to_dfg[rb];
  if (a < 0 && b < 0) return false;
  if (a >= 0 && !IsCompatible(a, rb)) return false;
  if (b >= 0 && !IsCompatible(b, ra)) return false;
  return true;
}

bool PlacementSearchEngine::ApplyResourceSwap(PlacementState& state, int ra, int rb) const {
  if (!CanSwapResources(state, ra, rb)) return false;
  const int a = state.mrrg_to_dfg[ra];
  const int b = state.mrrg_to_dfg[rb];
  state.mrrg_to_dfg[ra] = b;
  state.mrrg_to_dfg[rb] = a;
  if (a >= 0) state.dfg_to_mrrg[a] = rb;
  if (b >= 0) state.dfg_to_mrrg[b] = ra;
  return true;
}

bool PlacementSearchEngine::ApplyRandomResourceSwap(PlacementState& state,
                             const std::vector<int>& resources) {
  if (resources.size() < 2) return false;
  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(resources.size()) - 1);
  for (int attempt = 0; attempt < 32; attempt++) {
    const int ra = resources[dist(rng_)];
    const int rb = resources[dist(rng_)];
    if (ApplyResourceSwap(state, ra, rb)) return true;
  }
  return false;
}

bool PlacementSearchEngine::ApplyPlacementCostSampledSwap(PlacementState& state,
                                   const std::vector<int>& resources) {
  if (resources.size() < 2) return false;

  struct Candidate {
    double cost_delta = std::numeric_limits<double>::infinity();
    int first_resource = -1;
    int second_resource = -1;
    int random_order = 0;
  };

  const int resource_count = static_cast<int>(resources.size());
  const int total_pairs = resource_count * (resource_count - 1) / 2;
  const int sample_count =
      std::min(total_pairs, std::max(16, resource_count * 4));

  auto normalize_pair = [](int lhs, int rhs) {
    if (lhs > rhs) std::swap(lhs, rhs);
    return std::pair<int, int>(lhs, rhs);
  };

  std::set<std::pair<int, int>> sampled_pairs;
  if (sample_count == total_pairs) {
    for (int i = 0; i < resource_count; i++) {
      for (int j = i + 1; j < resource_count; j++) {
        sampled_pairs.insert({i, j});
      }
    }
  } else {
    std::uniform_int_distribution<int> resource_dist(0, resource_count - 1);
    int attempts = 0;
    while (static_cast<int>(sampled_pairs.size()) < sample_count &&
           attempts < sample_count * 16) {
      attempts++;
      const int first = resource_dist(rng_);
      const int second = resource_dist(rng_);
      if (first == second) continue;
      sampled_pairs.insert(normalize_pair(first, second));
    }
  }

  std::vector<Candidate> candidates;
  candidates.reserve(sampled_pairs.size());
  int random_order = 0;
  for (const auto& [first_index, second_index] : sampled_pairs) {
    const int first_resource = resources[first_index];
    const int second_resource = resources[second_index];
    if (!CanSwapResources(state, first_resource, second_resource)) continue;
    candidates.push_back(Candidate{
        PlacementCostDeltaForSwap(state, first_resource, second_resource),
        first_resource, second_resource, random_order++});
  }
  if (candidates.empty()) return ApplyRandomResourceSwap(state, resources);

  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.cost_delta != b.cost_delta) {
                return a.cost_delta < b.cost_delta;
              }
              return a.random_order < b.random_order;
            });

  int selection_count = 1;
  while (selection_count < static_cast<int>(candidates.size()) &&
         candidates[selection_count].cost_delta ==
             candidates.front().cost_delta) {
    selection_count++;
  }
  std::uniform_int_distribution<int> dist(0, selection_count - 1);
  const Candidate& chosen = candidates[dist(rng_)];
  return ApplyResourceSwap(state, chosen.first_resource,
                           chosen.second_resource);
}

int PlacementSearchEngine::ResourceAfterSwap(int dfg_node_id, int first_resource,
                      int second_resource,
                      const PlacementState& state) const {
  int resource = state.dfg_to_mrrg[dfg_node_id];
  if (resource == first_resource) return second_resource;
  if (resource == second_resource) return first_resource;
  return resource;
}

double PlacementSearchEngine::PlacementCostDeltaForSwap(const PlacementState& state,
                                 int first_resource,
                                 int second_resource) const {
  std::vector<int> affected_edges;
  const int first_node = state.mrrg_to_dfg[first_resource];
  const int second_node = state.mrrg_to_dfg[second_resource];
  if (first_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[first_node].begin(),
                          incident_edge_ids_[first_node].end());
  }
  if (second_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[second_node].begin(),
                          incident_edge_ids_[second_node].end());
  }
  std::sort(affected_edges.begin(), affected_edges.end());
  affected_edges.erase(
      std::unique(affected_edges.begin(), affected_edges.end()),
      affected_edges.end());

  double before = 0.0;
  double after = 0.0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    before += EdgePlacementCost(edge, state.dfg_to_mrrg);
    after += ResourcePairPlacementCost(
        ResourceAfterSwap(edge.source, first_resource, second_resource,
                          state),
        ResourceAfterSwap(edge.target, first_resource, second_resource,
                          state));
  }
  return after - before;
}

int PlacementSearchEngine::PRISAWeakEdgeDeltaForSwap(const PlacementState& state,
                              int first_resource,
                              int second_resource) const {
  std::vector<int> affected_edges;
  const int first_node = state.mrrg_to_dfg[first_resource];
  const int second_node = state.mrrg_to_dfg[second_resource];
  if (first_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[first_node].begin(),
                          incident_edge_ids_[first_node].end());
  }
  if (second_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[second_node].begin(),
                          incident_edge_ids_[second_node].end());
  }
  std::sort(affected_edges.begin(), affected_edges.end());
  affected_edges.erase(
      std::unique(affected_edges.begin(), affected_edges.end()),
      affected_edges.end());

  const int resource_count = static_cast<int>(Placement2DResourceOrder().size());
  int before = 0;
  int after = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    before += PRISAEdgeIsWeak(state.dfg_to_mrrg[edge.source],
                              state.dfg_to_mrrg[edge.target],
                              resource_count)
                  ? 1
                  : 0;
    after += PRISAEdgeIsWeak(
                 ResourceAfterSwap(edge.source, first_resource,
                                   second_resource, state),
                 ResourceAfterSwap(edge.target, first_resource,
                                   second_resource, state),
                 resource_count)
                 ? 1
                 : 0;
  }
  return after - before;
}

int PlacementSearchEngine::EdgeMeshHop(const DFGEdgeInfo& edge,
                const std::vector<int>& dfg_to_mrrg) const {
  const int source_resource = dfg_to_mrrg[edge.source];
  const int target_resource = dfg_to_mrrg[edge.target];
  if (source_resource < 0 || target_resource < 0) return kInfDistance;
  return SpatialStepDistance(source_resource, target_resource);
}

PlacementSearchEngine::CostAwarePRISAMetrics PlacementSearchEngine::ComputeCostAwarePRISAMetrics(
    const PlacementState& state) const {
  CostAwarePRISAMetrics metrics;
  const int node_count = dfg_.GetNodeNum();
  std::vector<int> indegree(node_count, 0);
  std::vector<std::vector<std::pair<int, int>>> weighted_successors(
      node_count);

  for (const auto& edge : dfg_edges_) {
    const int hop = EdgeMeshHop(edge, state.dfg_to_mrrg);
    if (hop >= kInfDistance) {
      metrics.mesh_hop_sum = kImpossibleCost;
      metrics.max_mesh_hop = kInfDistance;
      metrics.mapped_lp_mesh_hop = kInfDistance;
      return metrics;
    }
    metrics.mesh_hop_sum += hop;
    metrics.max_mesh_hop = std::max(metrics.max_mesh_hop, hop);
    if (hop <= 1) metrics.direct_edge_count++;
    weighted_successors[edge.source].push_back(
        {edge.target, std::max(1, hop)});
    indegree[edge.target]++;
  }

  std::queue<int> q;
  std::vector<int> distance(node_count, 0);
  for (int node = 0; node < node_count; node++) {
    if (indegree[node] == 0) q.push(node);
  }

  int visited = 0;
  while (!q.empty()) {
    const int source = q.front();
    q.pop();
    visited++;
    metrics.mapped_lp_mesh_hop =
        std::max(metrics.mapped_lp_mesh_hop, distance[source]);
    for (const auto& [target, weight] : weighted_successors[source]) {
      distance[target] = std::max(distance[target], distance[source] + weight);
      indegree[target]--;
      if (indegree[target] == 0) q.push(target);
    }
  }

  if (visited != node_count) {
    metrics.mapped_lp_mesh_hop = static_cast<int>(metrics.mesh_hop_sum);
  } else {
    for (int value : distance) {
      metrics.mapped_lp_mesh_hop =
          std::max(metrics.mapped_lp_mesh_hop, value);
    }
  }
  return metrics;
}

double PlacementSearchEngine::CostAwarePRISAScore(const PlacementState& state) const {
  const int weak_edges = PRISAWeakEdgeCount(state);
  const CostAwarePRISAMetrics metrics = ComputeCostAwarePRISAMetrics(state);
  if (metrics.mesh_hop_sum >= kImpossibleCost ||
      metrics.max_mesh_hop >= kInfDistance ||
      metrics.mapped_lp_mesh_hop >= kInfDistance) {
    return kImpossibleCost;
  }
  // The strict PRISA WR test is intentionally coarse. This derived mapper
  // keeps WR as the first-stage objective, then optimizes the placement-only
  // paper metrics directly: mapped LP, max FIFO/hop, and total mesh hop.
  return weak_edges * 1000000.0 + metrics.mesh_hop_sum * 50.0 +
         metrics.mapped_lp_mesh_hop * 100.0 +
         metrics.max_mesh_hop * 200.0 -
         metrics.direct_edge_count * 80.0;
}

double PlacementSearchEngine::PRISAAcceptanceCost(const PlacementState& state) const {
  if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

double PlacementSearchEngine::PRISABestCost(const PlacementState& state) const {
  if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
  return PlacementCost(state);
}

int PlacementSearchEngine::CostAwarePRISACandidateSampleCount() const {
  if (options_.max_trials.has_value()) {
    return std::max(16, options_.max_trials.value());
  }
  return 512;
}

int PlacementSearchEngine::CostAwarePRISAFullEvaluationCount() const { return 8; }

double PlacementSearchEngine::CostAwarePRISALocalSwapScore(const PlacementState& state,
                                    int first_resource,
                                    int second_resource) const {
  std::vector<int> affected_edges;
  const int first_node = state.mrrg_to_dfg[first_resource];
  const int second_node = state.mrrg_to_dfg[second_resource];
  if (first_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[first_node].begin(),
                          incident_edge_ids_[first_node].end());
  }
  if (second_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[second_node].begin(),
                          incident_edge_ids_[second_node].end());
  }
  std::sort(affected_edges.begin(), affected_edges.end());
  affected_edges.erase(
      std::unique(affected_edges.begin(), affected_edges.end()),
      affected_edges.end());

  double mesh_delta = 0.0;
  int after_max_hop = 0;
  int direct_edge_gain = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    const int before = EdgeMeshHop(edge, state.dfg_to_mrrg);
    const int after = SpatialStepDistance(
        ResourceAfterSwap(edge.source, first_resource, second_resource,
                          state),
        ResourceAfterSwap(edge.target, first_resource, second_resource,
                          state));
    if (before >= kInfDistance || after >= kInfDistance) {
      return kImpossibleCost;
    }
    mesh_delta += after - before;
    after_max_hop = std::max(after_max_hop, after);
    if (before <= 1 && after > 1) direct_edge_gain--;
    if (before > 1 && after <= 1) direct_edge_gain++;
  }

  const int weak_delta =
      PRISAWeakEdgeDeltaForSwap(state, first_resource, second_resource);
  return weak_delta * 1000000.0 + mesh_delta * 100.0 +
         after_max_hop * 20.0 - direct_edge_gain * 80.0;
}

bool PlacementSearchEngine::ApplyCostAwarePRISAMove(PlacementState& state) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return false;

  struct Candidate {
    double score = std::numeric_limits<double>::infinity();
    int first_resource = -1;
    int second_resource = -1;
    int random_order = 0;
  };

  const int resource_count = static_cast<int>(resources.size());
  const int total_pairs = resource_count * (resource_count - 1) / 2;
  const int sample_count =
      std::min(total_pairs, CostAwarePRISACandidateSampleCount());

  auto normalize_pair = [](int lhs, int rhs) {
    if (lhs > rhs) std::swap(lhs, rhs);
    return std::pair<int, int>(lhs, rhs);
  };

  std::set<std::pair<int, int>> sampled_pairs;
  if (sample_count == total_pairs) {
    for (int i = 0; i < resource_count; i++) {
      for (int j = i + 1; j < resource_count; j++) {
        sampled_pairs.insert({i, j});
      }
    }
  } else {
    std::vector<std::pair<int, int>> edge_hops;
    edge_hops.reserve(dfg_edges_.size());
    for (const auto& edge : dfg_edges_) {
      edge_hops.push_back({EdgeMeshHop(edge, state.dfg_to_mrrg), edge.id});
    }
    std::sort(edge_hops.begin(), edge_hops.end(),
              std::greater<std::pair<int, int>>());
    const int focused_budget = std::max(1, sample_count / 2);
    const int focused_edge_count =
        std::min(4, static_cast<int>(edge_hops.size()));
    for (int idx = 0; idx < focused_edge_count; idx++) {
      const auto& edge = dfg_edges_[edge_hops[idx].second];
      const int endpoints[] = {state.dfg_to_mrrg[edge.source],
                               state.dfg_to_mrrg[edge.target]};
      for (int endpoint_resource : endpoints) {
        const int endpoint_index =
            ResourceOrderIndex()[endpoint_resource];
        if (endpoint_index < 0) continue;
        for (int other = 0;
             other < resource_count &&
             static_cast<int>(sampled_pairs.size()) < focused_budget;
             other++) {
          if (other == endpoint_index) continue;
          sampled_pairs.insert(normalize_pair(endpoint_index, other));
        }
      }
    }

    std::uniform_int_distribution<int> resource_dist(0, resource_count - 1);
    int attempts = 0;
    while (static_cast<int>(sampled_pairs.size()) < sample_count &&
           attempts < sample_count * 16) {
      attempts++;
      const int first = resource_dist(rng_);
      const int second = resource_dist(rng_);
      if (first == second) continue;
      sampled_pairs.insert(normalize_pair(first, second));
    }
  }

  std::vector<Candidate> candidates;
  candidates.reserve(sampled_pairs.size());
  int random_order = 0;
  for (const auto& [first_index, second_index] : sampled_pairs) {
    const int first_resource = resources[first_index];
    const int second_resource = resources[second_index];
    if (!CanSwapResources(state, first_resource, second_resource)) continue;
    candidates.push_back(Candidate{CostAwarePRISALocalSwapScore(
                                       state, first_resource,
                                       second_resource),
                                   first_resource, second_resource,
                                   random_order++});
  }
  if (candidates.empty()) return ApplyRandomResourceSwap(state, resources);

  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.score != b.score) return a.score < b.score;
              return a.random_order < b.random_order;
            });
  const int full_evaluation_count = std::min(
      CostAwarePRISAFullEvaluationCount(), static_cast<int>(candidates.size()));
  const double current_score = CostAwarePRISAScore(state);
  for (int i = 0; i < full_evaluation_count; i++) {
    PlacementState next = state;
    if (!ApplyResourceSwap(next, candidates[i].first_resource,
                           candidates[i].second_resource)) {
      candidates[i].score = kImpossibleCost;
      continue;
    }
    candidates[i].score = CostAwarePRISAScore(next);
  }
  candidates.resize(full_evaluation_count);
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) {
              if (a.score != b.score) return a.score < b.score;
              return a.random_order < b.random_order;
            });

  int selection_count = 1;
  if (candidates.front().score >= current_score) {
    selection_count = std::min(4, static_cast<int>(candidates.size()));
  } else {
    while (selection_count < static_cast<int>(candidates.size()) &&
           candidates[selection_count].score == candidates.front().score) {
      selection_count++;
    }
  }
  std::uniform_int_distribution<int> dist(0, selection_count - 1);
  const Candidate& chosen = candidates[dist(rng_)];
  return ApplyResourceSwap(state, chosen.first_resource,
                           chosen.second_resource);
}

int PlacementSearchEngine::CostAwarePRISAPolishPasses() const {
  return std::max(4, std::min(12, PRISAMaxIterations() / 125));
}

PlacementState PlacementSearchEngine::PolishCostAwarePRISA(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return state;

  struct Candidate {
    double local_score = std::numeric_limits<double>::infinity();
    int first_resource = -1;
    int second_resource = -1;
  };

  double current_score = CostAwarePRISAScore(state);
  for (int pass = 0;
       pass < CostAwarePRISAPolishPasses() && !HasTimedOut(start, 0.05);
       pass++) {
    std::vector<Candidate> candidates;
    const int resource_count = static_cast<int>(resources.size());
    candidates.reserve(resource_count * resource_count / 2);
    for (int i = 0; i < resource_count; i++) {
      for (int j = i + 1; j < resource_count; j++) {
        const int first_resource = resources[i];
        const int second_resource = resources[j];
        if (!CanSwapResources(state, first_resource, second_resource)) {
          continue;
        }
        candidates.push_back(Candidate{
            CostAwarePRISALocalSwapScore(state, first_resource,
                                         second_resource),
            first_resource, second_resource});
      }
    }
    if (candidates.empty()) break;

    const int reevaluate_count =
        std::min(64, static_cast<int>(candidates.size()));
    if (reevaluate_count < static_cast<int>(candidates.size())) {
      std::nth_element(
          candidates.begin(), candidates.begin() + reevaluate_count,
          candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.local_score < b.local_score;
          });
    }
    candidates.resize(reevaluate_count);

    bool improved = false;
    PlacementState best_state = state;
    double best_score = current_score;
    for (const auto& candidate : candidates) {
      PlacementState next = state;
      if (!ApplyResourceSwap(next, candidate.first_resource,
                             candidate.second_resource)) {
        continue;
      }
      const double next_score = CostAwarePRISAScore(next);
      if (next_score + 1.0e-9 < best_score) {
        best_score = next_score;
        best_state = std::move(next);
        improved = true;
      }
    }
    if (!improved) break;
    state = std::move(best_state);
    current_score = best_score;
  }
  return state;
}

int PlacementSearchEngine::DirectEdgeGainForSwap(const PlacementState& state, int first_resource,
                          int second_resource) const {
  std::vector<int> affected_edges;
  const int first_node = state.mrrg_to_dfg[first_resource];
  const int second_node = state.mrrg_to_dfg[second_resource];
  if (first_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[first_node].begin(),
                          incident_edge_ids_[first_node].end());
  }
  if (second_node >= 0) {
    affected_edges.insert(affected_edges.end(),
                          incident_edge_ids_[second_node].begin(),
                          incident_edge_ids_[second_node].end());
  }
  std::sort(affected_edges.begin(), affected_edges.end());
  affected_edges.erase(
      std::unique(affected_edges.begin(), affected_edges.end()),
      affected_edges.end());

  int gain = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    const int before = EdgeMeshHop(edge, state.dfg_to_mrrg);
    const int after = SpatialStepDistance(
        ResourceAfterSwap(edge.source, first_resource, second_resource,
                          state),
        ResourceAfterSwap(edge.target, first_resource, second_resource,
                          state));
    if (before <= 1 && after > 1) gain--;
    if (before > 1 && after <= 1) gain++;
  }
  return gain;
}

PlacementState PlacementSearchEngine::PolishCostAwarePRISADirectEdges(
    PlacementState state, std::chrono::steady_clock::time_point start) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return state;

  struct Candidate {
    int direct_gain = 0;
    double local_score = std::numeric_limits<double>::infinity();
    int first_resource = -1;
    int second_resource = -1;
  };

  CostAwarePRISAMetrics current = ComputeCostAwarePRISAMetrics(state);
  for (int pass = 0;
       pass < CostAwarePRISAPolishPasses() && !HasTimedOut(start, 0.05);
       pass++) {
    std::vector<Candidate> candidates;
    const int resource_count = static_cast<int>(resources.size());
    candidates.reserve(resource_count * resource_count / 2);
    for (int i = 0; i < resource_count; i++) {
      for (int j = i + 1; j < resource_count; j++) {
        const int first_resource = resources[i];
        const int second_resource = resources[j];
        if (!CanSwapResources(state, first_resource, second_resource)) {
          continue;
        }
        const int direct_gain =
            DirectEdgeGainForSwap(state, first_resource, second_resource);
        if (direct_gain <= 0) continue;
        candidates.push_back(Candidate{
            direct_gain,
            CostAwarePRISALocalSwapScore(state, first_resource,
                                         second_resource),
            first_resource, second_resource});
      }
    }
    if (candidates.empty()) break;

    const int reevaluate_count =
        std::min(256, static_cast<int>(candidates.size()));
    if (reevaluate_count < static_cast<int>(candidates.size())) {
      std::nth_element(
          candidates.begin(), candidates.begin() + reevaluate_count,
          candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.direct_gain != b.direct_gain) {
              return a.direct_gain > b.direct_gain;
            }
            return a.local_score < b.local_score;
          });
    }
    candidates.resize(reevaluate_count);

    bool improved = false;
    PlacementState best_state = state;
    CostAwarePRISAMetrics best_metrics = current;
    for (const auto& candidate : candidates) {
      PlacementState next = state;
      if (!ApplyResourceSwap(next, candidate.first_resource,
                             candidate.second_resource)) {
        continue;
      }
      const CostAwarePRISAMetrics next_metrics =
          ComputeCostAwarePRISAMetrics(next);
      if (next_metrics.direct_edge_count <= best_metrics.direct_edge_count) {
        continue;
      }
      if (next_metrics.max_mesh_hop > current.max_mesh_hop) continue;
      if (next_metrics.mapped_lp_mesh_hop > current.mapped_lp_mesh_hop) {
        continue;
      }
      if (next_metrics.mesh_hop_sum > current.mesh_hop_sum + 2.0 + 1.0e-9) {
        continue;
      }
      if (next_metrics.direct_edge_count > best_metrics.direct_edge_count ||
          (next_metrics.direct_edge_count == best_metrics.direct_edge_count &&
           next_metrics.mesh_hop_sum < best_metrics.mesh_hop_sum)) {
        best_state = std::move(next);
        best_metrics = next_metrics;
        improved = true;
      }
    }
    if (!improved) break;
    state = std::move(best_state);
    current = best_metrics;
  }
  return state;
}

bool PlacementSearchEngine::ApplyPRISAMove(PlacementState& state) {
  const auto& resources = Placement2DResourceOrder();
  if (resources.size() < 2) return false;
  if (UsesCostAwarePRISA()) {
    return ApplyCostAwarePRISAMove(state);
  }
  const auto& resource_order = ResourceOrderIndex();
  const int resource_count = static_cast<int>(resources.size());
  std::vector<int> row_weak(resource_count, 0);
  std::vector<int> column_weak(resource_count, 0);
  std::vector<int> row_potential(resource_count, 0);
  std::vector<int> column_potential(resource_count, 0);
  std::vector<std::vector<int>> weak_columns_by_row(resource_count);
  std::vector<std::vector<int>> weak_rows_by_column(resource_count);

  for (const auto& edge : dfg_edges_) {
    const int source_resource = state.dfg_to_mrrg[edge.source];
    const int target_resource = state.dfg_to_mrrg[edge.target];
    if (source_resource < 0 || target_resource < 0) continue;
    const int row = resource_order[source_resource];
    const int column = resource_order[target_resource];
    if (row < 0 || column < 0) continue;
    if (IsPRISAWeakRegion(row, column, resource_count)) {
      row_weak[row]++;
      column_weak[column]++;
      weak_columns_by_row[row].push_back(column);
      weak_rows_by_column[column].push_back(row);
    } else if (IsPRISAPotentialRegion(row, column, resource_count)) {
      row_potential[row]++;
      column_potential[column]++;
    }
  }

  auto random_argmax = [&](const std::vector<int>& values) {
    const int best = *std::max_element(values.begin(), values.end());
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(values.size()); i++) {
      if (values[i] == best) indices.push_back(i);
    }
    std::uniform_int_distribution<int> dist(
        0, static_cast<int>(indices.size()) - 1);
    return indices[dist(rng_)];
  };
  const int row_index = random_argmax(row_weak);
  const int column_index = random_argmax(column_weak);
  const int row_max = row_weak[row_index];
  const int column_max = column_weak[column_index];
  if (row_max == 0) {
    return ApplyPlacementCostSampledSwap(state, resources);
  }

  struct PRISACandidate {
    int first_index = -1;
    int second_index = -1;
    int weak_count = 0;
    int weak_delta = 0;
    int fixed_weak_count = 0;
    int potential_count = 0;
    double cost_delta = 0.0;
    int spatial_distance = 0;
    int random_order = 0;
  };

  auto best_candidate_for = [&](int first_index, int weak_count,
                                bool row_side)
      -> std::optional<PRISACandidate> {
    const auto& weak_counterparts = row_side
                                        ? weak_columns_by_row[first_index]
                                        : weak_rows_by_column[first_index];
    if (weak_counterparts.empty()) return std::nullopt;
    std::vector<int> candidate_indices(resource_count);
    std::iota(candidate_indices.begin(), candidate_indices.end(), 0);
    std::shuffle(candidate_indices.begin(), candidate_indices.end(), rng_);

    const int first_resource = resources[first_index];
    std::vector<PRISACandidate> candidates;
    candidates.reserve(candidate_indices.size());
    for (int order = 0; order < static_cast<int>(candidate_indices.size());
         order++) {
      const int second_index = candidate_indices[order];
      if (second_index == first_index) continue;
      const int second_resource = resources[second_index];
      if (!CanSwapResources(state, first_resource, second_resource)) continue;
      int fixed_weak_count = 0;
      for (int counterpart : weak_counterparts) {
        const bool moves_to_pr =
            row_side
                ? IsPRISAPotentialRegion(second_index, counterpart,
                                         resource_count)
                : IsPRISAPotentialRegion(counterpart, second_index,
                                         resource_count);
        if (moves_to_pr) fixed_weak_count++;
      }
      if (fixed_weak_count == 0) continue;
      candidates.push_back(PRISACandidate{
          first_index,
          second_index,
          weak_count,
          PRISAWeakEdgeDeltaForSwap(state, first_resource, second_resource),
          fixed_weak_count,
          row_side ? row_potential[second_index]
                   : column_potential[second_index],
          PlacementCostDeltaForSwap(state, first_resource, second_resource),
          SpatialStepDistance(first_resource, second_resource),
          order});
    }
    if (candidates.empty()) return std::nullopt;
    std::sort(candidates.begin(), candidates.end(),
              [](const PRISACandidate& a, const PRISACandidate& b) {
                // PRISA's second exchange part is selected from the
                // potential region with the fewest existing non-zero
                // elements. This keeps already compact rows/columns stable
                // while moving the selected weak entries toward PR.
                if (a.potential_count != b.potential_count) {
                  return a.potential_count < b.potential_count;
                }
                if (a.fixed_weak_count != b.fixed_weak_count) {
                  return a.fixed_weak_count > b.fixed_weak_count;
                }
                if (a.weak_delta != b.weak_delta) {
                  return a.weak_delta < b.weak_delta;
                }
                if (a.cost_delta != b.cost_delta) {
                  return a.cost_delta < b.cost_delta;
                }
                if (a.spatial_distance != b.spatial_distance) {
                  return a.spatial_distance < b.spatial_distance;
                }
                return a.random_order < b.random_order;
              });
    return candidates.front();
  };

  std::vector<PRISACandidate> candidates;
  const bool prefer_row =
      row_max > column_max ||
      (row_max == column_max &&
       std::uniform_int_distribution<int>(0, 1)(rng_) == 0);
  auto push_row_candidate = [&]() {
    if (row_max <= 0) return;
    auto candidate = best_candidate_for(row_index, row_max, true);
    if (candidate.has_value()) candidates.push_back(*candidate);
  };
  auto push_column_candidate = [&]() {
    if (column_max <= 0) return;
    auto candidate = best_candidate_for(column_index, column_max, false);
    if (candidate.has_value()) candidates.push_back(*candidate);
  };
  if (prefer_row) {
    push_row_candidate();
    if (candidates.empty()) push_column_candidate();
  } else {
    push_column_candidate();
    if (candidates.empty()) push_row_candidate();
  }
  if (candidates.empty()) {
    return ApplyPlacementCostSampledSwap(state, resources);
  }
  const auto& candidate = candidates.front();
  if (ApplyResourceSwap(state, resources[candidate.first_index],
                        resources[candidate.second_index])) {
    return true;
  }
  return ApplyPlacementCostSampledSwap(state, resources);
}

std::optional<PlacementState> PlacementSearchEngine::RunPRISA(
    std::chrono::steady_clock::time_point start) {
  std::optional<PlacementState> current = ConstructPRISAInitialPlacement();
  if (!current.has_value()) current = RandomLegalPlacement();
  if (!current.has_value()) return std::nullopt;

  PlacementState best = *current;
  double current_cost = PRISAAcceptanceCost(*current);
  double best_cost = PRISABestCost(*current);
  const double initial_cost = PlacementCost(*current);
  const double initial_acceptance_cost = current_cost;
  const double initial_best_cost = best_cost;
  const int initial_weak_edges = PRISAWeakEdgeCount(*current);
  std::vector<PlacementState> elite;
  elite.push_back(best);

  const int max_iterations = PRISAMaxIterations();
  const double start_probability = UsesPRISASIS() ? 0.20 : 0.40;
  const double end_probability = 0.01;
  double temperature = -1.0 / std::log(start_probability);
  const double final_temperature = -1.0 / std::log(end_probability);
  const double cooling =
      std::pow(final_temperature / temperature,
               1.0 / std::max(1, max_iterations));
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  int generated_moves = 0;
  int accepted_moves = 0;
  int improving_moves = 0;
  int completed_iterations = 0;

  for (int iteration = 0;
       iteration < max_iterations && !HasTimedOut(start, 0.05);
       iteration++) {
    completed_iterations = iteration + 1;
    PlacementState next = *current;
    if (!ApplyPRISAMove(next)) continue;
    generated_moves++;
    const double next_cost = PRISAAcceptanceCost(next);
    const double delta = next_cost - current_cost;
    const bool accept =
        delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
    if (accept) {
      *current = next;
      current_cost = next_cost;
      accepted_moves++;
    }
    const double next_best_cost = PRISABestCost(next);
    if (next_best_cost < best_cost) {
      best = next;
      best_cost = next_best_cost;
      improving_moves++;
      elite.push_back(best);
      if (elite.size() > 16) elite.erase(elite.begin());
    }
    temperature = std::max(final_temperature, temperature * cooling);
  }

  if (UsesCostAwarePRISA()) {
    const double before_polish_cost = best_cost;
    best = PolishCostAwarePRISA(best, start);
    best = PolishCostAwarePRISADirectEdges(best, start);
    best_cost = PRISABestCost(best);
    if (best_cost + 1.0e-9 < before_polish_cost) {
      improving_moves++;
    }
  }

  Log("prisa_result sis=" + std::to_string(UsesPRISASIS() ? 1 : 0) +
      " iterations=" + std::to_string(completed_iterations) +
      " generated_moves=" + std::to_string(generated_moves) +
      " accepted_moves=" + std::to_string(accepted_moves) +
      " improving_moves=" + std::to_string(improving_moves) +
      " initial_cost=" + std::to_string(initial_cost) +
      " best_cost=" + std::to_string(PlacementCost(best)) +
      " initial_acceptance_cost=" +
      std::to_string(initial_acceptance_cost) +
      " initial_best_cost=" + std::to_string(initial_best_cost) +
      " best_acceptance_cost=" + std::to_string(PRISAAcceptanceCost(best)) +
      " best_selection_cost=" + std::to_string(best_cost) +
      " initial_weak_edges=" + std::to_string(initial_weak_edges) +
      " best_weak_edges=" + std::to_string(PRISAWeakEdgeCount(best)));

  std::sort(elite.begin(), elite.end(), [&](const PlacementState& a,
                                            const PlacementState& b) {
    return PRISAAcceptanceCost(a) < PRISAAcceptanceCost(b);
  });
  if (options_.placement_only) return best;
  for (const auto& candidate : elite) {
    auto finalized = FinalizePhysicalPlacementIfNeeded(candidate);
    if (!finalized.has_value()) continue;
    if (TryRoutePlacement(*finalized).has_value()) return *finalized;
    if (HasTimedOut(start, 0.01)) break;
  }
  return FinalizePhysicalPlacementIfNeeded(best);
}

}  // namespace mapper::detail::placement_search
