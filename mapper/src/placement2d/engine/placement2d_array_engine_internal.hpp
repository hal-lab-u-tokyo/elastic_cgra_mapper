#pragma once

#include <mapper/placement2d/placement2d_array_mapper_base.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mapper::detail::placement2d {

constexpr double kImpossibleCost = 1.0e12;

struct EdgeInfo {
  int source = -1;
  int target = -1;
};

struct PlacementState {
  std::vector<int> dfg_to_cell;
  std::vector<int> cell_to_dfg;
};

struct CPUMappingPlacementContext {
  PlacementState state;
  std::vector<int> freedom;
  std::vector<int> mutable_degree;
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
  double cost = kImpossibleCost;
  int direct_edges = 0;
  int max_distance = 0;
};

struct CostAwarePRISAMetrics {
  double mesh_hop_sum = 0.0;
  int max_mesh_hop = 0;
  int mapped_lp_mesh_hop = 0;
  int direct_edge_count = 0;
};

enum class StepAnnotationKind { kIO, kReconvergence };

struct StepAnnotation {
  StepAnnotationKind kind = StepAnnotationKind::kReconvergence;
  int anchor_node = -1;
  int distance = 0;
};

struct Step {
  // A traversal plan is a list of directed edge-local placement decisions.
  // `anchor` should be placed first; `target` is placed near it. Annotations
  // are soft YOTT hints used by candidate ranking, not hard feasibility rules.
  int anchor = -1;
  int target = -1;
  int edge_id = -1;
  std::vector<StepAnnotation> annotations;
};

struct FaithfulRank {
  int annotation_priority = 3;
  double annotation_cost = 0.0;
  int locality_distance = 0;
  double degree_cost = 0.0;
  int random_order = 0;
};

double SecondsSince(std::chrono::steady_clock::time_point start);

class Placement2DArrayEngine {
 public:
  Placement2DArrayEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                         mapper::Placement2DArrayKind kind, double timeout_s,
                         const std::optional<std::string>& log_file_path,
                         std::optional<int> max_trials,
                         std::optional<int> seed_count,
                         std::optional<int> random_seed,
                         std::optional<int> max_iterations,
                         std::optional<bool> cpu_mapping_bug_compatible_degree,
                         const std::optional<std::string>& io_node_policy);

  mapper::MappingResult Run();

 private:
  entity::DFG& dfg_;
  entity::MRRG& mrrg_;
  mapper::Placement2DArrayKind kind_;
  double timeout_s_;
  std::optional<std::string> log_file_path_;
  std::optional<int> max_trials_;
  std::optional<int> seed_count_;
  std::optional<int> random_seed_;
  std::optional<int> max_iterations_;
  bool cpu_mapping_bug_compatible_degree_ = true;
  std::string io_node_policy_ = "opcode";
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
  std::vector<std::vector<int>> successors_;
  std::vector<std::vector<int>> predecessors_;
  std::vector<std::vector<int>> incident_edge_ids_;
  std::vector<int> degree_;
  std::vector<std::vector<int>> compatible_cells_;
  std::vector<int> prisa_cell_order_;
  std::vector<int> prisa_order_index_;
  std::vector<int> prisa_distance_matrix_;
  std::vector<char> prisa_weak_region_matrix_;
  int prisa_weak_distance_threshold_ = std::numeric_limits<int>::max();
  bool separate_io_cells_ = false;
  long long cell_visits_ = 0;
  mutable long long placement_swap_attempts_ = 0;

  // options
  std::string MapperName() const;
  bool IsPRISALike() const;
  bool UsesPRISASIS() const;
  bool UsesCostAwarePRISA() const;
  bool IsCPUMappingLike() const;
  bool IsFaithfulArrayTraversalLike() const;
  bool IsFaithfulArrayYOTO() const;
  bool IsFaithfulArrayYOTT() const;
  bool IsCPUMappingYOTO() const;
  bool IsCPUMappingYOTT() const;
  bool UsesStructuralIOCellTypes() const;
  int MaxTrials() const;
  int SeedCount() const;
  int MaxIterations() const;
  unsigned int SeedFor(int seed_index) const;
  void ResetSeed(int seed_index);
  bool HasTimedOut(std::chrono::steady_clock::time_point start,
                   double reserve_s = 0.01) const;
  void Log(const std::string& message) const;
  void RecordPlacementSwapAttempts(long long count = 1) const;
  std::string NodeLabel(int node) const;
  int PlacedCount(const PlacementState& state) const;
  int FreeCPUMappingCompatibleCellCount(int dfg_node,
                                        const PlacementState& state) const;
  void RecordFailure(const std::string& reason);

  // cache
  void BuildDFGCache();
  void BuildGridCache();
  void BuildCompatibilityCache();
  int PRISAResourceSideLength() const;
  int PRISAWeakPairCount(int resource_count) const;
  void BuildPRISACache();
  static std::string NormalizePolicy(std::string policy);
  int Cell(int row, int col) const;
  int Row(int cell) const;
  int Col(int cell) const;
  bool IsCompatible(int dfg_node, int cell) const;
  bool IsIONode(int dfg_node) const;
  bool CanPlace(int dfg_node, int cell, const PlacementState& state) const;
  int CPUMappingCellType(int cell) const;
  bool IsCPUMappingCompatible(int dfg_node, int cell) const;
  bool CanPlaceCPUMapping(int dfg_node, int cell,
                          const PlacementState& state) const;
  int CellFreedom(int cell) const;
  std::vector<int> InitialFreedomGrid() const;
  int DistanceCost(int a, int b) const;

  // quality
  double PlacementCost(const PlacementState& state) const;
  PlacementQuality ComputePlacementQuality(const PlacementState& state) const;
  bool IsBetterQuality(const PlacementQuality& candidate,
                       const PlacementQuality& best) const;
  int PRISAWeakEdgeCount(const PlacementState& state) const;
  CostAwarePRISAMetrics ComputeCostAwarePRISAMetrics(
      const PlacementState& state) const;
  double CostAwarePRISAScore(const PlacementState& state) const;
  double PRISAAcceptanceScore(const PlacementState& state) const;
  int BorderDistance(int cell) const;
  std::vector<int> CompatibleCells(int dfg_node,
                                   const PlacementState& state) const;
  void PlaceNode(int dfg_node, int cell, PlacementState& state) const;

  // faithful_traversal
  bool IsOutputNode(int dfg_node) const;
  bool IsCornerCell(int cell) const;
  double FaithfulIOPlacementScore(int dfg_node, int cell) const;
  double FaithfulInitialPlacementScore(int dfg_node, int cell) const;
  std::vector<int> RawTraversalRoots();
  int FindEdgeId(int source, int target) const;
  int FindLastStepTargeting(const std::vector<Step>& plan,
                            int target_node) const;
  void BackpropagateFaithfulAnnotation(std::vector<Step>& plan, int from_node,
                                       int anchor_node,
                                       StepAnnotationKind kind, int max_depth,
                                       int initial_distance = 1) const;
  std::vector<int> BuildCriticalPathScore() const;
  std::vector<double> BuildBetweennessCentralityScore() const;
  int ChooseFaithfulNeighbor(const std::vector<int>& candidates,
                             bool choose_from_fanout, int mode,
                             bool zigzag_take_back,
                             const std::vector<int>& critical_path,
                             const std::vector<double>& betweenness);
  std::vector<Step> BuildFaithfulTraversalPlan(bool annotate);
  int FaithfulCellFreedom(int cell, const PlacementState& state) const;
  double FaithfulDegreeMatchScore(int dfg_node, int cell,
                                  const PlacementState& state) const;
  int FreeCellsAtDistance(int anchor_cell, int target_distance,
                          const PlacementState& state) const;
  double FaithfulAnnotationScore(const Step& step, int candidate_cell,
                                 const PlacementState& state,
                                 StepAnnotationKind kind,
                                 bool lookahead_only) const;
  bool HasFaithfulAnnotationKind(const Step& step, StepAnnotationKind kind,
                                 bool lookahead_only) const;
  FaithfulRank FaithfulTraversalRank(const Step& step, int candidate_cell,
                                     const PlacementState& state,
                                     bool use_annotations,
                                     int random_order) const;
  static bool IsBetterFaithfulRank(const FaithfulRank& a,
                                   const FaithfulRank& b);
  std::vector<int> ClosestCompatibleCells(int dfg_node, int anchor_cell,
                                          const PlacementState& state) const;
  int ChooseFaithfulInitialCell(int dfg_node, PlacementState& state);
  bool PlaceFaithfulInitialNode(int dfg_node, PlacementState& state);
  bool PlaceFaithfulStep(const Step& step, PlacementState& state,
                         bool use_annotations);
  std::optional<PlacementState> ConstructFaithfulTraversalPlacement();
  std::optional<PlacementState> RunFaithfulTraversalMultiStart(
      std::chrono::steady_clock::time_point start);

  // cpu_mapping
  int FindUnusedEdgeIndex(int source, int target,
                          const std::vector<char>& used_edges) const;
  int ChooseCPUMappingZigZagNeighbor(
      const std::vector<int>& candidates, bool choose_from_fanout, int mode,
      bool zigzag_take_back, const std::vector<int>& critical_path,
      const std::vector<double>& betweenness);
  void AddCPUMappingCycleAnnotation(std::vector<Step>& plan, int step_index,
                                    int anchor_node, int distance) const;
  void ApplyCPUMappingCycleAnnotations(
      std::vector<Step>& plan,
      const std::vector<std::pair<int, int>>& cycle_edges) const;
  std::vector<Step> BuildCPUMappingPlan();
  std::vector<int> CPUMappingCompatibleCells(
      int dfg_node, const PlacementState& state) const;
  std::vector<int> CPUMappingIOCellsForInitialPlacement(
      int dfg_node, const PlacementState& state) const;
  int ChooseCPUMappingInitialCell(int dfg_node, const PlacementState& state);
  std::vector<std::pair<int, int>> CPUMappingTipCells(
      int anchor_cell, int dfg_node,
      const CPUMappingPlacementContext& context) const;
  std::vector<std::pair<int, int>> CPUMappingCellsWithinAnnotatedDistance(
      int anchor_cell, int distance,
      const CPUMappingPlacementContext& context) const;
  static void SortCPUMappingCellPairs(std::vector<std::pair<int, int>>& cells);
  static std::vector<std::pair<int, int>> IntersectCPUMappingCellPairs(
      std::vector<std::pair<int, int>> lhs,
      std::vector<std::pair<int, int>> rhs);
  int BestCPUMappingDegreeCell(const std::vector<std::pair<int, int>>& cells,
                               int dfg_node,
                               const CPUMappingPlacementContext& context) const;
  void PlaceCPUMappingNode(int dfg_node, int cell,
                           CPUMappingPlacementContext& context) const;
  bool PlaceCPUMappingInitialNode(int dfg_node,
                                  CPUMappingPlacementContext& context);
  static std::vector<std::pair<int, int>> CPUMappingAdjacencyOffsets();
  bool TryCPUMappingAdjacency(int dfg_node, int anchor_cell,
                              CPUMappingPlacementContext& context,
                              bool randomize_first);
  bool PlaceCPUMappingNearNodeYOTO(int dfg_node, int anchor_node,
                                   CPUMappingPlacementContext& context);
  bool DeferCPUMappingIfNearPlacementFails(bool placed);
  bool PlaceCPUMappingNearNodeYOTT(const Step& step,
                                   CPUMappingPlacementContext& context);
  bool PlaceCPUMappingOrientedStep(const Step& step,
                                   CPUMappingPlacementContext& context);
  void UpdateCPUMappingFreedomGrid(int placed_cell,
                                   std::vector<int>& freedom) const;
  CPUMappingPlacementContext MakeCPUMappingContext() const;
  std::optional<PlacementState> ConstructCPUMappingPlacement(
      const std::vector<Step>& plan);
  std::optional<PlacementState> ConstructCPUMappingPlacement();
  std::optional<PlacementState> RunCPUMappingMultiStart(
      std::chrono::steady_clock::time_point start);

  // prisa
  std::optional<PlacementState> RandomPlacement();
  int OtherEndpoint(int edge_id, int node) const;
  std::vector<int> LowBandwidthNodeOrder();
  std::optional<PlacementState> ConstructPRISAInitialPlacementFromOrder(
      const std::vector<int>& node_order);
  std::optional<PlacementState> ConstructPRISAInitialPlacement();
  bool PRISAIsWeak(int row, int column) const;
  bool PRISAIsPotential(int row, int column) const;
  bool PRISAEdgeRegion(const PlacementState& state, int edge_id,
                       bool* is_weak, bool* is_potential) const;
  void AddPRISAEdgeToStats(const PlacementState& state, int edge_id,
                           PRISAStats& stats, int sign) const;
  PRISAStats InitializePRISAStats(const PlacementState& state) const;
  std::vector<int> AffectedEdgesForCellPair(const PlacementState& state,
                                            int cell_a, int cell_b) const;
  int CellAfterSwap(int cell, int first_cell, int second_cell) const;
  int EdgeWeakAfterSwap(const PlacementState& state, int edge_id,
                        int first_cell, int second_cell) const;
  int PRISAWeakDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  double PlacementCostDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  int DirectEdgeDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  int AffectedMaxDistanceAfterCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  bool CanSwapCells(const PlacementState& state, int first_cell,
                    int second_cell) const;
  bool ApplyCellSwapWithStats(PlacementState& state, PRISAStats& stats,
                              int first_cell, int second_cell) const;
  bool ApplyCellSwap(PlacementState& state, int first_cell,
                     int second_cell) const;
  struct PRISAProposal {
    int first_cell = -1;
    int second_cell = -1;
    double cost_delta = 0.0;
    int direct_delta = 0;
    int affected_max_after = 0;
  };
  std::optional<PRISAProposal> ProposeRandomPRISASwap(
      const PlacementState& state);
  std::optional<PRISAProposal> ProposePlacementCostSampledSwap(
      const PlacementState& state, int requested_sample_count = -1);
  int PRISALightQualitySampleCount() const;
  double CostAwarePRISALocalSwapScore(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  int CostAwarePRISACandidateSampleCount() const;
  int CostAwarePRISAFullEvaluationCount() const;
  std::optional<PRISAProposal> ProposeCostAwarePRISASwap(
      const PlacementState& state);
  std::vector<int> PRISAWeakCounterparts(const PlacementState& state,
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
  bool IsBetterPRISACandidate(const PRISACandidate& candidate,
                              const PRISACandidate& best) const;
  std::optional<PRISACandidate> BestPRISACandidateFor(
      const PlacementState& state, const PRISAStats& stats, int first_index,
      bool row_side);
  int RandomArgmax(const std::vector<int>& values);
  std::optional<PRISAProposal> ProposePRISAMove(const PlacementState& state,
                                                const PRISAStats& stats);
  int PRISAMaxIterations() const;
  int PRISAPolishPasses() const;
  PlacementState PolishPRISAQuality(
      PlacementState state, std::chrono::steady_clock::time_point start);
  int DirectEdgeGainForCellSwap(const PlacementState& state, int first_cell,
                                int second_cell,
                                const std::vector<int>& affected_edges) const;
  PlacementState PolishCostAwarePRISADirectEdges(
      PlacementState state, std::chrono::steady_clock::time_point start);
  bool IsCostAwareParetoImprovement(
      const CostAwarePRISAMetrics& candidate,
      const CostAwarePRISAMetrics& current) const;
  bool IsBetterCostAwareParetoCandidate(
      const CostAwarePRISAMetrics& candidate,
      const CostAwarePRISAMetrics& best) const;
  PlacementState PolishCostAwarePRISAPareto(
      PlacementState state, std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunPRISA(
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunPRISAMultiSeed(
      std::chrono::steady_clock::time_point start);

  // sa
  bool ApplySwap(PlacementState& state, int a, int b);
  bool ApplyMoveToFreeCell(PlacementState& state, int node);
  std::optional<PlacementState> RunSA(std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunSAMultiSeed(
      std::chrono::steady_clock::time_point start);

};

}  // namespace mapper::detail::placement2d
