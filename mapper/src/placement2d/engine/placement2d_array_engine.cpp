#include "placement2d_array_engine_internal.hpp"

namespace mapper::detail::placement2d {

double SecondsSince(std::chrono::steady_clock::time_point start) {
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now - start)
      .count();
}

Placement2DArrayEngine::Placement2DArrayEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, mapper::Placement2DArrayKind kind,
    double timeout_s, const std::optional<std::string>& log_file_path,
    std::optional<int> max_trials, std::optional<int> seed_count,
    std::optional<int> random_seed, std::optional<int> max_iterations,
    std::optional<bool> cpu_mapping_bug_compatible_degree,
    const std::optional<std::string>& io_node_policy)
    : dfg_(dfg),
      mrrg_(mrrg),
      kind_(kind),
      timeout_s_(timeout_s > 0.0 ? timeout_s : 1.0),
      log_file_path_(log_file_path),
      max_trials_(max_trials),
      seed_count_(seed_count),
      random_seed_(random_seed),
      max_iterations_(max_iterations),
      cpu_mapping_bug_compatible_degree_(
          cpu_mapping_bug_compatible_degree.value_or(true)),
      io_node_policy_(NormalizePolicy(io_node_policy.value_or("opcode"))),
      config_(mrrg.GetMRRGConfig()),
      base_seed_(static_cast<unsigned int>(
          0x85EBCA6Bu ^ static_cast<unsigned int>(dfg.GetNodeNum() * 131 +
                                                  config_.row * 17 +
                                                  config_.column))),
      rng_(base_seed_) {
  BuildDFGCache();
  BuildGridCache();
  BuildCompatibilityCache();
  BuildPRISACache();
}

mapper::MappingResult Placement2DArrayEngine::Run() {
  const auto start = std::chrono::steady_clock::now();
  Log("start mapper=" + MapperName() + " nodes=" +
      std::to_string(dfg_.GetNodeNum()) + " cells=" +
      std::to_string(rows_ * cols_) + " max_trials=" +
      std::to_string(MaxTrials()) + " seed_count=" +
      std::to_string(SeedCount()));

  std::optional<PlacementState> placement;
  if (IsPRISALike()) {
    placement = RunPRISAMultiSeed(start);
  } else if (kind_ == mapper::Placement2DArrayKind::kSA) {
    placement = RunSAMultiSeed(start);
  } else if (IsCPUMappingLike()) {
    placement = RunCPUMappingMultiStart(start);
  } else if (IsFaithfulArrayTraversalLike()) {
    placement = RunFaithfulTraversalMultiStart(start);
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
    dfg_to_mrrg[node] = cell_to_mrrg_[cell];
  }

  std::vector<std::vector<int>> empty_routes(dfg_.GetNodeNum());
  entity::Mapping mapping(mrrg_, dfg_, dfg_to_mrrg, empty_routes);
  const double elapsed = SecondsSince(start);
  Log("success mapper=" + MapperName() + " time_s=" +
      std::to_string(elapsed) + " placement_cost=" +
      std::to_string(PlacementCost(*placement)) + " cell_visits=" +
      std::to_string(cell_visits_));
  return mapper::MappingResult(true, mapping, elapsed);
}

}  // namespace mapper::detail::placement2d

namespace mapper::detail {

MappingResult RunPlacement2DArrayEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    double timeout_s, const std::optional<std::string>& log_file_path,
    std::optional<int> max_trials, std::optional<int> seed_count,
    std::optional<int> random_seed, std::optional<int> max_iterations,
    std::optional<bool> cpu_mapping_bug_compatible_degree,
    const std::optional<std::string>& io_node_policy) {
  placement2d::Placement2DArrayEngine engine(
      dfg, mrrg, kind, timeout_s, log_file_path, max_trials, seed_count,
      random_seed, max_iterations, cpu_mapping_bug_compatible_degree,
      io_node_policy);
  return engine.Run();
}

}  // namespace mapper::detail
