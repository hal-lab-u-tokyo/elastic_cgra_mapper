#include "engine.hpp"

namespace mapper::detail::modulo_sa_routed {

std::optional<PlacementState> ModuloSAEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(false, false);
}

}  // namespace mapper::detail::modulo_sa_routed
