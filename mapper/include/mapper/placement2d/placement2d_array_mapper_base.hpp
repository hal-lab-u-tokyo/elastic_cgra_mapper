#pragma once

#include <mapper/mapper.hpp>
#include <memory>
#include <optional>
#include <string>

namespace mapper {

// Direct-grid engine variants. Method-specific mapper classes expose these as
// normal Algorithm.type names; the enum is only the internal dispatch key.
enum class Placement2DArrayKind {
  kPaperGuidedYOTO,
  kPaperGuidedYOTT,
  kSA,
  kCPUMappingYOTO,
  kCPUMappingYOTT,
  kCPUMappingYOTTCore,
  kCPUMappingYOTTCoreRepair,
  kPRISA,
  kPRISANoSIS,
  kCostAwarePRISA,
};

namespace detail {

struct Placement2DArrayOptions {
  double timeout_s = 1.0;
  std::optional<std::string> log_file_path;
  std::optional<int> max_trials;
  std::optional<int> seed_count;
  std::optional<int> random_seed;
  std::optional<int> max_iterations;
  std::optional<int> elite_placement_count;
  std::optional<std::string> io_node_policy;
  std::optional<std::string> trial_seed_policy;
  std::optional<std::string> traversal_order_policy;
  std::optional<std::string> traversal_neighbor_policy;
  std::optional<std::string> candidate_scope_policy;
  std::optional<std::string> candidate_rank_policy;
  std::optional<bool> use_yott_annotations;
  std::optional<bool> trace_trials;
};

MappingResult RunPlacement2DArrayEngine(entity::DFG& dfg, entity::MRRG& mrrg,
                                        Placement2DArrayKind kind,
                                        const Placement2DArrayOptions& options);

}  // namespace detail

class Placement2DArrayMapperBase : public IMapper {
 public:
  Placement2DArrayMapperBase(Placement2DArrayKind kind,
                             const std::shared_ptr<entity::DFG> dfg_ptr,
                             const std::shared_ptr<entity::MRRG> mrrg_ptr);

  MappingResult Execution() override;
  void SetLogFilePath(const std::string& log_file_path) override;
  void SetTimeOut(double time_out_s) override;
  void SetAcceptFeasibleSolution(bool accept_feasible_solution) override;
  void SetMaxTrials(int max_trials) override;
  void SetSeedCount(int seed_count) override;
  void SetRandomSeed(int random_seed) override;
  void SetMaxIterations(int max_iterations) override;
  void SetElitePlacementCount(int elite_placement_count) override;
  void SetIONodePolicy(const std::string& io_node_policy) override;
  void SetTrialSeedPolicy(const std::string& trial_seed_policy) override;
  void SetTraversalOrderPolicy(
      const std::string& traversal_order_policy) override;
  void SetTraversalNeighborPolicy(
      const std::string& traversal_neighbor_policy) override;
  void SetCandidateScopePolicy(
      const std::string& candidate_scope_policy) override;
  void SetCandidateRankPolicy(
      const std::string& candidate_rank_policy) override;
  void SetUseYOTTAnnotations(bool use_yott_annotations) override;
  void SetTraceTrials(bool trace_trials) override;

 protected:
  Placement2DArrayKind kind_;
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  detail::Placement2DArrayOptions options_;
};

}  // namespace mapper
