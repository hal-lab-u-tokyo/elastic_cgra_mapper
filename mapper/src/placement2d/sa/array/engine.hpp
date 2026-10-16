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

namespace mapper::detail::placement2d_sa_array {

double SecondsSince(std::chrono::steady_clock::time_point start);

class Placement2DSAEngine {
 public:
  Placement2DSAEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                      mapper::Placement2DArrayKind kind,
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
  bool UsesStructuralIOCellTypes() const;
  int MaxTrials() const;
  int SeedCount() const;
  int MaxIterations() const;
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
  std::vector<int> CompatibleCells(int dfg_node,
                                   const PlacementState& state) const;
  void PlaceNode(int dfg_node, int cell, PlacementState& state) const;

  std::vector<int> BuildCriticalPathScore() const;

  // prisa
  std::optional<PlacementState> RandomPlacement();

  // sa
  bool ApplySwap(PlacementState& state, int a, int b);
  bool ApplyMoveToFreeCell(PlacementState& state, int node);
  std::optional<PlacementState> RunSA(
      std::chrono::steady_clock::time_point start);
  std::optional<PlacementState> RunSAMultiSeed(
      std::chrono::steady_clock::time_point start);
};

}  // namespace mapper::detail::placement2d_sa_array
