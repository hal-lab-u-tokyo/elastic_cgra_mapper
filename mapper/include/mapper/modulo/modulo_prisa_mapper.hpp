#pragma once

#include <mapper/mapper.hpp>
#include <mapper/modulo/modulo_search_engine.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper {

class ModuloPhysicalPRISAMapperBase : public IMapper {
 public:
  ModuloPhysicalPRISAMapperBase() = default;
  ModuloPhysicalPRISAMapperBase(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr,
      detail::ModuloPlacementSearchKind search_kind);

  MappingResult Execution() override;
  void SetLogFilePath(const std::string& log_file_path) override;
  void SetTimeOut(double time_out_s) override;
  void SetAcceptFeasibleSolution(bool accept_feasible_solution) override;
  void SetMaxTrials(int max_trials) override;
  void SetSeedCount(int seed_count) override;
  void SetRoutingRetryCount(int routing_retry_count) override;
  void SetRandomSeed(int random_seed) override;
  void SetMaxIterations(int max_iterations) override;

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  detail::ModuloPlacementSearchKind search_kind_ =
      detail::ModuloPlacementSearchKind::kPhysicalPRISA;
  std::optional<std::string> log_file_path_;
  std::optional<double> timeout_s_;
  bool accept_feasible_solution_ = true;
  std::optional<int> max_trials_;
  std::optional<int> seed_count_;
  std::optional<int> routing_retry_count_;
  std::optional<int> random_seed_;
  std::optional<int> max_iterations_;
};

// PRISA-style 2D physical placement followed by deterministic modulo context
// assignment and the shared CGRA BFS/maze router.
class ModuloPhysicalPRISAMapper : public ModuloPhysicalPRISAMapperBase {
 public:
  ModuloPhysicalPRISAMapper() = default;
  ModuloPhysicalPRISAMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                            const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : ModuloPhysicalPRISAMapperBase(
            dfg_ptr, mrrg_ptr,
            detail::ModuloPlacementSearchKind::kPhysicalPRISA) {}
};

// PRISA-style 2D physical placement followed by deterministic modulo context
// assignment and Manhattan-prioritized legal routing on this repository's MRRG.
class ModuloPhysicalPRISAManhattanMapper
    : public ModuloPhysicalPRISAMapperBase {
 public:
  ModuloPhysicalPRISAManhattanMapper() = default;
  ModuloPhysicalPRISAManhattanMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr)
      : ModuloPhysicalPRISAMapperBase(
            dfg_ptr, mrrg_ptr,
            detail::ModuloPlacementSearchKind::kPhysicalPRISAManhattan) {}
};

}  // namespace mapper
