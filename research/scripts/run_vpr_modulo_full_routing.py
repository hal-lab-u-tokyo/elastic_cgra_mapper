#!/usr/bin/env python3

import argparse
import json
import re
import subprocess
import time
import xml.etree.ElementTree as ET
from pathlib import Path

from lib import (
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
    original_name_from_vpr_block,
    parse_packed_net_positions,
    parse_place_file,
    prepare_vpr_arch_xml,
    recover_buffer_like_positions,
    sanitize_blif_name,
    unique_blif_names,
    unresolved_env,
)
from run_vpr_modulo_routing import (
    NOP_OP,
    ROUTE_OP,
    add_unique_config_id,
    adjacent_nodes,
    config_id,
    context_candidates,
    get_config,
    make_empty_mapping,
    read_dfg_ops_and_edges,
    route_one_edge,
    run_vpr_placement,
    supported_ops_for_position,
)


ROUTE_NODE_RE = re.compile(
    r"Node:\s+(?P<id>\d+)\s+"
    r"(?P<kind>SOURCE|SINK|OPIN|IPIN|CHANX|CHANY)"
    r"\s+\((?P<x1>-?\d+),(?P<y1>-?\d+)\)"
    r"(?:\s+to\s+\((?P<x2>-?\d+),(?P<y2>-?\d+)\))?"
)
NET_RE = re.compile(r"Net\s+\d+\s+\((?P<name>[^)]+)\)")


def read_dfg_for_blif(dfg_path: Path) -> tuple:
    nodes, _, _, edges = read_dfg_ops_and_edges(dfg_path)
    predecessors = {node: [] for node in nodes}
    successors = {node: [] for node in nodes}
    for src, dst in edges:
        predecessors[dst].append(src)
        successors[src].append(dst)
    return nodes, edges, predecessors, successors


def write_all_ops_blif(dfg_path: Path, out_path: Path, name_map_path: Path) -> dict:
    nodes, edges, predecessors, successors = read_dfg_for_blif(dfg_path)
    name_map, reverse_map = unique_blif_names(nodes)
    terminal_nodes = [node for node in nodes if not successors[node]]

    lines = [".model cgra_dfg"]
    lines.append(".inputs")
    # Keep DFG terminals as ordinary operation blocks. Primary outputs would add
    # artificial IO routes that are not present in the CGRA mapping problem.
    lines.append(".outputs")
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


def resolve_mii(mii_arg: str, dfg: Path, arch_template: Path, out_dir: Path, missing_distance_policy: str) -> tuple:
    mii_data = compute_res_mii(dfg, arch_template, missing_distance_policy)
    write_json(out_dir / "mii.json", mii_data)
    computed_mii = int(mii_data["MII"]) if mii_data["MII"] is not None else None
    if mii_arg == "auto":
        if mii_data["MII"] is None:
            raise ValueError(f"Cannot compute MII; unsupported ops: {mii_data['unsupported_ops']}")
        return computed_mii, computed_mii
    return computed_mii, int(mii_arg)


def make_fixed_modulo_vpr_arch(arch_xml: Path, mapper_config: dict, work_dir: Path, rows: int, cols: int, ii: int) -> Path:
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
            "name": "cgra_modulo",
            "width": str(cols + 2),
            "height": str(rows * ii + 2),
        },
    )
    ET.SubElement(fixed, "perimeter", {"type": "io", "priority": "100"})
    ET.SubElement(fixed, "corners", {"type": "EMPTY", "priority": "101"})
    ET.SubElement(fixed, "fill", {"type": "clb", "priority": "10"})

    for segment in root.findall("./segmentlist/segment"):
        segment.set("length", str(int(mapper_config.get("segment_length", 1))))
        segment.set("type", mapper_config.get("segment_type", "unidir"))
        for sb in segment.findall("sb"):
            sb.set("type", "pattern")
            sb.text = "1 1"
        for cb in segment.findall("cb"):
            cb.set("type", "pattern")
            cb.text = "1"

    generated_arch = work_dir / "vpr_cgra_modulo_arch.xml"
    tree.write(generated_arch, encoding="unicode")
    return generated_arch.resolve()


def vpr_full_route_command(vpr_bin: str, arch_xml: Path, blif_path: Path, route_file: Path, mapper_config: dict) -> list:
    args = mapper_config.get("vpr_args")
    if args:
        return [vpr_bin, str(arch_xml), str(blif_path)] + [expand_env(arg) for arg in args]
    return [
        vpr_bin,
        str(arch_xml),
        str(blif_path),
        "--pack",
        "--place",
        "--route",
        "--absorb_buffer_luts",
        "off",
        "--constant_net_method",
        "route",
        "--place_algorithm",
        str(mapper_config.get("place_algorithm", "bounding_box")),
        "--route_chan_width",
        str(int(mapper_config.get("route_chan_width", 32))),
        "--route_file",
        str(route_file),
        "--device",
        "cgra_modulo",
        "--disp",
        "off",
    ]


def vpr_pack_place_command(vpr_bin: str, arch_xml: Path, blif_path: Path, rr_graph_file: Path, mapper_config: dict) -> list:
    return [
        vpr_bin,
        str(arch_xml),
        str(blif_path),
        "--pack",
        "--place",
        "--absorb_buffer_luts",
        "off",
        "--timing_analysis",
        "off",
        "--const_gen_inference",
        "none",
        "--constant_net_method",
        "route",
        "--sweep_dangling_primary_ios",
        "off",
        "--sweep_dangling_nets",
        "off",
        "--sweep_dangling_blocks",
        "off",
        "--sweep_constant_primary_outputs",
        "off",
        "--place_algorithm",
        str(mapper_config.get("place_algorithm", "bounding_box")),
        "--route_chan_width",
        str(int(mapper_config.get("route_chan_width", 32))),
        "--write_rr_graph",
        str(rr_graph_file),
        "--device",
        "cgra_modulo",
        "--disp",
        "off",
    ]


def vpr_route_with_rr_graph_command(vpr_bin: str, arch_xml: Path, blif_path: Path, net_file: Path, place_file: Path, rr_graph_file: Path, route_file: Path, mapper_config: dict) -> list:
    return [
        vpr_bin,
        str(arch_xml),
        str(blif_path),
        "--route",
        "--net_file",
        str(net_file),
        "--place_file",
        str(place_file),
        "--read_rr_graph",
        str(rr_graph_file),
        "--timing_analysis",
        "off",
        "--router_algorithm",
        "breadth_first",
        "--bb_factor",
        str(int(mapper_config.get("route_bb_factor", 1000))),
        "--route_chan_width",
        str(int(mapper_config.get("route_chan_width", 32))),
        "--route_file",
        str(route_file),
        "--absorb_buffer_luts",
        "off",
        "--const_gen_inference",
        "none",
        "--constant_net_method",
        "route",
        "--sweep_dangling_primary_ios",
        "off",
        "--sweep_dangling_nets",
        "off",
        "--sweep_dangling_blocks",
        "off",
        "--sweep_constant_primary_outputs",
        "off",
        "--verify_file_digests",
        "off",
        "--device",
        "cgra_modulo",
        "--disp",
        "off",
    ]


def clone_xml_child(parent: ET.Element, child: ET.Element) -> ET.Element:
    return ET.fromstring(ET.tostring(child, encoding="unicode"))


def xml_loc(node: ET.Element) -> dict:
    loc = node.find("loc")
    return {
        "xlow": int(loc.get("xlow")),
        "ylow": int(loc.get("ylow")),
        "xhigh": int(loc.get("xhigh")),
        "yhigh": int(loc.get("yhigh")),
        "ptc": int(loc.get("ptc")),
        "side": loc.get("side", ""),
    }


def xml_metadata(node: ET.Element) -> dict:
    metadata = {}
    metadata_node = node.find("metadata")
    if metadata_node is None:
        return metadata
    for meta in metadata_node.findall("meta"):
        metadata[meta.get("name", "")] = meta.text or ""
    return metadata


def parse_cgra_loc_metadata(node: ET.Element):
    raw = xml_metadata(node).get("cgra_loc")
    if not raw:
        return None
    parts = [part.strip() for part in raw.split(",")]
    if len(parts) != 3:
        return None
    return tuple(int(part) for part in parts)


def cgra_loc_from_vpr_xy(x: int, y: int, rows: int, cols: int, ii: int):
    if not (1 <= x <= cols and 1 <= y <= rows * ii):
        return None
    return site_to_cgra_loc(x, y, rows, cols, ii)


def node_cgra_loc(node: ET.Element, rows: int, cols: int, ii: int):
    metadata = xml_metadata(node)
    if metadata.get("bridge") == "true":
        return None
    loc_from_metadata = parse_cgra_loc_metadata(node)
    if loc_from_metadata is not None:
        return loc_from_metadata
    loc = xml_loc(node)
    return cgra_loc_from_vpr_xy(loc["xlow"], loc["ylow"], rows, cols, ii)


def cgra_loc_to_vpr_position(loc: tuple, ii: int) -> tuple:
    row, col, context = loc
    return col + 1, row * ii + context + 1


def write_fixed_place_file(template_place: Path, fixed_place: Path, op_locations: dict, rows: int, cols: int, ii: int, reverse_map: dict) -> None:
    lines = []
    placed = set()
    for raw_line in template_place.read_text(errors="replace").splitlines():
        line = raw_line.rstrip("\n")
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or stripped.startswith("Netlist_File") or stripped.startswith("Array size"):
            lines.append(line)
            continue
        parts = stripped.split()
        if len(parts) < 4:
            lines.append(line)
            continue
        original = original_name_from_vpr_block(parts[0], reverse_map) or reverse_map.get(parts[0], "")
        if original not in op_locations:
            lines.append(line)
            continue
        x, y = cgra_loc_to_vpr_position(op_locations[original], ii)
        placed.add(original)
        suffix = " ".join(parts[3:])
        lines.append(f"{parts[0]}\t\t{x}\t{y}\t{suffix}")
    missing = sorted(set(op_locations) - placed)
    if missing:
        raise RuntimeError(f"fixed VPR place file missed {len(missing)} DFG blocks")
    fixed_place.write_text("\n".join(lines) + "\n")


def write_custom_cgra_rr_graph(base_rr_graph: Path, custom_rr_graph: Path, arch: dict, op_locations: dict, mapper_config: dict) -> dict:
    rows = int(arch["row"])
    cols = int(arch["column"])
    ii = int(arch["context_size"])
    tree = ET.parse(base_rr_graph)
    base_root = tree.getroot()
    new_root = ET.Element("rr_graph", dict(base_root.attrib))

    for tag in ("channels", "switches", "segments", "block_types", "grid"):
        child = base_root.find(tag)
        if child is not None:
            new_root.append(clone_xml_child(new_root, child))

    base_nodes = base_root.find("rr_nodes")
    base_edges = base_root.find("rr_edges")
    new_nodes = ET.SubElement(new_root, "rr_nodes")
    new_edges = ET.SubElement(new_root, "rr_edges")

    id_map = {}
    node_types = {}
    loc_to_opins = {}
    loc_to_ipins = {}
    old_id_to_loc = {}
    next_node_id = 0

    def add_node_like(old_node: ET.Element) -> int:
        nonlocal next_node_id
        new_id = next_node_id
        next_node_id += 1
        old_id = int(old_node.get("id"))
        copied = clone_xml_child(new_nodes, old_node)
        copied.set("id", str(new_id))
        new_nodes.append(copied)
        id_map[old_id] = new_id
        node_types[new_id] = copied.get("type")
        loc = node_cgra_loc(copied, rows, cols, ii)
        if loc is not None:
            old_id_to_loc[old_id] = loc
            if copied.get("type") == "OPIN":
                loc_to_opins.setdefault(loc, []).append(new_id)
            elif copied.get("type") == "IPIN":
                loc_to_ipins.setdefault(loc, []).append(new_id)
        return new_id

    for old_node in base_nodes.findall("node"):
        if old_node.get("type") in {"SOURCE", "SINK", "OPIN", "IPIN"} and node_cgra_loc(old_node, rows, cols, ii) is not None:
            add_node_like(old_node)

    occupied = set(op_locations.values())
    route_node_by_loc = {}
    node_xy = {}
    bridge_node_count = 0
    synthetic_ptc_by_span = {}
    for context in range(ii):
        for row in range(rows):
            for col in range(cols):
                loc = (row, col, context)
                if loc in occupied:
                    continue
                if ROUTE_OP not in supported_ops_for_position(row, col, arch):
                    continue
                node_id = next_node_id
                next_node_id += 1
                route_node_by_loc[loc] = node_id
                node_types[node_id] = "CHANX"
                x = col + 1
                y = row * ii + context + 1
                node_xy[node_id] = (x, y)
                node = ET.SubElement(
                    new_nodes,
                    "node",
                    {
                        "id": str(node_id),
                        "type": "CHANX",
                        "direction": "BI_DIR",
                        "capacity": "1",
                    },
                )
                ET.SubElement(
                    node,
                    "loc",
                    {
                        "xlow": str(x),
                        "ylow": str(y),
                        "xhigh": str(x),
                        "yhigh": str(y),
                        "ptc": "0",
                    },
                )
                ET.SubElement(node, "timing", {"R": "0", "C": "0"})
                ET.SubElement(node, "segment", {"segment_id": "0"})
                metadata = ET.SubElement(node, "metadata")
                ET.SubElement(metadata, "meta", {"name": "cgra_loc"}).text = f"{row},{col},{context}"

    edge_set = set()

    def add_edge(src: int, sink: int, switch_id: str = "0") -> None:
        if src == sink:
            return
        key = (src, sink, str(switch_id))
        if key in edge_set:
            return
        edge_set.add(key)
        ET.SubElement(new_edges, "edge", {"src_node": str(src), "sink_node": str(sink), "switch_id": str(switch_id)})

    def add_bridge_node(node_type: str, xlow: int, ylow: int, xhigh: int, yhigh: int) -> int:
        nonlocal next_node_id, bridge_node_count
        node_id = next_node_id
        next_node_id += 1
        bridge_node_count += 1
        node_types[node_id] = node_type
        node_xy[node_id] = (xlow, ylow)
        span_key = (node_type, xlow, ylow, xhigh, yhigh)
        ptc = synthetic_ptc_by_span.get(span_key, 0)
        synthetic_ptc_by_span[span_key] = ptc + 1
        node = ET.SubElement(
            new_nodes,
            "node",
            {
                "id": str(node_id),
                "type": node_type,
                "direction": "BI_DIR",
                "capacity": "1",
            },
        )
        ET.SubElement(
            node,
            "loc",
            {
                "xlow": str(xlow),
                "ylow": str(ylow),
                "xhigh": str(xhigh),
                "yhigh": str(yhigh),
                "ptc": str(ptc),
            },
        )
        ET.SubElement(node, "timing", {"R": "0", "C": "0"})
        ET.SubElement(node, "segment", {"segment_id": "0"})
        metadata = ET.SubElement(node, "metadata")
        ET.SubElement(metadata, "meta", {"name": "bridge"}).text = "true"
        return node_id

    geometric_edge_cache = {}

    def add_geometric_route_edge(src: int, sink: int) -> None:
        """Represent one logical CGRA routing hop using VPR-adjacent RR segments.

        The CGRA modulo graph is a time-expanded routing graph, so a legal CGRA
        hop can look like a long jump in VPR's 2D coordinate system. VPR's route
        checker validates geometric adjacency in addition to edge existence; the
        bridge nodes below make that checker see a normal CHANX/CHANY path while
        the importer ignores the bridge nodes and validates the logical CGRA hop.
        """
        if src == sink:
            return
        cache_key = (src, sink)
        if cache_key in geometric_edge_cache:
            chain = geometric_edge_cache[cache_key]
            for a, b in zip(chain, chain[1:]):
                add_edge(a, b)
            return
        src_type = node_types[src]
        sink_type = node_types[sink]
        if src_type != "CHANX" or sink_type != "CHANX":
            add_edge(src, sink)
            geometric_edge_cache[cache_key] = [src, sink]
            return
        x1, y1 = node_xy[src]
        x2, y2 = node_xy[sink]
        if y1 == y2 and abs(x1 - x2) <= 1:
            add_edge(src, sink)
            geometric_edge_cache[cache_key] = [src, sink]
            return

        chain = [src]
        if y1 == y2:
            chain.append(add_bridge_node("CHANX", min(x1, x2), y1, max(x1, x2), y1))
        else:
            chain.append(add_bridge_node("CHANY", x1, min(y1, y2), x1, max(y1, y2)))
            if x1 != x2:
                chain.append(add_bridge_node("CHANX", min(x1, x2), y2, max(x1, x2), y2))
        chain.append(sink)
        geometric_edge_cache[cache_key] = chain
        for a, b in zip(chain, chain[1:]):
            add_edge(a, b)

    for edge in base_edges.findall("edge"):
        old_src = int(edge.get("src_node"))
        old_sink = int(edge.get("sink_node"))
        if old_src not in id_map or old_sink not in id_map:
            continue
        src_type = node_types[id_map[old_src]]
        sink_type = node_types[id_map[old_sink]]
        if (src_type == "SOURCE" and sink_type == "OPIN") or (src_type == "IPIN" and sink_type == "SINK"):
            add_edge(id_map[old_src], id_map[old_sink], edge.get("switch_id", "0"))

    def add_ipin_edges(src_id: int, target_loc: tuple) -> None:
        for ipin in loc_to_ipins.get(target_loc, []):
            add_edge(src_id, ipin)

    for loc, opins in loc_to_opins.items():
        for nxt in adjacent_nodes(loc, arch):
            route_id = route_node_by_loc.get(nxt)
            for opin in opins:
                if route_id is not None:
                    add_edge(opin, route_id)
                add_ipin_edges(opin, nxt)

    for loc, route_id in route_node_by_loc.items():
        for nxt in adjacent_nodes(loc, arch):
            next_route_id = route_node_by_loc.get(nxt)
            if next_route_id is not None:
                add_geometric_route_edge(route_id, next_route_id)
            add_ipin_edges(route_id, nxt)

    ET.ElementTree(new_root).write(custom_rr_graph, encoding="unicode")
    return {
        "route_node_by_loc": {",".join(map(str, loc)): node_id for loc, node_id in route_node_by_loc.items()},
        "bridge_node_count": bridge_node_count,
    }


def op_locations_from_vpr_place(dfg_data: dict, place_path: Path, net_path: Path, ops: dict, arch: dict) -> tuple:
    rows = int(arch["row"])
    cols = int(arch["column"])
    ii = int(arch["context_size"])
    top_positions, _, _ = parse_place_file(place_path, dfg_data["reverse_map"])
    positions = parse_packed_net_positions(net_path, top_positions, dfg_data["reverse_map"])
    missing = sorted(node for node in dfg_data["nodes"] if node not in positions)
    if missing:
        raise RuntimeError(f"VPR placement missed {len(missing)} DFG blocks")

    op_locations = {}
    op_occupancy = {}
    normalized_positions = {}
    for node in dfg_data["nodes"]:
        loc = vpr_position_to_cgra(positions[node], rows, cols, ii)
        if ops[node] not in supported_ops_for_position(loc[0], loc[1], arch):
            raise RuntimeError(f"operation {node} ({ops[node]}) is not supported at VPR location {loc}")
        if loc in op_occupancy:
            raise RuntimeError(f"multiple operations mapped to {loc}: {op_occupancy[loc]} and {node}")
        op_locations[node] = loc
        op_occupancy[loc] = node
        normalized_positions[node] = (loc[0] * ii + loc[2] + 1, loc[1] + 1)
    return op_locations, normalized_positions


def run_vpr_full_route(dfg: Path, arch: dict, output_dir: Path, benchmark: str, mapper_config: dict, timeout_sec: float, op_locations: dict = None, ops: dict = None, attempt_name: str = "vpr_full_route", progress: bool = False) -> dict:
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

    work_dir = output_dir / attempt_name
    work_dir.mkdir(parents=True, exist_ok=True)
    rows = int(arch["row"])
    cols = int(arch["column"])
    ii = int(arch["context_size"])
    vpr_arch = make_fixed_modulo_vpr_arch(arch_xml.resolve(), mapper_config, work_dir, rows, cols, ii)
    blif_path = (work_dir / f"{benchmark}.blif").resolve()
    name_map_path = work_dir / "vpr_name_map.json"
    dfg_data = write_all_ops_blif(dfg, blif_path, name_map_path)
    route_file = (work_dir / f"{benchmark}.route").resolve()
    stdout_path = work_dir / "stdout.txt"
    stderr_path = work_dir / "stderr.txt"
    base_rr_graph = (work_dir / "base_rr_graph.xml").resolve()
    custom_rr_graph = (work_dir / "cgra_mrrg_rr_graph.xml").resolve()
    cmd = vpr_pack_place_command(vpr_bin, vpr_arch, blif_path, base_rr_graph, mapper_config)
    write_json(work_dir / "command.json", {"cmd": cmd, "cwd": str(work_dir)})

    started_at = time.time()
    if progress:
        print(f"  VPR pack/place: running {' '.join(cmd)}", flush=True)
    proc = subprocess.run(cmd, cwd=work_dir, capture_output=True, text=True, timeout=timeout_sec)
    pack_place_elapsed = time.time() - started_at
    stdout_path.write_text(proc.stdout)
    stderr_path.write_text(proc.stderr)
    if proc.returncode != 0:
        raise RuntimeError(f"VPR pack/place returned {proc.returncode}")

    place_path = find_place_file(work_dir, blif_path).resolve()
    net_path = find_net_file(work_dir, blif_path).resolve()
    if op_locations is None:
        if ops is None:
            _, ops, _, _ = read_dfg_ops_and_edges(dfg)
        op_locations, positions = op_locations_from_vpr_place(dfg_data, place_path, net_path, ops, arch)
        fixed_place_path = place_path
    else:
        missing = sorted(node for node in dfg_data["nodes"] if node not in op_locations)
        if missing:
            raise RuntimeError(f"fixed VPR routing placement missing {len(missing)} DFG nodes")
        positions = {
            node: (op_locations[node][0] * ii + op_locations[node][2] + 1, op_locations[node][1] + 1)
            for node in dfg_data["nodes"]
        }
        fixed_place_path = (work_dir / f"{benchmark}_fixed.place").resolve()
        write_fixed_place_file(place_path, fixed_place_path, op_locations, rows, cols, ii, dfg_data["reverse_map"])
    rr_info = write_custom_cgra_rr_graph(base_rr_graph, custom_rr_graph, arch, op_locations, mapper_config)
    write_json(work_dir / "custom_rr_graph_info.json", rr_info)

    route_stdout_path = work_dir / "route_stdout.txt"
    route_stderr_path = work_dir / "route_stderr.txt"
    route_cmd = vpr_route_with_rr_graph_command(
        vpr_bin,
        vpr_arch,
        blif_path,
        net_path,
        fixed_place_path,
        custom_rr_graph,
        route_file,
        mapper_config,
    )
    write_json(work_dir / "route_command.json", {"cmd": route_cmd, "cwd": str(work_dir)})
    if progress:
        print(f"  VPR route custom RR graph: running {' '.join(route_cmd)}", flush=True)
    remaining_timeout = max(1.0, timeout_sec - pack_place_elapsed)
    route_proc = subprocess.run(route_cmd, cwd=work_dir, capture_output=True, text=True, timeout=remaining_timeout)
    elapsed = time.time() - started_at
    route_stdout_path.write_text(route_proc.stdout)
    route_stderr_path.write_text(route_proc.stderr)
    if route_proc.returncode != 0:
        raise RuntimeError(f"VPR custom-RR route returned {route_proc.returncode}")
    if not route_file.exists():
        raise RuntimeError("VPR did not write a route file")
    if dfg_data["edges"] and "total_nets_routed: 0" in route_proc.stdout:
        raise RuntimeError("VPR custom-RR route succeeded without routing any nets")
    return {
        "positions": positions,
        "reverse_map": dfg_data["reverse_map"],
        "route_file": route_file,
        "rr_graph_file": custom_rr_graph,
        "elapsed": elapsed,
        "stdout_file": route_stdout_path,
        "stderr_file": route_stderr_path,
        "work_dir": work_dir,
    }


def site_to_cgra_loc(x: int, y: int, rows: int, cols: int, ii: int) -> tuple:
    core_x = min(max(x, 1), cols)
    core_y = min(max(y, 1), rows * ii)
    col = core_x - 1
    flat_y = core_y - 1
    row = flat_y // ii
    context = flat_y % ii
    return (row, col, context)


def cgra_to_vpr_site(loc: tuple) -> tuple:
    row, col, context = loc
    return (col + 1, row * 10_000 + context + 1)


def vpr_position_to_cgra(position: tuple, rows: int, cols: int, ii: int) -> tuple:
    row, col = position
    return site_to_cgra_loc(col, row, rows, cols, ii)


def route_node_locations(line: str, rows: int, cols: int, ii: int) -> list:
    match = ROUTE_NODE_RE.search(line)
    if not match:
        return []
    x1 = int(match.group("x1"))
    y1 = int(match.group("y1"))
    x2 = int(match.group("x2") or x1)
    y2 = int(match.group("y2") or y1)
    kind = match.group("kind")
    locs = []
    if kind == "CHANX":
        step = 1 if x2 >= x1 else -1
        for x in range(x1, x2 + step, step):
            locs.append(site_to_cgra_loc(x, y1, rows, cols, ii))
    elif kind == "CHANY":
        step = 1 if y2 >= y1 else -1
        for y in range(y1, y2 + step, step):
            locs.append(site_to_cgra_loc(x1, y, rows, cols, ii))
    else:
        locs.append(site_to_cgra_loc(x1, y1, rows, cols, ii))
    result = []
    for loc in locs:
        if not result or result[-1] != loc:
            result.append(loc)
    return result


def parse_vpr_route(route_file: Path, reverse_map: dict, rows: int, cols: int, ii: int) -> dict:
    nets = {}
    current_net = ""
    current_branch = []
    last_was_sink = False
    for raw_line in route_file.read_text(errors="replace").splitlines():
        line = raw_line.strip()
        net_match = NET_RE.search(line)
        if net_match:
            raw_name = net_match.group("name")
            current_net = original_name_from_vpr_block(raw_name, reverse_map) or reverse_map.get(raw_name, raw_name)
            nets.setdefault(current_net, [])
            current_branch = []
            last_was_sink = False
            continue
        if not current_net or "Node:" not in line:
            continue
        locs = route_node_locations(line, rows, cols, ii)
        if not locs:
            continue
        kind = ROUTE_NODE_RE.search(line).group("kind")
        if last_was_sink:
            current_branch = list(locs)
            last_was_sink = False
        else:
            for loc in locs:
                if not current_branch or current_branch[-1] != loc:
                    current_branch.append(loc)
        if kind == "SINK":
            nets[current_net].append(list(current_branch))
            last_was_sink = True
    return nets


def read_rr_node_locs(rr_graph: Path, rows: int, cols: int, ii: int) -> dict:
    root = ET.parse(rr_graph).getroot()
    result = {}
    for node in root.find("rr_nodes").findall("node"):
        node_id = int(node.get("id"))
        loc = node_cgra_loc(node, rows, cols, ii)
        if loc is None:
            continue
        result[node_id] = loc
    return result


def parse_vpr_route_by_rr_id(route_file: Path, reverse_map: dict, rr_node_locs: dict) -> dict:
    nets = {}
    current_net = ""
    current_branch = []
    last_was_sink = False
    for raw_line in route_file.read_text(errors="replace").splitlines():
        line = raw_line.strip()
        net_match = NET_RE.search(line)
        if net_match:
            raw_name = net_match.group("name")
            current_net = original_name_from_vpr_block(raw_name, reverse_map) or reverse_map.get(raw_name, raw_name)
            nets.setdefault(current_net, [])
            current_branch = []
            last_was_sink = False
            continue
        if not current_net or "Node:" not in line:
            continue
        match = ROUTE_NODE_RE.search(line)
        if not match:
            continue
        loc = rr_node_locs.get(int(match.group("id")))
        if loc is None:
            continue
        kind = match.group("kind")
        if last_was_sink:
            current_branch = [loc]
            last_was_sink = False
        elif not current_branch or current_branch[-1] != loc:
            current_branch.append(loc)
        if kind == "SINK":
            nets[current_net].append(list(current_branch))
            last_was_sink = True
    return nets


def find_branch_for_edge(branches: list, src_loc: tuple, dst_loc: tuple) -> list:
    best = []
    for branch in branches:
        if dst_loc not in branch:
            continue
        if src_loc in branch:
            start = branch.index(src_loc)
        else:
            start = 0
        end = len(branch) - 1 - list(reversed(branch)).index(dst_loc)
        if start <= end:
            candidate = branch[start : end + 1]
        else:
            candidate = list(reversed(branch[end : start + 1]))
        if not best or len(candidate) < len(best):
            best = candidate
    return best


def connect_legal_segment(source_name: str, start: tuple, target: tuple, op_occupancy: dict, route_node_owner: dict, route_edge_owner: dict, arch: dict) -> list:
    if start == target:
        return [start]
    return route_one_edge(source_name, start, target, op_occupancy, route_node_owner, route_edge_owner, arch)


def legalize_vpr_branch(source_name: str, branch: list, op_occupancy: dict, route_node_owner: dict, route_edge_owner: dict, arch: dict) -> list:
    if not branch:
        return []
    legal_path = [branch[0]]
    current = branch[0]
    for waypoint in branch[1:]:
        segment = connect_legal_segment(source_name, current, waypoint, op_occupancy, route_node_owner, route_edge_owner, arch)
        if segment is None:
            return []
        for loc in segment[1:]:
            if legal_path[-1] != loc:
                legal_path.append(loc)
        current = waypoint
    return legal_path


def free_route_waypoint(loc: tuple, op_occupancy: dict, arch: dict) -> tuple:
    if loc not in op_occupancy:
        return loc
    row, col, context = loc
    ii = int(arch["context_size"])
    for offset in range(1, ii):
        candidate = (row, col, (context + offset) % ii)
        if candidate not in op_occupancy and ROUTE_OP in supported_ops_for_position(row, col, arch):
            return candidate
    return loc


def remap_interior_waypoints(branch: list, op_occupancy: dict, arch: dict) -> list:
    if len(branch) <= 2:
        return branch
    result = [branch[0]]
    for loc in branch[1:-1]:
        candidate = free_route_waypoint(loc, op_occupancy, arch)
        if result[-1] != candidate:
            result.append(candidate)
    if result[-1] != branch[-1]:
        result.append(branch[-1])
    return result


def write_mapping_from_vpr_route(mapping_path: Path, arch: dict, nodes: list, ops: dict, const_values: dict, op_locations: dict, routed_paths: list) -> None:
    mapping = make_empty_mapping(arch)
    for node in nodes:
        loc = op_locations[node]
        config = get_config(mapping, loc)
        config["operation_type"] = ops[node]
        config["operation_name"] = node
        config["const_value"] = int(const_values.get(node, 0))

    for src, dst, path in routed_paths:
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


def import_vpr_routing(nodes: list, ops: dict, const_values: dict, edges: list, arch: dict, positions: dict, route_file: Path, reverse_map: dict, mapping_path: Path, rr_graph: Path) -> None:
    rows = int(arch["row"])
    cols = int(arch["column"])
    ii = int(arch["context_size"])
    op_locations = {}
    op_occupancy = {}
    for node in nodes:
        loc = vpr_position_to_cgra(positions[node], rows, cols, ii)
        if ops[node] not in supported_ops_for_position(loc[0], loc[1], arch):
            raise RuntimeError(f"operation {node} ({ops[node]}) is not supported at VPR location {loc}")
        if loc in op_occupancy:
            raise RuntimeError(f"multiple operations mapped to {loc}: {op_occupancy[loc]} and {node}")
        op_locations[node] = loc
        op_occupancy[loc] = node

    rr_node_locs = read_rr_node_locs(rr_graph, rows, cols, ii)
    vpr_nets = parse_vpr_route_by_rr_id(route_file, reverse_map, rr_node_locs)
    route_node_owner = {}
    route_edge_owner = {}
    routed_paths = []
    missing_edges = []
    for src, dst in edges:
        src_loc = op_locations[src]
        dst_loc = op_locations[dst]
        branch = find_branch_for_edge(vpr_nets.get(src, []), src_loc, dst_loc)
        if not branch:
            missing_edges.append((src, dst))
            continue
        branch[0] = src_loc
        branch[-1] = dst_loc
        branch = remap_interior_waypoints(branch, op_occupancy, arch)
        path = legalize_vpr_branch(src, branch, op_occupancy, route_node_owner, route_edge_owner, arch)
        if not path:
            raise RuntimeError(f"could not legalize VPR route for edge {src}->{dst}")
        for a, b in zip(path, path[1:]):
            route_edge_owner.setdefault((a, b), src)
            if b != src_loc and b != dst_loc and b not in op_occupancy:
                route_node_owner.setdefault(b, src)
        routed_paths.append((src, dst, path))
    if missing_edges:
        raise RuntimeError(f"VPR route import missed {len(missing_edges)} DFG edges")
    write_mapping_from_vpr_route(mapping_path, arch, nodes, ops, const_values, op_locations, routed_paths)


def run_one_vpr_modulo_full_route(
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
    context_seed_count = int(mapper_config.get("context_seed_count", 4))
    random_seed = int(mapper_config.get("random_seed", 3001))
    placement_mode = str(mapper_config.get("modulo_placement_mode", "vpr_context"))
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
        run_dir = raw_dir / "mapping" / f"vpr_full_route_{time.time_ns()}"
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
            "runner": "vpr_modulo_full_route",
        }
        write_json(run_dir / f"input_log_vpr_full_route_ii_{ii}.json", input_log)
        started_at = time.time()
        status = "failed"
        reason = ""
        mapping_path = run_dir / f"mapping_vpr_full_route_ii_{ii}.json"
        stdout_file = trial_dir / "stdout.txt"
        stderr_file = trial_dir / "stderr.txt"
        stdout_file.write_text("")
        stderr_file.write_text("")
        route_file = ""
        try:
            if placement_mode == "vpr_context":
                vpr = run_vpr_full_route(
                    dfg,
                    arch,
                    trial_dir,
                    benchmark,
                    mapper_config,
                    timeout_sec,
                    op_locations=None,
                    ops=ops,
                    attempt_name="vpr_full_route_context_placement",
                    progress=progress,
                )
                stdout_file = vpr["stdout_file"]
                stderr_file = vpr["stderr_file"]
                route_file = str(vpr["route_file"])
                import_vpr_routing(
                    nodes,
                    ops,
                    const_values,
                    edges,
                    arch,
                    vpr["positions"],
                    vpr["route_file"],
                    vpr["reverse_map"],
                    mapping_path,
                    vpr["rr_graph_file"],
                )
                status = "success"
                reason = ""
            elif placement_mode == "physical_then_context":
                placement = run_vpr_placement(dfg, arch_path, trial_dir, benchmark, mapper_config, timeout_sec, progress=progress)
                stdout_file = placement["stdout_file"]
                stderr_file = placement["stderr_file"]
                reason = "VPR custom-RR routing failed for all context assignments"
                for context_index, contexts in enumerate(context_candidates(nodes, edges, ii, random_seed + ii, context_seed_count)):
                    op_locations = {}
                    op_occupancy = {}
                    invalid_reason = ""
                    for node in nodes:
                        row, col = placement["positions"][node]
                        if ops[node] not in supported_ops_for_position(row, col, arch):
                            invalid_reason = f"operation {node} ({ops[node]}) is not supported at physical PE ({row},{col})"
                            break
                        loc = (row, col, int(contexts[node]) % ii)
                        if loc in op_occupancy:
                            invalid_reason = f"multiple operations mapped to {loc}: {op_occupancy[loc]} and {node}"
                            break
                        op_locations[node] = loc
                        op_occupancy[loc] = node
                    if invalid_reason:
                        reason = invalid_reason
                        continue
                    try:
                        vpr = run_vpr_full_route(
                            dfg,
                            arch,
                            trial_dir,
                            benchmark,
                            mapper_config,
                            timeout_sec,
                            op_locations,
                            ops=ops,
                            attempt_name=f"vpr_full_route_ctx_{context_index}",
                            progress=progress,
                        )
                        stdout_file = vpr["stdout_file"]
                        stderr_file = vpr["stderr_file"]
                        route_file = str(vpr["route_file"])
                        import_vpr_routing(
                            nodes,
                            ops,
                            const_values,
                            edges,
                            arch,
                            vpr["positions"],
                            vpr["route_file"],
                            vpr["reverse_map"],
                            mapping_path,
                            vpr["rr_graph_file"],
                        )
                        status = "success"
                        reason = ""
                        break
                    except Exception as exc:
                        reason = f"context {context_index}: {exc}"
            else:
                raise ValueError(f"unknown modulo_placement_mode: {placement_mode}")
        except Exception as exc:
            reason = str(exc)
        elapsed = time.time() - started_at
        write_json(
            run_dir / f"output_log_vpr_full_route_ii_{ii}.json",
            {
                "is_success": status == "success",
                "mapping_time_s": elapsed,
                "external_reason": reason,
                "vpr_route_file": route_file,
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
        row["vpr_route_file"] = route_file
        rows.append(row)
        summary["trials"].append(
            {
                "II": ii,
                "success": status == "success",
                "run_dir": str(run_dir),
                "trial_dir": str(trial_dir),
                "arch_file": str(arch_path),
                "mapping_file": str(mapping_path) if mapping_path.exists() else "",
                "vpr_route_file": route_file,
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
    parser = argparse.ArgumentParser(description="Run VPR pack/place/route and import the route into modulo CGRA mapping.")
    parser.add_argument("--dfg", required=True, type=Path)
    parser.add_argument("--arch-template", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--benchmark", default="")
    parser.add_argument("--mapper-name", default="vpr_sa_full_route")
    parser.add_argument("--arch-name", default="")
    parser.add_argument("--mii", default="auto")
    parser.add_argument("--ii-max", required=True, type=int)
    parser.add_argument("--timeout-sec", required=True, type=float)
    parser.add_argument("--mapper-config-json", default="{}")
    parser.add_argument("--missing-distance-policy", default="self_loop")
    parser.add_argument("--progress", action="store_true")
    args = parser.parse_args()
    summary = run_one_vpr_modulo_full_route(
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
