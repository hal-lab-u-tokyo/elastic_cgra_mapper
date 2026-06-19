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
    if (NeedsAllPairsDistances()) {
      BuildAllPairsDistances();
    }
    if (IsPRISALike()) {
      BuildPRISADistanceRegions();
    }
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
        std::to_string(IsPRISALike() ? PRISAMaxIterations()
                                     : MaxIterations()));

    std::optional<PlacementState> placement;
    if (IsPRISALike()) {
      placement = RunPRISAMultiSeed(start);
    } else if (IsSALike()) {
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
  std::vector<int> placement2d_resources_;
  std::vector<int> placement2d_resource_order_index_;
  std::vector<int> prisa_resources_;
  std::vector<int> prisa_resource_order_index_;
  std::vector<int> prisa_distance_matrix_;
  std::vector<char> prisa_weak_region_matrix_;
  int prisa_weak_distance_threshold_ = kInfDistance;
  std::unordered_map<long long, int> mrrg_edge_id_;
  Annotation annotation_;

  std::string MapperName() const {
    switch (search_kind_) {
      case PlacementSearchKind::kYOTO:
        return "YOTO";
      case PlacementSearchKind::kYOTT:
        return "YOTT";
      case PlacementSearchKind::kModuloPhysicalYOTO:
        return "ModuloPhysicalYOTO";
      case PlacementSearchKind::kModuloPhysicalYOTT:
        return "ModuloPhysicalYOTT";
      case PlacementSearchKind::kModuloPhysicalPRISA:
        return "ModuloPhysicalPRISA";
      case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
        return "ModuloPhysicalPRISAManhattan";
      case PlacementSearchKind::kYOTOWithFallback:
        return "YOTOWithFallback";
      case PlacementSearchKind::kYOTTWithFallback:
        return "YOTTWithFallback";
      case PlacementSearchKind::kSA:
        return "SA";
      case PlacementSearchKind::kPlacement2DYOTO:
        return "Placement2DYOTO";
      case PlacementSearchKind::kPlacement2DYOTT:
        return "Placement2DYOTT";
      case PlacementSearchKind::kPlacement2DSA:
        return "Placement2DSA";
      case PlacementSearchKind::kPlacement2DPRISA:
        return "Placement2DPRISA";
      case PlacementSearchKind::kPlacement2DPRISANoSIS:
        return "Placement2DPRISANoSIS";
      case PlacementSearchKind::kPlacement2DCostAwarePRISA:
        return "Placement2DCostAwarePRISA";
    }
    return "unknown";
  }

  bool IsSALike() const {
    return search_kind_ == PlacementSearchKind::kSA ||
           search_kind_ == PlacementSearchKind::kPlacement2DSA;
  }

  bool IsPRISALike() const {
    return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
           search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan ||
           search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
           search_kind_ == PlacementSearchKind::kPlacement2DPRISANoSIS ||
           search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
  }

  bool UsesPRISASIS() const {
    return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
           search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan ||
           search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
           search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
  }

  bool UsesManhattanRouting() const {
    return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan;
  }

  bool UsesCostAwarePRISA() const {
    return search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
  }

  bool IsYOTTLike() const {
    return search_kind_ == PlacementSearchKind::kYOTT ||
           search_kind_ == PlacementSearchKind::kYOTTWithFallback ||
           search_kind_ == PlacementSearchKind::kModuloPhysicalYOTT ||
           search_kind_ == PlacementSearchKind::kPlacement2DYOTT;
  }

  bool IsPlacement2DTraversalLike() const {
    return search_kind_ == PlacementSearchKind::kPlacement2DYOTO ||
           search_kind_ == PlacementSearchKind::kPlacement2DYOTT;
  }

  bool IsPhysicalThenContextLike() const {
    return search_kind_ == PlacementSearchKind::kModuloPhysicalYOTO ||
           search_kind_ == PlacementSearchKind::kModuloPhysicalYOTT ||
           search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
           search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan;
  }

  bool UsesPhysicalPlacementStage() const {
    return IsPlacement2DTraversalLike() || IsPhysicalThenContextLike();
  }

  bool UsesPaperTraversalPlan() const {
    return search_kind_ == PlacementSearchKind::kYOTO ||
           search_kind_ == PlacementSearchKind::kYOTT ||
           IsPhysicalThenContextLike() ||
           search_kind_ == PlacementSearchKind::kYOTOWithFallback ||
           search_kind_ == PlacementSearchKind::kYOTTWithFallback ||
           IsPlacement2DTraversalLike();
  }

  bool UsesModuloFallbackCandidates() const {
    return search_kind_ == PlacementSearchKind::kYOTOWithFallback ||
           search_kind_ == PlacementSearchKind::kYOTTWithFallback;
  }

  bool UsesPhysicalPeExclusivePlacement() const {
    return UsesPhysicalPlacementStage() ||
           search_kind_ == PlacementSearchKind::kPlacement2DSA ||
           IsPRISALike();
  }

  bool UsesSeparatePlacement2DIOCells() const {
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

  bool UsesApproximatePlacementDistance() const {
    return search_kind_ == PlacementSearchKind::kPlacement2DYOTO ||
           search_kind_ == PlacementSearchKind::kPlacement2DYOTT ||
           IsPhysicalThenContextLike() ||
           search_kind_ == PlacementSearchKind::kPlacement2DSA ||
           IsPRISALike();
  }

  bool NeedsAllPairsDistances() const {
    return !UsesApproximatePlacementDistance();
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
    std::sort(prisa_resources_.begin(), prisa_resources_.end(),
              [&](int a, int b) {
                const auto pa = mrrg_.GetNodeProperty(a).position_id;
                const auto pb = mrrg_.GetNodeProperty(b).position_id;
                if (pa.first != pb.first) return pa.first < pb.first;
                // PRISA's low-bandwidth labeling assumes nearby labels are
                // nearby resources. A serpentine path avoids row-major jumps
                // from the end of one row to the start of the next row.
                const bool reverse_row = ((pa.first - min_row) % 2) != 0;
                return reverse_row ? pa.second > pb.second
                                   : pa.second < pb.second;
              });
    prisa_resource_order_index_.assign(node_num, -1);
    for (int i = 0; i < static_cast<int>(prisa_resources_.size()); i++) {
      prisa_resource_order_index_[prisa_resources_[i]] = i;
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

  bool IsIONode(int dfg_node_id) const {
    const entity::OpType op = dfg_.GetNodeProperty(dfg_node_id).op;
    return entity::IsMemoryAccessOperation(op);
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
    return std::min(DirectedResourceDistance(from_mrrg_node, to_mrrg_node),
                    DirectedResourceDistance(to_mrrg_node, from_mrrg_node));
  }

  int DirectedResourceDistance(int from_mrrg_node, int to_mrrg_node) const {
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
    if (!distance_.empty()) return distance_[from_mrrg_node][to_mrrg_node];
    return ApproximateDirectedResourceDistance(from_mrrg_node, to_mrrg_node);
  }

  int SpatialStepDistance(int from_mrrg_node, int to_mrrg_node) const {
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
    const auto from = mrrg_.GetNodeProperty(from_mrrg_node).position_id;
    const auto to = mrrg_.GetNodeProperty(to_mrrg_node).position_id;
    const int row_distance = std::abs(from.first - to.first);
    const int column_distance = std::abs(from.second - to.second);
    const auto config = mrrg_.GetMRRGConfig();
    if (config.network_type == entity::MRRGNetworkType::kOneHopAxis2) {
      return (row_distance + 1) / 2 + (column_distance + 1) / 2;
    }
    if (config.network_type == entity::MRRGNetworkType::kDiagonal) {
      return std::max(row_distance, column_distance);
    }
    return row_distance + column_distance;
  }

  int ApproximateDirectedResourceDistance(int from_mrrg_node,
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

  int PhysicalDistance(int from_mrrg_node, int to_mrrg_node) const {
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
    return std::max(1, SpatialStepDistance(from_mrrg_node, to_mrrg_node));
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

  bool IsCornerResource(int mrrg_node_id) const {
    const auto property = mrrg_.GetNodeProperty(mrrg_node_id);
    const auto config = mrrg_.GetMRRGConfig();
    const bool row_on_border =
        property.position_id.first == 0 ||
        property.position_id.first == config.row - 1;
    const bool column_on_border =
        property.position_id.second == 0 ||
        property.position_id.second == config.column - 1;
    return row_on_border && column_on_border;
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

  double Placement2DInitialPlacementScore(int dfg_node_id,
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

  std::vector<double> BuildBetweennessCentralityScore() const {
    const int n = dfg_.GetNodeNum();
    std::vector<double> centrality(n, 0.0);
    for (int source = 0; source < n; source++) {
      std::vector<std::vector<int>> predecessors_on_paths(n);
      std::vector<int> distance(n, -1);
      std::vector<double> sigma(n, 0.0);
      std::vector<double> dependency(n, 0.0);
      std::vector<int> order;
      std::queue<int> q;

      distance[source] = 0;
      sigma[source] = 1.0;
      q.push(source);
      while (!q.empty()) {
        const int v = q.front();
        q.pop();
        order.push_back(v);
        for (int w : successors_[v]) {
          if (w == v) continue;
          if (distance[w] < 0) {
            distance[w] = distance[v] + 1;
            q.push(w);
          }
          if (distance[w] == distance[v] + 1) {
            sigma[w] += sigma[v];
            predecessors_on_paths[w].push_back(v);
          }
        }
      }

      for (auto it = order.rbegin(); it != order.rend(); ++it) {
        const int w = *it;
        if (sigma[w] == 0.0) continue;
        for (int v : predecessors_on_paths[w]) {
          dependency[v] +=
              (sigma[v] / sigma[w]) * (1.0 + dependency[w]);
        }
        if (w != source) centrality[w] += dependency[w];
      }
    }
    return centrality;
  }

  int Placement2DCentralityScore(int dfg_node_id) const {
    return annotation_.degree[dfg_node_id] +
           annotation_.in_degree[dfg_node_id] * annotation_.out_degree[dfg_node_id];
  }

  int ChoosePlacement2DNeighbor(const std::vector<int>& candidates,
                           bool choose_from_fanout, int mode,
                           bool zigzag_take_back,
                           const std::vector<int>& critical_path,
                           const std::vector<double>& betweenness) {
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
        if (betweenness[a] != betweenness[b]) {
          return betweenness[a] < betweenness[b];
        }
        return annotation_.degree[a] < annotation_.degree[b];
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
    const auto betweenness = BuildBetweennessCentralityScore();
    int selection_mode = 0;
    if (annotate && IsYOTTLike()) {
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
                                   true, critical_path, betweenness);
          for (int i = 0; i < fanin; i++) stack.push_back({a, 1});
          stack.push_back({b, 0});
          remove_value(local_fanout[a], b);
          remove_value(local_fanin[b], a);
          append_step(a, b);
        } else if (fanin >= 1) {
          b = ChoosePlacement2DNeighbor(local_fanin[a], false, selection_mode,
                                   true, critical_path, betweenness);
          stack.push_back({a, 1});
          for (int i = 0; i < fanin; i++) stack.push_back({b, 1});
          remove_value(local_fanin[a], b);
          remove_value(local_fanout[b], a);
          append_step(a, b);
        }
      } else {
        if (fanin >= 1) {
          b = ChoosePlacement2DNeighbor(local_fanin[a], false, selection_mode,
                                   false, critical_path, betweenness);
          for (int i = 0; i < fanout; i++) stack.push_back({a, 0});
          stack.push_back({b, 1});
          remove_value(local_fanin[a], b);
          remove_value(local_fanout[b], a);
          append_step(a, b);
        } else if (fanout >= 1) {
          b = ChoosePlacement2DNeighbor(local_fanout[a], true, selection_mode,
                                   false, critical_path, betweenness);
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
    if (UsesPhysicalPlacementStage()) {
      std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
        const double sa = Placement2DInitialPlacementScore(dfg_node_id, a);
        const double sb = Placement2DInitialPlacementScore(dfg_node_id, b);
        if (sa != sb) return sa < sb;
        return a < b;
      });
      const double best_score =
          Placement2DInitialPlacementScore(dfg_node_id, candidates.front());
      std::vector<int> best_candidates;
      for (int candidate : candidates) {
        if (Placement2DInitialPlacementScore(dfg_node_id, candidate) <=
            best_score + 1.0e-9) {
          best_candidates.push_back(candidate);
        }
      }
      std::uniform_int_distribution<int> dist(
          0, static_cast<int>(best_candidates.size()) - 1);
      chosen = best_candidates[dist(rng_)];
    } else if (IsIONode(dfg_node_id)) {
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
      if (UsesPhysicalPeExclusivePlacement() &&
          IsPhysicalPositionOccupied(r, state)) {
        continue;
      }
      if (!UsesPhysicalPeExclusivePlacement() && state.mrrg_to_dfg[r] != -1) {
        continue;
      }
      if (!SupportsAnyDFGOperation(r)) continue;
      const int distance = UsesPhysicalPlacementStage()
                               ? PhysicalDistance(anchor_mrrg_node, r)
                               : ResourceDistance(anchor_mrrg_node, r);
      if (distance == target_distance) result++;
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
          const int distance = UsesPhysicalPlacementStage()
                                   ? PhysicalDistance(candidate_mrrg_node,
                                                      anchor_mrrg_node)
                                   : ResourceDistance(candidate_mrrg_node,
                                                      anchor_mrrg_node);
          score += 2.0 * std::abs(distance - annotation.distance);
        }
      } else if (anchor_mrrg_node >= 0) {
        const int distance = UsesPhysicalPlacementStage()
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
        UsesPhysicalPlacementStage()
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
    if (search_kind_ == PlacementSearchKind::kPlacement2DYOTO ||
        search_kind_ == PlacementSearchKind::kModuloPhysicalYOTO) {
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

  std::optional<PlacementState> ConstructTraversalPlacement(
      bool use_paper_traversal_plan, bool use_annotations) {
    TraversalPlan plan = use_paper_traversal_plan
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

  std::vector<int> BuildASAPContextLevels() const {
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

  std::optional<int> FindSamePhysicalResourceAtContext(
      int base_resource, int dfg_node_id, int desired_context,
      const PlacementState& assigned) const {
    const int context_size =
        std::max(1, mrrg_.GetMRRGConfig().context_size);
    const auto base_position = mrrg_.GetNodeProperty(base_resource).position_id;
    for (int delta = 0; delta < context_size; delta++) {
      const int context_id = (desired_context + delta) % context_size;
      for (int resource = 0; resource < mrrg_.GetNodeNum(); resource++) {
        if (assigned.mrrg_to_dfg[resource] != -1) continue;
        const auto property = mrrg_.GetNodeProperty(resource);
        if (property.position_id != base_position) continue;
        if (property.context_id != context_id) continue;
        if (!IsCompatible(dfg_node_id, resource)) continue;
        return resource;
      }
    }
    return std::nullopt;
  }

  std::optional<PlacementState> AssignContextsToPhysicalPlacement(
      const PlacementState& physical_placement) const {
    if (!IsPhysicalThenContextLike()) return physical_placement;

    const int context_size =
        std::max(1, mrrg_.GetMRRGConfig().context_size);
    if (context_size == 1) return physical_placement;

    const std::vector<int> levels = BuildASAPContextLevels();
    PlacementState assigned;
    assigned.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
    assigned.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);

    for (int dfg_node_id = 0; dfg_node_id < dfg_.GetNodeNum(); dfg_node_id++) {
      const int base_resource = physical_placement.dfg_to_mrrg[dfg_node_id];
      if (base_resource < 0) return std::nullopt;
      const int desired_context = levels[dfg_node_id] % context_size;
      auto resource = FindSamePhysicalResourceAtContext(
          base_resource, dfg_node_id, desired_context, assigned);
      if (!resource.has_value()) return std::nullopt;
      assigned.dfg_to_mrrg[dfg_node_id] = resource.value();
      assigned.mrrg_to_dfg[resource.value()] = dfg_node_id;
    }

    return assigned;
  }

  std::optional<PlacementState> FinalizePhysicalPlacementIfNeeded(
      const PlacementState& placement) const {
    if (!IsPhysicalThenContextLike()) return placement;
    return AssignContextsToPhysicalPlacement(placement);
  }

  void PushPlacementCandidate(std::vector<PlacementState>& result,
                              const std::optional<PlacementState>& placement) {
    if (!placement.has_value()) return;
    auto finalized = FinalizePhysicalPlacementIfNeeded(*placement);
    if (finalized.has_value()) result.push_back(*finalized);
  }

  std::optional<PlacementState> ConstructGreedyPlacement() {
    return ConstructTraversalPlacement(UsesPaperTraversalPlan(), IsYOTTLike());
  }

  std::vector<PlacementState> ConstructGreedyPlacementCandidates() {
    std::vector<PlacementState> result;
    auto primary =
        ConstructTraversalPlacement(UsesPaperTraversalPlan(), IsYOTTLike());
    PushPlacementCandidate(result, primary);

    // Fallback variants intentionally mix the paper-style traversal with older
    // routing-aware placements. The pure YOTO/YOTT mappers do not enter this
    // block, so comparison results remain attributable to one strategy.
    if (UsesModuloFallbackCandidates()) {
      auto fallback = ConstructTraversalPlacement(false, IsYOTTLike());
      PushPlacementCandidate(result, fallback);
      if (IsYOTTLike()) {
        auto yoto_style_primary = ConstructTraversalPlacement(true, false);
        PushPlacementCandidate(result, yoto_style_primary);
        auto yoto_style_fallback = ConstructTraversalPlacement(false, false);
        PushPlacementCandidate(result, yoto_style_fallback);
      }
    }
    return result;
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
        search_kind_ == PlacementSearchKind::kYOTT ||
        search_kind_ == PlacementSearchKind::kYOTOWithFallback ||
        search_kind_ == PlacementSearchKind::kYOTTWithFallback ||
        IsPlacement2DTraversalLike() ||
        IsPhysicalThenContextLike();
    const int placement_variants_per_trial =
        UsesModuloFallbackCandidates() ? (IsYOTTLike() ? 4 : 2) : 1;
    const int elite_limit = route_all_placement_trials
                                ? std::max(8, max_trials * seed_count *
                                                  placement_variants_per_trial)
                                : std::max(8, RoutingRetryCount() * 2);
    int total_trials = 0;

    for (int seed_index = 0;
         seed_index < seed_count && !HasTimedOut(start, 0.1);
         seed_index++) {
      ResetSeed(seed_index);
      for (int trial = 0;
           trial < max_trials && !HasTimedOut(start, 0.1);
           trial++, total_trials++) {
        auto placements = ConstructGreedyPlacementCandidates();
        for (const auto& placement : placements) {
          const double cost = PlacementCost(placement);
          if (cost < best_unrouted_cost) {
            best_unrouted = placement;
            best_unrouted_cost = cost;
          }
          elite.push_back({cost, placement});
          std::sort(elite.begin(), elite.end(),
                    [](const auto& a, const auto& b) {
                      return a.first < b.first;
                    });
          if (static_cast<int>(elite.size()) > elite_limit) {
            elite.pop_back();
          }
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

  double ResourcePairPlacementCost(int from_mrrg_node,
                                   int to_mrrg_node) const {
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return kImpossibleCost;
    const int distance = DirectedResourceDistance(from_mrrg_node, to_mrrg_node);
    if (distance >= kInfDistance) return kImpossibleCost;
    return DirectedDistanceCost(distance);
  }

  double EdgePlacementCost(const DFGEdgeInfo& edge,
                           const std::vector<int>& dfg_to_mrrg) const {
    return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                     dfg_to_mrrg[edge.target]);
  }

  double PlacementCost(const PlacementState& state) const {
    double cost = 0.0;
    for (const auto& edge : dfg_edges_) {
      const double edge_cost = EdgePlacementCost(edge, state.dfg_to_mrrg);
      if (edge_cost >= kImpossibleCost) return kImpossibleCost;
      cost += edge_cost;
    }
    return cost;
  }

  bool PRISAEdgeIsWeak(int source_resource, int target_resource,
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

  int PRISAWeakEdgeCount(const PlacementState& state) const {
    const int resource_count = static_cast<int>(Placement2DResourceOrder().size());
    int weak_edges = 0;
    for (const auto& edge : dfg_edges_) {
      if (PRISAEdgeIsWeak(state.dfg_to_mrrg[edge.source],
                          state.dfg_to_mrrg[edge.target], resource_count)) {
        weak_edges++;
      }
    }
    return weak_edges;
  }

  std::optional<PlacementState> RandomLegalPlacement() {
    PlacementState state;
    state.dfg_to_mrrg.assign(dfg_.GetNodeNum(), -1);
    state.mrrg_to_dfg.assign(mrrg_.GetNodeNum(), -1);
    std::vector<int> node_order(dfg_.GetNodeNum());
    std::iota(node_order.begin(), node_order.end(), 0);
    if (search_kind_ == PlacementSearchKind::kPlacement2DSA || IsPRISALike()) {
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

  std::optional<PlacementState> RunPRISAMultiSeed(
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

  int PRISAMaxIterations() const {
    if (options_.max_iterations.has_value()) {
      return std::max(1, options_.max_iterations.value());
    }
    return std::max(1, dfg_.GetNodeNum() * 10);
  }

  const std::vector<int>& Placement2DResourceOrder() const {
    if (IsPRISALike() && !prisa_resources_.empty()) return prisa_resources_;
    return placement2d_resources_;
  }

  const std::vector<int>& ResourceOrderIndex() const {
    if (IsPRISALike() && !prisa_resource_order_index_.empty()) {
      return prisa_resource_order_index_;
    }
    return placement2d_resource_order_index_;
  }

  int PRISAResourceSideLength() const {
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

  int PRISAWeakPairCount(int resource_count) const {
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

  void BuildPRISADistanceRegions() {
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

  bool IsPRISAWeakRegion(int row, int column, int resource_count) const {
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

  bool IsPRISAPotentialRegion(int row, int column,
                              int resource_count) const {
    if (row == column) return false;
    return !IsPRISAWeakRegion(row, column, resource_count);
  }

  std::vector<int> LowBandwidthNodeOrder() {
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

  std::optional<PlacementState> ConstructPRISAInitialPlacementFromOrder(
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

  int PRISAInitialSolutionSampleCount() const {
    if (!UsesPRISASIS()) return 1;
    return 1;
  }

  std::optional<PlacementState> ConstructPRISAInitialPlacement() {
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

  bool CanSwapResources(const PlacementState& state, int ra, int rb) const {
    if (ra == rb || ra < 0 || rb < 0) return false;
    const int a = state.mrrg_to_dfg[ra];
    const int b = state.mrrg_to_dfg[rb];
    if (a < 0 && b < 0) return false;
    if (a >= 0 && !IsCompatible(a, rb)) return false;
    if (b >= 0 && !IsCompatible(b, ra)) return false;
    return true;
  }

  bool ApplyResourceSwap(PlacementState& state, int ra, int rb) const {
    if (!CanSwapResources(state, ra, rb)) return false;
    const int a = state.mrrg_to_dfg[ra];
    const int b = state.mrrg_to_dfg[rb];
    state.mrrg_to_dfg[ra] = b;
    state.mrrg_to_dfg[rb] = a;
    if (a >= 0) state.dfg_to_mrrg[a] = rb;
    if (b >= 0) state.dfg_to_mrrg[b] = ra;
    return true;
  }

  bool ApplyRandomResourceSwap(PlacementState& state,
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

  bool ApplyPlacementCostSampledSwap(PlacementState& state,
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

  int ResourceAfterSwap(int dfg_node_id, int first_resource,
                        int second_resource,
                        const PlacementState& state) const {
    int resource = state.dfg_to_mrrg[dfg_node_id];
    if (resource == first_resource) return second_resource;
    if (resource == second_resource) return first_resource;
    return resource;
  }

  double PlacementCostDeltaForSwap(const PlacementState& state,
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

  int PRISAWeakEdgeDeltaForSwap(const PlacementState& state,
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

  struct CostAwarePRISAMetrics {
    double mesh_hop_sum = 0.0;
    int max_mesh_hop = 0;
    int mapped_lp_mesh_hop = 0;
    int direct_edge_count = 0;
  };

  int EdgeMeshHop(const DFGEdgeInfo& edge,
                  const std::vector<int>& dfg_to_mrrg) const {
    const int source_resource = dfg_to_mrrg[edge.source];
    const int target_resource = dfg_to_mrrg[edge.target];
    if (source_resource < 0 || target_resource < 0) return kInfDistance;
    return SpatialStepDistance(source_resource, target_resource);
  }

  CostAwarePRISAMetrics ComputeCostAwarePRISAMetrics(
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

  double CostAwarePRISAScore(const PlacementState& state) const {
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

  double PRISAAcceptanceCost(const PlacementState& state) const {
    if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
    return PlacementCost(state);
  }

  double PRISABestCost(const PlacementState& state) const {
    if (UsesCostAwarePRISA()) return CostAwarePRISAScore(state);
    return PlacementCost(state);
  }

  int CostAwarePRISACandidateSampleCount() const {
    if (options_.max_trials.has_value()) {
      return std::max(16, options_.max_trials.value());
    }
    return 512;
  }

  int CostAwarePRISAFullEvaluationCount() const { return 8; }

  double CostAwarePRISALocalSwapScore(const PlacementState& state,
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

  bool ApplyCostAwarePRISAMove(PlacementState& state) {
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

  int CostAwarePRISAPolishPasses() const {
    return std::max(4, std::min(12, PRISAMaxIterations() / 125));
  }

  PlacementState PolishCostAwarePRISA(
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

  int DirectEdgeGainForSwap(const PlacementState& state, int first_resource,
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

  PlacementState PolishCostAwarePRISADirectEdges(
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

  bool ApplyPRISAMove(PlacementState& state) {
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

  std::optional<PlacementState> RunPRISA(
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
      const int da = DirectedResourceDistance(state.dfg_to_mrrg[a.source],
                                              state.dfg_to_mrrg[a.target]);
      const int db = DirectedResourceDistance(state.dfg_to_mrrg[b.source],
                                              state.dfg_to_mrrg[b.target]);
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

  int ManhattanRouteScore(int from_mrrg_node, int to_mrrg_node) const {
    if (from_mrrg_node < 0 || to_mrrg_node < 0) return kInfDistance;
    if (from_mrrg_node == to_mrrg_node) return 0;
    const auto from = mrrg_.GetNodeProperty(from_mrrg_node);
    const auto to = mrrg_.GetNodeProperty(to_mrrg_node);
    const int context_size = std::max(1, from.context_size);
    const int context_delta =
        (to.context_id - from.context_id + context_size) % context_size;
    return SpatialStepDistance(from_mrrg_node, to_mrrg_node) + context_delta;
  }

  std::optional<PathResult> FindManhattanRoutePath(
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
      if (!CanUseNode(first_to, source_dfg_node, source_mrrg, target_mrrg,
                      state, usage)) {
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

  std::optional<PathResult> FindManhattanRoutePathNonEmptyAllowed(
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
      std::stable_sort(edge_ids.begin(), edge_ids.end(),
                       [&](int lhs, int rhs) {
                         const int lto =
                             mrrg_.GetEdgeSourceTarget(lhs).second;
                         const int rto =
                             mrrg_.GetEdgeSourceTarget(rhs).second;
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
        q.push({next_steps + ManhattanRouteScore(v, target_mrrg), next_steps,
                v});
      }
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
