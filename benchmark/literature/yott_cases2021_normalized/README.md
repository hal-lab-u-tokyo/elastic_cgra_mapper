# YOTT Case-Study Benchmarks

Ready DOTs: 23/23  
Node counts matching the YOTT paper: 23/23  
Edge counts matching the YOTT paper: 23/23  
Graph source counts matching paper IN: 23/23  
Graph sink counts matching paper OUT: 23/23  
Opcode `load` counts matching paper IN: 2/23  
Opcode `output` counts matching paper OUT: 13/23

The paper's IN/OUT columns match graph source/sink counts. They do not always match normalized `load`/`output` opcodes because the public DOT files do not encode I/O ports in a uniform opcode convention.

| benchmark | ready | nodes | edges | paper IN | source nodes | mapper load | paper OUT | sink nodes | mapper output |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `mac` | ready | 11/11 | 11/11 | 3 | 3 | 2 | 1 | 1 | 1 |
| `simple` | ready | 14/14 | 15/15 | 4 | 4 | 2 | 1 | 1 | 1 |
| `horner_bs` | ready | 17/17 | 16/16 | 4 | 4 | 2 | 1 | 1 | 1 |
| `mults1` | ready | 24/24 | 27/27 | 1 | 1 | 4 | 1 | 1 | 1 |
| `arf` | ready | 28/28 | 30/30 | 8 | 8 | 0 | 2 | 2 | 0 |
| `conv3` | ready | 28/28 | 30/30 | 9 | 9 | 3 | 1 | 1 | 1 |
| `motion_vec` | ready | 32/32 | 29/29 | 14 | 14 | 2 | 3 | 3 | 2 |
| `fir2` | ready | 40/40 | 39/39 | 16 | 16 | 0 | 1 | 1 | 0 |
| `fir1` | ready | 44/44 | 43/43 | 22 | 22 | 22 | 1 | 1 | 1 |
| `fdback_pts` | ready | 54/54 | 51/51 | 21 | 21 | 7 | 5 | 5 | 4 |
| `k4n4op` | ready | 59/59 | 74/74 | 22 | 22 | 0 | 2 | 2 | 0 |
| `h2v2_smo` | ready | 62/62 | 65/65 | 16 | 16 | 16 | 1 | 1 | 1 |
| `cosine1` | ready | 66/66 | 76/76 | 16 | 16 | 0 | 8 | 8 | 0 |
| `ewf` | ready | 66/66 | 79/79 | 2 | 2 | 0 | 5 | 5 | 0 |
| `cplx8` | ready | 77/77 | 91/91 | 1 | 1 | 6 | 1 | 1 | 1 |
| `fir16` | ready | 77/77 | 91/91 | 1 | 1 | 0 | 1 | 1 | 0 |
| `cosine2` | ready | 81/81 | 91/91 | 31 | 31 | 0 | 8 | 8 | 0 |
| `filter_rgb` | ready | 84/84 | 97/97 | 2 | 2 | 0 | 1 | 1 | 1 |
| `collapse_pyr` | ready | 105/105 | 122/122 | 6 | 6 | 9 | 9 | 9 | 9 |
| `interpolate` | ready | 108/108 | 104/104 | 48 | 48 | 12 | 4 | 4 | 4 |
| `w_bmp_head` | ready | 110/110 | 92/92 | 38 | 38 | 11 | 25 | 25 | 24 |
| `matmul` | ready | 116/116 | 124/124 | 24 | 24 | 28 | 4 | 4 | 4 |
| `invert_matrix` | ready | 357/357 | 378/378 | 77 | 77 | 64 | 16 | 16 | 16 |
