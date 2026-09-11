# Traversal/YOTT DOT Compatibility Report

- CPU mapping root: external path supplied at runtime
- cgra_mapper root: current repository
- Passed: 19/19

This check compares node count, edge count, node order, edge order, source/sink input and output sets, and `op=in/out` node sets between the author `cpu_mapping` DOT files and the copied `benchmark/literature/traversal_yott` DOT files.

| Set | Benchmark | Status | Nodes | Edges | Inputs | Outputs | Details |
| --- | --- | --- | ---: | ---: | --- | --- | --- |
| lisa | atax | PASS | 15 | 15 | `1 13 4 7` | `11 14` |  |
| lisa | bicg | PASS | 26 | 27 | `1 13 17 23 4 7` | `11 21 24 25` |  |
| lisa | cholesky | PASS | 9 | 9 | `1 8` | `6` |  |
| lisa | doitgen | PASS | 19 | 19 | `1 10 17 7` | `15 18 2` |  |
| lisa | gemm | PASS | 19 | 19 | `1 11 17 4 6` | `15 18` |  |
| lisa | gemver | PASS | 23 | 24 | `1 12 15 21 4 7` | `19 22` |  |
| lisa | gesummv | PASS | 26 | 27 | `1 13 18 23 4 8` | `11 21 24 25` |  |
| lisa | mvt | PASS | 16 | 16 | `1 14 4 8` | `12 15` |  |
| lisa | symm | PASS | 18 | 18 | `1 10 16 5` | `14 17` |  |
| lisa | syrk | PASS | 15 | 15 | `1 13 4 7` | `11 14` |  |
| lisa | trmm | PASS | 20 | 20 | `1 12 18 5 7` | `16 19` |  |
| m_bench | arf | PASS | 38 | 40 | `28 29 30 31 32 33 34 35` | `36 37` |  |
| m_bench | collapse_pyr | PASS | 86 | 95 | `72 73 74 75 76` | `12 16 23 25 39 40 61 67 71 78 79 80 81 82 83 84 85` |  |
| m_bench | conv3 | PASS | 25 | 26 | `1 11 14 16 19 22 4 6 8` | `23 24` |  |
| m_bench | ewf | PASS | 47 | 60 | `40 41` | `42 43 44 45 46` |  |
| m_bench | h2v2_smooth | PASS | 64 | 67 | `1 18 2 23 52 53 54 55 56 57 58 59 60 61 62 63` | `49` |  |
| m_bench | mac | PASS | 11 | 11 | `1 10 4` | `8` |  |
| m_bench | mults1 | PASS | 25 | 28 | `24` | `19` |  |
| m_bench | simple | PASS | 14 | 15 | `1 11 4 8` | `9` |  |
