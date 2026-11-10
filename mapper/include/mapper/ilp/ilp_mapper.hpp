#pragma once

#include <mapper/mapper.hpp>

namespace mapper {
class ILPMapper : public IMapper {
 public:
  ILPMapper(const std::shared_ptr<entity::DFG> dfg,
            const std::shared_ptr<entity::MRRG> mrrg)
      : dfg_ptr_(dfg), mrrg_ptr_(mrrg) {}
  MappingResult Execution() override;
  void SetLogFilePath(const std::string& path) override {
    log_file_path_ = path;
  }
  void SetTimeOut(double timeout) override { timeout_s_ = timeout; }
  void SetAcceptFeasibleSolution(bool accept) override {
    accept_feasible_solution_ = accept;
  }

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  std::optional<std::string> log_file_path_;
  std::optional<double> timeout_s_;
  bool accept_feasible_solution_ = true;
};
}  // namespace mapper
