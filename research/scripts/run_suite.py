#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

from lib import load_json, write_json, write_metrics_csv
from run_modulo_mapping import run_one


def parse_filter(values: list) -> set:
    result = set()
    for value in values:
        for item in value.split(","):
            item = item.strip()
            if item:
                result.add(item)
    return result


def selected(value: str, filter_values: set) -> bool:
    return not filter_values or value in filter_values


def expand_benchmark_sets(manifest: dict) -> list:
    if "benchmark_sets" not in manifest:
        return [
            {
                "name": manifest.get("benchmark_set", "default"),
                "benchmark_root": manifest["benchmark_root"],
                "benchmarks": manifest["benchmarks"],
            }
        ]

    sets = []
    default_root = manifest.get("benchmark_root", "")
    for benchmark_set in manifest["benchmark_sets"]:
        sets.append(
            {
                "name": benchmark_set["name"],
                "benchmark_root": benchmark_set.get("benchmark_root", default_root),
                "benchmarks": benchmark_set["benchmarks"],
            }
        )
    return sets


def main() -> None:
    parser = argparse.ArgumentParser(description="Run a research experiment manifest.")
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--only-benchmark-set", action="append", default=[])
    parser.add_argument("--only-benchmark", action="append", default=[])
    parser.add_argument("--only-arch", action="append", default=[])
    parser.add_argument("--only-mapper", action="append", default=[])
    args = parser.parse_args()

    manifest = load_json(args.manifest)
    filters = {
        "benchmark_set": sorted(parse_filter(args.only_benchmark_set)),
        "benchmark": sorted(parse_filter(args.only_benchmark)),
        "arch": sorted(parse_filter(args.only_arch)),
        "mapper": sorted(parse_filter(args.only_mapper)),
    }
    args.out.mkdir(parents=True, exist_ok=True)
    manifest_log = dict(manifest)
    manifest_log["run_filter"] = filters
    write_json(args.out / "manifest.json", manifest_log)

    all_rows = []
    summaries = []
    timeout_sec = float(manifest["timeout_sec"])
    parallel_num = int(manifest.get("parallel_num", 1))
    mapping_bin = Path(manifest["mapping_bin"])
    missing_distance_policy = manifest.get("mii_missing_distance_policy", "self_loop")

    benchmark_set_filter = set(filters["benchmark_set"])
    benchmark_filter = set(filters["benchmark"])
    arch_filter = set(filters["arch"])
    mapper_filter = set(filters["mapper"])

    for benchmark_set in expand_benchmark_sets(manifest):
        benchmark_root = Path(benchmark_set["benchmark_root"])
        benchmark_set_name = benchmark_set["name"]
        if not selected(benchmark_set_name, benchmark_set_filter):
            continue
        for benchmark in benchmark_set["benchmarks"]:
            if not selected(benchmark, benchmark_filter):
                continue
            dfg = benchmark_root / f"{benchmark}.dot"
            for arch in manifest["architectures"]:
                arch_name = arch["name"]
                if not selected(arch_name, arch_filter):
                    continue
                arch_template = Path(arch["template"])
                ii_max = int(arch["ii_max"])
                mii = str(arch.get("mii", "auto"))
                for mapper in manifest["mappers"]:
                    mapper_name = mapper["name"]
                    if not selected(mapper_name, mapper_filter):
                        continue
                    mapper_config = Path(mapper["mapper_config"])
                    run_dir = (
                        args.out
                        / f"set={benchmark_set_name}"
                        / f"benchmark={benchmark}"
                        / f"arch={arch_name}"
                        / f"mapper={mapper_name}"
                    )
                    summary = run_one(
                        mapping_bin=mapping_bin,
                        dfg=dfg,
                        arch_template=arch_template,
                        mapper_config=mapper_config,
                        output_dir=run_dir,
                        benchmark=benchmark,
                        mapper_name=mapper_name,
                        arch_name=arch_name,
                        mii=mii,
                        ii_max=ii_max,
                        timeout_sec=timeout_sec,
                        parallel_num=parallel_num,
                        missing_distance_policy=missing_distance_policy,
                    )
                    summary["benchmark_set"] = benchmark_set_name
                    summaries.append(summary)
                    metrics_path = run_dir / "metrics.csv"
                    if metrics_path.exists():
                        with metrics_path.open() as f:
                            for row in csv.DictReader(f):
                                row["benchmark_set"] = benchmark_set_name
                                all_rows.append(row)

    write_json(args.out / "summary.json", {"runs": summaries})
    write_metrics_csv(args.out / "metrics.csv", all_rows)


if __name__ == "__main__":
    main()
