#pragma once

#include <mapper/mapper.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper {

enum class Placement2DArrayKind { kYOTO, kYOTT, kSA };

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

class Placement2DArrayYOTOMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayYOTOMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kYOTO, dfg_ptr,
                                   mrrg_ptr) {}
};

class Placement2DArrayYOTTMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayYOTTMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kYOTT, dfg_ptr,
                                   mrrg_ptr) {}
};

class Placement2DArraySAMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArraySAMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kSA, dfg_ptr,
                                   mrrg_ptr) {}
};

}  // namespace mapper
