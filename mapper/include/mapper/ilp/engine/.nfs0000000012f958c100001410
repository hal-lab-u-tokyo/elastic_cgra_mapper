#pragma once
#include <gurobi_c++.h>

#include <chrono>
#include <mapper/mapper.hpp>
#include <memory>
#include <vector>
namespace mapper::ilp {
struct DFGEdge {
  int source;
  int target;
};
class FullRoutingILPEngine {
 public:
  FullRoutingILPEngine(const std::shared_ptr<entity::DFG>& dfg,
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
  int mrrg_edge_num;
  std::vector<DFGEdge> dfg_edge_vec;
  int dfg_edge_num;
  std::vector<std::vector<GRBVar>> map_op_to_PE;
  std::vector<std::vector<GRBVar>> map_source_to_route_edge;
  std::vector<std::vector<GRBVar>> map_source_to_route_PE;
  std::vector<std::vector<GRBVar>> map_dfg_edge_to_mrrg_edge;
  std::vector<std::vector<GRBVar>> map_dfg_edge_to_route_PE;
};
}  // namespace mapper::ilp
