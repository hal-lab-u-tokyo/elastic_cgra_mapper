# Adding a Mapper

The executable selects a mapper from `Algorithm.type` in a mapper config JSON.
The implementation map is in `mapper/README.md`.

First decide whether the mapper solves 2D placement or modulo mapping. Add the
mapper behind `mapper::IMapper`, register it by name, add a config under
`research/configs/mapper/`, and include it in the matching manifest.

- 2D placement mappers live under `mapper/include/mapper/placement2d/` and
  `mapper/src/placement2d/`.
- Modulo mappers live under `mapper/include/mapper/modulo/` and
  `mapper/src/modulo/`.
- Shared MRRG placement/routing helpers live under `mapper/src/common/search/`.

## C++ Extension Points

1. Implement a class derived from `mapper::IMapper`.
2. Put declarations under `mapper/include/mapper/modulo/` or
   `mapper/include/mapper/placement2d/`.
3. Put implementations under `mapper/src/modulo/` or
   `mapper/src/placement2d/`.
4. Register the mapper type in `mapper/src/mapper_registry.cpp`.
5. Add a mapper config under `research/configs/mapper/`.
6. Add the new mapper config to a research experiment manifest.

Example registry entry:

```cpp
#include <mapper/modulo/my_mapper.hpp>

RegisterNames<mapper::MyMapper>({"MyMapper"});
```

The JSON reader stores `Algorithm.type` as a string and does not need to be
edited when adding a mapper. If the type is misspelled,
`mapper::CreateMapper` prints the registered mapper names.

`mapper/src/CMakeLists.txt` collects `.cpp` files under `mapper/src/`, so new
source files are picked up after re-running CMake or `scripts/build.sh`.

## Required Mapper Interface

New C++ mappers should implement:

- `MappingResult Execution()`: run placement/routing and return success,
  mapping, and mapping time.
- `void SetLogFilePath(const std::string&)`: use this when the mapper writes
  solver or debug logs; heuristic mappers can store or ignore it.
- `void SetTimeOut(double)`: respect this for long searches.
- `void SetAcceptFeasibleSolution(bool)`: mainly for solver-backed mappers;
  heuristic mappers can store or ignore it.

The constructor should accept `std::shared_ptr<entity::DFG>` and
`std::shared_ptr<entity::MRRG>`. Runtime options are applied through
`IMapper::Configure()` and the relevant virtual setters.

## Research Evaluation Flow

For modulo mapping, add the new mapper to
`research/configs/experiments/modulo/compare.json`. Use the same DFG,
architecture, II sweep, timeout, and metrics for every mapper:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/modulo/compare.json
```

For 2D placement algorithms, add the mapper to
`research/configs/experiments/placement2d/compare.json` instead:

```bash
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/compare.json
```

Do not merge these result sets. Modulo mapping may place multiple operations on
different contexts of one physical PE; 2D placement forbids that.

For placement-oriented modulo work, compare at least `achieved_II`,
`mapping_time_sec`, `compute_pe_utilization`, `route_to_compute_ratio`,
`avg_manhattan_distance`, and `compute_bbox_utilization` by benchmark. Do not
rely only on aggregate averages.

External mappers can be compared without changing C++ if they emit the same
`metrics.csv` columns.
