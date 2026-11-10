#include <mapper/prisa/engine/prisa_engine.hpp>
#include <mapper/prisa/prisa_options.hpp>
#include <mapper/prisa/routed_prisa_mapper.hpp>
#include <stdexcept>

namespace mapper::prisa {
namespace {
MappingResult Execute(PrisaEngine& engine) {
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

MappingResult RunPRISASearch(const std::shared_ptr<entity::DFG>& dfg_ptr,
                             const std::shared_ptr<entity::MRRG>& mrrg_ptr,
                             PlacementSearchKind search_kind,
                             const std::optional<double>& timeout_s,
                             const std::optional<std::string>& log_file_path,
                             const PlacementSearchOptions& options) {
  const double effective_timeout = timeout_s.value_or(1.0);
  // One context is a spatial array; multiple contexts require context-aware
  // mapping.
  if (search_kind == PlacementSearchKind::kAutoPRISA) {
    search_kind = mrrg_ptr->GetMRRGConfig().context_size > 1
                      ? PlacementSearchKind::kModuloPhysicalPRISA
                      : PlacementSearchKind::kPlacement2DPRISA;
  }
  switch (search_kind) {
    case PlacementSearchKind::kModuloPhysicalPRISA:
    case PlacementSearchKind::kModuloPhysicalPRISAManhattan:
    case PlacementSearchKind::kPlacement2DPRISA:
    case PlacementSearchKind::kPlacement2DPRISANoSIS:
    case PlacementSearchKind::kPlacement2DCostAwarePRISA:
      break;
    default:
      throw std::invalid_argument("invalid prisa search variant");
  }
  PrisaEngine engine(*dfg_ptr, *mrrg_ptr, search_kind, effective_timeout,
                     log_file_path, options);
  return Execute(engine);
}

MappingResult RunPrisaArray(entity::DFG& dfg, entity::MRRG& mrrg,
                            Placement2DArrayKind kind,
                            const Placement2DArrayOptions& options) {
  switch (kind) {
    case mapper::prisa::Placement2DArrayKind::kPRISA:
    case mapper::prisa::Placement2DArrayKind::kPRISANoSIS:
    case mapper::prisa::Placement2DArrayKind::kCostAwarePRISA:
      break;
    default:
      throw std::invalid_argument("invalid prisa array variant");
  }
  PrisaEngine engine(dfg, mrrg, kind, options);
  return Execute(engine);
}
}  // namespace mapper::prisa

namespace mapper {

ModuloPhysicalPRISAMapper::ModuloPhysicalPRISAMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloPhysicalPRISAMapper::Execution() {
  prisa::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.max_iterations = max_iterations_;
  return prisa::RunPRISASearch(dfg_ptr_, mrrg_ptr_, search_kind_, timeout_s_,
                               log_file_path_, options);
}

void ModuloPhysicalPRISAMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloPhysicalPRISAMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloPhysicalPRISAMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloPhysicalPRISAMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloPhysicalPRISAMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloPhysicalPRISAMapper::SetRoutingRetryCount(int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloPhysicalPRISAMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void ModuloPhysicalPRISAMapper::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

ModuloPhysicalPRISAManhattanMapper::ModuloPhysicalPRISAManhattanMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloPhysicalPRISAManhattanMapper::Execution() {
  prisa::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.max_iterations = max_iterations_;
  return prisa::RunPRISASearch(dfg_ptr_, mrrg_ptr_, search_kind_, timeout_s_,
                               log_file_path_, options);
}

void ModuloPhysicalPRISAManhattanMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void ModuloPhysicalPRISAManhattanMapper::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void ModuloPhysicalPRISAManhattanMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void ModuloPhysicalPRISAManhattanMapper::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void ModuloPhysicalPRISAManhattanMapper::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void ModuloPhysicalPRISAManhattanMapper::SetRoutingRetryCount(
    int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void ModuloPhysicalPRISAManhattanMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void ModuloPhysicalPRISAManhattanMapper::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

}  // namespace mapper
