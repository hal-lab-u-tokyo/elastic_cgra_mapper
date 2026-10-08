# Shared Search Engine

MRRG-resource placement and routing used by 2D and modulo frontends.

```text
engine.cpp
engine_internal.hpp

support/
  options.cpp
  resources.cpp
  quality.cpp
  routing.cpp
  context_assignment.cpp

algorithms/
  traversal_plan.cpp
  traversal_placement.cpp
  traversal.cpp
  sa.cpp
  prisa_initial.cpp
  prisa_moves.cpp
  prisa_refinement.cpp
  prisa.cpp
```

- Traversal order and annotations: `algorithms/traversal_plan.cpp`
- Candidate ranking and placement: `algorithms/traversal_placement.cpp`
- SA: `algorithms/sa.cpp`
- PRISA: `algorithms/prisa_*.cpp`
- Routing and route validation: `support/routing.cpp`
- Modulo context assignment: `support/context_assignment.cpp`
- MRRG compatibility: `support/resources.cpp`
- Objectives: `support/quality.cpp`
