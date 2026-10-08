#!/usr/bin/env python3

import argparse
from pathlib import Path

from lib import load_json, resolve_repo_path
from run_suite import expand_benchmark_sets, manifest_mappers, manifest_problem_type


def join_values(values: list[str]) -> str:
    return ", ".join(values) if values else "-"


def arch_summary(arch: dict) -> str:
    parts = []
    if "template" in arch:
        parts.append(f"template={Path(arch['template']).name}")
    if "auto_grid" in arch:
        auto_grid = arch["auto_grid"]
        if isinstance(auto_grid, dict):
            parts.append(f"auto_grid={auto_grid.get('policy', '')}")
        else:
            parts.append(f"auto_grid={auto_grid}")
    for key in ("memory_io", "network_type", "ii", "mii", "ii_max"):
        if key in arch:
            parts.append(f"{key}={arch[key]}")
    return ", ".join(parts)


def mapper_summary(mapper: dict) -> str:
    parts = []
    if "runner" in mapper:
        parts.append(f"runner={mapper['runner']}")
    elif "mapper_config" in mapper:
        parts.append(f"config={Path(mapper['mapper_config']).name}")
    for key in ("mapper_role", "placement_method", "routing_method"):
        if key in mapper:
            parts.append(f"{key}={mapper[key]}")
    return ", ".join(parts)


def display_path(path: Path, repo_root: Path) -> str:
    try:
        return path.relative_to(repo_root).as_posix()
    except ValueError:
        return path.as_posix()


def print_manifest_options(manifest_path: Path, manifest_path_text: str,
                           manifest: dict) -> None:
    print(f"# {manifest.get('name', manifest_path.stem)}")
    print(f"manifest: {manifest_path_text}")
    print(f"problem_type: {manifest_problem_type(manifest)}")
    print(f"evaluation_mode: {manifest.get('evaluation_mode', 'routing')}")
    print("")

    print("benchmark sets:")
    for benchmark_set in expand_benchmark_sets(manifest):
        benchmarks = list(benchmark_set["benchmarks"])
        print(f"  {benchmark_set['name']} ({len(benchmarks)}): {join_values(benchmarks)}")
    print("")

    print("architectures:")
    for arch in manifest.get("architectures", []):
        print(f"  {arch['name']}: {arch_summary(arch)}")
    print("")

    print("mappers:")
    for mapper in manifest_mappers(manifest):
        print(f"  {mapper['name']}: {mapper_summary(mapper)}")
    print("")

    print("filters:")
    print("  --only-benchmark-set <name>[,<name>]")
    print("  --only-benchmark <name>[,<name>]")
    print("  --only-arch <name>[,<name>]")
    print("  --only-mapper <name>[,<name>]")


def first_example_values(manifest: dict) -> dict:
    benchmark_sets = expand_benchmark_sets(manifest)
    first_set = benchmark_sets[0] if benchmark_sets else {"name": "", "benchmarks": [""]}
    architectures = manifest.get("architectures", [{"name": ""}])
    mappers = manifest_mappers(manifest)
    return {
        "benchmark_set": first_set["name"],
        "benchmark": first_set["benchmarks"][0] if first_set["benchmarks"] else "",
        "arch": architectures[0].get("name", ""),
        "mapper": mappers[0].get("name", "") if mappers else "",
    }


def print_command_templates(manifest_path_text: str, values: dict) -> None:
    print("")
    print("preflight:")
    print(
        "python3 research/scripts/preflight_manifest.py \\\n"
        f"  --manifest {manifest_path_text} \\\n"
        "  --out-dir /tmp/cgra_mapper_preflight \\\n"
        f"  --only-benchmark-set {values['benchmark_set']} \\\n"
        f"  --only-benchmark {values['benchmark']} \\\n"
        f"  --only-arch {values['arch']} \\\n"
        f"  --only-mapper {values['mapper']}"
    )
    print("")
    print("run:")
    print(
        "python3 research/scripts/run_suite.py \\\n"
        f"  --manifest {manifest_path_text} \\\n"
        f"  --only-benchmark-set {values['benchmark_set']} \\\n"
        f"  --only-benchmark {values['benchmark']} \\\n"
        f"  --only-arch {values['arch']} \\\n"
        f"  --only-mapper {values['mapper']} \\\n"
        "  --tag trial"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="List manifest choices and filter-ready commands.")
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--repo-root", default=Path.cwd(), type=Path)
    parser.add_argument("--no-commands", action="store_true")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    manifest_path = resolve_repo_path(args.manifest, repo_root)
    manifest = load_json(manifest_path)
    manifest_path_text = display_path(manifest_path, repo_root)

    print_manifest_options(manifest_path, manifest_path_text, manifest)
    if not args.no_commands:
        print_command_templates(manifest_path_text, first_example_values(manifest))


if __name__ == "__main__":
    main()
