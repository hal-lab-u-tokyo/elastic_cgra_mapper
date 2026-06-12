#include <mapper/detail/placement_search_engine.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr int kInfDistance = 1 << 28;
constexpr double kImpossibleCost = 1.0e8;

struct DFGEdgeInfo {
  int id = -1;
  int source = -1;
  int target = -1;
};

struct PathResult {
  std::vector<int> edge_ids;
  std::vector<int> node_ids;
};

struct RouteUsage {
  std::vector<int> route_node_owner;
  std::vector<int> route_edge_owner;
  std::vector<std::vector<int>> source_to_route_edges;
};

struct PlacementState {
  std::vector<int> dfg_to_mrrg;
  std::vector<int> mrrg_to_dfg;
};

enum class SequenceAnnotationKind { kIO, kReconvergence };

struct SequenceAnnotation {
  SequenceAnnotationKind kind = SequenceAnnotationKind::kReconvergence;
  int anchor_node = -1;
  int distance = 1;
};

struct TraversalStep {
  int placed_node = -1;
  int target_node = -1;
  int edge_id = -1;
  std::vector<SequenceAnnotation> annotations;
};

struct TraversalPlan {
  std::vector<int> roots;
  std::vector<TraversalStep> sequence;
};

struct Annotation {
  std::vector<int> in_degree;
  std::vector<int> out_degree;
  std::vector<int> degree;
};

struct CandidateRank {
  int annotation_priority = 0;
  double annotation_cost = 0.0;
  int locality_distance = 0;
  double degree_cost = 0.0;
  int random_order = 0;
};

using mapper::detail::PlacementSearchKind;
using mapper::detail::PlacementSearchOptions;

long long EdgeKey(int from, int to) {
  return (static_cast<long long>(from) << 32) ^
         static_cast<unsigned int>(to);
}

double SecondsSince(std::chrono::steady_clock::time_point start) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now - start)
      .count();
}

bool SupportsOperation(const entity::MRRGNodeProperty& node_property,
                       entity::OpType op) {
  return std::find(node_property.supported_operations.begin(),
                   node_property.supported_operations.end(),
                   op) != node_property.supported_operations.end();
}

int DirectedDistanceCost(int distance) {
  return distance >= kInfDistance ? kInfDistance : std::max(1, distance);
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
    if (edge.target >= 0 && edge.target < node_num && edge.target != edge.source) {
      incident_edge_ids[edge.target].push_back(edge.id);
    }
  }
  return incident_edge_ids;
}

class PlacementSearchEngine {
 public:
  PlacementSearchEngine(entity::DFG& dfg, entity::MRRG& mrrg,
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
            0x9E3779B9u ^
            static_cast<unsigned int>(dfg.GetNodeNum() * 131 +
                                      mrrg.GetNodeNum()))),
        rng_(base_seed_) {
    ResetSeed(0);
    dfg_edges_ = BuildDFGEdges(dfg_);
    incident_edge_ids_ = BuildIncidentEdgeIds(dfg_.GetNodeNum(), dfg_edges_);
    successors_ = BuildSuccessors(dfg_);
    predecessors_ = BuildPredecessors(dfg_);
    BuildMRRGCache();
    BuildAllPairsDistances();
    annotation_ = BuildAnnotation();
  }

  mapper::MappingResult Run() {
    const auto start = std::chrono::steady_clock::now();
    Log("start mapper=" + MapperName() + " dfg_nodes=" +
        std::to_string(dfg_.GetNodeNum()) + " mrrg_nodes=" +
        std::to_string(mrrg_.GetNodeNum()) + " max_trials=" +
        std::to_string(MaxTrials()) + " seed_count=" +
        std::to_string(SeedCount()) + " routing_retry_count=" +
        std::to_string(RoutingRetryCount()) + " max_iterations=" +
        std::to_string(MaxIterations()));

    std::optional<PlacementState> placement;
    if (IsSALike()) {
      placement = RunSAMultiSeed(start);
    } else {
      placement = RunGreedyMultiStart(start);
    }

    if (!placement.has_value()) {
      return MakeFailureResult(start);
    }

    if (options_.placement_only) {
      const auto mapping = MakePlacementOnlyMapping(*placement);
      const double mapping_time_s = SecondsSince(start);
      Log("placement_only_success mapper=" + MapperName() + " time_s=" +
          std::to_string(mapping_time_s) + " placement_cost=" +
          std::to_string(PlacementCost(*placement)));
      return mapper::MappingResult(true, mapping, mapping_time_s);
    }

    std::optional<RouteUsage> route_usage = TryRoutePlacement(*placement);
    if (!route_usage.has_value()) {
      return MakeFailureResult(start);
    }

    for (auto& edges : route_usage->source_to_route_edges) {
      std::sort(edges.begin(), edges.end());
      edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    }

    const auto mapping = entity::GenerateMappingFromRoutingResult(
        mrrg_, dfg_, placement->dfg_to_mrrg,
        route_usage->source_to_route_edges);
    const double mapping_time_s = SecondsSince(start);
    Log("success mapper=" + MapperName() + " time_s=" +
        std::to_string(mapping_time_s) + " placement_cost=" +
        std::to_string(PlacementCost(*placement)) + " route_edges=" +
        std::to_string(RouteEdgeCount(*route_usage)));
    return mapper::MappingResult(true, mapping, mapping_time_s);
  }

 private:
  entity::DFG& dfg_;
  entity::MRRG& mrrg_;
  PlacementSearchKind search_kind_;
  double timeout_s_;
  std::optional<std::string> log_file_path_;
  PlacementSearchOptions options_;
  unsigned int base_seed_;
  std::mt19937 rng_;

  std::vector<DFGEdgeInfo> dfg_edges_;
  std::vector<std::vector<int>> incident_edge_ids_;
  std::vector<std::vector<int>> successors_;
  std::vector<std::vector<int>> predecessors_;
  std::vector<std::vector<int>> distance_;
  std::vector<int> mrrg_in_degree_;
  std::vector<int> mrrg_out_degree_;
  std::vector<int> mrrg_degree_;
  std::unordered_map<long long, int> mrrg_edge_id_;
  Annotation annotation_;

  std::string MapperName() const {
    switch (search_kind_) {
      case PlacementSearchKind::kYOTO:
        return "YOTO";
      case PlacementSearchKind::kYOTT:
        return "YOTT";
      case PlacementSearchKind::kSA:
        return "SA";
      case PlacementSearchKind::kPlacement2DYOTO:
        return "Placement2DYOTO";
      case PlacementSearchKind::kPlacement2DYOTT:
        return "Placement2DYOTT";
      case PlacementSearchKind::kPlacement2DSA:
        return "Placement2DSA";
    }
    return "unknown";
  }

  bool IsSALike() const {
    return search_kind_ == PlacementSearchKind::kSA ||
           search_kind_ == PlacementSearchKind::kPlacement2DSA;
  }

  bool IsYOTTLike() const {
    return search_kind_ == PlacementSearchKind::kYOTT ||
           search_kind_ == PlacementSearchKind::kPlacement2DYOTT;
  }

  bool IsPlacement2DTraversalLike() const {
    return search_kind_ == PlacementSearchKind::kPlacement2DYOTO ||
           search_kind_ == PlacementSearchKind::kPlacement2DYOTT;
  }

  bool UsesPhysicalPeExclusivePlacement() const {
    return IsPlacement2DTraversalLike() ||
           search_kind_ == PlacementSearchKind::kPlacement2DSA;
  }

  int SeedCount() const {
    if (!options_.seed_count.has_value()) return 1;
    return std::max(1, options_.seed_count.value());
  }

  int MaxTrials() const {
    const int default_trials = std::max(100, dfg_.GetNodeNum() * 4);
    if (!options_.max_trials.has_value()) return default_trials;
    return std::max(1, options_.max_trials.value());
  }

  int RoutingRetryCount() const {
    if (!options_.routing_retry_count.has_value()) return 4;
    return std::max(1, options_.routing_retry_count.value());
  }

  int MaxIterations() const {
    const int default_iterations =
        std::max(5000, dfg_.GetNodeNum() * mrrg_.GetNodeNum() * 30);
    if (!options_.max_iterations.has_value()) return default_iterations;
    return std::max(1, options_.max_iterations.value());
  }

  unsigned int SeedFor(int seed_index) const {
    const unsigned int configured_seed =
        options_.random_seed.has_value()
            ? static_cast<unsigned int>(options_.random_seed.value())
            : base_seed_;
    return configured_seed +
           static_cast<unsigned int>(0x9E3779B9u * seed_index);
  }

  void ResetSeed(int seed_index) { rng_.seed(SeedFor(seed_index)); }

  void Log(const std::string& message) const {
    if (!log_file_path_.has_value()) return;
    std::ofstream ofs(log_file_path_.value(), std::ios::app);
    if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
  }

  mapper::MappingResult MakeFailureResult(
      std::chrono::steady_clock::time_point start) const {
    const double mapping_time_s = SecondsSince(start);
    Log("failure mapper=" + MapperName() + " time_s=" +
        std::to_string(mapping_time_s));
    return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                                 mapping_time_s);
  }

  entity::Mapping MakePlacementOnlyMapping(const PlacementState& placement) const {
    // Keep direct neighbor connections in the emitted mapping, but do not insert
    // route nodes. Paper-style placement evaluation can then read operation
    // locations and direct-edge quality without requiring full routing success.
    std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
    return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg,
                           empty_route_edges);
  }

  void BuildMRRGCache() {
    const int node_num = mrrg_.GetNodeNum();
    mrrg_in_degree_.assign(node_num, 0);
    mrrg_out_degree_.assign(node_num, 0);
    for (int u = 0; u < node_num; u++) {
      for (int edge_id : mrrg_.GetOutEdgeIdVec(u)) {
        const auto edge = mrrg_.GetEdgeSourceTarget(edge_id);
        const int from = edge.first;
        const int to = edge.second;
        mrrg_edge_id_[EdgeKey(from, to)] = edge_id;
        mrrg_out_degree_[from]++;
        mrrg_in_degree_[to]++;
      }
    }
    mrrg_degree_.resize(node_num, 0);
    for (int i = 0; i < node_num; i++) {
      mrrg_degree_[i] = mrrg_in_degree_[i] + mrrg_out_degree_[i];
    }
  }

  void BuildAllPairsDistances() {
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

  Annotation BuildAnnotation() {
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

  bool HasTimedOut(std::chrono::steady_clock::time_point start,
                   double reserve_s = 0.02) const {
    return SecondsSince(start) + reserve_s >= timeout_s_;
  }

  bool IsCompatible(int dfg_node_id, int mrrg_node_id) const {
    const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
    return SupportsOperation(mrrg_.GetNodeProperty(mrrg_node_id), op);
  }

  bool SamePhysicalPosition(int lhs_mrrg_node, int rhs_mrrg_node) const {
    return mrrg_.GetNodeProperty(lhs_mrrg_node).position_id ==
           mrrg_.GetNodeProperty(rhs_mrrg_node).position_id;
  }

  bool IsPhysicalPositionOccupied(int mrrg_node_id,
                                  const PlacementState& state) const {
    for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
      if (state.mrrg_to_dfg[r] == -1) continue;
      if (SamePhysicalPosition(mrrg_node_id, r)) return true;
    }
    return false;
  }

  bool CanOccupyResource(int dfg_node_id, int mrrg_node_id,
                         const PlacementState& state) const {
    if (state.mrrg_to_dfg[mrrg_node_id] != -1) return false;
    if (!IsCompatible(dfg_node_id, mrrg_node_id)) return false;
    // The public cpu_mapping implementation places onto a 2D grid cell, not
    // onto multiple modulo contexts of the same physical PE. Keep that
    // behavior for the 2D-placement variants.
    if (UsesPhysicalPeExclusivePlacement() &&
        IsPhysicalPositionOccupied(mrrg_node_id, state)) {
      return false;
    }
    return true;
  }

  std::vector<int> CompatibleResources(int dfg_node_id,
                                       const PlacementState& state) const {
    std::vector<int> result;
    result.reserve(mrrg_.GetNodeNum());
    for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
      if (!CanOccupyResource(dfg_node_id, r, state)) continue;
      result.push_back(r);
    }
    return result;
  }

  std::vector<int> ClosestCompatibleResources(int dfg_node_id,
                                              int anchor_mrrg_node,
                                              const PlacementState& state) const {
    if (anchor_mrrg_node < 0) return CompatibleResources(dfg_node_id, state);
    std::vector<int> result;
    int best_distance = kInfDistance;
    for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
      if (!CanOccupyResource(dfg_node_id, r, state)) continue;
      const int distance = IsPlacement2DTraversalLike()
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

  bool IsIONode(int dfg_node_id) const {
    const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
    if (op == entity::OpType::OUTPUT || op == entity::OpType::STORE) return true;
    return op == entity::OpType::LOAD && predecessors_[dfg_node_id].empty();
  }

  bool SupportsAnyDFGOperation(int mrrg_node_id) const {
    const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
    for (entity::OpType op : property.supported_operations) {
      if (entity::IsDFGOp(op) && op != entity::OpType::ROUTE &&
          op != entity::OpType::NOP) {
        return true;
      }
    }
    return false;
  }

  int ResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
    return std::min(distance_[from_mrrg_node][to_mrrg_node],
                    distance_[to_mrrg_node][from_mrrg_node]);
  }

  int PhysicalDistance(int from_mrrg_node, int to_mrrg_node) const {
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
    const auto from = mrrg_.GetNodeProperty(from_mrrg_node).position_id;
    const auto to = mrrg_.GetNodeProperty(to_mrrg_node).position_id;
    const int row_distance = std::abs(from.first - to.first);
    const int column_distance = std::abs(from.second - to.second);
    const auto config = mrrg_.GetMRRGConfig();
    if (config.network_type == entity::MRRGNetworkType::kOneHopAxis2) {
      return std::max(1, (row_distance + 1) / 2 +
                             (column_distance + 1) / 2);
    }
    if (config.network_type == entity::MRRGNetworkType::kDiagonal) {
      return std::max(1, std::max(row_distance, column_distance));
    }
    return std::max(1, row_distance + column_distance);
  }

  int ResourceFreedom(int mrrg_node_id, const PlacementState& state) const {
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

  int BorderDistance(int mrrg_node_id) const {
    const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
    const int row = property.position_id.first;
    const int column = property.position_id.second;
    const auto config = mrrg_.GetMRRGConfig();
    return std::min(std::min(row, config.row - 1 - row),
                    std::min(column, config.column - 1 - column));
  }

  double IOPlacementScore(int dfg_node_id, int mrrg_node_id) const {
    const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
    double score = BorderDistance(mrrg_node_id);
    if (entity::IsMemoryAccessOperation(dfg_.GetNodeProperty(dfg_node_id).op) ||
        dfg_.GetNodeProperty(dfg_node_id).op == entity::OpType::OUTPUT) {
      score += property.is_memory_accessible ? -8.0 : 40.0;
    } else {
      score += property.is_memory_accessible ? -1.0 : 0.0;
    }
    return score;
  }

  std::vector<int> GetRawOutputTraversalRoots() {
    std::vector<int> roots;
    for (int v = 0; v < dfg_.GetNodeNum(); v++) {
      if (successors_[v].empty() ||
          dfg_.GetNodeProperty(v).op == entity::OpType::OUTPUT) {
        roots.push_back(v);
      }
    }
    if (roots.empty()) {
      for (int v = 0; v < dfg_.GetNodeNum(); v++) roots.push_back(v);
    }
    std::shuffle(roots.begin(), roots.end(), rng_);
    return roots;
  }

  std::vector<int> GetOutputTraversalRoots() {
    std::vector<int> roots;
    for (int v = 0; v < dfg_.GetNodeNum(); v++) {
      if (successors_[v].empty() ||
          dfg_.GetNodeProperty(v).op == entity::OpType::OUTPUT) {
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

  int OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const {
    if (edge.source == node_id) return edge.target;
    if (edge.target == node_id) return edge.source;
    return -1;
  }

  int FindDFGEdgeId(int source, int target) const {
    for (const auto& edge : dfg_edges_) {
      if (edge.source == source && edge.target == target) return edge.id;
    }
    for (const auto& edge : dfg_edges_) {
      if (edge.source == target && edge.target == source) return edge.id;
    }
    return -1;
  }

  std::vector<int> UnvisitedIncidentEdges(
      int dfg_node_id, const std::vector<char>& visited_edges) const {
    std::vector<int> result;
    for (int edge_id : incident_edge_ids_[dfg_node_id]) {
      if (!visited_edges[edge_id]) result.push_back(edge_id);
    }
    return result;
  }

  int PickTraversalEdge(int dfg_node_id, const std::vector<char>& visited_nodes,
                        const std::vector<char>& visited_edges) {
    std::vector<int> candidates = UnvisitedIncidentEdges(dfg_node_id, visited_edges);
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

  int FindLastStepTargeting(const TraversalPlan& plan, int target_node) const {
    for (int i = static_cast<int>(plan.sequence.size()) - 1; i >= 0; i--) {
      if (plan.sequence[i].target_node == target_node) return i;
    }
    return -1;
  }

  void BackpropagateAnnotation(TraversalPlan& plan, int from_node,
                               int anchor_node,
                               SequenceAnnotationKind kind, int max_depth,
                               int initial_distance = 1) const {
    int current = from_node;
    int distance = initial_distance;
    while (distance <= max_depth) {
      int step_index = FindLastStepTargeting(plan, current);
      if (step_index < 0) return;
      plan.sequence[step_index].annotations.push_back(
          SequenceAnnotation{kind, anchor_node, distance});
      current = plan.sequence[step_index].placed_node;
      distance++;
    }
  }

  std::vector<int> BuildCriticalPathScore() const {
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

  int Placement2DCentralityScore(int dfg_node_id) const {
    return annotation_.degree[dfg_node_id] +
           annotation_.in_degree[dfg_node_id] * annotation_.out_degree[dfg_node_id];
  }

  int ChoosePlacement2DNeighbor(const std::vector<int>& candidates,
                           bool choose_from_fanout, int mode,
                           bool zigzag_take_back,
                           const std::vector<int>& critical_path) {
    if (candidates.empty()) return -1;
    if (candidates.size() == 1) return candidates.front();

    if (mode == 0) {
      return *std::max_element(candidates.begin(), candidates.end(),
                               [&](int a, int b) {
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
      return *std::max_element(candidates.begin(), candidates.end(),
                               [&](int a, int b) {
        return Placement2DCentralityScore(a) < Placement2DCentralityScore(b);
      });
    }
    if (mode == 2) {
      return *std::max_element(candidates.begin(), candidates.end(),
                               [&](int a, int b) {
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

  TraversalPlan BuildPlacement2DTraversalPlan(bool annotate) {
    const int kPlacement2DMaxAnnotationDistance = 2;
    TraversalPlan plan;
    std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
    std::vector<std::vector<int>> local_fanin = predecessors_;
    std::vector<std::vector<int>> local_fanout = successors_;
    std::vector<std::pair<int, int>> stack;
    for (int root : GetRawOutputTraversalRoots()) stack.push_back({root, 1});
    const auto critical_path = BuildCriticalPathScore();
    int selection_mode = 0;
    if (search_kind_ == PlacementSearchKind::kPlacement2DYOTT) {
      std::uniform_int_distribution<int> mode_dist(0, 3);
      selection_mode = mode_dist(rng_);
    }

    auto remove_value = [](std::vector<int>& values, int value) {
      values.erase(std::remove(values.begin(), values.end(), value),
                   values.end());
    };

    auto push_remaining = [&]() {
      for (int node = 0; node < dfg_.GetNodeNum(); node++) {
        if (!local_fanin[node].empty() || !local_fanout[node].empty()) {
          stack.push_back({node, 1});
          return true;
        }
      }
      for (int node = 0; node < dfg_.GetNodeNum(); node++) {
        if (!visited_nodes[node]) {
          stack.push_back({node, 1});
          return true;
        }
      }
      return false;
    };

    auto append_step = [&](int from, int to) {
      if (from < 0 || to < 0 || from == to) return;
      TraversalStep step{from, to, FindDFGEdgeId(from, to), {}};
      if (annotate && IsIONode(to)) {
        step.annotations.push_back(
            SequenceAnnotation{SequenceAnnotationKind::kIO, to, 1});
        BackpropagateAnnotation(plan, from, to, SequenceAnnotationKind::kIO,
                                kPlacement2DMaxAnnotationDistance, 2);
      }
      if (annotate && visited_nodes[to]) {
        BackpropagateAnnotation(plan, from, to,
                                SequenceAnnotationKind::kReconvergence,
                                kPlacement2DMaxAnnotationDistance, 1);
      }
      plan.sequence.push_back(step);
    };

    if (stack.empty()) push_remaining();
    while (!stack.empty()) {
      int a = stack.back().first;
      int direction = stack.back().second;
      stack.pop_back();
      if (a < 0 || a >= dfg_.GetNodeNum()) continue;
      if (!visited_nodes[a]) plan.roots.push_back(a);

      const int fanin = static_cast<int>(local_fanin[a].size());
      const int fanout = static_cast<int>(local_fanout[a].size());
      int b = -1;
      if (direction == 1) {
        if (fanout >= 1) {
          b = ChoosePlacement2DNeighbor(local_fanout[a], true, selection_mode,
                                   true, critical_path);
          for (int i = 0; i < fanin; i++) stack.push_back({a, 1});
          stack.push_back({b, 0});
          remove_value(local_fanout[a], b);
          remove_value(local_fanin[b], a);
          append_step(a, b);
        } else if (fanin >= 1) {
          b = ChoosePlacement2DNeighbor(local_fanin[a], false, selection_mode,
                                   true, critical_path);
          stack.push_back({a, 1});
          for (int i = 0; i < fanin; i++) stack.push_back({b, 1});
          remove_value(local_fanin[a], b);
          remove_value(local_fanout[b], a);
          append_step(a, b);
        }
      } else {
        if (fanin >= 1) {
          b = ChoosePlacement2DNeighbor(local_fanin[a], false, selection_mode,
                                   false, critical_path);
          for (int i = 0; i < fanout; i++) stack.push_back({a, 0});
          stack.push_back({b, 1});
          remove_value(local_fanin[a], b);
          remove_value(local_fanout[b], a);
          append_step(a, b);
        } else if (fanout >= 1) {
          b = ChoosePlacement2DNeighbor(local_fanout[a], true, selection_mode,
                                   false, critical_path);
          stack.push_back({a, 0});
          for (int i = 0; i < fanout; i++) stack.push_back({b, 0});
          remove_value(local_fanout[a], b);
          remove_value(local_fanin[b], a);
          append_step(a, b);
        }
      }
      visited_nodes[a] = true;
      if (stack.empty()) push_remaining();
    }

    return plan;
  }

  TraversalPlan BuildTraversalPlan(bool annotate) {
    constexpr int kReconvergenceDepth = 3;
    constexpr int kIODepth = 3;
    TraversalPlan plan;
    std::vector<char> visited_nodes(dfg_.GetNodeNum(), false);
    std::vector<char> visited_edges(dfg_edges_.size(), false);
    std::vector<int> stack = GetOutputTraversalRoots();

    auto push_remaining_component_root = [&]() {
      for (int v = 0; v < dfg_.GetNodeNum(); v++) {
        if (!visited_nodes[v]) {
          stack.push_back(v);
          return true;
        }
      }
      return false;
    };

    if (stack.empty()) push_remaining_component_root();

    while (!stack.empty()) {
      int placed_node = stack.back();
      stack.pop_back();
      if (!visited_nodes[placed_node]) {
        visited_nodes[placed_node] = true;
        plan.roots.push_back(placed_node);
      }

      while (true) {
        std::vector<int> unvisited_edges =
            UnvisitedIncidentEdges(placed_node, visited_edges);
        if (unvisited_edges.empty()) break;
        if (unvisited_edges.size() > 1) stack.push_back(placed_node);

        const int edge_id =
            PickTraversalEdge(placed_node, visited_nodes, visited_edges);
        if (edge_id < 0) break;
        visited_edges[edge_id] = true;
        const int target_node = OtherEndpoint(dfg_edges_[edge_id], placed_node);
        if (target_node < 0 || target_node == placed_node) continue;

        TraversalStep pending_step{placed_node, target_node, edge_id, {}};
        if (annotate && IsIONode(target_node)) {
          pending_step.annotations.push_back(SequenceAnnotation{
              SequenceAnnotationKind::kIO, target_node, 1});
          BackpropagateAnnotation(plan, placed_node, target_node,
                                  SequenceAnnotationKind::kIO, kIODepth, 2);
        }

        if (!visited_nodes[target_node]) {
          plan.sequence.push_back(pending_step);
          visited_nodes[target_node] = true;
          placed_node = target_node;
          continue;
        }

        if (annotate) {
          BackpropagateAnnotation(plan, placed_node, target_node,
                                  SequenceAnnotationKind::kReconvergence,
                                  kReconvergenceDepth, 1);
        }

        if (stack.empty()) break;
        placed_node = stack.back();
        stack.pop_back();
      }

      if (stack.empty()) {
        bool has_unvisited_edge = false;
        for (char visited : visited_edges) {
          if (!visited) {
            has_unvisited_edge = true;
            break;
          }
        }
        if (has_unvisited_edge) {
          for (int v = 0; v < dfg_.GetNodeNum(); v++) {
            if (!UnvisitedIncidentEdges(v, visited_edges).empty()) {
              stack.push_back(v);
              break;
            }
          }
        } else {
          push_remaining_component_root();
        }
      }
    }

    return plan;
  }

  std::optional<int> PlaceInitialNode(int dfg_node_id, PlacementState& state) {
    std::vector<int> candidates = CompatibleResources(dfg_node_id, state);
    if (candidates.empty()) return std::nullopt;
    std::shuffle(candidates.begin(), candidates.end(), rng_);
    int chosen = candidates.front();
    if (IsIONode(dfg_node_id)) {
      std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
        const double sa = IOPlacementScore(dfg_node_id, a);
        const double sb = IOPlacementScore(dfg_node_id, b);
        if (sa != sb) return sa < sb;
        return a < b;
      });
      const double best_score = IOPlacementScore(dfg_node_id, candidates.front());
      std::vector<int> best_candidates;
      for (int candidate : candidates) {
        if (IOPlacementScore(dfg_node_id, candidate) <= best_score + 1.0) {
          best_candidates.push_back(candidate);
        }
      }
      std::uniform_int_distribution<int> dist(
          0, static_cast<int>(best_candidates.size()) - 1);
      chosen = best_candidates[dist(rng_)];
    }
    state.dfg_to_mrrg[dfg_node_id] = chosen;
    state.mrrg_to_dfg[chosen] = dfg_node_id;
    return chosen;
  }

  double DegreeMatchScore(int dfg_node_id, int mrrg_node_id,
                          const PlacementState& state) const {
    const int target_degree = annotation_.degree[dfg_node_id];
    const int freedom = ResourceFreedom(mrrg_node_id, state);
    if (freedom >= target_degree) return freedom - target_degree;
    return 4.0 * (target_degree - freedom);
  }

  int FreeResourcesAtDistance(int anchor_mrrg_node, int target_distance,
                              const PlacementState& state) const {
    int result = 0;
    for (int r = 0; r < mrrg_.GetNodeNum(); r++) {
      if (state.mrrg_to_dfg[r] != -1) continue;
      if (!SupportsAnyDFGOperation(r)) continue;
      if (ResourceDistance(anchor_mrrg_node, r) == target_distance) result++;
    }
    return result;
  }

  double AnnotationScore(const TraversalStep& step, int candidate_mrrg_node,
                         const PlacementState& state,
                         SequenceAnnotationKind kind,
                         bool lookahead_only) const {
    double best = kImpossibleCost;
    for (const auto& annotation : step.annotations) {
      if (annotation.kind != kind) continue;
      if (lookahead_only && annotation.distance < 2) continue;
      if (!lookahead_only && kind == SequenceAnnotationKind::kReconvergence &&
          annotation.distance >= 2) {
        continue;
      }

      double score = 0.0;
      const int anchor_mrrg_node = state.dfg_to_mrrg[annotation.anchor_node];
      if (kind == SequenceAnnotationKind::kIO) {
        score += IOPlacementScore(step.target_node, candidate_mrrg_node);
        if (anchor_mrrg_node >= 0) {
          const int distance = IsPlacement2DTraversalLike()
                                   ? PhysicalDistance(candidate_mrrg_node,
                                                      anchor_mrrg_node)
                                   : ResourceDistance(candidate_mrrg_node,
                                                      anchor_mrrg_node);
          score += 2.0 * std::abs(distance - annotation.distance);
        }
      } else if (anchor_mrrg_node >= 0) {
        const int distance = IsPlacement2DTraversalLike()
                                 ? PhysicalDistance(candidate_mrrg_node,
                                                    anchor_mrrg_node)
                                 : ResourceDistance(candidate_mrrg_node,
                                                    anchor_mrrg_node);
        score += 3.0 * std::abs(distance - annotation.distance);
        if (lookahead_only) {
          score -= 0.25 * FreeResourcesAtDistance(
                             anchor_mrrg_node, annotation.distance - 1, state);
        }
      } else {
        continue;
      }
      best = std::min(best, score);
    }
    return best;
  }

  bool HasAnnotationKind(const TraversalStep& step,
                         SequenceAnnotationKind kind,
                         bool lookahead_only) const {
    for (const auto& annotation : step.annotations) {
      if (annotation.kind != kind) continue;
      if (lookahead_only && annotation.distance < 2) continue;
      if (!lookahead_only && kind == SequenceAnnotationKind::kReconvergence &&
          annotation.distance >= 2) {
        continue;
      }
      return true;
    }
    return false;
  }

  CandidateRank TraversalPlacementRank(const TraversalStep& step,
                                       int candidate_mrrg_node,
                                       const PlacementState& state,
                                       bool use_annotations,
                                       int random_order) const {
    const int placed_mrrg_node = state.dfg_to_mrrg[step.placed_node];
    const int locality_distance = DirectedDistanceCost(
        IsPlacement2DTraversalLike()
            ? PhysicalDistance(placed_mrrg_node, candidate_mrrg_node)
            : ResourceDistance(placed_mrrg_node, candidate_mrrg_node));
    CandidateRank rank;
    rank.annotation_priority = 3;
    rank.annotation_cost = 0.0;
    rank.locality_distance = locality_distance;
    rank.degree_cost =
        use_annotations ? DegreeMatchScore(step.target_node,
                                           candidate_mrrg_node, state)
                        : 0.0;
    rank.random_order = random_order;

    if (use_annotations && HasAnnotationKind(step, SequenceAnnotationKind::kIO,
                                             false)) {
      rank.annotation_priority = 0;
      rank.annotation_cost = AnnotationScore(step, candidate_mrrg_node, state,
                                             SequenceAnnotationKind::kIO,
                                             false);
      return rank;
    }
    if (use_annotations &&
        HasAnnotationKind(step, SequenceAnnotationKind::kReconvergence, true)) {
      rank.annotation_priority = 1;
      rank.annotation_cost =
          AnnotationScore(step, candidate_mrrg_node, state,
                          SequenceAnnotationKind::kReconvergence, true);
      return rank;
    }
    if (use_annotations && HasAnnotationKind(
                               step, SequenceAnnotationKind::kReconvergence,
                               false)) {
      rank.annotation_priority = 2;
      rank.annotation_cost =
          AnnotationScore(step, candidate_mrrg_node, state,
                          SequenceAnnotationKind::kReconvergence, false);
      return rank;
    }

    return rank;
  }

  bool PlaceTraversalStep(const TraversalStep& step, PlacementState& state,
                          bool use_annotations) {
    if (state.dfg_to_mrrg[step.target_node] >= 0) return true;
    if (state.dfg_to_mrrg[step.placed_node] < 0) {
      if (!PlaceInitialNode(step.placed_node, state).has_value()) return false;
    }
    std::vector<int> candidates = ClosestCompatibleResources(
        step.target_node, state.dfg_to_mrrg[step.placed_node], state);
    if (candidates.empty()) return false;
    if (search_kind_ == PlacementSearchKind::kPlacement2DYOTO) {
      std::sort(candidates.begin(), candidates.end());
    } else {
      std::shuffle(candidates.begin(), candidates.end(), rng_);
    }
    std::vector<std::pair<CandidateRank, int>> scored_candidates;
    scored_candidates.reserve(candidates.size());
    for (int order = 0; order < static_cast<int>(candidates.size()); order++) {
      int candidate = candidates[order];
      scored_candidates.push_back(
          {TraversalPlacementRank(step, candidate, state, use_annotations,
                                  order),
           candidate});
    }
    std::sort(scored_candidates.begin(), scored_candidates.end(),
              [](const auto& a, const auto& b) {
                if (a.first.annotation_priority != b.first.annotation_priority) {
                  return a.first.annotation_priority <
                         b.first.annotation_priority;
                }
                if (a.first.annotation_cost != b.first.annotation_cost) {
                  return a.first.annotation_cost < b.first.annotation_cost;
                }
                if (a.first.locality_distance != b.first.locality_distance) {
                  return a.first.locality_distance <
                         b.first.locality_distance;
                }
                if (a.first.degree_cost != b.first.degree_cost) {
                  return a.first.degree_cost < b.first.degree_cost;
                }
                return a.first.random_order < b.first.random_order;
              });
    const int chosen = scored_candidates.front().second;
    state.dfg_to_mrrg[step.target_node] = chosen;
    state.mrrg_to_dfg[chosen] = step.target_node;
    return true;
  }

  std::optional<PlacementState> ConstructTraversalPlacement() {
    const bool use_annotations = IsYOTTLike();
    TraversalPlan plan = IsPlacement2DTraversalLike()
                             ? BuildPlacement2DTraversalPlan(use_annotations)
                             : BuildTraversalPlan(use_annotations);
    PlacementState state;
    state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
    state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);

    for (const auto& step : plan.sequence) {
      if (!PlaceTraversalStep(step, state, use_annotations)) return std::nullopt;
    }
    for (int node_id = 0; node_id < dfg_.GetNodeNum(); node_id++) {
      if (state.dfg_to_mrrg[node_id] < 0) {
        if (!PlaceInitialNode(node_id, state).has_value()) return std::nullopt;
      }
    }
    return state;
  }

  std::optional<PlacementState> ConstructGreedyPlacement() {
    return ConstructTraversalPlacement();
  }

  std::optional<PlacementState> RunGreedyMultiStart(
      std::chrono::steady_clock::time_point start) {
    const int max_trials = MaxTrials();
    const int seed_count = SeedCount();
    std::optional<PlacementState> best_unrouted;
    double best_unrouted_cost = std::numeric_limits<double>::infinity();
    std::vector<std::pair<double, PlacementState>> elite;
    const bool route_all_placement_trials =
        search_kind_ == PlacementSearchKind::kYOTO ||
        search_kind_ == PlacementSearchKind::kYOTT || IsPlacement2DTraversalLike();
    const int elite_limit = route_all_placement_trials
                                ? std::max(8, max_trials * seed_count)
                                : std::max(8, RoutingRetryCount() * 2);
    int total_trials = 0;

    for (int seed_index = 0;
         seed_index < seed_count && !HasTimedOut(start, 0.1);
         seed_index++) {
      ResetSeed(seed_index);
      for (int trial = 0;
           trial < max_trials && !HasTimedOut(start, 0.1);
           trial++, total_trials++) {
        auto placement = ConstructGreedyPlacement();
        if (!placement.has_value()) continue;
        const double cost = PlacementCost(*placement);
        if (cost < best_unrouted_cost) {
          best_unrouted = placement;
          best_unrouted_cost = cost;
        }
        elite.push_back({cost, *placement});
        std::sort(elite.begin(), elite.end(),
                  [](const auto& a, const auto& b) {
                    return a.first < b.first;
                  });
        if (static_cast<int>(elite.size()) > elite_limit) {
          elite.pop_back();
        }
      }
    }

    std::optional<PlacementState> best_routeable;
    double best_routeable_cost = std::numeric_limits<double>::infinity();
    if (options_.placement_only) {
      Log("greedy_search trials=" + std::to_string(total_trials) +
          " elite=" + std::to_string(elite.size()) +
          " placement_only=1");
      return best_unrouted;
    }
    for (const auto& candidate : elite) {
      if (HasTimedOut(start, 0.02)) break;
      auto route_usage = TryRoutePlacement(candidate.second);
      if (route_usage.has_value() && candidate.first < best_routeable_cost) {
        best_routeable = candidate.second;
        best_routeable_cost = candidate.first;
      }
    }

    Log("greedy_search trials=" + std::to_string(total_trials) +
        " elite=" + std::to_string(elite.size()) +
        " routeable=" + (best_routeable.has_value() ? "1" : "0"));
    if (best_routeable.has_value()) return best_routeable;
    return best_unrouted;
  }

  double PlacementCost(const PlacementState& state) const {
    double cost = 0.0;
    for (const auto& edge : dfg_edges_) {
      const int from = state.dfg_to_mrrg[edge.source];
      const int to = state.dfg_to_mrrg[edge.target];
      if (from < 0 || to < 0) return kImpossibleCost;
      const int d = distance_[from][to];
      if (d >= kInfDistance) {
        cost += kImpossibleCost;
      } else {
        cost += DirectedDistanceCost(d);
      }
    }
    return cost;
  }

  std::optional<PlacementState> RandomLegalPlacement() {
    PlacementState state;
    state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
    state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
    std::vector<int> node_order(dfg_.GetNodeNum());
    std::iota(node_order.begin(), node_order.end(), 0);
    if (search_kind_ == PlacementSearchKind::kPlacement2DSA) {
      std::shuffle(node_order.begin(), node_order.end(), rng_);
    } else {
      std::sort(node_order.begin(), node_order.end(), [&](int a, int b) {
        return annotation_.degree[a] > annotation_.degree[b];
      });
    }
    for (int dfg_node_id : node_order) {
      auto candidates = CompatibleResources(dfg_node_id, state);
      if (candidates.empty()) return std::nullopt;
      std::uniform_int_distribution<int> dist(0,
                                              static_cast<int>(candidates.size()) - 1);
      int chosen = candidates[dist(rng_)];
      state.dfg_to_mrrg[dfg_node_id] = chosen;
      state.mrrg_to_dfg[chosen] = dfg_node_id;
    }
    return state;
  }

  std::optional<PlacementState> RunSAMultiSeed(
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
      auto placement = RunSA(start);
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
    Log("sa_search seeds=" + std::to_string(attempted_seeds) +
        " routeable=" + (best_routeable.has_value() ? "1" : "0"));
    if (options_.placement_only) return best_unrouted;
    if (best_routeable.has_value()) return best_routeable;
    return best_unrouted;
  }

  std::optional<PlacementState> RunSA(
      std::chrono::steady_clock::time_point start) {
    if (search_kind_ == PlacementSearchKind::kPlacement2DSA) {
      return RunPlacement2DSA(start);
    }

    std::optional<PlacementState> current = ConstructGreedyPlacement();
    if (!current.has_value()) current = RandomLegalPlacement();
    if (!current.has_value()) return std::nullopt;

    PlacementState best = *current;
    double current_cost = PlacementCost(*current);
    double best_cost = current_cost;
    std::vector<PlacementState> elite;
    elite.push_back(best);

    const int node_num = dfg_.GetNodeNum();
    double temperature = std::max(1.0, current_cost / std::max(1, node_num));
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    int iteration = 0;
    const int max_iterations = MaxIterations();

    while (iteration < max_iterations && !HasTimedOut(start, 0.05)) {
      iteration++;
      PlacementState next = *current;
      if (!ApplyRandomMove(next)) continue;
      const double next_cost = PlacementCost(next);
      const double delta = next_cost - current_cost;
      const bool accept = delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
      if (accept) {
        *current = next;
        current_cost = next_cost;
      }
      if (next_cost < best_cost) {
        best = next;
        best_cost = next_cost;
        elite.push_back(best);
        if (elite.size() > 16) elite.erase(elite.begin());
      }
      temperature *= 0.995;
      if (temperature < 1.0e-4) temperature = std::max(1.0, best_cost / 10.0);
    }

    std::sort(elite.begin(), elite.end(), [&](const PlacementState& a,
                                              const PlacementState& b) {
      return PlacementCost(a) < PlacementCost(b);
    });
    if (options_.placement_only) return best;
    for (const auto& candidate : elite) {
      if (TryRoutePlacement(candidate).has_value()) return candidate;
      if (HasTimedOut(start, 0.01)) break;
    }
    return best;
  }

  std::optional<PlacementState> RunPlacement2DSA(
      std::chrono::steady_clock::time_point start) {
    std::optional<PlacementState> current = RandomLegalPlacement();
    if (!current.has_value()) return std::nullopt;

    PlacementState best = *current;
    double current_cost = PlacementCost(*current);
    double best_cost = current_cost;
    std::vector<PlacementState> elite;
    elite.push_back(best);

    std::uniform_real_distribution<double> unit(0.0, 1.0);
    double temperature = 100.0;
    int iteration = 0;
    const int max_iterations = MaxIterations();
    std::vector<int> node_order(dfg_.GetNodeNum());
    std::iota(node_order.begin(), node_order.end(), 0);

    auto try_candidate = [&](const PlacementState& next) {
      const double next_cost = PlacementCost(next);
      const double delta = next_cost - current_cost;
      const bool accept =
          delta <= 0.0 || unit(rng_) < std::exp(-delta / temperature);
      if (accept) {
        *current = next;
        current_cost = next_cost;
      }
      if (next_cost < best_cost) {
        best = next;
        best_cost = next_cost;
        elite.push_back(best);
        if (elite.size() > 16) elite.erase(elite.begin());
      }
    };

    while (iteration < max_iterations && !HasTimedOut(start, 0.05) &&
           temperature > 1.0e-5) {
      std::shuffle(node_order.begin(), node_order.end(), rng_);
      for (int i = 0; i < static_cast<int>(node_order.size()); i++) {
        for (int j = i + 1; j < static_cast<int>(node_order.size()); j++) {
          PlacementState next = *current;
          if (!ApplySwapMove(next, node_order[i], node_order[j])) continue;
          try_candidate(next);
          iteration++;
          if (iteration >= max_iterations || HasTimedOut(start, 0.05)) break;
        }
        if (iteration >= max_iterations || HasTimedOut(start, 0.05)) break;
      }
      const int empty_cell_trials = std::max(1, dfg_.GetNodeNum());
      for (int i = 0; i < empty_cell_trials && iteration < max_iterations &&
                      !HasTimedOut(start, 0.05);
           i++) {
        PlacementState next = *current;
        if (!ApplyMoveToFreeResource(next)) continue;
        try_candidate(next);
        iteration++;
      }
      temperature *= 0.999;
    }

    std::sort(elite.begin(), elite.end(), [&](const PlacementState& a,
                                              const PlacementState& b) {
      return PlacementCost(a) < PlacementCost(b);
    });
    if (options_.placement_only) return best;
    for (const auto& candidate : elite) {
      if (TryRoutePlacement(candidate).has_value()) return candidate;
      if (HasTimedOut(start, 0.01)) break;
    }
    return best;
  }

  bool ApplyRandomMove(PlacementState& state) {
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

  bool ApplySwapMove(PlacementState& state, int a, int b) const {
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

  bool ApplyMoveToFreeResource(PlacementState& state) {
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

  std::optional<RouteUsage> TryRoutePlacement(const PlacementState& state) {
    std::vector<std::vector<DFGEdgeInfo>> route_orders;
    route_orders.push_back(dfg_edges_);

    route_orders.push_back(dfg_edges_);
    std::sort(route_orders.back().begin(), route_orders.back().end(),
              [&](const auto& a, const auto& b) {
      const int da = distance_[state.dfg_to_mrrg[a.source]]
                              [state.dfg_to_mrrg[a.target]];
      const int db = distance_[state.dfg_to_mrrg[b.source]]
                              [state.dfg_to_mrrg[b.target]];
      return da > db;
    });

    route_orders.push_back(route_orders.back());
    std::reverse(route_orders.back().begin(), route_orders.back().end());

    route_orders.push_back(dfg_edges_);
    std::sort(route_orders.back().begin(), route_orders.back().end(),
              [&](const auto& a, const auto& b) {
                const int degree_a = annotation_.degree[a.source] +
                                     annotation_.degree[a.target];
                const int degree_b = annotation_.degree[b.source] +
                                     annotation_.degree[b.target];
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

  std::optional<RouteUsage> TryRoutePlacementInOrder(
      const PlacementState& state,
      const std::vector<DFGEdgeInfo>& route_order) const {
    RouteUsage usage;
    usage.route_node_owner.assign(mrrg_.GetNodeNum(), -1);
    usage.route_edge_owner.assign(mrrg_.GetEdgeNum(), -1);
    usage.source_to_route_edges.assign(dfg_.GetNodeNum(), {});

    for (const auto& edge : route_order) {
      const int source_node = state.dfg_to_mrrg[edge.source];
      const int target_node = state.dfg_to_mrrg[edge.target];
      auto path = FindRoutePath(edge.source, source_node, target_node, state,
                                usage);
      if (!path.has_value()) return std::nullopt;
      ReserveRoute(edge.source, source_node, target_node, *path, state, usage);
    }
    return usage;
  }

  std::optional<PathResult> FindRoutePath(int source_dfg_node, int source_mrrg,
                                          int target_mrrg,
                                          const PlacementState& state,
                                          const RouteUsage& usage) const {
    if (source_mrrg != target_mrrg) {
      return FindRoutePathNonEmptyAllowed(source_dfg_node, source_mrrg,
                                          target_mrrg, state, usage, false);
    }

    // Self-loop/recurrent edge: force at least one MRRG edge.
    for (int first_edge : mrrg_.GetOutEdgeIdVec(source_mrrg)) {
      if (!CanUseEdge(first_edge, source_dfg_node, usage)) continue;
      auto edge_pair = mrrg_.GetEdgeSourceTarget(first_edge);
      int first_to = edge_pair.second;
      if (!CanUseNode(first_to, source_dfg_node, source_mrrg, target_mrrg,
                      state, usage)) {
        continue;
      }
      if (first_to == target_mrrg) {
        return PathResult{{first_edge}, {source_mrrg, target_mrrg}};
      }
      auto suffix = FindRoutePathNonEmptyAllowed(source_dfg_node, first_to,
                                                 target_mrrg, state, usage,
                                                 false);
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

  std::optional<PathResult> FindRoutePathNonEmptyAllowed(
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

  bool CanUseEdge(int edge_id, int source_dfg_node,
                  const RouteUsage& usage) const {
    return usage.route_edge_owner[edge_id] == -1 ||
           usage.route_edge_owner[edge_id] == source_dfg_node;
  }

  bool CanUseNode(int mrrg_node_id, int source_dfg_node, int source_mrrg,
                  int target_mrrg, const PlacementState& state,
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

  void ReserveRoute(int source_dfg_node, int source_mrrg, int target_mrrg,
                    const PathResult& path, const PlacementState& state,
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

  int RouteEdgeCount(const RouteUsage& usage) const {
    int result = 0;
    for (const auto& edges : usage.source_to_route_edges) {
      std::set<int> unique_edges(edges.begin(), edges.end());
      result += static_cast<int>(unique_edges.size());
    }
    return result;
  }
};

}  // namespace

namespace mapper::detail {

MappingResult RunPlacementSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    PlacementSearchKind search_kind,
    const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.has_value() ? timeout_s.value() : 1.0;
  PlacementSearchEngine engine(*dfg_ptr, *mrrg_ptr, search_kind,
                               effective_timeout, log_file_path, options);
  return engine.Run();
}

}  // namespace mapper::detail
