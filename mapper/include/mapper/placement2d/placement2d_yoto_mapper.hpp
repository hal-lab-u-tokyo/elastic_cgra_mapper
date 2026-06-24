#pragma once

#include <mapper/mapper.hpp>
#include <mapper/placement2d/placement2d_array_mapper_base.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper {

class Placement2DYOTOMapper : public IMapper {
 public:
  Placement2DYOTOMapper() = default;
  Placement2DYOTOMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                        const std::shared_ptr<entity::MRRG> mrrg_ptr);

  MappingResult Execution() override;
  void SetLogFilePath(const std::string& log_file_path) override;
  void SetTimeOut(double time_out_s) override;
  void SetAcceptFeasibleSolution(bool accept_feasible_solution) override;
  void SetMaxTrials(int max_trials) override;
  void SetSeedCount(int seed_count) override;
  void SetRoutingRetryCount(int routing_retry_count) override;
  void SetRandomSeed(int random_seed) override;
  void SetPlacementOnly(bool placement_only) override;

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
  bool placement_only_ = false;
};

class Placement2DFaithfulArrayYOTOMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DFaithfulArrayYOTOMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kFaithfulYOTO,
                                   dfg_ptr, mrrg_ptr) {}
};

class Placement2DCPUMappingYOTOMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DCPUMappingYOTOMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kCPUMappingYOTO,
                                   dfg_ptr, mrrg_ptr) {}
};

}  // namespace mapper
