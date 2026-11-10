#include <mapper/yoto/engine/yoto_engine.hpp>
#include <mapper/yoto/routed_yoto_mapper.hpp>
#include <mapper/yoto/routed_yoto_with_fallback_mapper.hpp>
#include <mapper/yoto/yoto_options.hpp>
#include <stdexcept>

namespace mapper::yoto {
namespace {
MappingResult Execute(YotoEngine& engine) {
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

MappingResult RunYOTOSearch(const std::shared_ptr<entity::DFG>& dfg_ptr,
                            const std::shared_ptr<entity::MRRG>& mrrg_ptr,
                            PlacementSearchKind search_kind,
                            const std::optional<double>& timeout_s,
                            const std::optional<std::string>& log_file_path,
                            const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  // One context is a spatial array; multiple contexts require context-aware
  // mapping.
  if (search_kind == PlacementSearchKind::kAutoYOTO) {
    search_kind = mrrg_ptr->GetMRRGConfig().context_size > 1
                      ? PlacementSearchKind::kYOTO
                      : PlacementSearchKind::kPlacement2DYOTO;
  }
  switch (search_kind) {
    case PlacementSearchKind::kYOTO:
    case PlacementSearchKind::kModuloPhysicalYOTO:
    case PlacementSearchKind::kYOTOWithFallback:
    case PlacementSearchKind::kPlacement2DYOTO:
      break;
    default:
      throw std::invalid_argument("invalid yoto search variant");
  }
  YotoEngine engine(*dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                    log_file_path, options);
  return Execute(engine);
}

MappingResult RunYotoArray(entity::DFG& dfg, entity::MRRG& mrrg,
                           Placement2DArrayKind kind,
                           const Placement2DArrayOptions& options) {
  switch (kind) {
    case mapper::yoto::Placement2DArrayKind::kPaperGuidedYOTO:
    case mapper::yoto::Placement2DArrayKind::kCPUMappingYOTO:
      break;
    default:
      throw std::invalid_argument("invalid yoto array variant");
  }
  YotoEngine engine(dfg, mrrg, kind, options);
  return Execute(engine);
}
}  // namespace mapper::yoto

namespace mapper {

ModuloYOTOMapper::ModuloYOTOMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                                   const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloYOTOMapper::Execution() {
  yoto::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return yoto::RunYOTOSearch(dfg_ptr_, mrrg_ptr_,
                             yoto::PlacementSearchKind::kYOTO, timeout_s_,
                             log_file_path_, options);
}

void ModuloYOTOMapper::SetLogFilePath(const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloYOTOMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloYOTOMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloYOTOMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloYOTOMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloYOTOMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloYOTOMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

ModuloPhysicalYOTOMapper::ModuloPhysicalYOTOMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloPhysicalYOTOMapper::Execution() {
  yoto::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return yoto::RunYOTOSearch(dfg_ptr_, mrrg_ptr_,
                             yoto::PlacementSearchKind::kModuloPhysicalYOTO,
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

namespace mapper {

ModuloYOTOWithFallbackMapper::ModuloYOTOWithFallbackMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloYOTOWithFallbackMapper::Execution() {
  yoto::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  return yoto::RunYOTOSearch(dfg_ptr_, mrrg_ptr_,
                             yoto::PlacementSearchKind::kYOTOWithFallback,
                             timeout_s_, log_file_path_, options);
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
