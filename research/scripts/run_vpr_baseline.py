#!/usr/bin/env python3

from __future__ import annotations

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
    longest_path_cost,
    parse_placement_log,
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


def emit_names_node(lines: list, input_names: list, output: str) -> None:
    lines.append(".names " + " ".join(input_names + [output]))
    if input_names:
        lines.append(f"{'1' * len(input_names)} 1")
    else:
        lines.append("1")


def emit_protected_buffer_lut(lines: list, input_name: str, output: str) -> None:
    """Keep an internal one-input DFG node from becoming a VPR buffer.

    VPR's buffer absorption is still useful for structural output pads, but it
    also removes one-input internal DFG nodes.  Repeating the same fanin makes
    the atom a two-pin LUT for VPR packing while preserving pass-through
    behavior and without adding a synthetic constant block to the device.
    """
    lines.append(".names " + " ".join([input_name, input_name, output]))
    lines.append("11 1")


def emit_lut_tree(lines: list, input_names: list, output: str,
                  lut_size: int, used_names: set, prefix: str) -> None:
    if len(input_names) <= lut_size:
        emit_names_node(lines, input_names, output)
        return

    level = list(input_names)
    temp_index = 0
    while len(level) > lut_size:
        next_level = []
        for start in range(0, len(level), lut_size):
            chunk = level[start:start + lut_size]
            if len(chunk) == 1:
                next_level.append(chunk[0])
                continue
            temp_index += 1
            temp = f"__{prefix}_lut{temp_index}"
            while temp in used_names:
                temp_index += 1
                temp = f"__{prefix}_lut{temp_index}"
            used_names.add(temp)
            emit_names_node(lines, chunk, temp)
            next_level.append(temp)
        level = next_level
    emit_names_node(lines, level, output)


def write_blif(
    dfg_path: Path,
    out_path: Path,
    name_map_path: Path,
    protect_internal_buffer_luts: bool = False,
    output_nodes_as_io_pads: bool = False,
    output_lut_io_pads: bool = False,
    input_nodes_as_luts: bool = False,
    dfg_nodes_as_luts_only: bool = False,
) -> dict:
    nodes, edges, predecessors, successors = read_dfg(dfg_path)
    name_map, reverse_map = unique_blif_names(nodes)
    input_nodes = [node for node in nodes if not predecessors[node]]
    output_nodes = [node for node in nodes if not successors[node]]
    if not input_nodes and nodes:
        input_nodes = [nodes[0]]
    if not output_nodes and nodes:
        output_nodes = [nodes[-1]]

    input_pad_names = {}
    if input_nodes_as_luts:
        for node in input_nodes:
            input_pad_names[node] = f"__pi_{name_map[node]}"

    lines = [".model cgra_dfg"]
    if not dfg_nodes_as_luts_only:
        lines.append(
            ".inputs "
            + " ".join(input_pad_names.get(node, name_map[node]) for node in input_nodes)
        )
        lines.append(".outputs " + " ".join(name_map[node] for node in output_nodes))

    used_blif_names = set(name_map.values())
    used_blif_names.update(input_pad_names.values())
    output_io_pad_fanin = {}
    output_io_pad_dropped_fanins = {}
    lut_size = 6
    for node in nodes:
        if node in input_nodes and not input_nodes_as_luts and not dfg_nodes_as_luts_only:
            continue
        fanin = predecessors[node]
        output = name_map[node]
        if node in input_nodes and input_nodes_as_luts and not dfg_nodes_as_luts_only:
            input_names = [input_pad_names[node]]
        else:
            input_names = [name_map[src] for src in fanin]
        if output_nodes_as_io_pads and not output_lut_io_pads and node in output_nodes:
            if input_names:
                emit_names_node(lines, [input_names[0]], output)
                output_io_pad_fanin[node] = fanin[0]
                if len(fanin) > 1:
                    output_io_pad_dropped_fanins[node] = fanin[1:]
            else:
                emit_names_node(lines, [], output)
            continue
        if (
            protect_internal_buffer_luts
            and len(input_names) == 1
            and node not in output_nodes
        ):
            emit_protected_buffer_lut(lines, input_names[0], output)
            continue
        emit_lut_tree(lines, input_names, output, lut_size,
                      used_blif_names, sanitize_blif_name(node))

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
            "protect_internal_buffer_luts": protect_internal_buffer_luts,
            "output_nodes_as_io_pads": output_nodes_as_io_pads,
            "output_lut_io_pads": output_lut_io_pads,
            "input_nodes_as_luts": input_nodes_as_luts,
            "dfg_nodes_as_luts_only": dfg_nodes_as_luts_only,
            "input_pad_names": input_pad_names,
            "output_io_pad_fanin": output_io_pad_fanin,
            "output_io_pad_dropped_fanins": output_io_pad_dropped_fanins,
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


def fixed_layout_xml(rows: int, cols: int, io_corners: bool = False, all_clb: bool = False) -> str:
    if all_clb:
        return f"""  <layout>
    <fixed_layout name="cgra_{cols}x{rows}_all_clb" width="{cols}" height="{rows}">
        <!-- All cells are placement sites for DFG operations. -->
        <fill type="clb" priority="10"/>
    </fixed_layout>
</layout>"""

    corner_rule = (
        ""
        if io_corners
        else '        <corners type="EMPTY" priority="101"/>\n'
    )
    return f"""  <layout>
    <fixed_layout name="cgra_{cols}x{rows}" width="{cols}" height="{rows}">
        <!--Perimeter of 'io' blocks with optional empty corners-->
        <perimeter type="io" priority="100"/>
{corner_rule.rstrip()}
        <!--Fill with 'clb'-->
        <fill type="clb" priority="10"/>
    </fixed_layout>
</layout>"""


def io_lut_output_pb_type_xml(capacity: int = 8) -> str:
    """I/O block that can host a multi-input output DFG node.

    Standard VPR I/O pads can consume only one net.  Several YOTT case-study
    sink nodes have multiple predecessors, so mapping them to a standard
    output pad drops DFG edges before placement.  This mode lets VPR pack the
    sink node's `.names` atom and its `.output` atom into the same perimeter
    I/O site, preserving all incoming DFG edges while keeping output legality.
    """
    return f"""    <!-- Define I/O pads begin -->
    <pb_type name="io" capacity="{capacity}" area="0">
      <input name="outpad" num_pins="6"/>
      <output name="inpad" num_pins="1"/>
      <clock name="clock" num_pins="1"/>

      <mode name="inpad">
        <pb_type name="inpad" blif_model=".input" num_pb="1">
          <output name="inpad" num_pins="1"/>
        </pb_type>
        <interconnect>
          <direct name="inpad" input="inpad.inpad" output="io.inpad">
            <delay_constant max="4.243e-11" in_port="inpad.inpad" out_port="io.inpad"/>
          </direct>
        </interconnect>
      </mode>

      <mode name="outpad">
        <pb_type name="outpad" blif_model=".output" num_pb="1">
          <input name="outpad" num_pins="1"/>
        </pb_type>
        <interconnect>
          <direct name="outpad" input="io.outpad[0:0]" output="outpad.outpad">
            <delay_constant max="1.394e-11" in_port="io.outpad" out_port="outpad.outpad"/>
          </direct>
        </interconnect>
      </mode>

      <mode name="out_lutpad">
        <pb_type name="lut6" blif_model=".names" num_pb="1" class="lut">
          <input name="in" num_pins="6" port_class="lut_in"/>
          <output name="out" num_pins="1" port_class="lut_out"/>
          <delay_matrix type="max" in_port="lut6.in" out_port="lut6.out">
            2.352e-10
            2.352e-10
            2.352e-10
            2.352e-10
            2.352e-10
            2.352e-10
          </delay_matrix>
        </pb_type>
        <pb_type name="outpad" blif_model=".output" num_pb="1">
          <input name="outpad" num_pins="1"/>
        </pb_type>
        <interconnect>
          <direct name="lutpad_in" input="io.outpad" output="lut6.in">
            <delay_constant max="1.000e-12" in_port="io.outpad" out_port="lut6.in"/>
          </direct>
          <direct name="lutpad_out" input="lut6.out" output="outpad.outpad">
            <pack_pattern name="output_lutpad" in_port="lut6.out" out_port="outpad.outpad"/>
            <delay_constant max="1.394e-11" in_port="lut6.out" out_port="outpad.outpad"/>
          </direct>
        </interconnect>
      </mode>

      <fc in_type="frac" in_val="0.15" out_type="frac" out_val="0.10"/>

      <pinlocations pattern="custom">
        <loc side="left">io.outpad io.inpad io.clock</loc>
        <loc side="top">io.outpad io.inpad io.clock</loc>
        <loc side="right">io.outpad io.inpad io.clock</loc>
        <loc side="bottom">io.outpad io.inpad io.clock</loc>
      </pinlocations>

      <power method="ignore"/>
    </pb_type>
    <!-- Define I/O pads ends -->"""


def replace_vpr_io_block(text: str, capacity: int = 8) -> str:
    pattern = re.compile(
        r"\s*<!-- Define I/O pads begin -->.*?<!-- Define I/O pads ends -->",
        re.DOTALL,
    )
    replaced, count = pattern.subn(
        "\n" + io_lut_output_pb_type_xml(capacity=capacity), text, count=1
    )
    if count != 1:
        raise ValueError("failed to replace VPR I/O block with output-LUT I/O block")
    return replaced


def replace_vpr_layout(
    text: str,
    rows: int,
    cols: int,
    io_corners: bool = False,
    all_clb: bool = False,
) -> str:
    pattern = re.compile(r"\s*<layout>.*?</layout>", re.DOTALL)
    replaced, count = pattern.subn(
        "\n" + fixed_layout_xml(rows, cols, io_corners=io_corners, all_clb=all_clb),
        text,
        count=1,
    )
    if count != 1:
        raise ValueError("failed to replace VPR architecture layout with fixed CGRA layout")
    return replaced


def prepare_vpr_arch_xml(
    arch_xml: Path,
    mapper_config: dict,
    work_dir: Path,
    arch: dict = None,
) -> Path:
    use_pack_capacity_one = int(mapper_config.get("pack_capacity", 0) or 0) == 1
    fix_layout_to_arch = bool(mapper_config.get("fixed_layout_to_arch", False))
    if not use_pack_capacity_one and not fix_layout_to_arch:
        return arch_xml

    text = arch_xml.read_text()
    if use_pack_capacity_one:
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

    if fix_layout_to_arch:
        if not arch:
            raise ValueError("fixed_layout_to_arch requires an architecture summary")
        io_corners = mapper_config.get("fixed_layout_io_corners")
        if io_corners is None:
            io_corners = str(arch.get("memory_io", "")) == "perimeter"
        text = replace_vpr_layout(
            text,
            int(arch["rows"]),
            int(arch["cols"]),
            io_corners=bool(io_corners),
            all_clb=bool(mapper_config.get("fixed_layout_all_clb", False)),
        )

    if bool(mapper_config.get("output_lut_io_pads", False)):
        vpr_io_capacity = int(mapper_config.get("vpr_io_capacity", 8) or 8)
        text = replace_vpr_io_block(text, capacity=vpr_io_capacity)

    generated_arch = work_dir / "vpr_k6_N1_40nm.xml"
    generated_arch.write_text(text)
    return generated_arch.resolve()


def placement_metrics(dfg_path: Path, positions: dict, cost_model: str) -> dict:
    nodes, edges, _, _ = read_dfg(dfg_path)
    placed_edges = []
    for src, dst in edges:
        if src in positions and dst in positions:
            placed_edges.append((src, dst))

    wirelengths = []
    costs = []
    mesh_weighted_edges = []
    for src, dst in placed_edges:
        src_row, src_col = positions[src]
        dst_row, dst_col = positions[dst]
        dx = abs(src_col - dst_col)
        dy = abs(src_row - dst_row)
        mesh_hop = dx + dy
        wirelengths.append(mesh_hop)
        mesh_weighted_edges.append((src, dst, max(1, mesh_hop)))
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
            "placement_optimal_distance_count": "",
            "placement_optimal_distance_ratio": "",
            "placement_fifo_like_sum": "",
            "placement_avg_fifo_like": "",
            "placement_max_fifo_like": "",
            "placement_paper_fifo_sum": "",
            "placement_avg_paper_fifo": "",
            "placement_max_paper_fifo": "",
            "placement_mesh_hop_sum": "",
            "placement_avg_mesh_hop": "",
            "placement_max_mesh_hop": "",
            "placement_mesh_optimal_edge_count": "",
            "placement_mesh_optimal_edge_ratio": "",
            "placement_mesh_fifo_sum": "",
            "placement_avg_mesh_fifo": "",
            "placement_max_mesh_fifo": "",
            "placement_mapped_lp_mesh_hop": "",
        }

    fifo_values = [max(0, wirelength - 1) for wirelength in wirelengths]
    paper_fifo_values = [max(0, cost - 1) for cost in costs]
    placed_nodes = [node for node in nodes if node in positions]
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
        "placement_optimal_distance_count": sum(1 for wirelength in wirelengths if wirelength == 1),
        "placement_optimal_distance_ratio": safe_ratio(
            sum(1 for wirelength in wirelengths if wirelength == 1), len(placed_edges)
        ),
        "placement_fifo_like_sum": sum(paper_fifo_values),
        "placement_avg_fifo_like": safe_ratio(sum(paper_fifo_values), len(placed_edges)),
        "placement_max_fifo_like": max(paper_fifo_values),
        "placement_paper_fifo_sum": sum(paper_fifo_values),
        "placement_avg_paper_fifo": safe_ratio(sum(paper_fifo_values), len(placed_edges)),
        "placement_max_paper_fifo": max(paper_fifo_values),
        "placement_mesh_hop_sum": sum(wirelengths),
        "placement_avg_mesh_hop": safe_ratio(sum(wirelengths), len(placed_edges)),
        "placement_max_mesh_hop": max(wirelengths),
        "placement_mesh_optimal_edge_count": sum(1 for wirelength in wirelengths if wirelength <= 1),
        "placement_mesh_optimal_edge_ratio": safe_ratio(
            sum(1 for wirelength in wirelengths if wirelength <= 1), len(placed_edges)
        ),
        "placement_mesh_fifo_sum": sum(fifo_values),
        "placement_avg_mesh_fifo": safe_ratio(sum(fifo_values), len(placed_edges)),
        "placement_max_mesh_fifo": max(fifo_values),
        "placement_mapped_lp_mesh_hop": longest_path_cost(
            placed_nodes, mesh_weighted_edges
        ),
    }


def blif_nets(blif_path: Path) -> list[list[str]]:
    """Return VPR placement-objective nets from the generated BLIF.

    DFG-edge metrics and VPR's bounding-box objective are not identical when
    a node fans out.  Keeping this parser in the VPR runner makes both views
    available in the same metrics row.
    """
    if not blif_path.exists():
        return []

    drivers: set[str] = set()
    loads: dict[str, list[str]] = {}
    inputs: list[str] = []
    outputs: list[str] = []
    for raw_line in blif_path.read_text(errors="replace").splitlines():
        parts = raw_line.strip().split()
        if not parts:
            continue
        if parts[0] == ".inputs":
            inputs.extend(parts[1:])
        elif parts[0] == ".outputs":
            outputs.extend(parts[1:])
        elif parts[0] == ".names" and len(parts) >= 2:
            fanins = parts[1:-1]
            output = parts[-1]
            drivers.add(output)
            for fanin in fanins:
                loads.setdefault(fanin, []).append(output)
    for node in inputs:
        drivers.add(node)
    for node in outputs:
        loads.setdefault(node, []).append(node)
    return [
        [driver] + loads.get(driver, [])
        for driver in sorted(drivers)
        if len([driver] + loads.get(driver, [])) >= 2
    ]


def blif_net_metrics(
    blif_path: Path,
    reverse_map: dict,
    positions: dict,
    cost_model: str,
) -> dict:
    blif_positions = {
        blif_name: positions[original_name]
        for blif_name, original_name in reverse_map.items()
        if original_name in positions
    }
    costs = []
    for pins in blif_nets(blif_path):
        points = [blif_positions[pin] for pin in pins if pin in blif_positions]
        if len(points) < 2:
            continue
        rows = [row for row, _ in points]
        cols = [col for _, col in points]
        dx = max(cols) - min(cols)
        dy = max(rows) - min(rows)
        costs.append(placement_cost(dx, dy, cost_model))

    if not costs:
        return {
            "blif_net_count": "",
            "blif_cost_sum": "",
            "blif_avg_cost": "",
            "blif_max_cost": "",
            "blif_optimal_net_count": "",
            "blif_optimal_net_ratio": "",
            "blif_paper_fifo_sum": "",
            "blif_avg_paper_fifo": "",
            "blif_max_paper_fifo": "",
        }
    fifo_values = [max(0, cost - 1) for cost in costs]
    return {
        "blif_net_count": len(costs),
        "blif_cost_sum": sum(costs),
        "blif_avg_cost": safe_ratio(sum(costs), len(costs)),
        "blif_max_cost": max(costs),
        "blif_optimal_net_count": sum(1 for cost in costs if cost <= 1),
        "blif_optimal_net_ratio": safe_ratio(
            sum(1 for cost in costs if cost <= 1), len(costs)
        ),
        "blif_paper_fifo_sum": sum(fifo_values),
        "blif_avg_paper_fifo": safe_ratio(sum(fifo_values), len(fifo_values)),
        "blif_max_paper_fifo": max(fifo_values),
    }


def compute_bbox_metrics(positions: dict) -> tuple:
    if not positions:
        return "", ""
    rows = [row for row, _ in positions.values()]
    cols = [col for _, col in positions.values()]
    area = (max(rows) - min(rows) + 1) * (max(cols) - min(cols) + 1)
    return area, safe_ratio(len(set(positions.values())), area)


def has_vpr_option(command: list, option: str) -> bool:
    return any(str(arg) == option for arg in command)


def vpr_option_value(command: list, option: str) -> str:
    for index, arg in enumerate(command):
        if str(arg) == option and index + 1 < len(command):
            return str(command[index + 1])
    return ""


def with_vpr_output_files(command: list, blif_path: Path, include_route: bool = False, route_file: Path = None) -> list:
    work_dir = blif_path.parent
    stem = blif_path.stem
    outputs = [
        ("--net_file", work_dir / f"{stem}.net"),
        ("--place_file", work_dir / f"{stem}.place"),
    ]
    if include_route:
        outputs.append(("--route_file", route_file if route_file else work_dir / f"{stem}.route"))
    for option, path in outputs:
        if not has_vpr_option(command, option):
            command.extend([option, str(Path(path).resolve())])
    return command


def vpr_command(vpr_bin: str, arch_xml: Path, blif_path: Path, mapper_config: dict) -> list:
    args = mapper_config.get("vpr_args")
    if args:
        command = [vpr_bin, str(arch_xml), str(blif_path)] + [expand_env(arg) for arg in args]
        command.extend(expand_env(arg) for arg in mapper_config.get("vpr_extra_args", []))
        return with_vpr_output_files(command, blif_path, include_route=has_vpr_option(command, "--route"))

    place_algorithm = str(mapper_config.get("place_algorithm", "bounding_box"))
    absorb_buffer_luts = "on" if bool(mapper_config.get("absorb_buffer_luts", False)) else "off"
    command = [
        vpr_bin,
        str(arch_xml),
        str(blif_path),
        "--pack",
        "--place",
        "--absorb_buffer_luts",
        absorb_buffer_luts,
        "--place_algorithm",
        place_algorithm,
        "--disp",
        "off",
    ]
    if mapper_config.get("seed") not in (None, ""):
        command.extend(["--seed", str(int(mapper_config["seed"]))])
    if bool(mapper_config.get("place_all_ops", False)):
        const_gen_inference = str(mapper_config.get("const_gen_inference", "none"))
        constant_net_method = str(mapper_config.get("constant_net_method", "route"))
        command.extend(
            [
                "--timing_analysis",
                "off",
                "--const_gen_inference",
                const_gen_inference,
                "--constant_net_method",
                constant_net_method,
                "--sweep_dangling_primary_ios",
                "off",
                "--sweep_dangling_nets",
                "off",
                "--sweep_dangling_blocks",
                "off",
                "--sweep_constant_primary_outputs",
                "off",
            ]
        )
    command.extend(expand_env(arg) for arg in mapper_config.get("vpr_extra_args", []))
    return with_vpr_output_files(command, blif_path)


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

    arch = architecture_summary(arch_template)
    work_dir = output_dir / "vpr"
    work_dir.mkdir(parents=True, exist_ok=True)
    arch_xml = prepare_vpr_arch_xml(arch_xml, mapper_config, work_dir, arch)
    blif_path = (work_dir / f"{benchmark}.blif").resolve()
    name_map_path = work_dir / "vpr_name_map.json"
    dfg_data = write_blif(
        dfg,
        blif_path,
        name_map_path,
        protect_internal_buffer_luts=bool(
            mapper_config.get("protect_internal_buffer_luts", False)
        ),
        output_nodes_as_io_pads=bool(
            mapper_config.get("output_nodes_as_io_pads", False)
        ),
        output_lut_io_pads=bool(mapper_config.get("output_lut_io_pads", False)),
        input_nodes_as_luts=bool(mapper_config.get("input_nodes_as_luts", False)),
        dfg_nodes_as_luts_only=bool(mapper_config.get("dfg_nodes_as_luts_only", False)),
    )
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

    place_path = find_place_file(work_dir, blif_path)
    net_path = find_net_file(work_dir, blif_path)
    top_positions, parsed_rows, parsed_cols = parse_place_file(place_path, dfg_data["reverse_map"])
    positions = parse_packed_net_positions(net_path, top_positions, dfg_data["reverse_map"])
    recovered_buffer_nodes = []
    if bool(mapper_config.get("recover_optimized_buffers", True)):
        positions, recovered_buffer_nodes = recover_buffer_like_positions(dfg, positions)
    # Use the effective CGRA dimensions from the experiment manifest for
    # fair metric denominators. VPR's `.place` array size may exclude how this
    # repository accounts for DFG input/output nodes as CGRA operations.
    rows = arch["rows"]
    cols = arch["cols"]
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
    blif_placement = blif_net_metrics(
        blif_path,
        dfg_data["reverse_map"],
        positions,
        arch["placement_cost_model"],
    )
    placement_log_metrics = parse_placement_log(stdout_path)
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
        "placement_swap_attempts": placement_log_metrics[
            "placement_swap_attempts"
        ],
        "placement_cell_visits": placement_log_metrics["placement_cell_visits"],
        "vpr_initial_placement_cost": placement_log_metrics[
            "vpr_initial_placement_cost"
        ],
        "vpr_initial_bb_cost": placement_log_metrics["vpr_initial_bb_cost"],
        "vpr_final_placement_cost": placement_log_metrics[
            "vpr_final_placement_cost"
        ],
        "vpr_final_bb_cost": placement_log_metrics["vpr_final_bb_cost"],
        "wall_time_sec": elapsed,
        "vpr_inner_num": vpr_option_value(cmd, "--inner_num"),
        "vpr_extra_args": " ".join(str(arg) for arg in mapper_config.get("vpr_extra_args", [])),
        "vpr_io_capacity": mapper_config.get("vpr_io_capacity", ""),
        "dfg_nodes_as_luts_only": mapper_config.get("dfg_nodes_as_luts_only", ""),
        "fixed_layout_all_clb": mapper_config.get("fixed_layout_all_clb", ""),
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
        **blif_placement,
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
