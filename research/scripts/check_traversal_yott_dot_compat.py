#!/usr/bin/env python3

import argparse
import csv
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Tuple


@dataclass
class DotGraph:
    path: Path
    node_order: List[str] = field(default_factory=list)
    node_attrs: Dict[str, Dict[str, str]] = field(default_factory=dict)
    edges: List[Tuple[str, str]] = field(default_factory=list)

    @property
    def inputs(self) -> List[str]:
        return sorted(
            node
            for node in self.node_order
            if all(dst != node for _, dst in self.edges)
        )

    @property
    def outputs(self) -> List[str]:
        return sorted(
            node
            for node in self.node_order
            if all(src != node for src, _ in self.edges)
        )

    @property
    def op_inputs(self) -> List[str]:
        return sorted(
            node
            for node, attrs in self.node_attrs.items()
            if attrs.get("op", "").strip('"') in {"in", "input"}
        )

    @property
    def op_outputs(self) -> List[str]:
        return sorted(
            node
            for node, attrs in self.node_attrs.items()
            if attrs.get("op", "").strip('"') in {"out", "output"}
        )


NODE_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*|\d+)\s*\[(.*)\]\s*;?\s*$")
EDGE_RE = re.compile(
    r"^\s*([A-Za-z_][A-Za-z0-9_]*|\d+)\s*->\s*"
    r"([A-Za-z_][A-Za-z0-9_]*|\d+)\s*(?:\[(.*)\])?\s*;?\s*$"
)
ATTR_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(\"[^\"]*\"|[^,\]\s]+)")


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[2]


def strip_line_comment(line: str) -> str:
    if "//" not in line:
        return line
    return line.split("//", 1)[0]


def parse_attrs(raw: str) -> Dict[str, str]:
    attrs = {}
    for key, value in ATTR_RE.findall(raw or ""):
        attrs[key] = value.strip().strip('"')
    return attrs


def parse_dot(path: Path) -> DotGraph:
    graph = DotGraph(path=path)
    for line_no, raw_line in enumerate(path.read_text().splitlines(), start=1):
        line = strip_line_comment(raw_line).strip()
        if not line:
            continue
        if line.startswith("#"):
            continue
        if line.startswith("digraph ") or line in {"{", "}"}:
            continue

        edge_match = EDGE_RE.match(line)
        if edge_match:
            graph.edges.append((edge_match.group(1), edge_match.group(2)))
            continue

        node_match = NODE_RE.match(line)
        if node_match:
            node = node_match.group(1)
            if node not in graph.node_attrs:
                graph.node_order.append(node)
            graph.node_attrs[node] = parse_attrs(node_match.group(2))
            continue

        raise ValueError(f"{path}:{line_no}: unsupported DOT statement: {raw_line}")
    return graph


def first_diff(left: List, right: List) -> str:
    if left == right:
        return ""
    limit = min(len(left), len(right))
    for index in range(limit):
        if left[index] != right[index]:
            return f"index {index}: cpu={left[index]!r}, repo={right[index]!r}"
    return f"length mismatch: cpu={len(left)}, repo={len(right)}"


def compare_graphs(set_name: str, benchmark: str, cpu_path: Path, repo_path: Path) -> dict:
    if not cpu_path.exists():
        return {
            "set": set_name,
            "benchmark": benchmark,
            "status": "FAIL",
            "details": f"missing CPU mapping file: {cpu_path}",
        }
    if not repo_path.exists():
        return {
            "set": set_name,
            "benchmark": benchmark,
            "status": "FAIL",
            "details": f"missing repo file: {repo_path}",
        }

    cpu = parse_dot(cpu_path)
    repo = parse_dot(repo_path)
    checks = {
        "node_count_match": len(cpu.node_order) == len(repo.node_order),
        "edge_count_match": len(cpu.edges) == len(repo.edges),
        "node_order_match": cpu.node_order == repo.node_order,
        "edge_order_match": cpu.edges == repo.edges,
        "input_set_match": cpu.inputs == repo.inputs,
        "output_set_match": cpu.outputs == repo.outputs,
        "op_input_set_match": cpu.op_inputs == repo.op_inputs,
        "op_output_set_match": cpu.op_outputs == repo.op_outputs,
    }
    details = []
    if not checks["node_order_match"]:
        details.append("node_order " + first_diff(cpu.node_order, repo.node_order))
    if not checks["edge_order_match"]:
        details.append("edge_order " + first_diff(cpu.edges, repo.edges))
    if not checks["input_set_match"]:
        details.append(f"inputs cpu={cpu.inputs}, repo={repo.inputs}")
    if not checks["output_set_match"]:
        details.append(f"outputs cpu={cpu.outputs}, repo={repo.outputs}")
    if not checks["op_input_set_match"]:
        details.append(f"op_inputs cpu={cpu.op_inputs}, repo={repo.op_inputs}")
    if not checks["op_output_set_match"]:
        details.append(f"op_outputs cpu={cpu.op_outputs}, repo={repo.op_outputs}")

    return {
        "set": set_name,
        "benchmark": benchmark,
        "status": "PASS" if all(checks.values()) else "FAIL",
        **checks,
        "cpu_nodes": len(cpu.node_order),
        "repo_nodes": len(repo.node_order),
        "cpu_edges": len(cpu.edges),
        "repo_edges": len(repo.edges),
        "cpu_inputs": " ".join(cpu.inputs),
        "repo_inputs": " ".join(repo.inputs),
        "cpu_outputs": " ".join(cpu.outputs),
        "repo_outputs": " ".join(repo.outputs),
        "details": "; ".join(details),
    }


def write_csv(path: Path, rows: List[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "set",
        "benchmark",
        "status",
        "node_count_match",
        "edge_count_match",
        "node_order_match",
        "edge_order_match",
        "input_set_match",
        "output_set_match",
        "op_input_set_match",
        "op_output_set_match",
        "cpu_nodes",
        "repo_nodes",
        "cpu_edges",
        "repo_edges",
        "cpu_inputs",
        "repo_inputs",
        "cpu_outputs",
        "repo_outputs",
        "details",
    ]
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow({key: row.get(key, "") for key in fieldnames})


def write_markdown(path: Path, rows: List[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    passed = sum(1 for row in rows if row["status"] == "PASS")
    lines = [
        "# Traversal/YOTT DOT Compatibility Report",
        "",
        "- CPU mapping root: external path supplied at runtime",
        "- cgra_mapper root: current repository",
        f"- Passed: {passed}/{len(rows)}",
        "",
        "This check compares node count, edge count, node order, edge order, source/sink input and output sets, and `op=in/out` node sets between the author `cpu_mapping` DOT files and the copied `benchmark/literature/traversal_yott` DOT files.",
        "",
        "| Set | Benchmark | Status | Nodes | Edges | Inputs | Outputs | Details |",
        "| --- | --- | --- | ---: | ---: | --- | --- | --- |",
    ]
    for row in rows:
        details = row.get("details") or ""
        lines.append(
            "| {set} | {benchmark} | {status} | {repo_nodes} | {repo_edges} | `{repo_inputs}` | `{repo_outputs}` | {details} |".format(
                **row
            )
        )
    path.write_text("\n".join(lines) + "\n")


def collect_rows(args) -> List[dict]:
    repo_root = args.repo_root.resolve()
    cpu_root = args.cpu_mapping_root.resolve()
    rows = []
    for set_name in args.sets:
        repo_dir = repo_root / "benchmark" / "literature" / "traversal_yott" / set_name / args.subset
        cpu_dir = cpu_root / "bench" / set_name / args.subset
        repo_files = sorted(repo_dir.glob("*.dot"))
        cpu_files = sorted(cpu_dir.glob("*.dot"))
        names = sorted({path.name for path in repo_files} | {path.name for path in cpu_files})
        for name in names:
            rows.append(
                compare_graphs(
                    set_name,
                    Path(name).stem,
                    cpu_dir / name,
                    repo_dir / name,
                )
            )
    return rows


def main() -> int:
    repo_root = repo_root_from_script()
    parser = argparse.ArgumentParser(
        description="Check copied Traversal/YOTT DOT files against the author cpu_mapping DOT files."
    )
    parser.add_argument("--repo-root", type=Path, default=repo_root)
    parser.add_argument(
        "--cpu-mapping-root",
        type=Path,
        default=os.environ.get("CPU_MAPPING_ROOT"),
        required=os.environ.get("CPU_MAPPING_ROOT") is None,
        help="Path to the external cpu_mapping repository. Can also be set with CPU_MAPPING_ROOT.",
    )
    parser.add_argument("--sets", nargs="+", default=["lisa", "m_bench"])
    parser.add_argument("--subset", default="dac")
    parser.add_argument(
        "--report",
        type=Path,
        default=repo_root
        / "benchmark"
        / "literature"
        / "traversal_yott"
        / "dot_compatibility_report.md",
    )
    parser.add_argument(
        "--csv",
        type=Path,
        default=repo_root
        / "benchmark"
        / "literature"
        / "traversal_yott"
        / "dot_compatibility.csv",
    )
    parser.add_argument("--allow-mismatch", action="store_true")
    args = parser.parse_args()

    rows = collect_rows(args)
    write_csv(args.csv, rows)
    write_markdown(args.report, rows)

    passed = sum(1 for row in rows if row["status"] == "PASS")
    failed = len(rows) - passed
    print(f"Checked {len(rows)} DOT files: {passed} passed, {failed} failed")
    print(f"Report: {args.report}")
    print(f"CSV: {args.csv}")
    if failed and not args.allow_mismatch:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
