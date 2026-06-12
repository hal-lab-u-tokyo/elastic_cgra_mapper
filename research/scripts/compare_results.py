#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path

SUCCESS_STATUSES = {"success", "optimal", "timeout_feasible"}


def mean(rows: list, key: str) -> float:
    values = [float(row[key]) for row in rows if row.get(key) not in {"", None}]
    return sum(values) / len(values) if values else 0


def mean_prefer(rows: list, primary_key: str, fallback_key: str) -> float:
    values = []
    for row in rows:
        value = row.get(primary_key)
        if value in {"", None}:
            value = row.get(fallback_key)
        if value not in {"", None}:
            values.append(float(value))
    return sum(values) / len(values) if values else 0


def mean_prefer3(rows: list, primary_key: str, secondary_key: str, fallback_key: str) -> float:
    values = []
    for row in rows:
        value = row.get(primary_key)
        if value in {"", None}:
            value = row.get(secondary_key)
        if value in {"", None}:
            value = row.get(fallback_key)
        if value not in {"", None}:
            values.append(float(value))
    return sum(values) / len(values) if values else 0


def mean_values(values: list) -> float:
    return sum(values) / len(values) if values else 0.0


def case_key(row: dict) -> tuple:
    return (
        row.get("benchmark_set", "") or "default",
        row.get("benchmark", ""),
        row.get("arch_name", ""),
        row.get("mapper", ""),
    )


def row_time(row: dict, *keys: str) -> float:
    for key in keys:
        if row.get(key) not in {"", None}:
            return float(row[key])
    return 0.0


def best_success(rows: list):
    success_rows = [row for row in rows if row.get("status") in SUCCESS_STATUSES]
    if not success_rows:
        return None
    return min(
        success_rows,
        key=lambda row: (
            float(row.get("achieved_II") or 1e18),
            float(row.get("mapping_time_sec") or 1e18),
        ),
    )


def summarize_cases(rows: list) -> tuple:
    by_case = defaultdict(list)
    for row in rows:
        by_case[case_key(row)].append(row)
    best_rows = []
    case_mapping_times = []
    case_wall_times = []
    for case_rows in by_case.values():
        case_mapping_times.append(
            sum(row_time(row, "mapping_time_sec", "wall_time_sec") for row in case_rows)
        )
        case_wall_times.append(
            sum(row_time(row, "wall_time_sec", "mapping_time_sec") for row in case_rows)
        )
        best = best_success(case_rows)
        if best:
            best_rows.append(best)
    return by_case, best_rows, case_mapping_times, case_wall_times


def main() -> None:
    parser = argparse.ArgumentParser(description="Create a small Markdown summary from metrics.csv.")
    parser.add_argument("--metrics", required=True, type=Path)
    parser.add_argument("--group-by", default="mapper")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    groups = defaultdict(list)
    with args.metrics.open() as f:
        for row in csv.DictReader(f):
            groups[row.get(args.group_by, "")].append(row)

    lines = [f"# Comparison by `{args.group_by}`", ""]
    lines.append(
        "| group | cases | solved | attempts | achieved II mean | mapping time mean | wall time mean | optimal edge mean | placement cost mean | fifo-like mean | fifo-like max | compute PE util mean | context util mean | route/compute mean | avg hop mean | bbox util mean |"
    )
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for group, rows in sorted(groups.items()):
        cases, best_rows, case_mapping_times, case_wall_times = summarize_cases(rows)
        lines.append(
            f"| {group} | {len(cases)} | {len(best_rows)} | {len(rows)} | "
            f"{mean(best_rows, 'achieved_II'):.3f} | "
            f"{mean_values(case_mapping_times):.6f} | "
            f"{mean_values(case_wall_times):.3f} | "
            f"{mean_prefer3(best_rows, 'placement_optimal_edge_ratio', 'placement_direct_edge_ratio', 'direct_dfg_edge_ratio'):.3f} | "
            f"{mean_prefer(best_rows, 'placement_avg_cost', 'placement_avg_wirelength'):.3f} | "
            f"{mean_prefer(best_rows, 'placement_avg_fifo_like', 'placement_avg_fifo'):.3f} | "
            f"{mean_prefer(best_rows, 'placement_max_fifo_like', 'placement_max_fifo'):.3f} | "
            f"{mean(best_rows, 'compute_pe_utilization'):.3f} | "
            f"{mean(best_rows, 'pe_context_utilization'):.3f} | "
            f"{mean(best_rows, 'route_to_compute_ratio'):.3f} | "
            f"{mean(best_rows, 'avg_manhattan_distance'):.3f} | "
            f"{mean(best_rows, 'compute_bbox_utilization'):.3f} |"
        )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
