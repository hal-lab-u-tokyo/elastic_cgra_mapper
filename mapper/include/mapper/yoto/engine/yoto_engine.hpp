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
#include <mapper/yoto/yoto_options.hpp>
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

namespace mapper::yoto {
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
inline double DegreePenalty(int target_degree, int freedom) {
  if (freedom >= target_degree) return freedom - target_degree;
  return 4.0 * (target_degree - freedom);
}

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

constexpr double kGridImpossibleCost = 1.0e12;

struct EdgeInfo {
  int source = -1;
  int target = -1;
};

struct CPUMappingPlacementContext {
  PlacementState state;
  std::vector<int> freedom;
  std::vector<int> mutable_degree;
};

struct CPUMappingTrialResult {
  PlacementState state;
  double traversal_edge_cost = kGridImpossibleCost;
  double final_placement_cost = kGridImpossibleCost;
};

struct TrialTraceMetrics {
  double placement_cost = kGridImpossibleCost;
  double mesh_hop_sum = 0.0;
  double avg_mesh_hop = 0.0;
  int max_mesh_hop = 0;
  double mesh_optimal_edge_ratio = 0.0;
  double paper_optimal_edge_ratio = 0.0;
  double avg_mesh_fifo = 0.0;
  int max_mesh_fifo = 0;
  double p95_mesh_fifo = 0.0;
  double avg_paper_fifo = 0.0;
  int max_paper_fifo = 0;
};

enum class StepAnnotationKind { kIO, kReconvergence };

struct StepAnnotation {
  StepAnnotationKind kind = StepAnnotationKind::kReconvergence;
  int anchor_node = -1;
  int distance = 0;
};

struct Step {
  // One edge-local decision: place target near an already placed anchor.
  // Annotations are soft YOTT ranking hints, not feasibility constraints.
  int anchor = -1;
  int target = -1;
  int edge_id = -1;
  std::vector<StepAnnotation> annotations;
};

struct PaperGuidedRank {
  int annotation_priority = 3;
  double annotation_cost = 0.0;
  int locality_distance = 0;
  double degree_cost = 0.0;
  int random_order = 0;
};

class YotoEngine {
 public:
  YotoEngine(entity::DFG& dfg, entity::MRRG& mrrg,
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

  YotoEngine(entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
             const Placement2DArrayOptions& options);
  bool PlacementOnly() const {
    return grid_placement_ || options_.placement_only;
  }

 private:
  entity::DFG& dfg_;
  entity::MRRG& mrrg_;
  PlacementSearchKind search_kind_{};
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
  bool IsPhysicalThenContextLike() const;
  bool UsesPhysicalPlacementStage() const;
  bool UsesPaperTraversalPlan() const;
  bool UsesModuloFallbackCandidates() const;
  bool UsesPhysicalPeExclusivePlacement() const;
  bool UsesSeparatePlacement2DIOCells() const;
  bool UsesApproximatePlacementDistance() const;
  bool NeedsAllPairsDistances() const;
  mapper::MappingResult MakeFailureResult(
      std::chrono::steady_clock::time_point start) const;
  entity::Mapping MakePlacementOnlyMapping(
      const PlacementState& placement) const;
  void BuildAllPairsDistances();
  Annotation BuildAnnotation();
  bool CanOccupyResource(int dfg_node_id, int mrrg_node_id,
                         const PlacementState& state) const;
  std::vector<int> ClosestCompatibleResources(
      int dfg_node_id, int anchor_mrrg_node, const PlacementState& state) const;
  int ResourceFreedom(int mrrg_node_id, const PlacementState& state) const;
  double EdgePlacementCost(const DFGEdgeInfo& edge,
                           const std::vector<int>& dfg_to_mrrg) const;
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
  std::optional<int> FindSamePhysicalResourceAtContext(
      int base_resource, int dfg_node_id, int desired_context,
      const PlacementState& assigned) const;
  std::optional<PlacementState> AssignContextsToPhysicalPlacement(
      const PlacementState& physical_placement) const;
  std::optional<PlacementState> FinalizePhysicalPlacementIfNeeded(
      const PlacementState& placement) const;
  std::vector<PlacementState> ConstructGreedyPlacementCandidates();
  std::optional<PlacementState> RunGreedyMultiStart(
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
  Placement2DArrayKind kind_{};
  std::optional<int> max_trials_;
  std::optional<int> seed_count_;
  std::optional<int> random_seed_;
  std::optional<int> max_iterations_;
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
  bool GridIsCPUMappingLike() const;
  bool GridIsPaperGuidedTraversalLike() const;
  bool GridIsPaperGuidedYOTO() const;
  bool GridUsesYOTTAnnotations() const;
  bool GridUsesStructuralIOCellTypes() const;
  unsigned int GridTrialSeedFor(int seed_index, int trial_index) const;
  int GridAuthorRandomInt(int exclusive_upper_bound);
  void GridShuffleWithAuthorRandom(std::vector<int>& values);
  void GridShuffleWithAuthorRandom(std::array<std::pair<int, int>, 4>& values);
  int GridTraversalNeighborMode(bool is_yott);
  void GridResetTrialSeed(int seed_index, int trial_index);
  bool GridUsesPerTrialSeeds() const;
  bool GridTraceTrials() const;
  std::string GridTrialTracePath() const;
  void GridWriteTrialTraceHeader(std::ofstream& trace) const;
  TrialTraceMetrics GridComputeTrialTraceMetrics(
      const PlacementState& state) const;
  std::string GridPlacementSignature(const PlacementState& state) const;
  std::string GridPlacementHash(const PlacementState& state) const;
  std::string GridPlanHash(const std::vector<Step>& plan) const;
  void GridWriteTrialTraceRow(std::ofstream& trace, const std::string& phase,
                              int seed_index, int trial_index, bool success,
                              bool selected_as_best, double selection_primary,
                              double selection_secondary,
                              const std::optional<PlacementState>& state,
                              double elapsed_s,
                              const std::string& plan_hash = "") const;
  void GridRecordPlacementSwapAttempts(long long count = 1) const;
  std::string GridNodeLabel(int node) const;
  int GridPlacedCount(const PlacementState& state) const;
  int GridFreeCPUMappingCompatibleCellCount(int dfg_node,
                                            const PlacementState& state) const;
  void GridRecordFailure(const std::string& reason);
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
  bool GridCanPlaceCPUMapping(int dfg_node, int cell,
                              const PlacementState& state) const;
  int GridCellFreedom(int cell) const;
  std::vector<int> GridInitialFreedomGrid() const;
  int GridDistanceCost(int a, int b) const;
  double GridPlacementCost(const PlacementState& state) const;
  int GridBorderDistance(int cell) const;
  std::vector<int> GridCompatibleCells(int dfg_node,
                                       const PlacementState& state) const;
  void GridPlaceNode(int dfg_node, int cell, PlacementState& state) const;
  bool GridIsOutputNode(int dfg_node) const;
  bool GridIsCornerCell(int cell) const;
  std::vector<int> GridRawTraversalRoots();
  std::vector<int> GridCPUMappingTraversalRoots();
  int GridFindEdgeId(int source, int target) const;
  int GridFindLastStepTargeting(const std::vector<Step>& plan,
                                int target_node) const;
  std::vector<int> GridBuildCPUMappingInputDistanceScore() const;
  std::vector<double> GridBuildBetweennessCentralityScore() const;
  double GridPaperGuidedIOPlacementScore(int dfg_node, int cell) const;
  double GridPaperGuidedInitialPlacementScore(int dfg_node, int cell) const;
  void GridBackpropagatePaperGuidedAnnotation(std::vector<Step>& plan,
                                              int from_node, int anchor_node,
                                              StepAnnotationKind kind,
                                              int max_depth,
                                              int initial_distance = 1) const;
  int GridChoosePaperGuidedNeighbor(const std::vector<int>& candidates,
                                    bool choose_from_fanout, int mode,
                                    bool zigzag_take_back,
                                    const std::vector<int>& critical_path,
                                    const std::vector<double>& betweenness);
  std::vector<Step> GridBuildPaperGuidedTraversalPlan(bool annotate);
  int GridPaperGuidedCellFreedom(int cell, const PlacementState& state) const;
  double GridPaperGuidedDegreeMatchScore(int dfg_node, int cell,
                                         const PlacementState& state) const;
  int GridFreeCellsAtDistance(int anchor_cell, int target_distance,
                              const PlacementState& state) const;
  double GridPaperGuidedAnnotationScore(const Step& step, int candidate_cell,
                                        const PlacementState& state,
                                        StepAnnotationKind kind,
                                        bool lookahead_only) const;
  bool GridHasPaperGuidedAnnotationKind(const Step& step,
                                        StepAnnotationKind kind,
                                        bool lookahead_only) const;
  PaperGuidedRank GridPaperGuidedTraversalRank(const Step& step,
                                               int candidate_cell,
                                               const PlacementState& state,
                                               bool use_annotations,
                                               int random_order) const;
  static bool GridIsBetterPaperGuidedRank(const PaperGuidedRank& a,
                                          const PaperGuidedRank& b);
  std::vector<int> GridClosestCompatibleCells(
      int dfg_node, int anchor_cell, const PlacementState& state) const;
  int GridChoosePaperGuidedInitialCell(int dfg_node, PlacementState& state);
  bool GridPlacePaperGuidedInitialNode(int dfg_node, PlacementState& state);
  bool GridPlacePaperGuidedStep(const Step& step, PlacementState& state,
                                bool use_annotations);
  std::optional<PlacementState> GridConstructPaperGuidedTraversalPlacement();
  std::optional<PlacementState> GridRunPaperGuidedTraversalMultiStart(
      std::chrono::steady_clock::time_point start);
  int GridFindUnusedEdgeIndex(int source, int target,
                              const std::vector<char>& used_edges) const;
  int GridChooseCPUMappingZigZagNeighbor(
      const std::vector<int>& candidates, bool choose_from_fanout, int mode,
      bool zigzag_take_back, const std::vector<int>& critical_path,
      const std::vector<double>& betweenness);
  void GridAddCPUMappingCycleAnnotation(std::vector<Step>& plan, int step_index,
                                        int anchor_node, int distance) const;
  void GridApplyCPUMappingCycleAnnotations(
      std::vector<Step>& plan,
      const std::vector<std::pair<int, int>>& cycle_edges) const;
  std::vector<Step> GridBuildCPUMappingPlan();
  std::vector<int> GridCPUMappingCompatibleCells(
      int dfg_node, const PlacementState& state) const;
  std::vector<std::pair<int, int>> GridCPUMappingAllCompatibleCellPairs(
      int dfg_node, const CPUMappingPlacementContext& context) const;
  std::vector<int> GridCPUMappingIOCellsForInitialPlacement(
      int dfg_node, const PlacementState& state) const;
  int GridChooseCPUMappingInitialCell(int dfg_node,
                                      const PlacementState& state);
  std::vector<std::pair<int, int>> GridCPUMappingTipCells(
      int anchor_cell, int dfg_node,
      const CPUMappingPlacementContext& context) const;
  int GridBestCPUMappingDegreeCell(
      const std::vector<std::pair<int, int>>& cells, int dfg_node,
      const CPUMappingPlacementContext& context);
  void GridPlaceCPUMappingNode(int dfg_node, int cell,
                               CPUMappingPlacementContext& context) const;
  bool GridPlaceCPUMappingInitialNode(int dfg_node,
                                      CPUMappingPlacementContext& context);
  static std::vector<std::pair<int, int>> GridCPUMappingAdjacencyOffsets();
  bool GridTryCPUMappingAdjacency(int dfg_node, int anchor_cell,
                                  CPUMappingPlacementContext& context,
                                  bool randomize_first);
  bool GridPlaceCPUMappingNearNodeYOTO(int dfg_node, int anchor_node,
                                       CPUMappingPlacementContext& context);
  bool GridDeferCPUMappingIfNearPlacementFails(bool placed);
  bool GridPlaceCPUMappingOrientedStep(const Step& step,
                                       CPUMappingPlacementContext& context);
  void GridUpdateCPUMappingFreedomGrid(int placed_cell,
                                       std::vector<int>& freedom) const;
  CPUMappingPlacementContext GridMakeCPUMappingContext() const;
  std::optional<CPUMappingTrialResult> GridConstructCPUMappingPlacement(
      const std::vector<Step>& plan);
  std::optional<CPUMappingTrialResult> GridConstructCPUMappingPlacement();
  std::optional<PlacementState> GridRunCPUMappingMultiStart(
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
    return search_kind_ == PlacementSearchKind::kPlacement2DYOTO;
  }
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
  std::vector<int> BuildASAPContextLevels() const;
  void PushPlacementCandidate(std::vector<PlacementState>& result,
                              const std::optional<PlacementState>& placement);
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
  bool IsIONode(int dfg_node_id) const;
  bool SupportsAnyDFGOperation(int mrrg_node_id) const;
  int ResourceDistance(int from_mrrg_node, int to_mrrg_node) const;
  int DirectedResourceDistance(int from_mrrg_node, int to_mrrg_node) const;
  int SpatialStepDistance(int from_mrrg_node, int to_mrrg_node) const;
  int ApproximateDirectedResourceDistance(int from_mrrg_node,
                                          int to_mrrg_node) const;
  int PhysicalDistance(int from_mrrg_node, int to_mrrg_node) const;
  int BorderDistance(int mrrg_node_id) const;
  bool IsCornerResource(int mrrg_node_id) const;
  double IOPlacementScore(int dfg_node_id, int mrrg_node_id) const;
  double Placement2DInitialPlacementScore(int dfg_node_id,
                                          int mrrg_node_id) const;
  int ManhattanRouteScore(int from_mrrg_node, int to_mrrg_node) const;
};

}  // namespace mapper::yoto
