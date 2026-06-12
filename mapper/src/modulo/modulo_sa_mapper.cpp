#include <mapper/modulo/modulo_sa_mapper.hpp>

#include <mapper/modulo/modulo_search_engine.hpp>
#include <mapper/mapper_factory.hpp>

namespace {

const bool kModuloSAMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloSAMapper>("ModuloSAMapper");
const bool kModuloSAShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloSAMapper>("ModuloSA");
const bool kLegacySAMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloSAMapper>("SAMapper");
const bool kLegacySAShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloSAMapper>("SA");

}  // namespace

namespace mapper {

ModuloSAMapper::ModuloSAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                   const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloSAMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.max_iterations = max_iterations_;
  return detail::RunModuloPlacementSearchMapper(
      dfg_ptr_, mrrg_ptr_, detail::ModuloPlacementSearchKind::kSA, timeout_s_,
      log_file_path_, options);
}

void ModuloSAMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloSAMapper::SetTimeOut(double time_out_s) { timeout_s_ = time_out_s; }

void ModuloSAMapper::SetAcceptFeasibleSolution(bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloSAMapper::SetMaxTrials(int max_trials) { max_trials_ = max_trials; }

void ModuloSAMapper::SetSeedCount(int seed_count) { seed_count_ = seed_count; }

void ModuloSAMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloSAMapper::SetRandomSeed(int random_seed) { random_seed_ = random_seed; }

void ModuloSAMapper::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

}  // namespace mapper
