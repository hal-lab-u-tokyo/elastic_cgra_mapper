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
#include <mapper/detail/placement2d_array_mapper_base.hpp>
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

namespace mapper::detail::prisa_array {

double SecondsSince(std::chrono::steady_clock::time_point start);

class Placement2DPRISAEngine {
 public:
  Placement2DPRISAEngine(
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
  bool UsesPRISASIS() const;
  bool UsesCostAwarePRISA() const;
  bool UsesStructuralIOCellTypes() const;
  int MaxTrials() const;
  int SeedCount() const;
  unsigned int SeedFor(int seed_index) const;
  void ResetSeed(int seed_index);
  bool HasTimedOut(std::chrono::steady_clock::time_point start,
                   double reserve_s = 0.01) const;
  void Log(const std::string& message) const;
  std::string NodeLabel(int node) const;

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
  int CellFreedom(int cell) const;
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
  std::vector<int> CompatibleCells(int dfg_node,
                                   const PlacementState& state) const;
  void PlaceNode(int dfg_node, int cell, PlacementState& state) const;

  std::vector<int> BuildCriticalPathScore() const;

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
};

}  // namespace mapper::detail::prisa_array
