#pragma once

#include <limits>
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

struct CPUMappingTrialResult {
  PlacementState state;
  double traversal_edge_cost = kImpossibleCost;
  double final_placement_cost = kImpossibleCost;
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

struct ProfileRepairMetrics {
  int non_direct_cost_edges = 0;
  int total_cost_fifo = 0;
  int max_cost_fifo = 0;
};

struct ProfileRepairContext {
  ProfileRepairMetrics metrics;
  std::vector<int> edge_cost_fifo;
  std::vector<int> cost_fifo_histogram;
  bool prioritize_max_fifo = false;
  int minimum_direct_edges = 0;
  int maximum_total_cost_fifo = std::numeric_limits<int>::max();
  int maximum_cost_fifo = std::numeric_limits<int>::max();
};

struct ProfileRepairMove {
  std::vector<std::pair<int, int>> assignments;
  ProfileRepairMetrics metrics;
};

struct TrialTraceMetrics {
  double placement_cost = kImpossibleCost;
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

}  // namespace mapper::detail::placement2d
