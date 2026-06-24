#!/usr/bin/env python3
"""Plot local YOTT Fig.19/Fig.22-style benchmark quality comparisons."""

from __future__ import annotations

import argparse
import csv
import math
import os
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "cgra_mapper_mpl"))
os.environ.setdefault("XDG_CACHE_HOME", str(Path(tempfile.gettempdir()) / "cgra_mapper_xdg_cache"))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


DEFAULT_METRICS = (
    "research/results/placement2d/reproduction/traversal_yott/"
    "20260622-034821_cpu_mapping_order_degree_compare/metrics.csv"
)
DEFAULT_OUTPUT_DIR = (
    "research/results/placement2d/reproduction/traversal_yott/"
    "20260622-034821_cpu_mapping_order_degree_compare/figures"
)

MAPPER_LABELS = {
    "faithful_array_yoto_100": "Faithful YOTO-100",
    "faithful_array_yoto_1000": "Faithful YOTO-1000",
    "faithful_array_yott_100": "Faithful YOTT-100",
    "faithful_array_yott_1000": "Faithful YOTT-1000",
    "cpu_mapping_yoto_100": "CPU map YOTO-100",
    "cpu_mapping_yoto_1000": "CPU map YOTO-1000",
    "cpu_mapping_yott_100": "CPU map YOTT-100",
    "cpu_mapping_yott_1000": "CPU map YOTT-1000",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metrics-csv", default=DEFAULT_METRICS)
    parser.add_argument("--output-dir", default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--arch", default="one_hop_perimeter_no_corners_io")
    return parser.parse_args()


def as_float(value: str) -> float | None:
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    if math.isnan(result):
        return None
    return result


def load_rows(path: Path, arch: str) -> list[dict[str, str]]:
    selected = set(MAPPER_LABELS)
    rows: list[dict[str, str]] = []
    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row.get("arch_name") != arch:
                continue
            if row.get("mapper") not in selected:
                continue
            if row.get("status") != "success":
                continue
            rows.append(row)
    return rows


def write_csv(rows: list[dict[str, str]], path: Path) -> None:
    fields = [
        "benchmark_set",
        "benchmark",
        "mapper",
        "mapper_label",
        "placement_optimal_distance_ratio",
        "placement_max_fifo",
        "placement_avg_fifo",
        "placement_avg_mesh_hop",
        "mapping_time_sec",
    ]
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(
                {
                    "benchmark_set": row["benchmark_set"],
                    "benchmark": row["benchmark"],
                    "mapper": row["mapper"],
                    "mapper_label": MAPPER_LABELS[row["mapper"]],
                    "placement_optimal_distance_ratio": row[
                        "placement_optimal_distance_ratio"
                    ],
                    "placement_max_fifo": row["placement_max_fifo"],
                    "placement_avg_fifo": row["placement_avg_fifo"],
                    "placement_avg_mesh_hop": row["placement_avg_mesh_hop"],
                    "mapping_time_sec": row["mapping_time_sec"],
                }
            )


def draw(rows: list[dict[str, str]], output_base: Path) -> None:
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
    order = []
    for row in rows:
        key = (row["benchmark_set"], row["benchmark"])
        if key not in order:
            order.append(key)
    order.sort(key=lambda key: (key[0], key[1]))

    mapper_order = [
        "faithful_array_yoto_100",
        "faithful_array_yoto_1000",
        "faithful_array_yott_100",
        "faithful_array_yott_1000",
        "cpu_mapping_yoto_100",
        "cpu_mapping_yoto_1000",
        "cpu_mapping_yott_100",
        "cpu_mapping_yott_1000",
    ]
    colors = {
        "faithful_array_yoto_100": "#A8C5E6",
        "faithful_array_yoto_1000": "#3B6EA8",
        "faithful_array_yott_100": "#1F4E79",
        "faithful_array_yott_1000": "#102A43",
        "cpu_mapping_yoto_100": "#B7DDBB",
        "cpu_mapping_yoto_1000": "#95CBA5",
        "cpu_mapping_yott_100": "#2F8A5B",
        "cpu_mapping_yott_1000": "#195A3A",
    }
    markers = {
        "faithful_array_yoto_100": "o",
        "faithful_array_yoto_1000": "s",
        "faithful_array_yott_100": "^",
        "faithful_array_yott_1000": "D",
        "cpu_mapping_yoto_100": "o",
        "cpu_mapping_yoto_1000": "s",
        "cpu_mapping_yott_100": "^",
        "cpu_mapping_yott_1000": "D",
    }

    value_by_mapper = {
        mapper: {key: (None, None) for key in order} for mapper in mapper_order
    }
    for row in rows:
        key = (row["benchmark_set"], row["benchmark"])
        mapper = row["mapper"]
        value_by_mapper[mapper][key] = (
            as_float(row["placement_optimal_distance_ratio"]),
            as_float(row["placement_max_fifo"]),
        )

    labels = [f"{bench}" for _, bench in order]
    x = list(range(len(order)))

    fig, axes = plt.subplots(2, 1, figsize=(7.2, 4.6), sharex=True, constrained_layout=True)
    panels = [
        (0, "placement_optimal_distance_ratio", "Optimal edge ratio", "higher is better"),
        (1, "placement_max_fifo", "Max FIFO", "lower is better"),
    ]
    for ax_index, _, ylabel, subtitle in panels:
        ax = axes[ax_index]
        for mapper in mapper_order:
            if mapper not in value_by_mapper:
                continue
            series = []
            for key in order:
                values = value_by_mapper[mapper][key]
                series.append(values[0] if ax_index == 0 else values[1])
            ax.plot(
                x,
                series,
                marker=markers[mapper],
                markersize=3.5,
                linewidth=1.2,
                color=colors[mapper],
                label=MAPPER_LABELS[mapper],
            )
        ax.set_ylabel(ylabel)
        ax.set_title(subtitle, loc="left", fontsize=7, color="#555555", pad=2)
        ax.grid(axis="y", color="#D8D8D8", linewidth=0.55, zorder=0)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.tick_params(axis="both", length=2.5, width=0.6)
    axes[0].set_ylim(0.0, 1.05)
    axes[1].set_ylim(bottom=0.0)
    axes[1].set_xticks(x)
    axes[1].set_xticklabels(labels, rotation=50, ha="right")
    axes[0].legend(loc="upper center", bbox_to_anchor=(0.5, 1.34), ncol=3, frameon=False)
    fig.text(
        0.01,
        -0.02,
        "Local Fig.19/Fig.22-style comparison. Plot-only paper figures are compared by reported trends.",
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
    rows = load_rows(Path(args.metrics_csv), args.arch)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(rows, output_dir / "yott_fig19_22_local_values.csv")
    draw(rows, output_dir / "yott_fig19_22_local")
    print(output_dir / "yott_fig19_22_local.pdf")
    print(output_dir / "yott_fig19_22_local.png")
    print(output_dir / "yott_fig19_22_local.svg")


if __name__ == "__main__":
    main()
