#include <mapper/mapper.hpp>

namespace mapper {

void IMapper::Configure(const MapperOptions& options) {
  SetLogFilePath(options.log_file_path);
  SetTimeOut(options.timeout_s);
  SetAcceptFeasibleSolution(options.accept_feasible_solution);
  SetPlacementOnly(options.placement_only);
  if (options.max_trials) SetMaxTrials(*options.max_trials);
  if (options.seed_count) SetSeedCount(*options.seed_count);
  if (options.routing_retry_count) {
    SetRoutingRetryCount(*options.routing_retry_count);
  }
  if (options.random_seed) SetRandomSeed(*options.random_seed);
  if (options.max_iterations) SetMaxIterations(*options.max_iterations);
  if (options.elite_placement_count) {
    SetElitePlacementCount(*options.elite_placement_count);
  }
  if (options.io_node_policy) SetIONodePolicy(*options.io_node_policy);
  if (options.trial_seed_policy) {
    SetTrialSeedPolicy(*options.trial_seed_policy);
  }
  if (options.traversal_order_policy) {
    SetTraversalOrderPolicy(*options.traversal_order_policy);
  }
  if (options.traversal_neighbor_policy) {
    SetTraversalNeighborPolicy(*options.traversal_neighbor_policy);
  }
  if (options.candidate_scope_policy) {
    SetCandidateScopePolicy(*options.candidate_scope_policy);
  }
  if (options.candidate_rank_policy) {
    SetCandidateRankPolicy(*options.candidate_rank_policy);
  }
  if (options.use_yott_annotations) {
    SetUseYOTTAnnotations(*options.use_yott_annotations);
  }
  if (options.trace_trials) SetTraceTrials(*options.trace_trials);
}

}  // namespace mapper
