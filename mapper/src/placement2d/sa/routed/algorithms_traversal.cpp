#include "engine.hpp"

namespace mapper::detail::placement2d_sa_routed {

std::optional<PlacementState> Placement2DSAEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(false, false);
}

}  // namespace mapper::detail::placement2d_sa_routed
