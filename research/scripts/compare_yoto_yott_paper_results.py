#!/usr/bin/env python3
"""Generate a careful YOTO/YOTT paper-comparison audit report.

The report intentionally avoids digitizing plot-only paper figures. Numeric
paper tables are read from research/data/papers/*.csv; plot-only figures are
checked against textual claims from the paper and local counterpart plots.
"""

from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path


DEFAULT_RUN_DIR = (
    "research/results/placement2d/paper_comparison/traversal_yott/"
    "20260622-034821_cpu_mapping_order_degree_compare"
)
DEFAULT_ARCH = "one_hop_perimeter_no_corners_io"

LOCAL_MAPPERS = {
    "faithful_array_yoto_100": "Faithful Array YOTO-100",
    "faithful_array_yoto_1000": "Faithful Array YOTO-1000",
    "faithful_array_yott_100": "Faithful Array YOTT-100",
    "faithful_array_yott_1000": "Faithful Array YOTT-1000",
    "cpu_mapping_yoto_100": "CPU Mapping YOTO-100",
    "cpu_mapping_yoto_1000": "CPU Mapping YOTO-1000",
    "cpu_mapping_yott_100": "CPU Mapping YOTT-100",
    "cpu_mapping_yott_1000": "CPU Mapping YOTT-1000",
}

PAPER_METHOD_TO_LOCAL = {
    ("yoto", "100"): ["faithful_array_yoto_100", "cpu_mapping_yoto_100"],
    ("yoto", "1000"): ["faithful_array_yoto_1000", "cpu_mapping_yoto_1000"],
    ("yott", "100"): ["faithful_array_yott_100", "cpu_mapping_yott_100"],
    ("yott", "1000"): ["faithful_array_yott_1000", "cpu_mapping_yott_1000"],
}

YOTT_TEXTUAL_CLAIMS = [
    (
        "Fig. 18/19 optimal edges",
        "YOTT-100 optimally maps 90.5% of edges on average.",
    ),
    (
        "Fig. 19 optimal edges",
        "YOTT-100 outperforms YOTO-100 for all 23 benchmarks.",
    ),
    (
        "Fig. 19 optimal edges",
        "YOTT-100 outperforms YOTO-1000 in 21 of 23 benchmarks.",
    ),
    (
        "Fig. 21/22 FIFO",
        "YOTT-100 and SA-100 reach the best FIFO results among the compared placements.",
    ),
    (
        "Fig. 22 FIFO",
        "YOTT-100 outperforms YOTO-1000 for max FIFO.",
    ),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", default=DEFAULT_RUN_DIR)
    parser.add_argument("--paper-dir", default="research/data/papers")
    parser.add_argument("--arch", default=DEFAULT_ARCH)
    parser.add_argument(
        "--output-md",
        default=None,
        help="default: <run-dir>/figures/yoto_yott_paper_audit.md",
    )
    return parser.parse_args()


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


def mean(values: list[float]) -> float:
    return sum(values) / len(values)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def load_metrics(metrics_csv: Path, arch: str) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for row in read_csv(metrics_csv):
        if row.get("arch_name") != arch:
            continue
        if row.get("mapper") not in LOCAL_MAPPERS:
            continue
        rows.append(row)
    return rows


def aggregate_by_mapper(rows: list[dict[str, str]]) -> dict[str, dict[str, float]]:
    metrics = [
        "mapping_time_sec",
        "placement_avg_mesh_hop",
        "placement_avg_fifo",
        "placement_max_fifo",
        "placement_p95_fifo",
        "placement_optimal_distance_ratio",
        "placement_mapped_lp_mesh_hop",
        "placement_criticality_weighted_mesh_hop",
        "placement_max_cut_congestion",
        "placement_estimated_max_link_demand",
    ]
    buckets: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    success_count: dict[str, int] = defaultdict(int)
    attempt_count: dict[str, int] = defaultdict(int)

    for row in rows:
        mapper = row["mapper"]
        attempt_count[mapper] += 1
        if row.get("status") == "success":
            success_count[mapper] += 1
        for metric in metrics:
            value = as_float(row.get(metric))
            if value is not None:
                buckets[mapper][metric].append(value)

    result: dict[str, dict[str, float]] = {}
    for mapper, metric_values in buckets.items():
        result[mapper] = {
            metric: mean(values) for metric, values in metric_values.items() if values
        }
        result[mapper]["attempts"] = float(attempt_count[mapper])
        result[mapper]["successes"] = float(success_count[mapper])
    return result


def format_float(value: float | None, digits: int = 3) -> str:
    if value is None:
        return "-"
    return f"{value:.{digits}f}"


def mapper_mean(
    aggregates: dict[str, dict[str, float]], mapper: str, metric: str
) -> float | None:
    return aggregates.get(mapper, {}).get(metric)


def make_table1_section(
    paper_rows: list[dict[str, str]], aggregates: dict[str, dict[str, float]]
) -> list[str]:
    lines = [
        "## YOTT Table 1: Aggregate Speed, Wire Length, FIFO",
        "",
        "The paper table is numeric, so its values are stored in `research/data/papers/yott_table1.csv`.",
        "Local speedup and FIFO-reduction ratios are not paper-equivalent unless a matching local VPR BB baseline is present in the same run. Therefore this table compares the directly corresponding local metric only: average mesh hop as average wire length.",
        "",
        "| paper method | paper wire length | local mapper | local mesh hop | local - paper | local time sec | local avg FIFO | local max FIFO |",
        "| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for row in paper_rows:
        key = (row["method"], row["trials"])
        local_names = PAPER_METHOD_TO_LOCAL.get(key, [])
        if not local_names:
            continue
        paper_wire = as_float(row["average_wire_length_segments"])
        for mapper in local_names:
            local_wire = mapper_mean(aggregates, mapper, "placement_avg_mesh_hop")
            delta = None if paper_wire is None or local_wire is None else local_wire - paper_wire
            lines.append(
                "| "
                + " | ".join(
                    [
                        f"{row['method'].upper()}-{row['trials']}",
                        format_float(paper_wire, 2),
                        LOCAL_MAPPERS[mapper],
                        format_float(local_wire, 3),
                        format_float(delta, 3),
                        format_float(mapper_mean(aggregates, mapper, "mapping_time_sec"), 6),
                        format_float(mapper_mean(aggregates, mapper, "placement_avg_fifo"), 3),
                        format_float(mapper_mean(aggregates, mapper, "placement_max_fifo"), 3),
                    ]
                )
                + " |"
            )
    lines.append("")
    return lines


def paper_benchmark_names(paper_rows: list[dict[str, str]]) -> set[str]:
    names: set[str] = set()
    for row in paper_rows:
        name = row.get("benchmark", "")
        if name:
            names.add(name)
    return names


def local_benchmark_names(rows: list[dict[str, str]]) -> set[str]:
    return {row["benchmark"] for row in rows if row.get("benchmark")}


def make_benchmark_coverage_section(
    paper_rows: list[dict[str, str]], metric_rows: list[dict[str, str]]
) -> list[str]:
    paper_names = paper_benchmark_names(paper_rows)
    local_names = local_benchmark_names(metric_rows)
    alias = {
        "filterrgb": "filter_rgb",
        "h2v2_smo": "h2v2_smooth",
        "cplx8": "cplx8",
        "fir16": "fir16",
    }

    def canonical(name: str) -> str:
        lower = name.lower()
        return alias.get(lower, lower)

    normalized_paper = {canonical(name) for name in paper_names}
    normalized_local = {canonical(name) for name in local_names}
    matched = sorted(normalized_paper & normalized_local)
    missing_local = sorted(normalized_paper - normalized_local)
    extra_local = sorted(normalized_local - normalized_paper)
    note = (
        "The benchmark set matches by name after spelling aliases. Exact node/edge alignment is checked separately by `benchmark/literature/yott_cases2021_normalized/table3_alignment.md`."
        if not missing_local and not extra_local
        else "The benchmark mismatch must be reported whenever local averages are compared to paper averages."
    )

    return [
        "## YOTT Table 3: Benchmark Coverage",
        "",
        "The paper's benchmark statistics are stored in `research/data/papers/yott_table3_benchmarks.csv`.",
        f"- Paper benchmarks with names: {len(paper_names)}",
        f"- Local benchmarks in this run: {len(local_names)}",
        f"- Name matches after small spelling aliases: {len(matched)}",
        f"- Paper benchmarks not present locally: {', '.join(missing_local) if missing_local else '-'}",
        f"- Local benchmarks not in the YOTT Table 3 list: {', '.join(extra_local) if extra_local else '-'}",
        "",
        note,
        "",
    ]


def success_rows_by_mapper(rows: list[dict[str, str]], mapper: str) -> dict[str, dict[str, str]]:
    selected: dict[str, dict[str, str]] = {}
    for row in rows:
        if row.get("mapper") == mapper and row.get("status") == "success":
            selected[row["benchmark"]] = row
    return selected


def compare_pair(
    rows: list[dict[str, str]],
    left_mapper: str,
    right_mapper: str,
    metric: str,
    higher_is_better: bool,
) -> tuple[int, int, int]:
    left = success_rows_by_mapper(rows, left_mapper)
    right = success_rows_by_mapper(rows, right_mapper)
    common = sorted(set(left) & set(right))
    wins = 0
    ties = 0
    for benchmark in common:
        left_value = as_float(left[benchmark].get(metric))
        right_value = as_float(right[benchmark].get(metric))
        if left_value is None or right_value is None:
            continue
        if math.isclose(left_value, right_value, rel_tol=1e-9, abs_tol=1e-9):
            ties += 1
        elif higher_is_better and left_value > right_value:
            wins += 1
        elif (not higher_is_better) and left_value < right_value:
            wins += 1
    return wins, ties, len(common)


def make_plot_claim_section(rows: list[dict[str, str]]) -> list[str]:
    checks = [
        (
            "Faithful Array: YOTT-100 vs YOTO-100, optimal edge ratio",
            "faithful_array_yott_100",
            "faithful_array_yoto_100",
            "placement_optimal_distance_ratio",
            True,
        ),
        (
            "Faithful Array: YOTT-100 vs YOTO-1000, optimal edge ratio",
            "faithful_array_yott_100",
            "faithful_array_yoto_1000",
            "placement_optimal_distance_ratio",
            True,
        ),
        (
            "CPU Mapping: YOTT-100 vs YOTO-1000, optimal edge ratio",
            "cpu_mapping_yott_100",
            "cpu_mapping_yoto_1000",
            "placement_optimal_distance_ratio",
            True,
        ),
        (
            "Faithful Array: YOTT-100 vs YOTO-100, max FIFO",
            "faithful_array_yott_100",
            "faithful_array_yoto_100",
            "placement_max_fifo",
            False,
        ),
        (
            "Faithful Array: YOTT-100 vs YOTO-1000, max FIFO",
            "faithful_array_yott_100",
            "faithful_array_yoto_1000",
            "placement_max_fifo",
            False,
        ),
        (
            "CPU Mapping: YOTT-100 vs YOTO-1000, max FIFO",
            "cpu_mapping_yott_100",
            "cpu_mapping_yoto_1000",
            "placement_max_fifo",
            False,
        ),
    ]
    lines = [
        "## Plot-Only Figure Claims",
        "",
        "Paper figure bars are not digitized. The checks below compare local runs against textual claims from the YOTT paper.",
        "",
        "Paper textual claims tracked:",
    ]
    for location, claim in YOTT_TEXTUAL_CLAIMS:
        lines.append(f"- {location}: {claim}")
    lines.extend(
        [
            "",
            "Local pairwise checks:",
            "",
            "| check | local wins | ties | common benchmarks |",
            "| --- | ---: | ---: | ---: |",
        ]
    )
    for label, left, right, metric, higher in checks:
        wins, ties, common = compare_pair(rows, left, right, metric, higher)
        lines.append(f"| {label} | {wins} | {ties} | {common} |")
    lines.extend(
        [
            "",
            "Generated local counterpart plots:",
            "",
            "- `figures/yott_fig19_22_local.{pdf,png,svg}`",
            "- `figures/yott_fig19_22_local_values.csv`",
            "",
        ]
    )
    return lines


def make_missing_section() -> list[str]:
    return [
        "## Remaining Gaps",
        "",
        "| paper artifact | current status | required for exact paper match |",
        "| --- | --- | --- |",
        "| YOTT Table 2 | paper values stored; local category metrics not exported | edge category export for `first`, `reconvergent`, and `io` |",
        "| YOTT Table 3 | original 23 DOTs are normalized and node/edge counts match; paper IN/OUT ports are not equivalent to mapper `load/output` counts | edge-category export and an explicit paper-port model if IN/OUT must be compared |",
        "| YOTT Fig. 20(b) | local runtime exists; node/cell visits not exported | export node/cell visit or swaps-per-node counters |",
        "| YOTT Fig. 23 | no clean RA-off paired config in current run | mapper/config switch that disables reconvergent annotation only |",
        "| YOTT Table 4 | paper values stored; local multi-kernel DOTs not present | generator for cplx8/fir16 combined kernels |",
        "| TRAVERSAL Table I | not covered by placement2d run | modulo ADRES 4x4 II=2 paper-comparison manifest |",
        "| TRAVERSAL Fig. 14/15/16 | plot-only; no digitization | local counterpart plots with closest strategy set |",
        "",
    ]


def write_report(
    output_md: Path,
    paper_dir: Path,
    run_dir: Path,
    arch: str,
    metric_rows: list[dict[str, str]],
    aggregates: dict[str, dict[str, float]],
) -> None:
    paper_table1 = read_csv(paper_dir / "yott_table1.csv")
    paper_table3 = read_csv(paper_dir / "yott_table3_benchmarks.csv")

    lines = [
        "# YOTO/YOTT Paper Comparison Audit",
        "",
        f"- Run directory: `{run_dir}`",
        f"- Metrics: `{run_dir / 'metrics.csv'}`",
        f"- Architecture filter: `{arch}`",
        f"- Paper reference data: `{paper_dir}`",
        "",
        "This report compares only values that are directly supported by local metrics. Plot-only paper figures are not digitized.",
        "",
    ]
    lines.extend(make_table1_section(paper_table1, aggregates))
    lines.extend(make_benchmark_coverage_section(paper_table3, metric_rows))
    lines.extend(make_plot_claim_section(metric_rows))
    lines.extend(make_missing_section())
    output_md.parent.mkdir(parents=True, exist_ok=True)
    output_md.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    args = parse_args()
    run_dir = Path(args.run_dir)
    paper_dir = Path(args.paper_dir)
    output_md = (
        Path(args.output_md)
        if args.output_md
        else run_dir / "figures" / "yoto_yott_paper_audit.md"
    )
    metric_rows = load_metrics(run_dir / "metrics.csv", args.arch)
    aggregates = aggregate_by_mapper(metric_rows)
    write_report(output_md, paper_dir, run_dir, args.arch, metric_rows, aggregates)
    print(output_md)


if __name__ == "__main__":
    main()
