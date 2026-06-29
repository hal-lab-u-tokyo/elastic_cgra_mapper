#!/usr/bin/env python3
"""Create per-benchmark YOTO/YOTT comparison tables from local metrics."""

from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


DEFAULT_RUN_ROOT = "research/results/placement2d/paper_comparison/yott_cases2021"

MAPPER_LABELS = {
    "vpr_bb": "VPR BB",
    "faithful_array_yoto_100": "Faithful YOTO-100",
    "faithful_array_yoto_1000": "Faithful YOTO-1000",
    "faithful_array_yott_100": "Faithful YOTT-100",
    "faithful_array_yott_1000": "Faithful YOTT-1000",
    "cpu_mapping_yoto_100": "CPU map YOTO-100",
    "cpu_mapping_yoto_1000": "CPU map YOTO-1000",
    "cpu_mapping_yott_100": "CPU map YOTT-100",
    "cpu_mapping_yott_1000": "CPU map YOTT-1000",
}

BASELINE_MAPPER = "vpr_bb"

REPORT_MAPPERS = [
    "faithful_array_yoto_1000",
    "faithful_array_yott_1000",
    "cpu_mapping_yoto_1000",
    "cpu_mapping_yott_1000",
]

PAPER_METHOD_TO_MAPPERS = {
    ("vpr_bb", ""): ["vpr_bb"],
    ("yoto", "100"): ["faithful_array_yoto_100", "cpu_mapping_yoto_100"],
    ("yott", "100"): ["faithful_array_yott_100", "cpu_mapping_yott_100"],
    ("yoto", "1000"): ["faithful_array_yoto_1000", "cpu_mapping_yoto_1000"],
    ("yott", "1000"): ["faithful_array_yott_1000", "cpu_mapping_yott_1000"],
}

PAPER_AVERAGE_COLUMNS = [
    "method",
    "trials",
    "speedup_vs_vpr_bb",
    "average_wire_length_segments",
    "fifo_reduction_vs_vpr_bb",
]


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


def as_float(value: str | None) -> float | None:
    if value is None or value == "":
        return None
    try:
        number = float(value)
    except ValueError:
        return None
    if math.isnan(number):
        return None
    return number


def fmt(value: float | None, digits: int = 3) -> str:
    if value is None:
        return "-"
    if math.isinf(value):
        return "inf"
    return f"{value:.{digits}f}"


def metric_text(value: float | None) -> str:
    if value is None:
        return ""
    if math.isinf(value):
        return "inf"
    return repr(value)


def canonical_benchmark(name: str) -> str:
    aliases = {
        "cplx8": "cplx8",
        "filterrgb": "filter_rgb",
        "filter_rgb": "filter_rgb",
        "fir16": "fir16",
        "h2v2_smo": "h2v2_smo",
        "h2v2_smooth": "h2v2_smo",
    }
    lower = name.strip().lower()
    return aliases.get(lower, lower)


def load_paper_table3(path: Path) -> dict[str, dict[str, str]]:
    rows = read_csv(path)
    result: dict[str, dict[str, str]] = {}
    for row in rows:
        name = row.get("benchmark", "")
        if not name or name == "avg":
            continue
        result[canonical_benchmark(name)] = row
    return result


def load_metrics(path: Path) -> list[dict[str, str]]:
    rows = []
    for row in read_csv(path):
        mapper = row.get("mapper", "")
        if mapper not in MAPPER_LABELS:
            continue
        rows.append(row)
    return rows


def paper_value(paper_row: dict[str, str] | None, key: str) -> str:
    if paper_row is None:
        return ""
    return paper_row.get(key, "")


def write_long_csv(
    metrics_rows: list[dict[str, str]],
    paper_by_benchmark: dict[str, dict[str, str]],
    output_csv: Path,
) -> None:
    fields = [
        "benchmark",
        "paper_index",
        "paper_nodes",
        "paper_edges",
        "paper_inputs",
        "paper_outputs",
        "paper_yott_first_percent",
        "paper_yoto_first_percent",
        "paper_yott_reconvergent_percent",
        "paper_yoto_reconvergent_percent",
        "mapper",
        "mapper_label",
        "status",
        "mapping_time_sec",
        "placement_optimal_edge_ratio",
        "placement_avg_mesh_hop",
        "placement_avg_paper_fifo",
        "placement_max_cost",
        "placement_mapped_lp_mesh_hop",
    ]
    with output_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for row in sorted(
            metrics_rows,
            key=lambda r: (
                int(paper_by_benchmark.get(canonical_benchmark(r["benchmark"]), {}).get("index", "999")),
                r["benchmark"],
                r["mapper"],
            ),
        ):
            paper_row = paper_by_benchmark.get(canonical_benchmark(row["benchmark"]))
            writer.writerow(
                {
                    "benchmark": row["benchmark"],
                    "paper_index": paper_value(paper_row, "index"),
                    "paper_nodes": paper_value(paper_row, "nodes"),
                    "paper_edges": paper_value(paper_row, "edges"),
                    "paper_inputs": paper_value(paper_row, "inputs"),
                    "paper_outputs": paper_value(paper_row, "outputs"),
                    "paper_yott_first_percent": paper_value(paper_row, "yott_first_percent"),
                    "paper_yoto_first_percent": paper_value(paper_row, "yoto_first_percent"),
                    "paper_yott_reconvergent_percent": paper_value(
                        paper_row, "yott_reconvergent_percent"
                    ),
                    "paper_yoto_reconvergent_percent": paper_value(
                        paper_row, "yoto_reconvergent_percent"
                    ),
                    "mapper": row["mapper"],
                    "mapper_label": MAPPER_LABELS[row["mapper"]],
                    "status": row.get("status", ""),
                    "mapping_time_sec": row.get("mapping_time_sec", ""),
                    "placement_optimal_edge_ratio": row.get(
                        "placement_optimal_edge_ratio", ""
                    ),
                    "placement_avg_mesh_hop": row.get("placement_avg_mesh_hop", ""),
                    "placement_avg_paper_fifo": row.get("placement_avg_paper_fifo", ""),
                    "placement_max_cost": row.get("placement_max_cost", ""),
                    "placement_mapped_lp_mesh_hop": row.get(
                        "placement_mapped_lp_mesh_hop", ""
                    ),
                }
            )


def write_paper_metric_csv(
    paper_by_benchmark: dict[str, dict[str, str]],
    metrics_by_key: dict[tuple[str, str], dict[str, str]],
    output_csv: Path,
) -> None:
    fields = [
        "benchmark",
        "paper_index",
        "paper_nodes",
        "paper_edges",
        "paper_yoto_first_percent",
        "paper_yoto_reconvergent_percent",
        "paper_yott_first_percent",
        "paper_yott_reconvergent_percent",
        "mapper",
        "mapper_label",
        "local_speedup_vs_vpr_bb",
        "local_average_wire_length_segments",
        "local_fifo_reduction_vs_vpr_bb",
        "local_mapping_time_sec",
        "local_swap_attempts",
        "local_optimal_edge_ratio",
        "local_max_fifo",
    ]
    paper_rows = sorted(
        paper_by_benchmark.values(), key=lambda row: int(row.get("index", "999"))
    )
    with output_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for paper in paper_rows:
            benchmark = paper["benchmark"]
            baseline = metrics_by_key.get((canonical_benchmark(benchmark), BASELINE_MAPPER))
            for mapper, label in MAPPER_LABELS.items():
                row = metrics_by_key.get((canonical_benchmark(benchmark), mapper))
                metrics = local_paper_metrics(row, baseline)
                writer.writerow(
                    {
                        "benchmark": benchmark,
                        "paper_index": paper["index"],
                        "paper_nodes": paper["nodes"],
                        "paper_edges": paper["edges"],
                        "paper_yoto_first_percent": paper["yoto_first_percent"],
                        "paper_yoto_reconvergent_percent": paper[
                            "yoto_reconvergent_percent"
                        ],
                        "paper_yott_first_percent": paper["yott_first_percent"],
                        "paper_yott_reconvergent_percent": paper[
                            "yott_reconvergent_percent"
                        ],
                        "mapper": mapper,
                        "mapper_label": label,
                        "local_speedup_vs_vpr_bb": metric_text(
                            metrics["speedup_vs_vpr_bb"]
                        ),
                        "local_average_wire_length_segments": metric_text(
                            metrics["average_wire_length_segments"]
                        ),
                        "local_fifo_reduction_vs_vpr_bb": metric_text(
                            metrics["fifo_reduction_vs_vpr_bb"]
                        ),
                        "local_mapping_time_sec": metric_text(
                            metrics["mapping_time_sec"]
                        ),
                        "local_swap_attempts": metric_text(
                            metrics["placement_swap_attempts"]
                        ),
                        "local_optimal_edge_ratio": metric_text(
                            metric(row, "placement_optimal_edge_ratio")
                        ),
                        "local_max_fifo": metric_text(
                            metric(row, "placement_max_cost")
                        ),
                    }
                )


def row_map(metrics_rows: list[dict[str, str]]) -> dict[tuple[str, str], dict[str, str]]:
    return {
        (canonical_benchmark(row["benchmark"]), row["mapper"]): row
        for row in metrics_rows
    }


def metric(row: dict[str, str] | None, key: str) -> float | None:
    if row is None:
        return None
    return as_float(row.get(key))


def local_wire_length(row: dict[str, str] | None) -> float | None:
    # Use the active placement cost model for paper-style wire length.
    # In the YOTT one-hop setting this is the segment count used for FIFO
    # estimates; raw Manhattan hop is exported separately for routing studies.
    return metric(row, "placement_avg_cost")


def fifo_sum(row: dict[str, str] | None) -> float | None:
    value = metric(row, "placement_paper_fifo_sum")
    if value is not None:
        return value
    avg = metric(row, "placement_avg_paper_fifo")
    edge_count = metric(row, "placement_edge_count")
    if avg is None or edge_count is None:
        return None
    return avg * edge_count


def safe_ratio(numerator: float | None, denominator: float | None) -> float | None:
    if numerator is None or denominator is None:
        return None
    if denominator == 0:
        if numerator == 0:
            return None
        return math.inf
    return numerator / denominator


def local_paper_metrics(
    row: dict[str, str] | None, baseline_row: dict[str, str] | None
) -> dict[str, float | None]:
    is_baseline = row is not None and row is baseline_row
    return {
        "speedup_vs_vpr_bb": 1.0
        if is_baseline
        else safe_ratio(
            metric(baseline_row, "mapping_time_sec"), metric(row, "mapping_time_sec")
        ),
        "average_wire_length_segments": local_wire_length(row),
        "fifo_reduction_vs_vpr_bb": 1.0
        if is_baseline
        else safe_ratio(fifo_sum(baseline_row), fifo_sum(row)),
        "mapping_time_sec": metric(row, "mapping_time_sec"),
        "placement_swap_attempts": metric(row, "placement_swap_attempts"),
    }


def method_summary(metrics_rows: list[dict[str, str]]) -> dict[str, dict[str, float]]:
    buckets: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    for row in metrics_rows:
        if row.get("status") != "success":
            continue
        mapper = row["mapper"]
        for key in [
            "mapping_time_sec",
            "placement_optimal_edge_ratio",
            "placement_avg_mesh_hop",
            "placement_avg_paper_fifo",
            "placement_max_cost",
            "placement_mapped_lp_mesh_hop",
            "placement_swap_attempts",
        ]:
            value = as_float(row.get(key))
            if value is not None:
                buckets[mapper][key].append(value)
    return {
        mapper: {key: sum(values) / len(values) for key, values in values_by_key.items()}
        for mapper, values_by_key in buckets.items()
    }


def add_paper_average_table(lines: list[str], paper_table1: list[dict[str, str]]) -> None:
    lines.extend(
        [
            "## Paper Table 1 Averages",
            "",
            "These are paper aggregate values, not per-benchmark values.",
            "",
            "| method | trials | speedup vs VPR-BB | avg wire length | FIFO reduction vs VPR-BB |",
            "| --- | ---: | ---: | ---: | ---: |",
        ]
    )
    selected = {
        ("vpr_bb", ""),
        ("yoto", "100"),
        ("yoto", "1000"),
        ("yott", "100"),
        ("yott", "1000"),
    }
    for row in paper_table1:
        if (row["method"], row["trials"]) not in selected:
            continue
        lines.append(
            "| "
            + " | ".join(
                [
                    row["method"].upper(),
                    row["trials"],
                    row["speedup_vs_vpr_bb"],
                    row["average_wire_length_segments"],
                    row["fifo_reduction_vs_vpr_bb"],
                ]
            )
            + " |"
        )
    lines.append("")


def aggregate_local_paper_metrics(
    metrics_rows: list[dict[str, str]],
) -> dict[str, dict[str, float | None]]:
    by_mapper: dict[str, list[dict[str, str]]] = defaultdict(list)
    baseline_by_benchmark: dict[str, dict[str, str]] = {}
    for row in metrics_rows:
        if row.get("status") != "success":
            continue
        benchmark = canonical_benchmark(row["benchmark"])
        if row["mapper"] == BASELINE_MAPPER:
            baseline_by_benchmark[benchmark] = row
        by_mapper[row["mapper"]].append(row)

    result: dict[str, dict[str, float | None]] = {}
    for mapper, rows in by_mapper.items():
        total_baseline_time = 0.0
        total_mapper_time = 0.0
        total_baseline_fifo = 0.0
        total_mapper_fifo = 0.0
        wire_values: list[float] = []
        total_cost = 0.0
        total_edges = 0.0
        total_optimal_edges = 0.0
        time_values: list[float] = []
        swap_values: list[float] = []
        for row in rows:
            baseline = baseline_by_benchmark.get(canonical_benchmark(row["benchmark"]))
            baseline_time = metric(baseline, "mapping_time_sec")
            mapper_time = metric(row, "mapping_time_sec")
            baseline_fifo = fifo_sum(baseline)
            mapper_fifo = fifo_sum(row)
            wire = local_wire_length(row)
            if baseline_time is not None and mapper_time is not None:
                total_baseline_time += baseline_time
                total_mapper_time += mapper_time
                time_values.append(mapper_time)
            if baseline_fifo is not None and mapper_fifo is not None:
                total_baseline_fifo += baseline_fifo
                total_mapper_fifo += mapper_fifo
            if wire is not None:
                wire_values.append(wire)
            cost_sum = metric(row, "placement_cost_sum")
            edge_count = metric(row, "placement_edge_count")
            optimal_edge_count = metric(row, "placement_optimal_edge_count")
            if cost_sum is not None and edge_count is not None:
                total_cost += cost_sum
                total_edges += edge_count
            if optimal_edge_count is not None:
                total_optimal_edges += optimal_edge_count
            swaps = metric(row, "placement_swap_attempts")
            if swaps is not None:
                swap_values.append(swaps)
        result[mapper] = {
            "speedup_vs_vpr_bb": safe_ratio(total_baseline_time, total_mapper_time),
            "average_wire_length_segments": (
                sum(wire_values) / len(wire_values) if wire_values else None
            ),
            "edge_weighted_wire_length_segments": safe_ratio(total_cost, total_edges),
            "optimal_mapped_edge_ratio": safe_ratio(total_optimal_edges, total_edges),
            "fifo_reduction_vs_vpr_bb": safe_ratio(
                total_baseline_fifo, total_mapper_fifo
            ),
            "mapping_time_sec": (
                sum(time_values) / len(time_values) if time_values else None
            ),
            "placement_swap_attempts": (
                sum(swap_values) / len(swap_values) if swap_values else None
            ),
        }
    return result


def add_paper_metric_comparison_table(
    lines: list[str],
    paper_table1: list[dict[str, str]],
    local_metrics: dict[str, dict[str, float | None]],
) -> None:
    lines.extend(
        [
            "## Paper-Metric Aggregate Comparison",
            "",
            "Local values use the same columns as YOTT Table 1. Speedup and FIFO reduction are computed against the local VPR-BB row in the same run.",
            "",
            "| paper method | implementation | paper speedup | local speedup | paper avg wire | local avg wire | edge-weighted wire | optimal mapped edges | paper FIFO reduction | local FIFO reduction | local time sec | local swap attempts |",
            "| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for row in paper_table1:
        key = (row["method"], row["trials"])
        for mapper in PAPER_METHOD_TO_MAPPERS.get(key, []):
            local = local_metrics.get(mapper, {})
            lines.append(
                "| "
                + " | ".join(
                    [
                        f"{row['method'].upper()}-{row['trials']}",
                        MAPPER_LABELS[mapper],
                        row["speedup_vs_vpr_bb"],
                        fmt(local.get("speedup_vs_vpr_bb")),
                        row["average_wire_length_segments"],
                        fmt(local.get("average_wire_length_segments")),
                        fmt(local.get("edge_weighted_wire_length_segments")),
                        fmt(local.get("optimal_mapped_edge_ratio")),
                        row["fifo_reduction_vs_vpr_bb"],
                        fmt(local.get("fifo_reduction_vs_vpr_bb")),
                        fmt(local.get("mapping_time_sec"), 6),
                        fmt(local.get("placement_swap_attempts"), 0),
                    ]
                )
                + " |"
            )
    lines.append("")


def add_local_summary_table(lines: list[str], summary: dict[str, dict[str, float]]) -> None:
    lines.extend(
        [
            "## Local Method Averages",
            "",
            "| mapper | time sec | swap attempts | optimal mapped edges | avg mesh hop | avg FIFO | max FIFO | mapped LP |",
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for mapper, label in MAPPER_LABELS.items():
        values = summary.get(mapper, {})
        lines.append(
            "| "
            + " | ".join(
                [
                    label,
                    fmt(values.get("mapping_time_sec"), 6),
                    fmt(values.get("placement_swap_attempts"), 0),
                    fmt(values.get("placement_optimal_edge_ratio")),
                    fmt(values.get("placement_avg_mesh_hop")),
                    fmt(values.get("placement_avg_paper_fifo")),
                    fmt(values.get("placement_max_cost")),
                    fmt(values.get("placement_mapped_lp_mesh_hop")),
                ]
            )
            + " |"
        )
    lines.append("")


def add_benchmark_table(
    lines: list[str],
    paper_by_benchmark: dict[str, dict[str, str]],
    metrics_by_key: dict[tuple[str, str], dict[str, str]],
    trials: str,
) -> None:
    mappers = [
        BASELINE_MAPPER,
        f"faithful_array_yoto_{trials}",
        f"faithful_array_yott_{trials}",
        f"cpu_mapping_yoto_{trials}",
        f"cpu_mapping_yott_{trials}",
    ]
    lines.extend(
        [
            f"## Per-Benchmark {trials}-Trial Comparison",
            "",
            "Paper columns come from YOTT Table 3. Local columns use the YOTT Table 1 metric order: `speedup / avg_wire / FIFO_reduction`.",
            "",
            "| # | benchmark | paper nodes/edges | paper YOTO first/reconv % | paper YOTT first/reconv % | VPR BB | Faithful YOTO | Faithful YOTT | CPU YOTO | CPU YOTT |",
            "| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    paper_rows = sorted(
        paper_by_benchmark.values(), key=lambda row: int(row.get("index", "999"))
    )

    def compact(benchmark: str, mapper: str) -> str:
        row = metrics_by_key.get((canonical_benchmark(benchmark), mapper))
        baseline = metrics_by_key.get((canonical_benchmark(benchmark), BASELINE_MAPPER))
        if row is None:
            return "-"
        metrics = local_paper_metrics(row, baseline)
        return (
            f"{fmt(metrics['speedup_vs_vpr_bb'])}/"
            f"{fmt(metrics['average_wire_length_segments'])}/"
            f"{fmt(metrics['fifo_reduction_vs_vpr_bb'])}"
        )

    for paper in paper_rows:
        bench = paper["benchmark"]
        lines.append(
            "| "
            + " | ".join(
                [
                    paper["index"],
                    bench,
                    f"{paper['nodes']}/{paper['edges']}",
                    f"{paper['yoto_first_percent']}/{paper['yoto_reconvergent_percent']}",
                    f"{paper['yott_first_percent']}/{paper['yott_reconvergent_percent']}",
                    compact(bench, mappers[0]),
                    compact(bench, mappers[1]),
                    compact(bench, mappers[2]),
                    compact(bench, mappers[3]),
                    compact(bench, mappers[4]),
                ]
            )
            + " |"
        )
    lines.append("")


def add_benchmark_winner_table(
    lines: list[str], metrics_rows: list[dict[str, str]]
) -> None:
    lines.extend(
        [
            "## Per-Benchmark Winners",
            "",
            "| benchmark | best optimal mapped edges | best FIFO | fastest |",
            "| --- | --- | --- | --- |",
        ]
    )
    by_benchmark: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in metrics_rows:
        if row.get("status") == "success":
            by_benchmark[canonical_benchmark(row["benchmark"])].append(row)

    def best_label(rows: list[dict[str, str]], key: str, reverse: bool) -> str:
        best_row = None
        best_value = None
        for row in rows:
            value = as_float(row.get(key))
            if value is None:
                continue
            if best_value is None:
                best_value = value
                best_row = row
                continue
            if (reverse and value > best_value) or ((not reverse) and value < best_value):
                best_value = value
                best_row = row
        if best_row is None:
            return "-"
        return f"{MAPPER_LABELS[best_row['mapper']]} ({fmt(best_value, 4)})"

    for benchmark in sorted(by_benchmark):
        rows = by_benchmark[benchmark]
        display_name = rows[0]["benchmark"]
        lines.append(
            "| "
            + " | ".join(
                [
                    display_name,
                    best_label(rows, "placement_optimal_edge_ratio", True),
                    best_label(rows, "placement_max_cost", False),
                    best_label(rows, "mapping_time_sec", False),
                ]
            )
            + " |"
        )
    lines.append("")


def write_markdown(
    metrics_rows: list[dict[str, str]],
    paper_table1: list[dict[str, str]],
    paper_by_benchmark: dict[str, dict[str, str]],
    output_md: Path,
    output_csv: Path,
    paper_metric_csv: Path,
) -> None:
    lines = [
        "# YOTT Per-Benchmark Local Comparison",
        "",
        f"Source metrics: `{output_csv.parent.parent / 'metrics.csv'}`",
        f"Full long-form table: `{output_csv}`",
        f"Paper-metric table: `{paper_metric_csv}`",
        "",
        "Per-benchmark local metric format: `speedup_vs_VPR-BB / average_wire_length / FIFO_reduction_vs_VPR-BB`.",
        "YOTT paper per-benchmark quality values are plotted in figures; the numeric per-benchmark paper columns here are Table 3 benchmark and edge-category fields.",
        "",
    ]
    add_paper_average_table(lines, paper_table1)
    add_paper_metric_comparison_table(
        lines, paper_table1, aggregate_local_paper_metrics(metrics_rows)
    )
    add_local_summary_table(lines, method_summary(metrics_rows))
    metrics_by_key = row_map(metrics_rows)
    add_benchmark_table(lines, paper_by_benchmark, metrics_by_key, "100")
    add_benchmark_table(lines, paper_by_benchmark, metrics_by_key, "1000")
    add_benchmark_winner_table(lines, metrics_rows)
    output_md.write_text("\n".join(lines) + "\n")


def main() -> None:
    args = parse_args()
    run_dir = resolve_run_dir(args.run_dir)
    output_dir = Path(args.output_dir) if args.output_dir else run_dir / "figures"
    output_dir.mkdir(parents=True, exist_ok=True)

    metrics_rows = load_metrics(run_dir / "metrics.csv")
    paper_dir = Path(args.paper_dir)
    paper_table1 = read_csv(paper_dir / "yott_table1.csv")
    paper_by_benchmark = load_paper_table3(paper_dir / "yott_table3_benchmarks.csv")

    output_csv = output_dir / "yott_benchmark_method_table.csv"
    paper_metric_csv = output_dir / "yott_paper_metric_by_benchmark.csv"
    output_md = output_dir / "yott_benchmark_method_table.md"
    metrics_by_key = row_map(metrics_rows)
    write_long_csv(metrics_rows, paper_by_benchmark, output_csv)
    write_paper_metric_csv(paper_by_benchmark, metrics_by_key, paper_metric_csv)
    write_markdown(
        metrics_rows,
        paper_table1,
        paper_by_benchmark,
        output_md,
        output_csv,
        paper_metric_csv,
    )
    print(output_md)
    print(output_csv)
    print(paper_metric_csv)


if __name__ == "__main__":
    main()
