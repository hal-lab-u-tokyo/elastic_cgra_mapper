#include <mapper/ilp/ilp_workflow.hpp>
#include <mapper/ilp/placement2d_ilp_mapper.hpp>
mapper::Placement2DILPMapper::Placement2DILPMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr)
    : dfg_ptr_(dfg_ptr), mrrg_ptr_(mrrg_ptr) {}

mapper::Placement2DILPMapper* mapper::Placement2DILPMapper::CreateMapper(
    const std::shared_ptr<entity::DFG> dfg_ptr,
    const std::shared_ptr<entity::MRRG> mrrg_ptr) {
  mapper::Placement2DILPMapper* result = new mapper::Placement2DILPMapper;
  *result = mapper::Placement2DILPMapper(dfg_ptr, mrrg_ptr);

  return result;
}

mapper::MappingResult mapper::Placement2DILPMapper::Execution() {
  return ilp::RunILP<ilp::SpatialILPEngine>(dfg_ptr_, mrrg_ptr_, log_file_path_,
                                            timeout_s_,
                                            accept_feasible_solution_);
}

void mapper::Placement2DILPMapper::SetLogFilePath(
    const std::string& log_file_path) {
  log_file_path_ = log_file_path;
  return;
}

void mapper::Placement2DILPMapper::SetTimeOut(double timeout_s) {
  timeout_s_ = timeout_s;
  return;
}

void mapper::Placement2DILPMapper::SetAcceptFeasibleSolution(
    bool accept_feasible_solution) {
  accept_feasible_solution_ = accept_feasible_solution;
  return;
}
