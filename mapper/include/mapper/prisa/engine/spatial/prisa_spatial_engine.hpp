#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <mapper/prisa/engine/prisa_engine_operations.hpp>
#include <mapper/prisa/prisa_options.hpp>
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

namespace mapper::prisa_spatial {

constexpr int kInfDistance = 1 << 28;
constexpr double kImpossibleCost = 1.0e8;

using mapper::prisa::PlacementSearchKind;
using mapper::prisa::PlacementSearchOptions;

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

using mapper::prisa::DirectedDistanceCost;
using mapper::prisa::SecondsSince;
using mapper::prisa::SupportsOperation;
std::vector<DFGEdgeInfo> BuildDFGEdges(entity::DFG& dfg);
std::vector<std::vector<int>> BuildSuccessors(entity::DFG& dfg);
std::vector<std::vector<int>> BuildPredecessors(entity::DFG& dfg);
std::vector<std::vector<int>> BuildIncidentEdgeIds(
    int node_num, const std::vector<DFGEdgeInfo>& edges);

class Placement2DPRISAEngine
    : public mapper::prisa::PrisaEngineOperations<Placement2DPRISAEngine> {
  template <typename>
  friend class mapper::prisa::PrisaEngineOperations;

 public:
  Placement2DPRISAEngine(entity::DFG& dfg, entity::MRRG& mrrg,
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
  bool UsesCostAwarePRISA() const;
  bool UsesPhysicalPeExclusivePlacement() const;
  bool UsesSeparatePlacement2DIOCells() const;
  bool UsesApproximatePlacementDistance() const;
  mapper::MappingResult MakeFailureResult(
      std::chrono::steady_clock::time_point start) const;
  entity::Mapping MakePlacementOnlyMapping(
      const PlacementState& placement) const;

  // resources
  Annotation BuildAnnotation();
  bool CanOccupyResource(int dfg_node_id, int mrrg_node_id,
                         const PlacementState& state) const;

  // quality
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

  // prisa
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

}  // namespace mapper::prisa_spatial
