#include <mapper/yoto/engine/spatial/yoto_spatial_engine.hpp>

namespace mapper::yoto_spatial {

double Placement2DYOTOEngine::EdgePlacementCost(
    const DFGEdgeInfo& edge, const std::vector<int>& dfg_to_mrrg) const {
  return ResourcePairPlacementCost(dfg_to_mrrg[edge.source],
                                   dfg_to_mrrg[edge.target]);
}

}  // namespace mapper::yoto_spatial
