#!/usr/bin/env python3

import argparse
import atexit
import csv
import platform
import subprocess
import sys
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
try:
    from zoneinfo import ZoneInfo
except ImportError:
    ZoneInfo = None

from lib import (
    auto_square_arch_for_dfg,
    load_json,
    placement2d_capacity_check,
    placement2d_capacity_check_for_arch,
    write_json,
    write_metrics_csv,
)
from run_modulo_mapping import run_one
from run_vpr_baseline import run_one_vpr
from run_vpr_modulo_full_routing import run_one_vpr_modulo_full_route
from run_vpr_modulo_routing import run_one_vpr_modulo

LOCAL_TIMEZONE = ZoneInfo("Asia/Tokyo") if ZoneInfo else timezone(timedelta(hours=9), "JST")
ANSI_STYLES = {
    "reset": "\033[0m",
    "bold": "\033[1m",
    "dim": "\033[2m",
    "red": "\033[31m",
    "green": "\033[32m",
    "yellow": "\033[33m",
    "blue": "\033[34m",
    "magenta": "\033[35m",
    "cyan": "\033[36m",
}
SUCCESS_STATUSES = {"optimal", "success"}
WARNING_STATUSES = {"timeout_feasible", "feasible", "skipped"}
FAILURE_STATUSES = {"failed", "timeout", "infeasible", "error"}
EXTERNAL_RUNNERS = {"vpr", "vpr_modulo", "vpr_modulo_full_route"}


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


def mapper_runner(mapper: dict) -> str:
    return str(mapper.get("runner", mapper.get("external_runner", "cgra"))).lower()


def manifest_problem_type(manifest: dict) -> str:
    if "problem_type" in manifest:
        return str(manifest["problem_type"])
    mode = str(manifest.get("mode", ""))
    if mode.startswith("placement2d"):
        return "placement2d"
    return "modulo"


def placement2d_arch_ii(arch: dict):
    for key in ("ii", "context_size", "ii_max"):
        if key in arch:
            return int(arch[key])
    return None


def arch_has_auto_grid(arch: dict) -> bool:
    return bool(arch.get("auto_grid"))


def auto_grid_policy(arch: dict) -> str:
    auto_grid = arch.get("auto_grid", {})
    if isinstance(auto_grid, str):
        return auto_grid
    return str(auto_grid.get("policy", "traversal_fully_pipelined"))


def apply_arch_overrides(arch_config: dict, arch_manifest: dict) -> dict:
    result = dict(arch_config)
    for key in ("memory_io", "network_type", "CGRA_type", "local_reg_size", "placement_cost_model"):
        if key in arch_manifest:
            result[key] = arch_manifest[key]
    return result


def effective_arch_dict_for_plan_item(item: dict) -> dict:
    arch = item["arch"]
    if arch_has_auto_grid(arch):
        arch_config = auto_square_arch_for_dfg(
            Path(arch["template"]),
            item["dfg"],
            auto_grid_policy(arch),
        )
    else:
        arch_config = load_json(Path(arch["template"]))
    return apply_arch_overrides(arch_config, arch)


def prepare_arch_template_for_run(item: dict, run_dir: Path) -> Path:
    arch = item["arch"]
    arch_template = Path(arch["template"])
    has_overrides = any(
        key in arch for key in ("memory_io", "network_type", "CGRA_type", "local_reg_size", "placement_cost_model")
    )
    if not arch_has_auto_grid(arch) and not has_overrides:
        return arch_template

    effective_arch = effective_arch_dict_for_plan_item(item)
    generated_dir = run_dir / "generated_inputs"
    generated_dir.mkdir(parents=True, exist_ok=True)
    generated_arch_path = generated_dir / "arch_template.json"
    write_json(generated_arch_path, effective_arch)
    return generated_arch_path


def prepare_mapper_config_for_run(
    mapper: dict,
    run_dir: Path,
    evaluation_mode: str,
) -> Path:
    if mapper_runner(mapper) in EXTERNAL_RUNNERS:
        return Path()
    mapper_config_path = Path(mapper["mapper_config"])
    if evaluation_mode != "placement_only":
        return mapper_config_path

    mapper_config = load_json(mapper_config_path)
    algorithm = dict(mapper_config.get("Algorithm", {}))
    algorithm["placement_only"] = True
    mapper_config["Algorithm"] = algorithm
    generated_dir = run_dir / "generated_inputs"
    generated_dir.mkdir(parents=True, exist_ok=True)
    generated_mapper_path = generated_dir / "mapper_config.json"
    write_json(generated_mapper_path, mapper_config)
    return generated_mapper_path


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


def build_run_plan(manifest: dict, filters: dict) -> list:
    benchmark_set_filter = set(filters["benchmark_set"])
    benchmark_filter = set(filters["benchmark"])
    arch_filter = set(filters["arch"])
    mapper_filter = set(filters["mapper"])
    run_plan = []

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
                for mapper in manifest["mappers"]:
                    mapper_name = mapper["name"]
                    if not selected(mapper_name, mapper_filter):
                        continue
                    run_plan.append(
                        {
                            "benchmark_set": benchmark_set_name,
                            "benchmark": benchmark,
                            "dfg": dfg,
                            "arch": arch,
                            "mapper": mapper,
                        }
                    )
    return run_plan


def validate_run_plan_for_problem_type(run_plan: list, problem_type: str) -> None:
    if problem_type != "placement2d":
        return
    issues = []
    for item in run_plan:
        arch = item["arch"]
        arch_ii = placement2d_arch_ii(arch)
        if arch_ii != 1:
            issues.append(
                f"{item['benchmark_set']}/{item['benchmark']} | {arch['name']}: "
                "placement2d requires explicit ii/context_size/ii_max = 1"
            )
        has_overrides = any(
            key in arch
            for key in (
                "memory_io",
                "network_type",
                "CGRA_type",
                "local_reg_size",
                "placement_cost_model",
            )
        )
        if arch_has_auto_grid(arch) or has_overrides:
            capacity = placement2d_capacity_check_for_arch(
                item["dfg"],
                effective_arch_dict_for_plan_item(item),
            )
        else:
            capacity = placement2d_capacity_check(item["dfg"], Path(arch["template"]))
        if not capacity["ok"]:
            issues.append(
                f"{item['benchmark_set']}/{item['benchmark']} | {arch['name']}: "
                f"{capacity['detail']}"
            )
    if issues:
        issue_text = "\n".join(f"- {issue}" for issue in issues)
        raise ValueError(
            "placement2d manifest preflight failed before running mappings:\n"
            f"{issue_text}"
        )


def now_local() -> datetime:
    if LOCAL_TIMEZONE:
        return datetime.now(LOCAL_TIMEZONE)
    return datetime.now().astimezone()


def timestamp_for_dir(value: datetime) -> str:
    return value.strftime("%Y%m%d-%H%M%S")


def clean_tag(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in {"-", "_"} else "-" for ch in value)


def clean_group_path(value: str) -> Path:
    parts = [clean_tag(part) for part in value.split("/") if part]
    return Path(*parts) if parts else Path("uncategorized")


def format_duration(seconds: float) -> str:
    seconds = max(0.0, seconds)
    if seconds < 10:
        return f"{seconds:.1f}s"
    rounded_seconds = int(round(seconds))
    hours, remainder = divmod(rounded_seconds, 3600)
    minutes, secs = divmod(remainder, 60)
    if hours:
        return f"{hours}h{minutes:02d}m{secs:02d}s"
    if minutes:
        return f"{minutes}m{secs:02d}s"
    return f"{secs}s"


def should_use_color(setting: str) -> bool:
    if setting == "always":
        return True
    if setting == "never":
        return False
    return sys.stdout.isatty()


def colorize(text: object, color_enabled: bool, *styles: str) -> str:
    value = str(text)
    if not color_enabled:
        return value
    prefix = "".join(ANSI_STYLES[style] for style in styles if style in ANSI_STYLES)
    return f"{prefix}{value}{ANSI_STYLES['reset']}" if prefix else value


def status_color(status: str) -> str:
    if status in SUCCESS_STATUSES:
        return "green"
    if status in WARNING_STATUSES:
        return "yellow"
    if status in FAILURE_STATUSES:
        return "red"
    return "cyan"


def status_label(status: str) -> str:
    if status in SUCCESS_STATUSES:
        return "OK"
    if status == "skipped":
        return "SKIP"
    if status in WARNING_STATUSES:
        return "FEAS"
    if status in FAILURE_STATUSES:
        return "FAIL"
    return "INFO"


def field(label: str, value: object, color_enabled: bool, value_style: str = "") -> str:
    value_styles = (value_style,) if value_style else ()
    return f"{label}={colorize(value, color_enabled, *value_styles)}"


def progress_prefix(index: int, total: int, color_enabled: bool) -> str:
    width = max(len(str(total)), 1)
    return colorize(f"[{index:>{width}}/{total}]", color_enabled, "bold")


def resolve_result_dir(args: argparse.Namespace, manifest: dict, started_at: datetime) -> Path:
    if args.out:
        return args.out

    suffix = f"_{clean_tag(args.tag)}" if args.tag else ""
    result_group = manifest.get("result_group", args.manifest.stem)
    return args.out_root / clean_group_path(result_group) / f"{timestamp_for_dir(started_at)}{suffix}"


def run_command(args: list, cwd: Path) -> str:
    try:
        proc = subprocess.run(
            args,
            cwd=cwd,
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return ""
    if proc.returncode != 0:
        return ""
    return proc.stdout.strip()


def git_metadata(cwd: Path) -> dict:
    root = run_command(["git", "rev-parse", "--show-toplevel"], cwd)
    git_cwd = Path(root) if root else cwd
    status_short = run_command(["git", "status", "--short"], git_cwd)
    return {
        "root": root,
        "commit": run_command(["git", "rev-parse", "HEAD"], git_cwd),
        "branch": run_command(["git", "rev-parse", "--abbrev-ref", "HEAD"], git_cwd),
        "is_dirty": bool(status_short),
        "status_short": status_short,
    }


def build_metadata(
    args: argparse.Namespace,
    result_dir: Path,
    started_at: datetime,
    filters: dict,
) -> dict:
    return {
        "status": "running",
        "started_at": started_at.isoformat(timespec="seconds"),
        "ended_at": "",
        "duration_sec": "",
        "reports_generated": "",
        "manifest_path": str(args.manifest),
        "result_dir": str(result_dir),
        "command": sys.argv,
        "filters": filters,
        "python": sys.version.split()[0],
        "host": platform.node(),
        "git": git_metadata(Path.cwd()),
        "log_conventions": {
            "metrics_csv": "metrics.csv",
            "manifest_snapshot": "manifest.json",
            "summary_json": "summary.json",
            "per_mapper_summary": "set=<set>/benchmark=<benchmark>/arch=<arch>/mapper=<mapper>/summary.json",
            "per_ii_stdout": "set=<set>/benchmark=<benchmark>/arch=<arch>/mapper=<mapper>/tried_ii/ii_<II>/stdout.txt",
            "per_ii_stderr": "set=<set>/benchmark=<benchmark>/arch=<arch>/mapper=<mapper>/tried_ii/ii_<II>/stderr.txt",
            "raw_mapping_logs": "metrics.csv columns mapping_file, gurobi_log_file, run_dir, raw_output_dir",
        },
    }


def write_run_info(result_dir: Path, metadata: dict) -> None:
    git = metadata.get("git", {})
    filters = metadata.get("filters", {})
    lines = [
        "# Run Info",
        "",
        f"- status: `{metadata.get('status', '')}`",
        f"- started_at: `{metadata.get('started_at', '')}`",
        f"- ended_at: `{metadata.get('ended_at', '')}`",
        f"- duration_sec: `{metadata.get('duration_sec', '')}`",
        f"- manifest: `{metadata.get('manifest_path', '')}`",
        f"- result_dir: `{metadata.get('result_dir', '')}`",
        f"- problem_type: `{metadata.get('problem_type', '')}`",
        f"- evaluation_mode: `{metadata.get('evaluation_mode', '')}`",
        f"- reports_generated: `{metadata.get('reports_generated', '')}`",
        f"- git_commit: `{git.get('commit', '')}`",
        f"- git_branch: `{git.get('branch', '')}`",
        f"- git_dirty: `{git.get('is_dirty', '')}`",
        "",
        "## Filters",
        "",
    ]
    for key in ("benchmark_set", "benchmark", "arch", "mapper"):
        values = filters.get(key, [])
        lines.append(f"- {key}: `{', '.join(values) if values else 'all'}`")
    lines.extend(
        [
            "",
            "## Main Files",
            "",
            "- `metrics.csv`: normalized result rows and pointers to mapping/log files",
            "- `summary.json`: per benchmark/architecture/mapper summary",
            "- `manifest.json`: manifest snapshot plus run filters",
            "- `run_metadata.json`: command, timestamps, git state, and log conventions",
            "- `benchmark_report.md`, `summary_by_mapper.md`, `summary_by_arch.md`, `summary_by_set.md`, `validation.md`, `routing_validation.md`: generated by `generate_reports.py`",
            "",
            "## Log Lookup",
            "",
            "- Per-II stdout/stderr are in `trial_dir`, `stdout_file`, and `stderr_file` columns of `metrics.csv`.",
            "- Mapper output, mapping JSON, and Gurobi logs are in `run_dir`, `mapping_file`, and `gurobi_log_file` columns.",
            "",
        ]
    )
    if git.get("status_short"):
        lines.extend(["## Git Status", "", "```text", git["status_short"], "```", ""])
    (result_dir / "run_info.md").write_text("\n".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser(description="Run a research experiment manifest.")
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--out-root", default=Path("research/results"), type=Path)
    parser.add_argument("--tag", default="")
    parser.add_argument("--skip-reports", action="store_true")
    parser.add_argument("--quiet", action="store_true", help="Suppress progress output.")
    parser.add_argument("--verbose", action="store_true", help="Print per-II progress for each mapper run.")
    parser.add_argument(
        "--color",
        choices=["auto", "always", "never"],
        default="auto",
        help="Control ANSI colors in progress output.",
    )
    parser.add_argument("--only-benchmark-set", action="append", default=[])
    parser.add_argument("--only-benchmark", action="append", default=[])
    parser.add_argument("--only-arch", action="append", default=[])
    parser.add_argument("--only-mapper", action="append", default=[])
    args = parser.parse_args()
    color_enabled = should_use_color(args.color)

    started_at = now_local()
    started_monotonic = time.monotonic()
    manifest = load_json(args.manifest)
    problem_type = manifest_problem_type(manifest)
    result_dir = resolve_result_dir(args, manifest, started_at)
    filters = {
        "benchmark_set": sorted(parse_filter(args.only_benchmark_set)),
        "benchmark": sorted(parse_filter(args.only_benchmark)),
        "arch": sorted(parse_filter(args.only_arch)),
        "mapper": sorted(parse_filter(args.only_mapper)),
    }
    result_dir.mkdir(parents=True, exist_ok=True)
    metadata = build_metadata(args, result_dir, started_at, filters)
    metadata["problem_type"] = problem_type
    metadata["evaluation_mode"] = str(manifest.get("evaluation_mode", "routing"))
    write_json(result_dir / "run_metadata.json", metadata)
    write_run_info(result_dir, metadata)
    completed = {"ok": False}

    def close_unfinished_metadata() -> None:
        if completed["ok"]:
            return
        metadata["status"] = "failed_or_interrupted"
        metadata["ended_at"] = now_local().isoformat(timespec="seconds")
        metadata["duration_sec"] = round(time.monotonic() - started_monotonic, 3)
        write_json(result_dir / "run_metadata.json", metadata)
        write_run_info(result_dir, metadata)

    atexit.register(close_unfinished_metadata)

    manifest_log = dict(manifest)
    manifest_log["run_filter"] = filters
    write_json(result_dir / "manifest.json", manifest_log)

    all_rows = []
    summaries = []
    timeout_sec = float(manifest["timeout_sec"])
    parallel_num = int(manifest.get("parallel_num", 1))
    mapping_bin = Path(manifest["mapping_bin"])
    missing_distance_policy = manifest.get("mii_missing_distance_policy", "self_loop")
    run_plan = build_run_plan(manifest, filters)
    validate_run_plan_for_problem_type(run_plan, problem_type)
    evaluation_mode = str(manifest.get("evaluation_mode", "routing"))

    if not args.quiet:
        print(
            f"Result directory: {result_dir}",
            flush=True,
        )
        print(
            f"Planned runs: {len(run_plan)} mapper/benchmark/architecture combinations",
            flush=True,
        )
        print(f"Problem type: {problem_type}", flush=True)
        print(f"Evaluation mode: {evaluation_mode}", flush=True)
        if args.verbose:
            print(
                "Verbose progress: per-II attempts are shown.",
                flush=True,
            )

    for index, item in enumerate(run_plan, start=1):
        run_started_monotonic = time.monotonic()
        benchmark_set_name = item["benchmark_set"]
        benchmark = item["benchmark"]
        dfg = item["dfg"]
        arch = item["arch"]
        mapper = item["mapper"]
        arch_name = arch["name"]
        ii_max = 1 if problem_type == "placement2d" else int(arch["ii_max"])
        mii = str(arch.get("mii", "auto"))
        mapper_name = mapper["name"]
        runner = mapper_runner(mapper)
        run_dir = (
            result_dir
            / f"set={benchmark_set_name}"
            / f"benchmark={benchmark}"
            / f"arch={arch_name}"
            / f"mapper={mapper_name}"
        )
        arch_template = prepare_arch_template_for_run(item, run_dir)
        if args.verbose and not args.quiet:
            print(
                f"{progress_prefix(index, len(run_plan), color_enabled)} "
                f"START {benchmark_set_name}/{benchmark} | {arch_name} | {mapper_name}",
                flush=True,
            )
        if runner == "vpr":
            summary = run_one_vpr(
                dfg=dfg,
                arch_template=arch_template,
                output_dir=run_dir,
                benchmark=benchmark,
                mapper_name=mapper_name,
                arch_name=arch_name,
                timeout_sec=timeout_sec,
                mapper_config=mapper,
                problem_type=problem_type,
                evaluation_mode=evaluation_mode,
                progress=args.verbose and not args.quiet,
            )
        elif runner == "vpr_modulo":
            summary = run_one_vpr_modulo(
                dfg=dfg,
                arch_template=arch_template,
                output_dir=run_dir,
                benchmark=benchmark,
                mapper_name=mapper_name,
                arch_name=arch_name,
                mii=mii,
                ii_max=ii_max,
                timeout_sec=timeout_sec,
                mapper_config=mapper,
                missing_distance_policy=missing_distance_policy,
                progress=args.verbose and not args.quiet,
            )
        elif runner == "vpr_modulo_full_route":
            summary = run_one_vpr_modulo_full_route(
                dfg=dfg,
                arch_template=arch_template,
                output_dir=run_dir,
                benchmark=benchmark,
                mapper_name=mapper_name,
                arch_name=arch_name,
                mii=mii,
                ii_max=ii_max,
                timeout_sec=timeout_sec,
                mapper_config=mapper,
                missing_distance_policy=missing_distance_policy,
                progress=args.verbose and not args.quiet,
            )
        else:
            mapper_config = prepare_mapper_config_for_run(
                mapper,
                run_dir,
                evaluation_mode,
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
                problem_type=problem_type,
                evaluation_mode=evaluation_mode,
                progress=args.verbose and not args.quiet,
            )
        summary["benchmark_set"] = benchmark_set_name
        summaries.append(summary)
        if not args.quiet:
            achieved = summary.get("achieved_II")
            achieved_text = achieved if achieved is not None else "-"
            run_elapsed = time.monotonic() - run_started_monotonic
            total_elapsed = time.monotonic() - started_monotonic
            remaining_runs = len(run_plan) - index
            eta_seconds = (total_elapsed / index) * remaining_runs if index else 0
            status = str(summary.get("status", "unknown"))
            status_style = status_color(status)
            label = status_label(status)
            print(
                f"{progress_prefix(index, len(run_plan), color_enabled)} "
                f"{colorize(f'{label:<4}', color_enabled, 'bold', status_style)} "
                f"{benchmark_set_name}/{benchmark} | {arch_name} | {mapper_name} | "
                f"{field('II', achieved_text, color_enabled, 'bold')} "
                f"{field('status', status, color_enabled)} | "
                f"{field('run', format_duration(run_elapsed), color_enabled)} "
                f"{field('elapsed', format_duration(total_elapsed), color_enabled)} "
                f"{field('eta', format_duration(eta_seconds), color_enabled)}",
                flush=True,
            )
        metrics_path = run_dir / "metrics.csv"
        if metrics_path.exists():
            with metrics_path.open() as f:
                for row in csv.DictReader(f):
                    row["benchmark_set"] = benchmark_set_name
                    all_rows.append(row)

    write_json(result_dir / "summary.json", {"runs": summaries})
    write_metrics_csv(result_dir / "metrics.csv", all_rows)
    reports_generated = False
    if not args.skip_reports:
        subprocess.run(
            [
                sys.executable,
                str(Path(__file__).resolve().parent / "generate_reports.py"),
                "--result-dir",
                str(result_dir),
            ],
            check=True,
        )
        reports_generated = True
    metadata["status"] = "completed"
    metadata["ended_at"] = now_local().isoformat(timespec="seconds")
    metadata["duration_sec"] = round(time.monotonic() - started_monotonic, 3)
    metadata["run_count"] = len(summaries)
    metadata["metric_row_count"] = len(all_rows)
    metadata["reports_generated"] = reports_generated
    write_json(result_dir / "run_metadata.json", metadata)
    write_run_info(result_dir, metadata)
    completed["ok"] = True
    if not args.quiet:
        print(
            f"Completed. Result directory: {result_dir}",
            flush=True,
        )
    else:
        print(f"Result directory: {result_dir}", flush=True)


if __name__ == "__main__":
    main()
