#pragma once

#include <mapper/mapper.hpp>

#include <memory>
#include <optional>
#include <string>

namespace mapper {

class PlacementFirstHeuristicMapper : public IILPMapper {
 public:
  PlacementFirstHeuristicMapper(){};
  PlacementFirstHeuristicMapper(const std::shared_ptr<entity::DFG> dfg_ptr,
                                const std::shared_ptr<entity::MRRG> mrrg_ptr);
  PlacementFirstHeuristicMapper* CreateMapper(
      const std::shared_ptr<entity::DFG> dfg_ptr,
      const std::shared_ptr<entity::MRRG> mrrg_ptr);
  MappingResult Execution();
  void SetLogFilePath(const std::string& log_file_path);
  void SetTimeOut(double time_out_s);
  void SetAcceptFeasibleSolution(bool accept_feasible_solution);

 private:
  std::shared_ptr<entity::DFG> dfg_ptr_;
  std::shared_ptr<entity::MRRG> mrrg_ptr_;
  std::optional<std::string> log_file_path_;
  std::optional<double> timeout_s_;
  bool accept_feasible_solution_ = true;
};

}  // namespace mapper
