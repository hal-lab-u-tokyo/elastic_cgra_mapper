#include <mapper/sa/engine/spatial/sa_spatial_engine.hpp>

namespace mapper::sa_spatial {

std::optional<PlacementState> Placement2DSAEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(false, false);
}

}  // namespace mapper::sa_spatial
