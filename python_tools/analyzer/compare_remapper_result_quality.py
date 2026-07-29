#!/usr/bin/env python3
"""Compare two remapper_result.csv files for mapping-quality regressions.

The analyzer CSV contains success, parallel_num, mapping_type_num,
remapping_time_s, and hardware_utilization. This script compares groups of
matching inputs and exits nonzero when the candidate quality decreases.
"""

import argparse
import csv
import sys
from collections import defaultdict

DEFAULT_KEY_COLUMNS = [
    "benchmark_name",
    "cgra_row",
    "cgra_column",
    "cgra_memory_io",
    "cgra_type",
    "cgra_network_type",
    "cgra_local_reg_size",
    "cgra_context_size",
    "cgra_loop_controllers",
    "remapper_type",
    "num_available_mappings",
    "database_mapping_files_num",
]

UTILIZATION_COLUMNS = [
    "hardware_utilization",
    "utilization",
    "cgra_utilization",
]


def parse_bool(value):
    return str(value).strip().lower() in {"1", "true", "yes", "y"}


def parse_float(value, default=0.0):
    text = str(value).strip()
    if text == "" or text.lower() == "none":
        return default
    return float(text)


def parse_int(value, default=0):
    return int(parse_float(value, default))


def load_rows(path):
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        fieldnames = reader.fieldnames or []
    return rows, fieldnames


def row_key(row, key_columns):
    return tuple(row.get(column, "") for column in key_columns)


def is_meaningful_success(row):
    return (
        parse_bool(row.get("mapping_succeed", "False"))
        and parse_int(row.get("parallel_num", 0)) > 0
        and parse_int(row.get("mapping_type_num", 0)) > 0
    )


def summarize(rows, utilization_columns):
    successful = [row for row in rows if is_meaningful_success(row)]
    summary = {
        "rows": len(rows),
        "success": len(successful),
        "parallel_sum": sum(parse_int(row.get("parallel_num", 0)) for row in successful),
        "mapping_type_sum": sum(parse_int(row.get("mapping_type_num", 0)) for row in successful),
        "time_sum": sum(parse_float(row.get("remapper_time_s", 0)) for row in successful),
    }
    for column in utilization_columns:
        values = [parse_float(row.get(column, 0)) for row in successful]
        summary[f"{column}_sum"] = sum(values)
        summary[f"{column}_max"] = max(values, default=0.0)
    return summary


def format_key(key_columns, key):
    return ", ".join(f"{column}={value}" for column, value in zip(key_columns, key))


def compare_groups(baseline_groups, candidate_groups, utilization_columns):
    regressions = []
    improvements = 0
    common_keys = sorted(set(baseline_groups) & set(candidate_groups))

    for key in common_keys:
        baseline = summarize(baseline_groups[key], utilization_columns)
        candidate = summarize(candidate_groups[key], utilization_columns)
        reasons = []

        if candidate["success"] < baseline["success"]:
            reasons.append(("success", baseline["success"], candidate["success"]))
        if candidate["parallel_sum"] < baseline["parallel_sum"]:
            reasons.append(("parallel_sum", baseline["parallel_sum"], candidate["parallel_sum"]))
        if candidate["mapping_type_sum"] < baseline["mapping_type_sum"]:
            reasons.append(("mapping_type_sum", baseline["mapping_type_sum"], candidate["mapping_type_sum"]))
        for column in utilization_columns:
            sum_name = f"{column}_sum"
            max_name = f"{column}_max"
            if candidate[sum_name] + 1e-12 < baseline[sum_name]:
                reasons.append((sum_name, baseline[sum_name], candidate[sum_name]))
            if candidate[max_name] + 1e-12 < baseline[max_name]:
                reasons.append((max_name, baseline[max_name], candidate[max_name]))

        if reasons:
            regressions.append((key, baseline, candidate, reasons))
        elif (
            candidate["success"] > baseline["success"]
            or candidate["parallel_sum"] > baseline["parallel_sum"]
            or candidate["mapping_type_sum"] > baseline["mapping_type_sum"]
        ):
            improvements += 1

    return common_keys, regressions, improvements


def build_groups(rows, key_columns):
    groups = defaultdict(list)
    for row in rows:
        groups[row_key(row, key_columns)].append(row)
    return groups


def main():
    parser = argparse.ArgumentParser(
        description="Compare two remapper_result.csv files and fail if mapping quality decreases."
    )
    parser.add_argument("baseline_csv", help="Baseline remapper_result.csv")
    parser.add_argument("candidate_csv", help="Candidate remapper_result.csv")
    parser.add_argument(
        "--key-columns",
        default=",".join(DEFAULT_KEY_COLUMNS),
        help="Comma-separated columns used to match input groups.",
    )
    parser.add_argument(
        "--allow-missing-candidate-groups",
        action="store_true",
        help="Do not fail when a baseline group is absent from the candidate CSV.",
    )
    args = parser.parse_args()

    key_columns = [column.strip() for column in args.key_columns.split(",") if column.strip()]
    baseline_rows, baseline_fields = load_rows(args.baseline_csv)
    candidate_rows, candidate_fields = load_rows(args.candidate_csv)

    missing_key_columns = [
        column for column in key_columns if column not in baseline_fields or column not in candidate_fields
    ]
    if missing_key_columns:
        print("Missing key columns: " + ", ".join(missing_key_columns), file=sys.stderr)
        return 2

    utilization_columns = [
        column
        for column in UTILIZATION_COLUMNS
        if column in baseline_fields and column in candidate_fields
    ]

    baseline_groups = build_groups(baseline_rows, key_columns)
    candidate_groups = build_groups(candidate_rows, key_columns)
    common_keys, regressions, improvements = compare_groups(
        baseline_groups, candidate_groups, utilization_columns
    )

    missing_candidate_groups = sorted(set(baseline_groups) - set(candidate_groups))
    regressed_missing_candidate_groups = [
        key for key in missing_candidate_groups
        if summarize(baseline_groups[key], utilization_columns)["success"] > 0
    ]
    candidate_only_groups = sorted(set(candidate_groups) - set(baseline_groups))

    baseline_total = summarize(baseline_rows, utilization_columns)
    candidate_total = summarize(candidate_rows, utilization_columns)

    print("Baseline rows:", len(baseline_rows))
    print("Candidate rows:", len(candidate_rows))
    print("Common groups:", len(common_keys))
    print("Baseline-only groups:", len(missing_candidate_groups))
    print("Candidate-only groups:", len(candidate_only_groups))
    print("Improved common groups:", improvements)
    print("Regressed common groups:", len(regressions))
    print("Utilization columns compared:", ", ".join(utilization_columns) or "<none in CSV>")
    print()
    print("Overall meaningful-quality totals")
    print("metric,baseline,candidate")
    for metric in ["success", "parallel_sum", "mapping_type_sum", "time_sum"]:
        print(f"{metric},{baseline_total[metric]},{candidate_total[metric]}")
    for column in utilization_columns:
        for suffix in ["sum", "max"]:
            metric = f"{column}_{suffix}"
            print(f"{metric},{baseline_total[metric]},{candidate_total[metric]}")

    failed = False
    if regressed_missing_candidate_groups and not args.allow_missing_candidate_groups:
        failed = True
        print("\nBaseline groups with meaningful success missing in candidate:")
        for key in regressed_missing_candidate_groups[:50]:
            print("  " + format_key(key_columns, key))
        if len(regressed_missing_candidate_groups) > 50:
            print(f"  ... {len(regressed_missing_candidate_groups) - 50} more")
    elif missing_candidate_groups:
        print("\nBaseline-only groups have no meaningful baseline success; not treated as regressions.")

    if regressions:
        failed = True
        print("\nQuality regressions:")
        for index, (key, baseline, candidate, reasons) in enumerate(regressions, start=1):
            print(f"[{index}] " + format_key(key_columns, key))
            print(
                "  baseline: "
                f"success={baseline['success']} parallel_sum={baseline['parallel_sum']} "
                f"mapping_type_sum={baseline['mapping_type_sum']} time_sum={baseline['time_sum']}"
            )
            print(
                "  candidate: "
                f"success={candidate['success']} parallel_sum={candidate['parallel_sum']} "
                f"mapping_type_sum={candidate['mapping_type_sum']} time_sum={candidate['time_sum']}"
            )
            for metric, baseline_value, candidate_value in reasons:
                print(f"  regression: {metric} {baseline_value} -> {candidate_value}")

    if failed:
        return 1

    print("\nPASS: no mapping-quality regression found in common groups.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
