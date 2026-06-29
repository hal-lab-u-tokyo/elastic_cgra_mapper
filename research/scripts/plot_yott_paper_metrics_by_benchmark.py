#!/usr/bin/env python3
"""Plot local YOTT paper-style metrics per benchmark."""

from __future__ import annotations

import argparse
import csv
import math
import os
import re
import tempfile
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "cgra_mapper_mpl"))
os.environ.setdefault("XDG_CACHE_HOME", str(Path(tempfile.gettempdir()) / "cgra_mapper_xdg_cache"))

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


DEFAULT_RUN_ROOT = "research/results/placement2d/paper_comparison/yott_cases2021"
BASELINE_MAPPER = "vpr_bb"

MAPPER_LABELS = {
    BASELINE_MAPPER: "VPR BB",
    "faithful_array_yoto_100": "Faithful YOTO-100",
    "faithful_array_yoto_1000": "Faithful YOTO-1000",
    "faithful_array_yott_100": "Faithful YOTT-100",
    "faithful_array_yott_1000": "Faithful YOTT-1000",
    "cpu_mapping_yoto_100": "CPU map YOTO-100",
    "cpu_mapping_yoto_1000": "CPU map YOTO-1000",
    "cpu_mapping_yott_100": "CPU map YOTT-100",
    "cpu_mapping_yott_1000": "CPU map YOTT-1000",
}

TABLE1_MAPPERS = [
    BASELINE_MAPPER,
    "faithful_array_yoto_100",
    "faithful_array_yoto_1000",
    "faithful_array_yott_100",
    "faithful_array_yott_1000",
    "cpu_mapping_yoto_100",
    "cpu_mapping_yoto_1000",
    "cpu_mapping_yott_100",
    "cpu_mapping_yott_1000",
]

YOTO_YOTT_FIGURE_MAPPERS = [
    BASELINE_MAPPER,
    "faithful_array_yoto_100",
    "faithful_array_yoto_1000",
    "faithful_array_yott_100",
    "faithful_array_yott_1000",
    "cpu_mapping_yoto_100",
    "cpu_mapping_yoto_1000",
    "cpu_mapping_yott_100",
    "cpu_mapping_yott_1000",
]

COLORS = {
    "faithful_array_yoto_100": "#9BBFDB",
    "faithful_array_yoto_1000": "#3F6F9F",
    "faithful_array_yott_100": "#133C63",
    "faithful_array_yott_1000": "#071E33",
    "cpu_mapping_yoto_100": "#A9D7A7",
    "cpu_mapping_yoto_1000": "#63A86A",
    "cpu_mapping_yott_100": "#26784D",
    "cpu_mapping_yott_1000": "#11482E",
    BASELINE_MAPPER: "#8A8A8A",
}

MARKERS = {
    "faithful_array_yoto_100": "o",
    "faithful_array_yoto_1000": "s",
    "faithful_array_yott_100": "^",
    "faithful_array_yott_1000": "D",
    "cpu_mapping_yoto_100": "o",
    "cpu_mapping_yoto_1000": "s",
    "cpu_mapping_yott_100": "^",
    "cpu_mapping_yott_1000": "D",
    BASELINE_MAPPER: "x",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--run-dir",
        default=None,
        help="Result directory containing metrics.csv. Defaults to the latest yott_cases2021 run.",
    )
    parser.add_argument("--paper-dir", default="research/data/papers")
    parser.add_argument("--output-dir", default=None)
    return parser.parse_args()


def resolve_run_dir(path: str | None) -> Path:
    if path is not None:
        return Path(path)
    root = Path(DEFAULT_RUN_ROOT)
    runs = sorted(p.parent for p in root.glob("*/metrics.csv"))
    if not runs:
        raise FileNotFoundError(f"No metrics.csv found under {root}")
    return runs[-1]


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


def as_float(value: str | None) -> float | None:
    if value is None or value == "":
        return None
    try:
        result = float(value)
    except ValueError:
        return None
    if math.isnan(result):
        return None
    return result


def safe_ratio(numerator: float | None, denominator: float | None) -> float | None:
    if numerator is None or denominator is None:
        return None
    if denominator == 0:
        if numerator == 0:
            return None
        return math.inf
    return numerator / denominator


def metric(row: dict[str, str] | None, key: str) -> float | None:
    if row is None:
        return None
    return as_float(row.get(key))


def fifo_sum(row: dict[str, str] | None) -> float | None:
    value = metric(row, "placement_paper_fifo_sum")
    if value is not None:
        return value
    avg = metric(row, "placement_avg_paper_fifo")
    edge_count = metric(row, "placement_edge_count")
    if avg is None or edge_count is None:
        return None
    return avg * edge_count


def load_paper_order(path: Path) -> list[dict[str, str]]:
    rows = []
    for row in read_csv(path):
        if row.get("benchmark") and row["benchmark"] != "avg":
            rows.append(row)
    return sorted(rows, key=lambda row: int(row["index"]))


def load_paper_vpr_bb_fifo(path: Path) -> dict[str, float]:
    values: dict[str, float] = {}
    if not path.exists():
        return values
    for row in read_csv(path):
        if row.get("method") != "VPR_BB":
            continue
        if row.get("metric") != "maximal_fifo_size":
            continue
        value = as_float(row.get("value"))
        if value is not None:
            values[canonical_benchmark(row["benchmark"])] = value
    return values


def load_metrics(path: Path) -> dict[tuple[str, str], dict[str, str]]:
    selected = set(MAPPER_LABELS) | {BASELINE_MAPPER}
    rows = {}
    for row in read_csv(path):
        if row.get("mapper") in selected and row.get("status") == "success":
            rows[(canonical_benchmark(row["benchmark"]), row["mapper"])] = row
    return rows


def paper_metric_value(
    row: dict[str, str] | None, baseline: dict[str, str] | None, metric_name: str
) -> float | None:
    if metric_name == "speedup_vs_vpr_bb":
        if row is baseline:
            return 1.0
        return safe_ratio(metric(baseline, "mapping_time_sec"), metric(row, "mapping_time_sec"))
    if metric_name == "average_wire_length_segments":
        # YOTT's fully-pipelined one-hop CGRA reports wire length in
        # one-hop routing segments.  `placement_avg_cost` follows the active
        # placement cost model, while `placement_avg_mesh_hop` is raw Manhattan
        # distance and overstates same-row/column one-hop links.
        return metric(row, "placement_avg_cost")
    if metric_name == "fifo_reduction_vs_vpr_bb":
        if row is baseline:
            return 1.0
        return safe_ratio(fifo_sum(baseline), fifo_sum(row))
    if metric_name == "optimal_mapped_edge_ratio":
        return metric(row, "placement_optimal_edge_ratio")
    if metric_name == "max_fifo":
        # YOTT Fig.22 counts one-hop links as FIFO size 1.  This matches the
        # placement cost used for the one-hop CGRA model; the older
        # `placement_max_paper_fifo` column subtracts one and makes adjacent
        # edges appear as zero FIFO.
        return metric(row, "placement_max_cost")
    if metric_name == "mapping_time_sec":
        return metric(row, "mapping_time_sec")
    if metric_name == "mapping_time_ms":
        value = metric(row, "mapping_time_sec")
        return None if value is None else value * 1000.0
    if metric_name == "swap_count":
        return logged_swap_count(row)
    raise ValueError(metric_name)


def logged_swap_count(row: dict[str, str] | None) -> float | None:
    if row is None:
        return None
    value = as_float(row.get("placement_swap_attempts"))
    if value is not None:
        return value
    candidates = [row.get("gurobi_log_file", ""), row.get("stdout_file", "")]
    patterns = [
        re.compile(r"placement_swap_attempts=([0-9]+)"),
        re.compile(r"Placement total # of swap attempts:\s*([0-9]+)"),
    ]
    for candidate in candidates:
        if not candidate:
            continue
        path = Path(candidate)
        if not path.exists():
            continue
        text = path.read_text(errors="ignore")
        for pattern in patterns:
            match = pattern.search(text)
            if match:
                return float(match.group(1))
    return None


def finite_plot_values(values: list[float | None]) -> tuple[list[float | None], bool]:
    finite = [v for v in values if v is not None and math.isfinite(v)]
    if not finite:
        return values, False
    cap = max(finite) * 1.12 if max(finite) > 0 else 1.0
    capped = [cap if v is not None and math.isinf(v) else v for v in values]
    return capped, any(v is not None and math.isinf(v) for v in values)


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


def draw_line_plot(
    *,
    paper_rows: list[dict[str, str]],
    metrics_by_key: dict[tuple[str, str], dict[str, str]],
    paper_vpr_bb_fifo: dict[str, float] | None,
    mappers: list[str],
    metric_name: str,
    ylabel: str,
    output_base: Path,
    ylog: bool = False,
    y_min: float | None = None,
    y_max: float | None = None,
) -> None:
    setup_style()
    x = list(range(1, len(paper_rows) + 1))
    labels = [row["index"] for row in paper_rows]
    names = [row["benchmark"] for row in paper_rows]
    fig, ax = plt.subplots(figsize=(7.2, 3.2), constrained_layout=True)
    any_inf = False

    if metric_name == "max_fifo" and paper_vpr_bb_fifo:
        values = [
            paper_vpr_bb_fifo.get(canonical_benchmark(paper["benchmark"]))
            for paper in paper_rows
        ]
        plot_values, _ = finite_plot_values(values)
        ax.plot(
            x,
            plot_values,
            marker="x",
            markersize=3.4,
            linewidth=1.2,
            linestyle="--",
            color="#111111",
            label="Paper VPR BB",
        )

    for mapper in mappers:
        values = []
        for paper in paper_rows:
            key = canonical_benchmark(paper["benchmark"])
            row = metrics_by_key.get((key, mapper))
            baseline = metrics_by_key.get((key, BASELINE_MAPPER))
            value = paper_metric_value(row, baseline, metric_name)
            if metric_name == "optimal_mapped_edge_ratio" and value is not None:
                value *= 100.0
            values.append(value)
        plot_values, had_inf = finite_plot_values(values)
        any_inf = any_inf or had_inf
        ax.plot(
            x,
            plot_values,
            marker=MARKERS[mapper],
            markersize=3.4,
            linewidth=1.2,
            color=COLORS[mapper],
            label=MAPPER_LABELS[mapper],
        )

    ax.set_ylabel(ylabel)
    ax.set_xlabel("Benchmarks")
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    if ylog:
        ax.set_yscale("log")
    if y_min is not None or y_max is not None:
        ax.set_ylim(bottom=y_min, top=y_max)
    ax.grid(axis="y", color="#D8D8D8", linewidth=0.55)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.tick_params(axis="both", length=2.5, width=0.6)
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, 1.25), ncol=3, frameon=False)
    if any_inf:
        fig.text(
            0.01,
            -0.03,
            "Infinite FIFO-reduction values are capped at the plot top.",
            ha="left",
            va="bottom",
            fontsize=5.7,
            color="#666666",
        )

    for ext in (".svg", ".pdf", ".png"):
        kwargs = {"bbox_inches": "tight"}
        if ext == ".png":
            kwargs["dpi"] = 300
        fig.savefig(output_base.with_suffix(ext), **kwargs)
    plt.close(fig)


def draw_fig20(
    *,
    paper_rows: list[dict[str, str]],
    metrics_by_key: dict[tuple[str, str], dict[str, str]],
    mappers: list[str],
    output_base: Path,
) -> None:
    setup_style()
    x = list(range(1, len(paper_rows) + 1))
    labels = [row["index"] for row in paper_rows]
    fig, axes = plt.subplots(2, 1, figsize=(7.2, 4.3), sharex=True, constrained_layout=True)
    panels = [
        (axes[0], "mapping_time_ms", "(a) Execution time (ms)", True),
        (axes[1], "swap_count", "(b) Number of swaps", True),
    ]
    for ax, metric_name, ylabel, ylog in panels:
        for mapper in mappers:
            values = []
            for paper in paper_rows:
                key = canonical_benchmark(paper["benchmark"])
                row = metrics_by_key.get((key, mapper))
                baseline = metrics_by_key.get((key, BASELINE_MAPPER))
                values.append(paper_metric_value(row, baseline, metric_name))
            plot_values, _ = finite_plot_values(values)
            ax.plot(
                x,
                plot_values,
                marker=MARKERS[mapper],
                markersize=3.2,
                linewidth=1.1,
                color=COLORS[mapper],
                label=MAPPER_LABELS[mapper],
            )
        ax.set_ylabel(ylabel)
        if ylog:
            ax.set_yscale("log")
        ax.grid(axis="y", color="#D8D8D8", linewidth=0.55)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)
        ax.tick_params(axis="both", length=2.5, width=0.6)
    axes[1].set_xlabel("Benchmarks")
    axes[1].set_xticks(x)
    axes[1].set_xticklabels(labels)
    axes[0].legend(loc="upper center", bbox_to_anchor=(0.5, 1.34), ncol=3, frameon=False)

    for ext in (".svg", ".pdf", ".png"):
        kwargs = {"bbox_inches": "tight"}
        if ext == ".png":
            kwargs["dpi"] = 300
        fig.savefig(output_base.with_suffix(ext), **kwargs)
    plt.close(fig)


def draw_all(
    paper_rows: list[dict[str, str]],
    metrics_by_key: dict[tuple[str, str], dict[str, str]],
    paper_vpr_bb_fifo: dict[str, float],
    output_dir: Path,
) -> list[Path]:
    figures = [
        (
            "yott_table1_speedup_vs_vpr_bb_by_benchmark",
            TABLE1_MAPPERS,
            "speedup_vs_vpr_bb",
            "Speedup Ratio",
            True,
            None,
            None,
        ),
        (
            "yott_table1_average_wire_length_by_benchmark",
            TABLE1_MAPPERS,
            "average_wire_length_segments",
            "Average Wire Length in Segments",
            False,
            0,
            None,
        ),
        (
            "yott_table1_fifo_reduction_vs_vpr_bb_by_benchmark",
            TABLE1_MAPPERS,
            "fifo_reduction_vs_vpr_bb",
            "FIFO Size Reduction",
            False,
            0,
            None,
        ),
        (
            "yott_fig19_optimal_mapped_edges_by_benchmark",
            YOTO_YOTT_FIGURE_MAPPERS,
            "optimal_mapped_edge_ratio",
            "Percentage of optimal mapped edges",
            False,
            50,
            105,
        ),
        (
            "yott_fig22_max_fifo_by_benchmark",
            YOTO_YOTT_FIGURE_MAPPERS,
            "max_fifo",
            "Maximal FIFO Size",
            False,
            0,
            None,
        ),
    ]
    written: list[Path] = []
    for stem, mappers, metric_name, ylabel, ylog, y_min, y_max in figures:
        output_base = output_dir / stem
        draw_line_plot(
            paper_rows=paper_rows,
            metrics_by_key=metrics_by_key,
            paper_vpr_bb_fifo=paper_vpr_bb_fifo,
            mappers=mappers,
            metric_name=metric_name,
            ylabel=ylabel,
            output_base=output_base,
            ylog=ylog,
            y_min=y_min,
            y_max=y_max,
        )
        written.append(output_base.with_suffix(".svg"))
    fig20_base = output_dir / "yott_fig20_execution_time_and_swaps_by_benchmark"
    draw_fig20(
        paper_rows=paper_rows,
        metrics_by_key=metrics_by_key,
        mappers=TABLE1_MAPPERS,
        output_base=fig20_base,
    )
    written.append(fig20_base.with_suffix(".svg"))
    return written


def main() -> None:
    args = parse_args()
    run_dir = resolve_run_dir(args.run_dir)
    output_dir = Path(args.output_dir) if args.output_dir else run_dir / "figures"
    output_dir.mkdir(parents=True, exist_ok=True)
    paper_rows = load_paper_order(Path(args.paper_dir) / "yott_table3_benchmarks.csv")
    paper_vpr_bb_fifo = load_paper_vpr_bb_fifo(
        Path(args.paper_dir) / "yott_figure_paper_metrics.csv"
    )
    metrics_by_key = load_metrics(run_dir / "metrics.csv")
    for path in draw_all(paper_rows, metrics_by_key, paper_vpr_bb_fifo, output_dir):
        print(path)


if __name__ == "__main__":
    main()
