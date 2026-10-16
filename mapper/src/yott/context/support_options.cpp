#include "engine.hpp"

namespace mapper::detail::yott_context {

std::string ModuloYOTTEngine::MapperName() const {
  switch (search_kind_) {
    case PlacementSearchKind::kYOTT:
      return "YOTT";
    case PlacementSearchKind::kModuloPhysicalYOTT:
      return "ModuloPhysicalYOTT";
    case PlacementSearchKind::kYOTTWithFallback:
      return "YOTTWithFallback";
  }
  return "unknown";
}

bool ModuloYOTTEngine::IsYOTTLike() const {
  return search_kind_ == PlacementSearchKind::kYOTT ||
         search_kind_ == PlacementSearchKind::kYOTTWithFallback ||
         search_kind_ == PlacementSearchKind::kModuloPhysicalYOTT || false;
}

bool ModuloYOTTEngine::IsPhysicalThenContextLike() const {
  return false || search_kind_ == PlacementSearchKind::kModuloPhysicalYOTT ||
         false || false;
}

bool ModuloYOTTEngine::UsesPhysicalPlacementStage() const {
  return false || IsPhysicalThenContextLike();
}

bool ModuloYOTTEngine::UsesPaperTraversalPlan() const {
  return false || search_kind_ == PlacementSearchKind::kYOTT ||
         IsPhysicalThenContextLike() || false ||
         search_kind_ == PlacementSearchKind::kYOTTWithFallback || false;
}

bool ModuloYOTTEngine::UsesModuloFallbackCandidates() const {
  return false || search_kind_ == PlacementSearchKind::kYOTTWithFallback;
}

bool ModuloYOTTEngine::UsesPhysicalPeExclusivePlacement() const {
  return UsesPhysicalPlacementStage() || false || false;
}

bool ModuloYOTTEngine::UsesSeparatePlacement2DIOCells() const {
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

bool ModuloYOTTEngine::UsesApproximatePlacementDistance() const {
  return false || false || IsPhysicalThenContextLike() || false || false;
}

bool ModuloYOTTEngine::NeedsAllPairsDistances() const {
  return !UsesApproximatePlacementDistance();
}

int ModuloYOTTEngine::SeedCount() const {
  if (!options_.seed_count.has_value()) return 1;
  return std::max(1, options_.seed_count.value());
}

int ModuloYOTTEngine::MaxTrials() const {
  const int default_trials = std::max(100, dfg_.GetNodeNum() * 4);
  if (!options_.max_trials.has_value()) return default_trials;
  return std::max(1, options_.max_trials.value());
}

int ModuloYOTTEngine::RoutingRetryCount() const {
  if (!options_.routing_retry_count.has_value()) return 4;
  return std::max(1, options_.routing_retry_count.value());
}

int ModuloYOTTEngine::MaxIterations() const {
  const int default_iterations =
      std::max(5000, dfg_.GetNodeNum() * mrrg_.GetNodeNum() * 30);
  if (!options_.max_iterations.has_value()) return default_iterations;
  return std::max(1, options_.max_iterations.value());
}

unsigned int ModuloYOTTEngine::SeedFor(int seed_index) const {
  const unsigned int configured_seed =
      options_.random_seed.has_value()
          ? static_cast<unsigned int>(options_.random_seed.value())
          : base_seed_;
  return configured_seed + static_cast<unsigned int>(0x9E3779B9u * seed_index);
}

void ModuloYOTTEngine::ResetSeed(int seed_index) {
  rng_.seed(SeedFor(seed_index));
}

void ModuloYOTTEngine::Log(const std::string& message) const {
  if (!log_file_path_.has_value()) return;
  std::ofstream ofs(log_file_path_.value(), std::ios::app);
  if (ofs) ofs << "[" << MapperName() << "] " << message << "\n";
}

mapper::MappingResult ModuloYOTTEngine::MakeFailureResult(
    std::chrono::steady_clock::time_point start) const {
  const double mapping_time_s = SecondsSince(start);
  Log("failure mapper=" + MapperName() +
      " time_s=" + std::to_string(mapping_time_s));
  return mapper::MappingResult(false, entity::Mapping(mrrg_.GetMRRGConfig()),
                               mapping_time_s);
}

entity::Mapping ModuloYOTTEngine::MakePlacementOnlyMapping(
    const PlacementState& placement) const {
  // Keep direct neighbor connections in the emitted mapping, but do not insert
  // route nodes. Paper-style placement evaluation can then read operation
  // locations and direct-edge quality without requiring full routing success.
  std::vector<std::vector<int>> empty_route_edges(dfg_.GetNodeNum());
  return entity::Mapping(mrrg_, dfg_, placement.dfg_to_mrrg, empty_route_edges);
}

}  // namespace mapper::detail::yott_context
