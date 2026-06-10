#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def mean(rows: list, key: str) -> float:
    values = [float(row[key]) for row in rows if row.get(key) not in {"", None}]
    return sum(values) / len(values) if values else 0


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
        "| group | trials | success | achieved II mean | mapping time mean | compute PE util mean | context util mean | route/compute mean | avg hop mean | bbox util mean |"
    )
    lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |")
    for group, rows in sorted(groups.items()):
        success_rows = [r for r in rows if r["status"] in {"success", "optimal", "timeout_feasible"}]
        lines.append(
            f"| {group} | {len(rows)} | {len(success_rows)} | "
            f"{mean(success_rows, 'achieved_II'):.3f} | "
            f"{mean(rows, 'mapping_time_sec'):.3f} | "
            f"{mean(success_rows, 'compute_pe_utilization'):.3f} | "
            f"{mean(success_rows, 'pe_context_utilization'):.3f} | "
            f"{mean(success_rows, 'route_to_compute_ratio'):.3f} | "
            f"{mean(success_rows, 'avg_manhattan_distance'):.3f} | "
            f"{mean(success_rows, 'compute_bbox_utilization'):.3f} |"
        )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
