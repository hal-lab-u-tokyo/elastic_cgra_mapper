#pragma once

#include <mapper/yoto/yoto_options.hpp>
#include <memory>

namespace mapper {

class Placement2DYOTOMapper : public IMapper {
 public:
  Placement2DYOTOMapper(std::shared_ptr<entity::DFG> dfg,
                        std::shared_ptr<entity::MRRG> mrrg)
      : dfg_ptr_(dfg), mrrg_ptr_(mrrg) {}
  MappingResult Execution() override {
    return yoto::RunYOTOSearch(dfg_ptr_, mrrg_ptr_,
                               yoto::PlacementSearchKind::kPlacement2DYOTO,
                               timeout_s_, log_file_path_, options_);
  }
  void SetMaxTrials(int value) override { options_.max_trials = value; }
  void SetSeedCount(int value) override { options_.seed_count = value; }
  void SetRandomSeed(int value) override { options_.random_seed = value; }
  void SetMaxIterations(int value) override { options_.max_iterations = value; }
  void SetRoutingRetryCount(int value) override {
    options_.routing_retry_count = value;
  }
  void SetPlacementOnly(bool value) override {
    options_.placement_only = value;
  }
  void SetAcceptFeasibleSolution(bool) override {}
  void SetLogFilePath(const std::string& value) override {
    log_file_path_ = value;
  }
  void SetTimeOut(double value) override { timeout_s_ = value; }

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  yoto::PlacementSearchOptions options_;
  std::optional<double> timeout_s_;
  std::optional<std::string> log_file_path_;
};

class Placement2DPaperGuidedArrayYOTOMapper : public IMapper {
 public:
  Placement2DPaperGuidedArrayYOTOMapper(std::shared_ptr<entity::DFG> dfg,
                                        std::shared_ptr<entity::MRRG> mrrg)
      : dfg_ptr_(dfg), mrrg_ptr_(mrrg) {}
  MappingResult Execution() override {
    return yoto::RunYotoArray(*dfg_ptr_, *mrrg_ptr_,
                              yoto::Placement2DArrayKind::kPaperGuidedYOTO,
                              options_);
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
  yoto::Placement2DArrayOptions options_;
};

class Placement2DCPUMappingYOTOMapper : public IMapper {
 public:
  Placement2DCPUMappingYOTOMapper(std::shared_ptr<entity::DFG> dfg,
                                  std::shared_ptr<entity::MRRG> mrrg)
      : dfg_ptr_(dfg), mrrg_ptr_(mrrg) {}
  MappingResult Execution() override {
    return yoto::RunYotoArray(*dfg_ptr_, *mrrg_ptr_,
                              yoto::Placement2DArrayKind::kCPUMappingYOTO,
                              options_);
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
  yoto::Placement2DArrayOptions options_;
};

}  // namespace mapper
