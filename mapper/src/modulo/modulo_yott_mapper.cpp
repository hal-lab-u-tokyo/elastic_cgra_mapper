#include <mapper/modulo/modulo_yott_mapper.hpp>

#include <mapper/modulo/modulo_search_engine.hpp>
#include <mapper/mapper_factory.hpp>

namespace {

const bool kModuloYOTTMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloYOTTMapper>("ModuloYOTTMapper");
const bool kModuloYOTTShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloYOTTMapper>("ModuloYOTT");
const bool kModuloPhysicalYOTTMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloPhysicalYOTTMapper>(
        "ModuloPhysicalYOTTMapper");
const bool kModuloPhysicalYOTTShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloPhysicalYOTTMapper>(
        "ModuloPhysicalYOTT");
const bool kLegacyYOTTMapperRegistered =
    mapper::RegisterMapperType<mapper::ModuloYOTTMapper>("YOTTMapper");
const bool kLegacyYOTTShortNameRegistered =
    mapper::RegisterMapperType<mapper::ModuloYOTTMapper>("YOTT");

}  // namespace

namespace mapper {

ModuloYOTTMapper::ModuloYOTTMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                       const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloYOTTMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return detail::RunModuloPlacementSearchMapper(
      dfg_ptr_, mrrg_ptr_, detail::ModuloPlacementSearchKind::kYOTT, timeout_s_,
      log_file_path_, options);
}

void ModuloYOTTMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloYOTTMapper::SetTimeOut(double time_out_s) { timeout_s_ = time_out_s; }

void ModuloYOTTMapper::SetAcceptFeasibleSolution(bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloYOTTMapper::SetMaxTrials(int max_trials) { max_trials_ = max_trials; }

void ModuloYOTTMapper::SetSeedCount(int seed_count) { seed_count_ = seed_count; }

void ModuloYOTTMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloYOTTMapper::SetRandomSeed(int random_seed) { random_seed_ = random_seed; }

ModuloPhysicalYOTTMapper::ModuloPhysicalYOTTMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloPhysicalYOTTMapper::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return detail::RunModuloPlacementSearchMapper(
      dfg_ptr_, mrrg_ptr_, detail::ModuloPlacementSearchKind::kPhysicalYOTT,
      timeout_s_, log_file_path_, options);
}

void ModuloPhysicalYOTTMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloPhysicalYOTTMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloPhysicalYOTTMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloPhysicalYOTTMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloPhysicalYOTTMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloPhysicalYOTTMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloPhysicalYOTTMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

}  // namespace mapper
