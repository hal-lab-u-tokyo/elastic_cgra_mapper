#pragma once

#include <mapper/mapper.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper {

// Direct-grid engine variants. Method-specific mapper classes expose these as
// normal Algorithm.type names; the enum is only the internal dispatch key.
enum class Placement2DArrayKind {
  kYOTO,
  kYOTT,
  kSA,
  kCPUMappingYOTO,
  kCPUMappingYOTT,
  kPRISA,
  kPRISANoSIS,
  kCostAwarePRISA,
};

class Placement2DArrayMapperBase : public IMapper {
 public:
  Placement2DArrayMapperBase(
      Placement2DArrayKind kind, const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr);

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

 protected:
  Placement2DArrayKind kind_;
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  std::optional<std::string> log_file_path_;
  std::optional<double> timeout_s_;
  bool accept_feasible_solution_ = true;
  std::optional<int> max_trials_;
  std::optional<int> seed_count_;
  std::optional<int> routing_retry_count_;
  std::optional<int> random_seed_;
  std::optional<int> max_iterations_;
  bool placement_only_ = false;
};

namespace detail {

MappingResult RunPlacement2DArrayEngine(
    entity::DFG& dfg, entity::MRRG& mrrg, Placement2DArrayKind kind,
    double timeout_s, const std::optional<std::string>& log_file_path,
    std::optional<int> max_trials, std::optional<int> seed_count,
    std::optional<int> random_seed, std::optional<int> max_iterations);

}  // namespace detail

}  // namespace mapper
