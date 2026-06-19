#pragma once

#include <mapper/mapper.hpp>
#include <mapper/placement2d/placement2d_array_mapper_base.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper {

class Placement2DPRISAMapperBase : public IMapper {
 public:
  Placement2DPRISAMapperBase() = default;
  Placement2DPRISAMapperBase(const std::shared_ptr<entity::DFG> dfg_ptr,
                             const std::shared_ptr<entity::MRRG> mrrg_ptr,
                             bool use_sis, bool cost_aware = false);

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
  bool use_sis_ = true;
  bool cost_aware_ = false;

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
  std::optional<int> max_iterations_;
  bool placement_only_ = false;
};

class Placement2DPRISAMapper : public Placement2DPRISAMapperBase {
 public:
  Placement2DPRISAMapper() { use_sis_ = true; }
  Placement2DPRISAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                         const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DPRISAMapperBase(dfg_ptr, mrrg_ptr, true) {}
};

class Placement2DPRISANoSISMapper : public Placement2DPRISAMapperBase {
 public:
  Placement2DPRISANoSISMapper() { use_sis_ = false; }
  Placement2DPRISANoSISMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                              const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DPRISAMapperBase(dfg_ptr, mrrg_ptr, false) {}
};

class Placement2DCostAwarePRISAMapper : public Placement2DPRISAMapperBase {
 public:
  Placement2DCostAwarePRISAMapper() {
    use_sis_ = true;
    cost_aware_ = true;
  }
  Placement2DCostAwarePRISAMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DPRISAMapperBase(dfg_ptr, mrrg_ptr, true, true) {}
};

class Placement2DArrayPRISAMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayPRISAMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kPRISA, dfg_ptr,
                                   mrrg_ptr) {}
};

class Placement2DArrayPRISANoSISMapper : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayPRISANoSISMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kPRISANoSIS, dfg_ptr,
                                   mrrg_ptr) {}
};

class Placement2DArrayCostAwarePRISAMapper
    : public Placement2DArrayMapperBase {
 public:
  Placement2DArrayCostAwarePRISAMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : Placement2DArrayMapperBase(Placement2DArrayKind::kCostAwarePRISA,
                                   dfg_ptr, mrrg_ptr) {}
};

}  // namespace mapper
