#include "../engine_internal.hpp"

namespace mapper::detail::placement_search {

std::string PlacementSearchEngine::MapperName() const {
  switch (search_kind_) {
    case PlacementSearchKind::kYOTO:
      return "YOTO";
    case PlacementSearchKind::kYOTT:
      return "YOTT";
    case PlacementSearchKind::kModuloPhysicalYOTO:
      return "ModuloPhysicalYOTO";
    case PlacementSearchKind::kModuloPhysicalYOTT:
      return "ModuloPhysicalYOTT";
    case PlacementSearchKind::kModuloPhysicalPRISA:
      return "ModuloPhysicalPRISA";
    case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
      return "ModuloPhysicalPRISAManhattan";
    case PlacementSearchKind::kYOTOWithFallback:
      return "YOTOWithFallback";
    case PlacementSearchKind::kYOTTWithFallback:
      return "YOTTWithFallback";
    case PlacementSearchKind::kSA:
      return "SA";
    case PlacementSearchKind::kPlacement2DYOTO:
      return "Placement2DYOTO";
    case PlacementSearchKind::kPlacement2DYOTT:
      return "Placement2DYOTT";
    case PlacementSearchKind::kPlacement2DSA:
      return "Placement2DSA";
    case PlacementSearchKind::kPlacement2DPRISA:
      return "Placement2DPRISA";
    case PlacementSearchKind::kPlacement2DPRISANoSIS:
      return "Placement2DPRISANoSIS";
    case PlacementSearchKind::kPlacement2DCostAwarePRISA:
      return "Placement2DCostAwarePRISA";
  }
  return "unknown";
}

bool PlacementSearchEngine::IsSALike() const {
  return search_kind_ == PlacementSearchKind::kSA ||
         search_kind_ == PlacementSearchKind::kPlacement2DSA;
}

bool PlacementSearchEngine::IsPRISALike() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISANoSIS ||
         search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

bool PlacementSearchEngine::UsesPRISASIS() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan ||
         search_kind_ == PlacementSearchKind::kPlacement2DPRISA ||
         search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

bool PlacementSearchEngine::UsesManhattanRouting() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan;
}

bool PlacementSearchEngine::UsesCostAwarePRISA() const {
  return search_kind_ == PlacementSearchKind::kPlacement2DCostAwarePRISA;
}

bool PlacementSearchEngine::IsYOTTLike() const {
  return search_kind_ == PlacementSearchKind::kYOTT ||
         search_kind_ == PlacementSearchKind::kYOTTWithFallback ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalYOTT ||
         search_kind_ == PlacementSearchKind::kPlacement2DYOTT;
}

bool PlacementSearchEngine::IsPlacement2DTraversalLike() const {
  return search_kind_ == PlacementSearchKind::kPlacement2DYOTO ||
         search_kind_ == PlacementSearchKind::kPlacement2DYOTT;
}

bool PlacementSearchEngine::IsPhysicalThenContextLike() const {
  return search_kind_ == PlacementSearchKind::kModuloPhysicalYOTO ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalYOTT ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISA ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalPRISAManhattan;
}

bool PlacementSearchEngine::UsesPhysicalPlacementStage() const {
  return IsPlacement2DTraversalLike() || IsPhysicalThenContextLike();
}

bool PlacementSearchEngine::UsesPaperTraversalPlan() const {
  return search_kind_ == PlacementSearchKind::kYOTO ||
         search_kind_ == PlacementSearchKind::kYOTT ||
         IsPhysicalThenContextLike() ||
         search_kind_ == PlacementSearchKind::kYOTOWithFallback ||
         search_kind_ == PlacementSearchKind::kYOTTWithFallback ||
         IsPlacement2DTraversalLike();
}

bool PlacementSearchEngine::UsesModuloFallbackCandidates() const {
  return search_kind_ == PlacementSearchKind::kYOTOWithFallback ||
         search_kind_ == PlacementSearchKind::kYOTTWithFallback;
}

bool PlacementSearchEngine::UsesPhysicalPeExclusivePlacement() const {
  return UsesPhysicalPlacementStage() ||
         search_kind_ == PlacementSearchKind::kPlacement2DSA || IsPRISALike();
}

bool PlacementSearchEngine::UsesSeparatePlacement2DIOCells() const {
  if (!UsesPhysicalPlacementStage()) return false;
  bool has_memory_cell = false;
  bool has_compute_only_cell = false;
  for (int r : placement2d_resources_) {
    const auto property = mrrg_.GetNodeProperty(r);
    has_memory_cell = has_memory_cell || property.is_memory_accessible;
    has_compute_only_cell =
        has_compute_only_cell || !property.is_memory_accessible;
  }
  return has_memory_cell && has_compute_only_cell;
}

bool PlacementSearchEngine::UsesApproximatePlacementDistance() const {
  return search_kind_ == PlacementSearchKind::kPlacement2DYOTO ||
         search_kind_ == PlacementSearchKind::kPlacement2DYOTT ||
         IsPhysicalThenContextLike() ||
         search_kind_ == PlacementSearchKind::kPlacement2DSA || IsPRISALike();
}

bool PlacementSearchEngine::NeedsAllPairsDistances() const {
  return !UsesApproximatePlacementDistance();
}

int PlacementSearchEngine::SeedCount() const {
  if (!options_.seed_count.has_value()) return 1;
  return std::max(1, options_.seed_count.value());
}

int PlacementSearchEngine::MaxTrials() const {
  const int default_trials = std::max(100, dfg_.GetNodeNum() * 4);
  if (!options_.max_trials.has_value()) return default_trials;
  return std::max(1, options_.max_trials.value());
}

int PlacementSearchEngine::RoutingRetryCount() const {
  if (!options_.routing_retry_count.has_value()) return 4;
  return std::max(1, options_.routing_retry_count.value());
}

int PlacementSearchEngine::MaxIterations() const {
  const int default_iterations =
      std::max(5000, dfg_.GetNodeNum() * mrrg_.GetNodeNum() * 30);
  if (!options_.max_iterations.has_value()) return default_iterations;
  return std::max(1, options_.max_iterations.value());
}

unsigned int PlacementSearchEngine::SeedFor(int seed_index) const {
  const unsigned int configured_seed =
      options_.random_seed.has_value()
          ? static_cast<unsigned int>(options_.random_seed.value())
          : base_seed_;
  return configured_seed + static_cast<unsigned int>(0x9E3779B9u * seed_index);
}

void PlacementSearchEngine::ResetSeed(int seed_index) {
  rng_.seed(SeedFor(seed_index));
}

void PlacementSearchEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

mapper::MappingResult PlacementSearchEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping PlacementSearchEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

}  // namespace mapper::detail::placement_search
