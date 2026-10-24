# YOTT engine

`mapper/include/mapper/yott/engine/yott_engine.hpp` declares the single
`mapper::yott::YottEngine`, its state and operations. `yott_engine.cpp` owns
setup, options and resource caches; `yott_placement.cpp` owns grid and MRRG
placement/search; `yott_routing.cpp` owns routing and reservation.

`mapper/src/yott/yott_mapper.cpp` expresses the placement-only or routed
workflow directly. Named grid, spatial, context, physical/context and fallback
variants preserve their objectives, seed/candidate ordering and annotations.
YOTT Core and YOTT Core Repair remain separate algorithms.
