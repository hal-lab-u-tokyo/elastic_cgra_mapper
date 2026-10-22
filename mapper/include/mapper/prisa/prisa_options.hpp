#pragma once
#include <mapper/mapper.hpp>
#include <memory>
#include <optional>
#include <string>

namespace mapper::prisa {
enum class PlacementSearchKind {
  kAutoPRISA,
  kModuloPhysicalPRISA,
  kModuloPhysicalPRISAManhattan,
  kPlacement2DPRISA,
  kPlacement2DPRISANoSIS,
  kPlacement2DCostAwarePRISA
};
struct PlacementSearchOptions {
  std::optional<int> max_trials;
  std::optional<int> seed_count;
  std::optional<int> routing_retry_count;
  std::optional<int> random_seed;
  std::optional<int> max_iterations;
  bool placement_only = false;
};
enum class Placement2DArrayKind { kPRISA, kPRISANoSIS, kCostAwarePRISA };
struct Placement2DArrayOptions {
  double timeout_s = 1.0;
  std::optional<std::string> log_file_path;
  std::optional<int> max_trials;
  std::optional<int> seed_count;
  std::optional<int> random_seed;
  std::optional<int> max_iterations;
  std::optional<int> elite_placement_count;
  std::optional<std::string> io_node_policy;
  std::optional<std::string> trial_seed_policy;
  std::optional<std::string> traversal_order_policy;
  std::optional<std::string> traversal_neighbor_policy;
  std::optional<std::string> candidate_scope_policy;
  std::optional<std::string> candidate_rank_policy;
  std::optional<bool> use_yott_annotations;
  std::optional<bool> trace_trials;
};
MappingResult RunPRISASearch(const std::shared_ptr<entity::DFG>&,
                             const std::shared_ptr<entity::MRRG>&,
                             PlacementSearchKind, const std::optional<double>&,
                             const std::optional<std::string>&,
                             const PlacementSearchOptions&);
MappingResult RunPrisaArray(entity::DFG&, entity::MRRG&, Placement2DArrayKind,
                            const Placement2DArrayOptions&);
}  // namespace mapper::prisa
