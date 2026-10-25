#pragma once
#include <gurobi_c++.h>

#include <chrono>
#include <mapper/mapper.hpp>
#include <memory>
#include <vector>
namespace mapper::ilp {
class SpatialILPEngine {
 public:
  SpatialILPEngine(const std::shared_ptr<entity::DFG>& dfg,
                   const std::shared_ptr<entity::MRRG>& mrrg,
                   const std::optional<std::string>& log_file_path,
                   const std::optional<double>& timeout_s,
                   bool accept_feasible_solution);
  void BuildVariables();
  void SetObjective();
  void AddConstraints();
  void Optimize();
  MappingResult CollectResult(std::chrono::system_clock::time_point start_time);

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  bool accept_feasible_solution_;
  GRBEnv env_{true};
  std::unique_ptr<GRBModel> model_;
  int dfg_node_num;
  int mrrg_node_num;
  std::vector<std::vector<GRBVar>> map_op_to_PE;
  std::vector<std::vector<GRBVar>> map_output_to_route;
};
}  // namespace mapper::ilp
