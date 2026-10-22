#include <mapper/yott/engine/context/yott_context_engine.hpp>

namespace mapper::yott_context {

double ModuloYOTTEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

}  // namespace mapper::yott_context
