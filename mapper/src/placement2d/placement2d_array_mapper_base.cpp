#include <mapper/placement2d/placement2d_array_mapper_base.hpp>

namespace mapper {

// Thin frontend shared by direct-grid 2D placement mappers.
//
// Execution path:
//   mapper-specific Algorithm.type
//     -> Placement2DArrayMapperBase(kind)
//     -> detail::RunPlacement2DArrayEngine(...)
//     -> engine/array_engine.cpp
//
// Add algorithm behavior in engine/*.cpp. Keep this file limited to forwarding
// JSON-configurable mapper options.

Placement2DArrayMapperBase::Placement2DArrayMapperBase(
    Placement2DArrayKind kind, const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : kind_(kind), dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

MappingResult Placement2DArrayMapperBase::Execution() {
  return detail::RunPlacement2DArrayEngine(*dfg_ptr_, *mrrg_ptr_, kind_,
                                           options_);
}

void Placement2DArrayMapperBase::SetLogFilePath(
    const std::string& log_file_path) {
  options_.log_file_path = log_file_path;
}

void Placement2DArrayMapperBase::SetTimeOut(double time_out_s) {
  options_.timeout_s = time_out_s;
}

void Placement2DArrayMapperBase::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  (void)accept_feasible_solution;
}

void Placement2DArrayMapperBase::SetMaxTrials(int max_trials) {
  options_.max_trials = max_trials;
}

void Placement2DArrayMapperBase::SetSeedCount(int seed_count) {
  options_.seed_count = seed_count;
}

void Placement2DArrayMapperBase::SetRandomSeed(int random_seed) {
  options_.random_seed = random_seed;
}

void Placement2DArrayMapperBase::SetMaxIterations(int max_iterations) {
  options_.max_iterations = max_iterations;
}

void Placement2DArrayMapperBase::SetElitePlacementCount(
    int elite_placement_count) {
  options_.elite_placement_count = elite_placement_count;
}

void Placement2DArrayMapperBase::SetIONodePolicy(
    const std::string& io_node_policy) {
  options_.io_node_policy = io_node_policy;
}

void Placement2DArrayMapperBase::SetTrialSeedPolicy(
    const std::string& trial_seed_policy) {
  options_.trial_seed_policy = trial_seed_policy;
}

void Placement2DArrayMapperBase::SetTraversalOrderPolicy(
    const std::string& traversal_order_policy) {
  options_.traversal_order_policy = traversal_order_policy;
}

void Placement2DArrayMapperBase::SetTraversalNeighborPolicy(
    const std::string& traversal_neighbor_policy) {
  options_.traversal_neighbor_policy = traversal_neighbor_policy;
}

void Placement2DArrayMapperBase::SetCandidateScopePolicy(
    const std::string& candidate_scope_policy) {
  options_.candidate_scope_policy = candidate_scope_policy;
}

void Placement2DArrayMapperBase::SetCandidateRankPolicy(
    const std::string& candidate_rank_policy) {
  options_.candidate_rank_policy = candidate_rank_policy;
}

void Placement2DArrayMapperBase::SetUseYOTTAnnotations(
    bool use_yott_annotations) {
  options_.use_yott_annotations = use_yott_annotations;
}

void Placement2DArrayMapperBase::SetTraceTrials(bool trace_trials) {
  options_.trace_trials = trace_trials;
}

}  // namespace mapper
