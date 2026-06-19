#!/usr/bin/env python3

import argparse
import re
from pathlib import Path


def sanitize_dot_id(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not value:
        value = "node"
    if value[0].isdigit():
        value = f"n_{value}"
    return value


def logical_lines(path: Path):
    pending = ""
    for raw in path.read_text(errors="replace").splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if line.endswith("\\"):
            pending += line[:-1].strip() + " "
            continue
        yield (pending + line).strip()
        pending = ""
    if pending:
        yield pending.strip()


def parse_assignment(token: str):
    if "=" not in token:
        return None, None
    key, value = token.split("=", 1)
    return key.strip(), value.strip()


def choose_subckt_output(assignments: dict):
    for key in ("q", "out", "outpad", "result", "O", "Y", "z"):
        if key in assignments:
            return assignments[key]
    if assignments:
        return list(assignments.values())[-1]
    return ""


def parse_blif(path: Path):
    blocks = []
    producer = {}
    inputs_by_block = {}

    for line in logical_lines(path):
        if line.startswith(".names"):
            parts = line.split()
            if len(parts) < 2:
                continue
            wires = parts[1:]
            output = wires[-1]
            if output.lower() in {"gnd", "vcc", "$false", "$true"}:
                continue
            name = f"names_{len(blocks)}_{output}"
            blocks.append((name, output, wires[:-1], "add"))
            producer[output] = name
            inputs_by_block[name] = wires[:-1]
        elif line.startswith(".latch"):
            parts = line.split()
            if len(parts) < 3:
                continue
            input_wire = parts[1]
            output = parts[2]
            name = f"latch_{len(blocks)}_{output}"
            blocks.append((name, output, [input_wire], "select"))
            producer[output] = name
            inputs_by_block[name] = [input_wire]
        elif line.startswith(".subckt"):
            parts = line.split()
            if len(parts) < 3:
                continue
            assignments = {}
            for token in parts[2:]:
                key, value = parse_assignment(token)
                if key and value:
                    assignments[key] = value
            output = choose_subckt_output(assignments)
            if not output:
                continue
            input_wires = [value for key, value in assignments.items()
                           if value != output and key not in {"clk", "clock"}]
            name = f"subckt_{len(blocks)}_{output}"
            blocks.append((name, output, input_wires, "add"))
            producer[output] = name
            inputs_by_block[name] = input_wires

    dot_nodes = []
    dot_edges = []
    used_ids = set()
    name_to_id = {}
    for name, _, _, opcode in blocks:
        node_id = sanitize_dot_id(name)
        base = node_id
        suffix = 1
        while node_id in used_ids:
            suffix += 1
            node_id = f"{base}_{suffix}"
        used_ids.add(node_id)
        name_to_id[name] = node_id
        dot_nodes.append((node_id, opcode))

    edge_seen = set()
    for name, _, _, _ in blocks:
        dst = name_to_id[name]
        for wire in inputs_by_block.get(name, []):
            src_name = producer.get(wire)
            if not src_name or src_name == name:
                continue
            src = name_to_id[src_name]
            edge = (src, dst)
            if edge in edge_seen:
                continue
            edge_seen.add(edge)
            dot_edges.append(edge)

    return dot_nodes, dot_edges


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
        description="Convert a BLIF netlist into a placement-quality DFG dot file.")
    parser.add_argument("input_blif", type=Path)
    parser.add_argument("output_dot", type=Path)
    args = parser.parse_args()
    nodes, edges = parse_blif(args.input_blif)
    if not nodes:
        raise SystemExit(f"no BLIF blocks found in {args.input_blif}")
    write_dot(nodes, edges, args.output_dot)
    print(f"wrote {args.output_dot} ({len(nodes)} nodes, {len(edges)} edges)")


if __name__ == "__main__":
    main()
