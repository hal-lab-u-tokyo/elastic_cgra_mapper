#!/usr/bin/env python3

import argparse
import json
import random
import subprocess
import time
import xml.etree.ElementTree as ET
from collections import deque
from pathlib import Path

import networkx as nx

from lib import (
    clean_dot_value,
    clean_node_name,
    compute_res_mii,
    load_json,
    make_arch_for_ii,
    normalize_run,
    write_json,
    write_metrics_csv,
)
from run_vpr_baseline import (
    expand_env,
    find_net_file,
    find_place_file,
    parse_packed_net_positions,
    parse_place_file,
    prepare_vpr_arch_xml,
    recover_buffer_like_positions,
    unresolved_env,
    unique_blif_names,
    vpr_command,
    write_blif,
)


MEMORY_OPS = {"load", "store", "output"}
ROUTE_OP = "route"
NOP_OP = "nop"
SUCCESS_STATUSES = {"success"}


def read_dfg_ops_and_edges(dfg_path: Path) -> tuple:
    graph = nx.nx_pydot.read_dot(str(dfg_path))
    nodes = []
    ops = {}
    const_values = {}
    node_set = set()
    for node_name, attrs in graph.nodes(data=True):
        clean_name = clean_node_name(node_name)
        if clean_name in {"", "\\n", "\n"}:
            continue
        opcode = clean_dot_value(attrs.get("opcode", ""))
        if not opcode:
            continue
        nodes.append(clean_name)
        node_set.add(clean_name)
        ops[clean_name] = opcode
        value = clean_dot_value(attrs.get("value", ""))
        if value:
            try:
                const_values[clean_name] = int(value)
            except ValueError:
                const_values[clean_name] = 0

    edges = []
    for src, dst in graph.edges():
        clean_src = clean_node_name(src)
        clean_dst = clean_node_name(dst)
        if clean_src in node_set and clean_dst in node_set:
            edges.append((clean_src, clean_dst))
    return nodes, ops, const_values, edges


def supported_ops_for_position(row: int, col: int, arch: dict) -> set:
    memory_io = str(arch.get("memory_io", "all"))
    rows = int(arch["row"])
    cols = int(arch["column"])
    all_ops = {
        "add",
        "fadd",
        "sub",
        "fsub",
        "mul",
        "fmul",
        "div",
        "sdiv",
        "fdiv",
        "const",
        "load",
        "output",
        "store",
        "nop",
        "route",
        "or",
        "shift",
        "icmp",
        "cmpgt",
        "cmpge",
        "cmpeq",
        "select",
    }
    if memory_io == "all":
        return all_ops
    is_perimeter = row == 0 or col == 0 or row == rows - 1 or col == cols - 1
    is_corner = (row == 0 or row == rows - 1) and (col == 0 or col == cols - 1)
    if memory_io in {"perimeter", "border"} and is_perimeter:
        return all_ops
    if memory_io in {"perimeter_no_corners", "border_no_corners"} and is_perimeter and not is_corner:
        return all_ops
    if memory_io == "both_ends" and (col == 0 or col == cols - 1):
        return all_ops
    if memory_io == "one_end" and col == 0:
        return all_ops
    return all_ops - MEMORY_OPS


def adjacent_nodes(node: tuple, arch: dict) -> list:
    row, col, ctx = node
    rows = int(arch["row"])
    cols = int(arch["column"])
    ii = int(arch["context_size"])
    next_ctx = (ctx + 1) % ii
    network_type = str(arch.get("network_type", "orthogonal"))
    deltas = []
    if network_type in {"one_hop_axis2", "one_hop", "1hop"}:
        deltas = [(0, 1), (0, 2), (1, 0), (2, 0), (0, -1), (0, -2), (-1, 0), (-2, 0)]
    else:
        for dr in (-1, 0, 1):
            for dc in (-1, 0, 1):
                if dr == 0 and dc == 0:
                    continue
                if network_type == "orthogonal" and abs(dr) + abs(dc) > 1:
                    continue
                deltas.append((dr, dc))
    result = []
    for dr, dc in deltas:
        nr = row + dr
        nc = col + dc
        if 0 <= nr < rows and 0 <= nc < cols:
            result.append((nr, nc, next_ctx))
    result.append((row, col, next_ctx))
    return result


def edge_key(src: tuple, dst: tuple) -> tuple:
    return src + dst


def route_one_edge(source_name: str, source: tuple, target: tuple, op_occupancy: dict, route_node_owner: dict, route_edge_owner: dict, arch: dict):
    q = deque([source])
    parent = {source: None}
    while q:
        current = q.popleft()
        for nxt in adjacent_nodes(current, arch):
            ekey = edge_key(current, nxt)
            owner = route_edge_owner.get(ekey)
            if owner is not None and owner != source_name:
                continue
            if nxt != target and nxt in op_occupancy:
                continue
            if nxt != source and nxt != target and ROUTE_OP not in supported_ops_for_position(nxt[0], nxt[1], arch):
                continue
            node_owner = route_node_owner.get(nxt)
            if nxt != source and nxt != target and node_owner is not None and node_owner != source_name:
                continue
            if nxt in parent:
                continue
            parent[nxt] = current
            if nxt == target:
                path = [target]
                while path[-1] != source:
                    path.append(parent[path[-1]])
                path.reverse()
                return path
            q.append(nxt)
    return None


def route_with_contexts(nodes: list, ops: dict, positions: dict, contexts: dict, edges: list, arch: dict, route_order: list):
    op_locations = {}
    op_occupancy = {}
    for node in nodes:
        if node not in positions:
            return None
        row, col = positions[node]
        if row < 0 or col < 0 or row >= int(arch["row"]) or col >= int(arch["column"]):
            return None
        if ops[node] not in supported_ops_for_position(row, col, arch):
            return None
        loc = (row, col, int(contexts[node]) % int(arch["context_size"]))
        if loc in op_occupancy:
            return None
        op_locations[node] = loc
        op_occupancy[loc] = node

    route_node_owner = {}
    route_edge_owner = {}
    paths = []
    for src, dst in route_order:
        path = route_one_edge(src, op_locations[src], op_locations[dst], op_occupancy, route_node_owner, route_edge_owner, arch)
        if path is None:
            return None
        for a, b in zip(path, path[1:]):
            route_edge_owner.setdefault(edge_key(a, b), src)
            if b != op_locations[src] and b != op_locations[dst] and b not in op_occupancy:
                route_node_owner.setdefault(b, src)
        paths.append((src, dst, path))
    return {
        "op_locations": op_locations,
        "paths": paths,
    }


def topological_contexts(nodes: list, edges: list, ii: int) -> dict:
    graph = nx.DiGraph()
    graph.add_nodes_from(nodes)
    graph.add_edges_from(edges)
    levels = {node: 0 for node in nodes}
    if nx.is_directed_acyclic_graph(graph):
        for node in nx.topological_sort(graph):
            for succ in graph.successors(node):
                levels[succ] = max(levels[succ], levels[node] + 1)
    else:
        for src, dst in edges:
            levels[dst] = max(levels.get(dst, 0), levels.get(src, 0) + 1)
    return {node: levels[node] % ii for node in nodes}


def context_candidates(nodes: list, edges: list, ii: int, seed: int, count: int) -> list:
    result = []
    result.append({node: 0 for node in nodes})
    result.append(topological_contexts(nodes, edges, ii))
    result.append({node: (-value) % ii for node, value in result[-1].items()})
    rng = random.Random(seed)
    while len(result) < count:
        result.append({node: rng.randrange(ii) for node in nodes})
    return result


def route_order_candidates(edges: list, positions: dict, seed: int, count: int) -> list:
    result = [list(edges)]
    result.append(sorted(edges, key=lambda e: abs(positions[e[0]][0] - positions[e[1]][0]) + abs(positions[e[0]][1] - positions[e[1]][1]), reverse=True))
    result.append(list(reversed(result[-1])))
    degree = {}
    for a, b in edges:
        degree[a] = degree.get(a, 0) + 1
        degree[b] = degree.get(b, 0) + 1
    result.append(sorted(edges, key=lambda e: degree.get(e[0], 0) + degree.get(e[1], 0), reverse=True))
    rng = random.Random(seed + 1009)
    while len(result) < count:
        shuffled = list(edges)
        rng.shuffle(shuffled)
        result.append(shuffled)
    return result


def make_empty_mapping(arch: dict) -> dict:
    rows = int(arch["row"])
    cols = int(arch["column"])
    ii = int(arch["context_size"])
    pe_config = []
    for row in range(rows):
        for col in range(cols):
            pe_config.append(
                {
                    "row_id": row,
                    "column_id": col,
                    "config": [
                        {
                            "context_id": ctx,
                            "operation_type": NOP_OP,
                            "operation_name": NOP_OP,
                            "const_value": 0,
                            "to_config_id": [],
                            "from_config_id": [],
                        }
                        for ctx in range(ii)
                    ],
                }
            )
    return {
        "column": cols,
        "row": rows,
        "context_size": ii,
        "memory_io_type": arch.get("memory_io", "all"),
        "cgra_type": arch.get("CGRA_type", "default"),
        "network_type": arch.get("network_type", "orthogonal"),
        "PE_config": pe_config,
    }


def get_config(mapping: dict, loc: tuple) -> dict:
    row, col, ctx = loc
    cols = int(mapping["column"])
    return mapping["PE_config"][row * cols + col]["config"][ctx]


def config_id(loc: tuple) -> dict:
    return {"row_id": loc[0], "column_id": loc[1], "context_id": loc[2]}


def add_unique_config_id(items: list, loc: tuple) -> None:
    cid = config_id(loc)
    if cid not in items:
        items.append(cid)


def write_routed_mapping(mapping_path: Path, arch: dict, nodes: list, ops: dict, const_values: dict, route_result: dict) -> None:
    mapping = make_empty_mapping(arch)
    for node in nodes:
        loc = route_result["op_locations"][node]
        config = get_config(mapping, loc)
        config["operation_type"] = ops[node]
        config["operation_name"] = node
        config["const_value"] = int(const_values.get(node, 0))

    for src, dst, path in route_result["paths"]:
        for loc in path[1:-1]:
            config = get_config(mapping, loc)
            if config["operation_type"] == NOP_OP:
                config["operation_type"] = ROUTE_OP
                config["operation_name"] = ROUTE_OP
        for a, b in zip(path, path[1:]):
            from_config = get_config(mapping, a)
            to_config = get_config(mapping, b)
            add_unique_config_id(from_config["to_config_id"], b)
            add_unique_config_id(to_config["from_config_id"], a)

    write_json(mapping_path, mapping)


def write_all_ops_blif(dfg_path: Path, out_path: Path, name_map_path: Path) -> dict:
    nodes, _, _, edges = read_dfg_ops_and_edges(dfg_path)
    predecessors = {node: [] for node in nodes}
    successors = {node: [] for node in nodes}
    for src, dst in edges:
        predecessors[dst].append(src)
        successors[src].append(dst)
    terminal_nodes = [node for node in nodes if not successors[node]]
    name_map, reverse_map = unique_blif_names(nodes)

    lines = [".model cgra_dfg", ".inputs", ".outputs"]
    for node in nodes:
        fanin = predecessors[node]
        output = name_map[node]
        input_names = [name_map[src] for src in fanin]
        lines.append(".names " + " ".join(input_names + [output]))
        if input_names:
            lines.append(f"{'1' * len(input_names)} 1")
        else:
            lines.append("1")
    lines.append(".end")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines) + "\n")
    write_json(
        name_map_path,
        {
            "original_to_blif": name_map,
            "blif_to_original": reverse_map,
            "input_nodes": [],
            "output_nodes": [],
            "terminal_nodes": terminal_nodes,
            "edge_count": len(edges),
        },
    )
    return {
        "nodes": nodes,
        "edges": edges,
        "predecessors": predecessors,
        "successors": successors,
        "name_map": name_map,
        "reverse_map": reverse_map,
    }


def make_fixed_physical_vpr_arch(arch_xml: Path, mapper_config: dict, work_dir: Path, rows: int, cols: int) -> Path:
    base_arch = prepare_vpr_arch_xml(arch_xml, mapper_config, work_dir)
    tree = ET.parse(base_arch)
    root = tree.getroot()

    layout = root.find("layout")
    if layout is None:
        layout = ET.SubElement(root, "layout")
    for child in list(layout):
        layout.remove(child)
    fixed = ET.SubElement(
        layout,
        "fixed_layout",
        {
            "name": "cgra_physical",
            "width": str(cols + 2),
            "height": str(rows + 2),
        },
    )
    ET.SubElement(fixed, "perimeter", {"type": "io", "priority": "100"})
    ET.SubElement(fixed, "corners", {"type": "EMPTY", "priority": "101"})
    ET.SubElement(fixed, "fill", {"type": "clb", "priority": "10"})

    generated_arch = work_dir / "vpr_cgra_physical_arch.xml"
    tree.write(generated_arch, encoding="unicode")
    return generated_arch.resolve()


def run_vpr_placement(dfg: Path, arch_template: Path, output_dir: Path, benchmark: str, mapper_config: dict, timeout_sec: float, progress: bool = False) -> dict:
    vpr_bin = expand_env(mapper_config.get("vpr_bin", "vpr"))
    arch_xml_value = expand_env(mapper_config.get("arch_xml", ""))
    if unresolved_env(vpr_bin):
        raise FileNotFoundError(f"unresolved vpr_bin: {mapper_config.get('vpr_bin', '')}")
    if not Path(vpr_bin).exists():
        raise FileNotFoundError(f"VPR binary not found: {vpr_bin}")
    vpr_bin = str(Path(vpr_bin).resolve())
    if not arch_xml_value:
        raise FileNotFoundError("VPR architecture XML is not set; set VPR_ARCH_XML or mapper.arch_xml")
    arch_xml = Path(arch_xml_value)
    if not arch_xml.exists():
        raise FileNotFoundError(f"VPR architecture XML not found: {arch_xml}")

    work_dir = output_dir / "vpr"
    work_dir.mkdir(parents=True, exist_ok=True)
    arch = load_json(arch_template)
    arch_xml = make_fixed_physical_vpr_arch(
        arch_xml.resolve(),
        mapper_config,
        work_dir,
        int(arch["row"]),
        int(arch["column"]),
    )
    blif_path = (work_dir / f"{benchmark}.blif").resolve()
    name_map_path = work_dir / "vpr_name_map.json"
    if bool(mapper_config.get("place_all_ops", False)):
        dfg_data = write_all_ops_blif(dfg, blif_path, name_map_path)
    else:
        dfg_data = write_blif(dfg, blif_path, name_map_path)
    stdout_path = work_dir / "stdout.txt"
    stderr_path = work_dir / "stderr.txt"
    cmd = vpr_command(vpr_bin, arch_xml, blif_path, mapper_config)
    write_json(work_dir / "command.json", {"cmd": cmd, "cwd": str(work_dir)})
    started_at = time.time()
    if progress:
        print(f"  VPR modulo seed: running {' '.join(cmd)}", flush=True)
    proc = subprocess.run(cmd, cwd=work_dir, capture_output=True, text=True, timeout=timeout_sec)
    elapsed = time.time() - started_at
    stdout_path.write_text(proc.stdout)
    stderr_path.write_text(proc.stderr)
    if proc.returncode != 0:
        raise RuntimeError(f"VPR returned {proc.returncode}")
    place_path = find_place_file(work_dir, blif_path)
    net_path = find_net_file(work_dir, blif_path)
    top_positions, _, _ = parse_place_file(place_path, dfg_data["reverse_map"])
    positions = parse_packed_net_positions(net_path, top_positions, dfg_data["reverse_map"])
    if bool(mapper_config.get("recover_optimized_buffers", True)):
        positions, _ = recover_buffer_like_positions(dfg, positions)
    missing = sorted(node for node in dfg_data["nodes"] if node not in positions)
    if missing:
        raise RuntimeError(f"VPR placement missing {len(missing)} DFG nodes")
    positions = {
        node: (
            min(max(int(position[0]) - 1, 0), int(arch["row"]) - 1),
            min(max(int(position[1]) - 1, 0), int(arch["column"]) - 1),
        )
        for node, position in positions.items()
    }
    return {
        "positions": positions,
        "elapsed": elapsed,
        "stdout_file": stdout_path,
        "stderr_file": stderr_path,
        "work_dir": work_dir,
    }


def resolve_mii(mii_arg: str, dfg: Path, arch_template: Path, out_dir: Path, missing_distance_policy: str) -> tuple:
    mii_data = compute_res_mii(dfg, arch_template, missing_distance_policy)
    write_json(out_dir / "mii.json", mii_data)
    computed_mii = int(mii_data["MII"]) if mii_data["MII"] is not None else None
    if mii_arg == "auto":
        if mii_data["MII"] is None:
            raise ValueError(f"Cannot compute MII; unsupported ops: {mii_data['unsupported_ops']}")
        return computed_mii, computed_mii
    return computed_mii, int(mii_arg)


def run_one_vpr_modulo(
    dfg: Path,
    arch_template: Path,
    output_dir: Path,
    benchmark: str,
    mapper_name: str,
    arch_name: str,
    mii: str,
    ii_max: int,
    timeout_sec: float,
    mapper_config: dict,
    missing_distance_policy: str = "self_loop",
    progress: bool = False,
) -> dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    computed_mii, start_ii = resolve_mii(mii, dfg, arch_template, output_dir, missing_distance_policy)
    nodes, ops, const_values, edges = read_dfg_ops_and_edges(dfg)
    context_seed_count = int(mapper_config.get("context_seed_count", 16))
    routing_retry_count = int(mapper_config.get("routing_retry_count", 8))
    random_seed = int(mapper_config.get("random_seed", 3001))
    rows = []
    summary = {
        "benchmark": benchmark,
        "mapper": mapper_name,
        "arch_name": arch_name,
        "problem_type": "modulo",
        "evaluation_mode": "routing",
        "dfg": str(dfg),
        "arch_template": str(arch_template),
        "MII": computed_mii,
        "start_II": start_ii,
        "ii_max": ii_max,
        "timeout_sec": timeout_sec,
        "status": "failed",
        "achieved_II": None,
        "trials": [],
    }

    for ii in range(start_ii, ii_max + 1):
        trial_dir = output_dir / "tried_ii" / f"ii_{ii}"
        raw_dir = trial_dir / "raw" / f"attempt_{time.time_ns()}"
        run_dir = raw_dir / "mapping" / f"vpr_modulo_{time.time_ns()}"
        run_dir.mkdir(parents=True, exist_ok=True)
        arch_path = trial_dir / "arch.json"
        write_json(arch_path, make_arch_for_ii(arch_template, ii, force_default=True))
        arch = load_json(arch_path)
        input_log = {
            "dfg_file": str(dfg.resolve()),
            "cgra_file": str(arch_path.resolve()),
            "timeout_s": timeout_sec,
            "parallel_num": 1,
            "mapper": mapper_name,
            "runner": "vpr_modulo",
        }
        write_json(run_dir / f"input_log_vpr_modulo_ii_{ii}.json", input_log)
        started_at = time.time()
        status = "failed"
        reason = ""
        mapping_path = run_dir / f"mapping_vpr_modulo_ii_{ii}.json"
        stdout_file = trial_dir / "stdout.txt"
        stderr_file = trial_dir / "stderr.txt"
        stdout_file.write_text("")
        stderr_file.write_text("")
        try:
            placement = run_vpr_placement(dfg, arch_path, trial_dir, benchmark, mapper_config, timeout_sec, progress=progress)
            stdout_file = placement["stdout_file"]
            stderr_file = placement["stderr_file"]
            route_orders = route_order_candidates(edges, placement["positions"], random_seed + ii, routing_retry_count)
            routed = None
            for contexts in context_candidates(nodes, edges, ii, random_seed + ii, context_seed_count):
                for order in route_orders:
                    routed = route_with_contexts(nodes, ops, placement["positions"], contexts, edges, arch, order)
                    if routed is not None:
                        break
                if routed is not None:
                    break
            if routed is None:
                reason = "routing failed for VPR placement"
            else:
                write_routed_mapping(mapping_path, arch, nodes, ops, const_values, routed)
                status = "success"
        except Exception as exc:
            reason = str(exc)
        elapsed = time.time() - started_at
        write_json(
            run_dir / f"output_log_vpr_modulo_ii_{ii}.json",
            {
                "is_success": status == "success",
                "mapping_time_s": elapsed,
                "external_reason": reason,
            },
        )
        row = normalize_run(
            run_dir,
            benchmark=benchmark,
            mapper=mapper_name,
            arch_name=arch_name,
            mii=computed_mii,
            achieved_ii=ii if status == "success" else None,
            start_ii=start_ii,
            dfg_path=dfg,
            arch_path=arch_path,
            evaluation_mode="routing",
        )
        row["status"] = status
        row["external_reason"] = reason
        row["wall_time_sec"] = elapsed
        row["process_returncode"] = 0 if status == "success" else 1
        row["trial_dir"] = str(trial_dir)
        row["stdout_file"] = str(stdout_file)
        row["stderr_file"] = str(stderr_file)
        row["arch_file"] = str(arch_path)
        row["raw_output_dir"] = str(raw_dir)
        rows.append(row)
        summary["trials"].append(
            {
                "II": ii,
                "success": status == "success",
                "run_dir": str(run_dir),
                "trial_dir": str(trial_dir),
                "arch_file": str(arch_path),
                "mapping_file": str(mapping_path) if mapping_path.exists() else "",
                "external_reason": reason,
                "wall_time_sec": elapsed,
            }
        )
        if progress:
            print(f"    II={ii}: {status}, elapsed={elapsed:.3f}s {reason}", flush=True)
        if status == "success":
            summary["status"] = "success"
            summary["achieved_II"] = ii
            break

    write_json(output_dir / "summary.json", summary)
    write_metrics_csv(output_dir / "metrics.csv", rows)
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description="Run VPR SA placement followed by modulo CGRA routing.")
    parser.add_argument("--dfg", required=True, type=Path)
    parser.add_argument("--arch-template", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--benchmark", default="")
    parser.add_argument("--mapper-name", default="vpr_sa_routed")
    parser.add_argument("--arch-name", default="")
    parser.add_argument("--mii", default="auto")
    parser.add_argument("--ii-max", required=True, type=int)
    parser.add_argument("--timeout-sec", required=True, type=float)
    parser.add_argument("--mapper-config-json", default="{}")
    parser.add_argument("--missing-distance-policy", default="self_loop")
    parser.add_argument("--progress", action="store_true")
    args = parser.parse_args()
    summary = run_one_vpr_modulo(
        dfg=args.dfg,
        arch_template=args.arch_template,
        output_dir=args.output_dir,
        benchmark=args.benchmark or args.dfg.stem,
        mapper_name=args.mapper_name,
        arch_name=args.arch_name or args.arch_template.stem,
        mii=args.mii,
        ii_max=args.ii_max,
        timeout_sec=args.timeout_sec,
        mapper_config=json.loads(args.mapper_config_json),
        missing_distance_policy=args.missing_distance_policy,
        progress=args.progress,
    )
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
