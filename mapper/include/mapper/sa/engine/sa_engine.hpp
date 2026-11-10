#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <mapper/sa/sa_options.hpp>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mapper::sa {

inline double SecondsSince(std::chrono::steady_clock::time_point start) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now - start)
      .count();
}
inline bool SupportsOperation(const entity::MRRGNodeProperty& node_property,
                              entity::OpType op) {
  return std::find(node_property.supported_operations.begin(),
                   node_property.supported_operations.end(),
                   op) != node_property.supported_operations.end();
}
inline int DirectedDistanceCost(int distance) {
  return distance >= (1 << 28) ? (1 << 28) : std::max(1, distance);
}

constexpr double kGridImpossibleCost = 1.0e12;

struct EdgeInfo {
  int source = -1;
  int target = -1;
};

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
  std::vector<int> dfg_to_cell;
  std::vector<int> cell_to_dfg;
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

std::vector<DFGEdgeInfo> BuildDFGEdges(entity::DFG& dfg);
std::vector<std::vector<int>> BuildSuccessors(entity::DFG& dfg);
std::vector<std::vector<int>> BuildPredecessors(entity::DFG& dfg);
std::vector<std::vector<int>> BuildIncidentEdgeIds(
    int node_num, const std::vector<DFGEdgeInfo>& edges);

class SaEngine {
 public:
  SaEngine(entity::DFG& dfg, entity::MRRG& mrrg,
           PlacementSearchKind search_kind, double timeout_s,
           const std::optional<std::string>& log_file_path,
           const PlacementSearchOptions& options);

  std::chrono::steady_clock::time_point StartSearch();
  std::optional<PlacementState> SearchPlacement(
      std::chrono::steady_clock::time_point start);
  mapper::MappingResult Failure(
      std::chrono::steady_clock::time_point start) const;
  mapper::MappingResult FinishPlacementOnly(
      const PlacementState& placement,
      std::chrono::steady_clock::time_point start);
  std::optional<RouteUsage> RoutePlacement(const PlacementState& placement);
  mapper::MappingResult FinishRoutedMapping(
      const PlacementState& placement, RouteUsage& route_usage,
      std::chrono::steady_clock::time_point start);

  SaEngine(entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
           const Placement2DArrayOptions& options);
  bool PlacementOnly() const {
    return grid_placement_ || options_.placement_only;
  }

 private:
  entity::DFG& dfg_;
  entity::MRRG& mrrg_;
  PlacementSearchKind search_kind_ = PlacementSearchKind::kSA;
  double timeout_s_;
  std::optional<std::string> log_file_path_;
  PlacementSearchOptions options_;
  std::vector<DFGEdgeInfo> dfg_edges_;
  std::vector<std::vector<int>> incident_edge_ids_;
  std::vector<std::vector<int>> successors_;
  std::vector<std::vector<int>> predecessors_;
  std::vector<std::vector<int>> distance_;
  std::vector<int> placement2d_resources_;
  std::vector<int> placement2d_resource_order_index_;
  std::vector<int> prisa_resources_;
  std::vector<int> prisa_resource_order_index_;
  Annotation annotation_;
  std::string MapperName() const;
  mapper::MappingResult MakeFailureResult(
      std::chrono::steady_clock::time_point start) const;
  entity::Mapping MakePlacementOnlyMapping(
      const PlacementState& placement) const;
  void BuildAllPairsDistances();
  Annotation BuildAnnotation();
  bool CanOccupyResource(int dfg_node_id, int mrrg_node_id,
                         const PlacementState& state) const;
  int ResourceFreedom(int mrrg_node_id, const PlacementState& state) const;
  double EdgePlacementCost(const DFGEdgeInfo& edge,
                           const std::vector<int>& dfg_to_mrrg) const;
  std::optional<PlacementState> RandomLegalPlacement();
  int OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const;
  int FindLastStepTargeting(const TraversalPlan& plan, int target_node) const;
  void BackpropagateAnnotation(TraversalPlan& plan, int from_node,
                               int anchor_node, SequenceAnnotationKind kind,
                               int max_depth, int initial_distance = 1) const;
  std::vector<double> BuildBetweennessCentralityScore() const;
  TraversalPlan BuildPlacement2DTraversalPlan(bool annotate);
  TraversalPlan BuildTraversalPlan(bool annotate);
  std::optional<int> PlaceInitialNode(int dfg_node_id, PlacementState& state);
  int FreeResourcesAtDistance(int anchor_mrrg_node, int target_distance,
                              const PlacementState& state) const;
  double AnnotationScore(const TraversalStep& step, int candidate_mrrg_node,
                         const PlacementState& state,
                         SequenceAnnotationKind kind,
                         bool lookahead_only) const;
  bool HasAnnotationKind(const TraversalStep& step, SequenceAnnotationKind kind,
                         bool lookahead_only) const;
  CandidateRank TraversalPlacementRank(const TraversalStep& step,
                                       int candidate_mrrg_node,
                                       const PlacementState& state,
                                       bool use_annotations,
                                       int random_order) const;
  bool PlaceTraversalStep(const TraversalStep& step, PlacementState& state,
                          bool use_annotations);
  std::optional<PlacementState> ConstructTraversalPlacement(
      bool use_paper_traversal_plan, bool use_annotations);
  std::optional<PlacementState> ConstructGreedyPlacement();
  std::optional<PlacementState> RunSAMultiSeed(
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunSA(
      std::chrono::steady_clock::time_point start);
  std::optional<RouteUsage> TryRoutePlacement(const PlacementState& state);
  std::optional<RouteUsage> TryRoutePlacementInOrder(
      const PlacementState& state,
      const std::vector<DFGEdgeInfo>& route_order) const;
  std::optional<PathResult> FindRoutePath(int source_dfg_node, int source_mrrg,
                                          int target_mrrg,
                                          const PlacementState& state,
                                          const RouteUsage& usage) const;
  std::optional<PathResult> FindManhattanRoutePath(
      int source_dfg_node, int source_mrrg, int target_mrrg,
      const PlacementState& state, const RouteUsage& usage) const;
  std::optional<PathResult> FindManhattanRoutePathNonEmptyAllowed(
      int source_dfg_node, int source_mrrg, int target_mrrg,
      const PlacementState& state, const RouteUsage& usage,
      bool forbid_zero_length) const;
  std::optional<PathResult> FindRoutePathNonEmptyAllowed(
      int source_dfg_node, int source_mrrg, int target_mrrg,
      const PlacementState& state, const RouteUsage& usage,
      bool forbid_zero_length) const;
  bool CanUseEdge(int edge_id, int source_dfg_node,
                  const RouteUsage& usage) const;
  bool CanUseNode(int mrrg_node_id, int source_dfg_node, int source_mrrg,
                  int target_mrrg, const PlacementState& state,
                  const RouteUsage& usage) const;
  void ReserveRoute(int source_dfg_node, int source_mrrg, int target_mrrg,
                    const PathResult& path, const PlacementState& state,
                    RouteUsage& usage) const;
  int RouteEdgeCount(const RouteUsage& usage) const;
  std::optional<PlacementState> RunPlacement2DSA(
      std::chrono::steady_clock::time_point start);
  mapper::sa::Placement2DArrayKind kind_ = Placement2DArrayKind::kSA;
  std::optional<int> elite_placement_count_;
  std::string io_node_policy_ = "opcode";
  std::string trial_seed_policy_ = "continuous";
  std::string traversal_order_policy_ = "zigzag";
  std::string traversal_neighbor_policy_ = "default";
  std::string candidate_scope_policy_ = "default";
  std::string candidate_rank_policy_ = "default";
  std::optional<bool> use_yott_annotations_;
  std::optional<bool> trace_trials_;
  std::string last_failure_reason_;
  entity::MRRGConfig config_;
  unsigned int base_seed_;
  std::mt19937 rng_;
  int rows_ = 0;
  int cols_ = 0;
  std::vector<int> cell_to_mrrg_;
  std::vector<std::vector<entity::OpType>> cell_ops_;
  std::vector<bool> cell_memory_accessible_;
  std::vector<EdgeInfo> edges_;
  std::vector<int> degree_;
  std::vector<int> critical_path_score_;
  int critical_path_max_ = 0;
  std::vector<std::vector<int>> compatible_cells_;
  std::vector<int> initial_freedom_;
  bool separate_io_cells_ = false;
  long long cell_visits_ = 0;
  mutable long long placement_swap_attempts_ = 0;
  std::string GridMapperName() const;
  bool GridUsesStructuralIOCellTypes() const;
  std::string GridNodeLabel(int node) const;
  void GridBuildDFGCache();
  void GridBuildGridCache();
  void GridBuildCompatibilityCache();
  static std::string GridNormalizePolicy(std::string policy);
  int GridCell(int row, int col) const;
  int GridRow(int cell) const;
  int GridCol(int cell) const;
  bool GridIsCompatible(int dfg_node, int cell) const;
  bool GridIsIONode(int dfg_node) const;
  bool GridCanPlace(int dfg_node, int cell, const PlacementState& state) const;
  int GridCPUMappingCellType(int cell) const;
  bool GridIsCPUMappingCompatible(int dfg_node, int cell) const;
  int GridCellFreedom(int cell) const;
  int GridDistanceCost(int a, int b) const;
  double GridPlacementCost(const PlacementState& state) const;
  std::vector<int> GridCompatibleCells(int dfg_node,
                                       const PlacementState& state) const;
  void GridPlaceNode(int dfg_node, int cell, PlacementState& state) const;
  std::optional<PlacementState> GridRandomPlacement();
  bool GridApplySwap(PlacementState& state, int a, int b);
  bool GridApplyMoveToFreeCell(PlacementState& state, int node);
  std::optional<PlacementState> GridRunSA(
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> GridRunSAMultiSeed(
      std::chrono::steady_clock::time_point start);
  std::chrono::steady_clock::time_point GridStartSearch();
  std::optional<PlacementState> GridSearchPlacement(
      std::chrono::steady_clock::time_point start);
  mapper::MappingResult GridFailure(
      std::chrono::steady_clock::time_point start) const;
  mapper::MappingResult GridFinishPlacement(
      const PlacementState& placement,
      std::chrono::steady_clock::time_point start);
  bool grid_placement_ = false;
  bool UsesSpatialResources() const {
    return search_kind_ == PlacementSearchKind::kPlacement2DSA;
  }

  bool ApplyRandomMove(PlacementState& state);
  bool ApplySwapMove(PlacementState& state, int a, int b) const;
  bool ApplyMoveToFreeResource(PlacementState& state);
  double DegreeMatchScore(int dfg_node_id, int mrrg_node_id,
                          const PlacementState& state) const;
  std::vector<int> GetRawOutputTraversalRoots();
  std::vector<int> GetOutputTraversalRoots();
  int FindDFGEdgeId(int source, int target) const;
  std::vector<int> UnvisitedIncidentEdges(
      int dfg_node_id, const std::vector<char>& visited_edges) const;
  int PickTraversalEdge(int dfg_node_id, const std::vector<char>& visited_nodes,
                        const std::vector<char>& visited_edges);
  std::vector<int> BuildCriticalPathScore() const;
  int ChoosePlacement2DNeighbor(const std::vector<int>& candidates,
                                bool choose_from_fanout, int mode,
                                bool zigzag_take_back,
                                const std::vector<int>& critical_path,
                                const std::vector<double>& betweenness);
  bool UsesSeparatePlacement2DIOCells() const;
  int SeedCount() const;
  int MaxTrials() const;
  int RoutingRetryCount() const;
  int MaxIterations() const;
  unsigned int SeedFor(int seed_index) const;
  void ResetSeed(int seed_index);
  void Log(const std::string& message) const;
  double ResourcePairPlacementCost(int from_mrrg_node, int to_mrrg_node) const;
  double PlacementCost(const PlacementState& state) const;
  void BuildMRRGCache();
  bool HasTimedOut(std::chrono::steady_clock::time_point start,
                   double reserve_s = -1.0) const;
  bool IsCompatible(int dfg_node_id, int mrrg_node_id) const;
  bool SamePhysicalPosition(int lhs_mrrg_node, int rhs_mrrg_node) const;
  bool IsPhysicalPositionOccupied(int mrrg_node_id,
                                  const PlacementState& state) const;
  std::vector<int> CompatibleResources(int dfg_node_id,
                                       const PlacementState& state) const;
  std::vector<int> ClosestCompatibleResources(
      int dfg_node_id, int anchor_mrrg_node, const PlacementState& state) const;
  bool IsIONode(int dfg_node_id) const;
  bool SupportsAnyDFGOperation(int mrrg_node_id) const;
  int ResourceDistance(int from_mrrg_node, int to_mrrg_node) const;
  int DirectedResourceDistance(int from_mrrg_node, int to_mrrg_node) const;
  int SpatialStepDistance(int from_mrrg_node, int to_mrrg_node) const;
  int ApproximateDirectedResourceDistance(int from_mrrg_node,
                                          int to_mrrg_node) const;
  int PhysicalDistance(int from_mrrg_node, int to_mrrg_node) const;
  int BorderDistance(int mrrg_node_id) const;
  double IOPlacementScore(int dfg_node_id, int mrrg_node_id) const;
  int ManhattanRouteScore(int from_mrrg_node, int to_mrrg_node) const;
};

}  // namespace mapper::sa
