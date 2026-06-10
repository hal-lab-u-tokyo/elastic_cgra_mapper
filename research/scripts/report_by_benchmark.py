#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path


SUCCESS_STATUSES = {"success", "optimal", "timeout_feasible"}


def mean(rows: list, key: str) -> float:
    values = [float(row[key]) for row in rows if row.get(key) not in {"", None}]
    return sum(values) / len(values) if values else 0.0


def best_success(rows: list):
    success_rows = [row for row in rows if row.get("status") in SUCCESS_STATUSES]
    if not success_rows:
        return None
    return min(success_rows, key=lambda row: (float(row.get("achieved_II") or 1e18), float(row.get("mapping_time_sec") or 1e18)))


def add_group_table(lines: list, title: str, groups: dict) -> None:
    lines.extend([f"## {title}", ""])
    lines.append(
        "| group | trials | success | achieved II mean | time mean | compute PE util | route/compute | avg hop | bbox util |"
    )
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for group, rows in sorted(groups.items()):
        success_rows = [row for row in rows if row.get("status") in SUCCESS_STATUSES]
        lines.append(
            f"| {group} | {len(rows)} | {len(success_rows)} | "
            f"{mean(success_rows, 'achieved_II'):.3f} | "
            f"{mean(rows, 'mapping_time_sec'):.3f} | "
            f"{mean(success_rows, 'compute_pe_utilization'):.3f} | "
            f"{mean(success_rows, 'route_to_compute_ratio'):.3f} | "
            f"{mean(success_rows, 'avg_manhattan_distance'):.3f} | "
            f"{mean(success_rows, 'compute_bbox_utilization'):.3f} |"
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
            "| set | mapper | arch | status | MII | achieved II | time | compute PE util | context util | route/compute | avg hop | bbox util |"
        )
        lines.append("| --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
        for row in sorted(benchmark_rows, key=lambda r: (r.get("mapper", ""), r.get("arch_name", ""), r.get("achieved_II", ""))):
            lines.append(
                f"| {row.get('benchmark_set', '') or 'default'} | {row.get('mapper', '')} | {row.get('arch_name', '')} | {row.get('status', '')} | "
                f"{row.get('MII', '')} | {row.get('achieved_II', '')} | {row.get('mapping_time_sec', '')} | "
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
