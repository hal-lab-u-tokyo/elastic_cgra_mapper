#pragma once
#include <mapper/mapper.hpp>
namespace mapper {
// Canonical ILP mapper. Explicit legacy classes retain their formulation.
class ILPMapper : public IMapper {
 public:
  ILPMapper(const std::shared_ptr<entity::DFG> dfg,
            const std::shared_ptr<entity::MRRG> mrrg)
      : dfg_(dfg), mrrg_(mrrg) {}
  MappingResult Execution() override;
  void SetLogFilePath(const std::string& path) override { log_ = path; }
  void SetTimeOut(double timeout) override { timeout_ = timeout; }
  void SetAcceptFeasibleSolution(bool accept) override { accept_ = accept; }

 private:
  std::shared_ptr<entity::DFG> dfg_;
  std::shared_ptr<entity::MRRG> mrrg_;
  std::optional<std::string> log_;
  std::optional<double> timeout_;
  bool accept_ = true;
};
}  // namespace mapper
