# Mapper Extension Guide

The current executable selects mappers from `Algorithm.type` in a mapper config JSON. The already supported values are `ILPMapper`, `ILPPlacementMapper`, and `PlacementFirstHeuristicMapper`.

For an initial research prototype, the easiest path is to add a new mapper implementation behind the same executable interface, then add a config under `research/configs/mapper/` and include it in an experiment manifest.

Current C++ extension points:

1. Add a new enum value to `entity::AlgorithmType`.
2. Update `io::ReadMapperConfigFromJsonFile` and `io::WriteMapperConfigToJsonFile` with the new `Algorithm.type` string.
3. Implement a class derived from `mapper::IILPMapper`.
4. Register it in `mapper::CreateMapper` in `mapper/src/mapper_factory.cpp`.
5. Add a mapper config under `research/configs/mapper/`.
6. Add the new mapper config to a research experiment manifest.

Recommended next cleanup:

1. Add a stable result object for success, timeout, infeasible, objective, bound, and gap.
2. Keep the command-line interface of `build/mapping` unchanged so the research runner and existing scripts continue to work.
3. Add a smoke test that runs a tiny DFG through every registered mapper type.

External mappers can be compared without changing the C++ code if they can emit a normalized row with the same `metrics.csv` columns.
