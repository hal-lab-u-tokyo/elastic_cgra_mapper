# PRISA VPR-8 Benchmarks

This directory contains normalized DOT benchmarks for PRISA-style 2D placement experiments.

The target list follows the VPR-8 benchmark table in the PRISA paper. Ten benchmarks are available at a compatible or near-compatible operation-DFG granularity. `interpolate` is from the UCSB EXPRESS benchmark page (`https://web.ece.ucsb.edu/EXPRESS/benchmark/mesa/interpolate_aux.html`). The other operation-DFG DOTs are included as normalized benchmark artifacts in this repository. The remaining VPR-8 entries were regenerated from VTR Verilog sources by running ODIN/ABC/VPR and converting packed `.net` files; these generated entries are useful stress tests, but not all of them match the paper's packed-block count.

| benchmark | PRISA table nodes | normalized nodes | status |
| --- | ---: | ---: | --- |
| `motion_vec` | 32 | 32 | matched |
| `fir1` | 44 | 44 | matched |
| `fir2` | 40 | 40 | matched |
| `k4n4op` | 59 | 62 | available, node-count mismatch |
| `h2v2_smo` | 62 | 64 | available, node-count mismatch |
| `fir16` | 77 | 77 | matched |
| `cosine2` | 81 | 81 | matched |
| `interpolate` | 108 | 108 | matched |
| `w_bmp_head` | 110 | 110 | matched |
| `diffeq2` | 121 | 245 | generated from VTR, granularity mismatch |
| `boundtop` | 144 | 399 | generated from VTR, granularity mismatch |
| `mksmadapter` | 256 | 517 | generated from VTR, granularity mismatch |
| `invert_matrix` | 357 | 357 | matched |
| `blob_merge` | 841 | 839 | generated from VTR, near paper count |

The VTR repository has Verilog/BLIF material for the generated entries, but those files are not always at the same benchmark granularity as the PRISA table. For example, the regenerated `diffeq2` packed net contains 245 top-level VPR blocks, while the PRISA table reports 121. See `normalization_report.md` for the stage-by-stage diagnosis.
