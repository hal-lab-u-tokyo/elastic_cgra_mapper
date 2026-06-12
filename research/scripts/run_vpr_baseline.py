#!/usr/bin/env python3

import argparse
import json
import os
import re
import shutil
import subprocess
import time
import xml.etree.ElementTree as ET
from pathlib import Path

import networkx as nx

from lib import (
    clean_dot_value,
    clean_node_name,
    load_json,
    placement_cost,
    read_dfg_stats,
    safe_ratio,
    write_json,
    write_metrics_csv,
)


SUCCESS_STATUSES = {"success"}
ENV_PATTERN = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)(?::-(.*?))?\}|\$([A-Za-z_][A-Za-z0-9_]*)")


def expand_env(value) -> str:
    if value is None:
        return ""

    def replace(match: re.Match) -> str:
        name = match.group(1) or match.group(3)
        default = match.group(2)
        current = os.environ.get(name, "")
        if current:
            return current
        if default is not None:
            return default
        return match.group(0)

    return ENV_PATTERN.sub(replace, str(value))


def unresolved_env(value: str) -> bool:
    return "$" in value


def sanitize_blif_name(value: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9_.$-]", "_", value)
    if not sanitized:
        sanitized = "node"
    if sanitized[0].isdigit():
        sanitized = f"n_{sanitized}"
    return sanitized


def read_dfg(dfg_path: Path) -> tuple:
    graph = nx.nx_pydot.read_dot(str(dfg_path))
    nodes = []
    node_set = set()
    for node_name, attrs in graph.nodes(data=True):
        clean_name = clean_node_name(node_name)
        if clean_name in {"", "\\n", "\n"}:
            continue
        if not clean_dot_value(attrs.get("opcode", "")):
            continue
        nodes.append(clean_name)
        node_set.add(clean_name)

    predecessors = {node: [] for node in nodes}
    successors = {node: [] for node in nodes}
    edges = []
    for src, dst in graph.edges():
        clean_src = clean_node_name(src)
        clean_dst = clean_node_name(dst)
        if clean_src not in node_set or clean_dst not in node_set:
            continue
        edges.append((clean_src, clean_dst))
        predecessors[clean_dst].append(clean_src)
        successors[clean_src].append(clean_dst)
    return nodes, edges, predecessors, successors


def unique_blif_names(nodes: list) -> tuple:
    name_map = {}
    reverse_map = {}
    used = set()
    for node in nodes:
        base = sanitize_blif_name(node)
        candidate = base
        suffix = 1
        while candidate in used:
            suffix += 1
            candidate = f"{base}_{suffix}"
        used.add(candidate)
        name_map[node] = candidate
        reverse_map[candidate] = node
    return name_map, reverse_map


def write_blif(dfg_path: Path, out_path: Path, name_map_path: Path) -> dict:
    nodes, edges, predecessors, successors = read_dfg(dfg_path)
    name_map, reverse_map = unique_blif_names(nodes)
    input_nodes = [node for node in nodes if not predecessors[node]]
    output_nodes = [node for node in nodes if not successors[node]]
    if not input_nodes and nodes:
        input_nodes = [nodes[0]]
    if not output_nodes and nodes:
        output_nodes = [nodes[-1]]

    lines = [".model cgra_dfg"]
    lines.append(".inputs " + " ".join(name_map[node] for node in input_nodes))
    lines.append(".outputs " + " ".join(name_map[node] for node in output_nodes))

    for node in nodes:
        if node in input_nodes:
            continue
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
            "input_nodes": input_nodes,
            "output_nodes": output_nodes,
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


def parse_place_file(place_path: Path, reverse_map: dict) -> tuple:
    positions = {}
    rows = ""
    cols = ""
    if not place_path.exists():
        return positions, rows, cols

    array_size_pattern = re.compile(r"Array size:\s*(\d+)\s*x\s*(\d+)")
    for raw_line in place_path.read_text(errors="replace").splitlines():
        line = raw_line.strip()
        if not line:
            continue
        array_match = array_size_pattern.search(line)
        if array_match:
            cols = int(array_match.group(1))
            rows = int(array_match.group(2))
            continue
        if line.startswith("#") or line.startswith("Netlist_File") or line.startswith("Array size"):
            continue
        parts = line.split()
        if len(parts) < 3:
            continue
        try:
            x = int(parts[1])
            y = int(parts[2])
        except ValueError:
            continue
        original = original_name_from_vpr_block(parts[0], reverse_map)
        if original:
            positions[original] = (y, x)
    return positions, rows, cols


def original_name_from_vpr_block(block_name: str, reverse_map: dict) -> str:
    candidates = []
    base = block_name.split("[", 1)[0]
    candidates.append(base)
    if ":" in base:
        candidates.append(base.split(":", 1)[1])
    for candidate in candidates:
        original = reverse_map.get(candidate)
        if original:
            return original
    return ""


def parse_packed_net_positions(net_path: Path, top_positions: dict, reverse_map: dict) -> dict:
    if not net_path.exists() or not top_positions:
        return dict(top_positions)

    try:
        root = ET.parse(net_path).getroot()
    except ET.ParseError:
        return dict(top_positions)

    positions = dict(top_positions)

    def assign_descendants(block, top_position) -> None:
        original = original_name_from_vpr_block(block.attrib.get("name", ""), reverse_map)
        if original:
            positions[original] = top_position
        for child in block:
            if child.tag == "block":
                assign_descendants(child, top_position)

    for top_block in root:
        if top_block.tag != "block":
            continue
        top_original = original_name_from_vpr_block(top_block.attrib.get("name", ""), reverse_map)
        top_position = top_positions.get(top_original)
        if not top_position:
            continue
        assign_descendants(top_block, top_position)

    return positions


def recover_buffer_like_positions(dfg_path: Path, positions: dict) -> tuple:
    nodes, _, predecessors, successors = read_dfg(dfg_path)
    recovered = dict(positions)
    recovered_nodes = []
    node_set = set(nodes)
    changed = True
    while changed:
        changed = False
        for node in nodes:
            if node in recovered:
                continue
            pred_positions = {
                recovered[pred]
                for pred in predecessors.get(node, [])
                if pred in recovered
            }
            succ_positions = {
                recovered[succ]
                for succ in successors.get(node, [])
                if succ in recovered
            }
            if len(pred_positions) == 1:
                recovered[node] = next(iter(pred_positions))
            elif len(succ_positions) == 1:
                recovered[node] = next(iter(succ_positions))
            else:
                continue
            recovered_nodes.append(node)
            changed = True
    return recovered, sorted(node for node in recovered_nodes if node in node_set)


def find_place_file(work_dir: Path, blif_path: Path) -> Path:
    candidates = [
        work_dir / f"{blif_path.stem}.place",
        work_dir / "cgra_dfg.place",
    ]
    candidates.extend(sorted(work_dir.glob("*.place")))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return work_dir / f"{blif_path.stem}.place"


def find_net_file(work_dir: Path, blif_path: Path) -> Path:
    candidates = [
        work_dir / f"{blif_path.stem}.net",
        work_dir / "cgra_dfg.net",
    ]
    candidates.extend(sorted(work_dir.glob("*.net")))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return work_dir / f"{blif_path.stem}.net"


def architecture_summary(arch_path: Path) -> dict:
    arch = load_json(arch_path)
    network_type = str(arch.get("network_type", ""))
    placement_cost_model = str(arch.get("placement_cost_model", "mesh"))
    if placement_cost_model == "mesh" and network_type in {"one_hop_axis2", "one_hop", "1hop"}:
        placement_cost_model = "one_hop_axis2"
    return {
        "rows": int(arch["row"]),
        "cols": int(arch["column"]),
        "context_size": int(arch.get("context_size", 1)),
        "cgra_type": str(arch.get("CGRA_type", "")),
        "network_type": network_type,
        "memory_io": str(arch.get("memory_io", "")),
        "placement_cost_model": placement_cost_model,
    }


def prepare_vpr_arch_xml(arch_xml: Path, mapper_config: dict, work_dir: Path) -> Path:
    if int(mapper_config.get("pack_capacity", 0) or 0) != 1:
        return arch_xml

    text = arch_xml.read_text()
    replacements = {
        '<input name="I" num_pins="40" equivalent="full"/>': '<input name="I" num_pins="6" equivalent="full"/>',
        '<output name="O" num_pins="10" equivalent="instance"/>': '<output name="O" num_pins="1" equivalent="instance"/>',
        '<pb_type name="fle" num_pb="10">': '<pb_type name="fle" num_pb="1">',
        'input="clb.I fle[9:0].out" output="fle[9:0].in"': 'input="clb.I fle[0:0].out" output="fle[0:0].in"',
        'out_port="fle[9:0].in"': 'out_port="fle[0:0].in"',
        'in_port="fle[9:0].out"': 'in_port="fle[0:0].out"',
        'input="clb.clk" output="fle[9:0].clk"': 'input="clb.clk" output="fle[0:0].clk"',
        'input="fle[9:0].out" output="clb.O"': 'input="fle[0:0].out" output="clb.O"',
    }
    missing = [old for old in replacements if old not in text]
    if missing:
        raise ValueError(
            "pack_capacity=1 currently expects the bundled k6_N10-style VPR architecture; "
            f"missing patterns: {missing}"
        )
    for old, new in replacements.items():
        text = text.replace(old, new)
    text = text.replace("K = 6, N = 10", "K = 6, N = 1")

    generated_arch = work_dir / "vpr_k6_N1_40nm.xml"
    generated_arch.write_text(text)
    return generated_arch.resolve()


def placement_metrics(dfg_path: Path, positions: dict, cost_model: str) -> dict:
    _, edges, _, _ = read_dfg(dfg_path)
    placed_edges = []
    for src, dst in edges:
        if src in positions and dst in positions:
            placed_edges.append((src, dst))

    wirelengths = []
    costs = []
    for src, dst in placed_edges:
        src_row, src_col = positions[src]
        dst_row, dst_col = positions[dst]
        dx = abs(src_col - dst_col)
        dy = abs(src_row - dst_row)
        wirelengths.append(dx + dy)
        costs.append(placement_cost(dx, dy, cost_model))

    if not placed_edges:
        return {
            "placement_edge_count": "",
            "placement_wirelength_sum": "",
            "placement_avg_wirelength": "",
            "placement_max_wirelength": "",
            "placement_direct_edge_count": "",
            "placement_direct_edge_ratio": "",
            "placement_fifo_sum": "",
            "placement_avg_fifo": "",
            "placement_max_fifo": "",
            "placement_cost_sum": "",
            "placement_avg_cost": "",
            "placement_max_cost": "",
            "placement_optimal_edge_count": "",
            "placement_optimal_edge_ratio": "",
            "placement_fifo_like_sum": "",
            "placement_avg_fifo_like": "",
            "placement_max_fifo_like": "",
        }

    fifo_values = [max(0, wirelength - 1) for wirelength in wirelengths]
    fifo_like_values = [max(0, cost - 1) for cost in costs]
    return {
        "placement_edge_count": len(placed_edges),
        "placement_wirelength_sum": sum(wirelengths),
        "placement_avg_wirelength": safe_ratio(sum(wirelengths), len(placed_edges)),
        "placement_max_wirelength": max(wirelengths),
        "placement_direct_edge_count": sum(1 for wirelength in wirelengths if wirelength <= 1),
        "placement_direct_edge_ratio": safe_ratio(
            sum(1 for wirelength in wirelengths if wirelength <= 1), len(placed_edges)
        ),
        "placement_fifo_sum": sum(fifo_values),
        "placement_avg_fifo": safe_ratio(sum(fifo_values), len(placed_edges)),
        "placement_max_fifo": max(fifo_values),
        "placement_cost_sum": sum(costs),
        "placement_avg_cost": safe_ratio(sum(costs), len(placed_edges)),
        "placement_max_cost": max(costs),
        "placement_optimal_edge_count": sum(1 for cost in costs if cost <= 1),
        "placement_optimal_edge_ratio": safe_ratio(sum(1 for cost in costs if cost <= 1), len(placed_edges)),
        "placement_fifo_like_sum": sum(fifo_like_values),
        "placement_avg_fifo_like": safe_ratio(sum(fifo_like_values), len(placed_edges)),
        "placement_max_fifo_like": max(fifo_like_values),
    }


def compute_bbox_metrics(positions: dict) -> tuple:
    if not positions:
        return "", ""
    rows = [row for row, _ in positions.values()]
    cols = [col for _, col in positions.values()]
    area = (max(rows) - min(rows) + 1) * (max(cols) - min(cols) + 1)
    return area, safe_ratio(len(set(positions.values())), area)


def vpr_command(vpr_bin: str, arch_xml: Path, blif_path: Path, mapper_config: dict) -> list:
    args = mapper_config.get("vpr_args")
    if args:
        return [vpr_bin, str(arch_xml), str(blif_path)] + [expand_env(arg) for arg in args]

    place_algorithm = str(mapper_config.get("place_algorithm", "bounding_box"))
    return [
        vpr_bin,
        str(arch_xml),
        str(blif_path),
        "--pack",
        "--place",
        "--absorb_buffer_luts",
        "off",
        "--place_algorithm",
        place_algorithm,
        "--disp",
        "off",
    ]


def skipped_summary(
    output_dir: Path,
    benchmark: str,
    mapper_name: str,
    arch_name: str,
    dfg: Path,
    arch_template: Path,
    timeout_sec: float,
    reason: str,
) -> dict:
    arch = architecture_summary(arch_template)
    stats = read_dfg_stats(dfg)
    row = {
        "benchmark": benchmark,
        "mapper": mapper_name,
        "arch_name": arch_name,
        "evaluation_mode": "placement_only",
        "cgra_type": arch["cgra_type"],
        "network_type": arch["network_type"],
        "memory_io": arch["memory_io"],
        "placement_cost_model": arch["placement_cost_model"],
        "rows": arch["rows"],
        "cols": arch["cols"],
        "context_size": 1,
        "MII": 1,
        "start_II": 1,
        "achieved_II": "",
        "status": "skipped",
        "timeout_sec": timeout_sec,
        "dfg_nodes": stats["node_count"],
        "dfg_edges": stats["edge_count"],
        "external_reason": reason,
        "arch_file": str(arch_template),
        "run_dir": str(output_dir),
    }
    summary = {
        "benchmark": benchmark,
        "mapper": mapper_name,
        "arch_name": arch_name,
        "problem_type": "placement2d",
        "evaluation_mode": "placement_only",
        "dfg": str(dfg),
        "arch_template": str(arch_template),
        "status": "skipped",
        "achieved_II": None,
        "external_reason": reason,
        "trials": [],
    }
    write_json(output_dir / "summary.json", summary)
    write_metrics_csv(output_dir / "metrics.csv", [row])
    return summary


def run_one_vpr(
    dfg: Path,
    arch_template: Path,
    output_dir: Path,
    benchmark: str,
    mapper_name: str,
    arch_name: str,
    timeout_sec: float,
    mapper_config: dict,
    problem_type: str = "placement2d",
    evaluation_mode: str = "placement_only",
    progress: bool = False,
) -> dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    if problem_type != "placement2d" or evaluation_mode != "placement_only":
        raise ValueError("VPR baseline runner is placement2d placement_only only.")

    vpr_bin = expand_env(mapper_config.get("vpr_bin", "vpr"))
    arch_xml_value = expand_env(mapper_config.get("arch_xml", ""))
    allow_missing = bool(mapper_config.get("allow_missing", True))
    if unresolved_env(vpr_bin):
        reason = f"unresolved vpr_bin: {mapper_config.get('vpr_bin', '')}"
        if allow_missing:
            return skipped_summary(output_dir, benchmark, mapper_name, arch_name, dfg, arch_template, timeout_sec, reason)
        raise FileNotFoundError(reason)
    if not shutil.which(vpr_bin) and not Path(vpr_bin).exists():
        reason = f"VPR binary not found: {vpr_bin}"
        if allow_missing:
            return skipped_summary(output_dir, benchmark, mapper_name, arch_name, dfg, arch_template, timeout_sec, reason)
        raise FileNotFoundError(reason)
    if Path(vpr_bin).exists():
        vpr_bin = str(Path(vpr_bin).resolve())
    if not arch_xml_value or unresolved_env(arch_xml_value):
        reason = "VPR architecture XML is not set; set VPR_ARCH_XML or mapper.arch_xml"
        if allow_missing:
            return skipped_summary(output_dir, benchmark, mapper_name, arch_name, dfg, arch_template, timeout_sec, reason)
        raise FileNotFoundError(reason)
    arch_xml = Path(arch_xml_value)
    if not arch_xml.exists():
        reason = f"VPR architecture XML not found: {arch_xml}"
        if allow_missing:
            return skipped_summary(output_dir, benchmark, mapper_name, arch_name, dfg, arch_template, timeout_sec, reason)
        raise FileNotFoundError(reason)
    arch_xml = arch_xml.resolve()

    work_dir = output_dir / "vpr"
    work_dir.mkdir(parents=True, exist_ok=True)
    arch_xml = prepare_vpr_arch_xml(arch_xml, mapper_config, work_dir)
    blif_path = (work_dir / f"{benchmark}.blif").resolve()
    name_map_path = work_dir / "vpr_name_map.json"
    dfg_data = write_blif(dfg, blif_path, name_map_path)
    stdout_path = work_dir / "stdout.txt"
    stderr_path = work_dir / "stderr.txt"
    command_path = work_dir / "command.json"
    cmd = vpr_command(vpr_bin, arch_xml, blif_path, mapper_config)
    write_json(command_path, {"cmd": cmd, "cwd": str(work_dir)})

    if progress:
        print(f"  VPR: running {' '.join(cmd)}", flush=True)
    started_at = time.time()
    timed_out = False
    try:
        proc = subprocess.run(
            cmd,
            cwd=work_dir,
            capture_output=True,
            text=True,
            timeout=timeout_sec,
        )
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        elapsed = time.time() - started_at
        stdout_path.write_text(exc.stdout or "")
        stderr_path.write_text(exc.stderr or "")
        returncode = ""
    else:
        elapsed = time.time() - started_at
        stdout_path.write_text(proc.stdout)
        stderr_path.write_text(proc.stderr)
        returncode = proc.returncode

    arch = architecture_summary(arch_template)
    place_path = find_place_file(work_dir, blif_path)
    net_path = find_net_file(work_dir, blif_path)
    top_positions, parsed_rows, parsed_cols = parse_place_file(place_path, dfg_data["reverse_map"])
    positions = parse_packed_net_positions(net_path, top_positions, dfg_data["reverse_map"])
    recovered_buffer_nodes = []
    if bool(mapper_config.get("recover_optimized_buffers", True)):
        positions, recovered_buffer_nodes = recover_buffer_like_positions(dfg, positions)
    rows = parsed_rows or arch["rows"]
    cols = parsed_cols or arch["cols"]
    stats = read_dfg_stats(dfg)
    missing_nodes = sorted(node for node in dfg_data["nodes"] if node not in positions)
    status = "success"
    external_reason = ""
    if timed_out:
        status = "timeout"
        external_reason = "VPR timed out"
    elif returncode != 0:
        status = "failed"
        external_reason = f"VPR returned {returncode}"
    elif missing_nodes:
        status = "failed"
        external_reason = f"VPR placement missing {len(missing_nodes)} DFG nodes"

    bbox_area, bbox_util = compute_bbox_metrics(positions)
    placement = placement_metrics(dfg, positions, arch["placement_cost_model"])
    compute_ops = len(positions)
    total_pe_contexts = int(rows) * int(cols)
    row = {
        "benchmark": benchmark,
        "mapper": mapper_name,
        "arch_name": arch_name,
        "evaluation_mode": "placement_only",
        "cgra_type": arch["cgra_type"],
        "network_type": arch["network_type"],
        "memory_io": arch["memory_io"],
        "placement_cost_model": arch["placement_cost_model"],
        "rows": rows,
        "cols": cols,
        "context_size": 1,
        "MII": 1,
        "start_II": 1,
        "achieved_II": 1 if status in SUCCESS_STATUSES else "",
        "II_ratio": 1 if status in SUCCESS_STATUSES else "",
        "status": status,
        "mapping_time_sec": elapsed,
        "wall_time_sec": elapsed,
        "process_returncode": returncode,
        "timeout_sec": timeout_sec,
        "parallel_num": 1,
        "dfg_nodes": stats["node_count"],
        "dfg_edges": stats["edge_count"],
        "used_ops": compute_ops,
        "compute_ops": compute_ops,
        "memory_ops": "",
        "const_ops": "",
        "route_ops": 0,
        "nop_ops": max(0, total_pe_contexts - compute_ops),
        "total_pe_contexts": total_pe_contexts,
        "utilization": safe_ratio(compute_ops, total_pe_contexts),
        "pe_context_utilization": safe_ratio(compute_ops, total_pe_contexts),
        "compute_context_utilization": safe_ratio(compute_ops, total_pe_contexts),
        "route_context_utilization": 0,
        "memory_context_utilization": "",
        "active_pe_count": len(set(positions.values())),
        "active_compute_pe_count": len(set(positions.values())),
        "active_route_pe_count": 0,
        "active_memory_pe_count": "",
        "pe_active_ratio": safe_ratio(len(set(positions.values())), total_pe_contexts),
        "compute_pe_utilization": safe_ratio(len(set(positions.values())), total_pe_contexts),
        "route_pe_ratio": 0,
        "memory_pe_ratio": "",
        "used_context_count": 1 if positions else 0,
        "context_active_ratio": 1 if positions else 0,
        "max_context_load": compute_ops,
        "min_context_load": compute_ops,
        "avg_context_load": compute_ops,
        "context_load_stddev": 0,
        "max_compute_contexts_per_pe": 1 if positions else 0,
        "avg_compute_contexts_per_active_compute_pe": safe_ratio(compute_ops, len(set(positions.values()))),
        "routing_overhead_ratio": 0,
        "route_to_compute_ratio": 0,
        "compute_bbox_area": bbox_area,
        "compute_bbox_utilization": bbox_util,
        **placement,
        "direct_dfg_edge_count": placement.get("placement_direct_edge_count", ""),
        "routed_dfg_edge_count": "",
        "direct_dfg_edge_ratio": placement.get("placement_direct_edge_ratio", ""),
        "mapping_file": "",
        "external_reason": external_reason,
        "external_log_file": str(command_path),
        "run_dir": str(output_dir),
        "trial_dir": str(work_dir),
        "stdout_file": str(stdout_path),
        "stderr_file": str(stderr_path),
        "arch_file": str(arch_template),
        "raw_output_dir": str(work_dir),
    }

    summary = {
        "benchmark": benchmark,
        "mapper": mapper_name,
        "arch_name": arch_name,
        "problem_type": problem_type,
        "evaluation_mode": evaluation_mode,
        "dfg": str(dfg),
        "arch_template": str(arch_template),
        "vpr_bin": vpr_bin,
        "vpr_arch_xml": str(arch_xml),
        "status": status,
        "achieved_II": 1 if status in SUCCESS_STATUSES else None,
        "external_reason": external_reason,
        "missing_nodes": missing_nodes,
        "place_file": str(place_path) if place_path.exists() else "",
        "net_file": str(net_path) if net_path.exists() else "",
        "top_level_placed_blocks": len(top_positions),
        "recovered_placed_nodes": len(positions),
        "recovered_buffer_like_nodes": recovered_buffer_nodes,
        "trials": [
            {
                "returncode": returncode,
                "success": status in SUCCESS_STATUSES,
                "trial_dir": str(work_dir),
                "stdout_file": str(stdout_path),
                "stderr_file": str(stderr_path),
                "wall_time_sec": elapsed,
            }
        ],
    }
    write_json(output_dir / "summary.json", summary)
    write_metrics_csv(output_dir / "metrics.csv", [row])
    return summary


def main() -> None:
    parser = argparse.ArgumentParser(description="Run VPR as an external placement-only baseline.")
    parser.add_argument("--dfg", required=True, type=Path)
    parser.add_argument("--arch-template", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--benchmark", default="")
    parser.add_argument("--mapper-name", default="vpr")
    parser.add_argument("--arch-name", default="")
    parser.add_argument("--timeout-sec", required=True, type=float)
    parser.add_argument("--mapper-config-json", default="{}")
    parser.add_argument("--progress", action="store_true")
    args = parser.parse_args()
    summary = run_one_vpr(
        dfg=args.dfg,
        arch_template=args.arch_template,
        output_dir=args.output_dir,
        benchmark=args.benchmark or args.dfg.stem,
        mapper_name=args.mapper_name,
        arch_name=args.arch_name or args.arch_template.stem,
        timeout_sec=args.timeout_sec,
        mapper_config=json.loads(args.mapper_config_json),
        progress=args.progress,
    )
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
