#include <mapper/placement2d/placement2d_yoto_mapper.hpp>

#include <mapper/placement2d/placement2d_search_engine.hpp>
#include <mapper/mapper_factory.hpp>

// This file is the YOTO-facing mapper entry point. It registers the public
// Algorithm.type names and forwards execution to either the shared placement
// search engine or the direct-grid array engine.

namespace {

const bool kPlacement2DYOTOMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DYOTOMapper>(
        "Placement2DYOTOMapper");
const bool kPlacement2DYOTOShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DYOTOMapper>("Placement2DYOTO");
const bool kPlacement2DFaithfulArrayYOTOMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DFaithfulArrayYOTOMapper>(
        "Placement2DFaithfulArrayYOTOMapper");
const bool kPlacement2DFaithfulArrayYOTOShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DFaithfulArrayYOTOMapper>(
        "Placement2DFaithfulArrayYOTO");
const bool kPlacement2DCPUMappingYOTOMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DCPUMappingYOTOMapper>(
        "Placement2DCPUMappingYOTOMapper");
const bool kPlacement2DCPUMappingYOTOShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DCPUMappingYOTOMapper>(
        "Placement2DCPUMappingYOTO");

}  // namespace

namespace mapper {

Placement2DYOTOMapper::Placement2DYOTOMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult Placement2DYOTOMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.placement_only = placement_only_;
  return detail::RunPlacement2DSearchMapper(
      dfg_ptr_, mrrg_ptr_, detail::Placement2DSearchKind::kYOTO,
      timeout_s_, log_file_path_, options);
}

void Placement2DYOTOMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void Placement2DYOTOMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void Placement2DYOTOMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void Placement2DYOTOMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void Placement2DYOTOMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void Placement2DYOTOMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void Placement2DYOTOMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void Placement2DYOTOMapper::SetPlacementOnly(bool placement_only) {
  placement_only_ = placement_only;
}

}  // namespace mapper
