#pragma once

#include <limits>
#include <vector>

namespace mapper::yoto_array {

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

}  // namespace mapper::yoto_array
