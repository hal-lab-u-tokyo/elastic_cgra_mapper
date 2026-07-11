# YOTT Placement

YOTO/YOTT placement-only comparison on the YOTT paper's 23 case-study benchmarks. This is a best-effort comparison setup; exact paper reproduction is not established.

## Setup

| item | value |
| --- | --- |
| manifest | `research/configs/experiments/placement2d/literature/yott_2021.json` |
| benchmark set | `benchmark/literature/yott_cases2021_source_order_normalized` |
| architecture | `fit_io_no_corners` |
| grid policy | `cpu_mapping_yoto_yott_fit_structural_io` |
| I/O cells | perimeter, corners excluded |
| network model | `one_hop_axis2` |
| VPR BB setting | `inner_num = 7`, `seed = 1`, output-LUT I/O pads |

The manifest compares YOTO, YOTT, YOTT Core, YOTT Core Repair, and VPR BB on all 23 benchmarks.

## Mappers

| mapper family | role | implementation |
| --- | --- | --- |
| CPU Mapping YOTO/YOTT | public `cpu_mapping`-style traversal and placement behavior | `mapper/src/placement2d/engine/algorithms/yoto_yott/` |
| YOTT Core | reduced traversal and randomized candidate selection | `mapper/src/placement2d/engine/algorithms/yoto_yott/yott_core.cpp` |
| YOTT Core Repair | YOTT Core followed by guarded local improvement | `mapper/src/placement2d/engine/algorithms/yoto_yott/yott_core_repair*.cpp` |
| VPR BB | external simulated-annealing placement baseline with bounding-box cost | `research/scripts/run_vpr_baseline.py` |

## Algorithm Correspondence

The paper and the released `cpu_mapping` code are not identical specifications.
The regular `Placement2DCPUMappingYOTTMapper` follows released-code commit
`8062550` where its behavior is explicit and uses the paper to resolve clear
implementation errors.

| stage | YOTT paper | regular YOTT mapper | correspondence |
| --- | --- | --- | --- |
| Grid and I/O sites | text states a minimum `ceil(sqrt(N))` square grid; I/O PEs are on the border | released-code setting: `ceil(sqrt(non-I/O nodes)) + 2`, structural I/O on the non-corner perimeter | follows released code, not the paper's stated grid formula |
| Trial generation | repeat fast placements from different random choices | fixed seed stream, shuffled output roots, and one traversal mode per trial | close; the random stream is deterministic but container and parser order can still differ |
| First traversal | visit every edge and build an annotated sequence | output-rooted zigzag traversal, remaining-edge completion, and source-order DFG data | close for ordinary connected DFGs |
| Reconvergence | detect a revisited target and backpropagate distance annotations | `ApplyCPUMappingCycleAnnotations()` backpropagates reconvergence distances | implemented from the released code |
| I/O guidance | backpropagate I/O-distance annotations | structural I/O nodes are constrained to perimeter cells; no separate I/O annotations are propagated | different |
| Degree matching | match target-node degree to free neighboring cells | `BestCPUMappingDegreeCell()` ranks by remaining candidate-cell freedom | implemented; the released cell-index comparison is corrected |
| Look-ahead | for a distance-two annotation, preserve a distance-one cell for the next placement | intersects tip cells with annotation-distance regions, then minimizes distance to annotated anchors | partial; it does not explicitly test the next placement required by the paper |
| No-annotation placement | randomized closest neighbors with breadth-first fallback | one-hop tip cells followed by randomized nearest-first adjacency search | close |
| Trial selection | retain the minimum wire-length placement | retain minimum traversal edge cost, then final placement cost | close, but failed-edge penalties can change ties |

An optional paper-guided array implementation is available in
`mapper/src/placement2d/engine/algorithms/yoto_yott/paper_guided.cpp`. It uses
explicit I/O annotations and repository-specific candidate ranking, so it is
not part of the default literature manifest and should not be labeled an exact
reproduction.

## Paper Metrics

| paper figure/table | repository metric |
| --- | --- |
| Table 1 speedup | VPR BB `mapping_time_sec` divided by mapper `mapping_time_sec` |
| Table 1 average wire length | `placement_avg_cost` |
| Table 1 FIFO reduction | VPR BB FIFO sum divided by mapper FIFO sum |
| Fig. 19 optimal mapped edges | `placement_optimal_edge_ratio * 100` |
| Fig. 20(a) execution time | `mapping_time_sec * 1000` |
| Fig. 20(b) number of swaps | `placement_swap_attempts` |
| Fig. 22 maximal FIFO | `placement_max_cost` for the paper-style one-hop segment convention |

`placement_max_paper_fifo` is still recorded as `max(0, placement_cost - 1)`, but the corrected VPR BB FIFO series from the paper is closer to raw `placement_max_cost`.

## VPR BB

The closest VPR-BB-only setting is described in [YOTT VPR BB](yott_vpr_bb.md).

The all-mapper comparison here uses `fit_io_no_corners / inner=7 / seed=1` because every YOTO/YOTT mapper also needs enough structural I/O sites.

## Commands

```bash
docker compose -f environment/docker-compose.yaml exec gurobi bash -lc '
set -e
cd /home/ubuntu/elastic_cgra_mapper

RUN_DIR="research/results/placement2d/literature/yott_2021/$(date +%Y%m%d-%H%M%S)_metric_compare"

python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/literature/yott_2021.json \
  --out "$RUN_DIR"
'
```

## Status

- The 23 benchmark graphs match the paper's node and edge counts, but opcode-level I/O counts do not always match the paper's IN/OUT columns.
- The paper does not publish the exact VPR command line, VTR version, I/O encoding, or FIFO postprocess.
- VPR BB maximal FIFO is close under `placement_max_cost`, but not exact for every benchmark.
- YOTO/YOTT implementation variants should be compared as hypotheses, not treated as confirmed reproductions.
- Placement-only figures do not prove routed CGRA correctness.

References:

- YOTT paper: <https://doi.org/10.1145/3477038>
- released `cpu_mapping` code: <https://github.com/canesche/cpu_mapping/tree/8062550694de3ff5615eba35b363979d1529d39e>

See also:

- [YOTT benchmarks](yott_benchmarks.md)
- [YOTT VPR BB](yott_vpr_bb.md)
- [Metrics](../experiments/metrics.md)
