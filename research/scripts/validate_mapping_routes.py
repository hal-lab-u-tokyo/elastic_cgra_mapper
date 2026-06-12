#!/usr/bin/env python3

import argparse
import csv
import json
import re
from collections import defaultdict, deque
from pathlib import Path
from typing import Optional


SUCCESS_STATUSES = {"success", "optimal", "timeout_feasible"}
ROUTE_OP = "route"
NOP_OP = "nop"


def clean_value(value) -> str:
    return str(value).strip().strip('"')


def parse_config_id(value: dict) -> tuple:
    return (
        int(value["row_id"]),
        int(value["column_id"]),
        int(value["context_id"]),
    )


def config_id_list(value) -> list:
    if not value or isinstance(value, str):
        return []
    if isinstance(value, dict):
        return [parse_config_id(value)]
    return [parse_config_id(item) for item in value]


def parse_dot(dfg_path: Path) -> tuple:
    nodes = {}
    edges = []
    node_pattern = re.compile(r'^\s*"?([^"\s\[]+)"?\s*\[(.+)\]\s*;?')
    edge_pattern = re.compile(r'^\s*"?([^"\s\[]+)"?\s*->\s*"?([^"\s\[]+)"?')
    opcode_pattern = re.compile(r'opcode\s*=\s*"?([^",\]\s]+)"?')

    for line in dfg_path.read_text().splitlines():
        edge_match = edge_pattern.match(line)
        if edge_match:
            edges.append((clean_value(edge_match.group(1)), clean_value(edge_match.group(2))))
            continue

        node_match = node_pattern.match(line)
        if not node_match:
            continue
        opcode_match = opcode_pattern.search(node_match.group(2))
        if opcode_match:
            nodes[clean_value(node_match.group(1))] = clean_value(opcode_match.group(1))

    return nodes, edges


def input_log_dfg(row: dict) -> Optional[Path]:
    run_dir = Path(row.get("run_dir", ""))
    if not run_dir.exists():
        return None
    input_logs = sorted(run_dir.glob("input_log_*.json"))
    if not input_logs:
        return None
    with input_logs[0].open() as f:
        dfg_file = json.load(f).get("dfg_file", "")
    if not dfg_file:
        return None
    dfg_path = Path(dfg_file)
    if dfg_path.exists():
        return dfg_path
    copied = sorted(run_dir.glob("*.dot"))
    return copied[0] if copied else None


def fallback_dfg(row: dict, repo_root: Path) -> Optional[Path]:
    benchmark = row.get("benchmark", "")
    if not benchmark:
        return None
    candidates = [
        repo_root / "benchmark" / "kernel" / f"{benchmark}.dot",
        repo_root / "benchmark" / "cgrame_kernel" / f"{benchmark}.dot",
    ]
    candidates.extend((repo_root / "benchmark" / "parallel").glob(f"**/{benchmark}.dot"))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def resolve_dfg(row: dict, repo_root: Path) -> Optional[Path]:
    return input_log_dfg(row) or fallback_dfg(row, repo_root)


def load_mapping(mapping_path: Path) -> dict:
    with mapping_path.open() as f:
        return json.load(f)


def build_mapping_graph(mapping: dict) -> tuple:
    configs = {}
    outgoing = defaultdict(list)
    incoming = defaultdict(list)
    out_of_range_edges = []
    invalid_connection_edges = []
    connection_stats = {
        "total": 0,
        "same_context": 0,
        "cross_context": 0,
    }

    rows = int(mapping["row"])
    cols = int(mapping["column"])
    context_size = int(mapping["context_size"])
    network_type = mapping.get("network_type", "")
    cgra_type = mapping.get("cgra_type", "default")

    for pe in mapping.get("PE_config", []):
        row = int(pe["row_id"])
        col = int(pe["column_id"])
        for config in pe.get("config", []):
            context = int(config["context_id"])
            config_id = (row, col, context)
            configs[config_id] = {
                "operation_type": clean_value(config.get("operation_type", "")),
                "operation_name": clean_value(config.get("operation_name", "")),
                "to": config_id_list(config.get("to_config_id", [])),
                "from": config_id_list(config.get("from_config_id", [])),
            }

    def in_bounds(config_id: tuple) -> bool:
        row, col, context = config_id
        return 0 <= row < rows and 0 <= col < cols and 0 <= context < context_size

    def spatially_adjacent(src: tuple, dst: tuple) -> bool:
        row_delta = abs(src[0] - dst[0])
        col_delta = abs(src[1] - dst[1])
        if row_delta == 0 and col_delta == 0:
            return True
        if network_type == "orthogonal":
            return row_delta + col_delta == 1
        return max(row_delta, col_delta) == 1

    def legal_mrrg_connection(src: tuple, dst: tuple) -> bool:
        if not spatially_adjacent(src, dst):
            return False
        next_context = (src[2] + 1) % context_size
        same_pe = src[0] == dst[0] and src[1] == dst[1]
        if cgra_type == "elastic":
            return dst[2] == next_context if same_pe else True
        return dst[2] == next_context

    for src, config in configs.items():
        for dst in config["to"]:
            connection_stats["total"] += 1
            if src[2] == dst[2]:
                connection_stats["same_context"] += 1
            else:
                connection_stats["cross_context"] += 1
            outgoing[src].append(dst)
            incoming[dst].append(src)
            if dst not in configs or not in_bounds(dst):
                out_of_range_edges.append((src, dst))
            elif not legal_mrrg_connection(src, dst):
                invalid_connection_edges.append((src, dst))

    return configs, outgoing, incoming, out_of_range_edges, invalid_connection_edges, connection_stats


def find_valid_route_path(src_ids: list, dst_ids: list, configs: dict, outgoing: dict) -> list:
    dst_set = set(dst_ids)
    for start in src_ids:
        queue = deque([(start, [start])])
        seen = {start}
        while queue:
            current, path = queue.popleft()
            for nxt in outgoing.get(current, []):
                if nxt in dst_set:
                    return path + [nxt]
                if nxt in seen or nxt not in configs:
                    continue
                if configs[nxt]["operation_type"] != ROUTE_OP:
                    continue
                seen.add(nxt)
                queue.append((nxt, path + [nxt]))
    return []


def path_crosses_context(path: list) -> bool:
    return any(src[2] != dst[2] for src, dst in zip(path, path[1:]))


def validate_row(row: dict, row_id: int, repo_root: Path) -> tuple:
    failures = []
    warnings = []
    stats = {
        "checked_edges": 0,
        "route_paths": 0,
        "same_context_route_paths": 0,
        "cross_context_route_paths": 0,
        "mapping_connections": 0,
        "same_context_connections": 0,
        "cross_context_connections": 0,
    }
    prefix = (
        f"row {row_id} ({row.get('benchmark', '')}/{row.get('mapper', '')}/"
        f"II={row.get('achieved_II', '')})"
    )

    mapping_file = row.get("mapping_file", "")
    if not mapping_file:
        failures.append(f"{prefix}: successful row has no mapping_file")
        return failures, warnings, stats

    mapping_path = Path(mapping_file)
    if not mapping_path.exists():
        failures.append(f"{prefix}: mapping_file does not exist: {mapping_file}")
        return failures, warnings, stats

    dfg_path = resolve_dfg(row, repo_root)
    if dfg_path is None or not dfg_path.exists():
        failures.append(f"{prefix}: DFG file could not be resolved")
        return failures, warnings, stats

    dfg_nodes, dfg_edges = parse_dot(dfg_path)
    mapping = load_mapping(mapping_path)
    (
        configs,
        outgoing,
        incoming,
        out_of_range_edges,
        invalid_connection_edges,
        connection_stats,
    ) = build_mapping_graph(mapping)
    stats["mapping_connections"] += connection_stats["total"]
    stats["same_context_connections"] += connection_stats["same_context"]
    stats["cross_context_connections"] += connection_stats["cross_context"]

    if out_of_range_edges:
        failures.append(f"{prefix}: {len(out_of_range_edges)} mapping connections point outside the CGRA")
    if invalid_connection_edges:
        failures.append(f"{prefix}: {len(invalid_connection_edges)} mapping connections are not legal MRRG edges")

    for src, dsts in outgoing.items():
        for dst in dsts:
            if src in configs and dst in configs and src not in configs[dst]["from"]:
                failures.append(f"{prefix}: missing reciprocal from_config_id for {src} -> {dst}")
                break

    op_locations = defaultdict(list)
    op_types = {}
    for config_id, config in configs.items():
        op_type = config["operation_type"]
        op_name = config["operation_name"]
        if op_type in {NOP_OP, ROUTE_OP}:
            continue
        op_locations[op_name].append(config_id)
        op_types[op_name] = op_type

    for node_name, opcode in dfg_nodes.items():
        locations = op_locations.get(node_name, [])
        if not locations:
            failures.append(f"{prefix}: DFG node `{node_name}` is not placed in mapping")
            continue
        if len(locations) > 1:
            failures.append(f"{prefix}: DFG node `{node_name}` is placed {len(locations)} times")
        if op_types.get(node_name) != opcode:
            failures.append(
                f"{prefix}: DFG node `{node_name}` opcode mismatch "
                f"(dfg={opcode}, mapping={op_types.get(node_name)})"
            )

    for src, dst in dfg_edges:
        stats["checked_edges"] += 1
        if src not in op_locations or dst not in op_locations:
            continue
        route_path = find_valid_route_path(op_locations[src], op_locations[dst], configs, outgoing)
        if not route_path:
            failures.append(f"{prefix}: no route path for DFG edge `{src}` -> `{dst}`")
            continue
        stats["route_paths"] += 1
        if path_crosses_context(route_path):
            stats["cross_context_route_paths"] += 1
        else:
            stats["same_context_route_paths"] += 1

    mapped_extra_ops = sorted(set(op_locations) - set(dfg_nodes))
    if mapped_extra_ops:
        warnings.append(f"{prefix}: mapping contains non-DFG op names: {', '.join(mapped_extra_ops[:8])}")

    return failures, warnings, stats


def add_items(lines: list, title: str, items: list) -> None:
    lines.extend([f"## {title}", ""])
    if not items:
        lines.extend(["None.", ""])
        return
    for item in items:
        lines.append(f"- {item}")
    lines.append("")


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate that mapped routes connect every DFG edge.")
    parser.add_argument("--metrics", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--repo-root", default=Path("."), type=Path)
    args = parser.parse_args()

    rows = []
    skipped_placement_only = 0
    with args.metrics.open() as f:
        for row in csv.DictReader(f):
            if row.get("evaluation_mode") == "placement_only":
                skipped_placement_only += 1
                continue
            if row.get("status") in SUCCESS_STATUSES:
                rows.append(row)

    failures = []
    warnings = []
    totals = {
        "checked_edges": 0,
        "route_paths": 0,
        "same_context_route_paths": 0,
        "cross_context_route_paths": 0,
        "mapping_connections": 0,
        "same_context_connections": 0,
        "cross_context_connections": 0,
    }
    for idx, row in enumerate(rows, start=1):
        row_failures, row_warnings, row_stats = validate_row(row, idx, args.repo_root)
        failures.extend(row_failures)
        warnings.extend(row_warnings)
        for key, value in row_stats.items():
            totals[key] += value

    lines = ["# Routing Validation", ""]
    lines.append(f"Placement-only rows skipped: {skipped_placement_only}")
    lines.append(f"Successful mapping rows checked: {len(rows)}")
    lines.append(f"DFG edges checked: {totals['checked_edges']}")
    lines.append(f"Route paths found: {totals['route_paths']}")
    lines.append(
        "Route paths by context: "
        f"same={totals['same_context_route_paths']}, "
        f"cross={totals['cross_context_route_paths']}"
    )
    lines.append(
        "Mapping connections by context: "
        f"same={totals['same_context_connections']}, "
        f"cross={totals['cross_context_connections']}"
    )
    lines.append(f"Overall status: {'FAIL' if failures else 'PASS'}")
    lines.append("")
    add_items(lines, "Failures", failures)
    add_items(lines, "Warnings", warnings)
    lines.extend(
        [
            "## Checks",
            "",
            "- Rows with `evaluation_mode=placement_only` are skipped because they intentionally do not claim routed connectivity.",
            "- Every successful row must have an existing `mapping_file` and resolvable DFG `.dot` file.",
            "- Every DFG operation must appear exactly once in the mapping with the expected opcode.",
            "- Every mapping connection must stay inside the CGRA and match a legal MRRG edge, including context transitions.",
            "- Every `to_config_id` connection must have a reciprocal `from_config_id` entry.",
            "- Every DFG edge must be reachable from its source operation to its destination operation through zero or more `route` configs.",
            "",
        ]
    )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
