#!/usr/bin/env python3
"""Plot VPR-BB candidate settings for the YOTT case-study benchmarks."""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import tempfile
from dataclasses import dataclass
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "cgra_mapper_mpl"))
os.environ.setdefault("XDG_CACHE_HOME", str(Path(tempfile.gettempdir()) / "cgra_mapper_xdg_cache"))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from run_vpr_baseline import parse_packed_net_positions, parse_place_file


PAPER_AVG_WIRE = 1.25
PAPER_VPR_BB_OPTIMAL_EDGE = 0.849
PAPER_VPR_BB_SWAPS_PER_NODE = 2935.0

DEFAULT_GRID_RUN = (
    "research/results/placement2d/probes/yott_vpr_grid_policy/"
    "20260628-090930_vpr_grid_policy_probe"
)
DEFAULT_EFFORT_RUN = (
    "research/results/placement2d/probes/yott_vpr_effort_policy/"
    "20260628-091743_inner_num_probe"
)
DEFAULT_MULTI_RUN = (
    "research/results/placement2d/probes/yott_vpr_multi_metric_policy/"
    "20260628-143157_inner_grid_sweep"
)
DEFAULT_PAPER_FIGURE_VALUES = "research/data/papers/yott_figure_paper_metrics.csv"

BENCHMARKS = [
    "mac",
    "simple",
    "horner_bs",
    "mults1",
    "arf",
    "conv3",
    "motion_vec",
    "fir2",
    "fir1",
    "fdback_pts",
    "k4n4op",
    "h2v2_smo",
    "cosine1",
    "ewf",
    "Cplx8",
    "Fir16",
    "cosine2",
    "FilterRGB",
    "collapse_pyr",
    "interpolate",
    "w_bmp_head",
    "matmul",
    "invert_matrix",
]


@dataclass(frozen=True)
class Candidate:
    label: str
    source: str
    key: str
    color: str
    marker: str


CANDIDATES = [
    Candidate("fit-I/O no corners", "grid", "fit_io_no_corners", "#4C78A8", "o"),
    Candidate("strict no corners", "grid", "strict_no_corners", "#F58518", "s"),
    Candidate("strict corners", "grid", "strict_corners", "#54A24B", "^"),
    Candidate("strict no corners, inner=10", "effort", "10", "#B279A2", "D"),
    Candidate("strict no corners, inner=50", "effort", "50", "#E45756", "v"),
]

SWEEP_CANDIDATES = [
    Candidate("strict no corners, inner=5", "multi", "strict_no_corners/5", "#4C78A8", "o"),
    Candidate("strict no corners, inner=6", "multi", "strict_no_corners/6", "#F58518", "s"),
    Candidate("strict no corners, inner=7", "multi", "strict_no_corners/7", "#54A24B", "^"),
    Candidate("strict no corners, inner=8", "multi", "strict_no_corners/8", "#B279A2", "D"),
    Candidate("strict no corners, inner=10", "multi", "strict_no_corners/10", "#E45756", "v"),
    Candidate("fit-I/O no corners, inner=5", "multi", "fit_io_no_corners/5", "#72B7B2", "P"),
    Candidate("fit-I/O no corners, inner=6", "multi", "fit_io_no_corners/6", "#FF9DA6", "X"),
    Candidate("fit-I/O no corners, inner=7", "multi", "fit_io_no_corners/7", "#9D755D", "*"),
    Candidate("fit-I/O no corners, inner=8", "multi", "fit_io_no_corners/8", "#BAB0AC", "h"),
    Candidate("fit-I/O no corners, inner=10", "multi", "fit_io_no_corners/10", "#A0CBE8", ">"),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--grid-run", default=DEFAULT_GRID_RUN)
    parser.add_argument("--effort-run", default=DEFAULT_EFFORT_RUN)
    parser.add_argument("--multi-run", default=DEFAULT_MULTI_RUN)
    parser.add_argument(
        "--paper-figure-values",
        dest="paper_figure_values",
        default=DEFAULT_PAPER_FIGURE_VALUES,
    )
    parser.add_argument(
        "--candidate-set",
        choices=["default", "sweep"],
        default="default",
        help="default uses the original probe candidates; sweep uses arch x inner_num candidates.",
    )
    parser.add_argument(
        "--output-dir",
        default=None,
        help="default: research/results/placement2d/probes/yott_vpr_bb_candidate_reproduction/<timestamp>",
    )
    return parser.parse_args()


def one_hop_cost(dx: int, dy: int) -> int:
    if dx == 0 and dy == 0:
        return 1
    return max(1, math.ceil(dx / 2) + math.ceil(dy / 2))


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def canonical_benchmark(name: str) -> str:
    aliases = {
        "filterrgb": "filter_rgb",
        "filter_rgb": "filter_rgb",
        "h2v2_smooth": "h2v2_smo",
        "h2v2_smo": "h2v2_smo",
        "cplx8": "cplx8",
        "fir16": "fir16",
    }
    return aliases.get(name.strip().lower(), name.strip().lower())


def paper_vpr_bb_max_fifo(path: Path) -> list[float | None]:
    if not path.exists():
        return [None for _ in BENCHMARKS]
    rows = read_csv(path)
    by_benchmark = {}
    for row in rows:
        if (
            row.get("figure") == "Fig21"
            and row.get("panel") == "b"
            and row.get("method") == "VPR_BB"
            and row.get("metric") == "maximal_fifo_size"
        ):
            try:
                by_benchmark[canonical_benchmark(row["benchmark"])] = float(row["value"])
            except (KeyError, ValueError):
                continue
    return [by_benchmark.get(canonical_benchmark(benchmark)) for benchmark in BENCHMARKS]


def metrics_by_candidate(
    grid_run: Path,
    effort_run: Path,
    multi_run: Path,
    candidates: list[Candidate],
) -> dict[tuple[str, str], dict[str, str]]:
    rows: dict[tuple[str, str], dict[str, str]] = {}
    for row in read_csv(grid_run / "metrics.csv"):
        if row.get("arch_name") in {candidate.key for candidate in candidates if candidate.source == "grid"}:
            rows[(row["arch_name"], row["benchmark"])] = row
    for row in read_csv(effort_run / "metrics.csv"):
        inner = str(row.get("inner_num", ""))
        if inner in {candidate.key for candidate in candidates if candidate.source == "effort"}:
            rows[(inner, row["benchmark"])] = row
    for row in read_csv(multi_run / "metrics.csv"):
        key = f"{row.get('arch_policy', '')}/{row.get('inner_num', '')}"
        if key in {candidate.key for candidate in candidates if candidate.source == "multi"}:
            rows[(key, row["benchmark"])] = row
    return rows


def candidate_vpr_dir(
    grid_run: Path,
    effort_run: Path,
    multi_run: Path,
    candidate: Candidate,
    benchmark: str,
) -> Path:
    if candidate.source == "grid":
        return grid_run / f"arch={candidate.key}" / f"benchmark={benchmark}" / "mapper=vpr_bb" / "vpr"
    if candidate.source == "effort":
        return effort_run / f"inner={candidate.key}" / f"benchmark={benchmark}" / "mapper=vpr_bb" / "vpr"
    arch_policy, inner_num = candidate.key.split("/")
    return (
        multi_run
        / f"arch={arch_policy}"
        / f"inner={inner_num}"
        / f"benchmark={benchmark}"
        / "mapper=vpr_bb"
        / "vpr"
    )


def blif_nets(blif_path: Path) -> list[list[str]]:
    drivers: set[str] = set()
    loads: dict[str, list[str]] = {}
    inputs: list[str] = []
    outputs: list[str] = []
    for raw_line in blif_path.read_text().splitlines():
        parts = raw_line.strip().split()
        if not parts:
            continue
        if parts[0] == ".inputs":
            inputs.extend(parts[1:])
        elif parts[0] == ".outputs":
            outputs.extend(parts[1:])
        elif parts[0] == ".names" and len(parts) >= 2:
            fanins = parts[1:-1]
            output = parts[-1]
            drivers.add(output)
            for fanin in fanins:
                loads.setdefault(fanin, []).append(output)
    for node in inputs:
        drivers.add(node)
    for node in outputs:
        loads.setdefault(node, []).append(node)
    return [
        [driver] + loads.get(driver, [])
        for driver in sorted(drivers)
        if len([driver] + loads.get(driver, [])) >= 2
    ]


def blif_net_metrics(vpr_dir: Path, benchmark: str) -> dict[str, float]:
    name_map = json.loads((vpr_dir / "vpr_name_map.json").read_text())
    reverse_map = name_map["blif_to_original"]
    top_positions, _, _ = parse_place_file(vpr_dir / f"{benchmark}.place", reverse_map)
    positions = parse_packed_net_positions(vpr_dir / f"{benchmark}.net", top_positions, reverse_map)
    blif_positions = {
        blif_name: positions[original_name]
        for blif_name, original_name in reverse_map.items()
        if original_name in positions
    }
    costs: list[int] = []
    for pins in blif_nets(vpr_dir / f"{benchmark}.blif"):
        points = [blif_positions[pin] for pin in pins if pin in blif_positions]
        if len(points) < 2:
            continue
        rows = [row for row, _ in points]
        cols = [col for _, col in points]
        costs.append(one_hop_cost(max(cols) - min(cols), max(rows) - min(rows)))
    return {
        "blif_net_count": float(len(costs)),
        "blif_avg_wire": sum(costs) / len(costs),
        "blif_optimal_ratio": sum(1 for cost in costs if cost <= 1) / len(costs),
        "blif_avg_fifo": sum(max(0, cost - 1) for cost in costs) / len(costs),
        "blif_max_cost": float(max(costs)),
        "blif_max_fifo": float(max(max(0, cost - 1) for cost in costs)),
    }


def as_float(row: dict[str, str], key: str) -> float:
    return float(row[key])


def collect_rows(
    grid_run: Path,
    effort_run: Path,
    multi_run: Path,
    candidates: list[Candidate],
) -> list[dict[str, object]]:
    raw_rows = metrics_by_candidate(grid_run, effort_run, multi_run, candidates)
    rows: list[dict[str, object]] = []
    for candidate in candidates:
        for index, benchmark in enumerate(BENCHMARKS, start=1):
            row = raw_rows[(candidate.key, benchmark)]
            vpr_dir = candidate_vpr_dir(grid_run, effort_run, multi_run, candidate, benchmark)
            blif = blif_net_metrics(vpr_dir, benchmark)
            rows.append(
                {
                    "benchmark_index": index,
                    "benchmark": benchmark,
                    "candidate": candidate.label,
                    "candidate_key": candidate.key,
                    "dfg_edge_count": as_float(row, "placement_edge_count"),
                    "dfg_cost_sum": as_float(row, "placement_cost_sum"),
                    "dfg_avg_wire": as_float(row, "placement_avg_cost"),
                    "dfg_optimal_ratio": as_float(row, "placement_optimal_edge_ratio"),
                    "mapping_time_ms": as_float(row, "mapping_time_sec") * 1000.0,
                    "swap_attempts": as_float(row, "placement_swap_attempts"),
                    "dfg_nodes": as_float(row, "dfg_nodes"),
                    "swaps_per_node": as_float(row, "placement_swap_attempts") / as_float(row, "dfg_nodes"),
                    # Paper VPR-BB Fig. 21/22 is closer to the raw one-hop
                    # segment cost than to the local cost-minus-one proxy.
                    "max_fifo": as_float(row, "placement_max_cost"),
                    "max_fifo_cost_minus_one": as_float(row, "placement_max_paper_fifo"),
                    **blif,
                }
            )
    return rows


def setup_style() -> None:
    plt.rcParams.update(
        {
            "font.family": "Helvetica",
            "font.size": 8,
            "axes.linewidth": 0.7,
            "axes.labelsize": 8,
            "xtick.labelsize": 6.5,
            "ytick.labelsize": 7,
            "legend.fontsize": 7,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )


def values(rows: list[dict[str, object]], candidate: Candidate, metric: str) -> list[float]:
    by_benchmark = {
        str(row["benchmark"]): float(row[metric])
        for row in rows
        if row["candidate"] == candidate.label
    }
    return [by_benchmark[benchmark] for benchmark in BENCHMARKS]


def plot_lines(
    rows: list[dict[str, object]],
    candidates: list[Candidate],
    output_path: Path,
    metric: str,
    ylabel: str,
    title: str,
    paper_line: float | None = None,
    paper_values: list[float | None] | None = None,
    percent: bool = False,
    log_y: bool = False,
) -> None:
    fig, ax = plt.subplots(figsize=(7.2, 2.8))
    x = list(range(1, len(BENCHMARKS) + 1))
    for candidate in candidates:
        ys = values(rows, candidate, metric)
        if percent:
            ys = [value * 100.0 for value in ys]
        ax.plot(
            x,
            ys,
            label=candidate.label,
            color=candidate.color,
            marker=candidate.marker,
            markersize=3.2,
            linewidth=1.0,
        )
    if paper_line is not None:
        y = paper_line * 100.0 if percent else paper_line
        ax.axhline(y, color="#222222", linewidth=0.9, linestyle="--", label="paper aggregate")
    if paper_values is not None and any(value is not None for value in paper_values):
        ax.plot(
            x,
            paper_values,
            label="Paper VPR-BB",
            color="#111111",
            marker=".",
            markersize=4.0,
            linewidth=1.0,
            linestyle=":",
        )
    if log_y:
        ax.set_yscale("log")
    ax.set_title(title, pad=6)
    ax.set_xlabel("Benchmarks")
    ax.set_ylabel(ylabel)
    ax.set_xticks(x)
    ax.set_xlim(0.6, len(BENCHMARKS) + 0.4)
    ax.grid(axis="y", color="#D7D7D7", linewidth=0.5)
    ax.legend(ncol=3, frameon=False, loc="upper center", bbox_to_anchor=(0.5, -0.24))
    fig.tight_layout()
    fig.savefig(output_path)
    fig.savefig(output_path.with_suffix(".png"), dpi=300)
    plt.close(fig)


def plot_definition_pairs(
    rows: list[dict[str, object]],
    candidates: list[Candidate],
    output_path: Path,
) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(8.4, 3.0), sharex=True)
    panels = [
        ("dfg_avg_wire", "DFG edge evaluation"),
        ("blif_avg_wire", "BLIF/VPR net evaluation"),
    ]
    x = list(range(1, len(BENCHMARKS) + 1))
    for ax, (metric, title) in zip(axes, panels):
        for candidate in candidates:
            ax.plot(
                x,
                values(rows, candidate, metric),
                label=candidate.label,
                color=candidate.color,
                marker=candidate.marker,
                markersize=3.0,
                linewidth=1.0,
            )
        ax.axhline(PAPER_AVG_WIRE, color="#222222", linewidth=0.9, linestyle="--")
        ax.set_title(title, pad=6)
        ax.set_xlabel("Benchmarks")
        ax.set_xticks(x)
        ax.set_xlim(0.6, len(BENCHMARKS) + 0.4)
        ax.grid(axis="y", color="#D7D7D7", linewidth=0.5)
    axes[0].set_ylabel("Average wire length in segments")
    axes[1].legend(ncol=1, frameon=False, loc="center left", bbox_to_anchor=(1.02, 0.5))
    fig.tight_layout()
    fig.savefig(output_path)
    fig.savefig(output_path.with_suffix(".png"), dpi=300)
    plt.close(fig)


def plot_fifo_definition_pairs(
    rows: list[dict[str, object]],
    candidates: list[Candidate],
    output_path: Path,
) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(8.4, 3.0), sharex=True)
    panels = [
        ("max_fifo", "DFG edge evaluation"),
        ("blif_max_fifo", "BLIF/VPR net evaluation"),
    ]
    x = list(range(1, len(BENCHMARKS) + 1))
    for ax, (metric, title) in zip(axes, panels):
        for candidate in candidates:
            ax.plot(
                x,
                values(rows, candidate, metric),
                label=candidate.label,
                color=candidate.color,
                marker=candidate.marker,
                markersize=3.0,
                linewidth=1.0,
            )
        ax.set_title(title, pad=6)
        ax.set_xlabel("Benchmarks")
        ax.set_xticks(x)
        ax.set_xlim(0.6, len(BENCHMARKS) + 0.4)
        ax.grid(axis="y", color="#D7D7D7", linewidth=0.5)
    axes[0].set_ylabel("Maximal FIFO Size")
    axes[1].legend(ncol=1, frameon=False, loc="center left", bbox_to_anchor=(1.02, 0.5))
    fig.tight_layout()
    fig.savefig(output_path)
    fig.savefig(output_path.with_suffix(".png"), dpi=300)
    plt.close(fig)


def write_summary(
    rows: list[dict[str, object]],
    candidates: list[Candidate],
    output_dir: Path,
) -> None:
    fields = [
        "candidate",
        "dfg_avg_wire",
        "blif_avg_wire",
        "dfg_edge_weighted_wire",
        "blif_net_weighted_wire",
        "dfg_optimal_ratio",
        "blif_optimal_ratio",
        "dfg_edge_weighted_optimal_ratio",
        "blif_net_weighted_optimal_ratio",
        "blif_max_fifo",
        "mapping_time_ms",
        "swaps_per_node",
        "weighted_swaps_per_node",
        "max_fifo",
        "max_fifo_cost_minus_one",
        "blif_max_cost",
        "wire_delta_from_paper_dfg",
        "wire_delta_from_paper_blif",
        "optimal_delta_from_paper_blif",
        "swaps_per_node_delta_from_paper",
    ]
    by_candidate = {candidate.label: [] for candidate in candidates}
    for row in rows:
        by_candidate[str(row["candidate"])].append(row)
    summary_rows = []
    for candidate in candidates:
        bucket = by_candidate[candidate.label]
        mean = lambda key: sum(float(row[key]) for row in bucket) / len(bucket)
        dfg_edge_weighted_wire = (
            sum(float(row["dfg_cost_sum"]) for row in bucket)
            / sum(float(row["dfg_edge_count"]) for row in bucket)
        )
        blif_net_weighted_wire = (
            sum(float(row["blif_avg_wire"]) * float(row["blif_net_count"]) for row in bucket)
            / sum(float(row["blif_net_count"]) for row in bucket)
        )
        dfg_edge_weighted_optimal_ratio = (
            sum(float(row["dfg_optimal_ratio"]) * float(row["dfg_edge_count"]) for row in bucket)
            / sum(float(row["dfg_edge_count"]) for row in bucket)
        )
        blif_net_weighted_optimal_ratio = (
            sum(float(row["blif_optimal_ratio"]) * float(row["blif_net_count"]) for row in bucket)
            / sum(float(row["blif_net_count"]) for row in bucket)
        )
        weighted_swaps_per_node = (
            sum(float(row["swap_attempts"]) for row in bucket)
            / sum(float(row["dfg_nodes"]) for row in bucket)
        )
        summary_rows.append(
            {
                "candidate": candidate.label,
                "dfg_avg_wire": mean("dfg_avg_wire"),
                "blif_avg_wire": mean("blif_avg_wire"),
                "dfg_edge_weighted_wire": dfg_edge_weighted_wire,
                "blif_net_weighted_wire": blif_net_weighted_wire,
                "dfg_optimal_ratio": mean("dfg_optimal_ratio"),
                "blif_optimal_ratio": mean("blif_optimal_ratio"),
                "dfg_edge_weighted_optimal_ratio": dfg_edge_weighted_optimal_ratio,
                "blif_net_weighted_optimal_ratio": blif_net_weighted_optimal_ratio,
                "blif_max_fifo": mean("blif_max_fifo"),
                "mapping_time_ms": mean("mapping_time_ms"),
                "swaps_per_node": mean("swaps_per_node"),
                "weighted_swaps_per_node": weighted_swaps_per_node,
                "max_fifo": mean("max_fifo"),
                "max_fifo_cost_minus_one": mean("max_fifo_cost_minus_one"),
                "blif_max_cost": mean("blif_max_cost"),
                "wire_delta_from_paper_dfg": abs(mean("dfg_avg_wire") - PAPER_AVG_WIRE),
                "wire_delta_from_paper_blif": abs(mean("blif_avg_wire") - PAPER_AVG_WIRE),
                "optimal_delta_from_paper_blif": abs(mean("blif_optimal_ratio") - PAPER_VPR_BB_OPTIMAL_EDGE),
                "swaps_per_node_delta_from_paper": abs(weighted_swaps_per_node - PAPER_VPR_BB_SWAPS_PER_NODE),
            }
        )
    with (output_dir / "vpr_bb_candidate_summary.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(summary_rows)
    with (output_dir / "vpr_bb_candidate_metrics.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    lines = [
        "# VPR-BB Candidate Comparison",
        "",
        "| candidate | DFG avg wire | DFG edge-wt wire | BLIF avg wire | BLIF net-wt wire | DFG optimal | DFG edge-wt optimal | BLIF optimal | BLIF net-wt optimal | time ms | mean swaps/node | weighted swaps/node | max FIFO | cost-1 FIFO |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for row in summary_rows:
        lines.append(
            "| "
            + " | ".join(
                [
                    str(row["candidate"]),
                    f"{row['dfg_avg_wire']:.3f}",
                    f"{row['dfg_edge_weighted_wire']:.3f}",
                    f"{row['blif_avg_wire']:.3f}",
                    f"{row['blif_net_weighted_wire']:.3f}",
                    f"{row['dfg_optimal_ratio'] * 100.0:.1f}%",
                    f"{row['dfg_edge_weighted_optimal_ratio'] * 100.0:.1f}%",
                    f"{row['blif_optimal_ratio'] * 100.0:.1f}%",
                    f"{row['blif_net_weighted_optimal_ratio'] * 100.0:.1f}%",
                    f"{row['mapping_time_ms']:.1f}",
                    f"{row['swaps_per_node']:.0f}",
                    f"{row['weighted_swaps_per_node']:.0f}",
                    f"{row['max_fifo']:.2f} / {row['blif_max_cost']:.2f}",
                    f"{row['max_fifo_cost_minus_one']:.2f} / {row['blif_max_fifo']:.2f}",
                ]
            )
            + " |"
        )
    lines.extend(
        [
            "",
            f"Paper Table 1 VPR-BB average wire length: `{PAPER_AVG_WIRE}`.",
            f"Paper-text VPR-BB optimal mapped edge reference: `{PAPER_VPR_BB_OPTIMAL_EDGE * 100.0:.1f}%`.",
            f"Paper-text VPR-BB swap reference: about `{PAPER_VPR_BB_SWAPS_PER_NODE:.0f}` swaps/node.",
            "DFG optimal uses the active one-hop placement-cost model; BLIF optimal evaluates the VPR netlist.",
            "Max FIFO uses raw one-hop segment cost for the paper VPR-BB comparison; cost-1 FIFO is kept as the local proxy.",
        ]
    )
    (output_dir / "vpr_bb_candidate_summary.md").write_text("\n".join(lines) + "\n")


def main() -> None:
    args = parse_args()
    grid_run = Path(args.grid_run)
    effort_run = Path(args.effort_run)
    multi_run = Path(args.multi_run)
    paper_figure_values = Path(args.paper_figure_values)
    candidates = CANDIDATES if args.candidate_set == "default" else SWEEP_CANDIDATES
    if args.output_dir:
        output_dir = Path(args.output_dir)
    else:
        import time

        output_dir = (
            Path("research/results/placement2d/probes/yott_vpr_bb_candidate_reproduction")
            / time.strftime("%Y%m%d-%H%M%S")
        )
    output_dir.mkdir(parents=True, exist_ok=True)
    rows = collect_rows(grid_run, effort_run, multi_run, candidates)
    write_summary(rows, candidates, output_dir)
    setup_style()
    plot_definition_pairs(rows, candidates, output_dir / "yott_table1_vpr_bb_wire_length_candidates.svg")
    plot_lines(
        rows,
        candidates,
        output_dir / "yott_fig18_vpr_bb_optimal_mapped_edges_candidates.svg",
        "blif_optimal_ratio",
        "Percentage of optimal mapped edges",
        "BLIF/VPR net evaluation",
        paper_line=PAPER_VPR_BB_OPTIMAL_EDGE,
        percent=True,
    )
    plot_lines(
        rows,
        candidates,
        output_dir / "yott_fig18_vpr_bb_dfg_optimal_mapped_edges_candidates.svg",
        "dfg_optimal_ratio",
        "Percentage of optimal mapped edges",
        "DFG edge evaluation",
        paper_line=PAPER_VPR_BB_OPTIMAL_EDGE,
        percent=True,
    )
    plot_lines(
        rows,
        candidates,
        output_dir / "yott_fig20a_vpr_bb_execution_time_candidates.svg",
        "mapping_time_ms",
        "Execution time (ms)",
        "VPR-BB candidate runtime",
        log_y=True,
    )
    plot_lines(
        rows,
        candidates,
        output_dir / "yott_fig20b_vpr_bb_swaps_candidates.svg",
        "swaps_per_node",
        "Number of swaps per node",
        "VPR-BB candidate swaps",
        paper_line=PAPER_VPR_BB_SWAPS_PER_NODE,
        log_y=True,
    )
    plot_lines(
        rows,
        candidates,
        output_dir / "yott_fig22_vpr_bb_max_fifo_candidates.svg",
        "max_fifo",
        "Maximal FIFO Size",
        "VPR-BB candidate maximal FIFO",
        paper_values=paper_vpr_bb_max_fifo(paper_figure_values),
    )
    plot_lines(
        rows,
        candidates,
        output_dir / "yott_fig22_vpr_bb_max_fifo_cost_minus_one_candidates.svg",
        "max_fifo_cost_minus_one",
        "Maximal FIFO Size",
        "VPR-BB candidate cost-1 FIFO proxy",
        paper_values=paper_vpr_bb_max_fifo(paper_figure_values),
    )
    plot_fifo_definition_pairs(
        rows,
        candidates,
        output_dir / "yott_fig22_vpr_bb_max_fifo_definition_candidates.svg",
    )
    print(output_dir)


if __name__ == "__main__":
    main()
