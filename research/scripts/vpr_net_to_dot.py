#!/usr/bin/env python3

import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path


def sanitize_dot_id(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not value:
        value = "node"
    if value[0].isdigit():
        value = f"n_{value}"
    return value


def clean_net_token(value: str) -> str:
    value = value.strip()
    if not value or value == "open":
        return ""
    value = value.split("->", 1)[0]
    return value.strip()


def opcode_from_name(name: str) -> str:
    prefix = name.split("_", 1)[0].lower()
    if prefix in {"mul", "mult"}:
        return "mul"
    if prefix in {"load", "input", "in"}:
        return "load"
    if prefix in {"output", "out", "store"}:
        return "output"
    if prefix in {"select", "mux"}:
        return "select"
    return "add"


def direct_child_blocks(root):
    return [
        child for child in root
        if child.tag == "block" and child.attrib.get("name", "") != "open"
    ]


def parse_vpr_net(path: Path):
    root = ET.parse(path).getroot()
    blocks = direct_child_blocks(root)
    raw_names = [block.attrib.get("name", "") for block in blocks]
    raw_name_set = set(raw_names)

    used_ids = set()
    name_to_id = {}
    nodes = []
    for raw_name in raw_names:
        node_id = sanitize_dot_id(raw_name)
        base = node_id
        suffix = 1
        while node_id in used_ids:
            suffix += 1
            node_id = f"{base}_{suffix}"
        used_ids.add(node_id)
        name_to_id[raw_name] = node_id
        nodes.append((node_id, opcode_from_name(raw_name)))

    edges = []
    seen_edges = set()
    for block in blocks:
        dst_name = block.attrib.get("name", "")
        dst = name_to_id.get(dst_name)
        if not dst:
            continue
        inputs = block.find("./inputs")
        if inputs is None:
            continue
        for port in inputs.findall("./port"):
            for raw_token in (port.text or "").split():
                src_name = clean_net_token(raw_token)
                if src_name not in raw_name_set or src_name == dst_name:
                    continue
                src = name_to_id[src_name]
                edge = (src, dst)
                if edge in seen_edges:
                    continue
                seen_edges.add(edge)
                edges.append(edge)
    return nodes, edges


def write_dot(nodes, edges, out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["digraph G {"]
    for node, opcode in nodes:
        lines.append(f'  {node} [opcode="{opcode}"];')
    for index, (src, dst) in enumerate(edges):
        lines.append(f"  {src} -> {dst} [operand={index % 2}];")
    lines.append("}")
    out_path.write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(
        description="Convert a packed VPR .net file into a placement-quality DFG dot file.")
    parser.add_argument("input_net", type=Path)
    parser.add_argument("output_dot", type=Path)
    args = parser.parse_args()
    nodes, edges = parse_vpr_net(args.input_net)
    if not nodes:
        raise SystemExit(f"no packed VPR blocks found in {args.input_net}")
    write_dot(nodes, edges, args.output_dot)
    print(f"wrote {args.output_dot} ({len(nodes)} nodes, {len(edges)} edges)")


if __name__ == "__main__":
    main()
