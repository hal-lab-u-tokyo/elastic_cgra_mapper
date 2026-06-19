#include <mapper/modulo/modulo_yoto_mapper.hpp>

#include <mapper/modulo/modulo_search_engine.hpp>
#include <mapper/mapper_factory.hpp>

namespace {

const bool kModuloYOTOMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloYOTOMapper>("ModuloYOTOMapper");
const bool kModuloYOTOShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloYOTOMapper>("ModuloYOTO");
const bool kModuloPhysicalYOTOMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloPhysicalYOTOMapper>(
        "ModuloPhysicalYOTOMapper");
const bool kModuloPhysicalYOTOShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloPhysicalYOTOMapper>(
        "ModuloPhysicalYOTO");
const bool kLegacyYOTOMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloYOTOMapper>("YOTOMapper");
const bool kLegacyYOTOShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloYOTOMapper>("YOTO");

}  // namespace

namespace mapper {

ModuloYOTOMapper::ModuloYOTOMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                       const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloYOTOMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return detail::RunModuloPlacementSearchMapper(
      dfg_ptr_, mrrg_ptr_, detail::ModuloPlacementSearchKind::kYOTO, timeout_s_,
      log_file_path_, options);
}

void ModuloYOTOMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloYOTOMapper::SetTimeOut(double time_out_s) { timeout_s_ = time_out_s; }

void ModuloYOTOMapper::SetAcceptFeasibleSolution(bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloYOTOMapper::SetMaxTrials(int max_trials) { max_trials_ = max_trials; }

void ModuloYOTOMapper::SetSeedCount(int seed_count) { seed_count_ = seed_count; }

void ModuloYOTOMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloYOTOMapper::SetRandomSeed(int random_seed) { random_seed_ = random_seed; }

ModuloPhysicalYOTOMapper::ModuloPhysicalYOTOMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloPhysicalYOTOMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return detail::RunModuloPlacementSearchMapper(
      dfg_ptr_, mrrg_ptr_, detail::ModuloPlacementSearchKind::kPhysicalYOTO,
      timeout_s_, log_file_path_, options);
}

void ModuloPhysicalYOTOMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloPhysicalYOTOMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloPhysicalYOTOMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloPhysicalYOTOMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloPhysicalYOTOMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloPhysicalYOTOMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloPhysicalYOTOMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

}  // namespace mapper
