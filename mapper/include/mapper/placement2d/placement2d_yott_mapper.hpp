#pragma once

#include <mapper/mapper.hpp>
#include <mapper/placement2d/placement2d_array_mapper_base.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper {

class Placement2DYOTTMapper : public IMapper {
 public:
  Placement2DYOTTMapper() = default;
  Placement2DYOTTMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
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

class Placement2DArrayYOTTMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayYOTTMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kYOTT, dfg_ptr,
                                   mrrg_ptr) {}
};

class Placement2DCPUMappingYOTTMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DCPUMappingYOTTMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kCPUMappingYOTT,
                                   dfg_ptr, mrrg_ptr) {}
};

}  // namespace mapper
