#include <mapper/placement2d/placement2d_sa_mapper.hpp>

#include <mapper/placement2d/placement2d_search_engine.hpp>
#include <mapper/mapper_factory.hpp>

namespace {

const bool kPlacement2DSAMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DSAMapper>(
        "Placement2DSAMapper");
const bool kPlacement2DSAShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DSAMapper>("Placement2DSA");

}  // namespace

namespace mapper {

Placement2DSAMapper::Placement2DSAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                               const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult Placement2DSAMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.max_iterations = max_iterations_;
  options.placement_only = placement_only_;
  return detail::RunPlacement2DSearchMapper(
      dfg_ptr_, mrrg_ptr_, detail::Placement2DSearchKind::kSA,
      timeout_s_, log_file_path_, options);
}

void Placement2DSAMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void Placement2DSAMapper::SetTimeOut(double time_out_s) { timeout_s_ = time_out_s; }

void Placement2DSAMapper::SetAcceptFeasibleSolution(bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void Placement2DSAMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void Placement2DSAMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void Placement2DSAMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void Placement2DSAMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void Placement2DSAMapper::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

void Placement2DSAMapper::SetPlacementOnly(bool placement_only) {
  placement_only_ = placement_only;
}

}  // namespace mapper
