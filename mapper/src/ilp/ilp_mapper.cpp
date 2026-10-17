#include <mapper/ilp/ilp_mapper.hpp>
#include <mapper/ilp/ilp_workflow.hpp>
namespace mapper {
MappingResult ILPMapper::Execution() {
  // Temporal MRRGs require explicit per-DFG-edge flow conservation.
  if (mrrg_->GetMRRGConfig().context_size > 1) {
    return ilp::RunILP<ilp::FullRoutingILPEngine>(dfg_, mrrg_, log_, timeout_,
                                                  accept_);
  }
  return ilp::RunILP<ilp::SpatialILPEngine>(dfg_, mrrg_, log_, timeout_,
                                            accept_);
}
}  // namespace mapper
