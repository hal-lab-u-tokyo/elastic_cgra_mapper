#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <mapper/detail/placement_search_engine.hpp>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mapper::detail::prisa_context {

constexpr int kInfDistance = 1 << 28;
constexpr double kImpossibleCost = 1.0e8;

using mapper::detail::PlacementSearchKind;
using mapper::detail::PlacementSearchOptions;

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

struct Annotation {
  std::vector<int> in_degree;
  std::vector<int> out_degree;
  std::vector<int> degree;
};

double SecondsSince(std::chrono::steady_clock::time_point start);
bool SupportsOperation(const entity::MRRGNodeProperty& node_property,
                       entity::OpType op);
int DirectedDistanceCost(int distance);
std::vector<DFGEdgeInfo> BuildDFGEdges(entity::DFG& dfg);
std::vector<std::vector<int>> BuildSuccessors(entity::DFG& dfg);
std::vector<std::vector<int>> BuildPredecessors(entity::DFG& dfg);
std::vector<std::vector<int>> BuildIncidentEdgeIds(
    int node_num, const std::vector<DFGEdgeInfo>& edges);

class ModuloPRISAEngine {
 public:
  ModuloPRISAEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                    PlacementSearchKind search_kind, double timeout_s,
                    const std::optional<std::string>& log_file_path,
                    const PlacementSearchOptions& options);

  std::chrono::steady_clock::time_point StartSearch();
  std::optional<PlacementState> SearchPlacement(
      std::chrono::steady_clock::time_point start);
  bool PlacementOnly() const;
  mapper::MappingResult Failure(
      std::chrono::steady_clock::time_point start) const;
  mapper::MappingResult FinishPlacementOnly(
      const PlacementState& placement,
      std::chrono::steady_clock::time_point start);
  std::optional<RouteUsage> RoutePlacement(const PlacementState& placement);
  mapper::MappingResult FinishRoutedMapping(
      const PlacementState& placement, RouteUsage& route_usage,
      std::chrono::steady_clock::time_point start);

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
  std::vector<int> placement2d_resources_;
  std::vector<int> placement2d_resource_order_index_;
  std::vector<int> prisa_resources_;
  std::vector<int> prisa_resource_order_index_;
  std::vector<int> prisa_distance_matrix_;
  std::vector<char> prisa_weak_region_matrix_;
  int prisa_weak_distance_threshold_ = kInfDistance;
  Annotation annotation_;

  // options
  std::string MapperName() const;
  bool IsPRISALike() const;
  bool UsesPRISASIS() const;
  bool UsesManhattanRouting() const;
  bool IsPhysicalThenContextLike() const;
  bool UsesPhysicalPlacementStage() const;
  bool UsesPhysicalPeExclusivePlacement() const;
  bool UsesSeparatePlacement2DIOCells() const;
  bool UsesApproximatePlacementDistance() const;
  bool NeedsAllPairsDistances() const;
  int SeedCount() const;
  int MaxTrials() const;
  int RoutingRetryCount() const;
  int MaxIterations() const;
  unsigned int SeedFor(int seed_index) const;
  void ResetSeed(int seed_index);
  void Log(const std::string& message) const;
  mapper::MappingResult MakeFailureResult(
      std::chrono::steady_clock::time_point start) const;
  entity::Mapping MakePlacementOnlyMapping(
      const PlacementState& placement) const;

  // resources
  void BuildMRRGCache();
  void BuildAllPairsDistances();
  Annotation BuildAnnotation();
  bool HasTimedOut(std::chrono::steady_clock::time_point start,
                   double reserve_s = 0.02) const;
  bool IsCompatible(int dfg_node_id, int mrrg_node_id) const;
  bool SamePhysicalPosition(int lhs_mrrg_node, int rhs_mrrg_node) const;
  bool IsPhysicalPositionOccupied(int mrrg_node_id,
                                  const PlacementState& state) const;
  bool CanOccupyResource(int dfg_node_id, int mrrg_node_id,
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

  // quality
  double ResourcePairPlacementCost(int from_mrrg_node, int to_mrrg_node) const;
  double EdgePlacementCost(const DFGEdgeInfo& edge,
                           const std::vector<int>& dfg_to_mrrg) const;
  double PlacementCost(const PlacementState& state) const;
  bool PRISAEdgeIsWeak(int source_resource, int target_resource,
                       int resource_count) const;
  int PRISAWeakEdgeCount(const PlacementState& state) const;
  std::optional<PlacementState> RandomLegalPlacement();

  int OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const;
  std::vector<int> BuildASAPContextLevels() const;
  std::optional<int> FindSamePhysicalResourceAtContext(
      int base_resource, int dfg_node_id, int desired_context,
      const PlacementState& assigned) const;
  std::optional<PlacementState> AssignContextsToPhysicalPlacement(
      const PlacementState& physical_placement) const;
  std::optional<PlacementState> FinalizePhysicalPlacementIfNeeded(
      const PlacementState& placement) const;

  // prisa
  std::optional<PlacementState> RunPRISAMultiSeed(
      std::chrono::steady_clock::time_point start);
  int PRISAMaxIterations() const;
  const std::vector<int>& Placement2DResourceOrder() const;
  const std::vector<int>& ResourceOrderIndex() const;
  int PRISAResourceSideLength() const;
  int PRISAWeakPairCount(int resource_count) const;
  void BuildPRISADistanceRegions();
  bool IsPRISAWeakRegion(int row, int column, int resource_count) const;
  bool IsPRISAPotentialRegion(int row, int column, int resource_count) const;
  std::vector<int> LowBandwidthNodeOrder();
  std::optional<PlacementState> ConstructPRISAInitialPlacementFromOrder(
      const std::vector<int>& node_order);
  int PRISAInitialSolutionSampleCount() const;
  std::optional<PlacementState> ConstructPRISAInitialPlacement();
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
  double CostAwarePRISAScore(const PlacementState& state) const;
  double PRISAAcceptanceCost(const PlacementState& state) const;
  double PRISABestCost(const PlacementState& state) const;
  bool ApplyPRISAMove(PlacementState& state);
  std::optional<PlacementState> RunPRISA(
      std::chrono::steady_clock::time_point start);

  // routing
  std::optional<RouteUsage> TryRoutePlacement(const PlacementState& state);
  std::optional<RouteUsage> TryRoutePlacementInOrder(
      const PlacementState& state,
      const std::vector<DFGEdgeInfo>& route_order) const;
  std::optional<PathResult> FindRoutePath(int source_dfg_node, int source_mrrg,
                                          int target_mrrg,
                                          const PlacementState& state,
                                          const RouteUsage& usage) const;
  int ManhattanRouteScore(int from_mrrg_node, int to_mrrg_node) const;
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
};

}  // namespace mapper::detail::prisa_context
