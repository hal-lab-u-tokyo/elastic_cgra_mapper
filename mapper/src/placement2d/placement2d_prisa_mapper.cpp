#include <mapper/placement2d/placement2d_prisa_mapper.hpp>

#include <mapper/mapper_factory.hpp>
#include <mapper/placement2d/placement2d_search_engine.hpp>

// This file is the PRISA-facing mapper entry point. It keeps the paper-faithful
// PRISA, no-SIS, cost-aware, and array variants in one place while forwarding
// the algorithm body to the shared engines.

namespace {

const bool kPlacement2DPRISAMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DPRISAMapper>(
        "Placement2DPRISAMapper");
const bool kPlacement2DPRISAShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DPRISAMapper>(
        "Placement2DPRISA");
const bool kPlacement2DPRISANoSISMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DPRISANoSISMapper>(
        "Placement2DPRISANoSISMapper");
const bool kPlacement2DPRISANoSISShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DPRISANoSISMapper>(
        "Placement2DPRISANoSIS");
const bool kPlacement2DCostAwarePRISAMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DCostAwarePRISAMapper>(
        "Placement2DCostAwarePRISAMapper");
const bool kPlacement2DCostAwarePRISAShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DCostAwarePRISAMapper>(
        "Placement2DCostAwarePRISA");
const bool kPlacement2DArrayPRISAMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayPRISAMapper>(
        "Placement2DArrayPRISAMapper");
const bool kPlacement2DArrayPRISAShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayPRISAMapper>(
        "Placement2DArrayPRISA");
const bool kPlacement2DArrayPRISANoSISMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayPRISANoSISMapper>(
        "Placement2DArrayPRISANoSISMapper");
const bool kPlacement2DArrayPRISANoSISShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayPRISANoSISMapper>(
        "Placement2DArrayPRISANoSIS");
const bool kPlacement2DArrayCostAwarePRISAMapperRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayCostAwarePRISAMapper>(
        "Placement2DArrayCostAwarePRISAMapper");
const bool kPlacement2DArrayCostAwarePRISAShortNameRegistered =
    mapper::RegisterMapperType<mapper::Placement2DArrayCostAwarePRISAMapper>(
        "Placement2DArrayCostAwarePRISA");

}  // namespace

namespace mapper {

Placement2DPRISAMapperBase::Placement2DPRISAMapperBase(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr, bool use_sis,
    bool cost_aware)
    : use_sis_(use_sis),
      cost_aware_(cost_aware),
      dfg_ptr_(dfg_ptr),
      mrrg_ptr_(mrrg_ptr) {}

MappingResult Placement2DPRISAMapperBase::Execution() {
  detail::PlacementSearchOptions options;
  options.max_trials = max_trials_;
  options.seed_count = seed_count_;
  options.routing_retry_count = routing_retry_count_;
  options.random_seed = random_seed_;
  options.max_iterations = max_iterations_;
  options.placement_only = placement_only_;
  detail::Placement2DSearchKind search_kind =
      cost_aware_ ? detail::Placement2DSearchKind::kCostAwarePRISA
                  : (use_sis_ ? detail::Placement2DSearchKind::kPRISA
                              : detail::Placement2DSearchKind::kPRISANoSIS);
  return detail::RunPlacement2DSearchMapper(
      dfg_ptr_, mrrg_ptr_, search_kind, timeout_s_, log_file_path_, options);
}

void Placement2DPRISAMapperBase::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void Placement2DPRISAMapperBase::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void Placement2DPRISAMapperBase::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void Placement2DPRISAMapperBase::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void Placement2DPRISAMapperBase::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void Placement2DPRISAMapperBase::SetRoutingRetryCount(
    int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void Placement2DPRISAMapperBase::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void Placement2DPRISAMapperBase::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

void Placement2DPRISAMapperBase::SetPlacementOnly(bool placement_only) {
  placement_only_ = placement_only;
}

}  // namespace mapper
