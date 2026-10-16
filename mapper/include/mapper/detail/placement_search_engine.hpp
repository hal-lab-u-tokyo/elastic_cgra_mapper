#pragma once

#include <mapper/mapper.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper::detail {

enum class PlacementSearchKind {
  // Modulo-aware adaptations: different contexts of one physical PE may hold
  // different operations.
  kYOTO,
  kYOTT,
  // Two-stage modulo adaptations: first place on the 2D physical PE grid, then
  // assign modulo contexts on the selected physical PEs before routing.
  kModuloPhysicalYOTO,
  kModuloPhysicalYOTT,
  kModuloPhysicalPRISA,
  kModuloPhysicalPRISAManhattan,
  // Explicit hybrid variants. They try the paper-style traversal first, then
  // routing-aware fallback placements. Keep them out of default comparisons
  // unless the experiment intentionally evaluates mixed strategies.
  kYOTOWithFallback,
  kYOTTWithFallback,
  kSA,
  // 2D placement variants: one physical PE is one placement slot, matching the
  // single-layer public cpu_mapping implementation.
  kPlacement2DYOTO,
  kPlacement2DYOTT,
  kPlacement2DSA,
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

MappingResult RunPlacementSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    PlacementSearchKind search_kind, const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options);

}  // namespace mapper::detail
