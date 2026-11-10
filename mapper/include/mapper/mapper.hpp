#pragma once

#include <entity/mapping.hpp>
#include <mapper/mapping_status.hpp>
#include <memory>
#include <optional>
#include <string>

namespace mapper {

struct MapperOptions {
  std::string log_file_path;
  double timeout_s = 1.0;
  bool accept_feasible_solution = true;
  bool placement_only = false;
};

struct MappingResult {
  MappingResult(MappingStatus status, const entity::Mapping& mapping,
                double mapping_time_s)
      : status(status),
        mapping_ptr(std::make_shared<entity::Mapping>(mapping)),
        mapping_time_s(mapping_time_s){};

  MappingStatus status;
  bool HasMapping() const { return mapper::HasMapping(status); }
  std::shared_ptr<entity::Mapping> mapping_ptr;
  double mapping_time_s;
};

class IMapper {
 public:
  virtual ~IMapper() = default;
  void Configure(const MapperOptions& options);
  virtual MappingResult Execution() = 0;
  virtual void SetLogFilePath(const std::string& log_file_path) = 0;
  virtual void SetTimeOut(double time_out_s) = 0;
  virtual void SetAcceptFeasibleSolution(bool accept_feasible_solution) = 0;
  virtual void SetMaxTrials(int max_trials) {}
  virtual void SetSeedCount(int seed_count) {}
  virtual void SetRoutingRetryCount(int routing_retry_count) {}
  virtual void SetRandomSeed(int random_seed) {}
  virtual void SetMaxIterations(int max_iterations) {}
  virtual void SetElitePlacementCount(int elite_placement_count) {}
  virtual void SetPlacementOnly(bool placement_only) {}
  virtual void SetIONodePolicy(const std::string& io_node_policy) {}
  virtual void SetTrialSeedPolicy(const std::string& trial_seed_policy) {}
  virtual void SetTraversalOrderPolicy(
      const std::string& traversal_order_policy) {}
  virtual void SetTraversalNeighborPolicy(
      const std::string& traversal_neighbor_policy) {}
  virtual void SetCandidateScopePolicy(
      const std::string& candidate_scope_policy) {}
  virtual void SetCandidateRankPolicy(
      const std::string& candidate_rank_policy) {}
  virtual void SetUseYOTTAnnotations(bool use_yott_annotations) {}
  virtual void SetTraceTrials(bool trace_trials) {}
};
}  // namespace mapper
