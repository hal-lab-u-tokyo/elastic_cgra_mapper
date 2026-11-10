#include <mapper/placement_first/engine/placement_first_engine.hpp>
#include <mapper/placement_first/routed_placement_first_mapper.hpp>
mapper::ModuloPlacementFirstMapper::ModuloPlacementFirstMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

mapper::ModuloPlacementFirstMapper*
mapper::ModuloPlacementFirstMapper::CreateMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  auto* result = new mapper::ModuloPlacementFirstMapper;
  *result = mapper::ModuloPlacementFirstMapper(dfg_ptr, mrrg_ptr);
  return result;
}

mapper::MappingResult mapper::ModuloPlacementFirstMapper::Execution() {
  const auto start = std::chrono::system_clock::now();
  placement_first::PlacementFirstEngine engine(dfg_ptr_, mrrg_ptr_);
  if (!engine.PlaceOperations()) return engine.Failure(start);
  if (!engine.RouteEdges()) return engine.Failure(start);
  return engine.Finish(start);
}

void mapper::ModuloPlacementFirstMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
}

void mapper::ModuloPlacementFirstMapper::SetTimeOut(double timeout_s) {
  timeout_s_ = timeout_s;
}

void mapper::ModuloPlacementFirstMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
}
