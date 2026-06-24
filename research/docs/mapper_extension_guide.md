# Mapper Extension Guide

The executable selects mappers from `Algorithm.type` in a mapper config JSON. Main values are `FullRoutingILPMapper`, `Placement2DILPMapper`, `ConnectivityPathILPMapper`, `ModuloPlacementFirstMapper`, `ModuloYOTOMapper`, `ModuloYOTTMapper`, `ModuloSAMapper`, `Placement2DYOTOMapper`, `Placement2DYOTTMapper`, `Placement2DSAMapper`, `Placement2DPRISAMapper`, and `Placement2DPRISANoSISMapper`. `ModuloYOTOWithFallbackMapper` and `ModuloYOTTWithFallbackMapper` are diagnostic hybrid variants; keep them separate from ordinary comparisons.

First decide whether the mapper solves modulo mapping or 2D placement. Add the mapper behind the same executable interface, register it by name, add a config under `research/configs/mapper/`, and include it in the matching manifest. `FullRoutingILPMapper` and `ConnectivityPathILPMapper` are modulo mappers because they model routed/context-aware mapping. `Placement2DILPMapper` lives with 2D placement mappers because it solves only the placement objective.

## C++ Extension Points

1. Implement a class derived from `mapper::IMapper`.
2. Put modulo-aware mapper declarations in `mapper/include/mapper/modulo/<new_mapper>.hpp`, or 2D placement mapper declarations in `mapper/include/mapper/placement2d/<new_mapper>.hpp`.
3. Put modulo-aware mapper implementations in `mapper/src/modulo/<new_mapper>.cpp`, or 2D placement mapper implementations in `mapper/src/placement2d/<new_mapper>.cpp`.
4. Register the mapper type in that `.cpp` file with `mapper::RegisterMapperType`.
5. Add a mapper config under `research/configs/mapper/`.
6. Add the new mapper config to a research experiment manifest.

Example registration:

```cpp
#include <mapper/mapper_factory.hpp>
#include <mapper/modulo/my_mapper.hpp>

namespace {
const bool kMyMapperRegistered =
    mapper::RegisterMapperType<mapper::MyMapper>("MyMapper");
}
```

The JSON reader stores `Algorithm.type` as a string and does not need to be edited when adding a mapper. If the type is misspelled, `mapper::CreateMapper` prints the registered mapper names.

`mapper/src/CMakeLists.txt` automatically collects `.cpp` files under `mapper/src/`, so new mapper source files are picked up after re-running CMake or `scripts/build.sh`.

## Required Mapper Interface

New C++ mappers should implement:

- `MappingResult Execution()`: run placement/routing and return success, mapping, and mapping time.
- `void SetLogFilePath(const std::string&)`: use this when the mapper writes solver or debug logs; heuristic mappers can store or ignore it.
- `void SetTimeOut(double)`: respect this for long searches.
- `void SetAcceptFeasibleSolution(bool)`: mainly for solver-backed mappers; heuristic mappers can store or ignore it.

The constructor should accept `std::shared_ptr<entity::DFG>` and `std::shared_ptr<entity::MRRG>` so `RegisterMapperType` can instantiate it.

## Research Evaluation Flow

For modulo mapping, add the new mapper to `research/configs/experiments/modulo/search.json`. Use the same DFG, architecture, II sweep, timeout, and metrics for every mapper:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/search.json
```

For 2D placement algorithms, add the mapper to `research/configs/experiments/placement2d/search.json` instead:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/search.json
```

Do not merge these result sets. Modulo mapping may place multiple operations on different contexts of one physical PE; 2D placement forbids that.

For placement-oriented work, compare at least `achieved_II`, `mapping_time_sec`, `compute_pe_utilization`, `route_to_compute_ratio`, `avg_manhattan_distance`, and `compute_bbox_utilization` by benchmark. Do not rely only on aggregate averages.

External mappers can be compared without changing C++ if they emit the same `metrics.csv` columns.
