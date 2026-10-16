#include "engine.hpp"

namespace mapper::detail::sa_context {

std::optional<PlacementState> ModuloSAEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(false, false);
}

}  // namespace mapper::detail::sa_context
