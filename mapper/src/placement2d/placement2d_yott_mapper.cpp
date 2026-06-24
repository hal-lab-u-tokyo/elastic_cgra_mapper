#include <mapper/placement2d/placement2d_yott_mapper.hpp>

#include <mapper/placement2d/placement2d_search_engine.hpp>
#include <mapper/mapper_factory.hpp>

// This file is the YOTT-facing mapper entry point. It registers the public
// Algorithm.type names and forwards execution to either the shared placement
// search engine or the direct-grid array engine.

namespace {

const bool kPlacement2DYOTTMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DYOTTMapper>(
        "Placement2DYOTTMapper");
const bool kPlacement2DYOTTShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DYOTTMapper>("Placement2DYOTT");
const bool kPlacement2DFaithfulArrayYOTTMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DFaithfulArrayYOTTMapper>(
        "Placement2DFaithfulArrayYOTTMapper");
const bool kPlacement2DFaithfulArrayYOTTShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DFaithfulArrayYOTTMapper>(
        "Placement2DFaithfulArrayYOTT");
const bool kPlacement2DCPUMappingYOTTMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DCPUMappingYOTTMapper>(
        "Placement2DCPUMappingYOTTMapper");
const bool kPlacement2DCPUMappingYOTTShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DCPUMappingYOTTMapper>(
        "Placement2DCPUMappingYOTT");

}  // namespace

namespace mapper {

Placement2DYOTTMapper::Placement2DYOTTMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult Placement2DYOTTMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.placement_only = placement_only_;
  return detail::RunPlacement2DSearchMapper(
      dfg_ptr_, mrrg_ptr_, detail::Placement2DSearchKind::kYOTT,
      timeout_s_, log_file_path_, options);
}

void Placement2DYOTTMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void Placement2DYOTTMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void Placement2DYOTTMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void Placement2DYOTTMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void Placement2DYOTTMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void Placement2DYOTTMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void Placement2DYOTTMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void Placement2DYOTTMapper::SetPlacementOnly(bool placement_only) {
  placement_only_ = placement_only;
}

}  // namespace mapper
