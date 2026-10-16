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

namespace mapper::detail::sa_spatial {

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

double SecondsSince(std::chrono::steady_clock::time_point start);
bool SupportsOperation(const entity::MRRGNodeProperty& node_property,
                       entity::OpType op);
int DirectedDistanceCost(int distance);
std::vector<DFGEdgeInfo> BuildDFGEdges(entity::DFG& dfg);
std::vector<std::vector<int>> BuildSuccessors(entity::DFG& dfg);
std::vector<std::vector<int>> BuildPredecessors(entity::DFG& dfg);
std::vector<std::vector<int>> BuildIncidentEdgeIds(
    int node_num, const std::vector<DFGEdgeInfo>& edges);

class Placement2DSAEngine {
 public:
  Placement2DSAEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                      PlacementSearchKind search_kind, double timeout_s,
                      const std::optional<std::string>& log_file_path,
                      const PlacementSearchOptions& options);

  mapper::MappingResult Run();

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
  Annotation annotation_;

  // options
  std::string MapperName() const;
  bool UsesSeparatePlacement2DIOCells() const;
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
  int ResourceFreedom(int mrrg_node_id, const PlacementState& state) const;
  int BorderDistance(int mrrg_node_id) const;
  double IOPlacementScore(int dfg_node_id, int mrrg_node_id) const;

  // quality
  double ResourcePairPlacementCost(int from_mrrg_node, int to_mrrg_node) const;
  double EdgePlacementCost(const DFGEdgeInfo& edge,
                           const std::vector<int>& dfg_to_mrrg) const;
  double PlacementCost(const PlacementState& state) const;
  std::optional<PlacementState> RandomLegalPlacement();

  // traversal
  std::vector<int> GetRawOutputTraversalRoots();
  std::vector<int> GetOutputTraversalRoots();
  int OtherEndpoint(const DFGEdgeInfo& edge, int node_id) const;
  int FindDFGEdgeId(int source, int target) const;
  std::vector<int> UnvisitedIncidentEdges(
      int dfg_node_id, const std::vector<char>& visited_edges) const;
  int PickTraversalEdge(int dfg_node_id, const std::vector<char>& visited_nodes,
                        const std::vector<char>& visited_edges);
  int FindLastStepTargeting(const TraversalPlan& plan, int target_node) const;
  void BackpropagateAnnotation(TraversalPlan& plan, int from_node,
                               int anchor_node, SequenceAnnotationKind kind,
                               int max_depth, int initial_distance = 1) const;
  std::vector<int> BuildCriticalPathScore() const;
  std::vector<double> BuildBetweennessCentralityScore() const;
  int ChoosePlacement2DNeighbor(const std::vector<int>& candidates,
                                bool choose_from_fanout, int mode,
                                bool zigzag_take_back,
                                const std::vector<int>& critical_path,
                                const std::vector<double>& betweenness);
  TraversalPlan BuildPlacement2DTraversalPlan(bool annotate);
  TraversalPlan BuildTraversalPlan(bool annotate);
  std::optional<int> PlaceInitialNode(int dfg_node_id, PlacementState& state);
  double DegreeMatchScore(int dfg_node_id, int mrrg_node_id,
                          const PlacementState& state) const;
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

  // sa
  std::optional<PlacementState> RunSAMultiSeed(
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunSA(
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunPlacement2DSA(
      std::chrono::steady_clock::time_point start);
  bool ApplyRandomMove(PlacementState& state);
  bool ApplySwapMove(PlacementState& state, int a, int b) const;
  bool ApplyMoveToFreeResource(PlacementState& state);

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

}  // namespace mapper::detail::sa_spatial
