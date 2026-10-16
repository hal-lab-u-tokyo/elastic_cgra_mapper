#pragma once

#include <mapper/mapper.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper {

// YOTT-style two-pass traversal: first build an annotated edge sequence, then
// greedily place with I/O, look-ahead, reconvergence, and degree matching.
// Routing is validated with this repository's MRRG/resource model.
class ModuloYOTTMapper : public IMapper {
 public:
  ModuloYOTTMapper() = default;
  ModuloYOTTMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                   const std::shared_ptr<entity::MRRG> mrrg_ptr);

  MappingResult Execution() override;
  void SetLogFilePath(const std::string& log_file_path) override;
  void SetTimeOut(double time_out_s) override;
  void SetAcceptFeasibleSolution(bool accept_feasible_solution) override;
  void SetMaxTrials(int max_trials) override;
  void SetSeedCount(int seed_count) override;
  void SetRoutingRetryCount(int routing_retry_count) override;
  void SetRandomSeed(int random_seed) override;

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  std::optional<std::string> log_file_path_;
  std::optional<double> timeout_s_;
  bool accept_feasible_solution_ = true;
  std::optional<int> max_trials_;
  std::optional<int> seed_count_;
  std::optional<int> routing_retry_count_;
  std::optional<int> random_seed_;
};

// YOTT-style annotated physical placement followed by deterministic modulo
// context assignment and route validation. Use this to study how paper-style
// placement transfers to the modulo routing problem.
class ModuloPhysicalYOTTMapper : public IMapper {
 public:
  ModuloPhysicalYOTTMapper() = default;
  ModuloPhysicalYOTTMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                           const std::shared_ptr<entity::MRRG> mrrg_ptr);

  MappingResult Execution() override;
  void SetLogFilePath(const std::string& log_file_path) override;
  void SetTimeOut(double time_out_s) override;
  void SetAcceptFeasibleSolution(bool accept_feasible_solution) override;
  void SetMaxTrials(int max_trials) override;
  void SetSeedCount(int seed_count) override;
  void SetRoutingRetryCount(int routing_retry_count) override;
  void SetRandomSeed(int random_seed) override;

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  std::optional<std::string> log_file_path_;
  std::optional<double> timeout_s_;
  bool accept_feasible_solution_ = true;
  std::optional<int> max_trials_;
  std::optional<int> seed_count_;
  std::optional<int> routing_retry_count_;
  std::optional<int> random_seed_;
};

}  // namespace mapper
