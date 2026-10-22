#include <mapper/prisa/engine/prisa_engine.hpp>

namespace mapper::prisa {

std::vector<DFGEdgeInfo> BuildDFGEdges(entity::DFG& dfg) {
  std::vector<DFGEdgeInfo> edges;
  int edge_id = 0;
  for (int source = 0; source < dfg.GetNodeNum(); source++) {
    for (int target : dfg.GetAdjacentNodeIdVec(source)) {
      edges.push_back({edge_id++, source, target});
    }
  }
  return edges;
}

std::vector<std::vector<int>> BuildSuccessors(entity::DFG& dfg) {
  std::vector<std::vector<int>> successors(dfg.GetNodeNum());
  for (int i = 0; i < dfg.GetNodeNum(); i++) {
    successors[i] = dfg.GetAdjacentNodeIdVec(i);
  }
  return successors;
}

std::vector<std::vector<int>> BuildPredecessors(entity::DFG& dfg) {
  std::vector<std::vector<int>> predecessors(dfg.GetNodeNum());
  for (int i = 0; i < dfg.GetNodeNum(); i++) {
    for (int successor : dfg.GetAdjacentNodeIdVec(i)) {
      if (successor == i) continue;
      predecessors[successor].push_back(i);
    }
  }
  return predecessors;
}

std::vector<std::vector<int>> BuildIncidentEdgeIds(
    int node_num, const std::vector<DFGEdgeInfo>& edges) {
  std::vector<std::vector<int>> incident_edge_ids(node_num);
  for (const auto& edge : edges) {
    if (edge.source >= 0 && edge.source < node_num) {
      incident_edge_ids[edge.source].push_back(edge.id);
    }
    if (edge.target >= 0 && edge.target < node_num &&
        edge.target != edge.source) {
      incident_edge_ids[edge.target].push_back(edge.id);
    }
  }
  return incident_edge_ids;
}

PrisaEngine::PrisaEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                         PlacementSearchKind search_kind, double timeout_s,
                         const std::optional<std::string>& log_file_path,
                         const PlacementSearchOptions& options)
    : dfg_(dfg),
      mrrg_(mrrg),
      search_kind_(search_kind),
      timeout_s_(timeout_s > 0.0 ? timeout_s : 1.0),
      log_file_path_(log_file_path),
      options_(options),
      base_seed_(static_cast<unsigned int>(
          0x9E3779B9u ^ static_cast<unsigned int>(dfg.GetNodeNum() * 131 +
                                                  mrrg.GetNodeNum()))),
      rng_(base_seed_) {
  ResetSeed(0);
  dfg_edges_ = BuildDFGEdges(dfg_);
  incident_edge_ids_ = BuildIncidentEdgeIds(dfg_.GetNodeNum(), dfg_edges_);
  successors_ = BuildSuccessors(dfg_);
  predecessors_ = BuildPredecessors(dfg_);
  BuildMRRGCache();
  if (NeedsAllPairsDistances()) {
    BuildAllPairsDistances();
  }
  if (IsPRISALike()) {
    BuildPRISADistanceRegions();
  }
  annotation_ = BuildAnnotation();
}

auto PrisaEngine::StartSearch() -> std::chrono::steady_clock::time_point {
  if (grid_placement_) return GridStartSearch();

  const auto start = std::chrono::steady_clock::now();
  Log("start mapper=" + MapperName() +
      " dfg_nodes=" + std::to_string(dfg_.GetNodeNum()) +
      " mrrg_nodes=" + std::to_string(mrrg_.GetNodeNum()) +
      " max_trials=" + std::to_string(MaxTrials()) +
      " seed_count=" + std::to_string(SeedCount()) + " routing_retry_count=" +
      std::to_string(RoutingRetryCount()) + " max_iterations=" +
      std::to_string(IsPRISALike() ? PRISAMaxIterations() : MaxIterations()));

  return start;
}

std::optional<PlacementState> PrisaEngine::SearchPlacement(
    std::chrono::steady_clock::time_point start) {
  if (grid_placement_) return GridSearchPlacement(start);

  return RunPRISAMultiSeed(start);
}

mapper::MappingResult PrisaEngine::Failure(
    std::chrono::steady_clock::time_point start) const {
  if (grid_placement_) return GridFailure(start);

  return MakeFailureResult(start);
}

mapper::MappingResult PrisaEngine::FinishPlacementOnly(
    const PlacementState& placement,
    std::chrono::steady_clock::time_point start) {
  if (grid_placement_) return GridFinishPlacement(placement, start);

  const auto mapping = MakePlacementOnlyMapping(placement);
  const double mapping_time_s = SecondsSince(start);
  Log("placement_only_success mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s) +
      " placement_cost=" + std::to_string(PlacementCost(placement)));
  return mapper::MappingResult(true, mapping, mapping_time_s);
}

std::optional<RouteUsage> PrisaEngine::RoutePlacement(
    const PlacementState& placement) {
  return TryRoutePlacement(placement);
}

mapper::MappingResult PrisaEngine::FinishRoutedMapping(
    const PlacementState& placement, RouteUsage& route_usage,
    std::chrono::steady_clock::time_point start) {
  for (auto& edges : route_usage.source_to_route_edges) {
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  }

  const auto mapping = entity::GenerateMappingFromRoutingResult(
      mrrg_, dfg_, placement.dfg_to_mrrg, route_usage.source_to_route_edges);
  const double mapping_time_s = SecondsSince(start);
  Log("success mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s) +
      " placement_cost=" + std::to_string(PlacementCost(placement)) +
      " route_edges=" + std::to_string(RouteEdgeCount(route_usage)));
  return mapper::MappingResult(true, mapping, mapping_time_s);
}

std::string PrisaEngine::MapperName() const {
  if (grid_placement_) return GridMapperName();
  switch (search_kind_) {
    case PlacementSearchKind::kModuloPhysicalPRISA:
      return "ModuloPhysicalPRISA";
    case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
      return "ModuloPhysicalPRISAManhattan";

    case PlacementSearchKind::kPlacement2DPRISA:
      return "Placement2DPRISA";
    case PlacementSearchKind::kPlacement2DPRISANoSIS:
      return "Placement2DPRISANoSIS";
    case PlacementSearchKind::kPlacement2DCostAwarePRISA:
      return "Placement2DCostAwarePRISA";
  }
  return "unknown";
}

bool PrisaEngine::IsPRISALike() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISANoSIS ||
         search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

bool PrisaEngine::UsesPRISASIS() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
         search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

bool PrisaEngine::UsesManhattanRouting() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan;
}

bool PrisaEngine::IsPhysicalThenContextLike() const {
  return false || false ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan;
}

bool PrisaEngine::UsesPhysicalPlacementStage() const {
  return false || IsPhysicalThenContextLike();
}

bool PrisaEngine::UsesPhysicalPeExclusivePlacement() const {
  return UsesPhysicalPlacementStage() || false || IsPRISALike();
}

bool PrisaEngine::UsesSeparatePlacement2DIOCells() const {
  if (!UsesPhysicalPlacementStage()) return false;
  bool has_memory_cell = false;
  bool has_compute_only_cell = false;
  for (int r : placement2d_resources_) {
    const auto property = mrrg_.GetNodeProperty(r);
    has_memory_cell = has_memory_cell || property.is_memory_accessible;
    has_compute_only_cell =
        has_compute_only_cell || !property.is_memory_accessible;
  }
  return has_memory_cell && has_compute_only_cell;
}

bool PrisaEngine::UsesApproximatePlacementDistance() const {
  return false || false || IsPhysicalThenContextLike() || false ||
         IsPRISALike();
}

mapper::MappingResult PrisaEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping PrisaEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

Annotation PrisaEngine::BuildAnnotation() {
  Annotation annotation;
  const int n = dfg_.GetNodeNum();
  annotation.in_degree.assign(n, 0);
  annotation.out_degree.assign(n, 0);
  annotation.degree.assign(n, 0);

  for (int v = 0; v < n; v++) {
    annotation.in_degree[v] = static_cast<int>(predecessors_[v].size());
    annotation.out_degree[v] = static_cast<int>(successors_[v].size());
    annotation.degree[v] = annotation.in_degree[v] + annotation.out_degree[v];
  }
  return annotation;
}

bool PrisaEngine::CanOccupyResource(int dfg_node_id, int mrrg_node_id,
                                    const PlacementState& state) const {
  if (state.mrrg_to_dfg[mrrg_node_id] != -1) return false;
  if (UsesPhysicalPlacementStage() &&
      mrrg_.GetNodeProperty(mrrg_node_id).context_id != 0) {
    return false;
  }
  if (!IsCompatible(dfg_node_id, mrrg_node_id)) return false;
  if (UsesSeparatePlacement2DIOCells()) {
    const bool is_io_node = IsIONode(dfg_node_id);
    const bool is_memory_cell =
        mrrg_.GetNodeProperty(mrrg_node_id).is_memory_accessible;
    if (is_io_node && !is_memory_cell) return false;
  }
  // Physical-placement variants first choose one physical PE per DFG node.
  // ModuloPhysical* mappers assign contexts only after this 2D placement.
  if (UsesPhysicalPeExclusivePlacement() &&
      IsPhysicalPositionOccupied(mrrg_node_id, state)) {
    return false;
  }
  return true;
}

bool PrisaEngine::UsesCostAwarePRISA() const {
  return search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

int PrisaEngine::PRISAMaxIterations() const {
  if (options_.max_iterations.has_value()) {
    return std::max(1, options_.max_iterations.value());
  }
  return std::max(1, dfg_.GetNodeNum() * 10);
}

int PrisaEngine::PRISAResourceSideLength() const {
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

int PrisaEngine::PRISAWeakPairCount(int resource_count) const {
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

void PrisaEngine::BuildPRISADistanceRegions() {
  const auto& resources = Placement2DResourceOrder();
  const int resource_count = static_cast<int>(resources.size());
  prisa_distance_matrix_.assign(resource_count * resource_count, 0);
  prisa_weak_region_matrix_.assign(resource_count * resource_count, 0);
  prisa_weak_distance_threshold_ = (1 << 28);
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
  std::sort(pair_distances.begin(), pair_distances.end(), std::greater<int>());
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

bool PrisaEngine::IsPRISAWeakRegion(int row, int column,
                                    int resource_count) const {
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

bool PrisaEngine::IsPRISAPotentialRegion(int row, int column,
                                         int resource_count) const {
  if (row == column) return false;
  return !IsPRISAWeakRegion(row, column, resource_count);
}

std::vector<int> PrisaEngine::LowBandwidthNodeOrder() {
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

int PrisaEngine::PRISAInitialSolutionSampleCount() const {
  if (!UsesPRISASIS()) return 1;
  return 1;
}

bool PrisaEngine::CanSwapResources(const PlacementState& state, int ra,
                                   int rb) const {
  if (ra == rb || ra < 0 || rb < 0) return false;
  const int a = state.mrrg_to_dfg[ra];
  const int b = state.mrrg_to_dfg[rb];
  if (a < 0 && b < 0) return false;
  if (a >= 0 && !IsCompatible(a, rb)) return false;
  if (b >= 0 && !IsCompatible(b, ra)) return false;
  return true;
}

bool PrisaEngine::ApplyResourceSwap(PlacementState& state, int ra,
                                    int rb) const {
  if (!CanSwapResources(state, ra, rb)) return false;
  const int a = state.mrrg_to_dfg[ra];
  const int b = state.mrrg_to_dfg[rb];
  state.mrrg_to_dfg[ra] = b;
  state.mrrg_to_dfg[rb] = a;
  if (a >= 0) state.dfg_to_mrrg[a] = rb;
  if (b >= 0) state.dfg_to_mrrg[b] = ra;
  return true;
}

bool PrisaEngine::ApplyRandomResourceSwap(PlacementState& state,
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

bool PrisaEngine::ApplyPlacementCostSampledSwap(
    PlacementState& state, const std::vector<int>& resources) {
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

int PrisaEngine::ResourceAfterSwap(int dfg_node_id, int first_resource,
                                   int second_resource,
                                   const PlacementState& state) const {
  int resource = state.dfg_to_mrrg[dfg_node_id];
  if (resource == first_resource) return second_resource;
  if (resource == second_resource) return first_resource;
  return resource;
}

double PrisaEngine::PlacementCostDeltaForSwap(const PlacementState& state,
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
        ResourceAfterSwap(edge.source, first_resource, second_resource, state),
        ResourceAfterSwap(edge.target, first_resource, second_resource, state));
  }
  return after - before;
}

int PrisaEngine::PRISAWeakEdgeDeltaForSwap(const PlacementState& state,
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

  const int resource_count =
      static_cast<int>(Placement2DResourceOrder().size());
  int before = 0;
  int after = 0;
  for (int edge_id : affected_edges) {
    const auto& edge = dfg_edges_[edge_id];
    before += PRISAEdgeIsWeak(state.dfg_to_mrrg[edge.source],
                              state.dfg_to_mrrg[edge.target], resource_count)
                  ? 1
                  : 0;
    after += PRISAEdgeIsWeak(ResourceAfterSwap(edge.source, first_resource,
                                               second_resource, state),
                             ResourceAfterSwap(edge.target, first_resource,
                                               second_resource, state),
                             resource_count)
                 ? 1
                 : 0;
  }
  return after - before;
}

double PrisaEngine::CostAwarePRISAScore(const PlacementState& state) const {
  const int weak_edges = PRISAWeakEdgeCount(state);
  const auto metrics = ComputeCostAwarePRISAMetrics(state);
  if (metrics.mesh_hop_sum >= 1.0e8 || metrics.max_mesh_hop >= (1 << 28) ||
      metrics.mapped_lp_mesh_hop >= (1 << 28)) {
    return 1.0e8;
  }
  // The strict PRISA WR test is intentionally coarse. This derived mapper
  // keeps WR as the first-stage objective, then optimizes the placement-only
  // paper metrics directly: mapped LP, max FIFO/hop, and total mesh hop.
  return weak_edges * 1000000.0 + metrics.mesh_hop_sum * 50.0 +
         metrics.mapped_lp_mesh_hop * 100.0 + metrics.max_mesh_hop * 200.0 -
         metrics.direct_edge_count * 80.0;
}

std::vector<int> PrisaEngine::BuildASAPContextLevels() const {
  const int n = dfg_.GetNodeNum();
  std::vector<int> indegree(n, 0);
  std::vector<int> level(n, 0);
  for (const auto& edge : dfg_edges_) {
    if (edge.source == edge.target) continue;
    indegree[edge.target]++;
  }

  std::queue<int> ready;
  for (int node_id = 0; node_id < n; node_id++) {
    if (indegree[node_id] == 0) ready.push(node_id);
  }

  std::vector<char> visited(n, false);
  while (!ready.empty()) {
    const int node_id = ready.front();
    ready.pop();
    visited[node_id] = true;
    for (int successor : successors_[node_id]) {
      if (successor == node_id) continue;
      level[successor] = std::max(level[successor], level[node_id] + 1);
      indegree[successor]--;
      if (indegree[successor] == 0) ready.push(successor);
    }
  }

  // Most DFGs are DAGs, with recurrent behavior represented by self edges.
  // If a benchmark contains a non-self cycle, keep the assignment
  // deterministic rather than introducing another heuristic fallback.
  for (int node_id = 0; node_id < n; node_id++) {
    if (!visited[node_id]) level[node_id] = node_id;
  }
  return level;
}

bool PrisaEngine::NeedsAllPairsDistances() const {
  return !UsesApproximatePlacementDistance();
}

int PrisaEngine::SeedCount() const {
  if (!options_.seed_count.has_value()) return 1;
  return std::max(1, options_.seed_count.value());
}

int PrisaEngine::MaxTrials() const {
  const int default_trials = std::max(100, dfg_.GetNodeNum() * 4);
  if (!options_.max_trials.has_value()) return default_trials;
  return std::max(1, options_.max_trials.value());
}

int PrisaEngine::RoutingRetryCount() const {
  if (!options_.routing_retry_count.has_value()) return 4;
  return std::max(1, options_.routing_retry_count.value());
}

int PrisaEngine::MaxIterations() const {
  const int default_iterations =
      std::max(5000, dfg_.GetNodeNum() * mrrg_.GetNodeNum() * 30);
  if (!options_.max_iterations.has_value()) return default_iterations;
  return std::max(1, options_.max_iterations.value());
}

unsigned int PrisaEngine::SeedFor(int seed_index) const {
  const unsigned int configured_seed =
      options_.random_seed.has_value()
          ? static_cast<unsigned int>(options_.random_seed.value())
          : base_seed_;
  return configured_seed + static_cast<unsigned int>(0x9E3779B9u * seed_index);
}

void PrisaEngine::ResetSeed(int seed_index) {
  const auto seed = SeedFor(seed_index);
  rng_.seed(seed);
  if (grid_placement_) std::srand(seed);
}

void PrisaEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

double PrisaEngine::ResourcePairPlacementCost(int from_mrrg_node,
                                              int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return 1.0e8;
  const int distance = DirectedResourceDistance(from_mrrg_node, to_mrrg_node);
  if (distance >= (1 << 28)) return 1.0e8;
  return DirectedDistanceCost(distance);
}

double PrisaEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : dfg_edges_) {
    const double edge_cost = EdgePlacementCost(edge, state.dfg_to_mrrg);
    if (edge_cost >= 1.0e8) return 1.0e8;
    cost += edge_cost;
  }
  return cost;
}

bool PrisaEngine::PRISAEdgeIsWeak(int source_resource, int target_resource,
                                  int resource_count) const {
  if (source_resource < 0 || target_resource < 0) return false;
  const auto& resource_order = ResourceOrderIndex();
  if (source_resource >= static_cast<int>(resource_order.size()) ||
      target_resource >= static_cast<int>(resource_order.size())) {
    return false;
  }
  const int row = resource_order[source_resource];
  const int column = resource_order[target_resource];
  if (row < 0 || column < 0) return false;
  return IsPRISAWeakRegion(row, column, resource_count);
}

int PrisaEngine::PRISAWeakEdgeCount(const PlacementState& state) const {
  const int resource_count =
      static_cast<int>(Placement2DResourceOrder().size());
  int weak_edges = 0;
  for (const auto& edge : dfg_edges_) {
    if (PRISAEdgeIsWeak(state.dfg_to_mrrg[edge.source],
                        state.dfg_to_mrrg[edge.target], resource_count)) {
      weak_edges++;
    }
  }
  return weak_edges;
}

void PrisaEngine::BuildMRRGCache() {
  const int node_num = mrrg_.GetNodeNum();
  placement2d_resources_.clear();
  placement2d_resources_.reserve(node_num);
  std::set<std::pair<int, int>> seen_positions;
  for (int r = 0; r < node_num; r++) {
    const auto property = mrrg_.GetNodeProperty(r);
    if (property.context_id != 0) continue;
    if (!SupportsAnyDFGOperation(r)) continue;
    if (!seen_positions.insert(property.position_id).second) continue;
    placement2d_resources_.push_back(r);
  }
  std::sort(placement2d_resources_.begin(), placement2d_resources_.end(),
            [&](int a, int b) {
              const auto pa = mrrg_.GetNodeProperty(a).position_id;
              const auto pb = mrrg_.GetNodeProperty(b).position_id;
              if (pa.first != pb.first) return pa.first < pb.first;
              // Paper-style 2D placement labels resources in row-major
              // order before applying PRISA's distance-matrix/SIS logic.
              return pa.second < pb.second;
            });
  placement2d_resource_order_index_.assign(node_num, -1);
  for (int i = 0; i < static_cast<int>(placement2d_resources_.size()); i++) {
    placement2d_resource_order_index_[placement2d_resources_[i]] = i;
  }

  prisa_resources_ = placement2d_resources_;
  int min_row = std::numeric_limits<int>::max();
  for (int r : prisa_resources_) {
    min_row = std::min(min_row, mrrg_.GetNodeProperty(r).position_id.first);
  }
  std::sort(
      prisa_resources_.begin(), prisa_resources_.end(), [&](int a, int b) {
        const auto pa = mrrg_.GetNodeProperty(a).position_id;
        const auto pb = mrrg_.GetNodeProperty(b).position_id;
        if (pa.first != pb.first) return pa.first < pb.first;
        // PRISA's low-bandwidth labeling assumes nearby labels are
        // nearby resources. A serpentine path avoids row-major jumps
        // from the end of one row to the start of the next row.
        const bool reverse_row = ((pa.first - min_row) % 2) != 0;
        return reverse_row ? pa.second > pb.second : pa.second < pb.second;
      });
  prisa_resource_order_index_.assign(node_num, -1);
  for (int i = 0; i < static_cast<int>(prisa_resources_.size()); i++) {
    prisa_resource_order_index_[prisa_resources_[i]] = i;
  }
}

void PrisaEngine::BuildAllPairsDistances() {
  const int node_num = mrrg_.GetNodeNum();
  distance_.assign(node_num, std::vector<int>(node_num, (1 << 28)));
  for (int source = 0; source < node_num; source++) {
    std::queue<int> q;
    distance_[source][source] = 0;
    q.push(source);
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      for (int v : mrrg_.GetAdjacentNodeIdVec(u)) {
        if (distance_[source][v] != (1 << 28)) continue;
        distance_[source][v] = distance_[source][u] + 1;
        q.push(v);
      }
    }
  }
}

bool PrisaEngine::HasTimedOut(std::chrono::steady_clock::time_point start,
                              double reserve_s) const {
  if (reserve_s < 0.0) reserve_s = grid_placement_ ? 0.01 : 0.02;

  return SecondsSince(start) + reserve_s >= timeout_s_;
}

bool PrisaEngine::IsCompatible(int dfg_node_id, int mrrg_node_id) const {
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
  return SupportsOperation(mrrg_.GetNodeProperty(mrrg_node_id), op);
}

bool PrisaEngine::SamePhysicalPosition(int lhs_mrrg_node,
                                       int rhs_mrrg_node) const {
  return mrrg_.GetNodeProperty(lhs_mrrg_node).position_id ==
         mrrg_.GetNodeProperty(rhs_mrrg_node).position_id;
}

bool PrisaEngine::IsPhysicalPositionOccupied(
    int mrrg_node_id, const PlacementState& state) const {
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (state.mrrg_to_dfg[r] == -1) continue;
    if (SamePhysicalPosition(mrrg_node_id, r)) return true;
  }
  return false;
}

std::vector<int> PrisaEngine::CompatibleResources(
    int dfg_node_id, const PlacementState& state) const {
  std::vector<int> result;
  result.reserve(mrrg_.GetNodeNum());
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (!CanOccupyResource(dfg_node_id, r, state)) continue;
    result.push_back(r);
  }
  return result;
}

bool PrisaEngine::IsIONode(int dfg_node_id) const {
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
  return entity::IsMemoryAccessOperation(op);
}

bool PrisaEngine::SupportsAnyDFGOperation(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  for (entity::OpType op : property.supported_operations) {
    if (entity::IsDFGOp(op) && op != entity::OpType::kRoute &&
        op != entity::OpType::kNop) {
      return true;
    }
  }
  return false;
}

int PrisaEngine::ResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  return std::min(DirectedResourceDistance(from_mrrg_node, to_mrrg_node),
                  DirectedResourceDistance(to_mrrg_node, from_mrrg_node));
}

int PrisaEngine::DirectedResourceDistance(int from_mrrg_node,
                                          int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  if (!distance_.empty()) return distance_[from_mrrg_node][to_mrrg_node];
  return ApproximateDirectedResourceDistance(from_mrrg_node, to_mrrg_node);
}

int PrisaEngine::SpatialStepDistance(int from_mrrg_node,
                                     int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  const auto from = mrrg_.GetNodeProperty(from_mrrg_node).position_id;
  const auto to = mrrg_.GetNodeProperty(to_mrrg_node).position_id;
  const int row_distance = std::abs(from.first - to.first);
  const int column_distance = std::abs(from.second - to.second);
  const auto config = mrrg_.GetMRRGConfig();
  if (config.network_type == entity::MRRGNetworkType::kOneHop2d) {
    return (row_distance + 1) / 2 + (column_distance + 1) / 2;
  }
  if (config.network_type == entity::MRRGNetworkType::kDiagonal) {
    return std::max(row_distance, column_distance);
  }
  return row_distance + column_distance;
}

int PrisaEngine::ApproximateDirectedResourceDistance(int from_mrrg_node,
                                                     int to_mrrg_node) const {
  if (from_mrrg_node == to_mrrg_node) return 0;
  const auto from = mrrg_.GetNodeProperty(from_mrrg_node);
  const auto to = mrrg_.GetNodeProperty(to_mrrg_node);
  const int context_size = std::max(1, from.context_size);
  const int context_delta =
      (to.context_id - from.context_id + context_size) % context_size;
  const int spatial_steps = SpatialStepDistance(from_mrrg_node, to_mrrg_node);

  if (mrrg_.GetMRRGConfig().cgra_type == entity::MRRGCGRAType::kElastic) {
    if (spatial_steps > 0) return std::max(1, spatial_steps);
    return context_delta == 0 ? 0 : context_delta;
  }

  int steps = std::max(spatial_steps, context_delta);
  if (steps == 0) steps = context_size;
  while (steps % context_size != context_delta) {
    steps++;
  }
  return steps;
}

int PrisaEngine::ManhattanRouteScore(int from_mrrg_node,
                                     int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  if (from_mrrg_node == to_mrrg_node) return 0;
  const auto from = mrrg_.GetNodeProperty(from_mrrg_node);
  const auto to = mrrg_.GetNodeProperty(to_mrrg_node);
  const int context_size = std::max(1, from.context_size);
  const int context_delta =
      (to.context_id - from.context_id + context_size) % context_size;
  return SpatialStepDistance(from_mrrg_node, to_mrrg_node) + context_delta;
}

PrisaEngine::PrisaEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                         mapper::prisa::Placement2DArrayKind kind,
                         const mapper::prisa::Placement2DArrayOptions& options)
    : dfg_(dfg),
      mrrg_(mrrg),
      timeout_s_(options.timeout_s > 0.0 ? options.timeout_s : 1.0),
      log_file_path_(options.log_file_path),
      kind_(kind),
      max_trials_(options.max_trials),
      seed_count_(options.seed_count),
      random_seed_(options.random_seed),
      max_iterations_(options.max_iterations),
      elite_placement_count_(options.elite_placement_count),
      io_node_policy_(
          GridNormalizePolicy(options.io_node_policy.value_or("opcode"))),
      trial_seed_policy_(GridNormalizePolicy(
          options.trial_seed_policy.value_or("continuous"))),
      traversal_order_policy_(GridNormalizePolicy(
          options.traversal_order_policy.value_or("zigzag"))),
      traversal_neighbor_policy_(GridNormalizePolicy(
          options.traversal_neighbor_policy.value_or("default"))),
      candidate_scope_policy_(GridNormalizePolicy(
          options.candidate_scope_policy.value_or("default"))),
      candidate_rank_policy_(GridNormalizePolicy(
          options.candidate_rank_policy.value_or("default"))),
      use_yott_annotations_(options.use_yott_annotations),
      trace_trials_(options.trace_trials),
      config_(mrrg.GetMRRGConfig()),
      base_seed_(static_cast<unsigned int>(
          0x85EBCA6Bu ^
          static_cast<unsigned int>(dfg.GetNodeNum() * 131 + config_.row * 17 +
                                    config_.column))),
      rng_(base_seed_) {
  {}
  grid_placement_ = true;
  prisa_weak_distance_threshold_ = std::numeric_limits<int>::max();
  options_.max_trials = options.max_trials;
  options_.seed_count = options.seed_count;
  options_.random_seed = options.random_seed;
  options_.max_iterations = options.max_iterations;
  GridBuildDFGCache();
  critical_path_score_ = GridBuildCriticalPathScore();
  critical_path_max_ = critical_path_score_.empty()
                           ? 0
                           : *std::max_element(critical_path_score_.begin(),
                                               critical_path_score_.end());
  GridBuildGridCache();
  GridBuildCompatibilityCache();
  GridBuildPRISACache();
}

auto PrisaEngine::GridStartSearch() -> std::chrono::steady_clock::time_point {
  const auto start = std::chrono::steady_clock::now();
  placement_swap_attempts_ = 0;
  cell_visits_ = 0;
  Log("start mapper=" + GridMapperName() +
      " nodes=" + std::to_string(dfg_.GetNodeNum()) +
      " cells=" + std::to_string(rows_ * cols_) +
      " max_trials=" + std::to_string(MaxTrials()) +
      " seed_count=" + std::to_string(SeedCount()));

  return start;
}

std::optional<PlacementState> PrisaEngine::GridSearchPlacement(
    std::chrono::steady_clock::time_point start) {
  return GridRunPRISAMultiSeed(start);
}

mapper::MappingResult PrisaEngine::GridFailure(
    std::chrono::steady_clock::time_point start) const {
  Log("failure mapper=" + GridMapperName() + " reason=" +
      (last_failure_reason_.empty() ? "no placement candidate"
                                    : last_failure_reason_));
  return mapper::MappingResult(false, entity::Mapping(config_),
                               SecondsSince(start));
}

mapper::MappingResult PrisaEngine::GridFinishPlacement(
    const PlacementState& placement,
    std::chrono::steady_clock::time_point start) {
  std::vector<int> dfg_to_mrrg(dfg_.GetNodeNum(), -1);
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    const int cell = placement.dfg_to_cell[node];
    if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) {
      return mapper::MappingResult(false, entity::Mapping(config_),
                                   SecondsSince(start));
    }
    if (GridUsesStructuralIOCellTypes() &&
        !GridIsCPUMappingCompatible(node, cell)) {
      Log("failure mapper=" + GridMapperName() +
          " reason=final placement violates structural I/O node=" +
          GridNodeLabel(node) + " cell=" + std::to_string(cell));
      return mapper::MappingResult(false, entity::Mapping(config_),
                                   SecondsSince(start));
    }
    dfg_to_mrrg[node] = cell_to_mrrg_[cell];
  }

  std::vector<std::vector<int>> empty_routes(dfg_.GetNodeNum());
  entity::Mapping mapping(mrrg_, dfg_, dfg_to_mrrg, empty_routes);
  const double elapsed = SecondsSince(start);
  Log("success mapper=" + GridMapperName() +
      " time_s=" + std::to_string(elapsed) +
      " placement_cost=" + std::to_string(GridPlacementCost(placement)) +
      " placement_swap_attempts=" + std::to_string(placement_swap_attempts_) +
      " cell_visits=" + std::to_string(cell_visits_));
  return mapper::MappingResult(true, mapping, elapsed);
}

void PrisaEngine::GridBuildDFGCache() {
  const int node_num = dfg_.GetNodeNum();
  successors_.assign(node_num, {});
  predecessors_.assign(node_num, {});
  incident_edge_ids_.assign(node_num, {});
  degree_.assign(node_num, 0);
  edges_.clear();
  for (int edge_id = 0; edge_id < dfg_.GetEdgeNum(); edge_id++) {
    const auto [source, target] = dfg_.GetEdgeSourceTarget(edge_id);
    const int local_edge_id = static_cast<int>(edges_.size());
    edges_.push_back({source, target});
    if (source >= 0 && source < node_num) {
      successors_[source].push_back(target);
      incident_edge_ids_[source].push_back(local_edge_id);
    }
    if (target >= 0 && target < node_num && target != source) {
      predecessors_[target].push_back(source);
      incident_edge_ids_[target].push_back(local_edge_id);
    }
  }
  for (int node = 0; node < node_num; node++) {
    degree_[node] =
        static_cast<int>(successors_[node].size() + predecessors_[node].size());
  }
}

void PrisaEngine::GridBuildGridCache() {
  rows_ = config_.row;
  cols_ = config_.column;
  cell_to_mrrg_.assign(rows_ * cols_, -1);
  cell_ops_.assign(rows_ * cols_, {});
  cell_memory_accessible_.assign(rows_ * cols_, false);
  for (int node_id = 0; node_id < mrrg_.GetNodeNum(); node_id++) {
    const auto property = mrrg_.GetNodeProperty(node_id);
    if (property.context_id != 0) continue;
    const int row = property.position_id.first;
    const int col = property.position_id.second;
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_) continue;
    const int cell = GridCell(row, col);
    cell_to_mrrg_[cell] = node_id;
    cell_ops_[cell] = property.supported_operations;
    cell_memory_accessible_[cell] = property.is_memory_accessible;
  }
  bool has_io_cell = false;
  bool has_compute_cell = false;
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (cell_to_mrrg_[cell] < 0) continue;
    has_io_cell = has_io_cell || cell_memory_accessible_[cell];
    has_compute_cell = has_compute_cell || !cell_memory_accessible_[cell];
  }
  separate_io_cells_ = has_io_cell && has_compute_cell;

  // GridCell connectivity depends only on the architecture. Cache it once
  // rather than rebuilding the same all-pairs table for every placement trial.
  initial_freedom_.assign(rows_ * cols_, 0);
  for (int cell = 0; cell < rows_ * cols_; cell++) {
    if (cell_to_mrrg_[cell] >= 0)
      initial_freedom_[cell] = GridCellFreedom(cell);
  }
}

void PrisaEngine::GridBuildCompatibilityCache() {
  compatible_cells_.assign(dfg_.GetNodeNum(), {});
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    auto& cells = compatible_cells_[node];
    cells.reserve(cell_to_mrrg_.size());
    for (int cell = 0; cell < static_cast<int>(cell_to_mrrg_.size()); cell++) {
      if (GridIsCompatible(node, cell)) cells.push_back(cell);
    }
  }
}

int PrisaEngine::GridPRISAResourceSideLength() const {
  return std::max(rows_, cols_);
}

int PrisaEngine::GridPRISAWeakPairCount(int resource_count) const {
  const int k = std::max(1, GridPRISAResourceSideLength());
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

void PrisaEngine::GridBuildPRISACache() {
  prisa_cell_order_.clear();
  prisa_cell_order_.reserve(rows_ * cols_);
  for (int row = 0; row < rows_; row++) {
    if ((row % 2) == 0) {
      for (int col = 0; col < cols_; col++) {
        const int cell = GridCell(row, col);
        if (cell_to_mrrg_[cell] >= 0) prisa_cell_order_.push_back(cell);
      }
    } else {
      for (int col = cols_ - 1; col >= 0; col--) {
        const int cell = GridCell(row, col);
        if (cell_to_mrrg_[cell] >= 0) prisa_cell_order_.push_back(cell);
      }
    }
  }

  prisa_order_index_.assign(rows_ * cols_, -1);
  for (int i = 0; i < static_cast<int>(prisa_cell_order_.size()); i++) {
    prisa_order_index_[prisa_cell_order_[i]] = i;
  }

  const int resource_count = static_cast<int>(prisa_cell_order_.size());
  prisa_distance_matrix_.assign(resource_count * resource_count, 0);
  prisa_weak_region_matrix_.assign(resource_count * resource_count, 0);
  prisa_weak_distance_threshold_ = std::numeric_limits<int>::max();
  if (resource_count < 2) return;

  for (int row = 0; row < resource_count; row++) {
    for (int column = 0; column < resource_count; column++) {
      if (row == column) continue;
      const int distance =
          GridDistanceCost(prisa_cell_order_[row], prisa_cell_order_[column]);
      prisa_distance_matrix_[row * resource_count + column] = distance;
    }
  }

  std::vector<int> pair_distances;
  pair_distances.reserve(resource_count * (resource_count - 1) / 2);
  for (int row = 0; row < resource_count; row++) {
    for (int column = 0; column < resource_count; column++) {
      if (row == column) continue;
      if (row < column) {
        pair_distances.push_back(
            prisa_distance_matrix_[row * resource_count + column]);
      }
    }
  }

  const int weak_pair_count = GridPRISAWeakPairCount(resource_count);
  if (weak_pair_count <= 0 || pair_distances.empty()) return;
  std::sort(pair_distances.begin(), pair_distances.end(), std::greater<int>());
  const int threshold_index =
      std::min(weak_pair_count, static_cast<int>(pair_distances.size())) - 1;
  prisa_weak_distance_threshold_ = pair_distances[threshold_index];
  for (int row = 0; row < resource_count; row++) {
    for (int column = 0; column < resource_count; column++) {
      if (row == column) continue;
      // The paper separates the distance matrix into heavy weak entries and
      // light potential entries.  On a mesh, many cells share the same
      // distance, so the threshold intentionally includes ties at the weak
      // boundary rather than imposing an arbitrary cell ordering.
      prisa_weak_region_matrix_[row * resource_count + column] =
          prisa_distance_matrix_[row * resource_count + column] >=
          prisa_weak_distance_threshold_;
    }
  }
}

std::string PrisaEngine::GridNormalizePolicy(std::string policy) {
  std::transform(
      policy.begin(), policy.end(), policy.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return policy;
}

int PrisaEngine::GridCell(int row, int col) const { return row * cols_ + col; }

int PrisaEngine::GridRow(int cell) const { return cell / cols_; }

int PrisaEngine::GridCol(int cell) const { return cell % cols_; }

bool PrisaEngine::GridIsCompatible(int dfg_node, int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return false;
  if (cell_to_mrrg_[cell] < 0) return false;
  if (GridUsesStructuralIOCellTypes()) {
    return true;
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return std::find(cell_ops_[cell].begin(), cell_ops_[cell].end(), op) !=
         cell_ops_[cell].end();
}

bool PrisaEngine::GridIsIONode(int dfg_node) const {
  if (io_node_policy_ == "structural") {
    return predecessors_[dfg_node].empty() || successors_[dfg_node].empty();
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return entity::IsMemoryAccessOperation(op);
}

bool PrisaEngine::GridCanPlace(int dfg_node, int cell,
                               const PlacementState& state) const {
  if (state.cell_to_dfg[cell] >= 0 || !GridIsCompatible(dfg_node, cell)) {
    return false;
  }
  if (GridUsesStructuralIOCellTypes()) {
    const int cell_type = GridCPUMappingCellType(cell);
    return cell_type >= 0 && cell_type == (GridIsIONode(dfg_node) ? 1 : 0);
  }
  if (separate_io_cells_ &&
      cell_memory_accessible_[cell] != GridIsIONode(dfg_node)) {
    return false;
  }
  return true;
}

int PrisaEngine::GridCPUMappingCellType(int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return -1;
  if (cell_to_mrrg_[cell] < 0) return -1;
  const int row = GridRow(cell);
  const int col = GridCol(cell);
  const bool is_corner =
      (row == 0 || row == rows_ - 1) && (col == 0 || col == cols_ - 1);
  if (separate_io_cells_) {
    // Structural-I/O YOTO/YOTT uses a logical type matrix: perimeter cells
    // host source/sink-like nodes and interior cells host compute nodes.
    // `perimeter_no_corners` still creates corner MRRG nodes, but the
    // paper-style grid treats those corner sites as unavailable.
    if (is_corner && !cell_memory_accessible_[cell]) return -1;
    return cell_memory_accessible_[cell] ? 1 : 0;
  }
  if (is_corner) return -1;
  const bool is_perimeter =
      row == 0 || col == 0 || row == rows_ - 1 || col == cols_ - 1;
  return is_perimeter ? 1 : 0;
}

bool PrisaEngine::GridIsCPUMappingCompatible(int dfg_node, int cell) const {
  if (!GridIsCompatible(dfg_node, cell)) return false;
  const int cell_type = GridCPUMappingCellType(cell);
  if (cell_type < 0) return false;
  return cell_type == (GridIsIONode(dfg_node) ? 1 : 0);
}

int PrisaEngine::GridCellFreedom(int cell) const {
  int result = 0;
  const int radius =
      config_.network_type == entity::MRRGNetworkType::kOneHop2d ? 2 : 1;
  const int row = GridRow(cell);
  const int col = GridCol(cell);
  for (int dr = -radius; dr <= radius; dr++) {
    for (int dc = -radius; dc <= radius; dc++) {
      if (dr == 0 && dc == 0) continue;
      const int other_row = row + dr;
      const int other_col = col + dc;
      if (other_row < 0 || other_col < 0 || other_row >= rows_ ||
          other_col >= cols_) {
        continue;
      }
      const int other = GridCell(other_row, other_col);
      if (cell_to_mrrg_[other] < 0) continue;
      if (GridDistanceCost(cell, other) == 1) result++;
    }
  }
  return result;
}

int PrisaEngine::GridDistanceCost(int a, int b) const {
  const int dx = std::abs(GridRow(a) - GridRow(b));
  const int dy = std::abs(GridCol(a) - GridCol(b));
  if (config_.network_type == entity::MRRGNetworkType::kOneHop2d) {
    return std::max(1, (dx + 1) / 2 + (dy + 1) / 2);
  }
  if (config_.network_type == entity::MRRGNetworkType::kDiagonal) {
    return std::max(1, std::max(dx, dy));
  }
  return std::max(1, dx + dy);
}

namespace {

double Percentile(std::vector<int> values, double percentile) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const double rank = percentile / 100.0 * (values.size() - 1);
  const int lower = static_cast<int>(std::floor(rank));
  const int upper = static_cast<int>(std::ceil(rank));
  if (lower == upper) return values[lower];
  const double fraction = rank - lower;
  return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

void WriteCost(std::ofstream& trace, double value) {
  if (value >= kGridImpossibleCost * 0.5 || !std::isfinite(value)) return;
  trace << value;
}

}  // namespace

// namespace

// namespace

std::string PrisaEngine::GridMapperName() const {
  switch (kind_) {
    case mapper::prisa::Placement2DArrayKind::kPRISA:
      return "Placement2DArrayPRISA";
    case mapper::prisa::Placement2DArrayKind::kPRISANoSIS:
      return "Placement2DArrayPRISANoSIS";
    case mapper::prisa::Placement2DArrayKind::kCostAwarePRISA:
      return "Placement2DArrayCostAwarePRISA";
  }
  return "Placement2DArray";
}

bool PrisaEngine::GridUsesPRISASIS() const {
  return kind_ == mapper::prisa::Placement2DArrayKind::kPRISA ||
         kind_ == mapper::prisa::Placement2DArrayKind::kCostAwarePRISA;
}

bool PrisaEngine::GridUsesCostAwarePRISA() const {
  return kind_ == mapper::prisa::Placement2DArrayKind::kCostAwarePRISA;
}

bool PrisaEngine::GridUsesStructuralIOCellTypes() const {
  return io_node_policy_ == "structural" && (false || false);
}

std::string PrisaEngine::GridNodeLabel(int node) const {
  if (node < 0 || node >= dfg_.GetNodeNum()) return std::to_string(node);
  const auto property = dfg_.GetNodeProperty(node);
  return std::to_string(node) + ":" + property.op_name + "/" + property.op_str;
}

}  // namespace mapper::prisa
