#!/usr/bin/env python3

import argparse
from pathlib import Path

from lib import find_one, normalize_run, write_metrics_csv


def main() -> None:
    parser = argparse.ArgumentParser(description="Collect normalized metrics from research result directories.")
    parser.add_argument("--results-dir", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    rows = []
    for run_dir in sorted(args.results_dir.glob("**/mapping/*")):
        if not run_dir.is_dir():
            continue
        if not find_one(run_dir, "output_log_*.json"):
            continue
        parts = run_dir.parts
        benchmark = ""
        mapper = ""
        arch_name = ""
        benchmark_set = ""
        achieved_ii = None
        for part in parts:
            if part.startswith("set="):
                benchmark_set = part.split("=", 1)[1]
            elif part.startswith("benchmark="):
                benchmark = part.split("=", 1)[1]
            elif part.startswith("mapper="):
                mapper = part.split("=", 1)[1]
            elif part.startswith("arch="):
                arch_name = part.split("=", 1)[1]
            elif part.startswith("ii_"):
                achieved_ii = int(part.split("_", 1)[1])
        row = normalize_run(
            run_dir,
            benchmark=benchmark,
            mapper=mapper,
            arch_name=arch_name,
            mii=None,
            achieved_ii=achieved_ii,
        )
        row["benchmark_set"] = benchmark_set
        rows.append(row)

    write_metrics_csv(args.out, rows)


if __name__ == "__main__":
    main()
