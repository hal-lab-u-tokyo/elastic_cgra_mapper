#include <mapper/yott/engine/spatial/yott_spatial_engine.hpp>

namespace mapper::yott_spatial {

double Placement2DYOTTEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

}  // namespace mapper::yott_spatial
