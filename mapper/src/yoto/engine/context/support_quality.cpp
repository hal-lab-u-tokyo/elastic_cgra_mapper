#include <mapper/yoto/engine/context/yoto_context_engine.hpp>

namespace mapper::yoto_context {

double ModuloYOTOEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

}  // namespace mapper::yoto_context
