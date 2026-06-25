# TRAVERSAL, YOTT, and PRISA

Problem settings, algorithms, benchmarks, metrics, and paper locations for TRAVERSAL/YOTO, YOTT, and PRISA.

Local paper files:

- `../../../Paper/TRAVERSAL.pdf`: "TRAVERSAL: A Fast and Adaptive Graph-Based Placement and Routing for CGRAs", IEEE TCAD 2021.
- `../../../Paper/yott.pdf`: "You Only Traverse Twice: A YOTT Placement, Routing, and Timing Approach for CGRAs", ACM TECS 2021.
- `../../../Paper/PRISA.pdf`: "PRISA: A Potential Region-based Intelligent Search Algorithm for Dataflow Graph Mapping in Spatial CGRAs", ACM TRETS 2025.

## High-Level Classification

| method | main problem in the paper | modulo scheduling? | routing treatment | paper locations |
| --- | --- | --- | --- | --- |
| TRAVERSAL / YOTO | Greedy graph-traversal placement and routing for CGRA P&R. It has both modulo-scheduled ADRES experiments and fully pipelined spatial-CGRA experiments. | Yes for the CGRA-ME/ADRES experiment: 4x4 ADRES, II = 2. The fully pipelined experiment is effectively II = 1 with one node per PE. | Placement and local routing are coupled in the traversal kernel. Remaining non-traversal edges are routed afterward. Timing is handled by elastic FIFO sizing. This is not a global routing-aware optimizer, but it is more than placement-only. | TRAVERSAL Sec. III-B to III-G, Sec. V-A to V-F, Table I, Table II, Table III, Fig. 14 to Fig. 16. |
| YOTT | 2D placement for fully pipelined spatial CGRA, designed to make later routing and timing easy. | No modulo scheduling in the main evaluation. Each DFG node gets a dedicated cell in a one-hop fully pipelined CGRA. | Placement first. The paper says most edges become adjacent, so only the remaining non-adjacent edges need a simple maze routing step. Timing/FIFO is evaluated after placement/routing. | YOTT Sec. 2.2, Sec. 2.3, Sec. 3, Sec. 4.1 to 4.7, Table 1 to Table 4, Fig. 18 to Fig. 23. |
| PRISA | 2D spatial mapping / placement for mesh CGRAs, formulated through adjacency and resource-distance matrices. | No modulo scheduling. CGRA-ME comparisons use II = 1; VPR-8 does not perform modulo scheduling. | Placement first. The paper explicitly uses simple Manhattan shortest-path routing after placement and states there is no routing feedback mechanism in PRISA. | PRISA Sec. 4, Algorithm 1, Algorithm 2, Sec. 5.1 to 5.8, Table 2, Table 3, Fig. 7 to Fig. 10. |

## TRAVERSAL / YOTO

### Algorithm

TRAVERSAL is the source paper for what later work calls YOTO, "you only traverse once". It builds an ordered input traversal list and traverses the DFG and target architecture together. When an edge is visited, one endpoint is already placed, and the other endpoint is greedily placed in a nearby free PE. The target-side adjacent list can be randomized, so multiple executions explore different placements. Output nodes are placed before processing the traversal list.

Routing is part of the paper's P&R story. Traversal edges are made local when possible, and remaining DFG edges that were not part of the traversal list are routed afterward. Timing is then handled by elastic FIFO sizing and path balancing. This means TRAVERSAL is not simply "2D placement plus no routing"; it is a greedy P&R method with a local routing/timing model. However, when it compares against VPR, the VPR baseline is placement-only, and the placement quality is interpreted through wire length and FIFO requirements.

Paper locations:

- Problem definitions, optimal P&R, placement/routing/timing motivation: Sec. III-A and Fig. 4.
- Main traversal placement and routing flow: Sec. III-B, especially "Input Traversal List P&R" and "Routing of the Remaining Edges".
- Zigzag traversal: Sec. III-C.
- Local optimization, topology modeling, modulo scheduling, elastic FIFO simplification: Sec. III-D to III-G.

### Problem Settings

TRAVERSAL uses two target-CGRA settings.

| setting | PE grid / II | interconnect and I/O | purpose | paper locations |
| --- | --- | --- | --- | --- |
| Modulo-scheduled ADRES | 4x4 ADRES, II = 2 | 1-hop interconnection, one memory operation per row, border-located I/O nodes | Compare with CGRA-ME ILP/SA and prior ILP work on small CGRA-ME benchmarks. | Sec. V-A, Table I, Sec. V-E. |
| Fully pipelined CGRA | Minimum square grid, `ceil(sqrt(N)) x ceil(sqrt(N))`, one DFG node per PE | Mesh or 1-hop interconnect, homogeneous PEs, elastic input FIFO size | Compare traversal strategies, branch-and-bound, GPU traversal, VPR SA, and depth-first heuristic. | Sec. V-A, Table II, Table III, Sec. V-F. |

The paper's fully pipelined setting differs from this repository's `cpu_mapping_yoto_yott` grid policy. The paper states `ceil(sqrt(N)) x ceil(sqrt(N))`. The public `cpu_mapping` style used by this repository for TRAVERSAL/YOTT paper comparison uses `ceil(sqrt(nodes - inputs - outputs)) + 2` with perimeter I/O slots. That is closer to the public code convention and the YOTT border-I/O model, but it is not identical to the simple minimum-square sentence in TRAVERSAL Sec. V-A.

### Benchmarks

TRAVERSAL Table I uses 15 CGRA-ME distribution benchmarks for the modulo-scheduled ADRES setting.

| benchmark | graph size |
| --- | ---: |
| accum | 18 |
| cap | 24 |
| conv2 | 16 |
| conv3 | 24 |
| mac | 11 |
| mac2 | 24 |
| matrixm | 17 |
| mults2 | 25 |
| nomem1 | 6 |
| simple | 12 |
| simple2 | 12 |
| sum | 7 |
| mults1 | 31 |
| 2loops1 | 11 |
| 2loops2 | 16 |

TRAVERSAL Table II uses 19 fully pipelined multimedia/DSP benchmarks, mostly from UCSB EXPRESS. The paper reports these as a speed-up evaluation against VPR.

| n | benchmark | graph size |
| ---: | --- | ---: |
| 1 | mac | 11 |
| 2 | simple | 14 |
| 3 | hornerbs | 17 |
| 4 | conv3 | 28 |
| 5 | arf | 28 |
| 6 | mults1 | 23 |
| 7 | motionvect | 32 |
| 8 | ewf | 66 |
| 9 | fir2 | 40 |
| 10 | fir1 | 44 |
| 11 | Cplx8 | 77 |
| 12 | Fir16 | 77 |
| 13 | h2v2_smo | 62 |
| 14 | feedback | 54 |
| 15 | FilterRGB | 84 |
| 16 | k4n4op | 62 |
| 17 | cosine1 | 66 |
| 18 | cosine2 | 82 |
| 19 | interpol | 108 |

This repository's normalized TRAVERSAL/YOTT set is in `benchmark/literature/traversal_yott_normalized/`. It contains 19 runnable DOTs from LISA-style and m_bench-style sources:

| set | benchmarks and local node counts |
| --- | --- |
| `lisa/dac` | `atax` 15, `bicg` 26, `cholesky` 9, `doitgen` 19, `gemm` 19, `gemver` 23, `gesummv` 26, `mvt` 16, `symm` 18, `syrk` 15, `trmm` 20 |
| `m_bench/dac` | `arf` 38, `collapse_pyr` 86, `conv3` 25, `ewf` 47, `h2v2_smooth` 64, `mac` 11, `mults1` 25, `simple` 14 |

The local normalized set is useful for experiments, but it is not exactly the same list or node granularity as TRAVERSAL Table II.

### Metrics

TRAVERSAL reports:

- Execution time: Chrono, GCC `-O3`, VPR placement-only time for VPR comparisons. Sec. V-C.
- FIFO size / elastic FIFO requirement: cost metric and quality proxy. Sec. V-D, Fig. 15, Fig. 16, Table III.
- Speed-up against VPR: Table II and Fig. 14.
- Optimal P&R / optimal adjacency: a mapping is optimal when adjacent DFG nodes are mapped to adjacent PEs and do not need elastic FIFOs. Sec. V-D.

Key reported trend:

- On modulo CGRA-ME Table I, `(All, 100)` traversal solves all 15 cases with geometric mean 54.3 us, while ILP/SA can time out under 1800 s.
- On fully pipelined Table II, single Zigzag, `(Z, 1000)`, and GPU traversal are compared with VPR and depth-first traversal. `(Z, 1000)` is slower than one traversal but improves FIFO quality; the paper reports 4.6x geometric speed-up over VPR for `(Z, 1000)`.

## YOTT

### Algorithm

YOTT improves YOTO by splitting the traversal into two passes.

1. The first traversal does not place nodes. It annotates graph structure:
   - I/O annotations: nodes and predecessors that should be placed toward grid borders.
   - Reconvergent annotations: edges caused by fanout/reconvergence that YOTO tends to miss.
   - Look-ahead information: guide placement direction before a future conflict appears.
2. The second traversal places nodes greedily using those annotations:
   - If no annotation exists, pick a close free cell and match local degree.
   - If annotations exist, prioritize I/O distance, then look-ahead, then reconvergence.
   - If no adjacent free cell exists, do a greedy breadth-first search around the already placed node.

YOTT is therefore primarily a placement algorithm. The paper evaluates placement quality because a near-optimal placement makes later routing and FIFO balancing simpler. For non-adjacent edges, the paper says a simple maze routing approach is used; Dijkstra or A* could also be used. It is not a routing-aware global search over an MRRG.

Paper locations:

- YOTO background and weaknesses: Sec. 2.3 to Sec. 2.5, Fig. 5 to Fig. 8.
- YOTT first traversal and annotations: Sec. 3.1, Fig. 9 to Fig. 15.
- YOTT second traversal and placement pseudocode: Sec. 3.2, Fig. 16.
- Complexity and routing/timing comments: end of Sec. 3.2.
- Routing after placement: Sec. 4.4 and Sec. 4.6.

### Problem Settings

| item | YOTT paper setting | paper locations |
| --- | --- | --- |
| CGRA model | One-hop fully pipelined CGRA. Each DFG node gets a dedicated cell. Data must arrive at a specific cycle. | Sec. 4, paragraph before Sec. 4.1. |
| PE grid size | Minimum square grid, `ceil(sqrt(N)) x ceil(sqrt(N))`. Example: N = 30 uses 6x6. | Sec. 4. |
| I/O constraints | I/O nodes should be placed at grid borders; YOTT adds I/O annotations and back-propagates I/O distance to predecessors. | Sec. 2.5, Sec. 3.1.2, Fig. 12, summary at end of Sec. 3.2. |
| Modulo scheduling | Not the main experimental setting. The paper discusses timing/FIFO for fully pipelined CGRA, not modulo-II sweep. | Sec. 2.2, Sec. 4, Sec. 5 related work. |
| Routing | Placement first; remaining non-adjacent edges are solved by maze routing. | Sec. 4.4, around Fig. 19; Sec. 4.6 around Fig. 21 to Fig. 22. |

### Benchmarks

YOTT Table 3 reports 23 dataflow benchmarks from UCSB and CGRA-ME.

| n | benchmark | N | E | IN | OUT |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | mac | 11 | 11 | 3 | 1 |
| 2 | simple | 14 | 15 | 4 | 1 |
| 3 | horner_bs | 17 | 16 | 4 | 1 |
| 4 | mults1 | 24 | 27 | 1 | 1 |
| 5 | arf | 28 | 30 | 8 | 2 |
| 6 | conv3 | 28 | 30 | 9 | 1 |
| 7 | motion_vec | 32 | 29 | 14 | 3 |
| 8 | fir2 | 40 | 39 | 16 | 1 |
| 9 | fir1 | 44 | 43 | 22 | 1 |
| 10 | fdback_pts | 54 | 51 | 21 | 5 |
| 11 | k4n4op | 59 | 74 | 22 | 2 |
| 12 | h2v2_smo | 62 | 65 | 16 | 1 |
| 13 | cosine1 | 66 | 76 | 16 | 8 |
| 14 | ewf | 66 | 79 | 2 | 5 |
| 15 | Cplx8 | 77 | 91 | 1 | 1 |
| 16 | Fir16 | 77 | 91 | 1 | 1 |
| 17 | cosine2 | 81 | 91 | 31 | 8 |
| 18 | FilterRGB | 84 | 97 | 2 | 1 |
| 19 | collapse_pyr | 105 | 122 | 6 | 9 |
| 20 | interpolate | 108 | 104 | 48 | 4 |
| 21 | w_bmp_head | 110 | 92 | 38 | 25 |
| 22 | matmul | 116 | 124 | 24 | 4 |
| 23 | invert_matrix | 357 | 378 | 77 | 16 |

Table 3 also classifies edges into I/O, first, and reconvergent categories. The average category split reported there is about 34% I/O edges, 56%/89% first edges for YOTT/YOTO, and 10%/11% reconvergent edges for YOTT/YOTO. The point of YOTT is that I/O and reconvergent annotations directly target edge categories that YOTO treats mostly as random side effects.

### Metrics

YOTT reports:

- Speed-up ratio against VPR BB, average wire length in segments, and FIFO-size ratio in Table 1.
- Average wire length by edge category in Table 2.
- Percentage of optimal mapped edges in Fig. 18 and Fig. 19. An edge is optimal if its endpoints are placed in neighboring cells, so no routing effort is needed.
- Placement execution time and number of swaps / node-cell evaluations in Fig. 20.
- Maximal FIFO size in Fig. 21 and Fig. 22.
- Multiple-kernel execution time and average FIFO size in Table 4.

Key reported trend:

- YOTT-100 is intended as the main quality/runtime point. It reaches similar wire length to SA-100, reduces FIFO size versus VPR BB and YOTO-1000, and is faster than both SA-100 and YOTO-1000.
- The paper says YOTT-100 outperforms YOTO-100 on all 23 benchmarks and YOTO-1000 on 21 of 23 benchmarks for optimal-edge percentage.
- YOTT-100 placement makes 90.5% of edges optimal, so the paper argues that routing is already "solved" for most edges before running the router.

## PRISA

### Algorithm

PRISA formulates spatial mapping through matrices:

- `AM`: DFG adjacency matrix.
- `DM`: resource-graph distance matrix, measured in hop count.
- `M`: mapping/permutation matrix.
- Mapping cost is based on the overlap between DFG adjacency and resource distance after permutation.

The core idea is to split the resource distance matrix into potential region (PR) and weak region (WR). Large-distance entries are WR; near-diagonal, low-distance entries are PR. PRISA then runs a modified simulated-annealing-style search that does not pick arbitrary swaps first. Instead, each iteration identifies non-zero mapped adjacency entries in WR, chooses a row or column with the most weak-region entries, and swaps it toward a row or column in PR with fewer weak-region entries. If there are no weak-region entries, the algorithm falls back to random selection.

SIS, Selective Initial Solution, is a separate initial-labeling method. It labels the DFG to reduce adjacency-matrix bandwidth before PRISA starts:

1. Find a high-degree node.
2. Pick the farthest node from it as the starting node.
3. Build levels through graph expansion.
4. Assign labels level by level so connected nodes receive nearby labels.

Paper locations:

- Matrix formulation and resource distance matrix: Sec. 2 and Sec. 3.
- Potential and weak region derivation: Sec. 4, Table 1, Fig. 5, Eq. 10 to Eq. 19.
- PRISA search algorithm: Sec. 4.1 and Algorithm 1.
- SIS low-bandwidth DFG labeling: Sec. 4.2, Algorithm 2, Fig. 6.
- Configuration: Sec. 5.3.

### Problem Settings

| item | PRISA paper setting | paper locations |
| --- | --- | --- |
| CGRA-ME architecture | ADRES-like mesh, 16-bit datapath, circuit-switched configurable modules. II = 1 for CGRA-ME comparisons. Small DFGs use 6x6, bigger DFGs use 10x10. No elastic FIFO buffers for fair comparison in that toolkit setting. | Sec. 5.1, Sec. 5.2, Table 2. |
| VPR-8 architecture | Homogeneous VPR-8 / VTR K6-N10-40nm architecture. The paper reports packed-block DFG size after VPR packing. | Sec. 5.1, Sec. 5.2, Table 3. |
| PE grid size | VPR-8 uses minimum square grid, `ceil(sqrt(DFG size)) x ceil(sqrt(DFG size))`. | Sec. 5.1. |
| I/O constraints | PRISA does not center the algorithm around explicit perimeter-I/O constraints. VPR-8 comparison is a homogeneous placement-quality experiment after packing. | Sec. 5.1 to 5.2. |
| Modulo scheduling | Not used. The paper states VPR-8 does not perform modulo scheduling and comparison with VPR-8 placement algorithms is feasible. CGRA-ME II is selected as 1. | Sec. 5.2. |
| Routing | Simple Manhattan shortest-path routing is applied after placement. If routing resources are exceeded, a nearest PE with free routing resources is selected; if routing fails, mapping is reinitialized. The current PRISA algorithm has no routing feedback mechanism. | Sec. 5.1, Sec. 5.7. |

### Benchmarks

PRISA Table 2 uses CGRA-ME benchmarks.

| benchmark | DFG size | CGRA size |
| --- | ---: | --- |
| Nomem1 | 6 | 6x6 |
| Sum | 7 | 6x6 |
| Mac | 11 | 6x6 |
| Simple | 12 | 6x6 |
| Simple2 | 12 | 6x6 |
| Conv2 | 16 | 6x6 |
| Matrixm | 17 | 6x6 |
| Accum | 18 | 6x6 |
| Cos-4 | 21 | 6x6 |
| Cosh-4 | 21 | 6x6 |
| Conv3 | 24 | 6x6 |
| Cap | 24 | 6x6 |
| Mac2 | 24 | 6x6 |
| Mults2 | 25 | 6x6 |
| Exp-6 | 26 | 6x6 |
| Mults1 | 31 | 6x6 |
| FFT | 38 | 10x10 |

PRISA Table 3 uses VPR-8 benchmarks. The paper reports DFG size as packed-block size.

| benchmark | DFG size | DFG LP |
| --- | ---: | ---: |
| Motion-vec | 32 | 6 |
| Fir1 | 44 | 11 |
| Fir2 | 40 | 11 |
| K4n4op | 59 | 7 |
| H2v2-smo | 62 | 19 |
| Fir16 | 77 | 23 |
| Cosine2 | 81 | 9 |
| Interpolate | 108 | 8 |
| W-bmp-head | 110 | 8 |
| Diffeq2 | 121 | 8 |
| Boundtop | 144 | 10 |
| Mksmadapter | 256 | 12 |
| Invert-matrix | 357 | 12 |
| Blob-merge | 841 | 23 |
| AVG | 166 | 11.92 |

This repository's closest VPR-8 normalized set is `benchmark/literature/prisa_vpr8_normalized/vpr8`. The exact paper granularity is only partly available:

- Matched: `motion_vec`, `fir1`, `fir2`, `fir16`, `cosine2`, `interpolate`, `w_bmp_head`, `invert_matrix`.
- Near: `k4n4op` is 62 here vs 59 in the paper; `h2v2_smo` is 64 here vs 62; `blob_merge` is 839 here vs 841.
- Generated with granularity mismatch: `diffeq2` is 245 here vs 121, `boundtop` is 399 here vs 144, `mksmadapter` is 517 here vs 256.

The mismatch is caused by benchmark extraction, not by the mapper. The local VTR/Verilog flow expands bit-level I/O, registers, constants, and packed LUT structure differently from the artifacts used in the PRISA paper.

### Metrics

PRISA reports:

- CT, compilation time: Table 2 for CGRA-ME and Table 3 for VPR-8.
- Maximal FIFO buffer size: Fig. 7. This is a placement-induced delay-mismatch metric, not an actual routed FIFO insertion pass in this repository.
- LP, longest path after mapping in hop count: Fig. 8.
- Percentage of optimal distances: Fig. 9. A communication distance of exactly 1 hop count is optimal.
- Convergence curves: Fig. 10.

Key reported trend:

- On CGRA-ME Table 2, PRISA without SIS solves all DFGs and averages 8.64 ms with 88.29% success rate under the no-elastic-FIFO criterion.
- On VPR-8 Table 3, average CT is VPR BB 328.2 ms, VPR fast 106.2 ms, YOTO 62.4 ms, YOTT 85.3 ms, Depth 72.8 ms, PRISA without SIS 109.4 ms, and PRISA with SIS 53.5 ms.
- For max FIFO on VPR-8, PRISA with SIS averages 3.64; the paper reports VPR BB 7.64, VPR fast 9.43, YOTO 6.14, YOTT 4.85, Depth 9.29, and PRISA without SIS 5.29.
- For LP, PRISA with SIS is reported as better than VPR BB, VPR fast, YOTO, YOTT, Depth, and PRISA without SIS by about 23%, 30%, 12%, 6%, 25%, and 6%, respectively.
- For optimal distances, PRISA with SIS is reported as best on average: about 91% optimal distances, improving over VPR BB, VPR fast, YOTO, YOTT, Depth, and PRISA without SIS by about 14%, 29%, 9%, 4%, 26%, and 4%, respectively.

## How This Repository Maps These Papers

| paper-style experiment | manifest | main mapper configs | notes |
| --- | --- | --- | --- |
| TRAVERSAL/YOTT placement-quality comparison | `research/configs/experiments/placement2d/paper_comparison/traversal_yott.json` | YOTO/YOTT 1, 10, 100, 1000; array YOTO/YOTT; VPR SA; placement-only ILP | Uses `placement_only`, II = 1, perimeter I/O, mesh and one-hop cost models. This is for placement quality and runtime trends, not routed correctness. |
| PRISA VPR-8 paper comparison | `research/configs/experiments/placement2d/paper_comparison/prisa_vpr8.json` | VPR SA/fast, `cpu_mapping_yoto_1000`, `cpu_mapping_yott_1000`, PRISA no-SIS, PRISA SIS | Uses `placement_only`, II = 1, minimum square grid, all-PE I/O. The benchmark set has known granularity caveats. |
| 2D placement search | `research/configs/experiments/placement2d/search.json` | CPU-mapping YOTO/YOTT 1000, PRISA, VPR SA/fast | Practical design-search subset. It mixes TRAVERSAL/YOTT-style and PRISA-style baselines under one placement-only setting. |
| Modulo mapping search | `research/configs/experiments/modulo/search.json` | modulo YOTO/YOTT/SA adaptations, PRISA physical-placement adaptation, placement-first heuristic, VPR-routed baseline | These are not literal paper paper comparisons of YOTT or PRISA. They are modulo adaptations: place operations, assign contexts when needed, then route through this repository's CGRA BFS/maze-style router or VPR-based routing baseline. |

Important interpretation rule:

- `placement2d` results answer "how good is the physical 2D placement under paper-style distance/FIFO/LP metrics?"
- `modulo` results answer "can the mapper find a valid II, context assignment, and route-correct mapping?"
- A 2D placement paper result should not be compared directly with a modulo routed result unless the evaluation goal is explicitly separated.

## Routing-Awareness Summary

| method | placement only? | placement then routing? | routing-aware placement? |
| --- | --- | --- | --- |
| TRAVERSAL/YOTO | No, the original paper is P&R-oriented. | Yes. Traversal places local edges; remaining edges are routed afterward; timing is handled with FIFOs. | Partly local. It uses adjacency/routing cost during greedy decisions, but it is not a global MRRG routing-aware optimizer. |
| YOTT | Mostly yes in the algorithmic contribution. | Yes. It uses maze routing for remaining non-adjacent edges after placement. | No global routing feedback. It is placement designed to minimize routing need. |
| PRISA | Yes as the core algorithm. | Yes. It uses simple Manhattan shortest-path routing after placement. | No. The paper explicitly says current PRISA has no routing feedback mechanism. |

For future work in this repository, this suggests three separate research tracks:

1. `placement2d`: compare PRISA/YOTO/YOTT/VPR under placement-only metrics.
2. `modulo placement then routing`: adapt placement algorithms to II/context placement, then use a fixed router.
3. `modulo routing-aware placement`: optimize placement while considering MRRG routing feasibility and pressure directly.

For modulo placement-then-routing experiments, `ModuloPhysicalYOTO/YOTT` are the cleaner literature-facing adaptations: run the 2D placement idea first, then assign contexts, then use CGRA BFS/maze routing. `ModuloPhysicalPRISA` uses the same adaptation and the same router, making it the fairer default when comparing placement algorithms under modulo routing. `ModuloPhysicalPRISAManhattan` follows the PRISA paper's placement-first and Manhattan-routing assumption, so it is useful as a paper-assumption probe, but it is less fair against YOTO/YOTT+BFS on routing-stress modulo cases. The `direct_context` YOTO/YOTT variants are useful engineering probes, but they are farther from the original 2D-placement papers because context slots are part of the placement search from the start.
