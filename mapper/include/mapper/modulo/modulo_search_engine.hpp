#pragma once

#include <mapper/detail/placement_search_engine.hpp>

namespace mapper::detail {

enum class ModuloPlacementSearchKind {
  kYOTO,
  kYOTT,
  kPhysicalYOTO,
  kPhysicalYOTT,
  kPhysicalPRISA,
  kPhysicalPRISAManhattan,
  kYOTOWithFallback,
  kYOTTWithFallback,
  kSA,
};

MappingResult RunModuloPlacementSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    ModuloPlacementSearchKind search_kind,
    const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options);

}  // namespace mapper::detail
