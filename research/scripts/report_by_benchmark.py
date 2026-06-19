#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path


SUCCESS_STATUSES = {"success", "optimal", "timeout_feasible"}


def mean(rows: list, key: str) -> float:
    values = [float(row[key]) for row in rows if row.get(key) not in {"", None}]
    return sum(values) / len(values) if values else 0.0


def mean_prefer(rows: list, primary_key: str, fallback_key: str) -> float:
    values = []
    for row in rows:
        value = row.get(primary_key)
        if value in {"", None}:
            value = row.get(fallback_key)
        if value not in {"", None}:
            values.append(float(value))
    return sum(values) / len(values) if values else 0.0


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
    return sum(values) / len(values) if values else 0.0


def value_prefer(row: dict, primary_key: str, fallback_key: str) -> str:
    value = row.get(primary_key, "")
    return value if value not in {"", None} else row.get(fallback_key, "")


def value_prefer3(row: dict, primary_key: str, secondary_key: str, fallback_key: str) -> str:
    value = row.get(primary_key, "")
    if value in {"", None}:
        value = row.get(secondary_key, "")
    if value in {"", None}:
        value = row.get(fallback_key, "")
    return value


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
    return min(success_rows, key=lambda row: (float(row.get("achieved_II") or 1e18), float(row.get("mapping_time_sec") or 1e18)))


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


def add_group_table(lines: list, title: str, groups: dict) -> None:
    lines.extend([f"## {title}", ""])
    lines.append(
        "| group | cases | solved | attempts | achieved II mean | mapping time mean | wall time mean | optimal distance | mesh optimal | placement cost | mesh hop | FIFO avg | max FIFO | mapped LP | routed FIFO | routed max FIFO | routed LP | compute PE util | route/compute | avg hop | bbox util |"
    )
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for group, rows in sorted(groups.items()):
        cases, best_rows, case_mapping_times, case_wall_times = summarize_cases(rows)
        lines.append(
            f"| {group} | {len(cases)} | {len(best_rows)} | {len(rows)} | "
            f"{mean(best_rows, 'achieved_II'):.3f} | "
            f"{mean_values(case_mapping_times):.6f} | "
            f"{mean_values(case_wall_times):.3f} | "
            f"{mean_prefer3(best_rows, 'placement_optimal_distance_ratio', 'placement_optimal_edge_ratio', 'placement_direct_edge_ratio'):.3f} | "
            f"{mean(best_rows, 'placement_mesh_optimal_edge_ratio'):.3f} | "
            f"{mean_prefer(best_rows, 'placement_avg_cost', 'placement_avg_wirelength'):.3f} | "
            f"{mean(best_rows, 'placement_avg_mesh_hop'):.3f} | "
            f"{mean_prefer(best_rows, 'placement_avg_fifo', 'placement_avg_fifo_like'):.3f} | "
            f"{mean_prefer(best_rows, 'placement_max_fifo', 'placement_max_mesh_fifo'):.3f} | "
            f"{mean(best_rows, 'placement_mapped_lp_mesh_hop'):.3f} | "
            f"{mean(best_rows, 'routed_avg_fifo'):.3f} | "
            f"{mean(best_rows, 'routed_max_fifo'):.3f} | "
            f"{mean(best_rows, 'routed_mapped_lp'):.3f} | "
            f"{mean(best_rows, 'compute_pe_utilization'):.3f} | "
            f"{mean(best_rows, 'route_to_compute_ratio'):.3f} | "
            f"{mean(best_rows, 'avg_manhattan_distance'):.3f} | "
            f"{mean(best_rows, 'compute_bbox_utilization'):.3f} |"
        )
    lines.append("")


def main() -> None:
    parser = argparse.ArgumentParser(description="Create a detailed benchmark-level Markdown report.")
    parser.add_argument("--metrics", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    with args.metrics.open() as f:
        rows = list(csv.DictReader(f))

    by_set = defaultdict(list)
    by_benchmark = defaultdict(list)
    by_mapper = defaultdict(list)
    by_arch = defaultdict(list)
    for row in rows:
        by_set[row.get("benchmark_set", "") or "default"].append(row)
        by_benchmark[row.get("benchmark", "")].append(row)
        by_mapper[row.get("mapper", "")].append(row)
        by_arch[row.get("arch_name", "")].append(row)

    lines = ["# Benchmark Report", ""]
    add_group_table(lines, "By Benchmark Set", by_set)
    add_group_table(lines, "By Mapper", by_mapper)
    add_group_table(lines, "By Architecture", by_arch)

    lines.extend(["## By Benchmark", ""])
    for benchmark, benchmark_rows in sorted(by_benchmark.items()):
        lines.extend([f"### {benchmark}", ""])
        lines.append(
            "| set | mapper | arch | status | MII | achieved II | mapping time | wall time | optimal distance | mesh optimal | placement cost | mesh hop | FIFO avg | max FIFO | mapped LP | routed FIFO | routed max FIFO | routed LP | compute PE util | context util | route/compute | avg hop | bbox util |"
        )
        lines.append("| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
        for row in sorted(benchmark_rows, key=lambda r: (r.get("mapper", ""), r.get("arch_name", ""), r.get("achieved_II", ""))):
            lines.append(
                f"| {row.get('benchmark_set', '') or 'default'} | {row.get('mapper', '')} | {row.get('arch_name', '')} | {row.get('status', '')} | "
                f"{row.get('MII', '')} | {row.get('achieved_II', '')} | {row.get('mapping_time_sec', '')} | {row.get('wall_time_sec', '')} | "
                f"{value_prefer3(row, 'placement_optimal_distance_ratio', 'placement_optimal_edge_ratio', 'placement_direct_edge_ratio')} | "
                f"{row.get('placement_mesh_optimal_edge_ratio', '')} | "
                f"{value_prefer(row, 'placement_avg_cost', 'placement_avg_wirelength')} | "
                f"{row.get('placement_avg_mesh_hop', '')} | "
                f"{value_prefer(row, 'placement_avg_fifo', 'placement_avg_fifo_like')} | {value_prefer(row, 'placement_max_fifo', 'placement_max_mesh_fifo')} | "
                f"{row.get('placement_mapped_lp_mesh_hop', '')} | "
                f"{row.get('routed_avg_fifo', '')} | {row.get('routed_max_fifo', '')} | "
                f"{row.get('routed_mapped_lp', '')} | "
                f"{row.get('compute_pe_utilization', '')} | {row.get('pe_context_utilization', '')} | "
                f"{row.get('route_to_compute_ratio', '')} | {row.get('avg_manhattan_distance', '')} | "
                f"{row.get('compute_bbox_utilization', '')} |"
            )
        best = best_success(benchmark_rows)
        if best:
            lines.extend(
                [
                    "",
                    f"Best successful result by II/time: `{best.get('mapper', '')}` on `{best.get('arch_name', '')}` with achieved II `{best.get('achieved_II', '')}`.",
                ]
            )
        lines.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
