#pragma once
#include <iostream>

#include "engine/full_routing_ilp_engine.hpp"
#include "engine/placement2d_ilp_engine.hpp"
namespace mapper::detail {
// Mapper workflow shared by the spatial and edge-flow ILP formulations.
template <typename Engine>
MappingResult RunILP(const std::shared_ptr<entity::DFG>& dfg,
                     const std::shared_ptr<entity::MRRG>& mrrg,
                     const std::optional<std::string>& log_file_path,
                     const std::optional<double>& timeout_s,
                     bool accept_feasible_solution) {
  const auto start_time = std::chrono::system_clock::now();
  try {
    Engine engine(dfg, mrrg, log_file_path, timeout_s,
                  accept_feasible_solution);
    engine.BuildVariables();
    engine.SetObjective();
    engine.AddConstraints();
    engine.Optimize();
    return engine.CollectResult(start_time);
  } catch (GRBException e) {
    std::cout << "Error code = " << e.getErrorCode() << std::endl;
    std::cout << e.getMessage() << std::endl;
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now() - start_time)
                             .count() /
                         1000.0;
    return MappingResult(false, entity::Mapping(mrrg->GetMRRGConfig()),
                         elapsed);
  }
}
}  // namespace mapper::detail
