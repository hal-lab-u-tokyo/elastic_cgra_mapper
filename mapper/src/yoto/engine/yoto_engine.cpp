#include <mapper/yoto/engine/yoto_engine.hpp>

namespace mapper::yoto {

std::string YotoEngine::MapperName() const {
  if (grid_placement_) return GridMapperName();
  switch (search_kind_) {
    case PlacementSearchKind::kYOTO:
      return "YOTO";
    case PlacementSearchKind::kModuloPhysicalYOTO:
      return "ModuloPhysicalYOTO";
    case PlacementSearchKind::kYOTOWithFallback:
      return "YOTOWithFallback";

    case PlacementSearchKind::kPlacement2DYOTO:
      return "Placement2DYOTO";
  }
  return "unknown";
}

bool YotoEngine::IsPhysicalThenContextLike() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalYOTO || false ||
         false || false;
}

bool YotoEngine::UsesPhysicalPlacementStage() const {
  return UsesSpatialResources() || IsPhysicalThenContextLike();
}

bool YotoEngine::UsesPaperTraversalPlan() const {
  return UsesSpatialResources() || search_kind_ == PlacementSearchKind::kYOTO ||
         false || IsPhysicalThenContextLike() ||
         search_kind_ == PlacementSearchKind::kYOTOWithFallback || false ||
         false;
}

bool YotoEngine::UsesModuloFallbackCandidates() const {
  return search_kind_ == PlacementSearchKind::kYOTOWithFallback || false;
}

bool YotoEngine::UsesPhysicalPeExclusivePlacement() const {
  return UsesPhysicalPlacementStage() || false || false;
}

bool YotoEngine::UsesSeparatePlacement2DIOCells() const {
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

bool YotoEngine::UsesApproximatePlacementDistance() const {
  return UsesSpatialResources() || false || false ||
         IsPhysicalThenContextLike() || false || false;
}

bool YotoEngine::NeedsAllPairsDistances() const {
  return !UsesApproximatePlacementDistance();
}

mapper::MappingResult YotoEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping YotoEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

void YotoEngine::BuildAllPairsDistances() {
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

Annotation YotoEngine::BuildAnnotation() {
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

bool YotoEngine::CanOccupyResource(int dfg_node_id, int mrrg_node_id,
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

std::vector<int> YotoEngine::ClosestCompatibleResources(
    int dfg_node_id, int anchor_mrrg_node, const PlacementState& state) const {
  if (anchor_mrrg_node < 0) return CompatibleResources(dfg_node_id, state);
  std::vector<int> result;
  int best_distance = kInfDistance;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (!CanOccupyResource(dfg_node_id, r, state)) continue;
    const int distance = UsesPhysicalPlacementStage()
                             ? PhysicalDistance(anchor_mrrg_node, r)
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

int YotoEngine::ResourceFreedom(int mrrg_node_id,
                                const PlacementState& state) const {
  int result = 0;
  std::set<std::pair<int, int>> counted_positions;
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (UsesPhysicalPeExclusivePlacement() &&
        IsPhysicalPositionOccupied(r, state)) {
      continue;
    }
    if (!UsesPhysicalPeExclusivePlacement() && state.mrrg_to_dfg[r] != -1) {
      continue;
    }
    if (!SupportsAnyDFGOperation(r)) continue;
    if (UsesPhysicalPeExclusivePlacement()) {
      if (SamePhysicalPosition(mrrg_node_id, r)) continue;
      if (PhysicalDistance(mrrg_node_id, r) != 1) continue;
      const auto position = mrrg_.GetNodeProperty(r).position_id;
      if (counted_positions.insert(position).second) result++;
    } else if (ResourceDistance(mrrg_node_id, r) == 1) {
      result++;
    }
  }
  return result;
}

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

YotoEngine::YotoEngine(entity::DFG& dfg, entity::MRRG& mrrg,
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
  if (!UsesSpatialResources() && NeedsAllPairsDistances()) {
    BuildAllPairsDistances();
  }
  {}
  annotation_ = BuildAnnotation();
}

auto YotoEngine::StartSearch() -> std::chrono::steady_clock::time_point {
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

std::optional<PlacementState> YotoEngine::SearchPlacement(
    std::chrono::steady_clock::time_point start) {
  if (grid_placement_) return GridSearchPlacement(start);

  return RunGreedyMultiStart(start);
}

mapper::MappingResult YotoEngine::Failure(
    std::chrono::steady_clock::time_point start) const {
  if (grid_placement_) return GridFailure(start);

  return MakeFailureResult(start);
}

mapper::MappingResult YotoEngine::FinishPlacementOnly(
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

std::optional<RouteUsage> YotoEngine::RoutePlacement(
    const PlacementState& placement) {
  return TryRoutePlacement(placement);
}

mapper::MappingResult YotoEngine::FinishRoutedMapping(
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

double YotoEngine::DegreeMatchScore(int dfg_node_id, int mrrg_node_id,
                                    const PlacementState& state) const {
  const int target_degree = annotation_.degree[dfg_node_id];
  const int freedom = ResourceFreedom(mrrg_node_id, state);
  return DegreePenalty(target_degree, freedom);
}

std::vector<int> YotoEngine::GetRawOutputTraversalRoots() {
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

std::vector<int> YotoEngine::GetOutputTraversalRoots() {
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

int YotoEngine::FindDFGEdgeId(int source, int target) const {
  for (const auto& edge : dfg_edges_) {
    if (edge.source == source && edge.target == target) return edge.id;
  }
  for (const auto& edge : dfg_edges_) {
    if (edge.source == target && edge.target == source) return edge.id;
  }
  return -1;
}

std::vector<int> YotoEngine::UnvisitedIncidentEdges(
    int dfg_node_id, const std::vector<char>& visited_edges) const {
  std::vector<int> result;
  for (int edge_id : incident_edge_ids_[dfg_node_id]) {
    if (!visited_edges[edge_id]) result.push_back(edge_id);
  }
  return result;
}

int YotoEngine::PickTraversalEdge(int dfg_node_id,
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

std::vector<int> YotoEngine::BuildCriticalPathScore() const {
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

int YotoEngine::ChoosePlacement2DNeighbor(
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

std::vector<int> YotoEngine::BuildASAPContextLevels() const {
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

void YotoEngine::PushPlacementCandidate(
    std::vector<PlacementState>& result,
    const std::optional<PlacementState>& placement) {
  if (!placement.has_value()) return;
  auto finalized = FinalizePhysicalPlacementIfNeeded(*placement);
  if (finalized.has_value()) result.push_back(*finalized);
}

int YotoEngine::SeedCount() const {
  if (!options_.seed_count.has_value()) return 1;
  return std::max(1, options_.seed_count.value());
}

int YotoEngine::MaxTrials() const {
  const int default_trials = std::max(100, dfg_.GetNodeNum() * 4);
  if (!options_.max_trials.has_value()) return default_trials;
  return std::max(1, options_.max_trials.value());
}

int YotoEngine::RoutingRetryCount() const {
  if (!options_.routing_retry_count.has_value()) return 4;
  return std::max(1, options_.routing_retry_count.value());
}

int YotoEngine::MaxIterations() const {
  const int default_iterations =
      std::max(5000, dfg_.GetNodeNum() * mrrg_.GetNodeNum() * 30);
  if (!options_.max_iterations.has_value()) return default_iterations;
  return std::max(1, options_.max_iterations.value());
}

unsigned int YotoEngine::SeedFor(int seed_index) const {
  const unsigned int configured_seed =
      options_.random_seed.has_value()
          ? static_cast<unsigned int>(options_.random_seed.value())
          : base_seed_;
  return configured_seed + static_cast<unsigned int>(0x9E3779B9u * seed_index);
}

void YotoEngine::ResetSeed(int seed_index) {
  const auto seed = SeedFor(seed_index);
  rng_.seed(seed);
  if (grid_placement_) std::srand(seed);
}

void YotoEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

double YotoEngine::ResourcePairPlacementCost(int from_mrrg_node,
                                             int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return 1.0e8;
  const int distance = DirectedResourceDistance(from_mrrg_node, to_mrrg_node);
  if (distance >= (1 << 28)) return 1.0e8;
  return DirectedDistanceCost(distance);
}

double YotoEngine::PlacementCost(const PlacementState& state) const {
  double cost = 0.0;
  for (const auto& edge : dfg_edges_) {
    const double edge_cost = EdgePlacementCost(edge, state.dfg_to_mrrg);
    if (edge_cost >= 1.0e8) return 1.0e8;
    cost += edge_cost;
  }
  return cost;
}

void YotoEngine::BuildMRRGCache() {
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

bool YotoEngine::HasTimedOut(std::chrono::steady_clock::time_point start,
                             double reserve_s) const {
  if (reserve_s < 0.0) reserve_s = grid_placement_ ? 0.01 : 0.02;

  return SecondsSince(start) + reserve_s >= timeout_s_;
}

bool YotoEngine::IsCompatible(int dfg_node_id, int mrrg_node_id) const {
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
  return SupportsOperation(mrrg_.GetNodeProperty(mrrg_node_id), op);
}

bool YotoEngine::SamePhysicalPosition(int lhs_mrrg_node,
                                      int rhs_mrrg_node) const {
  return mrrg_.GetNodeProperty(lhs_mrrg_node).position_id ==
         mrrg_.GetNodeProperty(rhs_mrrg_node).position_id;
}

bool YotoEngine::IsPhysicalPositionOccupied(int mrrg_node_id,
                                            const PlacementState& state) const {
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (state.mrrg_to_dfg[r] == -1) continue;
    if (SamePhysicalPosition(mrrg_node_id, r)) return true;
  }
  return false;
}

std::vector<int> YotoEngine::CompatibleResources(
    int dfg_node_id, const PlacementState& state) const {
  std::vector<int> result;
  result.reserve(mrrg_.GetNodeNum());
  for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
    if (!CanOccupyResource(dfg_node_id, r, state)) continue;
    result.push_back(r);
  }
  return result;
}

bool YotoEngine::IsIONode(int dfg_node_id) const {
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
  return entity::IsMemoryAccessOperation(op);
}

bool YotoEngine::SupportsAnyDFGOperation(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  for (entity::OpType op : property.supported_operations) {
    if (entity::IsDFGOp(op) && op != entity::OpType::kRoute &&
        op != entity::OpType::kNop) {
      return true;
    }
  }
  return false;
}

int YotoEngine::ResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  return std::min(DirectedResourceDistance(from_mrrg_node, to_mrrg_node),
                  DirectedResourceDistance(to_mrrg_node, from_mrrg_node));
}

int YotoEngine::DirectedResourceDistance(int from_mrrg_node,
                                         int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  if (!distance_.empty()) return distance_[from_mrrg_node][to_mrrg_node];
  return ApproximateDirectedResourceDistance(from_mrrg_node, to_mrrg_node);
}

int YotoEngine::SpatialStepDistance(int from_mrrg_node,
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

int YotoEngine::ApproximateDirectedResourceDistance(int from_mrrg_node,
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

int YotoEngine::PhysicalDistance(int from_mrrg_node, int to_mrrg_node) const {
  if (from_mrrg_node < 0 || to_mrrg_node < 0) return (1 << 28);
  return std::max(1, SpatialStepDistance(from_mrrg_node, to_mrrg_node));
}

int YotoEngine::BorderDistance(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  const int row = property.position_id.first;
  const int column = property.position_id.second;
  const auto config = mrrg_.GetMRRGConfig();
  return std::min(std::min(row, config.row - 1 - row),
                  std::min(column, config.column - 1 - column));
}

bool YotoEngine::IsCornerResource(int mrrg_node_id) const {
  const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
  const auto config = mrrg_.GetMRRGConfig();
  const bool row_on_border = property.position_id.first == 0 ||
                             property.position_id.first == config.row - 1;
  const bool column_on_border =
      property.position_id.second == 0 ||
      property.position_id.second == config.column - 1;
  return row_on_border && column_on_border;
}

double YotoEngine::IOPlacementScore(int dfg_node_id, int mrrg_node_id) const {
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

double YotoEngine::Placement2DInitialPlacementScore(int dfg_node_id,
                                                    int mrrg_node_id) const {
  if (IsIONode(dfg_node_id)) {
    double score = IOPlacementScore(dfg_node_id, mrrg_node_id);
    if (IsCornerResource(mrrg_node_id)) score += 4.0;
    return score;
  }
  double score = 0.0;
  if (BorderDistance(mrrg_node_id) == 0) score += 50.0;
  if (UsesSeparatePlacement2DIOCells() &&
      mrrg_.GetNodeProperty(mrrg_node_id).is_memory_accessible) {
    score += 100.0;
  }
  return score;
}

int YotoEngine::ManhattanRouteScore(int from_mrrg_node,
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

void YotoEngine::GridBuildDFGCache() {
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

void YotoEngine::GridBuildGridCache() {
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

void YotoEngine::GridBuildCompatibilityCache() {
  compatible_cells_.assign(dfg_.GetNodeNum(), {});
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    auto& cells = compatible_cells_[node];
    cells.reserve(cell_to_mrrg_.size());
    for (int cell = 0; cell < static_cast<int>(cell_to_mrrg_.size()); cell++) {
      if (GridIsCompatible(node, cell)) cells.push_back(cell);
    }
  }
}

std::string YotoEngine::GridNormalizePolicy(std::string policy) {
  std::transform(
      policy.begin(), policy.end(), policy.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return policy;
}

int YotoEngine::GridCell(int row, int col) const { return row * cols_ + col; }

int YotoEngine::GridRow(int cell) const { return cell / cols_; }

int YotoEngine::GridCol(int cell) const { return cell % cols_; }

bool YotoEngine::GridIsCompatible(int dfg_node, int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) return false;
  if (cell_to_mrrg_[cell] < 0) return false;
  if (GridUsesStructuralIOCellTypes()) {
    return true;
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return std::find(cell_ops_[cell].begin(), cell_ops_[cell].end(), op) !=
         cell_ops_[cell].end();
}

bool YotoEngine::GridIsIONode(int dfg_node) const {
  if (io_node_policy_ == "structural") {
    return predecessors_[dfg_node].empty() || successors_[dfg_node].empty();
  }
  const entity::OpType op = dfg_.GetNodeProperty(dfg_node).op;
  return entity::IsMemoryAccessOperation(op);
}

bool YotoEngine::GridCanPlace(int dfg_node, int cell,
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

int YotoEngine::GridCPUMappingCellType(int cell) const {
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

bool YotoEngine::GridIsCPUMappingCompatible(int dfg_node, int cell) const {
  if (!GridIsCompatible(dfg_node, cell)) return false;
  const int cell_type = GridCPUMappingCellType(cell);
  if (cell_type < 0) return false;
  return cell_type == (GridIsIONode(dfg_node) ? 1 : 0);
}

bool YotoEngine::GridCanPlaceCPUMapping(int dfg_node, int cell,
                                        const PlacementState& state) const {
  return cell >= 0 && cell < static_cast<int>(state.cell_to_dfg.size()) &&
         state.cell_to_dfg[cell] < 0 &&
         GridIsCPUMappingCompatible(dfg_node, cell);
}

int YotoEngine::GridCellFreedom(int cell) const {
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

std::vector<int> YotoEngine::GridInitialFreedomGrid() const {
  return initial_freedom_;
}

int YotoEngine::GridDistanceCost(int a, int b) const {
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

std::string YotoEngine::GridMapperName() const {
  switch (kind_) {
    case mapper::yoto::Placement2DArrayKind::kPaperGuidedYOTO:
      return "Placement2DPaperGuidedArrayYOTO";
    case mapper::yoto::Placement2DArrayKind::kCPUMappingYOTO:
      return "Placement2DCPUMappingYOTO";
  }
  return "Placement2DArray";
}

bool YotoEngine::GridIsCPUMappingLike() const {
  return kind_ == mapper::yoto::Placement2DArrayKind::kCPUMappingYOTO ||
         false || false || false;
}

bool YotoEngine::GridIsPaperGuidedTraversalLike() const {
  return kind_ == mapper::yoto::Placement2DArrayKind::kPaperGuidedYOTO || false;
}

bool YotoEngine::GridIsPaperGuidedYOTO() const {
  return kind_ == mapper::yoto::Placement2DArrayKind::kPaperGuidedYOTO;
}

bool YotoEngine::GridUsesYOTTAnnotations() const {
  if (use_yott_annotations_.has_value()) {
    return use_yott_annotations_.value();
  }
  return false || false;
}

bool YotoEngine::GridUsesStructuralIOCellTypes() const {
  return io_node_policy_ == "structural" &&
         (GridIsCPUMappingLike() || GridIsPaperGuidedTraversalLike());
}

unsigned int YotoEngine::GridTrialSeedFor(int seed_index,
                                          int trial_index) const {
  unsigned int seed = SeedFor(seed_index);
  seed ^= static_cast<unsigned int>(trial_index + 1) * 0x85EBCA6Bu;
  seed ^= seed >> 16;
  seed *= 0x7FEB352Du;
  seed ^= seed >> 15;
  return seed;
}

void YotoEngine::GridResetTrialSeed(int seed_index, int trial_index) {
  const unsigned int seed = GridTrialSeedFor(seed_index, trial_index);
  rng_.seed(seed);
  std::srand(seed);
}

bool YotoEngine::GridUsesPerTrialSeeds() const {
  return trial_seed_policy_ == "per_trial";
}

int YotoEngine::GridAuthorRandomInt(int exclusive_upper_bound) {
  if (exclusive_upper_bound <= 1) return 0;
  return std::rand() % exclusive_upper_bound;
}

void YotoEngine::GridShuffleWithAuthorRandom(std::vector<int>& values) {
  for (int i = 1; i < static_cast<int>(values.size()); i++) {
    std::swap(values[i], values[GridAuthorRandomInt(i + 1)]);
  }
}

void YotoEngine::GridShuffleWithAuthorRandom(
    std::array<std::pair<int, int>, 4>& values) {
  for (int i = 1; i < static_cast<int>(values.size()); i++) {
    std::swap(values[i], values[GridAuthorRandomInt(i + 1)]);
  }
}

int YotoEngine::GridTraversalNeighborMode(bool is_yott) {
  if (traversal_neighbor_policy_ == "degree") return 0;
  if (traversal_neighbor_policy_ == "betweenness") return 1;
  if (traversal_neighbor_policy_ == "critical_path") return 2;
  if (traversal_neighbor_policy_ == "zigzag") return 3;
  if (traversal_neighbor_policy_ == "random") return 4;
  if (traversal_neighbor_policy_ != "default") {
    Log("unknown traversal_neighbor_policy=" + traversal_neighbor_policy_ +
        "; using default");
  }
  if (!is_yott) return 0;
  if (false) return GridAuthorRandomInt(4);
  std::uniform_int_distribution<int> mode_dist(0, 3);
  return mode_dist(rng_);
}

bool YotoEngine::GridTraceTrials() const {
  return trace_trials_.value_or(false) && log_file_path_.has_value();
}

std::string YotoEngine::GridTrialTracePath() const {
  if (!log_file_path_.has_value()) return "trial_trace.csv";
  const std::string path = log_file_path_.value();
  const std::string::size_type slash = path.find_last_of("/\\");
  if (slash == std::string::npos) return "trial_trace.csv";
  return path.substr(0, slash + 1) + "trial_trace.csv";
}

void YotoEngine::GridWriteTrialTraceHeader(std::ofstream& trace) const {
  trace << "mapper,phase,seed_index,trial_index,success,selected_as_best,"
           "selection_primary,selection_secondary,placement_cost,"
           "mesh_hop_sum,avg_mesh_hop,max_mesh_hop,"
           "mesh_optimal_edge_ratio,paper_optimal_edge_ratio,"
           "avg_mesh_fifo,max_mesh_fifo,p95_mesh_fifo,"
           "avg_paper_fifo,max_paper_fifo,elapsed_sec,trial_seed,"
           "plan_hash,placement_hash,placement_signature\n";
}

TrialTraceMetrics YotoEngine::GridComputeTrialTraceMetrics(
    const PlacementState& state) const {
  TrialTraceMetrics metrics;
  metrics.placement_cost = 0.0;

  std::vector<int> mesh_fifo_values;
  mesh_fifo_values.reserve(edges_.size());
  int mesh_optimal_edges = 0;
  int paper_optimal_edges = 0;
  int paper_fifo_sum = 0;

  for (const auto& edge : edges_) {
    const int source_cell = state.dfg_to_cell[edge.source];
    const int target_cell = state.dfg_to_cell[edge.target];
    if (source_cell < 0 || target_cell < 0) {
      metrics.placement_cost = kGridImpossibleCost;
      return metrics;
    }

    const int dx = std::abs(GridRow(source_cell) - GridRow(target_cell));
    const int dy = std::abs(GridCol(source_cell) - GridCol(target_cell));
    const int mesh_hop = dx + dy;
    const int placement_cost = GridDistanceCost(source_cell, target_cell);
    const int mesh_fifo = std::max(0, mesh_hop - 1);
    const int paper_fifo = std::max(0, placement_cost - 1);

    metrics.placement_cost += placement_cost;
    metrics.mesh_hop_sum += mesh_hop;
    metrics.max_mesh_hop = std::max(metrics.max_mesh_hop, mesh_hop);
    metrics.max_mesh_fifo = std::max(metrics.max_mesh_fifo, mesh_fifo);
    metrics.max_paper_fifo = std::max(metrics.max_paper_fifo, paper_fifo);
    mesh_fifo_values.push_back(mesh_fifo);
    paper_fifo_sum += paper_fifo;
    if (mesh_hop <= 1) mesh_optimal_edges++;
    if (placement_cost <= 1) paper_optimal_edges++;
  }

  const int edge_count = static_cast<int>(edges_.size());
  if (edge_count > 0) {
    metrics.avg_mesh_hop = metrics.mesh_hop_sum / edge_count;
    metrics.mesh_optimal_edge_ratio =
        static_cast<double>(mesh_optimal_edges) / edge_count;
    metrics.paper_optimal_edge_ratio =
        static_cast<double>(paper_optimal_edges) / edge_count;
    metrics.avg_mesh_fifo =
        std::accumulate(mesh_fifo_values.begin(), mesh_fifo_values.end(), 0.0) /
        edge_count;
    metrics.p95_mesh_fifo = Percentile(mesh_fifo_values, 95.0);
    metrics.avg_paper_fifo = static_cast<double>(paper_fifo_sum) / edge_count;
  }
  return metrics;
}

std::string YotoEngine::GridPlacementSignature(
    const PlacementState& state) const {
  std::ostringstream out;
  for (int node = 0; node < static_cast<int>(state.dfg_to_cell.size());
       node++) {
    if (node > 0) out << ':';
    out << state.dfg_to_cell[node];
  }
  return out.str();
}

std::string YotoEngine::GridPlacementHash(const PlacementState& state) const {
  std::uint64_t hash = 1469598103934665603ULL;
  for (int cell : state.dfg_to_cell) {
    hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell + 1));
    hash *= 1099511628211ULL;
  }
  std::ostringstream out;
  out << std::hex << hash;
  return out.str();
}

std::string YotoEngine::GridPlanHash(const std::vector<Step>& plan) const {
  std::uint64_t hash = 1469598103934665603ULL;
  const auto append = [&hash](int value) {
    hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(value + 1));
    hash *= 1099511628211ULL;
  };
  for (const auto& step : plan) {
    append(step.anchor);
    append(step.target);
    append(step.edge_id);
    append(static_cast<int>(step.annotations.size()));
    for (const auto& annotation : step.annotations) {
      append(static_cast<int>(annotation.kind));
      append(annotation.anchor_node);
      append(annotation.distance);
    }
  }
  std::ostringstream out;
  out << std::hex << hash;
  return out.str();
}

void YotoEngine::GridWriteTrialTraceRow(
    std::ofstream& trace, const std::string& phase, int seed_index,
    int trial_index, bool success, bool selected_as_best,
    double selection_primary, double selection_secondary,
    const std::optional<PlacementState>& state, double elapsed_s,
    const std::string& plan_hash) const {
  const TrialTraceMetrics metrics = state.has_value()
                                        ? GridComputeTrialTraceMetrics(*state)
                                        : TrialTraceMetrics{};

  trace << GridMapperName() << "," << phase << "," << seed_index << ","
        << trial_index << "," << (success ? 1 : 0) << ","
        << (selected_as_best ? 1 : 0) << ",";
  WriteCost(trace, selection_primary);
  trace << ",";
  WriteCost(trace, selection_secondary);
  trace << ",";
  WriteCost(trace, metrics.placement_cost);
  trace << "," << metrics.mesh_hop_sum << "," << metrics.avg_mesh_hop << ","
        << metrics.max_mesh_hop << "," << metrics.mesh_optimal_edge_ratio << ","
        << metrics.paper_optimal_edge_ratio << "," << metrics.avg_mesh_fifo
        << "," << metrics.max_mesh_fifo << "," << metrics.p95_mesh_fifo << ","
        << metrics.avg_paper_fifo << "," << metrics.max_paper_fifo << ","
        << elapsed_s << ",";
  if (GridUsesPerTrialSeeds() && seed_index >= 0 && trial_index >= 0) {
    trace << GridTrialSeedFor(seed_index, trial_index);
  }
  trace << "," << plan_hash << ",";
  if (state.has_value()) {
    trace << GridPlacementHash(*state) << "," << GridPlacementSignature(*state);
  } else {
    trace << ",";
  }
  trace << "\n";
}

void YotoEngine::GridRecordPlacementSwapAttempts(long long count) const {
  if (count > 0) placement_swap_attempts_ += count;
}

std::string YotoEngine::GridNodeLabel(int node) const {
  if (node < 0 || node >= dfg_.GetNodeNum()) return std::to_string(node);
  const auto property = dfg_.GetNodeProperty(node);
  return std::to_string(node) + ":" + property.op_name + "/" + property.op_str;
}

int YotoEngine::GridPlacedCount(const PlacementState& state) const {
  int count = 0;
  for (int cell : state.dfg_to_cell) {
    if (cell >= 0) count++;
  }
  return count;
}

int YotoEngine::GridFreeCPUMappingCompatibleCellCount(
    int dfg_node, const PlacementState& state) const {
  int count = 0;
  for (int cell : compatible_cells_[dfg_node]) {
    if (GridCanPlaceCPUMapping(dfg_node, cell, state)) count++;
  }
  return count;
}

void YotoEngine::GridRecordFailure(const std::string& reason) {
  if (last_failure_reason_.empty()) last_failure_reason_ = reason;
}

YotoEngine::YotoEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                       mapper::yoto::Placement2DArrayKind kind,
                       const mapper::yoto::Placement2DArrayOptions& options)
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

auto YotoEngine::GridStartSearch() -> std::chrono::steady_clock::time_point {
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

std::optional<PlacementState> YotoEngine::GridSearchPlacement(
    std::chrono::steady_clock::time_point start) {
  return GridIsCPUMappingLike() ? GridRunCPUMappingMultiStart(start)
                                : GridRunPaperGuidedTraversalMultiStart(start);
}

mapper::MappingResult YotoEngine::GridFailure(
    std::chrono::steady_clock::time_point start) const {
  Log("failure mapper=" + GridMapperName() + " reason=" +
      (last_failure_reason_.empty() ? "no placement candidate"
                                    : last_failure_reason_));
  return mapper::MappingResult(false, entity::Mapping(config_),
                               SecondsSince(start));
}

mapper::MappingResult YotoEngine::GridFinishPlacement(
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

}  // namespace mapper::yoto
