#include "array_engine_internal.hpp"

namespace mapper::detail::placement2d {

double SecondsSince(std::chrono::steady_clock::time_point start) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now - start)
      .count();
}

Placement2DArrayEngine::Placement2DArrayEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, mapper::Placement2DArrayKind kind,
    const mapper::detail::Placement2DArrayOptions& options)
    : dfg_(dfg),
      mrrg_(mrrg),
      kind_(kind),
      timeout_s_(options.timeout_s > 0.0 ? options.timeout_s : 1.0),
      log_file_path_(options.log_file_path),
      max_trials_(options.max_trials),
      seed_count_(options.seed_count),
      random_seed_(options.random_seed),
      max_iterations_(options.max_iterations),
      elite_placement_count_(options.elite_placement_count),
      io_node_policy_(
          NormalizePolicy(options.io_node_policy.value_or("opcode"))),
      trial_seed_policy_(
          NormalizePolicy(options.trial_seed_policy.value_or("continuous"))),
      traversal_order_policy_(NormalizePolicy(
          options.traversal_order_policy.value_or("zigzag"))),
      traversal_neighbor_policy_(NormalizePolicy(
          options.traversal_neighbor_policy.value_or("default"))),
      candidate_scope_policy_(
          NormalizePolicy(options.candidate_scope_policy.value_or("default"))),
      candidate_rank_policy_(
          NormalizePolicy(options.candidate_rank_policy.value_or("default"))),
      use_yott_annotations_(options.use_yott_annotations),
      trace_trials_(options.trace_trials),
      config_(mrrg.GetMRRGConfig()),
      base_seed_(static_cast<unsigned int>(
          0x85EBCA6Bu ^
          static_cast<unsigned int>(dfg.GetNodeNum() * 131 + config_.row * 17 +
                                    config_.column))),
      rng_(base_seed_) {
  if (kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTTCoreRepair) {
    if (!options.candidate_scope_policy.has_value()) {
      candidate_scope_policy_ = "default";
    }
    if (!options.candidate_rank_policy.has_value()) {
      candidate_rank_policy_ = "profile_repair";
    }
    if (!options.max_iterations.has_value()) {
      max_iterations_ = 16;
    }
    if (!options.use_yott_annotations.has_value()) {
      use_yott_annotations_ = true;
    }
  } else if (kind_ == mapper::Placement2DArrayKind::kCPUMappingYOTTCore) {
    candidate_scope_policy_ = "default";
    candidate_rank_policy_ = "random";
    use_yott_annotations_ = true;
  }
  BuildDFGCache();
  critical_path_score_ = BuildCriticalPathScore();
  critical_path_max_ = critical_path_score_.empty()
                           ? 0
                           : *std::max_element(critical_path_score_.begin(),
                                               critical_path_score_.end());
  BuildGridCache();
  BuildCompatibilityCache();
  BuildPRISACache();
}

mapper::MappingResult Placement2DArrayEngine::Run() {
  const auto start = std::chrono::steady_clock::now();
  placement_swap_attempts_ = 0;
  cell_visits_ = 0;
  Log("start mapper=" + MapperName() +
      " nodes=" + std::to_string(dfg_.GetNodeNum()) +
      " cells=" + std::to_string(rows_ * cols_) +
      " max_trials=" + std::to_string(MaxTrials()) +
      " seed_count=" + std::to_string(SeedCount()));

  std::optional<PlacementState> placement;
  if (IsPRISALike()) {
    placement = RunPRISAMultiSeed(start);
  } else if (kind_ == mapper::Placement2DArrayKind::kSA) {
    placement = RunSAMultiSeed(start);
  } else if (IsCPUMappingYOTTCoreRepair()) {
    placement = RunCoreRepairMultiStart(start);
  } else if (IsCPUMappingYOTTCore()) {
    placement = RunYOTTCoreMultiStart(start);
  } else if (IsCPUMappingLike()) {
    placement = RunCPUMappingMultiStart(start);
  } else if (IsPaperGuidedTraversalLike()) {
    placement = RunPaperGuidedTraversalMultiStart(start);
  }

  if (!placement.has_value()) {
    Log("failure mapper=" + MapperName() + " reason=" +
        (last_failure_reason_.empty() ? "no placement candidate"
                                      : last_failure_reason_));
    return mapper::MappingResult(false, entity::Mapping(config_),
                                 SecondsSince(start));
  }

  std::vector<int> dfg_to_mrrg(dfg_.GetNodeNum(), -1);
  for (int node = 0; node < dfg_.GetNodeNum(); node++) {
    const int cell = placement->dfg_to_cell[node];
    if (cell < 0 || cell >= static_cast<int>(cell_to_mrrg_.size())) {
      return mapper::MappingResult(false, entity::Mapping(config_),
                                   SecondsSince(start));
    }
    if (UsesStructuralIOCellTypes() &&
        !IsCPUMappingCompatible(node, cell)) {
      Log("failure mapper=" + MapperName() +
          " reason=final placement violates structural I/O node=" +
          NodeLabel(node) + " cell=" + std::to_string(cell));
      return mapper::MappingResult(false, entity::Mapping(config_),
                                   SecondsSince(start));
    }
    dfg_to_mrrg[node] = cell_to_mrrg_[cell];
  }

  std::vector<std::vector<int>> empty_routes(dfg_.GetNodeNum());
  entity::Mapping mapping(mrrg_, dfg_, dfg_to_mrrg, empty_routes);
  const double elapsed = SecondsSince(start);
  Log("success mapper=" + MapperName() + " time_s=" + std::to_string(elapsed) +
      " placement_cost=" + std::to_string(PlacementCost(*placement)) +
      " placement_swap_attempts=" + std::to_string(placement_swap_attempts_) +
      " cell_visits=" + std::to_string(cell_visits_));
  return mapper::MappingResult(true, mapping, elapsed);
}

}  // namespace mapper::detail::placement2d

namespace mapper::detail {

MappingResult RunPlacement2DArrayEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    const Placement2DArrayOptions& options) {
  placement2d::Placement2DArrayEngine engine(dfg, mrrg, kind, options);
  return engine.Run();
}

}  // namespace mapper::detail
