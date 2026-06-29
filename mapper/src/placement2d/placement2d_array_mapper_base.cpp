#include <mapper/placement2d/placement2d_array_mapper_base.hpp>

namespace mapper {

// Thin frontend shared by direct-grid 2D placement mappers.
//
// Execution path:
//   mapper-specific Algorithm.type
//     -> Placement2DArrayMapperBase(kind)
//     -> detail::RunPlacement2DArrayEngine(...)
//     -> engine/placement2d_array_engine.cpp
//
// Add algorithm behavior in engine/*.cpp. Keep this file limited to forwarding
// JSON-configurable mapper options.

Placement2DArrayMapperBase::Placement2DArrayMapperBase(
    Placement2DArrayKind kind, const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : kind_(kind), dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult Placement2DArrayMapperBase::Execution() {
  return detail::RunPlacement2DArrayEngine(
      *dfg_ptr_, *mrrg_ptr_, kind_, timeout_s_.value_or(1.0), log_file_path_,
      max_trials_, seed_count_, random_seed_, max_iterations_,
      cpu_mapping_bug_compatible_degree_, io_node_policy_);
}

void Placement2DArrayMapperBase::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void Placement2DArrayMapperBase::SetTimeOut(double time_out_s) {
  timeout_s_ = time_out_s;
}

void Placement2DArrayMapperBase::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}

void Placement2DArrayMapperBase::SetMaxTrials(int max_trials) {
  max_trials_ = max_trials;
}

void Placement2DArrayMapperBase::SetSeedCount(int seed_count) {
  seed_count_ = seed_count;
}

void Placement2DArrayMapperBase::SetRoutingRetryCount(
    int routing_retry_count) {
  routing_retry_count_ = routing_retry_count;
}

void Placement2DArrayMapperBase::SetRandomSeed(int random_seed) {
  random_seed_ = random_seed;
}

void Placement2DArrayMapperBase::SetMaxIterations(int max_iterations) {
  max_iterations_ = max_iterations;
}

void Placement2DArrayMapperBase::SetPlacementOnly(bool placement_only) {
  placement_only_ = placement_only;
}

void Placement2DArrayMapperBase::SetCPUMappingBugCompatibleDegree(
    bool cpu_mapping_bug_compatible_degree) {
  cpu_mapping_bug_compatible_degree_ = cpu_mapping_bug_compatible_degree;
}

void Placement2DArrayMapperBase::SetIONodePolicy(
    const std::string& io_node_policy) {
  io_node_policy_ = io_node_policy;
}

}  // namespace mapper
