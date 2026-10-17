#include <mapper/ilp/ilp_mapper.hpp>

#include "workflow.hpp"
namespace mapper {
MappingResult ILPMapper::Execution() {
  // Temporal MRRGs require explicit per-DFG-edge flow conservation.
  if (mrrg_->GetMRRGConfig().context_size > 1) {
    return detail::RunILP<detail::FullRoutingILPEngine>(dfg_, mrrg_, log_,
                                                        timeout_, accept_);
  }
  return detail::RunILP<detail::SpatialILPEngine>(dfg_, mrrg_, log_, timeout_,
                                                  accept_);
}
}  // namespace mapper
