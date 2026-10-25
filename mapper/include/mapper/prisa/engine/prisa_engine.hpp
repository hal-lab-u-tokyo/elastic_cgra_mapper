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
#include <mapper/prisa/prisa_options.hpp>
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

namespace mapper::prisa {
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

struct Annotation {
  std::vector<int> in_degree;
  std::vector<int> out_degree;
  std::vector<int> degree;
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

struct PRISAStats {
  std::vector<int> row_weak;
  std::vector<int> column_weak;
  std::vector<int> row_potential;
  std::vector<int> column_potential;
  std::vector<char> edge_weak;
  std::vector<char> edge_potential;
  int weak_edges = 0;
};

struct PlacementQuality {
  double cost = kGridImpossibleCost;
  int direct_edges = 0;
  int max_distance = 0;
};

struct GridCostAwarePRISAMetrics {
  double mesh_hop_sum = 0.0;
  int max_mesh_hop = 0;
  int mapped_lp_mesh_hop = 0;
  int direct_edge_count = 0;
};

class PrisaEngine {
 public:
  PrisaEngine(entity::DFG& dfg, entity::MRRG& mrrg,
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

  PrisaEngine(entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
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
  std::vector<int> prisa_distance_matrix_;
  std::vector<char> prisa_weak_region_matrix_;
  int prisa_weak_distance_threshold_ = kInfDistance;
  Annotation annotation_;
  std::string MapperName() const;
  bool IsPRISALike() const;
  bool UsesPRISASIS() const;
  bool UsesManhattanRouting() const;
  bool IsPhysicalThenContextLike() const;
  bool UsesPhysicalPlacementStage() const;
  bool UsesPhysicalPeExclusivePlacement() const;
  bool UsesSeparatePlacement2DIOCells() const;
  bool UsesApproximatePlacementDistance() const;
  mapper::MappingResult MakeFailureResult(
      std::chrono::steady_clock::time_point start) const;
  entity::Mapping MakePlacementOnlyMapping(
      const PlacementState& placement) const;
  Annotation BuildAnnotation();
  bool CanOccupyResource(int dfg_node_id, int mrrg_node_id,
                         const PlacementState& state) const;
  double EdgePlacementCost(const DFGEdgeInfo& edge,
                           const std::vector<int>& dfg_to_mrrg) const;
  std::optional<PlacementState> RandomLegalPlacement();
  int OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const;
  std::optional<int> FindSamePhysicalResourceAtContext(
      int base_resource, int dfg_node_id, int desired_context,
      const PlacementState& assigned) const;
  std::optional<PlacementState> AssignContextsToPhysicalPlacement(
      const PlacementState& physical_placement) const;
  std::optional<PlacementState> FinalizePhysicalPlacementIfNeeded(
      const PlacementState& placement) const;
  std::optional<PlacementState> RunPRISAMultiSeed(
      std::chrono::steady_clock::time_point start);
  const std::vector<int>& Placement2DResourceOrder() const;
  const std::vector<int>& ResourceOrderIndex() const;
  std::optional<PlacementState> ConstructPRISAInitialPlacementFromOrder(
      const std::vector<int>& node_order);
  std::optional<PlacementState> ConstructPRISAInitialPlacement();
  struct CostAwarePRISAMetrics {
    double mesh_hop_sum = 0.0;
    int max_mesh_hop = 0;
    int mapped_lp_mesh_hop = 0;
    int direct_edge_count = 0;
  };
  int EdgeMeshHop(const DFGEdgeInfo& edge,
                  const std::vector<int>& dfg_to_mrrg) const;
  CostAwarePRISAMetrics ComputeCostAwarePRISAMetrics(
      const PlacementState& state) const;
  double PRISAAcceptanceCost(const PlacementState& state) const;
  double PRISABestCost(const PlacementState& state) const;
  bool ApplyPRISAMove(PlacementState& state);
  std::optional<PlacementState> RunPRISA(
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
  bool UsesCostAwarePRISA() const;
  int CostAwarePRISACandidateSampleCount() const;
  int CostAwarePRISAFullEvaluationCount() const;
  double CostAwarePRISALocalSwapScore(const PlacementState& state,
                                      int first_resource,
                                      int second_resource) const;
  bool ApplyCostAwarePRISAMove(PlacementState& state);
  int CostAwarePRISAPolishPasses() const;
  PlacementState PolishCostAwarePRISA(
      PlacementState state, std::chrono::steady_clock::time_point start);
  int DirectEdgeGainForSwap(const PlacementState& state, int first_resource,
                            int second_resource) const;
  PlacementState PolishCostAwarePRISADirectEdges(
      PlacementState state, std::chrono::steady_clock::time_point start);
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
  std::vector<int> prisa_cell_order_;
  std::vector<int> prisa_order_index_;
  bool separate_io_cells_ = false;
  long long cell_visits_ = 0;
  mutable long long placement_swap_attempts_ = 0;
  std::string GridMapperName() const;
  bool GridUsesPRISASIS() const;
  bool GridUsesCostAwarePRISA() const;
  bool GridUsesStructuralIOCellTypes() const;
  std::string GridNodeLabel(int node) const;
  void GridBuildDFGCache();
  void GridBuildGridCache();
  void GridBuildCompatibilityCache();
  int GridPRISAResourceSideLength() const;
  int GridPRISAWeakPairCount(int resource_count) const;
  void GridBuildPRISACache();
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
  PlacementQuality GridComputePlacementQuality(
      const PlacementState& state) const;
  bool GridIsBetterQuality(const PlacementQuality& candidate,
                           const PlacementQuality& best) const;
  int GridPRISAWeakEdgeCount(const PlacementState& state) const;
  GridCostAwarePRISAMetrics GridComputeCostAwarePRISAMetrics(
      const PlacementState& state) const;
  double GridCostAwarePRISAScore(const PlacementState& state) const;
  double GridPRISAAcceptanceScore(const PlacementState& state) const;
  std::vector<int> GridCompatibleCells(int dfg_node,
                                       const PlacementState& state) const;
  void GridPlaceNode(int dfg_node, int cell, PlacementState& state) const;
  std::vector<int> GridBuildCriticalPathScore() const;
  std::optional<PlacementState> GridRandomPlacement();
  int GridOtherEndpoint(int edge_id, int node) const;
  std::vector<int> GridLowBandwidthNodeOrder();
  std::optional<PlacementState> GridConstructPRISAInitialPlacementFromOrder(
      const std::vector<int>& node_order);
  std::optional<PlacementState> GridConstructPRISAInitialPlacement();
  bool GridPRISAIsWeak(int row, int column) const;
  bool GridPRISAIsPotential(int row, int column) const;
  bool GridPRISAEdgeRegion(const PlacementState& state, int edge_id,
                           bool* is_weak, bool* is_potential) const;
  void GridAddPRISAEdgeToStats(const PlacementState& state, int edge_id,
                               PRISAStats& stats, int sign) const;
  PRISAStats GridInitializePRISAStats(const PlacementState& state) const;
  std::vector<int> GridAffectedEdgesForCellPair(const PlacementState& state,
                                                int cell_a, int cell_b) const;
  int GridCellAfterSwap(int cell, int first_cell, int second_cell) const;
  int GridEdgeWeakAfterSwap(const PlacementState& state, int edge_id,
                            int first_cell, int second_cell) const;
  int GridPRISAWeakDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  double GridPlacementCostDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  int GridDirectEdgeDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  int GridAffectedMaxDistanceAfterCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  bool GridCanSwapCells(const PlacementState& state, int first_cell,
                        int second_cell) const;
  bool GridApplyCellSwapWithStats(PlacementState& state, PRISAStats& stats,
                                  int first_cell, int second_cell) const;
  bool GridApplyCellSwap(PlacementState& state, int first_cell,
                         int second_cell) const;
  struct PRISAProposal {
    int first_cell = -1;
    int second_cell = -1;
    double cost_delta = 0.0;
    int direct_delta = 0;
    int affected_max_after = 0;
  };
  std::optional<PRISAProposal> GridProposeRandomPRISASwap(
      const PlacementState& state);
  std::optional<PRISAProposal> GridProposePlacementCostSampledSwap(
      const PlacementState& state, int requested_sample_count = -1);
  int GridPRISALightQualitySampleCount() const;
  double GridCostAwarePRISALocalSwapScore(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  int GridCostAwarePRISACandidateSampleCount() const;
  int GridCostAwarePRISAFullEvaluationCount() const;
  std::optional<PRISAProposal> GridProposeCostAwarePRISASwap(
      const PlacementState& state);
  std::vector<int> GridPRISAWeakCounterparts(const PlacementState& state,
                                             const PRISAStats& stats,
                                             int first_index,
                                             bool row_side) const;
  struct PRISACandidate {
    int first_index = -1;
    int second_index = -1;
    int fixed_weak_count = 0;
    int potential_count = 0;
    int weak_delta = 0;
    double cost_delta = 0.0;
    int direct_delta = 0;
    int affected_max_after = 0;
    int spatial_distance = 0;
    int random_order = 0;
  };
  bool GridIsBetterPRISACandidate(const PRISACandidate& candidate,
                                  const PRISACandidate& best) const;
  std::optional<PRISACandidate> GridBestPRISACandidateFor(
      const PlacementState& state, const PRISAStats& stats, int first_index,
      bool row_side);
  int GridRandomArgmax(const std::vector<int>& values);
  std::optional<PRISAProposal> GridProposePRISAMove(const PlacementState& state,
                                                    const PRISAStats& stats);
  int GridPRISAMaxIterations() const;
  int GridPRISAPolishPasses() const;
  PlacementState GridPolishPRISAQuality(
      PlacementState state, std::chrono::steady_clock::time_point start);
  int GridDirectEdgeGainForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  PlacementState GridPolishCostAwarePRISADirectEdges(
      PlacementState state, std::chrono::steady_clock::time_point start);
  bool GridIsCostAwareParetoImprovement(
      const GridCostAwarePRISAMetrics& candidate,
      const GridCostAwarePRISAMetrics& current) const;
  bool GridIsBetterCostAwareParetoCandidate(
      const GridCostAwarePRISAMetrics& candidate,
      const GridCostAwarePRISAMetrics& best) const;
  PlacementState GridPolishCostAwarePRISAPareto(
      PlacementState state, std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> GridRunPRISA(
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> GridRunPRISAMultiSeed(
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
    return search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
           search_kind_ == PlacementSearchKind::kPlacement2DPRISANoSIS ||
           search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
  }
  int PRISAMaxIterations() const;
  int PRISAResourceSideLength() const;
  int PRISAWeakPairCount(int resource_count) const;
  void BuildPRISADistanceRegions();
  bool IsPRISAWeakRegion(int row, int column, int resource_count) const;
  bool IsPRISAPotentialRegion(int row, int column, int resource_count) const;
  std::vector<int> LowBandwidthNodeOrder();
  int PRISAInitialSolutionSampleCount() const;
  bool CanSwapResources(const PlacementState& state, int ra, int rb) const;
  bool ApplyResourceSwap(PlacementState& state, int ra, int rb) const;
  bool ApplyRandomResourceSwap(PlacementState& state,
                               const std::vector<int>& resources);
  bool ApplyPlacementCostSampledSwap(PlacementState& state,
                                     const std::vector<int>& resources);
  int ResourceAfterSwap(int dfg_node_id, int first_resource,
                        int second_resource, const PlacementState& state) const;
  double PlacementCostDeltaForSwap(const PlacementState& state,
                                   int first_resource,
                                   int second_resource) const;
  int PRISAWeakEdgeDeltaForSwap(const PlacementState& state, int first_resource,
                                int second_resource) const;
  double CostAwarePRISAScore(const PlacementState& state) const;
  std::vector<int> BuildASAPContextLevels() const;
  bool NeedsAllPairsDistances() const;
  int SeedCount() const;
  int MaxTrials() const;
  int RoutingRetryCount() const;
  int MaxIterations() const;
  unsigned int SeedFor(int seed_index) const;
  void ResetSeed(int seed_index);
  void Log(const std::string& message) const;
  double ResourcePairPlacementCost(int from_mrrg_node, int to_mrrg_node) const;
  double PlacementCost(const PlacementState& state) const;
  bool PRISAEdgeIsWeak(int source_resource, int target_resource,
                       int resource_count) const;
  int PRISAWeakEdgeCount(const PlacementState& state) const;
  void BuildMRRGCache();
  void BuildAllPairsDistances();
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
  int ManhattanRouteScore(int from_mrrg_node, int to_mrrg_node) const;
};

}  // namespace mapper::prisa
