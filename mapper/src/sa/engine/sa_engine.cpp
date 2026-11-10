#include <mapper/sa/engine/sa_engine.hpp>

namespace mapper::sa {

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

SaEngine::SaEngine(entity::DFG& dfg, entity::MRRG& mrrg,
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
  if (!UsesSpatialResources()) BuildAllPairsDistances();
  {}
  annotation_ = BuildAnnotation();
}

auto SaEngine::StartSearch() -> std::chrono::steady_clock::time_point {
  if (grid_placement_) return GridStartSearch();

  const auto start = std::chrono::steady_clock::now();
  Log("start mapper=" + MapperName() +
      " dfg_nodes=" + std::to_string(dfg_.GetNodeNum()) +
      " mrrg_nodes=" + std::to_string(mrrg_.GetNodeNum()) +
      " max_trials=" + std::to_string(MaxTrials()) +
      " seed_count=" + std::to_string(SeedCount()) +
      " routing_retry_count=" + std::to_string(RoutingRetryCount()) +
      " max_iterations=" + std::to_string(MaxIterations()));

  return start;
}

std::optional<PlacementState> SaEngine::SearchPlacement(
    std::chrono::steady_clock::time_point start) {
  if (grid_placement_) return GridSearchPlacement(start);

  return RunSAMultiSeed(start);
}

mapper::MappingResult SaEngine::Failure(
    std::chrono::steady_clock::time_point start) const {
  if (grid_placement_) return GridFailure(start);

  return MakeFailureResult(start);
}

mapper::MappingResult SaEngine::FinishPlacementOnly(
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

std::optional<RouteUsage> SaEngine::RoutePlacement(
    const PlacementState& placement) {
  return TryRoutePlacement(placement);
}

mapper::MappingResult SaEngine::FinishRoutedMapping(
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

SaEngine::SaEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                   mapper::sa::Placement2DArrayKind kind,
                   const mapper::sa::Placement2DArrayOptions& options)
    : dfg_(dfg),
      mrrg_(mrrg),
      timeout_s_(options.timeout_s > 0.0 ? options.timeout_s : 1.0),
      log_file_path_(options.log_file_path),
      kind_(kind),
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
  grid_placement_ = true;
  options_.max_trials = options.max_trials;
  options_.seed_count = options.seed_count;
  options_.random_seed = options.random_seed;
  options_.max_iterations = options.max_iterations;
  GridBuildDFGCache();
  critical_path_score_ = BuildCriticalPathScore();
  critical_path_max_ = critical_path_score_.empty()
                           ? 0
                           : *std::max_element(critical_path_score_.begin(),
                                               critical_path_score_.end());
  GridBuildGridCache();
  GridBuildCompatibilityCache();
}

auto SaEngine::GridStartSearch() -> std::chrono::steady_clock::time_point {
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

std::optional<PlacementState> SaEngine::GridSearchPlacement(
    std::chrono::steady_clock::time_point start) {
  return GridRunSAMultiSeed(start);
}

mapper::MappingResult SaEngine::GridFailure(
    std::chrono::steady_clock::time_point start) const {
  Log("failure mapper=" + GridMapperName() + " reason=" +
      (last_failure_reason_.empty() ? "no placement candidate"
                                    : last_failure_reason_));
  return mapper::MappingResult(false, entity::Mapping(config_),
                               SecondsSince(start));
}

mapper::MappingResult SaEngine::GridFinishPlacement(
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

std::string SaEngine::MapperName() const {
  if (grid_placement_) return GridMapperName();
  return search_kind_ == PlacementSearchKind::kPlacement2DSA ? "Placement2DSA"
                                                             : "SA";
}

mapper::MappingResult SaEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping SaEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

void SaEngine::BuildAllPairsDistances() {
  const int node_num = mrrg_.GetNodeNum();
  distance_.assign(node_num, std::vector<int>(node_num, kInfDistance));
  for (int source = 0; source < node_num; source++) {
    std::queue<int> q;
    distance_[source][source] = 0;
    q.push(source);
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      for (int v : mrrg_.GetAdjacentNodeIdVec(u)) {
        if (distance_[source][v] != kInfDistance) continue;
        distance_[source][v] = distance_[source][u] + 1;
        q.push(v);
      }
    }
  }
}

Annotation SaEngine::BuildAnnotation() {
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

bool SaEngine::CanOccupyResource(int dfg_node_id, int mrrg_node_id,
                                 const PlacementState& state) const {
  if (state.mrrg_to_dfg[mrrg_node_id] != -1) return false;
  if (!IsCompatible(dfg_node_id, mrrg_node_id)) return false;
  if (UsesSeparatePlacement2DIOCells()) {
    const bool is_io_node = IsIONode(dfg_node_id);
    const bool is_memory_cell =
        mrrg_.GetNodeProperty(mrrg_node_id).is_memory_accessible;
    if (is_io_node && !is_memory_cell) return false;
  }
  if (UsesSpatialResources() &&
      IsPhysicalPositionOccupied(mrrg_node_id, state)) {
    return false;
  }
  return true;
}

int SaEngine::ResourceFreedom(int mrrg_node_id,
                              const PlacementState& state) const {
  int result = 0;
  std::set<std::pair<int, int>> counted_positions;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (UsesSpatialResources() && IsPhysicalPositionOccupied(r, state)) {
      continue;
    }
    if (!UsesSpatialResources() && state.mrrg_to_dfg[r] != -1) {
      continue;
    }
    if (!SupportsAnyDFGOperation(r)) continue;
    if (UsesSpatialResources()) {
      if (SamePhysicalPosition(mrrg_node_id, r)) continue;
      if (PhysicalDistance(mrrg_node_id, r) != 1) continue;
      if (counted_positions.insert(mrrg_.GetNodeProperty(r).position_id).second)
        result++;
    } else if (ResourceDistance(mrrg_node_id, r) == 1) {
      result++;
    }
  }
  return result;
}

bool SaEngine::ApplyRandomMove(PlacementState& state) {
  const int node_num = dfg_.GetNodeNum();
  if (node_num <= 0) return false;
  std::uniform_int_distribution<int> node_dist(0, node_num - 1);
  std::uniform_real_distribution<double> unit(0.0, 1.0);

  if (unit(rng_) < 0.55) {
    // Swap two DFG nodes when both operations are legal at the other's PE.
    int a = node_dist(rng_);
    int b = node_dist(rng_);
    return ApplySwapMove(state, a, b);
  }

  return ApplyMoveToFreeResource(state);
}

bool SaEngine::ApplySwapMove(PlacementState& state, int a, int b) const {
  if (a == b) return false;
  int ra = state.dfg_to_mrrg[a];
  int rb = state.dfg_to_mrrg[b];
  if (ra < 0 || rb < 0) return false;
  if (!IsCompatible(a, rb) || !IsCompatible(b, ra)) return false;
  std::swap(state.dfg_to_mrrg[a], state.dfg_to_mrrg[b]);
  state.mrrg_to_dfg[ra] = b;
  state.mrrg_to_dfg[rb] = a;
  return true;
}

bool SaEngine::ApplyMoveToFreeResource(PlacementState& state) {
  const int node_num = dfg_.GetNodeNum();
  if (node_num <= 0) return false;
  std::uniform_int_distribution<int> node_dist(0, node_num - 1);
  int a = node_dist(rng_);
  std::vector<int> free_candidates;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (CanOccupyResource(a, r, state)) {
      free_candidates.push_back(r);
    }
  }
  if (free_candidates.empty()) return false;
  std::uniform_int_distribution<int> resource_dist(
      0, static_cast<int>(free_candidates.size()) - 1);
  int old_r = state.dfg_to_mrrg[a];
  int new_r = free_candidates[resource_dist(rng_)];
  state.dfg_to_mrrg[a] = new_r;
  state.mrrg_to_dfg[old_r] = -1;
  state.mrrg_to_dfg[new_r] = a;
  return true;
}

double SaEngine::DegreeMatchScore(int dfg_node_id, int mrrg_node_id,
                                  const PlacementState& state) const {
  const int target_degree = annotation_.degree[dfg_node_id];
  const int freedom = ResourceFreedom(mrrg_node_id, state);
  if (freedom >= target_degree) return freedom - target_degree;
  return 4.0 * (target_degree - freedom);
}

std::vector<int> SaEngine::GetRawOutputTraversalRoots() {
  std::vector<int> roots;
  for (int v = 0; v < dfg_.GetNodeNum(); v++) {
    if (successors_[v].empty() ||
        dfg_.GetNodeProperty(v).op == entity::OpType::kOutput) {
      roots.push_back(v);
    }
  }
  if (roots.empty()) {
    for (int v = 0; v < dfg_.GetNodeNum(); v++) roots.push_back(v);
  }
  std::shuffle(roots.begin(), roots.end(), rng_);
  return roots;
}

std::vector<int> SaEngine::GetOutputTraversalRoots() {
  std::vector<int> roots;
  for (int v = 0; v < dfg_.GetNodeNum(); v++) {
    if (successors_[v].empty() ||
        dfg_.GetNodeProperty(v).op == entity::OpType::kOutput) {
      roots.push_back(v);
    }
  }
  if (roots.empty()) {
    for (int v = 0; v < dfg_.GetNodeNum(); v++) roots.push_back(v);
  }
  std::shuffle(roots.begin(), roots.end(), rng_);
  std::stable_sort(roots.begin(), roots.end(), [&](int a, int b) {
    if (IsIONode(a) != IsIONode(b)) return IsIONode(a) > IsIONode(b);
    return annotation_.degree[a] > annotation_.degree[b];
  });
  return roots;
}

int SaEngine::FindDFGEdgeId(int source, int target) const {
  for (const auto& edge : dfg_edges_) {
    if (edge.source == source && edge.target == target) return edge.id;
  }
  for (const auto& edge : dfg_edges_) {
    if (edge.source == target && edge.target == source) return edge.id;
  }
  return -1;
}

std::vector<int> SaEngine::UnvisitedIncidentEdges(
    int dfg_node_id, const std::vector<char>& visited_edges) const {
  std::vector<int> result;
  for (int edge_id : incident_edge_ids_[dfg_node_id]) {
    if (!visited_edges[edge_id]) result.push_back(edge_id);
  }
  return result;
}

int SaEngine::PickTraversalEdge(int dfg_node_id,
                                const std::vector<char>& visited_nodes,
                                const std::vector<char>& visited_edges) {
  std::vector<int> candidates =
      UnvisitedIncidentEdges(dfg_node_id, visited_edges);
  if (candidates.empty()) return -1;
  std::shuffle(candidates.begin(), candidates.end(), rng_);
  std::stable_sort(candidates.begin(), candidates.end(), [&](int a, int b) {
    const int oa = OtherEndpoint(dfg_edges_[a], dfg_node_id);
    const int ob = OtherEndpoint(dfg_edges_[b], dfg_node_id);
    const bool a_unvisited = oa >= 0 && !visited_nodes[oa];
    const bool b_unvisited = ob >= 0 && !visited_nodes[ob];
    if (a_unvisited != b_unvisited) return a_unvisited > b_unvisited;
    const int degree_a = oa >= 0 ? annotation_.degree[oa] : -1;
    const int degree_b = ob >= 0 ? annotation_.degree[ob] : -1;
    return degree_a > degree_b;
  });
  return candidates.front();
}

std::vector<int> SaEngine::BuildCriticalPathScore() const {
  const int n = dfg_.GetNodeNum();
  std::vector<int> memo(n, -1);
  std::vector<char> visiting(n, false);
  std::function<int(int)> dfs = [&](int node) {
    if (memo[node] >= 0) return memo[node];
    if (visiting[node]) return 0;
    visiting[node] = true;
    int best = 0;
    for (int successor : successors_[node]) {
      if (successor == node) continue;
      best = std::max(best, 1 + dfs(successor));
    }
    visiting[node] = false;
    memo[node] = best;
    return best;
  };
  for (int node = 0; node < n; node++) dfs(node);
  return memo;
}

int SaEngine::ChoosePlacement2DNeighbor(
    const std::vector<int>& candidates, bool choose_from_fanout, int mode,
    bool zigzag_take_back, const std::vector<int>& critical_path,
    const std::vector<double>& betweenness) {
  if (candidates.empty()) return -1;
  if (candidates.size() == 1) return candidates.front();

  if (mode == 0) {
    return *std::max_element(
        candidates.begin(), candidates.end(), [&](int a, int b) {
          const int da = choose_from_fanout
                             ? static_cast<int>(successors_[a].size())
                             : static_cast<int>(predecessors_[a].size());
          const int db = choose_from_fanout
                             ? static_cast<int>(successors_[b].size())
                             : static_cast<int>(predecessors_[b].size());
          if (da != db) return da < db;
          return annotation_.degree[a] < annotation_.degree[b];
        });
  }
  if (mode == 1) {
    return *std::max_element(
        candidates.begin(), candidates.end(), [&](int a, int b) {
          if (betweenness[a] != betweenness[b]) {
            return betweenness[a] < betweenness[b];
          }
          return annotation_.degree[a] < annotation_.degree[b];
        });
  }
  if (mode == 2) {
    return *std::max_element(
        candidates.begin(), candidates.end(), [&](int a, int b) {
          if (critical_path[a] != critical_path[b]) {
            return critical_path[a] < critical_path[b];
          }
          return annotation_.degree[a] < annotation_.degree[b];
        });
  }
  if (mode == 3) {
    return zigzag_take_back ? candidates.back() : candidates.front();
  }

  std::uniform_int_distribution<int> dist(
      0, static_cast<int>(candidates.size()) - 1);
  return candidates[dist(rng_)];
}

bool SaEngine::UsesSeparatePlacement2DIOCells() const {
  return false;
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

int SaEngine::SeedCount() const {
  if (!options_.seed_count.has_value()) return 1;
  return std::max(1, options_.seed_count.value());
}

int SaEngine::MaxTrials() const {
  const int default_trials = std::max(100, dfg_.GetNodeNum() * 4);
  if (!options_.max_trials.has_value()) return default_trials;
  return std::max(1, options_.max_trials.value());
}

int SaEngine::RoutingRetryCount() const {
  if (!options_.routing_retry_count.has_value()) return 4;
  return std::max(1, options_.routing_retry_count.value());
}

int SaEngine::MaxIterations() const {
  const int default_iterations = std::max(
      5000, dfg_.GetNodeNum() * (grid_placement_ ? rows_ * cols_ * 20
                                                 : mrrg_.GetNodeNum() * 30));
  if (!options_.max_iterations.has_value()) return default_iterations;
  return std::max(1, options_.max_iterations.value());
}

unsigned int SaEngine::SeedFor(int seed_index) const {
  const unsigned int configured_seed =
      options_.random_seed.has_value()
          ? static_cast<unsigned int>(options_.random_seed.value())
          : base_seed_;
  return configured_seed + static_cast<unsigned int>(0x9E3779B9u * seed_index);
}

void SaEngine::ResetSeed(int seed_index) {
  const auto seed = SeedFor(seed_index);
  rng_.seed(seed);
  if (grid_placement_) std::srand(seed);
}

void SaEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

double SaEngine::ResourcePairPlacementCost(int from_mrrg_node,
                                           int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return 1.0e8;
  const int distance = DirectedResourceDistance(from_mrrg_node, to_mrrg_node);
  if (distance >= (1 << 28)) return 1.0e8;
  return DirectedDistanceCost(distance);
}

double SaEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : dfg_edges_) {
    const double edge_cost = EdgePlacementCost(edge, state.dfg_to_mrrg);
    if (edge_cost >= 1.0e8) return 1.0e8;
    cost += edge_cost;
  }
  return cost;
}

void SaEngine::BuildMRRGCache() {
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

bool SaEngine::HasTimedOut(std::chrono::steady_clock::time_point start,
                           double reserve_s) const {
  if (reserve_s < 0.0) reserve_s = grid_placement_ ? 0.01 : 0.02;
  return SecondsSince(start) + reserve_s >= timeout_s_;
}

bool SaEngine::IsCompatible(int dfg_node_id, int mrrg_node_id) const {
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
  return SupportsOperation(mrrg_.GetNodeProperty(mrrg_node_id), op);
}

bool SaEngine::SamePhysicalPosition(int lhs_mrrg_node,
                                    int rhs_mrrg_node) const {
  return mrrg_.GetNodeProperty(lhs_mrrg_node).position_id ==
         mrrg_.GetNodeProperty(rhs_mrrg_node).position_id;
}

bool SaEngine::IsPhysicalPositionOccupied(int mrrg_node_id,
                                          const PlacementState& state) const {
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (state.mrrg_to_dfg[r] == -1) continue;
    if (SamePhysicalPosition(mrrg_node_id, r)) return true;
  }
  return false;
}

std::vector<int> SaEngine::CompatibleResources(
    int dfg_node_id, const PlacementState& state) const {
  std::vector<int> result;
  result.reserve(mrrg_.GetNodeNum());
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (!CanOccupyResource(dfg_node_id, r, state)) continue;
    result.push_back(r);
  }
  return result;
}

std::vector<int> SaEngine::ClosestCompatibleResources(
    int dfg_node_id, int anchor_mrrg_node, const PlacementState& state) const {
  if (anchor_mrrg_node < 0) return CompatibleResources(dfg_node_id, state);
  std::vector<int> result;
  int best_distance = (1 << 28);
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (!CanOccupyResource(dfg_node_id, r, state)) continue;
    const int distance = false ? PhysicalDistance(anchor_mrrg_node, r)
                               : ResourceDistance(anchor_mrrg_node, r);
    if (distance > best_distance) continue;
    if (distance < best_distance) {
      result.clear();
      best_distance = distance;
    }
    result.push_back(r);
  }
  return result;
}

bool SaEngine::IsIONode(int dfg_node_id) const {
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
  return entity::IsMemoryAccessOperation(op);
}

bool SaEngine::SupportsAnyDFGOperation(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  for (entity::OpType op : property.supported_operations) {
    if (entity::IsDFGOp(op) && op != entity::OpType::kRoute &&
        op != entity::OpType::kNop) {
      return true;
    }
  }
  return false;
}

int SaEngine::ResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  return std::min(DirectedResourceDistance(from_mrrg_node, to_mrrg_node),
                  DirectedResourceDistance(to_mrrg_node, from_mrrg_node));
}

int SaEngine::DirectedResourceDistance(int from_mrrg_node,
                                       int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  if (!distance_.empty()) return distance_[from_mrrg_node][to_mrrg_node];
  return ApproximateDirectedResourceDistance(from_mrrg_node, to_mrrg_node);
}

int SaEngine::SpatialStepDistance(int from_mrrg_node, int to_mrrg_node) const {
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

int SaEngine::ApproximateDirectedResourceDistance(int from_mrrg_node,
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

int SaEngine::PhysicalDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  return std::max(1, SpatialStepDistance(from_mrrg_node, to_mrrg_node));
}

int SaEngine::BorderDistance(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  const int row = property.position_id.first;
  const int column = property.position_id.second;
  const auto config = mrrg_.GetMRRGConfig();
  return std::min(std::min(row, config.row - 1 - row),
                  std::min(column, config.column - 1 - column));
}

double SaEngine::IOPlacementScore(int dfg_node_id, int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  double score = BorderDistance(mrrg_node_id);
  if (entity::IsMemoryAccessOperation(dfg_.GetNodeProperty(dfg_node_id).op) ||
      dfg_.GetNodeProperty(dfg_node_id).op == entity::OpType::kOutput) {
    score += property.is_memory_accessible ? -8.0 : 40.0;
  } else {
    score += property.is_memory_accessible ? -1.0 : 0.0;
  }
  return score;
}

int SaEngine::ManhattanRouteScore(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  if (from_mrrg_node == to_mrrg_node) return 0;
  const auto from = mrrg_.GetNodeProperty(from_mrrg_node);
  const auto to = mrrg_.GetNodeProperty(to_mrrg_node);
  const int context_size = std::max(1, from.context_size);
  const int context_delta =
      (to.context_id - from.context_id + context_size) % context_size;
  return SpatialStepDistance(from_mrrg_node, to_mrrg_node) + context_delta;
}

void SaEngine::GridBuildDFGCache() {
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

void SaEngine::GridBuildGridCache() {
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

void SaEngine::GridBuildCompatibilityCache() {
  compatible_cells_.assign(dfg_.GetNodeNum(), {});
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    auto& cells = compatible_cells_[node];
    cells.reserve(cell_to_mrrg_.size());
    for (int cell = 0; cell < static_cast<int>(cell_to_mrrg_.size()); cell++) {
      if (GridIsCompatible(node, cell)) cells.push_back(cell);
    }
  }
}

std::string SaEngine::GridNormalizePolicy(std::string policy) {
  std::transform(
      policy.begin(), policy.end(), policy.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return policy;
}

int SaEngine::GridCell(int row, int col) const { return row * cols_ + col; }

int SaEngine::GridRow(int cell) const { return cell / cols_; }

int SaEngine::GridCol(int cell) const { return cell % cols_; }

bool SaEngine::GridIsCompatible(int dfg_node, int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return false;
  if (cell_to_mrrg_[cell] < 0) return false;
  if (GridUsesStructuralIOCellTypes()) {
    return true;
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return std::find(cell_ops_[cell].begin(), cell_ops_[cell].end(), op) !=
         cell_ops_[cell].end();
}

bool SaEngine::GridIsIONode(int dfg_node) const {
  if (io_node_policy_ == "structural") {
    return predecessors_[dfg_node].empty() || successors_[dfg_node].empty();
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return entity::IsMemoryAccessOperation(op);
}

bool SaEngine::GridCanPlace(int dfg_node, int cell,
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

int SaEngine::GridCPUMappingCellType(int cell) const {
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

bool SaEngine::GridIsCPUMappingCompatible(int dfg_node, int cell) const {
  if (!GridIsCompatible(dfg_node, cell)) return false;
  const int cell_type = GridCPUMappingCellType(cell);
  if (cell_type < 0) return false;
  return cell_type == (GridIsIONode(dfg_node) ? 1 : 0);
}

int SaEngine::GridCellFreedom(int cell) const {
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

int SaEngine::GridDistanceCost(int a, int b) const {
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

std::string SaEngine::GridMapperName() const {
  switch (kind_) {
    case mapper::sa::Placement2DArrayKind::kSA:
      return "Placement2DArraySA";
  }
  return "Placement2DArray";
}

bool SaEngine::GridUsesStructuralIOCellTypes() const {
  return io_node_policy_ == "structural" && (false || false);
}

std::string SaEngine::GridNodeLabel(int node) const {
  if (node < 0 || node >= dfg_.GetNodeNum()) return std::to_string(node);
  const auto property = dfg_.GetNodeProperty(node);
  return std::to_string(node) + ":" + property.op_name + "/" + property.op_str;
}

}  // namespace mapper::sa
