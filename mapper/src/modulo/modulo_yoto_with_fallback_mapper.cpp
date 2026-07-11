#include <mapper/modulo/modulo_yoto_with_fallback_mapper.hpp>

#include <mapper/modulo/modulo_search_engine.hpp>

namespace mapper {

ModuloYOTOWithFallbackMapper::ModuloYOTOWithFallbackMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloYOTOWithFallbackMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return detail::RunModuloPlacementSearchMapper(
      dfg_ptr_, mrrg_ptr_,
      detail::ModuloPlacementSearchKind::kYOTOWithFallback, timeout_s_,
      log_file_path_, options);
}

void ModuloYOTOWithFallbackMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloYOTOWithFallbackMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloYOTOWithFallbackMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloYOTOWithFallbackMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloYOTOWithFallbackMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloYOTOWithFallbackMapper::SetRoutingRetryCount(
    int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloYOTOWithFallbackMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

}  // namespace mapper
