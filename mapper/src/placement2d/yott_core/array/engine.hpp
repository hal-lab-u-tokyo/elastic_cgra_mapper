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

#include "types.hpp"

namespace mapper::detail::placement2d_yott_core_array {

double SecondsSince(std::chrono::steady_clock::time_point start);

class Placement2DYOTTCoreEngine {
 public:
  Placement2DYOTTCoreEngine(
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
  bool separate_io_cells_ = false;
  long long cell_visits_ = 0;
  mutable long long placement_swap_attempts_ = 0;

  // options
  std::string MapperName() const;
  bool UsesYOTTAnnotations() const;
  bool UsesStructuralIOCellTypes() const;
  int MaxTrials() const;
  int SeedCount() const;
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
  static std::string NormalizePolicy(std::string policy);
  int Cell(int row, int col) const;
  int Row(int cell) const;
  int Col(int cell) const;
  bool IsCompatible(int dfg_node, int cell) const;
  bool IsIONode(int dfg_node) const;
  int CPUMappingCellType(int cell) const;
  bool IsCPUMappingCompatible(int dfg_node, int cell) const;
  bool CanPlaceCPUMapping(int dfg_node, int cell,
                          const PlacementState& state) const;
  int CellFreedom(int cell) const;
  std::vector<int> InitialFreedomGrid() const;
  int DistanceCost(int a, int b) const;

  // quality
  double PlacementCost(const PlacementState& state) const;
  void PlaceNode(int dfg_node, int cell, PlacementState& state) const;

  // Shared traversal graph helpers.
  bool IsOutputNode(int dfg_node) const;
  std::vector<int> CPUMappingTraversalRoots();
  std::vector<int> BuildCriticalPathScore() const;
  std::vector<int> BuildCPUMappingInputDistanceScore() const;
  std::vector<double> BuildBetweennessCentralityScore() const;

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
  void PlaceCPUMappingNode(int dfg_node, int cell,
                           CPUMappingPlacementContext& context) const;
  bool PlaceCPUMappingInitialNode(int dfg_node,
                                  CPUMappingPlacementContext& context);
  static std::vector<std::pair<int, int>> CPUMappingAdjacencyOffsets();
  bool TryCPUMappingAdjacency(int dfg_node, int anchor_cell,
                              CPUMappingPlacementContext& context,
                              bool randomize_first);

  void UpdateCPUMappingFreedomGrid(int placed_cell,
                                   std::vector<int>& freedom) const;
  CPUMappingPlacementContext MakeCPUMappingContext() const;

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
};

}  // namespace mapper::detail::placement2d_yott_core_array
