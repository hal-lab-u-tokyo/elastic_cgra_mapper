#!/usr/bin/env python3
"""Plot YOTO/YOTT paper-vs-implementation placement metrics."""

from __future__ import annotations

import argparse
import csv
import math
import os
import tempfile
from collections import defaultdict
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "cgra_mapper_mpl"))
os.environ.setdefault("XDG_CACHE_HOME", str(Path(tempfile.gettempdir()) / "cgra_mapper_xdg_cache"))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch


PAPER = {
    "YOTO-1000": {
        "speedup_vs_vpr": 4.2,
        "wire_length": 1.17,
        "fifo_reduction_vs_vpr_bb": 1.92,
    },
    "YOTT-1000": {
        "speedup_vs_vpr": 2.1,
        "wire_length": 1.16,
        "fifo_reduction_vs_vpr_bb": 2.38,
    },
}

IMPLEMENTATIONS = {
    "Faithful Array": {
        "YOTO-1000": "faithful_array_yoto_1000",
        "YOTT-1000": "faithful_array_yott_1000",
    },
    "CPU Mapping": {
        "YOTO-1000": "cpu_mapping_yoto_1000",
        "YOTT-1000": "cpu_mapping_yott_1000",
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--metrics-csv",
        default=(
            "research/results/placement2d/reproduction/traversal_yott/"
            "20260622-034821_cpu_mapping_order_degree_compare/metrics.csv"
        ),
        help="metrics.csv produced by run_suite.py",
    )
    parser.add_argument(
        "--output-dir",
        default=(
            "research/results/placement2d/reproduction/traversal_yott/"
            "20260622-034821_cpu_mapping_order_degree_compare/figures"
        ),
        help="directory for the generated figure and plotted-value CSVs",
    )
    return parser.parse_args()


def to_float(value: str) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    if math.isnan(number):
        return None
    return number


def mean(values: list[float]) -> float:
    return sum(values) / len(values)


def load_mapper_means(metrics_csv: Path) -> dict[str, dict[str, float]]:
    buckets: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    with metrics_csv.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            mapper = row["mapper"]
            for metric in (
                "mapping_time_sec",
                "placement_avg_mesh_hop",
                "placement_avg_fifo",
                "placement_optimal_distance_ratio",
                "placement_max_fifo",
                "placement_mapped_lp_mesh_hop",
            ):
                number = to_float(row.get(metric, ""))
                if number is not None:
                    buckets[mapper][metric].append(number)

    means: dict[str, dict[str, float]] = {}
    for mapper, metric_values in buckets.items():
        means[mapper] = {
            metric: mean(values) for metric, values in metric_values.items() if values
        }
    return means


def build_plot_rows(mapper_means: dict[str, dict[str, float]]) -> list[dict[str, str | float]]:
    rows: list[dict[str, str | float]] = []

    paper_yoto = PAPER["YOTO-1000"]
    paper_fifo_yoto = paper_yoto["fifo_reduction_vs_vpr_bb"]
    paper_speed_yoto = paper_yoto["speedup_vs_vpr"]
    for algorithm, values in PAPER.items():
        rows.append(
            {
                "source": "Paper",
                "algorithm": algorithm,
                "relative_runtime_to_yoto": paper_speed_yoto / values["speedup_vs_vpr"],
                "wire_length_segments": values["wire_length"],
                "relative_fifo_to_yoto": paper_fifo_yoto
                / values["fifo_reduction_vs_vpr_bb"],
                "raw_speedup_vs_vpr": values["speedup_vs_vpr"],
                "raw_fifo_reduction_vs_vpr_bb": values["fifo_reduction_vs_vpr_bb"],
            }
        )

    for source, mapper_names in IMPLEMENTATIONS.items():
        yoto_mapper = mapper_names["YOTO-1000"]
        yoto_metrics = mapper_means[yoto_mapper]
        yoto_time = yoto_metrics["mapping_time_sec"]
        yoto_fifo = yoto_metrics["placement_avg_fifo"]
        for algorithm, mapper in mapper_names.items():
            metrics = mapper_means[mapper]
            rows.append(
                {
                    "source": source,
                    "algorithm": algorithm,
                    "relative_runtime_to_yoto": metrics["mapping_time_sec"] / yoto_time,
                    "wire_length_segments": metrics["placement_avg_mesh_hop"],
                    "relative_fifo_to_yoto": metrics["placement_avg_fifo"] / yoto_fifo,
                    "raw_mapping_time_sec": metrics["mapping_time_sec"],
                    "raw_avg_fifo": metrics["placement_avg_fifo"],
                    "raw_optimal_edge_ratio": metrics["placement_optimal_distance_ratio"],
                    "raw_max_fifo": metrics["placement_max_fifo"],
                    "raw_mapped_lp_mesh_hop": metrics["placement_mapped_lp_mesh_hop"],
                }
            )
    return rows


def write_rows_csv(rows: list[dict[str, str | float]], path: Path) -> None:
    fields = [
        "source",
        "algorithm",
        "relative_runtime_to_yoto",
        "wire_length_segments",
        "relative_fifo_to_yoto",
        "raw_speedup_vs_vpr",
        "raw_fifo_reduction_vs_vpr_bb",
        "raw_mapping_time_sec",
        "raw_avg_fifo",
        "raw_optimal_edge_ratio",
        "raw_max_fifo",
        "raw_mapped_lp_mesh_hop",
    ]
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def get_value(rows: list[dict[str, str | float]], source: str, algorithm: str, metric: str) -> float:
    for row in rows:
        if row["source"] == source and row["algorithm"] == algorithm:
            return float(row[metric])
    raise KeyError((source, algorithm, metric))


def draw_figure(rows: list[dict[str, str | float]], output_base: Path) -> None:
    plt.rcParams.update(
        {
            "font.family": "Helvetica",
            "font.size": 8,
            "axes.linewidth": 0.7,
            "axes.labelsize": 8,
            "xtick.labelsize": 7.5,
            "ytick.labelsize": 7.5,
            "legend.fontsize": 7.5,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )

    sources = ["Paper", "Faithful Array", "CPU Mapping"]
    algorithms = ["YOTO-1000", "YOTT-1000"]
    colors = {
        "Paper": "#4D4D4D",
        "Faithful Array": "#3B6EA8",
        "CPU Mapping": "#2F8A5B",
    }
    hatches = {"YOTO-1000": "", "YOTT-1000": "///"}

    panels = [
        ("relative_runtime_to_yoto", "Runtime / YOTO-1000", "lower is better", (0, 2.35)),
        ("wire_length_segments", "Average wire length", "segments, lower is better", None),
        ("relative_fifo_to_yoto", "FIFO size / YOTO-1000", "lower is better", None),
    ]

    fig, axes = plt.subplots(1, 3, figsize=(7.1, 2.55), constrained_layout=True)

    width = 0.28
    x_positions = list(range(len(sources)))
    offsets = {"YOTO-1000": -width / 2, "YOTT-1000": width / 2}

    for ax, (metric, ylabel, subtitle, ylim) in zip(axes, panels):
        for algorithm in algorithms:
            values = [get_value(rows, source, algorithm, metric) for source in sources]
            bars = ax.bar(
                [x + offsets[algorithm] for x in x_positions],
                values,
                width=width,
                color=[colors[source] for source in sources],
                edgecolor="#222222",
                linewidth=0.45,
                hatch=hatches[algorithm],
                zorder=3,
            )
            for bar, value in zip(bars, values):
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    bar.get_height(),
                    f"{value:.2f}",
                    ha="center",
                    va="bottom",
                    fontsize=6.2,
                    color="#222222",
                    rotation=0,
                )

        ax.set_ylabel(ylabel)
        ax.set_title(subtitle, loc="left", fontsize=7, color="#555555", pad=2)
        ax.set_xticks(x_positions)
        ax.set_xticklabels(sources, rotation=18, ha="right")
        ax.grid(axis="y", color="#D8D8D8", linewidth=0.55, zorder=0)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.tick_params(axis="both", length=2.5, width=0.6)
        if ylim is not None:
            ax.set_ylim(*ylim)
        else:
            current_top = ax.get_ylim()[1]
            ax.set_ylim(0, current_top * 1.12)

    legend_items = [
        Patch(facecolor="#777777", edgecolor="#222222", label="YOTO-1000"),
        Patch(facecolor="#777777", edgecolor="#222222", hatch="///", label="YOTT-1000"),
    ]
    fig.legend(
        handles=legend_items,
        loc="upper center",
        bbox_to_anchor=(0.52, 1.07),
        ncol=2,
        frameon=False,
        handlelength=1.5,
        columnspacing=1.4,
    )
    fig.text(
        0.01,
        -0.04,
        "Paper runtime and FIFO panels are normalized from YOTT Table 1; implementation values are from metrics.csv means.",
        ha="left",
        va="bottom",
        fontsize=6.4,
        color="#666666",
    )

    fig.savefig(output_base.with_suffix(".pdf"), bbox_inches="tight")
    fig.savefig(output_base.with_suffix(".png"), dpi=300, bbox_inches="tight")
    fig.savefig(output_base.with_suffix(".svg"), bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    args = parse_args()
    metrics_csv = Path(args.metrics_csv)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    mapper_means = load_mapper_means(metrics_csv)
    rows = build_plot_rows(mapper_means)
    write_rows_csv(rows, output_dir / "traversal_yott_paper_comparison_values.csv")
    draw_figure(rows, output_dir / "traversal_yott_paper_comparison")
    print(output_dir / "traversal_yott_paper_comparison.pdf")
    print(output_dir / "traversal_yott_paper_comparison.png")
    print(output_dir / "traversal_yott_paper_comparison.svg")


if __name__ == "__main__":
    main()
