#include <mapper/detail/search_mapper_base.hpp>

namespace mapper {

SearchMapperBase::SearchMapperBase(
    detail::SearchKind kind,
    const std::shared_ptr<entity::DFG>& dfg_ptr,
    const std::shared_ptr<entity::MRRG>& mrrg_ptr)
    : kind_(kind), dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult SearchMapperBase::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.max_iterations = max_iterations_;
  options.placement_only = placement_only_;
  return detail::RunSearchMapper(
      dfg_ptr_, mrrg_ptr_, kind_, timeout_s_, log_file_path_, options);
}

void SearchMapperBase::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void SearchMapperBase::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void SearchMapperBase::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  (void)accept_feasible_solution;
}

void SearchMapperBase::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void SearchMapperBase::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void SearchMapperBase::SetRoutingRetryCount(
    int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void SearchMapperBase::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void SearchMapperBase::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

void SearchMapperBase::SetPlacementOnly(bool placement_only) {
  placement_only_ = placement_only;
}

}  // namespace mapper
