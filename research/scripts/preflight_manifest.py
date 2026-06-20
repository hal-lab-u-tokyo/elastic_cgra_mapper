#!/usr/bin/env python3

import argparse
import csv
import math
from pathlib import Path

from lib import (
    auto_square_arch_for_dfg,
    compute_res_mii,
    load_json,
    placement2d_capacity_check,
    placement2d_capacity_check_for_arch,
    read_dfg_stats,
    write_json,
)
from run_mapper_case import EXTERNAL_RUNNERS, mapper_runner
from run_suite import (
    apply_arch_overrides,
    arch_has_auto_grid,
    auto_grid_policy,
    expand_benchmark_sets,
    manifest_mappers,
    manifest_problem_type,
    parse_filter,
    placement2d_arch_ii,
    selected,
)


CONTAINER_REPO_ROOT = Path("/home/ubuntu/elastic_cgra_mapper")


def resolve_repo_path(path: str, repo_root: Path) -> Path:
    raw_path = Path(path)
    if raw_path.is_absolute():
        try:
            relative = raw_path.relative_to(CONTAINER_REPO_ROOT)
            return repo_root / relative
        except ValueError:
            return raw_path
    return repo_root / raw_path


def status_for_row(row: dict) -> str:
    if row["missing_files"]:
        return "missing_file"
    if row["problem_type"] == "placement2d":
        if row["placement2d_ii_valid"] != "yes":
            return "invalid_placement2d_ii"
        if row["placement2d_capacity_ok"] != "yes":
            return "placement2d_capacity_error"
    if row["MII"] == "":
        return "invalid_mii"
    if row.get("runner") not in EXTERNAL_RUNNERS and row["unsupported_ops"]:
        return "unsupported_ops"
    if row["ii_max"] < row["start_II"]:
        return "ii_range_empty"
    return "ready"


def write_csv(path: Path, rows: list) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("")
        return
    fieldnames = list(rows[0].keys())
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def add_status_table(lines: list, rows: list) -> None:
    status_counts = {}
    for row in rows:
        status_counts[row["preflight_status"]] = status_counts.get(row["preflight_status"], 0) + 1

    lines.extend(["## Status Summary", ""])
    lines.append("| status | count |")
    lines.append("| --- | ---: |")
    for status, count in sorted(status_counts.items()):
        lines.append(f"| {status} | {count} |")
    lines.append("")


def add_issue_table(lines: list, rows: list) -> None:
    issue_rows = [row for row in rows if row["preflight_status"] != "ready"]
    lines.extend(["## Blocking Issues", ""])
    if not issue_rows:
        lines.extend(["None.", ""])
        return

    lines.append("| benchmark set | benchmark | arch | mapper | role | placement | routing | runner | status | detail |")
    lines.append("| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for row in issue_rows:
        detail_parts = []
        if row["missing_files"]:
            detail_parts.append(f"missing={row['missing_files']}")
        if row["unsupported_ops"]:
            detail_parts.append(f"unsupported={row['unsupported_ops']}")
        if row["MII"] == "":
            detail_parts.append("MII unavailable")
        if row["ii_max"] < row["start_II"]:
            detail_parts.append(f"ii_max {row['ii_max']} < start_II {row['start_II']}")
        if row.get("placement2d_ii_valid") == "no":
            detail_parts.append("placement2d requires ii/context_size/ii_max = 1")
        if row.get("placement2d_capacity_ok") == "no":
            detail_parts.append(
                f"dfg_nodes {row['dfg_nodes']} > physical_pes {row['physical_pes']}"
            )
        lines.append(
            f"| {row['benchmark_set']} | {row['benchmark']} | {row['arch_name']} | "
            f"{row['mapper']} | {row.get('mapper_role', '')} | {row.get('placement_method', '')} | "
            f"{row.get('routing_method', '')} | {row['runner']} | {row['preflight_status']} | {'; '.join(detail_parts)} |"
        )
    lines.append("")


def add_ready_table(lines: list, rows: list) -> None:
    ready_rows = [row for row in rows if row["preflight_status"] == "ready"]
    lines.extend(["## Ready Runs", ""])
    if not ready_rows:
        lines.extend(["None.", ""])
        return

    lines.append("| benchmark set | benchmark | arch | mapper | role | placement | routing | runner | MII | start II | ii max | max trials | dfg nodes | dfg edges |")
    lines.append("| --- | --- | --- | --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |")
    for row in ready_rows:
        lines.append(
            f"| {row['benchmark_set']} | {row['benchmark']} | {row['arch_name']} | {row['mapper']} | "
            f"{row.get('mapper_role', '')} | {row.get('placement_method', '')} | {row.get('routing_method', '')} | {row['runner']} | "
            f"{row['MII']} | {row['start_II']} | {row['ii_max']} | {row['max_trials']} | "
            f"{row['dfg_nodes']} | {row['dfg_edges']} |"
        )
    lines.append("")


def write_markdown(path: Path, manifest: dict, rows: list, timeout_sec: float) -> None:
    total_max_trials = sum(int(row["max_trials"]) for row in rows if row["max_trials"] != "")
    estimated_timeout_hours = total_max_trials * timeout_sec / 3600.0
    ready_count = sum(1 for row in rows if row["preflight_status"] == "ready")

    lines = ["# Manifest Preflight", ""]
    lines.append(f"Manifest: `{manifest.get('name', '')}`")
    lines.append(f"Rows: {len(rows)}")
    lines.append(f"Ready rows: {ready_count}")
    lines.append(f"Timeout per II: {timeout_sec:g} sec")
    lines.append(f"Upper-bound timeout budget: {estimated_timeout_hours:.2f} hours")
    lines.append("")
    lines.append("The timeout budget is a pessimistic upper bound: each mapper stops sweeping after the first successful II.")
    lines.append("")

    add_status_table(lines, rows)
    add_issue_table(lines, rows)
    add_ready_table(lines, rows)

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Preflight-check a research experiment manifest before a long run.")
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--repo-root", default=Path.cwd(), type=Path)
    parser.add_argument("--out-dir", required=True, type=Path)
    parser.add_argument("--only-benchmark-set", action="append", default=[])
    parser.add_argument("--only-benchmark", action="append", default=[])
    parser.add_argument("--only-arch", action="append", default=[])
    parser.add_argument("--only-mapper", action="append", default=[])
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    manifest = load_json(args.manifest)
    problem_type = manifest_problem_type(manifest)
    timeout_sec = float(manifest["timeout_sec"])
    missing_distance_policy = manifest.get("mii_missing_distance_policy", "self_loop")
    filters = {
        "benchmark_set": parse_filter(args.only_benchmark_set),
        "benchmark": parse_filter(args.only_benchmark),
        "arch": parse_filter(args.only_arch),
        "mapper": parse_filter(args.only_mapper),
    }

    rows = []
    for benchmark_set in expand_benchmark_sets(manifest):
        benchmark_root = resolve_repo_path(benchmark_set["benchmark_root"], repo_root)
        if not selected(benchmark_set["name"], filters["benchmark_set"]):
            continue
        for benchmark in benchmark_set["benchmarks"]:
            if not selected(benchmark, filters["benchmark"]):
                continue
            dfg_path = benchmark_root / f"{benchmark}.dot"
            for arch in manifest["architectures"]:
                if not selected(arch["name"], filters["arch"]):
                    continue
                arch_path = resolve_repo_path(arch["template"], repo_root)
                if problem_type == "placement2d":
                    arch_ii = placement2d_arch_ii(arch)
                    ii_max = arch_ii if arch_ii is not None else 1
                else:
                    arch_ii = None
                    ii_max = int(arch["ii_max"])
                mii_arg = str(arch.get("mii", "auto"))
                for mapper in manifest_mappers(manifest):
                    if not selected(mapper["name"], filters["mapper"]):
                        continue
                    runner = mapper_runner(mapper)
                    mapper_config_path = (
                        resolve_repo_path(mapper["mapper_config"], repo_root)
                        if runner not in EXTERNAL_RUNNERS
                        else Path()
                    )
                    missing_files = []
                    required_files = [
                        ("dfg", dfg_path),
                        ("arch", arch_path),
                    ]
                    if runner not in EXTERNAL_RUNNERS:
                        required_files.append(("mapper", mapper_config_path))
                    for label, path in required_files:
                        if not path.exists():
                            missing_files.append(label)

                    dfg_nodes = ""
                    dfg_edges = ""
                    op_counts = {}
                    mii = ""
                    res_mii = ""
                    rec_mii = ""
                    unsupported_ops = []
                    start_ii = ""
                    max_trials = ""
                    physical_pes = ""
                    placement2d_capacity_ok = ""
                    placement2d_ii_valid = ""
                    if not missing_files:
                        stats = read_dfg_stats(dfg_path)
                        dfg_nodes = stats["node_count"]
                        dfg_edges = stats["edge_count"]
                        op_counts = stats["op_counts"]
                        if arch_has_auto_grid(arch):
                            effective_arch = auto_square_arch_for_dfg(
                                arch_path,
                                dfg_path,
                                auto_grid_policy(arch),
                            )
                        else:
                            effective_arch = load_json(arch_path)
                        effective_arch = apply_arch_overrides(effective_arch, arch)
                        if problem_type == "placement2d":
                            if arch_has_auto_grid(arch):
                                capacity = placement2d_capacity_check_for_arch(
                                    dfg_path,
                                    effective_arch,
                                )
                            else:
                                capacity = placement2d_capacity_check(dfg_path, arch_path)
                            physical_pes = capacity["physical_pes"]
                            placement2d_capacity_ok = "yes" if capacity["ok"] else "no"
                            placement2d_ii_valid = "yes" if arch_ii == 1 else "no"
                            mii_data = compute_res_mii(
                                dfg_path, arch_path, missing_distance_policy
                            )
                            unsupported_ops = mii_data["unsupported_ops"]
                            mii = 1
                            res_mii = 1
                            rec_mii = ""
                            start_ii = 1
                            ii_max = 1
                            max_trials = 1
                        else:
                            mii_data = compute_res_mii(
                                dfg_path, arch_path, missing_distance_policy
                            )
                            mii = mii_data["MII"] if mii_data["MII"] is not None else ""
                            res_mii = mii_data["ResMII"] if mii_data["ResMII"] is not None else ""
                            rec_mii = mii_data["RecMII"] if mii_data["RecMII"] is not None else ""
                            unsupported_ops = mii_data["unsupported_ops"]
                            if mii_arg == "auto":
                                start_ii = mii
                            else:
                                start_ii = int(mii_arg)
                            if start_ii != "":
                                max_trials = max(0, ii_max - int(start_ii) + 1)

                    row = {
                        "problem_type": problem_type,
                        "runner": runner,
                        "mapper_role": mapper.get("mapper_role", ""),
                        "placement_method": mapper.get("placement_method", ""),
                        "routing_method": mapper.get("routing_method", ""),
                        "benchmark_set": benchmark_set["name"],
                        "benchmark": benchmark,
                        "mapper": mapper["name"],
                        "arch_name": arch["name"],
                        "dfg_path": str(dfg_path),
                        "arch_template": str(arch_path),
                        "mapper_config": str(mapper_config_path),
                        "missing_files": ",".join(missing_files),
                        "dfg_nodes": dfg_nodes,
                        "dfg_edges": dfg_edges,
                        "op_counts": ";".join(f"{op}:{count}" for op, count in sorted(op_counts.items())),
                        "ResMII": res_mii,
                        "RecMII": rec_mii,
                        "MII": mii,
                        "start_II": start_ii,
                        "ii_max": ii_max,
                        "max_trials": max_trials,
                        "physical_pes": physical_pes,
                        "placement2d_capacity_ok": placement2d_capacity_ok,
                        "placement2d_ii_valid": placement2d_ii_valid,
                        "unsupported_ops": ",".join(unsupported_ops),
                    }
                    row["preflight_status"] = status_for_row(row)
                    rows.append(row)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    write_json(args.out_dir / "manifest.json", manifest)
    write_csv(args.out_dir / "preflight.csv", rows)
    write_markdown(args.out_dir / "preflight.md", manifest, rows, timeout_sec)

    has_blocking_issue = any(row["preflight_status"] != "ready" for row in rows)
    if has_blocking_issue:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
