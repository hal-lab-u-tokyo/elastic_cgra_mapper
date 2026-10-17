#include "engine.hpp"

namespace mapper::detail::sa_spatial {

std::optional<PlacementState> Placement2DSAEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(false, false);
}

}  // namespace mapper::detail::sa_spatial
