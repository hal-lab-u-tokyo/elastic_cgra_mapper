#!/usr/bin/env python3

import argparse
import json
import subprocess
import time
from pathlib import Path

from lib import (
    compute_res_mii,
    find_one,
    find_single_run_dir,
    load_json,
    make_arch_for_ii,
    normalize_run,
    placement2d_capacity_check,
    write_json,
    write_metrics_csv,
)

EXTERNAL_RUNNERS = {"vpr", "vpr_modulo", "vpr_modulo_full_route"}


def mapper_runner(mapper: dict) -> str:
    return str(mapper.get("runner", mapper.get("external_runner", "cgra"))).lower()


def is_external_runner(mapper: dict) -> bool:
    return mapper_runner(mapper) in EXTERNAL_RUNNERS


def prepare_cli_mapper_config(mapper_config_path: Path, output_dir: Path, evaluation_mode: str) -> Path:
    if evaluation_mode != "placement_only":
        return mapper_config_path
    mapper_config = load_json(mapper_config_path)
    algorithm = dict(mapper_config.get("Algorithm", {}))
    algorithm["placement_only"] = True
    mapper_config["Algorithm"] = algorithm
    generated_dir = output_dir / "generated_inputs"
    generated_dir.mkdir(parents=True, exist_ok=True)
    generated_mapper_path = generated_dir / "mapper_config.json"
    write_json(generated_mapper_path, mapper_config)
    return generated_mapper_path


def resolve_mii(
    mii_arg: str,
    dfg: Path,
    arch_template: Path,
    out_dir: Path,
    missing_distance_policy: str,
) -> tuple:
    mii_data = compute_res_mii(dfg, arch_template, missing_distance_policy)
    write_json(out_dir / "mii.json", mii_data)
    computed_mii = int(mii_data["MII"]) if mii_data["MII"] is not None else None
    if mii_arg == "auto":
        if mii_data["MII"] is None:
            raise ValueError(f"Cannot compute MII; unsupported ops: {mii_data['unsupported_ops']}")
        return computed_mii, computed_mii
    return computed_mii, int(mii_arg)


def run_one(
    mapping_bin: Path,
    dfg: Path,
    arch_template: Path,
    mapper_config: Path,
    output_dir: Path,
    benchmark: str,
    mapper_name: str,
    arch_name: str,
    mii: str,
    ii_max: int,
    timeout_sec: float,
    parallel_num: int,
    missing_distance_policy: str = "self_loop",
    problem_type: str = "modulo",
    evaluation_mode: str = "routing",
    progress: bool = False,
) -> dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    if problem_type == "placement2d":
        capacity = placement2d_capacity_check(dfg, arch_template)
        write_json(
            output_dir / "mii.json",
            {
                "problem_type": problem_type,
                "MII": 1,
                "ResMII": 1,
                "RecMII": "",
                "placement2d_capacity": capacity,
                "notes": "2D placement fixes context_size/II to 1; modulo MII is not used.",
            },
        )
        if not capacity["ok"]:
            raise ValueError(
                "2D placement capacity check failed: "
                f"{capacity['detail']}. Use a larger 2D CGRA or a modulo-aware manifest."
            )
        computed_mii, start_ii, effective_ii_max = 1, 1, 1
    else:
        computed_mii, start_ii = resolve_mii(
            mii, dfg, arch_template, output_dir, missing_distance_policy
        )
        effective_ii_max = ii_max
    rows = []
    summary = {
        "benchmark": benchmark,
        "mapper": mapper_name,
        "arch_name": arch_name,
        "problem_type": problem_type,
        "evaluation_mode": evaluation_mode,
        "dfg": str(dfg),
        "arch_template": str(arch_template),
        "mapper_config": str(mapper_config),
        "MII": computed_mii,
        "start_II": start_ii,
        "ii_max": effective_ii_max,
        "requested_ii_max": ii_max,
        "timeout_sec": timeout_sec,
        "parallel_num": parallel_num,
        "missing_distance_policy": missing_distance_policy,
        "status": "failed",
        "achieved_II": None,
        "trials": [],
    }

    if parallel_num != 1:
        raise ValueError("Modulo mapper comparison should use parallel_num=1 because build/mapping duplicates the DFG otherwise.")

    if progress:
        print(
            f"  problem_type={problem_type}, MII={computed_mii}, "
            f"start_II={start_ii}, ii_max={effective_ii_max}",
            flush=True,
        )

    for ii in range(start_ii, effective_ii_max + 1):
        trial_dir = output_dir / "tried_ii" / f"ii_{ii}"
        attempt_id = f"attempt_{time.time_ns()}"
        raw_output_dir = trial_dir / "raw" / attempt_id
        arch_path = trial_dir / "arch.json"
        stdout_path = trial_dir / "stdout.txt"
        stderr_path = trial_dir / "stderr.txt"
        trial_dir.mkdir(parents=True, exist_ok=True)
        raw_output_dir.mkdir(parents=True, exist_ok=True)
        write_json(arch_path, make_arch_for_ii(arch_template, ii, force_default=True))

        cmd = [
            str(mapping_bin),
            str(dfg.resolve()),
            str(arch_path.resolve()),
            str(raw_output_dir.resolve()),
            str(mapper_config.resolve()),
            str(timeout_sec),
            str(parallel_num),
        ]
        if progress:
            print(f"    II={ii}: running", flush=True)
        started_at = time.time()
        proc = subprocess.run(cmd, capture_output=True, text=True)
        elapsed = time.time() - started_at
        stdout_path.write_text(proc.stdout)
        stderr_path.write_text(proc.stderr)

        run_dir = find_single_run_dir(raw_output_dir)
        output_log = find_one(run_dir, "output_log_*.json") if run_dir else None
        is_success = False
        if output_log:
            with output_log.open() as f:
                is_success = bool(json.load(f).get("is_success", False))

        row = normalize_run(
            run_dir if run_dir else raw_output_dir,
            benchmark=benchmark,
            mapper=mapper_name,
            arch_name=arch_name,
            mii=computed_mii,
            achieved_ii=ii if is_success else None,
            start_ii=start_ii,
            dfg_path=dfg,
            arch_path=arch_path,
            evaluation_mode=evaluation_mode,
        )
        row["process_returncode"] = proc.returncode
        row["wall_time_sec"] = elapsed
        row["trial_dir"] = str(trial_dir)
        row["stdout_file"] = str(stdout_path)
        row["stderr_file"] = str(stderr_path)
        row["arch_file"] = str(arch_path)
        row["raw_output_dir"] = str(raw_output_dir)
        rows.append(row)

        if progress:
            outcome = "success" if is_success else "failed"
            print(
                f"    II={ii}: {outcome}, status={row.get('status', '')}, "
                f"elapsed={elapsed:.3f}s",
                flush=True,
            )

        summary["trials"].append(
            {
                "II": ii,
                "returncode": proc.returncode,
                "success": is_success,
                "run_dir": str(run_dir) if run_dir else "",
                "trial_dir": str(trial_dir),
                "stdout_file": str(stdout_path),
                "stderr_file": str(stderr_path),
                "arch_file": str(arch_path),
                "raw_output_dir": str(raw_output_dir),
                "wall_time_sec": elapsed,
            }
        )
        if is_success:
            summary["status"] = row["status"]
            summary["achieved_II"] = ii
            break

    write_json(output_dir / "summary.json", summary)
    write_metrics_csv(output_dir / "metrics.csv", rows)
    return summary


def run_mapper_case(
    mapping_bin: Path,
    dfg: Path,
    arch_template: Path,
    mapper: dict,
    mapper_config: Path,
    output_dir: Path,
    benchmark: str,
    mapper_name: str,
    arch_name: str,
    mii: str,
    ii_max: int,
    timeout_sec: float,
    parallel_num: int,
    missing_distance_policy: str = "self_loop",
    problem_type: str = "modulo",
    evaluation_mode: str = "routing",
    progress: bool = False,
) -> dict:
    """Run one benchmark/architecture/mapper case through a single dispatcher.

    In-repository mappers use `build/mapping` and `mapper_config`. External
    baselines such as VPR keep their specialized implementation files, but
    experiment orchestration enters through this function for every mapper.
    """
    runner = mapper_runner(mapper)
    if runner == "vpr":
        from run_vpr_baseline import run_one_vpr

        return run_one_vpr(
            dfg=dfg,
            arch_template=arch_template,
            output_dir=output_dir,
            benchmark=benchmark,
            mapper_name=mapper_name,
            arch_name=arch_name,
            timeout_sec=timeout_sec,
            mapper_config=mapper,
            problem_type=problem_type,
            evaluation_mode=evaluation_mode,
            progress=progress,
        )
    if runner == "vpr_modulo":
        from run_vpr_modulo_routing import run_one_vpr_modulo

        return run_one_vpr_modulo(
            dfg=dfg,
            arch_template=arch_template,
            output_dir=output_dir,
            benchmark=benchmark,
            mapper_name=mapper_name,
            arch_name=arch_name,
            mii=mii,
            ii_max=ii_max,
            timeout_sec=timeout_sec,
            mapper_config=mapper,
            missing_distance_policy=missing_distance_policy,
            progress=progress,
        )
    if runner == "vpr_modulo_full_route":
        from run_vpr_modulo_full_routing import run_one_vpr_modulo_full_route

        return run_one_vpr_modulo_full_route(
            dfg=dfg,
            arch_template=arch_template,
            output_dir=output_dir,
            benchmark=benchmark,
            mapper_name=mapper_name,
            arch_name=arch_name,
            mii=mii,
            ii_max=ii_max,
            timeout_sec=timeout_sec,
            mapper_config=mapper,
            missing_distance_policy=missing_distance_policy,
            progress=progress,
        )
    if runner != "cgra":
        raise ValueError(f"Unknown mapper runner: {runner}")

    return run_one(
        mapping_bin=mapping_bin,
        dfg=dfg,
        arch_template=arch_template,
        mapper_config=mapper_config,
        output_dir=output_dir,
        benchmark=benchmark,
        mapper_name=mapper_name,
        arch_name=arch_name,
        mii=mii,
        ii_max=ii_max,
        timeout_sec=timeout_sec,
        parallel_num=parallel_num,
        missing_distance_policy=missing_distance_policy,
        problem_type=problem_type,
        evaluation_mode=evaluation_mode,
        progress=progress,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Run one mapper case with the unified research runner.")
    parser.add_argument("--mapping-bin", required=True, type=Path)
    parser.add_argument("--dfg", required=True, type=Path)
    parser.add_argument("--arch-template", required=True, type=Path)
    parser.add_argument("--mapper-config", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--benchmark", default="")
    parser.add_argument("--mapper-name", default="")
    parser.add_argument("--arch-name", default="")
    parser.add_argument("--mii", default="auto")
    parser.add_argument("--ii-max", required=True, type=int)
    parser.add_argument("--timeout-sec", required=True, type=float)
    parser.add_argument("--parallel-num", default=1, type=int)
    parser.add_argument(
        "--missing-distance-policy",
        choices=["strict", "self_loop"],
        default="self_loop",
        help="How to handle recurrence edges without explicit distance.",
    )
    parser.add_argument(
        "--problem-type",
        choices=["modulo", "placement2d"],
        default="modulo",
        help="Mapping problem setting. placement2d fixes context_size/II to 1.",
    )
    parser.add_argument(
        "--evaluation-mode",
        choices=["routing", "placement_only"],
        default="routing",
        help="routing requires valid mapped routes; placement_only measures placement quality.",
    )
    parser.add_argument("--progress", action="store_true", help="Print per-II progress to stdout.")
    args = parser.parse_args()

    benchmark = args.benchmark or args.dfg.stem
    mapper_name = args.mapper_name or args.mapper_config.stem
    arch_name = args.arch_name or args.arch_template.stem
    mapper_config = prepare_cli_mapper_config(args.mapper_config, args.output_dir, args.evaluation_mode)
    summary = run_mapper_case(
        mapping_bin=args.mapping_bin,
        dfg=args.dfg,
        arch_template=args.arch_template,
        mapper={"name": mapper_name, "mapper_config": str(mapper_config)},
        mapper_config=mapper_config,
        output_dir=args.output_dir,
        benchmark=benchmark,
        mapper_name=mapper_name,
        arch_name=arch_name,
        mii=args.mii,
        ii_max=args.ii_max,
        timeout_sec=args.timeout_sec,
        parallel_num=args.parallel_num,
        missing_distance_policy=args.missing_distance_policy,
        problem_type=args.problem_type,
        evaluation_mode=args.evaluation_mode,
        progress=args.progress,
    )
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
