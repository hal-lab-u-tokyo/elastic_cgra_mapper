#pragma once
#include <mapper/yoto/yoto_options.hpp>
namespace mapper {
class YOTOMapper : public IMapper {
 public:
  YOTOMapper(std::shared_ptr<entity::DFG> dfg,
             std::shared_ptr<entity::MRRG> mrrg)
      : dfg_ptr_(dfg), mrrg_ptr_(mrrg) {}
  MappingResult Execution() override {
    return yoto::RunYOTOSearch(dfg_ptr_, mrrg_ptr_,
                               yoto::PlacementSearchKind::kAutoYOTO, timeout_s_,
                               log_file_path_, options_);
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
}  // namespace mapper
