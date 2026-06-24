#!/usr/bin/env python3

import argparse
import csv
import json
import re
import xml.etree.ElementTree as ET
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

import networkx as nx
import pydot


SUPPORTED_MAPPER_OPS = {
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
    "or",
    "shift",
    "icmp",
    "cmpgt",
    "cmpge",
    "cmpeq",
    "loop",
    "select",
}

CONTROL_OPS = {"br", "branch"}

# These aliases preserve the resource class that matters for mapping research:
# memory operations stay memory operations, control-only operations are removed,
# and ALU/logic variants are folded into supported one-cycle ALU opcodes.
OP_ALIASES = {
    "in": "load",
    "i": "load",
    "input": "load",
    "ld": "load",
    "lod": "load",
    "load": "load",
    "loadb": "load",
    "oload": "load",
    "ls": "load",
    "cof": "load",
    "getelementptr": "load",
    "out": "output",
    "store": "output",
    "st": "output",
    "str": "output",
    "storeb": "output",
    "ostore": "output",
    "output": "output",
    "add": "add",
    "addi": "add",
    "iadd": "add",
    "atadd": "add",
    "stadd": "add",
    "fadd": "add",
    "sub": "sub",
    "subi": "sub",
    "neg": "sub",
    "fsub": "sub",
    "mul": "mul",
    "muli": "mul",
    "mult": "mul",
    "imult": "mul",
    "fmul": "mul",
    "div": "div",
    "sdiv": "sdiv",
    "fdiv": "fdiv",
    "or": "or",
    "and": "or",
    "andreturn": "or",
    "xor": "or",
    "cat": "or",
    "shift": "shift",
    "shl": "shift",
    "lsl": "shift",
    "shlreturn": "shift",
    "shr": "shift",
    "copyshr": "shift",
    "lshr": "shift",
    "ashr": "shift",
    "sl": "shift",
    "sr": "shift",
    "sra": "shift",
    "asr": "shift",
    "lsr": "shift",
    "sext": "shift",
    "zext": "shift",
    "trunc": "shift",
    "bitcast": "shift",
    "fptrunc": "shift",
    "fpext": "shift",
    "sitofp": "shift",
    "uitofp": "shift",
    "fptosi": "shift",
    "fptoui": "shift",
    "ptrtoint": "shift",
    "inttoptr": "shift",
    "insertelement": "select",
    "extractelement": "select",
    "shufflevector": "select",
    "icmp": "icmp",
    "eq": "icmp",
    "cmp": "icmp",
    "lt": "icmp",
    "gt": "cmpgt",
    "cmpgt": "cmpgt",
    "icmpgt": "cmpgt",
    "ge": "cmpge",
    "bge": "cmpge",
    "cmpge": "cmpge",
    "icmpge": "cmpge",
    "le": "icmp",
    "ble": "icmp",
    "cmpeq": "cmpeq",
    "icmpeq": "cmpeq",
    "select": "select",
    "sel": "select",
    "mux": "select",
    "copymux": "select",
    "switch": "select",
    "copyswitch": "select",
    "merge": "select",
    "movc": "select",
    "cmerge": "select",
    "reg": "select",
    "copy": "select",
    "brighter": "select",
    "rgb": "select",
    "pas": "select",
    "sep": "select",
    "phi": "const",
    "const": "const",
    "constant": "const",
    "imp": "add",
    "exp": "add",
    "bne": "add",
    "beq": "add",
    "x": "add",
    "a": "add",
    "g": "add",
    "s": "add",
    "nop": "nop",
    "loop": "loop",
}

CONTAINER_REPO_ROOT = Path("/home/ubuntu/elastic_cgra_mapper")


@dataclass
class Node:
    source_id: str
    op: str
    const_value: Optional[str] = None
    order: int = 0
    attrs: dict = field(default_factory=dict)


@dataclass
class Edge:
    src: str
    dst: str
    operand: Optional[int] = None
    distance: Optional[int] = None


@dataclass
class GraphData:
    nodes: Dict[str, Node] = field(default_factory=dict)
    edges: List[Edge] = field(default_factory=list)
    skipped_control_nodes: int = 0
    bypassed_control_edges: int = 0
    dropped_edges: int = 0
    unsupported_ops: Set[str] = field(default_factory=set)


def clean_value(value) -> str:
    if isinstance(value, list):
        value = value[0] if value else ""
    return str(value).strip().strip('"')


def clean_name(value) -> str:
    return str(value).strip().strip('"')


def sanitize_id(value: str, fallback: str) -> str:
    value = clean_name(value)
    value = re.sub(r"[^0-9A-Za-z_]+", "_", value).strip("_")
    if not value:
        value = fallback
    if value[0].isdigit():
        value = f"n_{value}"
    return value


def normalize_op(raw_op: str) -> Optional[str]:
    key = clean_value(raw_op).lower()
    if re.fullmatch(r"\d+", key):
        return "add"
    if key in CONTROL_OPS:
        return None
    if key in OP_ALIASES:
        return OP_ALIASES[key]
    candidates = []
    if "_" in key:
        candidates.append(key.split("_", 1)[0])
    match = re.match(r"([a-z]+)", key)
    if match:
        candidates.append(match.group(1))
    for candidate in candidates:
        if candidate in CONTROL_OPS:
            return None
        if candidate in OP_ALIASES:
            return OP_ALIASES[candidate]
    return key


def infer_cgra_bench_op(node_name: str) -> str:
    match = re.match(r"Node\d+([A-Za-z_][A-Za-z0-9_]*)$", clean_name(node_name))
    return match.group(1) if match else ""


def infer_label_op(attrs: dict) -> str:
    label = clean_value(attrs.get("label", ""))
    if not label:
        return ""
    label = label.replace("\\l", " ").strip()

    numbered_match = re.match(r"\(?\d+\)?\s+([A-Za-z_][A-Za-z0-9_.]*)", label)
    if numbered_match:
        return numbered_match.group(1)

    if " = " in label:
        rhs = label.split(" = ", 1)[1].strip()
    else:
        rhs = label
    token_match = re.match(r"([A-Za-z_][A-Za-z0-9_.]*)", rhs)
    return token_match.group(1) if token_match else ""


def infer_dot_node_op(node_name: str, attrs: dict) -> str:
    opcode = clean_value(attrs.get("opcode", ""))
    if opcode:
        return opcode

    op = clean_value(attrs.get("op", ""))
    if op:
        return op

    node_type = clean_value(attrs.get("type", ""))
    if node_type == "input":
        return "input"
    if node_type == "output":
        return "output"
    if node_type == "const":
        return "const"
    if node_type == "op":
        return clean_value(attrs.get("opcode", ""))

    name_op = infer_cgra_bench_op(node_name)
    if name_op and not re.fullmatch(r"x[0-9A-Fa-f]+", name_op):
        return name_op

    label_op = infer_label_op(attrs)
    if label_op:
        return label_op

    if re.fullmatch(r"\d+", clean_name(node_name)):
        return "add"

    name_match = re.match(r"([A-Za-z]+)", clean_name(node_name))
    if name_match:
        return name_match.group(1)

    return name_op


def get_const_value(attrs: dict) -> Optional[str]:
    for key in ("const_value", "value", "CONST"):
        if key in attrs:
            value = clean_value(attrs[key])
            if value != "":
                return value
    return None


def get_int_attr(attrs: dict, keys: Tuple[str, ...]) -> Optional[int]:
    for key in keys:
        if key not in attrs:
            continue
        value = clean_value(attrs[key])
        if value == "":
            continue
        try:
            return int(value)
        except ValueError:
            continue
    return None


def edge_data_items(graph, src, dst):
    data = graph.get_edge_data(src, dst, default={})
    if graph.is_multigraph():
        return data.values()
    return [data]


def read_dot_graph(path: Path):
    try:
        return nx.nx_pydot.read_dot(str(path))
    except Exception:
        text = path.read_text()
        # Some public cpu_mapping DOTs put semicolons inside attribute lists
        # such as `[label = ADD_1;]`, which Graphviz tolerates less consistently
        # and pydot rejects. Removing only the in-list terminator preserves the
        # graph structure while letting pydot parse these benchmark files.
        text = re.sub(r";\s*\]", "]", text)
        graphs = pydot.graph_from_dot_data(text)
        if not graphs:
            raise
        return nx.nx_pydot.from_pydot(graphs[0])


def parse_dot(path: Path) -> GraphData:
    raw_graph = read_dot_graph(path)
    graph = GraphData()
    control_nodes = set()

    for order, (node_name, attrs) in enumerate(raw_graph.nodes(data=True)):
        source_id = clean_name(node_name)
        if source_id in {"", "\\n", "\n", "node", "edge", "graph"}:
            continue
        raw_op = infer_dot_node_op(source_id, attrs)
        op = normalize_op(raw_op)
        if op is None:
            control_nodes.add(source_id)
            graph.skipped_control_nodes += 1
            continue
        if not op:
            graph.unsupported_ops.add(f"<missing:{source_id}>")
            continue
        if op not in SUPPORTED_MAPPER_OPS and op != "nop":
            graph.unsupported_ops.add(raw_op)
            continue

        const_value = get_const_value(attrs)
        if op == "const" and const_value is None:
            const_value = "0"
        graph.nodes[source_id] = Node(
            source_id=source_id, op=op, const_value=const_value, order=order, attrs=attrs
        )

    incoming_to_control = defaultdict(list)
    outgoing_from_control = defaultdict(list)
    direct_edges = []
    for src, dst in raw_graph.edges():
        clean_src = clean_name(src)
        clean_dst = clean_name(dst)
        for attrs in edge_data_items(raw_graph, src, dst):
            edge = Edge(
                clean_src,
                clean_dst,
                operand=get_int_attr(attrs, ("operand",)),
                distance=get_int_attr(attrs, ("distance", "dist", "iteration_distance", "loop_distance")),
            )
            if clean_src in control_nodes:
                outgoing_from_control[clean_src].append(edge)
            elif clean_dst in control_nodes:
                incoming_to_control[clean_dst].append(edge)
            else:
                direct_edges.append(edge)

    for edge in direct_edges:
        add_edge_if_valid(graph, edge)

    for control_node in control_nodes:
        for in_edge in incoming_to_control[control_node]:
            if graph.nodes.get(in_edge.src, Node("", "")).op in {"icmp", "const"}:
                continue
            for out_edge in outgoing_from_control[control_node]:
                dst_node = graph.nodes.get(out_edge.dst)
                if not dst_node or dst_node.op == "const":
                    continue
                distance = out_edge.distance if out_edge.distance is not None else in_edge.distance
                add_edge_if_valid(graph, Edge(in_edge.src, out_edge.dst, distance=distance))
                graph.bypassed_control_edges += 1

    return cleanup_graph(graph)


def parse_revamp_xml(path: Path) -> GraphData:
    root = ET.parse(path).getroot()
    graph = GraphData()
    pending_edges = []

    for order, item in enumerate(root.findall("./DFG/Node")):
        source_id = item.attrib["idx"]
        raw_op = ""
        for child in item:
            if child.tag == "OP" and child.text:
                raw_op = child.text
                break
        op = normalize_op(raw_op)
        if op is None:
            graph.skipped_control_nodes += 1
            continue
        if not op or (op not in SUPPORTED_MAPPER_OPS and op != "nop"):
            graph.unsupported_ops.add(raw_op)
            continue

        const_value = item.attrib.get("CONST")
        if op == "const" and const_value is None:
            const_value = "0"
        graph.nodes[source_id] = Node(
            source_id=source_id, op=op, const_value=const_value, order=order, attrs=item.attrib
        )

        outputs = item.find("./Outputs")
        if outputs is not None:
            for output in outputs.findall("./Output"):
                distance = int(output.attrib.get("nextiter", "0"))
                pending_edges.append(Edge(source_id, output.attrib["idx"], distance=distance if distance else None))

    for edge in pending_edges:
        add_edge_if_valid(graph, edge)

    return cleanup_graph(graph)


def add_edge_if_valid(graph: GraphData, edge: Edge) -> None:
    src = graph.nodes.get(edge.src)
    dst = graph.nodes.get(edge.dst)
    if src is None or dst is None:
        graph.dropped_edges += 1
        return
    if dst.op == "const":
        graph.dropped_edges += 1
        return
    if edge.distance is None and src.order >= dst.order:
        edge.distance = 1
    graph.edges.append(edge)


def cleanup_graph(graph: GraphData) -> GraphData:
    seen_edges = set()
    deduped_edges = []
    for edge in graph.edges:
        key = (edge.src, edge.dst, edge.distance)
        if key in seen_edges:
            continue
        seen_edges.add(key)
        deduped_edges.append(edge)
    graph.edges = assign_operands(deduped_edges)
    return graph


def assign_operands(edges: List[Edge]) -> List[Edge]:
    next_operand = defaultdict(int)
    result = []
    for edge in edges:
        operand = edge.operand
        if operand is None:
            operand = next_operand[edge.dst]
        next_operand[edge.dst] = max(next_operand[edge.dst], operand + 1)
        result.append(Edge(edge.src, edge.dst, operand=operand, distance=edge.distance))
    return result


def write_dot(graph: GraphData, path: Path) -> dict:
    path.parent.mkdir(parents=True, exist_ok=True)
    source_to_output = {}
    used_names = set()
    op_counts = defaultdict(int)

    for idx, (source_id, node) in enumerate(sorted(graph.nodes.items())):
        base = sanitize_id(f"{node.op}_{source_id}", f"n{idx}")
        name = base
        suffix = 1
        while name in used_names:
            suffix += 1
            name = f"{base}_{suffix}"
        used_names.add(name)
        source_to_output[source_id] = name
        op_counts[node.op] += 1

    lines = ["digraph G {"]
    for source_id, node in sorted(graph.nodes.items(), key=lambda item: source_to_output[item[0]]):
        out_name = source_to_output[source_id]
        attrs = [f'opcode="{node.op}"']
        if node.const_value is not None:
            attrs.append(f'const_value=" {str(node.const_value).strip()}"')
        lines.append(f'  {out_name} [{", ".join(attrs)}];')

    for edge in graph.edges:
        if edge.src not in source_to_output or edge.dst not in source_to_output:
            continue
        attrs = [f"operand={edge.operand if edge.operand is not None else 0}"]
        if edge.distance is not None:
            attrs.append(f"distance={edge.distance}")
        lines.append(f'  {source_to_output[edge.src]} -> {source_to_output[edge.dst]} [{", ".join(attrs)}];')
    lines.append("}")
    path.write_text("\n".join(lines) + "\n")

    return {
        "nodes": len(graph.nodes),
        "edges": len(graph.edges),
        "op_counts": dict(sorted(op_counts.items())),
    }


def normalized_relative_path(source: Path, benchmark_root: Path) -> Path:
    rel = source.relative_to(benchmark_root)
    return rel.with_suffix(".dot")


def normalize_file(source: Path, benchmark_root: Path, out_dir: Path) -> dict:
    if source.suffix == ".xml":
        graph = parse_revamp_xml(source)
    elif source.suffix == ".dot":
        graph = parse_dot(source)
    else:
        raise ValueError(f"unsupported source file: {source}")

    rel = normalized_relative_path(source, benchmark_root)
    output = out_dir / rel
    stats = write_dot(graph, output) if not graph.unsupported_ops and graph.nodes else {}
    return {
        "source": str(source.relative_to(benchmark_root)),
        "output": str(output.relative_to(out_dir)) if output.exists() else "",
        "status": "ready" if output.exists() and not graph.unsupported_ops and graph.nodes else "not_ready",
        "unsupported_ops": ",".join(sorted(graph.unsupported_ops)),
        "skipped_control_nodes": graph.skipped_control_nodes,
        "bypassed_control_edges": graph.bypassed_control_edges,
        "dropped_edges": graph.dropped_edges,
        **stats,
    }


def write_csv(path: Path, rows: List[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "source",
        "output",
        "status",
        "nodes",
        "edges",
        "op_counts",
        "unsupported_ops",
        "skipped_control_nodes",
        "bypassed_control_edges",
        "dropped_edges",
    ]
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def manifest_set_name(parent: Path) -> str:
    if str(parent) == ".":
        return "root"
    return str(parent).replace("/", "__")


def manifest_path_for(local_path: Path, local_repo_root: Path, repo_root_for_manifest: Path) -> Path:
    try:
        return repo_root_for_manifest / local_path.relative_to(local_repo_root)
    except ValueError:
        return local_path


def write_manifest(
    path: Path,
    out_dir: Path,
    rows: List[dict],
    local_repo_root: Path,
    repo_root_for_manifest: Path,
) -> None:
    sets = defaultdict(list)
    for row in rows:
        if row["status"] != "ready":
            continue
        output = Path(row["output"])
        sets[output.parent].append(output.stem)

    manifest = {
        "name": "all_normalized_benchmarks",
        "problem_type": "modulo",
        "mode": "modulo_default_ii_sweep",
        "mapping_bin": str(repo_root_for_manifest / "build/mapping"),
        "benchmark_sets": [
            {
                "name": manifest_set_name(parent),
                "benchmark_root": str(
                    manifest_path_for(out_dir / parent, local_repo_root, repo_root_for_manifest)
                ),
                "benchmarks": sorted(benchmarks),
            }
            for parent, benchmarks in sorted(sets.items(), key=lambda item: str(item[0]))
        ],
        "architectures": [
            {
                "name": "mesh6x6_default_all",
                "template": str(repo_root_for_manifest / "research/configs/arch_templates/mesh_6x6_default.json"),
                "ii_max": 24,
                "mii": "auto",
            }
        ],
        "mappers": [
            {
                "name": "full_routing_ilp",
                "mapper_config": str(repo_root_for_manifest / "research/configs/mapper/modulo/full_routing_ilp_mapper.json"),
            }
        ],
        "timeout_sec": 3,
        "parallel_num": 1,
        "mii_missing_distance_policy": "self_loop",
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, indent=2) + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Normalize benchmark DFGs into the mapper's DOT input format.")
    parser.add_argument("--benchmark-root", type=Path, default=Path("benchmark"))
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--manifest-out", type=Path)
    parser.add_argument("--report-out", type=Path)
    parser.add_argument("--repo-root-for-manifest", type=Path, default=CONTAINER_REPO_ROOT)
    args = parser.parse_args()

    benchmark_root = args.benchmark_root.resolve()
    local_repo_root = benchmark_root.parent if benchmark_root.name == "benchmark" else Path.cwd().resolve()
    out_dir = args.out_dir.resolve()
    sources = sorted(list(benchmark_root.rglob("*.dot")) + list(benchmark_root.rglob("*.xml")))
    rows = [normalize_file(source, benchmark_root, out_dir) for source in sources]

    write_csv(out_dir / "normalization.csv", rows)
    if args.report_out:
        ready_count = sum(1 for row in rows if row["status"] == "ready")
        lines = [
            "# Benchmark Normalization",
            "",
            f"Sources: {len(rows)}",
            f"Ready DOTs: {ready_count}",
            "",
            "| source | output | status | nodes | edges | unsupported ops |",
            "| --- | --- | --- | ---: | ---: | --- |",
        ]
        for row in rows:
            lines.append(
                f"| `{row['source']}` | `{row['output']}` | {row['status']} | "
                f"{row.get('nodes', '')} | {row.get('edges', '')} | {row['unsupported_ops']} |"
            )
        args.report_out.parent.mkdir(parents=True, exist_ok=True)
        args.report_out.write_text("\n".join(lines) + "\n")

    if args.manifest_out:
        write_manifest(
            args.manifest_out,
            out_dir,
            rows,
            local_repo_root,
            args.repo_root_for_manifest,
        )

    not_ready = [row for row in rows if row["status"] != "ready"]
    if not_ready:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
