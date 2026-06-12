#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path


SUCCESS_STATUSES = {"success", "optimal", "timeout_feasible"}
RATIO_FIELDS = [
    "II_ratio",
    "pe_context_utilization",
    "compute_context_utilization",
    "route_context_utilization",
    "memory_context_utilization",
    "pe_active_ratio",
    "compute_pe_utilization",
    "route_pe_ratio",
    "memory_pe_ratio",
    "context_active_ratio",
    "compute_bbox_utilization",
    "placement_direct_edge_ratio",
    "placement_optimal_edge_ratio",
]


def as_float(row: dict, key: str):
    value = row.get(key, "")
    if value in {"", None}:
        return None
    return float(value)


def check(condition: bool, failures: list, message: str) -> None:
    if not condition:
        failures.append(message)


def validate_row(row: dict, row_id: int) -> tuple:
    failures = []
    warnings = []
    prefix = f"row {row_id} ({row.get('benchmark', '')}/{row.get('mapper', '')})"

    rows = as_float(row, "rows")
    cols = as_float(row, "cols")
    total_pes = rows * cols if rows is not None and cols is not None else None
    total_contexts = as_float(row, "total_pe_contexts")
    context_size = as_float(row, "context_size")
    used_ops = as_float(row, "used_ops")
    compute_ops = as_float(row, "compute_ops")
    route_ops = as_float(row, "route_ops")
    nop_ops = as_float(row, "nop_ops")
    achieved_ii = as_float(row, "achieved_II")
    start_ii = as_float(row, "start_II")
    mii = as_float(row, "MII")

    if total_pes is not None and context_size is not None and total_contexts is not None:
        check(
            total_contexts == total_pes * context_size,
            failures,
            f"{prefix}: total_pe_contexts does not equal rows * cols * context_size",
        )
    if used_ops is not None and nop_ops is not None and total_contexts is not None:
        check(
            used_ops + nop_ops == total_contexts,
            failures,
            f"{prefix}: used_ops + nop_ops does not equal total_pe_contexts",
        )
    if used_ops is not None and compute_ops is not None and route_ops is not None:
        check(
            used_ops == compute_ops + route_ops,
            failures,
            f"{prefix}: used_ops does not equal compute_ops + route_ops",
        )
    if row.get("status") in SUCCESS_STATUSES:
        check(achieved_ii is not None, failures, f"{prefix}: successful row has no achieved_II")
        if mii is not None and achieved_ii is not None:
            check(achieved_ii >= mii, failures, f"{prefix}: achieved_II is smaller than MII")
        if start_ii is not None and achieved_ii is not None:
            check(achieved_ii >= start_ii, failures, f"{prefix}: achieved_II is smaller than start_II")
        if row.get("mapping_file"):
            check(Path(row["mapping_file"]).exists(), failures, f"{prefix}: mapping_file does not exist")

    for field in RATIO_FIELDS:
        value = as_float(row, field)
        if value is None:
            continue
        check(0.0 <= value <= 1.0, failures, f"{prefix}: {field} is outside [0, 1]")

    for field in [
        "active_pe_count",
        "active_compute_pe_count",
        "active_route_pe_count",
        "active_memory_pe_count",
    ]:
        value = as_float(row, field)
        if value is not None and total_pes is not None:
            check(value <= total_pes, failures, f"{prefix}: {field} exceeds total PEs")

    if mii is not None and start_ii is not None and start_ii > mii:
        warnings.append(f"{prefix}: start_II is larger than MII; this is fine for smoke tests but not for achieved-II evaluation")
    if row.get("status") == "timeout_feasible":
        warnings.append(f"{prefix}: feasible incumbent accepted after timeout; compare quality separately from proven optimality")
    if (
        row.get("status") in SUCCESS_STATUSES
        and row.get("evaluation_mode") != "placement_only"
        and route_ops == 0
    ):
        warnings.append(f"{prefix}: route_ops is zero; this benchmark may be too easy to stress routing")

    return failures, warnings


def add_table(lines: list, title: str, items: list) -> None:
    lines.extend([f"## {title}", ""])
    if not items:
        lines.extend(["None.", ""])
        return
    for item in items:
        lines.append(f"- {item}")
    lines.append("")


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate research metrics for internal consistency.")
    parser.add_argument("--metrics", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    with args.metrics.open() as f:
        rows = list(csv.DictReader(f))

    failures = []
    warnings = []
    benchmark_sets = set()
    benchmarks = set()
    mappers = set()
    archs = set()
    for idx, row in enumerate(rows, start=1):
        benchmark_sets.add(row.get("benchmark_set", "") or "default")
        benchmarks.add(row.get("benchmark", ""))
        mappers.add(row.get("mapper", ""))
        archs.add(row.get("arch_name", ""))
        row_failures, row_warnings = validate_row(row, idx)
        failures.extend(row_failures)
        warnings.extend(row_warnings)

    lines = ["# Metrics Validation", ""]
    lines.append(f"Rows: {len(rows)}")
    lines.append(f"Benchmark sets: {len(benchmark_sets)}")
    lines.append(f"Benchmarks: {len(benchmarks)}")
    lines.append(f"Mappers: {len(mappers)}")
    lines.append(f"Architectures: {len(archs)}")
    lines.append("")
    lines.append(f"Overall status: {'FAIL' if failures else 'PASS'}")
    lines.append("")

    add_table(lines, "Failures", failures)
    add_table(lines, "Warnings", warnings)

    lines.extend(
        [
            "## Prior-Work Sanity Checklist",
            "",
            "- II evaluation should start from MII for real quality comparisons; fixed larger start_II is only a smoke-test shortcut.",
            "- Report achieved II, success rate, and mapping/P&R time together; throughput claims should be tied to achieved II.",
            "- Separate useful compute utilization from route-only usage, because routing PEs can inflate total occupancy.",
            "- Inspect benchmark-level results, since routing pressure and failures often concentrate in only a subset of kernels.",
            "- Direct numeric comparison to papers is only meaningful with matching benchmarks, architecture size, memory model, routing resources, operation latencies, and timeout policy.",
            "",
        ]
    )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
