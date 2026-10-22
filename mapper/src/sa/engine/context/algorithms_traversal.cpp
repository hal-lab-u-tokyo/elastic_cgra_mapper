#include <mapper/sa/engine/context/sa_context_engine.hpp>

namespace mapper::sa_context {

std::optional<PlacementState> ModuloSAEngine::ConstructGreedyPlacement() {
  return ConstructTraversalPlacement(false, false);
}

}  // namespace mapper::sa_context
