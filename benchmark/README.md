# Benchmarks

Mapper-ready benchmarks are DOT files with normalized opcodes. Imported suites
may keep raw files next to normalized versions.

## Sets

| path | use |
| --- | --- |
| `kernel/` | native kernels for quick checks and modulo experiments |
| `cgrame_kernel/` | CGRA-ME-style kernels |
| `parallel/` | parallel matrix-multiply variants for routing pressure |
| `literature/yott_cases2021_source_order_normalized/` | YOTT 2021 placement comparison, source-order DFGs |
| `literature/yott_cases2021_normalized/` | YOTT 2021 placement comparison, normalized DFGs |
| `literature/prisa_vpr8_normalized/` | experimental PRISA-style placement inputs |
| `CGRA-Bench/` | upstream CGRA-Bench submodule |
| `GenMap/` | imported GenMap cases |
| `revamp_kernel/` | imported Revamp cases |

## Normalize

Use normalized DOTs in manifests. Convert raw or external files with:

```bash
python3 research/scripts/normalize_benchmarks.py
```

Normalization reports under `benchmark/literature/*/normalization_report.md`
record node counts, edge counts, opcode changes, and skipped cases.
