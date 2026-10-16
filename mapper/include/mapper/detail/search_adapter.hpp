#pragma once

#include <mapper/detail/placement_search_engine.hpp>

namespace mapper::detail {

enum class SearchKind {
  kAutoSA, kAutoPRISA, kAutoYOTO, kAutoYOTT,
  kYOTO,
  kYOTT,
  kSA,
  kPRISA,
  kPRISANoSIS,
  kCostAwarePRISA,
};

MappingResult RunSearchMapper(
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr,
    SearchKind search_kind,
    const std::optional<double>& timeout_s,
    const std::optional<std::string>& log_file_path,
    const PlacementSearchOptions& options);

}  // namespace mapper::detail
