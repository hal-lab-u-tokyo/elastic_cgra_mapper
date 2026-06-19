#include <mapper/modulo/modulo_prisa_mapper.hpp>

#include <mapper/mapper_factory.hpp>
#include <mapper/modulo/modulo_search_engine.hpp>

namespace {

const bool kModuloPhysicalPRISAMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloPhysicalPRISAMapper>(
        "ModuloPhysicalPRISAMapper");
const bool kModuloPhysicalPRISAShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloPhysicalPRISAMapper>(
        "ModuloPhysicalPRISA");
const bool kModuloPhysicalPRISAManhattanMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloPhysicalPRISAManhattanMapper>(
        "ModuloPhysicalPRISAManhattanMapper");
const bool kModuloPhysicalPRISAManhattanShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloPhysicalPRISAManhattanMapper>(
        "ModuloPhysicalPRISAManhattan");

}  // namespace

namespace mapper {

ModuloPhysicalPRISAMapperBase::ModuloPhysicalPRISAMapperBase(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr,
    detail::ModuloPlacementSearchKind search_kind)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr), search_kind_(search_kind) {}

MappingResult ModuloPhysicalPRISAMapperBase::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.max_iterations = max_iterations_;
  return detail::RunModuloPlacementSearchMapper(
      dfg_ptr_, mrrg_ptr_, search_kind_, timeout_s_, log_file_path_, options);
}

void ModuloPhysicalPRISAMapperBase::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloPhysicalPRISAMapperBase::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloPhysicalPRISAMapperBase::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloPhysicalPRISAMapperBase::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloPhysicalPRISAMapperBase::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloPhysicalPRISAMapperBase::SetRoutingRetryCount(
    int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloPhysicalPRISAMapperBase::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void ModuloPhysicalPRISAMapperBase::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

}  // namespace mapper
