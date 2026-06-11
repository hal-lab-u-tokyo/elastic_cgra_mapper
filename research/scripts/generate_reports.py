#!/usr/bin/env python3

import argparse
import subprocess
import sys
from pathlib import Path


def run_script(script_dir: Path, script_name: str, args: list) -> None:
    subprocess.run([sys.executable, str(script_dir / script_name), *args], check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the standard report set for a research result directory.")
    parser.add_argument("--result-dir", required=True, type=Path)
    args = parser.parse_args()

    result_dir = args.result_dir
    metrics = result_dir / "metrics.csv"
    if not metrics.exists():
        raise FileNotFoundError(f"metrics.csv not found: {metrics}")

    script_dir = Path(__file__).resolve().parent
    run_script(
        script_dir,
        "report_by_benchmark.py",
        ["--metrics", str(metrics), "--out", str(result_dir / "benchmark_report.md")],
    )
    run_script(
        script_dir,
        "compare_results.py",
        ["--metrics", str(metrics), "--group-by", "mapper", "--out", str(result_dir / "summary_by_mapper.md")],
    )
    run_script(
        script_dir,
        "compare_results.py",
        ["--metrics", str(metrics), "--group-by", "arch_name", "--out", str(result_dir / "summary_by_arch.md")],
    )
    run_script(
        script_dir,
        "compare_results.py",
        ["--metrics", str(metrics), "--group-by", "benchmark_set", "--out", str(result_dir / "summary_by_set.md")],
    )
    run_script(
        script_dir,
        "validate_metrics.py",
        ["--metrics", str(metrics), "--out", str(result_dir / "validation.md")],
    )
    run_script(
        script_dir,
        "validate_mapping_routes.py",
        [
            "--metrics",
            str(metrics),
            "--out",
            str(result_dir / "routing_validation.md"),
            "--repo-root",
            str(Path.cwd()),
        ],
    )


if __name__ == "__main__":
    main()
