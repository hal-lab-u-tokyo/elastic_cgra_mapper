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
#include <mapper/placement2d/placement2d_array_mapper_base.hpp>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "array_engine_types.hpp"

namespace mapper::detail::placement2d {

double SecondsSince(std::chrono::steady_clock::time_point start);

class Placement2DArrayEngine {
 public:
  Placement2DArrayEngine(
      entity::DFG& dfg, entity::MRRG& mrrg, mapper::Placement2DArrayKind kind,
      const mapper::detail::Placement2DArrayOptions& options);

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
  std::vector<std::vector<int>> successors_;
  std::vector<std::vector<int>> predecessors_;
  std::vector<std::vector<int>> incident_edge_ids_;
  std::vector<int> degree_;
  std::vector<int> critical_path_score_;
  int critical_path_max_ = 0;
  std::vector<std::vector<int>> compatible_cells_;
  std::vector<int> initial_freedom_;
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
  bool IsPaperGuidedTraversalLike() const;
  bool IsPaperGuidedYOTO() const;
  bool IsPaperGuidedYOTT() const;
  bool IsCPUMappingYOTO() const;
  bool IsCPUMappingYOTT() const;
  bool IsCPUMappingYOTTCore() const;
  bool IsCPUMappingYOTTCoreRepair() const;
  bool UsesYOTTAnnotations() const;
  bool UsesStructuralIOCellTypes() const;
  int MaxTrials() const;
  int SeedCount() const;
  int MaxIterations() const;
  int ElitePlacementCount() const;
  unsigned int SeedFor(int seed_index) const;
  unsigned int TrialSeedFor(int seed_index, int trial_index) const;
  int AuthorRandomInt(int exclusive_upper_bound);
  void ShuffleWithAuthorRandom(std::vector<int>& values);
  void ShuffleWithAuthorRandom(std::array<std::pair<int, int>, 4>& values);
  int TraversalNeighborMode(bool is_yott);
  void ResetSeed(int seed_index);
  void ResetTrialSeed(int seed_index, int trial_index);
  bool UsesPerTrialSeeds() const;
  bool HasTimedOut(std::chrono::steady_clock::time_point start,
                   double reserve_s = 0.01) const;
  void Log(const std::string& message) const;
  bool TraceTrials() const;
  std::string TrialTracePath() const;
  void WriteTrialTraceHeader(std::ofstream& trace) const;
  TrialTraceMetrics ComputeTrialTraceMetrics(const PlacementState& state) const;
  std::string PlacementSignature(const PlacementState& state) const;
  std::string PlacementHash(const PlacementState& state) const;
  std::string PlanHash(const std::vector<Step>& plan) const;
  void WriteTrialTraceRow(std::ofstream& trace, const std::string& phase,
                          int seed_index, int trial_index, bool success,
                          bool selected_as_best, double selection_primary,
                          double selection_secondary,
                          const std::optional<PlacementState>& state,
                          double elapsed_s,
                          const std::string& plan_hash = "") const;
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

  // Shared traversal graph helpers.
  bool IsOutputNode(int dfg_node) const;
  bool IsCornerCell(int cell) const;
  std::vector<int> RawTraversalRoots();
  std::vector<int> CPUMappingTraversalRoots();
  int FindEdgeId(int source, int target) const;
  int FindLastStepTargeting(const std::vector<Step>& plan,
                            int target_node) const;
  std::vector<int> BuildCriticalPathScore() const;
  std::vector<int> BuildCPUMappingInputDistanceScore() const;
  std::vector<double> BuildBetweennessCentralityScore() const;

  // Paper-guided YOTO/YOTT: initial placement and traversal plan.
  double PaperGuidedIOPlacementScore(int dfg_node, int cell) const;
  double PaperGuidedInitialPlacementScore(int dfg_node, int cell) const;
  void BackpropagatePaperGuidedAnnotation(std::vector<Step>& plan,
                                          int from_node, int anchor_node,
                                          StepAnnotationKind kind,
                                          int max_depth,
                                          int initial_distance = 1) const;
  int ChoosePaperGuidedNeighbor(const std::vector<int>& candidates,
                                bool choose_from_fanout, int mode,
                                bool zigzag_take_back,
                                const std::vector<int>& critical_path,
                                const std::vector<double>& betweenness);
  std::vector<Step> BuildPaperGuidedTraversalPlan(bool annotate);

  // Paper-guided YOTO/YOTT: candidate generation and ranking.
  int PaperGuidedCellFreedom(int cell, const PlacementState& state) const;
  double PaperGuidedDegreeMatchScore(int dfg_node, int cell,
                                     const PlacementState& state) const;
  int FreeCellsAtDistance(int anchor_cell, int target_distance,
                          const PlacementState& state) const;
  double PaperGuidedAnnotationScore(const Step& step, int candidate_cell,
                                    const PlacementState& state,
                                    StepAnnotationKind kind,
                                    bool lookahead_only) const;
  bool HasPaperGuidedAnnotationKind(const Step& step, StepAnnotationKind kind,
                                    bool lookahead_only) const;
  PaperGuidedRank PaperGuidedTraversalRank(const Step& step, int candidate_cell,
                                           const PlacementState& state,
                                           bool use_annotations,
                                           int random_order) const;
  static bool IsBetterPaperGuidedRank(const PaperGuidedRank& a,
                                      const PaperGuidedRank& b);
  std::vector<int> ClosestCompatibleCells(int dfg_node, int anchor_cell,
                                          const PlacementState& state) const;

  // Paper-guided YOTO/YOTT: trial construction and multi-start loop.
  int ChoosePaperGuidedInitialCell(int dfg_node, PlacementState& state);
  bool PlacePaperGuidedInitialNode(int dfg_node, PlacementState& state);
  bool PlacePaperGuidedStep(const Step& step, PlacementState& state,
                            bool use_annotations);
  std::optional<PlacementState> ConstructPaperGuidedTraversalPlacement();
  std::optional<PlacementState> RunPaperGuidedTraversalMultiStart(
      std::chrono::steady_clock::time_point start);

  // cpu_mapping YOTO/YOTT: traversal plan and annotations.
  int FindUnusedEdgeIndex(int source, int target,
                          const std::vector<char>& used_edges) const;
  int ChooseCPUMappingZigZagNeighbor(const std::vector<int>& candidates,
                                     bool choose_from_fanout, int mode,
                                     bool zigzag_take_back,
                                     const std::vector<int>& critical_path,
                                     const std::vector<double>& betweenness);
  void AddCPUMappingCycleAnnotation(std::vector<Step>& plan, int step_index,
                                    int anchor_node, int distance) const;
  void ApplyCPUMappingCycleAnnotations(
      std::vector<Step>& plan,
      const std::vector<std::pair<int, int>>& cycle_edges) const;
  std::vector<Step> BuildCPUMappingPlan();
  std::vector<Step> BuildGraphSearchPlan(bool breadth_first);
  std::vector<Step> BuildFullyRandomPlan();

  // cpu_mapping YOTO/YOTT: candidate cells and local placement.
  std::vector<int> CPUMappingCompatibleCells(int dfg_node,
                                             const PlacementState& state) const;
  std::vector<std::pair<int, int>> CPUMappingAllCompatibleCellPairs(
      int dfg_node, const CPUMappingPlacementContext& context) const;
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
                               const CPUMappingPlacementContext& context);
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

  // cpu_mapping YOTO/YOTT: one step, shared context, and trial loop.
  bool PlaceCPUMappingOrientedStep(const Step& step,
                                   CPUMappingPlacementContext& context);
  void UpdateCPUMappingFreedomGrid(int placed_cell,
                                   std::vector<int>& freedom) const;
  CPUMappingPlacementContext MakeCPUMappingContext() const;
  std::optional<CPUMappingTrialResult> ConstructCPUMappingPlacement(
      const std::vector<Step>& plan);
  std::optional<CPUMappingTrialResult> ConstructCPUMappingPlacement();
  std::optional<PlacementState> RunCPUMappingMultiStart(
      std::chrono::steady_clock::time_point start);

  // YOTTCore core: compact two-pass YOTT placement.
  std::vector<Step> BuildYOTTCorePlan();
  int ChooseYOTTCoreCell(const std::vector<std::pair<int, int>>& cells,
                         int dfg_node,
                         const CPUMappingPlacementContext& context);
  bool PlaceYOTTCoreStep(const Step& step, CPUMappingPlacementContext& context);
  std::optional<PlacementState> ConstructYOTTCorePlacement(
      const std::vector<Step>& plan);
  std::optional<PlacementState> RunYOTTCoreMultiStart(
      std::chrono::steady_clock::time_point start);

  // Profile repair improves the selected Core Repair placement without
  // worsening any point of its FIFO-tail distribution.
  ProfileRepairContext BuildProfileRepairContext(
      const PlacementState& state) const;
  std::vector<int> ProfileRepairActiveNodes(
      const ProfileRepairContext& context) const;
  std::vector<int> ProfileRepairEmptyCells(const PlacementState& state,
                                           int node) const;
  std::vector<int> ProfileRepairSwapNodes(const PlacementState& state,
                                          int node) const;
  std::vector<int> ProfileRepairPreferredEmptyCells(const PlacementState& state,
                                                    int node, int limit) const;
  std::optional<ProfileRepairMove> EvaluateProfileRepairMove(
      const PlacementState& state, const ProfileRepairContext& context,
      const std::vector<std::pair<int, int>>& assignments) const;
  void ApplyProfileRepairMove(PlacementState& state,
                              const ProfileRepairMove& move) const;
  std::optional<ProfileRepairMove> FindBestProfileRepairMove(
      const PlacementState& state, const ProfileRepairContext& context) const;
  PlacementState PolishProfileRepairPlacement(
      PlacementState state, std::chrono::steady_clock::time_point start);
  PlacementState PolishBalancedCoreRepairPlacement(
      PlacementState state, const ProfileRepairMetrics& guard,
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunCoreTailRepairMultiStart(
      std::chrono::steady_clock::time_point start);

  // YOTTCore repair: compact YOTTCore-derived algorithm.
  // It keeps the two-pass YOTT plan and adds bounded repair over the current
  // bottleneck edges.
  bool CoreRepairUsesCutDemand() const;
  bool CoreRepairUsesRepair() const;
  bool CoreRepairUsesTopM() const;
  int CoreRepairEdgePoolSize() const;
  bool CoreRepairUsesFifoScore() const;
  double CoreRepairEdgeWeight(int edge_id) const;
  double CoreRepairEdgeCost(int source_cell, int target_cell) const;
  double CoreRepairEdgeCost(int edge_id, int source_cell,
                            int target_cell) const;
  double CoreRepairCutScore(const PlacementState& state) const;
  double CoreRepairFifoScore(const PlacementState& state) const;
  std::pair<std::vector<double>, std::vector<double>> CoreRepairCutDemand(
      const PlacementState& state) const;
  double CoreRepairEdgePriority(const PlacementState& state, int edge_id,
                                const std::vector<double>& row_cut,
                                const std::vector<double>& col_cut) const;
  std::vector<int> CoreRepairEdgePool(const PlacementState& state) const;
  std::vector<int> CoreRepairCells(int center_cell, int radius) const;
  double CoreRepairLocalSwapDelta(const PlacementState& state, int first_cell,
                                  int second_cell,
                                  const std::vector<int>& affected_edges) const;
  double CoreRepairScore(const PlacementState& state) const;
  std::vector<std::pair<int, int>> CoreRepairCandidateCells(
      const Step& step, int dfg_node, int anchor_cell,
      const CPUMappingPlacementContext& context);
  double CoreRepairCandidateScore(
      const Step& step, int dfg_node, int candidate_cell,
      const CPUMappingPlacementContext& context) const;
  int ChooseCoreRepairCell(const std::vector<std::pair<int, int>>& cells,
                           const Step& step, int dfg_node,
                           const CPUMappingPlacementContext& context);
  bool PlaceCoreRepairStep(const Step& step,
                           CPUMappingPlacementContext& context);
  PlacementState PolishCoreRepairPlacement(
      PlacementState state, std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> ConstructCoreRepairPlacement(
      const std::vector<Step>& plan);
  std::optional<PlacementState> RunCoreRepairMultiStart(
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
  bool PRISAEdgeRegion(const PlacementState& state, int edge_id, bool* is_weak,
                       bool* is_potential) const;
  void AddPRISAEdgeToStats(const PlacementState& state, int edge_id,
                           PRISAStats& stats, int sign) const;
  PRISAStats InitializePRISAStats(const PlacementState& state) const;
  std::vector<int> AffectedEdgesForCellPair(const PlacementState& state,
                                            int cell_a, int cell_b) const;
  int CellAfterSwap(int cell, int first_cell, int second_cell) const;
  int EdgeWeakAfterSwap(const PlacementState& state, int edge_id,
                        int first_cell, int second_cell) const;
  int PRISAWeakDeltaForCellSwap(const PlacementState& state, int first_cell,
                                int second_cell,
                                const std::vector<int>& affected_edges) const;
  double PlacementCostDeltaForCellSwap(
      const PlacementState& state, int first_cell, int second_cell,
      const std::vector<int>& affected_edges) const;
  int DirectEdgeDeltaForCellSwap(const PlacementState& state, int first_cell,
                                 int second_cell,
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
                                         int first_index, bool row_side) const;
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
  bool IsCostAwareParetoImprovement(const CostAwarePRISAMetrics& candidate,
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
  std::optional<PlacementState> RunSA(
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunSAMultiSeed(
      std::chrono::steady_clock::time_point start);
};

}  // namespace mapper::detail::placement2d
