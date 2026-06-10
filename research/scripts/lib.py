#!/usr/bin/env python3

import csv
import json
import math
import re
from pathlib import Path
from typing import Dict, Iterable, Optional

import networkx as nx


MEMORY_OPS = {"load", "store", "output"}
EDGE_DISTANCE_KEYS = ("distance", "dist", "iteration_distance", "loop_distance")
NODE_LATENCY_KEYS = ("latency", "delay")
DEFAULT_OPERATION_LATENCY = 1
ROUTE_OP = "route"
NOP_OP = "nop"


def load_json(path: Path) -> dict:
    with path.open() as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def clean_dot_value(value) -> str:
    if isinstance(value, list):
        value = value[0] if value else ""
    return str(value).strip().strip('"')


def clean_node_name(value) -> str:
    return str(value).strip().strip('"')


def parse_int_attr(attrs: dict, keys: tuple, default: Optional[int] = None) -> Optional[int]:
    for key in keys:
        if key not in attrs:
            continue
        value = clean_dot_value(attrs[key])
        if value == "":
            continue
        return int(value)
    return default


def parse_config_id_list(value) -> list:
    if not value:
        return []
    if isinstance(value, str):
        return []
    if isinstance(value, dict):
        return [value]
    if isinstance(value, list):
        return value
    return []


def config_id_tuple(value: dict) -> tuple:
    return (
        int(value["row_id"]),
        int(value["column_id"]),
        int(value["context_id"]),
    )


def safe_ratio(numerator: float, denominator: float):
    return numerator / denominator if denominator else ""


def population_stddev(values: list):
    if not values:
        return ""
    mean = sum(values) / len(values)
    variance = sum((value - mean) ** 2 for value in values) / len(values)
    return math.sqrt(variance)


def read_dfg_stats(dfg_path: Path) -> dict:
    graph = nx.nx_pydot.read_dot(str(dfg_path))
    op_counts: Dict[str, int] = {}
    node_count = 0
    for node_name, attrs in graph.nodes(data=True):
        if str(node_name).strip() in {"", "\\n", "\n"}:
            continue
        opcode = clean_dot_value(attrs.get("opcode", ""))
        if not opcode:
            continue
        op_counts[opcode] = op_counts.get(opcode, 0) + 1
        node_count += 1

    edge_count = 0
    for src, dst in graph.edges():
        if str(src).strip() in {"", "\\n", "\n"}:
            continue
        if str(dst).strip() in {"", "\\n", "\n"}:
            continue
        edge_count += 1

    return {
        "node_count": node_count,
        "edge_count": edge_count,
        "op_counts": op_counts,
    }


def get_edge_data_items(graph, src, dst):
    data = graph.get_edge_data(src, dst, default={})
    if graph.is_multigraph():
        return data.values()
    return [data]


def resource_count_for_op(opcode: str, arch: dict) -> int:
    row = int(arch["row"])
    column = int(arch["column"])
    total_pe = row * column
    memory_io = arch["memory_io"]

    if opcode == "loop":
        return len(arch.get("loop_controllers", []))
    if opcode in MEMORY_OPS:
        if memory_io == "all":
            return total_pe
        if memory_io == "both_ends":
            return 2 * row if column > 1 else row
        if memory_io == "one_end":
            return row
        return 0
    return total_pe


def node_latency(node_attrs: dict, arch: dict) -> int:
    node_specific_latency = parse_int_attr(node_attrs, NODE_LATENCY_KEYS)
    if node_specific_latency is not None:
        return node_specific_latency

    opcode = clean_dot_value(node_attrs.get("opcode", ""))
    operation_latency = arch.get("operation_latency", {})
    if opcode in operation_latency:
        return int(operation_latency[opcode])
    if "default" in operation_latency:
        return int(operation_latency["default"])
    return DEFAULT_OPERATION_LATENCY


def edge_distance(src: str, dst: str, attrs: dict, missing_distance_policy: str) -> tuple:
    explicit_distance = parse_int_attr(attrs, EDGE_DISTANCE_KEYS)
    if explicit_distance is not None:
        return explicit_distance, "explicit"
    if missing_distance_policy == "self_loop" and src == dst:
        return 1, "inferred_self_loop"
    return 0, "missing"


def compute_rec_mii(
    dfg_path: Path,
    arch: dict,
    missing_distance_policy: str = "self_loop",
) -> dict:
    raw_graph = nx.nx_pydot.read_dot(str(dfg_path))
    graph = nx.DiGraph()
    node_latency_map = {}
    edge_distance_map = {}
    stats = {
        "explicit_distance_edges": 0,
        "inferred_self_loop_distance_edges": 0,
        "missing_distance_edges": 0,
        "self_loop_edges": 0,
        "zero_distance_cycles": 0,
    }

    for node_name, attrs in raw_graph.nodes(data=True):
        clean_name = clean_node_name(node_name)
        if clean_name in {"", "\\n", "\n"}:
            continue
        opcode = clean_dot_value(attrs.get("opcode", ""))
        if not opcode:
            continue
        graph.add_node(clean_name)
        node_latency_map[clean_name] = node_latency(attrs, arch)

    for src, dst in raw_graph.edges():
        clean_src = clean_node_name(src)
        clean_dst = clean_node_name(dst)
        if clean_src not in graph or clean_dst not in graph:
            continue
        if clean_src == clean_dst:
            stats["self_loop_edges"] += 1

        distances = []
        for attrs in get_edge_data_items(raw_graph, src, dst):
            distance, source = edge_distance(clean_src, clean_dst, attrs, missing_distance_policy)
            distances.append(distance)
            if source == "explicit":
                stats["explicit_distance_edges"] += 1
            elif source == "inferred_self_loop":
                stats["inferred_self_loop_distance_edges"] += 1
            else:
                stats["missing_distance_edges"] += 1

        if not distances:
            distances = [0]
        graph.add_edge(clean_src, clean_dst)
        # Multiple dependencies between the same two operations all constrain the schedule.
        # The smallest distance is the tightest recurrence bound for that pair.
        edge_distance_map[(clean_src, clean_dst)] = min(distances)

    rec_mii = 0
    cycles = []
    for cycle in nx.simple_cycles(graph):
        if not cycle:
            continue
        delay = 0
        distance = 0
        edges = []
        for idx, src in enumerate(cycle):
            dst = cycle[(idx + 1) % len(cycle)]
            delay += node_latency_map.get(src, DEFAULT_OPERATION_LATENCY)
            distance += edge_distance_map.get((src, dst), 0)
            edges.append(f"{src}->{dst}")

        if distance <= 0:
            stats["zero_distance_cycles"] += 1
            cycles.append(
                {
                    "nodes": cycle,
                    "edges": edges,
                    "delay": delay,
                    "distance": distance,
                    "RecMII": None,
                }
            )
            continue

        cycle_rec_mii = math.ceil(delay / distance)
        rec_mii = max(rec_mii, cycle_rec_mii)
        cycles.append(
            {
                "nodes": cycle,
                "edges": edges,
                "delay": delay,
                "distance": distance,
                "RecMII": cycle_rec_mii,
            }
        )

    cycles.sort(
        key=lambda cycle: (-1 if cycle["RecMII"] is None else cycle["RecMII"], cycle["delay"]),
        reverse=True,
    )
    rec_mii_value = None if stats["zero_distance_cycles"] > 0 else rec_mii
    return {
        "RecMII": rec_mii_value,
        "cycles": cycles,
        "cycle_count": len(cycles),
        "critical_cycles": cycles[:10],
        "missing_distance_policy": missing_distance_policy,
        "is_complete": rec_mii_value is not None,
        **stats,
    }


def compute_res_mii(
    dfg_path: Path,
    arch_template_path: Path,
    missing_distance_policy: str = "self_loop",
) -> dict:
    arch = load_json(arch_template_path)
    dfg_stats = read_dfg_stats(dfg_path)
    total_pes = int(arch["row"]) * int(arch["column"])
    total_dfg_ops = sum(dfg_stats["op_counts"].values())
    per_op = {}
    aggregate_bounds = {
        "all_operations": {
            "count": total_dfg_ops,
            "resources_per_cycle": total_pes,
            "ResMII": math.ceil(total_dfg_ops / total_pes) if total_pes else None,
        }
    }
    unsupported_ops = []

    res_mii = aggregate_bounds["all_operations"]["ResMII"] or 1
    for opcode, count in sorted(dfg_stats["op_counts"].items()):
        resources = resource_count_for_op(opcode, arch)
        if resources <= 0:
            unsupported_ops.append(opcode)
            op_mii = None
        else:
            op_mii = math.ceil(count / resources)
            res_mii = max(res_mii, op_mii)
        per_op[opcode] = {
            "count": count,
            "resources_per_cycle": resources,
            "ResMII": op_mii,
        }

    rec_mii_data = compute_rec_mii(dfg_path, arch, missing_distance_policy)
    rec_mii = rec_mii_data["RecMII"]
    mii = None if unsupported_ops or rec_mii is None else max(res_mii, rec_mii)

    return {
        "ResMII": None if unsupported_ops else res_mii,
        "RecMII": rec_mii,
        "MII": mii,
        "unsupported_ops": unsupported_ops,
        "aggregate_bounds": aggregate_bounds,
        "per_op": per_op,
        "recurrence": rec_mii_data,
        "dfg_nodes": dfg_stats["node_count"],
        "dfg_edges": dfg_stats["edge_count"],
        "notes": "RecMII uses explicit edge distance attributes when present and infers distance=1 only for missing-distance self-loops.",
    }


def make_arch_for_ii(arch_template_path: Path, ii: int, force_default: bool = True) -> dict:
    arch = load_json(arch_template_path)
    arch["context_size"] = int(ii)
    if force_default:
        arch["CGRA_type"] = "default"
    return arch


def find_single_run_dir(raw_output_dir: Path) -> Optional[Path]:
    mapping_root = raw_output_dir / "mapping"
    if not mapping_root.exists():
        return None
    candidates = [p for p in mapping_root.iterdir() if p.is_dir()]
    candidates.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return candidates[0] if candidates else None


def find_one(path: Path, pattern: str) -> Optional[Path]:
    matches = sorted(path.glob(pattern))
    return matches[0] if matches else None


def parse_gurobi_log(log_path: Optional[Path]) -> dict:
    result = {
        "gurobi_status": "",
        "objective_value": "",
        "best_bound": "",
        "mip_gap": "",
    }
    if not log_path or not log_path.exists():
        return result

    text = log_path.read_text(errors="replace")
    if "Optimal solution found" in text:
        result["gurobi_status"] = "optimal"
    elif "Time limit reached" in text:
        result["gurobi_status"] = "time_limit"
    elif "Model is infeasible" in text:
        result["gurobi_status"] = "infeasible"

    best = re.search(
        r"Best objective\s+([0-9.eE+-]+),\s+best bound\s+([0-9.eE+-]+),\s+gap\s+([0-9.]+%)",
        text,
    )
    if best:
        result["objective_value"] = best.group(1)
        result["best_bound"] = best.group(2)
        result["mip_gap"] = best.group(3)
    return result


def mapping_utilization(mapping_path: Optional[Path]) -> dict:
    empty = {
        "rows": "",
        "cols": "",
        "context_size": "",
        "memory_io": "",
        "cgra_type": "",
        "network_type": "",
        "used_ops": "",
        "compute_ops": "",
        "memory_ops": "",
        "const_ops": "",
        "route_ops": "",
        "nop_ops": "",
        "total_pe_contexts": "",
        "utilization": "",
        "pe_context_utilization": "",
        "compute_context_utilization": "",
        "route_context_utilization": "",
        "memory_context_utilization": "",
        "active_pe_count": "",
        "active_compute_pe_count": "",
        "active_route_pe_count": "",
        "active_memory_pe_count": "",
        "pe_active_ratio": "",
        "compute_pe_utilization": "",
        "route_pe_ratio": "",
        "memory_pe_ratio": "",
        "used_context_count": "",
        "context_active_ratio": "",
        "max_context_load": "",
        "min_context_load": "",
        "avg_context_load": "",
        "context_load_stddev": "",
        "max_compute_contexts_per_pe": "",
        "avg_compute_contexts_per_active_compute_pe": "",
        "routing_overhead_ratio": "",
        "route_to_compute_ratio": "",
        "connection_count": "",
        "inter_pe_connection_count": "",
        "same_pe_connection_count": "",
        "avg_manhattan_distance": "",
        "max_manhattan_distance": "",
        "avg_context_distance": "",
        "max_context_distance": "",
        "max_fanout": "",
        "avg_fanout": "",
        "compute_bbox_area": "",
        "compute_bbox_utilization": "",
    }
    if not mapping_path or not mapping_path.exists():
        return empty

    mapping = load_json(mapping_path)
    rows = int(mapping["row"])
    cols = int(mapping["column"])
    context_size = int(mapping["context_size"])
    total = rows * cols * context_size
    used_ops = 0
    compute_ops = 0
    memory_ops = 0
    const_ops = 0
    route_ops = 0
    nop_ops = 0
    active_pes = set()
    active_compute_pes = set()
    active_route_pes = set()
    active_memory_pes = set()
    context_loads = [0 for _ in range(context_size)]
    compute_contexts_per_pe = {}
    fanouts = []
    connection_count = 0
    inter_pe_connection_count = 0
    same_pe_connection_count = 0
    manhattan_distances = []
    context_distances = []
    compute_positions = set()

    for pe in mapping.get("PE_config", []):
        row = int(pe["row_id"])
        col = int(pe["column_id"])
        pe_id = (row, col)
        compute_contexts_per_pe.setdefault(pe_id, 0)
        for config in pe.get("config", []):
            op = clean_dot_value(config.get("operation_type", ""))
            context_id = int(config["context_id"])
            if op == NOP_OP:
                nop_ops += 1
            else:
                used_ops += 1
                active_pes.add(pe_id)
                context_loads[context_id] += 1
            if op == ROUTE_OP:
                route_ops += 1
                active_route_pes.add(pe_id)
            elif op != NOP_OP:
                compute_ops += 1
                compute_contexts_per_pe[pe_id] += 1
                active_compute_pes.add(pe_id)
                compute_positions.add(pe_id)
                if op in MEMORY_OPS:
                    memory_ops += 1
                    active_memory_pes.add(pe_id)
                if op == "const":
                    const_ops += 1

            to_config_ids = parse_config_id_list(config.get("to_config_id", []))
            if op != NOP_OP:
                fanouts.append(len(to_config_ids))
            for to_config_id in to_config_ids:
                to_row, to_col, to_context = config_id_tuple(to_config_id)
                connection_count += 1
                manhattan_distance = abs(row - to_row) + abs(col - to_col)
                context_distance = (to_context - context_id) % context_size
                manhattan_distances.append(manhattan_distance)
                context_distances.append(context_distance)
                if manhattan_distance == 0:
                    same_pe_connection_count += 1
                else:
                    inter_pe_connection_count += 1

    active_context_loads = [load for load in context_loads if load > 0]
    active_compute_pe_count = len(active_compute_pes)
    compute_context_values = [
        count for pe_id, count in compute_contexts_per_pe.items() if pe_id in active_compute_pes
    ]
    if compute_positions:
        min_row = min(row for row, _ in compute_positions)
        max_row = max(row for row, _ in compute_positions)
        min_col = min(col for _, col in compute_positions)
        max_col = max(col for _, col in compute_positions)
        compute_bbox_area = (max_row - min_row + 1) * (max_col - min_col + 1)
    else:
        compute_bbox_area = 0

    empty.update(
        {
            "rows": rows,
            "cols": cols,
            "context_size": context_size,
            "memory_io": mapping.get("memory_io_type", ""),
            "cgra_type": mapping.get("cgra_type", ""),
            "network_type": mapping.get("network_type", ""),
            "used_ops": used_ops,
            "compute_ops": compute_ops,
            "memory_ops": memory_ops,
            "const_ops": const_ops,
            "route_ops": route_ops,
            "nop_ops": nop_ops,
            "total_pe_contexts": total,
            "utilization": used_ops / total if total else "",
            "pe_context_utilization": safe_ratio(used_ops, total),
            "compute_context_utilization": safe_ratio(compute_ops, total),
            "route_context_utilization": safe_ratio(route_ops, total),
            "memory_context_utilization": safe_ratio(memory_ops, total),
            "active_pe_count": len(active_pes),
            "active_compute_pe_count": active_compute_pe_count,
            "active_route_pe_count": len(active_route_pes),
            "active_memory_pe_count": len(active_memory_pes),
            "pe_active_ratio": safe_ratio(len(active_pes), rows * cols),
            "compute_pe_utilization": safe_ratio(active_compute_pe_count, rows * cols),
            "route_pe_ratio": safe_ratio(len(active_route_pes), rows * cols),
            "memory_pe_ratio": safe_ratio(len(active_memory_pes), rows * cols),
            "used_context_count": len(active_context_loads),
            "context_active_ratio": safe_ratio(len(active_context_loads), context_size),
            "max_context_load": max(context_loads) if context_loads else "",
            "min_context_load": min(context_loads) if context_loads else "",
            "avg_context_load": sum(context_loads) / len(context_loads)
            if context_loads
            else "",
            "context_load_stddev": population_stddev(context_loads),
            "max_compute_contexts_per_pe": max(compute_context_values)
            if compute_context_values
            else 0,
            "avg_compute_contexts_per_active_compute_pe": safe_ratio(
                sum(compute_context_values), len(compute_context_values)
            ),
            "routing_overhead_ratio": safe_ratio(route_ops, used_ops),
            "route_to_compute_ratio": safe_ratio(route_ops, compute_ops),
            "connection_count": connection_count,
            "inter_pe_connection_count": inter_pe_connection_count,
            "same_pe_connection_count": same_pe_connection_count,
            "avg_manhattan_distance": safe_ratio(
                sum(manhattan_distances), len(manhattan_distances)
            ),
            "max_manhattan_distance": max(manhattan_distances)
            if manhattan_distances
            else 0,
            "avg_context_distance": safe_ratio(
                sum(context_distances), len(context_distances)
            ),
            "max_context_distance": max(context_distances) if context_distances else 0,
            "max_fanout": max(fanouts) if fanouts else 0,
            "avg_fanout": safe_ratio(sum(fanouts), len(fanouts)),
            "compute_bbox_area": compute_bbox_area,
            "compute_bbox_utilization": safe_ratio(
                active_compute_pe_count, compute_bbox_area
            ),
        }
    )
    return empty


def normalize_run(
    run_dir: Path,
    benchmark: str,
    mapper: str,
    arch_name: str,
    mii: Optional[int],
    achieved_ii: Optional[int],
    start_ii: Optional[int] = None,
    dfg_path: Optional[Path] = None,
) -> dict:
    input_log_path = find_one(run_dir, "input_log_*.json")
    output_log_path = find_one(run_dir, "output_log_*.json")
    mapping_path = find_one(run_dir, "mapping_*.json")
    gurobi_log_path = find_one(run_dir, "gurobi_log_*.log")

    input_log = load_json(input_log_path) if input_log_path else {}
    output_log = load_json(output_log_path) if output_log_path else {}
    cgra_metrics = mapping_utilization(mapping_path)
    gurobi_metrics = parse_gurobi_log(gurobi_log_path)

    if dfg_path is None and input_log.get("dfg_file"):
        dfg_path = Path(input_log["dfg_file"])
    dfg_stats = read_dfg_stats(dfg_path) if dfg_path and dfg_path.exists() else {
        "node_count": "",
        "edge_count": "",
    }

    is_success = bool(output_log.get("is_success", False))
    status = "success" if is_success else "failed"
    if gurobi_metrics["gurobi_status"] == "time_limit":
        status = "timeout_feasible" if is_success else "timeout"
    elif gurobi_metrics["gurobi_status"] == "infeasible":
        status = "infeasible"
    elif gurobi_metrics["gurobi_status"] == "optimal" and is_success:
        status = "optimal"

    ii_ratio = ""
    if mii and achieved_ii:
        ii_ratio = mii / achieved_ii

    row = {
        "benchmark": benchmark,
        "mapper": mapper,
        "arch_name": arch_name,
        "cgra_type": cgra_metrics["cgra_type"],
        "network_type": cgra_metrics["network_type"],
        "memory_io": cgra_metrics["memory_io"],
        "rows": cgra_metrics["rows"],
        "cols": cgra_metrics["cols"],
        "context_size": cgra_metrics["context_size"],
        "MII": mii if mii is not None else "",
        "start_II": start_ii if start_ii is not None else "",
        "achieved_II": achieved_ii if achieved_ii is not None else "",
        "II_ratio": ii_ratio,
        "status": status,
        "mapping_time_sec": output_log.get("mapping_time_s", ""),
        "timeout_sec": input_log.get("timeout_s", ""),
        "parallel_num": input_log.get("parallel_num", ""),
        "dfg_nodes": dfg_stats["node_count"],
        "dfg_edges": dfg_stats["edge_count"],
        "used_ops": cgra_metrics["used_ops"],
        "compute_ops": cgra_metrics["compute_ops"],
        "memory_ops": cgra_metrics["memory_ops"],
        "const_ops": cgra_metrics["const_ops"],
        "route_ops": cgra_metrics["route_ops"],
        "nop_ops": cgra_metrics["nop_ops"],
        "total_pe_contexts": cgra_metrics["total_pe_contexts"],
        "utilization": cgra_metrics["utilization"],
        "pe_context_utilization": cgra_metrics["pe_context_utilization"],
        "compute_context_utilization": cgra_metrics["compute_context_utilization"],
        "route_context_utilization": cgra_metrics["route_context_utilization"],
        "memory_context_utilization": cgra_metrics["memory_context_utilization"],
        "active_pe_count": cgra_metrics["active_pe_count"],
        "active_compute_pe_count": cgra_metrics["active_compute_pe_count"],
        "active_route_pe_count": cgra_metrics["active_route_pe_count"],
        "active_memory_pe_count": cgra_metrics["active_memory_pe_count"],
        "pe_active_ratio": cgra_metrics["pe_active_ratio"],
        "compute_pe_utilization": cgra_metrics["compute_pe_utilization"],
        "route_pe_ratio": cgra_metrics["route_pe_ratio"],
        "memory_pe_ratio": cgra_metrics["memory_pe_ratio"],
        "used_context_count": cgra_metrics["used_context_count"],
        "context_active_ratio": cgra_metrics["context_active_ratio"],
        "max_context_load": cgra_metrics["max_context_load"],
        "min_context_load": cgra_metrics["min_context_load"],
        "avg_context_load": cgra_metrics["avg_context_load"],
        "context_load_stddev": cgra_metrics["context_load_stddev"],
        "max_compute_contexts_per_pe": cgra_metrics["max_compute_contexts_per_pe"],
        "avg_compute_contexts_per_active_compute_pe": cgra_metrics[
            "avg_compute_contexts_per_active_compute_pe"
        ],
        "routing_overhead_ratio": cgra_metrics["routing_overhead_ratio"],
        "route_to_compute_ratio": cgra_metrics["route_to_compute_ratio"],
        "connection_count": cgra_metrics["connection_count"],
        "inter_pe_connection_count": cgra_metrics["inter_pe_connection_count"],
        "same_pe_connection_count": cgra_metrics["same_pe_connection_count"],
        "avg_manhattan_distance": cgra_metrics["avg_manhattan_distance"],
        "max_manhattan_distance": cgra_metrics["max_manhattan_distance"],
        "avg_context_distance": cgra_metrics["avg_context_distance"],
        "max_context_distance": cgra_metrics["max_context_distance"],
        "max_fanout": cgra_metrics["max_fanout"],
        "avg_fanout": cgra_metrics["avg_fanout"],
        "compute_bbox_area": cgra_metrics["compute_bbox_area"],
        "compute_bbox_utilization": cgra_metrics["compute_bbox_utilization"],
        "gurobi_status": gurobi_metrics["gurobi_status"],
        "objective_value": gurobi_metrics["objective_value"],
        "best_bound": gurobi_metrics["best_bound"],
        "mip_gap": gurobi_metrics["mip_gap"],
        "mapping_file": str(mapping_path) if mapping_path else "",
        "gurobi_log_file": str(gurobi_log_path) if gurobi_log_path else "",
        "run_dir": str(run_dir),
    }
    return row


CSV_FIELDS = [
    "benchmark",
    "benchmark_set",
    "mapper",
    "arch_name",
    "cgra_type",
    "network_type",
    "memory_io",
    "rows",
    "cols",
    "context_size",
    "MII",
    "start_II",
    "achieved_II",
    "II_ratio",
    "status",
    "mapping_time_sec",
    "wall_time_sec",
    "process_returncode",
    "timeout_sec",
    "parallel_num",
    "dfg_nodes",
    "dfg_edges",
    "used_ops",
    "compute_ops",
    "memory_ops",
    "const_ops",
    "route_ops",
    "nop_ops",
    "total_pe_contexts",
    "utilization",
    "pe_context_utilization",
    "compute_context_utilization",
    "route_context_utilization",
    "memory_context_utilization",
    "active_pe_count",
    "active_compute_pe_count",
    "active_route_pe_count",
    "active_memory_pe_count",
    "pe_active_ratio",
    "compute_pe_utilization",
    "route_pe_ratio",
    "memory_pe_ratio",
    "used_context_count",
    "context_active_ratio",
    "max_context_load",
    "min_context_load",
    "avg_context_load",
    "context_load_stddev",
    "max_compute_contexts_per_pe",
    "avg_compute_contexts_per_active_compute_pe",
    "routing_overhead_ratio",
    "route_to_compute_ratio",
    "connection_count",
    "inter_pe_connection_count",
    "same_pe_connection_count",
    "avg_manhattan_distance",
    "max_manhattan_distance",
    "avg_context_distance",
    "max_context_distance",
    "max_fanout",
    "avg_fanout",
    "compute_bbox_area",
    "compute_bbox_utilization",
    "gurobi_status",
    "objective_value",
    "best_bound",
    "mip_gap",
    "mapping_file",
    "gurobi_log_file",
    "run_dir",
]


def write_metrics_csv(path: Path, rows: Iterable[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in CSV_FIELDS})
