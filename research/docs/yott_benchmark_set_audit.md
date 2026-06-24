# YOTT Benchmark-Set Audit

The local YOTO/YOTT reproduction benchmark sets and the 23-benchmark YOTT Table 3 set use different DFG sources.

## Short Answer

The current `placement2d/reproduction/traversal_yott.json` run does **not** use the exact 23 benchmarks from YOTT Table 3. It uses 19 DOTs imported from the local `cpu_mapping` checkout:

- 11 `lisa/dac` kernels: `atax`, `bicg`, `cholesky`, `doitgen`, `gemm`, `gemver`, `gesummv`, `mvt`, `symm`, `syrk`, `trmm`
- 8 `m_bench/dac` kernels: `arf`, `collapse_pyr`, `conv3`, `ewf`, `h2v2_smooth`, `mac`, `mults1`, `simple`

YOTT Table 3 uses a different 23-benchmark set. Only `mac` and `simple` match the paper's node/edge counts in the current reproduction run. Several same-name `m_bench/dac` files are different variants from the YOTT Table 3 versions.

The cleanest way to align the benchmarks is to import the exact DOTs from the YOTT paper's benchmark repository:

- `https://github.com/lesc-ufv/benchmarks-cases-2021`

The YOTT paper cites this as reference `[25]`, "Dataflow graph benchmarks". The paper also says the benchmark sources are representative dataflow benchmarks from UCSB and CGRA-ME, but the GitHub repository is the paper-specific bundle and should be treated as the primary reproduction source.

## What The Paper Reports

YOTT Table 3 reports the following per-benchmark information:

- benchmark order, used as the x-axis label for later figures
- benchmark name
- number of nodes `N`
- number of edges `E`
- number of input nodes `IN`
- number of output nodes `OUT`
- YOTT/YOTO percentage of edges in three categories:
  - `IO`
  - `First`
  - `Recon`

The paper states that benchmark numbers in following graphs correspond to Table 3 and are ordered by node count. It also states that the benchmarks come from UCSB and CGRA-ME, and that the benchmark files are available in the LESC-UFV repository.

The numeric Table 3 values are stored locally in:

- `research/data/papers/yott_table3_benchmarks.csv`

## Current Difference

| category | count | details |
| --- | ---: | --- |
| Paper YOTT Table 3 benchmarks | 23 | `mac` through `invert_matrix` |
| Current reproduction benchmarks | 19 | 11 `lisa` + 8 `m_bench/dac` |
| Current reproduction benchmarks with same name and matching paper node/edge counts | 2 | `mac`, `simple` |
| Current reproduction benchmarks with same/similar name but different node/edge counts | 6 | `mults1`, `arf`, `conv3`, `h2v2_smooth`, `ewf`, `collapse_pyr` |
| Current reproduction benchmarks not in YOTT Table 3 | 11 | all current `lisa/dac` benchmarks |
| YOTT Table 3 benchmarks missing from current reproduction run | 15 | listed below |

Missing from the current reproduction run:

- `horner_bs`
- `motion_vec`
- `fir2`
- `fir1`
- `fdback_pts`
- `k4n4op`
- `cosine1`
- `cplx8`
- `fir16`
- `cosine2`
- `filter_rgb`
- `interpolate`
- `w_bmp_head`
- `matmul`
- `invert_matrix`

## The 23 Paper Benchmarks

| # | paper name | source DOT name | N | E | IN | OUT | YOTT edge categories IO/First/Recon | YOTO edge categories IO/First/Recon | current local status |
| ---: | --- | --- | ---: | ---: | ---: | ---: | --- | --- | --- |
| 1 | `mac` | `mac.dot` | 11 | 11 | 3 | 1 | 36 / 55 / 9 | - / 91 / 9 | exact in current `traversal_yott_normalized/m_bench/dac` |
| 2 | `simple` | `simple.dot` | 14 | 15 | 4 | 1 | 40 / 47 / 13 | - / 87 / 13 | exact in current `traversal_yott_normalized/m_bench/dac` |
| 3 | `horner_bs` | `horner_bs.dot` | 17 | 16 | 4 | 1 | 37 / 63 / 0 | - / 100 / 0 | not in current run; similar `horner_bezier_surf.dot` exists in adjacent `cpu_mapping`, but node count differs |
| 4 | `mults1` | `mults1.dot` | 24 | 27 | 1 | 1 | 18 / 78 / 4 | - / 85 / 15 | current run uses same name but 25 nodes / 28 edges |
| 5 | `arf` | `arf.dot` | 28 | 30 | 8 | 2 | 40 / 47 / 13 | - / 87 / 13 | current run uses same name but 38 nodes / 40 edges |
| 6 | `conv3` | `conv3.dot` | 28 | 30 | 9 | 1 | 37 / 53 / 10 | - / 90 / 10 | current run uses same name but 25 nodes / 26 edges |
| 7 | `motion_vec` | `motion_vec.dot` | 32 | 29 | 14 | 3 | 65 / 35 / 0 | - / 100 / 0 | not in current run; matching node/edge DOT exists in `prisa_vpr8_normalized` |
| 8 | `fir2` | `fir2.dot` | 40 | 39 | 16 | 1 | 44 / 56 / 0 | - / 100 / 0 | not in current run; matching node/edge DOT exists in `prisa_vpr8_normalized` |
| 9 | `fir1` | `fir1.dot` | 44 | 43 | 22 | 1 | 53 / 47 / 0 | - / 100 / 0 | not in current run; matching node/edge DOT exists in `prisa_vpr8_normalized` |
| 10 | `fdback_pts` | `fdback_pts.dot` | 54 | 51 | 21 | 5 | 61 / 35 / 4 | - / 96 / 4 | not in current run; similar `feedback_points.dot` exists in adjacent `cpu_mapping` |
| 11 | `k4n4op` | `k4n4op.dot` | 59 | 74 | 22 | 2 | 51 / 43 / 5 | - / 77 / 23 | not in current run; local candidate has 62 nodes / 76 edges |
| 12 | `h2v2_smo` | `h2v2_smo.dot` | 62 | 65 | 16 | 1 | 26 / 68 / 6 | - / 94 / 6 | current run uses `h2v2_smooth` with 64 nodes / 67 edges |
| 13 | `cosine1` | `cosine1.dot` | 66 | 76 | 16 | 8 | 31 / 45 / 24 | - / 76 / 24 | not in current run; exact node/edge DOT exists in adjacent `cpu_mapping/needs_work` |
| 14 | `ewf` | `ewf.dot` | 66 | 79 | 2 | 5 | 18 / 63 / 19 | - / 77 / 23 | current run uses same name but 47 nodes / 60 edges |
| 15 | `cplx8` | `Cplx8.dot` | 77 | 91 | 1 | 1 | 2 / 82 / 16 | - / 84 / 16 | not in current run; exact node/edge DOT exists in adjacent `cpu_mapping/needs_work` |
| 16 | `fir16` | `Fir16.dot` | 77 | 91 | 1 | 1 | 2 / 81 / 17 | - / 81 / 17 | not in current run; matching node/edge DOT exists in `prisa_vpr8_normalized` |
| 17 | `cosine2` | `cosine2.dot` | 81 | 91 | 31 | 8 | 43 / 37 / 20 | - / 80 / 20 | not in current run; matching node/edge DOT exists in `prisa_vpr8_normalized` |
| 18 | `filter_rgb` | `FilterRGB.dot` | 84 | 97 | 2 | 1 | 3 / 83 / 14 | - / 86 / 14 | not in current run; exact node/edge DOT exists in adjacent `cpu_mapping/needs_work` |
| 19 | `collapse_pyr` | `collapse_pyr.dot` | 105 | 122 | 6 | 9 | 22 / 59 / 19 | - / 79 / 21 | current run uses same name but 86 nodes / 95 edges |
| 20 | `interpolate` | `interpolate.dot` | 108 | 104 | 48 | 4 | 54 / 46 / 0 | - / 100 / 0 | not in current run; matching node/edge DOT exists in `prisa_vpr8_normalized` |
| 21 | `w_bmp_head` | `w_bmp_head.dot` | 110 | 92 | 38 | 25 | 61 / 32 / 7 | - / 92 / 8 | not in current run; matching node/edge DOT exists in `prisa_vpr8_normalized` |
| 22 | `matmul` | `matmul.dot` | 116 | 124 | 24 | 4 | 25 / 65 / 10 | - / 90 / 10 | not in current run; exact node/edge DOT exists in adjacent `cpu_mapping/needs_work` |
| 23 | `invert_matrix` | `invert_matrix.dot` | 357 | 378 | 77 | 16 | 29 / 61 / 10 | - / 90 / 10 | not in current run; matching node/edge DOT exists in `prisa_vpr8_normalized` |

## Why Some Same-Name Benchmarks Differ

The current reproduction imported the `dac` subset from the adjacent `cpu_mapping` checkout. That subset is internally consistent with the author-code clone and passes the local compatibility check:

- `benchmark/literature/traversal_yott/dot_compatibility_report.md`

However, it is not the exact YOTT Table 3 set. For example:

- paper `arf`: 28 nodes / 30 edges
- current `arf`: 38 nodes / 40 edges

This means the current `arf` is a different DFG granularity or variant, not merely a renamed version of the paper row.

## How To Align The Benchmark Set

Use the paper-specific LESC-UFV repository as the source of truth:

```bash
mkdir -p benchmark/literature/yott_cases2021_raw
for name in \
  mac simple horner_bs mults1 arf conv3 motion_vec fir2 fir1 fdback_pts \
  k4n4op h2v2_smo cosine1 ewf Cplx8 Fir16 cosine2 FilterRGB \
  collapse_pyr interpolate w_bmp_head matmul invert_matrix
do
  curl -L "https://raw.githubusercontent.com/lesc-ufv/benchmarks-cases-2021/main/${name}.dot" \
    -o "benchmark/literature/yott_cases2021_raw/${name}.dot"
done
```

Then normalize to the mapper input format:

```bash
python3 research/scripts/normalize_benchmarks.py \
  --benchmark-root benchmark/literature/yott_cases2021_raw \
  --out-dir benchmark/literature/yott_cases2021_normalized \
  --report-out benchmark/literature/yott_cases2021_normalized/normalization_report.md
```

After normalization, verify against `research/data/papers/yott_table3_benchmarks.csv`:

- node count
- edge count
- input count
- output count
- benchmark order

After this check passes, use the dedicated reproduction manifest.

## Reproduction Manifest

The aligned manifest is:

- `research/configs/experiments/placement2d/reproduction/yott_cases2021.json`

It uses:

- `benchmark_root`: `benchmark/literature/yott_cases2021_normalized`
- benchmark order exactly as Table 3
- YOTT reproduction architecture settings:
  - one-hop, fully pipelined placement-quality setting
  - perimeter I/O policy selected explicitly
  - `ii = 1`

Use `traversal_yott.json` for comparisons against the imported `cpu_mapping` benchmark subset. Use `yott_cases2021.json` for the YOTT Table 3 benchmark set.
