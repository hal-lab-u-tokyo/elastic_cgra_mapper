# Mapper Extension Guide

The current executable selects mappers from `Algorithm.type` in a mapper config JSON. The already supported values are `ILPMapper`, `ILPPlacementMapper`, `ConnectivityBasedILPMapper`, and `PlacementFirstHeuristicMapper`.

For an initial research prototype, the easiest path is to add a new mapper implementation behind the same executable interface, register it by name, then add a config under `research/configs/mapper/` and include it in an experiment manifest.

## C++ Extension Points

1. Implement a class derived from `mapper::IMapper`.
2. Put the declaration in `mapper/include/mapper/<new_mapper>.hpp`.
3. Put the implementation in `mapper/src/<new_mapper>.cpp`.
4. Register the mapper type in that `.cpp` file with `mapper::RegisterMapperType`.
5. Add a mapper config under `research/configs/mapper/`.
6. Add the new mapper config to a research experiment manifest.

Example registration:

```cpp
#include <mapper/mapper_factory.hpp>
#include <mapper/my_mapper.hpp>

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

For quick iteration, add the new mapper to `research/configs/experiments/algorithm_design_compare.json`. Use the same DFG, architecture, II sweep, timeout, and metrics for every mapper. A normal run creates the standard reports automatically:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/algorithm_design_compare.json
```

For placement-oriented work, compare at least `achieved_II`, `mapping_time_sec`, `compute_pe_utilization`, `route_to_compute_ratio`, `avg_manhattan_distance`, and `compute_bbox_utilization` by benchmark. Do not rely only on aggregate averages.

## Recommended Next Cleanup

1. Add a stable result object for success, timeout, infeasible, objective, bound, and gap.
2. Keep the command-line interface of `build/mapping` unchanged so the research runner and existing scripts continue to work.
3. Add a smoke test that runs a tiny DFG through every registered mapper type.

External mappers can be compared without changing the C++ code if they can emit a normalized row with the same `metrics.csv` columns.
