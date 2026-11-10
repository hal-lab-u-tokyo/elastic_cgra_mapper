#include <mapper/sa/routed_sa_mapper.hpp>
#include <mapper/sa/engine/sa_engine.hpp>
#include <mapper/sa/sa_options.hpp>
#include <stdexcept>

namespace mapper::sa {
namespace {
MappingResult Execute(SaEngine& engine) {
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

MappingResult RunSASearch(const std::shared_ptr<entity::DFG>& dfg,
                          const std::shared_ptr<entity::MRRG>& mrrg,
                          PlacementSearchKind kind,
                          const std::optional<double>& timeout,
                          const std::optional<std::string>& log,
                          const PlacementSearchOptions& options) {
  if (kind == PlacementSearchKind::kAutoSA) {
    kind = mrrg->GetMRRGConfig().context_size > 1
               ? PlacementSearchKind::kSA
               : PlacementSearchKind::kPlacement2DSA;
  }
  if (kind != PlacementSearchKind::kSA &&
      kind != PlacementSearchKind::kPlacement2DSA)
    throw std::invalid_argument("invalid sa search variant");
  SaEngine engine(*dfg, *mrrg, kind, timeout.value_or(1.0), log, options);
  return Execute(engine);
}

MappingResult RunSaArray(entity::DFG& dfg, entity::MRRG& mrrg,
                         Placement2DArrayKind kind,
                         const Placement2DArrayOptions& options) {
  if (kind != Placement2DArrayKind::kSA)
    throw std::invalid_argument("invalid sa array variant");
  SaEngine engine(dfg, mrrg, kind, options);
  return Execute(engine);
}
}  // namespace mapper::sa

namespace mapper {

ModuloSAMapper::ModuloSAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                               const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult ModuloSAMapper::Execution() {
  sa::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.max_iterations = max_iterations_;
  return sa::RunSASearch(dfg_ptr_, mrrg_ptr_, sa::PlacementSearchKind::kSA,
                         timeout_s_, log_file_path_, options);
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

void ModuloSAMapper::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void ModuloSAMapper::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

}  // namespace mapper
