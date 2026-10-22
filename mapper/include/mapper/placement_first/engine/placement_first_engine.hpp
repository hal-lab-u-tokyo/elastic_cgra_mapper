#pragma once
#include <chrono>
#include <mapper/mapper.hpp>
#include <set>
namespace mapper::placement_first {
class PlacementFirstEngine {
 public:
  PlacementFirstEngine(const std::shared_ptr<entity::DFG>& dfg,
                       const std::shared_ptr<entity::MRRG>& mrrg);
  bool PlaceOperations();
  bool RouteEdges();
  MappingResult Finish(std::chrono::system_clock::time_point start_time);
  MappingResult Failure(std::chrono::system_clock::time_point start_time) const;

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  entity::MRRGConfig mrrg_config;
  int dfg_node_num;
  int mrrg_node_num;
  std::vector<int> dfg_node_to_mrrg_node;
  std::vector<std::vector<int>> dfg_output_to_mrrg_edge;
  std::set<int> occupied_op_nodes;
};
}  // namespace mapper::placement_first
