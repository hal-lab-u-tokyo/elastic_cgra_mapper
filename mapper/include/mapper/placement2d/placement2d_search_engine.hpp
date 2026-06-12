#pragma once

#include <mapper/detail/placement_search_engine.hpp>

namespace mapper::detail {

enum class Placement2DSearchKind {
  kYOTO,
  kYOTT,
  kSA,
};

MappingResult RunPlacement2DSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    Placement2DSearchKind search_kind,
    const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options);

}  // namespace mapper::detail
