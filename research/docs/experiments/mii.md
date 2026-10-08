# MII Definition

This repository's research runner computes both ResMII and RecMII, then uses their maximum as MII.

```text
ResMII(op) = ceil(number of DFG nodes with opcode op / number of resources that can execute op per cycle)
ResMII(all) = ceil(total number of DFG nodes / total number of PEs)
ResMII = max(ResMII(all), max ResMII(op))
```

`ResMII(all)` is needed because most operation types share the same PE pool. Without it, a DFG with many different opcode types could incorrectly get `ResMII = 1` even when it has more operations than PEs in one II.

For memory operations, the opcode-specific resource count follows the architecture template's `memory_io` mode: `all` means every PE can access memory, `both_ends` means both edge columns, and `one_end` means one edge column.

For normal arithmetic and logic operations, the current model counts every PE as a usable resource. For `loop` operations, it uses the number of `loop_controllers`.

RecMII is computed over directed cycles in the DFG.

```text
RecMII(cycle) = ceil(sum(operation latency on the cycle) / sum(edge distance on the cycle))
RecMII = max RecMII(cycle)
MII = max(ResMII, RecMII)
```

The runner reads loop-carried distance from edge attributes named `distance`, `dist`, `iteration_distance`, or `loop_distance`. Edges without one of these attributes have distance 0, except when `mii_missing_distance_policy` is `self_loop`; in that mode, missing-distance self-loop edges are treated as distance 1. This default matches the current benchmark files, where accumulator feedback is represented as edges such as `add13->add13[operand=1]` without an explicit distance.

Operation latency defaults to 1 because the current mapper model does not expose multi-cycle operation latency. A node can override it with `latency` or `delay`, and an architecture template can override it with `operation_latency`, for example:

```json
{
  "operation_latency": {
    "default": 1,
    "mul": 2
  }
}
```

If a directed cycle has total distance 0, RecMII is reported as `null` because the recurrence bound cannot be computed safely. Use explicit distance attributes for publication-quality experiments.
