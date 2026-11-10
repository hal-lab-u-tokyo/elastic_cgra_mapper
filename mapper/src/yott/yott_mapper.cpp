#include <mapper/yott/engine/yott_engine.hpp>
#include <mapper/yott/routed_yott_mapper.hpp>
#include <mapper/yott/routed_yott_with_fallback_mapper.hpp>
#include <mapper/yott/yott_options.hpp>
#include <stdexcept>

namespace mapper::yott {
namespace {
MappingResult Execute(YottEngine& engine) {
  const auto start = engine.StartSearch();
  auto placement = engine.SearchPlacement(start);
  if (!placement) return engine.Failure(start);
  if (engine.PlacementOnly())
    return engine.FinishPlacementOnly(*placement, start);
  auto routes = engine.RoutePlacement(*placement);
  if (!routes) return engine.Failure(start);
  return engine.FinishRoutedMapping(*placement, *routes, start);
}
}  // namespace

MappingResult RunYOTTSearch(const std::shared_ptr<entity::DFG>& dfg_ptr,
                            const std::shared_ptr<entity::MRRG>& mrrg_ptr,
                            PlacementSearchKind search_kind,
                            const std::optional<double>& timeout_s,
                            const std::optional<std::string>& log_file_path,
                            const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  // One context is a spatial array; multiple contexts require context-aware
  // mapping.
  if (search_kind == PlacementSearchKind::kAutoYOTT) {
    search_kind = mrrg_ptr->GetMRRGConfig().context_size > 1
                      ? PlacementSearchKind::kYOTT
                      : PlacementSearchKind::kPlacement2DYOTT;
  }
  switch (search_kind) {
    case PlacementSearchKind::kYOTT:
    case PlacementSearchKind::kModuloPhysicalYOTT:
    case PlacementSearchKind::kYOTTWithFallback:
    case PlacementSearchKind::kPlacement2DYOTT:
      break;
    default:
      throw std::invalid_argument("invalid yott search variant");
  }
  YottEngine engine(*dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                    log_file_path, options);
  return Execute(engine);
}

MappingResult RunYottArray(entity::DFG& dfg, entity::MRRG& mrrg,
                           Placement2DArrayKind kind,
                           const Placement2DArrayOptions& options) {
  switch (kind) {
    case mapper::yott::Placement2DArrayKind::kPaperGuidedYOTT:
    case mapper::yott::Placement2DArrayKind::kCPUMappingYOTT:
      break;
    default:
      throw std::invalid_argument("invalid yott array variant");
  }
  YottEngine engine(dfg, mrrg, kind, options);
  return Execute(engine);
}
}  // namespace mapper::yott

namespace mapper {

ModuloYOTTMapper::ModuloYOTTMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                                   const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloYOTTMapper::Execution() {
  yott::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return yott::RunYOTTSearch(dfg_ptr_, mrrg_ptr_,
                             yott::PlacementSearchKind::kYOTT, timeout_s_,
                             log_file_path_, options);
}

void ModuloYOTTMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloYOTTMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloYOTTMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloYOTTMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloYOTTMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloYOTTMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloYOTTMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

ModuloPhysicalYOTTMapper::ModuloPhysicalYOTTMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloPhysicalYOTTMapper::Execution() {
  yott::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return yott::RunYOTTSearch(dfg_ptr_, mrrg_ptr_,
                             yott::PlacementSearchKind::kModuloPhysicalYOTT,
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

namespace mapper {

ModuloYOTTWithFallbackMapper::ModuloYOTTWithFallbackMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloYOTTWithFallbackMapper::Execution() {
  yott::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return yott::RunYOTTSearch(dfg_ptr_, mrrg_ptr_,
                             yott::PlacementSearchKind::kYOTTWithFallback,
                             timeout_s_, log_file_path_, options);
}

void ModuloYOTTWithFallbackMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloYOTTWithFallbackMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloYOTTWithFallbackMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloYOTTWithFallbackMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloYOTTWithFallbackMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloYOTTWithFallbackMapper::SetRoutingRetryCount(
    int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloYOTTWithFallbackMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

}  // namespace mapper
