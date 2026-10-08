# Benchmark Normalization

Sources: 14
Ready DOTs: 14

The PRISA paper states that VPR-8 benchmarks are evaluated after VPR packing and that the reported DFG size is the packed-block size. Ten DOTs are included at an operation-DFG granularity close to the paper table. Four missing VPR-8 entries were regenerated from VTR Verilog sources by running ODIN/ABC/VPR and converting the resulting packed `.net` files. Those generated DOTs are useful for stress testing, but three of them do not match the paper's packed-block count because the available Verilog-to-VPR flow expands bit-level I/O, registers, constants, and packable LUT structure differently from the benchmark artifacts used by the paper.

| benchmark | source | output | PRISA paper nodes | normalized nodes | edges | status |
| --- | --- | --- | ---: | ---: | ---: | --- |
| `motion_vec` | `vpr8/motion_vec.dot` | `vpr8/motion_vec.dot` | 32 | 32 | 29 | matched |
| `fir1` | `vpr8/fir1.dot` | `vpr8/fir1.dot` | 44 | 44 | 43 | matched |
| `fir2` | `vpr8/fir2.dot` | `vpr8/fir2.dot` | 40 | 40 | 39 | matched |
| `k4n4op` | `vpr8/k4n4op.dot` | `vpr8/k4n4op.dot` | 59 | 62 | 76 | near, node-count mismatch |
| `h2v2_smo` | `vpr8/h2v2_smo.dot` | `vpr8/h2v2_smo.dot` | 62 | 64 | 67 | near, node-count mismatch |
| `fir16` | `vpr8/fir16.dot` | `vpr8/fir16.dot` | 77 | 77 | 91 | matched |
| `cosine2` | `vpr8/cosine2.dot` | `vpr8/cosine2.dot` | 81 | 81 | 91 | matched |
| `interpolate` | `vpr8/interpolate.dot` | `vpr8/interpolate.dot` | 108 | 108 | 104 | matched |
| `w_bmp_head` | `vpr8/w_bmp_head.dot` | `vpr8/w_bmp_head.dot` | 110 | 110 | 92 | matched |
| `diffeq2` | VTR `diffeq2.v` packed `.net` | `vpr8/diffeq2.dot` | 121 | 245 | 188 | generated, granularity mismatch |
| `boundtop` | VTR `boundtop.v` packed `.net` | `vpr8/boundtop.dot` | 144 | 399 | 205 | generated, granularity mismatch |
| `mksmadapter` | VTR `mkSMAdapter4B.v` packed `.net` | `vpr8/mksmadapter.dot` | 256 | 517 | 363 | generated, granularity mismatch |
| `invert_matrix` | `vpr8/invert_matrix.dot` | `vpr8/invert_matrix.dot` | 357 | 357 | 378 | matched |
| `blob_merge` | VTR `blob_merge.v` packed `.net` | `vpr8/blob_merge.dot` | 841 | 839 | 1145 | generated, near paper count |

## Granularity Diagnosis

The regenerated VTR entries do not all land at the paper's DFG size. The main reason is not the PRISA mapper; it is the benchmark extraction path. For example, `diffeq2.v` generates a VPR packed net with 245 top-level blocks: 83 `clb` blocks and 162 bit-level `io` blocks. If vector I/O ports are grouped by signal name and FFs are counted separately, the estimate becomes `83 clb + 7 grouped I/O + 32 FF = 122`, which is close to the paper's 121. `boundtop` and `mksmadapter` show similar bit-level I/O effects, but they also differ in register/memory/constant treatment, so no simple grouping rule exactly recovers the paper counts. `blob_merge` happens to match the paper size closely at the top-level packed-block count.

Closer PRISA VPR-8 comparison would require the same packed benchmark artifacts or extraction script used by the paper authors. The current `prisa_vpr8` experiment is therefore a mixed set: the first ten entries are paper-granularity or near-paper-granularity DOTs, while the generated VTR entries are stress tests that help check whether the reported large-DFG trends appear.
