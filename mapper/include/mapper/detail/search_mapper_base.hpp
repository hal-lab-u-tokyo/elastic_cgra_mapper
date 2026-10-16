#pragma once

#include <mapper/mapper.hpp>
#include <mapper/detail/search_adapter.hpp>
#include <memory>
#include <optional>
#include <string>

namespace mapper {

// Shared configuration forwarding. Each algorithm owns execution and chooses
// its implementation from the MRRG; explicit legacy keys retain old variants.
class SearchMapperBase : public IMapper {
 public:
  SearchMapperBase(detail::SearchKind kind,
                              const std::shared_ptr<entity::DFG>& dfg_ptr,
                              const std::shared_ptr<entity::MRRG>& mrrg_ptr);

  MappingResult Execution() override;
  void SetLogFilePath(const std::string& log_file_path) override;
  void SetTimeOut(double time_out_s) override;
  void SetAcceptFeasibleSolution(bool accept_feasible_solution) override;
  void SetMaxTrials(int max_trials) override;
  void SetSeedCount(int seed_count) override;
  void SetRoutingRetryCount(int routing_retry_count) override;
  void SetRandomSeed(int random_seed) override;
  void SetMaxIterations(int max_iterations) override;
  void SetPlacementOnly(bool placement_only) override;

 private:
  detail::SearchKind kind_;
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  std::optional<std::string> log_file_path_;
  std::optional<double> timeout_s_;
  std::optional<int> max_trials_;
  std::optional<int> seed_count_;
  std::optional<int> routing_retry_count_;
  std::optional<int> random_seed_;
  std::optional<int> max_iterations_;
  bool placement_only_ = false;
};

}  // namespace mapper
