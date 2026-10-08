#!/bin/sh

cp yott_presentation.json ../research/configs/experiments/placement2d/yott_presentation.json
cd /home/ubuntu/elastic_cgra_mapper

# result saved at research/results/placement2d/yott_presentation/metrics.csv
python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/yott_presentation.json \
  --out research/results/placement2d/yott_presentation

cp research/results/placement2d/yott_presentation/metrics.csv /home/ubuntu/elastic_cgra_mapper/eval_pr/yott_presentation_metrics.csv

python3 - <<'PY'
import json
from pathlib import Path

SEED_BASE = 1706961029
SIZES = [64, 128, 256, 512, 1024, 2048]
WIDTH = 8

BENCHMARKS = [
    "mac", "simple", "horner_bs", "mults1", "arf", "conv3",
    "motion_vec", "fir2", "fir1", "fdback_pts", "k4n4op",
    "h2v2_smo", "cosine1", "ewf", "Cplx8", "Fir16",
    "cosine2", "FilterRGB", "collapse_pyr", "interpolate",
    "w_bmp_head", "matmul", "invert_matrix"
]

ARCH = {
    "name": "fit_io_no_corners",
    "template": "research/configs/arch_templates/mesh_10x10_default.json",
    "auto_grid": {
        "policy": "cpu_mapping_yoto_yott_fit_structural_io",
        "margin": 0
    },
    "memory_io": "perimeter_no_corners",
    "network_type": "one_hop_axis2",
    "ii": 1
}

def seed_mappers(trials):
    result = []
    for method, config in [
        ("yott", "research/configs/mapper/placement2d/yott.json"),
        ("core", "research/configs/mapper/placement2d/yott_core.json")
    ]:
        for index in range(10):
            result.append({
                "name": f"{method}_seed_{index + 1:02d}",
                "mapper_config": config,
                "algorithm_overrides": {
                    "max_trials": trials,
                    "random_seed": SEED_BASE + index
                }
            })
    return result

config_dir = Path("research/configs/experiments/placement2d")
config_dir.mkdir(parents=True, exist_ok=True)

multiseed = {
    "name": "placement2d_yott_multiseed",
    "problem_type": "placement2d",
    "evaluation_mode": "placement_only",
    "mode": "placement2d_fixed_ii",
    "result_group": "placement2d/yott_multiseed",
    "mapping_bin": "build/mapping",
    "benchmark_sets": [{
        "name": "yott_2021",
        "benchmark_root":
            "benchmark/literature/yott_cases2021_source_order_normalized",
        "benchmarks": BENCHMARKS
    }],
    "architectures": [ARCH],
    "mappers": seed_mappers(1000),
    "timeout_sec": 240,
    "parallel_num": 1,
    "mii_missing_distance_policy": "self_loop"
}

benchmark_root = Path("research/reproduction/benchmarks/placement_scaling")
benchmark_root.mkdir(parents=True, exist_ok=True)

def node_name(layer, lane):
    return f"n_{layer}_{lane}"

def make_dfg(node_count):
    layer_count = node_count // WIDTH - 2
    lines = ["digraph G {"]

    for lane in range(WIDTH):
        lines.append(f'  input_{lane} [opcode="load"];')

    for layer in range(layer_count):
        opcode = "add" if layer % 2 == 0 else "mul"
        for lane in range(WIDTH):
            lines.append(
                f'  {node_name(layer, lane)} [opcode="{opcode}"];'
            )

    for lane in range(WIDTH):
        lines.append(f'  output_{lane} [opcode="output"];')

    for lane in range(WIDTH):
        lines.append(
            f"  input_{lane} -> {node_name(0, lane)} [operand=0];"
        )
        lines.append(
            f"  input_{(lane + 1) % WIDTH} -> "
            f"{node_name(0, lane)} [operand=1];"
        )

    for layer in range(1, layer_count):
        for lane in range(WIDTH):
            lines.append(
                f"  {node_name(layer - 1, lane)} -> "
                f"{node_name(layer, lane)} [operand=0];"
            )
            lines.append(
                f"  {node_name(layer - 1, (lane - 1) % WIDTH)} -> "
                f"{node_name(layer, lane)} [operand=1];"
            )

    for lane in range(WIDTH):
        lines.append(
            f"  {node_name(layer_count - 1, lane)} -> "
            f"output_{lane} [operand=0];"
        )

    lines.append("}")
    return "\n".join(lines) + "\n"

for size in SIZES:
    path = benchmark_root / f"reconvergent_{size:04d}.dot"
    path.write_text(make_dfg(size), encoding="ascii")

scaling = {
    "name": "placement2d_yott_scaling",
    "problem_type": "placement2d",
    "evaluation_mode": "placement_only",
    "mode": "placement2d_fixed_ii",
    "result_group": "placement2d/yott_scaling",
    "allow_placement2d_capacity_mismatch": True,
    "mapping_bin": "build/mapping",
    "benchmark_sets": [{
        "name": "reconvergent_scaling",
        "benchmark_root":
            "research/reproduction/benchmarks/placement_scaling",
        "benchmarks": [
            f"reconvergent_{size:04d}" for size in SIZES
        ]
    }],
    "architectures": [ARCH],
    "mappers": seed_mappers(100),
    "timeout_sec": 300,
    "parallel_num": 1,
    "mii_missing_distance_policy": "self_loop"
}

(config_dir / "yott_multiseed.json").write_text(
    json.dumps(multiseed, indent=2) + "\n"
)
(config_dir / "yott_scaling.json").write_text(
    json.dumps(scaling, indent=2) + "\n"
)
PY

python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/yott_multiseed.json \
  --out research/results/placement2d/yott_multiseed

python3 research/scripts/run_suite.py \
  --manifest research/configs/experiments/placement2d/yott_scaling.json \
  --out research/results/placement2d/yott_scaling
