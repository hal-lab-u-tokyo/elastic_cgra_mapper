#pragma once

#include <mapper/yott_core/yott_core_options.hpp>
#include <memory>

namespace mapper {

// Minimal YOTT-derived baseline:
// random local traversal, YOTT reconvergence annotations, tip candidates,
// nearest fallback, and best-of-trials placement-cost selection.
class Placement2DCPUMappingYOTTCoreMapper : public IMapper {
 public:
  Placement2DCPUMappingYOTTCoreMapper(std::shared_ptr<entity::DFG> dfg,
                                      std::shared_ptr<entity::MRRG> mrrg)
      : dfg_ptr_(dfg), mrrg_ptr_(mrrg) {}
  MappingResult Execution() override {
    return yott_core::RunYottCoreArray(
        *dfg_ptr_, *mrrg_ptr_,
        yott_core::Placement2DArrayKind::kCPUMappingYOTTCore, options_);
  }
  void SetMaxTrials(int value) override { options_.max_trials = value; }
  void SetSeedCount(int value) override { options_.seed_count = value; }
  void SetRandomSeed(int value) override { options_.random_seed = value; }
  void SetMaxIterations(int value) override { options_.max_iterations = value; }
  void SetElitePlacementCount(int value) override {
    options_.elite_placement_count = value;
  }
  void SetIONodePolicy(const std::string& value) override {
    options_.io_node_policy = value;
  }
  void SetTrialSeedPolicy(const std::string& value) override {
    options_.trial_seed_policy = value;
  }
  void SetTraversalOrderPolicy(const std::string& value) override {
    options_.traversal_order_policy = value;
  }
  void SetTraversalNeighborPolicy(const std::string& value) override {
    options_.traversal_neighbor_policy = value;
  }
  void SetCandidateScopePolicy(const std::string& value) override {
    options_.candidate_scope_policy = value;
  }
  void SetCandidateRankPolicy(const std::string& value) override {
    options_.candidate_rank_policy = value;
  }
  void SetUseYOTTAnnotations(bool value) override {
    options_.use_yott_annotations = value;
  }
  void SetTraceTrials(bool value) override { options_.trace_trials = value; }
  void SetAcceptFeasibleSolution(bool) override {}
  void SetLogFilePath(const std::string& value) override {
    options_.log_file_path = value;
  }
  void SetTimeOut(double value) override { options_.timeout_s = value; }

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  yott_core::Placement2DArrayOptions options_;
};

}  // namespace mapper
