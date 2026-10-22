#include <mapper/ilp/full_routing_ilp_mapper.hpp>
#include <mapper/ilp/ilp_workflow.hpp>
mapper::FullRoutingILPMapper::FullRoutingILPMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

mapper::FullRoutingILPMapper* mapper::FullRoutingILPMapper::CreateMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  mapper::FullRoutingILPMapper* result = new mapper::FullRoutingILPMapper;
  *result = mapper::FullRoutingILPMapper(dfg_ptr, mrrg_ptr);

  return result;
}

mapper::MappingResult mapper::FullRoutingILPMapper::Execution() {
  return ilp::RunILP<ilp::FullRoutingILPEngine>(dfg_ptr_, mrrg_ptr_,
                                                log_file_path_, timeout_s_,
                                                accept_feasible_solution_);
}

void mapper::FullRoutingILPMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
  return;
}

void mapper::FullRoutingILPMapper::SetTimeOut(double timeout_s) {
  timeout_s_ = timeout_s;
  return;
}

void mapper::FullRoutingILPMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
  return;
}
