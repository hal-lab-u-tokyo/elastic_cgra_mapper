#!/usr/bin/env python3

import csv
import json
import math
import re
from collections import defaultdict, deque
from functools import lru_cache
from pathlib import Path
from typing import Dict, Iterable, Optional

import networkx as nx


MEMORY_OPS = {"load", "store", "output"}
EDGE_DISTANCE_KEYS = ("distance", "dist", "iteration_distance", "loop_distance")
NODE_LATENCY_KEYS = ("latency", "delay")
DEFAULT_OPERATION_LATENCY = 1
ROUTE_OP = "route"
NOP_OP = "nop"
CONTAINER_REPO_ROOT = Path("/home/ubuntu/elastic_cgra_mapper")


def load_json(path: Path) -> dict:
    with path.open() as f:
        return json.load(f)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def resolve_repo_path(path, repo_root: Path) -> Path:
    raw_path = Path(path)
    if raw_path.is_absolute():
        try:
            return repo_root / raw_path.relative_to(CONTAINER_REPO_ROOT)
        except ValueError:
            return raw_path
    return repo_root / raw_path


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


def percentile(values: list, percent: float):
    if not values:
        return ""
    sorted_values = sorted(values)
    index = math.ceil((percent / 100.0) * len(sorted_values)) - 1
    index = min(max(index, 0), len(sorted_values) - 1)
    return sorted_values[index]


def dfg_edge_criticality(graph, clean_nodes: set):
    dag = nx.DiGraph()
    dag.add_nodes_from(clean_nodes)
    for src, dst in graph.edges():
        clean_src = clean_node_name(src)
        clean_dst = clean_node_name(dst)
        if clean_src in clean_nodes and clean_dst in clean_nodes:
            dag.add_edge(clean_src, clean_dst)

    if not nx.is_directed_acyclic_graph(dag):
        return None

    topo_order = list(nx.topological_sort(dag))
    forward = {node: 0 for node in topo_order}
    for node in topo_order:
        for succ in dag.successors(node):
            forward[succ] = max(forward[succ], forward[node] + 1)

    backward = {node: 0 for node in topo_order}
    for node in reversed(topo_order):
        for succ in dag.successors(node):
            backward[node] = max(backward[node], backward[succ] + 1)

    critical_length = max(forward.values()) if forward else 0
    denominator = max(1, critical_length)
    criticality = {}
    for src, dst in dag.edges():
        through_edge = forward[src] + 1 + backward[dst]
        criticality[(src, dst)] = through_edge / denominator
    return criticality


def update_cut_congestion(
    row_a: int,
    col_a: int,
    row_b: int,
    col_b: int,
    horizontal_cuts: dict,
    vertical_cuts: dict,
) -> None:
    min_col, max_col = sorted((col_a, col_b))
    for cut_col in range(min_col, max_col):
        vertical_cuts[cut_col] += 1
    min_row, max_row = sorted((row_a, row_b))
    for cut_row in range(min_row, max_row):
        horizontal_cuts[cut_row] += 1


def update_xy_link_demand(
    row_a: int,
    col_a: int,
    row_b: int,
    col_b: int,
    link_demand: dict,
) -> None:
    row = row_a
    col = col_a
    col_step = 1 if col_b >= col else -1
    while col != col_b:
        next_col = col + col_step
        link = tuple(sorted(((row, col), (row, next_col))))
        link_demand[link] += 1
        col = next_col

    row_step = 1 if row_b >= row else -1
    while row != row_b:
        next_row = row + row_step
        link = tuple(sorted(((row, col), (next_row, col))))
        link_demand[link] += 1
        row = next_row


@lru_cache(maxsize=None)
def _read_dot_graph_cached(dfg_path: str, mtime_ns: int, file_size: int):
    del mtime_ns, file_size
    return nx.nx_pydot.read_dot(dfg_path)


def read_dot_graph(dfg_path: Path):
    resolved_path = dfg_path.resolve()
    file_stat = resolved_path.stat()
    return _read_dot_graph_cached(
        str(resolved_path), file_stat.st_mtime_ns, file_stat.st_size
    )


@lru_cache(maxsize=None)
def _read_dfg_stats_cached(
    dfg_path: str, mtime_ns: int, file_size: int
) -> tuple:
    graph = _read_dot_graph_cached(dfg_path, mtime_ns, file_size)
    op_counts: Dict[str, int] = {}
    nodes = []
    node_count = 0
    for node_name, attrs in graph.nodes(data=True):
        clean_name = clean_node_name(node_name)
        if clean_name in {"", "\\n", "\n"}:
            continue
        opcode = clean_dot_value(attrs.get("opcode", ""))
        if not opcode:
            continue
        nodes.append(clean_name)
        op_counts[opcode] = op_counts.get(opcode, 0) + 1
        node_count += 1

    edge_count = 0
    predecessors = {node: 0 for node in nodes}
    successors = {node: 0 for node in nodes}
    for src, dst in graph.edges():
        clean_src = clean_node_name(src)
        clean_dst = clean_node_name(dst)
        if clean_src in {"", "\\n", "\n"}:
            continue
        if clean_dst in {"", "\\n", "\n"}:
            continue
        if clean_src not in successors or clean_dst not in predecessors:
            continue
        successors[clean_src] += 1
        predecessors[clean_dst] += 1
        edge_count += 1

    input_count = sum(1 for node in nodes if predecessors[node] == 0)
    output_count = sum(1 for node in nodes if successors[node] == 0)
    return (
        node_count,
        edge_count,
        input_count,
        output_count,
        tuple(sorted(op_counts.items())),
    )


def read_dfg_stats(dfg_path: Path) -> dict:
    resolved_path = dfg_path.resolve()
    file_stat = resolved_path.stat()
    node_count, edge_count, input_count, output_count, op_counts = (
        _read_dfg_stats_cached(
            str(resolved_path), file_stat.st_mtime_ns, file_stat.st_size
        )
    )
    return {
        "node_count": node_count,
        "edge_count": edge_count,
        "input_count": input_count,
        "output_count": output_count,
        "op_counts": dict(op_counts),
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
        if memory_io in {"perimeter", "border"}:
            return total_pe - max(0, row - 2) * max(0, column - 2)
        if memory_io in {"perimeter_no_corners", "border_no_corners"}:
            return max(0, 2 * row + 2 * column - 8)
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
    raw_graph = read_dot_graph(dfg_path)
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


def _compute_res_mii_for_arch(
    dfg_path: Path,
    arch: dict,
    missing_distance_policy: str,
) -> dict:
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


@lru_cache(maxsize=None)
def _compute_res_mii_cached(
    dfg_path: str,
    mtime_ns: int,
    file_size: int,
    arch_json: str,
    missing_distance_policy: str,
) -> str:
    del mtime_ns, file_size
    result = _compute_res_mii_for_arch(
        Path(dfg_path), json.loads(arch_json), missing_distance_policy
    )
    return json.dumps(result, sort_keys=True)


def compute_res_mii(
    dfg_path: Path,
    arch_template_path: Path,
    missing_distance_policy: str = "self_loop",
) -> dict:
    resolved_path = dfg_path.resolve()
    file_stat = resolved_path.stat()
    arch_json = json.dumps(
        load_json(arch_template_path), sort_keys=True, separators=(",", ":")
    )
    result_json = _compute_res_mii_cached(
        str(resolved_path),
        file_stat.st_mtime_ns,
        file_stat.st_size,
        arch_json,
        missing_distance_policy,
    )
    return json.loads(result_json)


def placement2d_capacity_check(dfg_path: Path, arch_template_path: Path) -> dict:
    arch = load_json(arch_template_path)
    return placement2d_capacity_check_for_arch(dfg_path, arch)


def placement2d_capacity_check_for_arch(dfg_path: Path, arch: dict) -> dict:
    stats = read_dfg_stats(dfg_path)
    total_pes = int(arch["row"]) * int(arch["column"])
    dfg_nodes = int(stats["node_count"])
    op_counts = stats.get("op_counts", {})
    memory_nodes = sum(int(op_counts.get(opcode, 0)) for opcode in MEMORY_OPS)
    memory_pes = resource_count_for_op("load", arch)
    ok = dfg_nodes <= total_pes and memory_nodes <= memory_pes
    return {
        "dfg_nodes": dfg_nodes,
        "physical_pes": total_pes,
        "memory_nodes": memory_nodes,
        "memory_pes": memory_pes,
        "ok": ok,
        "detail": (
            f"dfg_nodes={dfg_nodes}, physical_pes={total_pes}, "
            f"memory_nodes={memory_nodes}, memory_pes={memory_pes}"
        ),
    }


def auto_square_arch_for_dfg(
    arch_template_path: Path,
    dfg_path: Path,
    policy: str,
    arch_overrides: Optional[dict] = None,
) -> dict:
    arch = load_json(arch_template_path)
    auto_grid = {}
    if arch_overrides:
        if isinstance(arch_overrides.get("auto_grid"), dict):
            auto_grid = arch_overrides["auto_grid"]
        for key in ("memory_io", "network_type", "CGRA_type", "local_reg_size", "placement_cost_model"):
            if key in arch_overrides:
                arch[key] = arch_overrides[key]
    stats = read_dfg_stats(dfg_path)
    node_count = int(stats["node_count"])
    if policy in {"ceil_sqrt_nodes", "traversal_fully_pipelined"}:
        grid_size = math.ceil(math.sqrt(node_count))
    elif policy in {"ceil_sqrt_nodes_plus_2", "traversal_fully_pipelined_with_io_ring"}:
        grid_size = math.ceil(math.sqrt(node_count)) + 2
    elif policy in {
        "ceil_sqrt_nodes_plus_2_fit_io",
        "traversal_fully_pipelined_with_io_ring_fit_io",
    }:
        op_counts = stats.get("op_counts", {})
        memory_nodes = sum(int(op_counts.get(opcode, 0)) for opcode in MEMORY_OPS)
        grid_size = math.ceil(math.sqrt(node_count)) + 2
        while True:
            candidate = dict(arch)
            candidate["row"] = int(grid_size)
            candidate["column"] = int(grid_size)
            if resource_count_for_op("load", candidate) >= memory_nodes:
                break
            grid_size += 1
    elif policy in {"cpu_mapping_yoto_yott", "ceil_sqrt_non_io_plus_2"}:
        io_count = int(stats.get("input_count", 0)) + int(stats.get("output_count", 0))
        grid_size = math.ceil(math.sqrt(max(0, node_count - io_count))) + 2
    elif policy in {
        "cpu_mapping_yoto_yott_fit_structural_io",
        "ceil_sqrt_non_io_plus_2_fit_structural_io",
    }:
        io_count = int(stats.get("input_count", 0)) + int(stats.get("output_count", 0))
        grid_size = math.ceil(math.sqrt(max(0, node_count - io_count))) + 2
        while True:
            candidate = dict(arch)
            candidate["row"] = int(grid_size)
            candidate["column"] = int(grid_size)
            if resource_count_for_op("load", candidate) >= io_count:
                break
            grid_size += 1
    else:
        raise ValueError(f"Unknown auto_grid policy: {policy}")
    grid_size += int(auto_grid.get("margin", 0))
    arch["row"] = int(grid_size)
    arch["column"] = int(grid_size)
    return arch


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


def parse_placement_log(log_path: Optional[Path]) -> dict:
    result = {
        "placement_swap_attempts": "",
        "placement_cell_visits": "",
        "vpr_initial_placement_cost": "",
        "vpr_initial_bb_cost": "",
        "vpr_final_placement_cost": "",
        "vpr_final_bb_cost": "",
    }
    if not log_path or not log_path.exists():
        return result

    text = log_path.read_text(errors="replace")
    swap_attempts = re.search(r"placement_swap_attempts=([0-9]+)", text)
    if not swap_attempts:
        swap_attempts = re.search(
            r"Placement total # of swap attempts:\s*([0-9]+)", text
        )
    if swap_attempts:
        result["placement_swap_attempts"] = swap_attempts.group(1)

    cell_visits = re.search(r"cell_visits=([0-9]+)", text)
    if cell_visits:
        result["placement_cell_visits"] = cell_visits.group(1)

    initial = re.search(
        r"Initial placement cost:\s*([0-9.eE+-]+),?\s+bb_cost:\s*([0-9.eE+-]+)",
        text,
    )
    if initial:
        result["vpr_initial_placement_cost"] = initial.group(1)
        result["vpr_initial_bb_cost"] = initial.group(2)
    placements = re.findall(
        r"Placement cost:\s*([0-9.eE+-]+),\s+bb_cost:\s*([0-9.eE+-]+)",
        text,
    )
    if placements:
        result["vpr_final_placement_cost"] = placements[-1][0]
        result["vpr_final_bb_cost"] = placements[-1][1]
    return result


def placement_cost(dx: int, dy: int, model: str) -> int:
    if dx == 0 and dy == 0:
        return 1
    if model in {"one_hop_axis2", "cpu_mapping_1hop"}:
        return max(1, math.ceil(dx / 2) + math.ceil(dy / 2))
    return max(1, dx + dy)


def longest_path_cost(nodes: Iterable[str], weighted_edges: Iterable[tuple]) -> object:
    node_list = list(nodes)
    node_set = set(node_list)
    successors = {node: [] for node in node_list}
    indegree = {node: 0 for node in node_list}
    for src, dst, weight in weighted_edges:
        if src not in node_set or dst not in node_set:
            continue
        successors[src].append((dst, max(1, int(weight))))
        indegree[dst] += 1

    queue = deque(node for node in node_list if indegree[node] == 0)
    distance = {node: 0 for node in node_list}
    visited = 0
    while queue:
        src = queue.popleft()
        visited += 1
        for dst, weight in successors[src]:
            distance[dst] = max(distance[dst], distance[src] + weight)
            indegree[dst] -= 1
            if indegree[dst] == 0:
                queue.append(dst)

    if visited != len(node_list):
        return ""
    return max(distance.values()) if distance else ""


def find_routed_path(src_ids: list, dst_ids: list, configs: dict, outgoing: dict) -> list:
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


def path_spatial_hop(path: list) -> int:
    return sum(
        abs(src[0] - dst[0]) + abs(src[1] - dst[1])
        for src, dst in zip(path, path[1:])
    )


def mapping_utilization(
    mapping_path: Optional[Path],
    dfg_path: Optional[Path] = None,
    arch_path: Optional[Path] = None,
) -> dict:
    placement_cost_model = "mesh"
    if arch_path and arch_path.exists():
        arch = load_json(arch_path)
        placement_cost_model = str(arch.get("placement_cost_model", placement_cost_model))

    empty = {
        "rows": "",
        "cols": "",
        "context_size": "",
        "memory_io": "",
        "cgra_type": "",
        "network_type": "",
        "placement_cost_model": placement_cost_model,
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
        "placement_p90_fifo": "",
        "placement_p95_fifo": "",
        "placement_max_mesh_fifo": "",
        "placement_mapped_lp_mesh_hop": "",
        "placement_criticality_weighted_mesh_hop": "",
        "placement_criticality_weighted_fifo": "",
        "placement_max_critical_edge_mesh_hop": "",
        "placement_max_critical_edge_fifo": "",
        "placement_max_cut_congestion": "",
        "placement_avg_cut_congestion": "",
        "placement_p95_cut_congestion": "",
        "placement_max_horizontal_cut_congestion": "",
        "placement_max_vertical_cut_congestion": "",
        "placement_estimated_total_link_demand": "",
        "placement_estimated_max_link_demand": "",
        "placement_estimated_avg_link_demand": "",
        "placement_estimated_p95_link_demand": "",
        "placement_estimated_used_link_ratio": "",
        "direct_dfg_edge_count": "",
        "routed_dfg_edge_count": "",
        "direct_dfg_edge_ratio": "",
        "routed_path_count": "",
        "routed_unreachable_edge_count": "",
        "routed_path_length_sum": "",
        "routed_avg_path_length": "",
        "routed_max_path_length": "",
        "routed_spatial_hop_sum": "",
        "routed_avg_spatial_hop": "",
        "routed_max_spatial_hop": "",
        "routed_fifo_sum": "",
        "routed_avg_fifo": "",
        "routed_max_fifo": "",
        "routed_mapped_lp": "",
    }
    if not mapping_path or not mapping_path.exists():
        return empty

    mapping = load_json(mapping_path)
    if placement_cost_model == "mesh" and mapping.get("network_type") in {
        "one_hop_axis2",
        "one_hop",
        "1hop",
    }:
        placement_cost_model = "one_hop_axis2"
        empty["placement_cost_model"] = placement_cost_model
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
    config_by_id = {}
    outgoing_by_config = defaultdict(list)
    op_locations_by_name = defaultdict(list)
    op_position_by_name = {}
    to_config_ids_by_name = {}
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

            config_id = (row, col, context_id)
            to_config_ids = parse_config_id_list(config.get("to_config_id", []))
            to_config_tuples = [config_id_tuple(to_config_id) for to_config_id in to_config_ids]
            op_name = clean_dot_value(config.get("operation_name", "")) if op != NOP_OP else ""
            config_by_id[config_id] = {
                "operation_type": op,
                "operation_name": op_name,
            }
            outgoing_by_config[config_id].extend(to_config_tuples)
            if op != NOP_OP:
                if op != ROUTE_OP and op_name:
                    op_position_by_name[op_name] = config_id
                    op_locations_by_name[op_name].append(config_id)
                    to_config_ids_by_name[op_name] = to_config_tuples
                fanouts.append(len(to_config_ids))
            for to_config_id in to_config_tuples:
                to_row, to_col, to_context = to_config_id
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

    direct_dfg_edge_count = ""
    routed_dfg_edge_count = ""
    direct_dfg_edge_ratio = ""
    routed_path_count = ""
    routed_unreachable_edge_count = ""
    routed_path_length_sum = ""
    routed_avg_path_length = ""
    routed_max_path_length = ""
    routed_spatial_hop_sum = ""
    routed_avg_spatial_hop = ""
    routed_max_spatial_hop = ""
    routed_fifo_sum = ""
    routed_avg_fifo = ""
    routed_max_fifo = ""
    routed_mapped_lp = ""
    placement_edge_count = ""
    placement_wirelength_sum = ""
    placement_avg_wirelength = ""
    placement_max_wirelength = ""
    placement_direct_edge_count = ""
    placement_direct_edge_ratio = ""
    placement_fifo_sum = ""
    placement_avg_fifo = ""
    placement_max_fifo = ""
    placement_cost_sum = ""
    placement_avg_cost = ""
    placement_max_cost = ""
    placement_optimal_edge_count = ""
    placement_optimal_edge_ratio = ""
    placement_optimal_distance_count = ""
    placement_optimal_distance_ratio = ""
    placement_fifo_like_sum = ""
    placement_avg_fifo_like = ""
    placement_max_fifo_like = ""
    placement_mesh_hop_sum = ""
    placement_avg_mesh_hop = ""
    placement_max_mesh_hop = ""
    placement_mesh_optimal_edge_count = ""
    placement_mesh_optimal_edge_ratio = ""
    placement_mesh_fifo_sum = ""
    placement_avg_mesh_fifo = ""
    placement_p90_fifo = ""
    placement_p95_fifo = ""
    placement_max_mesh_fifo = ""
    placement_mapped_lp_mesh_hop = ""
    placement_criticality_weighted_mesh_hop = ""
    placement_criticality_weighted_fifo = ""
    placement_max_critical_edge_mesh_hop = ""
    placement_max_critical_edge_fifo = ""
    placement_max_cut_congestion = ""
    placement_avg_cut_congestion = ""
    placement_p95_cut_congestion = ""
    placement_max_horizontal_cut_congestion = ""
    placement_max_vertical_cut_congestion = ""
    placement_estimated_total_link_demand = ""
    placement_estimated_max_link_demand = ""
    placement_estimated_avg_link_demand = ""
    placement_estimated_p95_link_demand = ""
    placement_estimated_used_link_ratio = ""
    if dfg_path and dfg_path.exists():
        raw_graph = read_dot_graph(dfg_path)
        clean_graph_nodes = {
            clean_node_name(node)
            for node, attrs in raw_graph.nodes(data=True)
            if clean_dot_value(attrs.get("opcode", ""))
        }
        edge_criticality = dfg_edge_criticality(raw_graph, clean_graph_nodes)
        total_dfg_edges = 0
        direct_edges = 0
        placement_wirelengths = []
        placement_costs = []
        mesh_weighted_edges = []
        criticality_weights = []
        criticality_weighted_hop_sum = 0.0
        criticality_weighted_fifo_sum = 0.0
        critical_edge_hops = []
        critical_edge_fifos = []
        horizontal_cuts = defaultdict(int)
        vertical_cuts = defaultdict(int)
        estimated_link_demand = defaultdict(int)
        routed_path_lengths = []
        routed_spatial_hops = []
        routed_fifo_values = []
        routed_weighted_edges = []
        unreachable_edges = 0
        for src, dst in raw_graph.edges():
            clean_src = clean_node_name(src)
            clean_dst = clean_node_name(dst)
            if clean_src not in op_position_by_name or clean_dst not in op_position_by_name:
                continue
            total_dfg_edges += 1
            src_position = op_position_by_name[clean_src]
            dst_position = op_position_by_name[clean_dst]
            manhattan = abs(src_position[0] - dst_position[0]) + abs(src_position[1] - dst_position[1])
            placement_wirelengths.append(manhattan)
            placement_fifo = max(0, manhattan - 1)
            if edge_criticality is not None:
                criticality = edge_criticality.get((clean_src, clean_dst), 1.0)
                criticality_weights.append(criticality)
                criticality_weighted_hop_sum += criticality * manhattan
                criticality_weighted_fifo_sum += criticality * placement_fifo
                if criticality >= 1.0:
                    critical_edge_hops.append(manhattan)
                    critical_edge_fifos.append(placement_fifo)
            update_cut_congestion(
                src_position[0],
                src_position[1],
                dst_position[0],
                dst_position[1],
                horizontal_cuts,
                vertical_cuts,
            )
            update_xy_link_demand(
                src_position[0],
                src_position[1],
                dst_position[0],
                dst_position[1],
                estimated_link_demand,
            )
            mesh_weighted_edges.append((clean_src, clean_dst, max(1, manhattan)))
            placement_costs.append(
                placement_cost(
                    abs(src_position[0] - dst_position[0]),
                    abs(src_position[1] - dst_position[1]),
                    placement_cost_model,
                )
            )
            if op_position_by_name[clean_dst] in to_config_ids_by_name.get(clean_src, []):
                direct_edges += 1
            route_path = find_routed_path(
                op_locations_by_name.get(clean_src, []),
                op_locations_by_name.get(clean_dst, []),
                config_by_id,
                outgoing_by_config,
            )
            if route_path:
                path_length = len(route_path) - 1
                routed_path_lengths.append(path_length)
                routed_spatial_hops.append(path_spatial_hop(route_path))
                routed_fifo_values.append(max(0, path_length - 1))
                routed_weighted_edges.append((clean_src, clean_dst, max(1, path_length)))
            else:
                unreachable_edges += 1
        if total_dfg_edges:
            direct_dfg_edge_count = direct_edges
            routed_dfg_edge_count = total_dfg_edges - direct_edges
            direct_dfg_edge_ratio = direct_edges / total_dfg_edges
            placement_edge_count = total_dfg_edges
            placement_wirelength_sum = sum(placement_wirelengths)
            placement_avg_wirelength = placement_wirelength_sum / total_dfg_edges
            placement_max_wirelength = max(placement_wirelengths)
            placement_direct_edge_count = sum(1 for distance in placement_wirelengths if distance <= 1)
            placement_direct_edge_ratio = placement_direct_edge_count / total_dfg_edges
            placement_fifo_values = [max(0, distance - 1) for distance in placement_wirelengths]
            placement_fifo_sum = sum(placement_fifo_values)
            placement_avg_fifo = placement_fifo_sum / total_dfg_edges
            placement_max_fifo = max(placement_fifo_values)
            placement_p90_fifo = percentile(placement_fifo_values, 90)
            placement_p95_fifo = percentile(placement_fifo_values, 95)
            placement_cost_sum = sum(placement_costs)
            placement_avg_cost = placement_cost_sum / total_dfg_edges
            placement_max_cost = max(placement_costs)
            placement_optimal_edge_count = sum(1 for cost in placement_costs if cost <= 1)
            placement_optimal_edge_ratio = placement_optimal_edge_count / total_dfg_edges
            placement_optimal_distance_count = sum(
                1 for distance in placement_wirelengths if distance == 1
            )
            placement_optimal_distance_ratio = (
                placement_optimal_distance_count / total_dfg_edges
            )
            placement_paper_fifo_values = [max(0, cost - 1) for cost in placement_costs]
            placement_fifo_like_sum = sum(placement_paper_fifo_values)
            placement_avg_fifo_like = placement_fifo_like_sum / total_dfg_edges
            placement_max_fifo_like = max(placement_paper_fifo_values)
            placement_mesh_hop_sum = placement_wirelength_sum
            placement_avg_mesh_hop = placement_avg_wirelength
            placement_max_mesh_hop = placement_max_wirelength
            placement_mesh_optimal_edge_count = placement_direct_edge_count
            placement_mesh_optimal_edge_ratio = placement_direct_edge_ratio
            placement_mesh_fifo_sum = placement_fifo_sum
            placement_avg_mesh_fifo = placement_avg_fifo
            placement_max_mesh_fifo = placement_max_fifo
            placement_mapped_lp_mesh_hop = longest_path_cost(
                op_position_by_name.keys(), mesh_weighted_edges
            )
            criticality_weight_sum = sum(criticality_weights)
            if criticality_weight_sum:
                placement_criticality_weighted_mesh_hop = (
                    criticality_weighted_hop_sum / criticality_weight_sum
                )
                placement_criticality_weighted_fifo = (
                    criticality_weighted_fifo_sum / criticality_weight_sum
                )
            placement_max_critical_edge_mesh_hop = (
                max(critical_edge_hops) if critical_edge_hops else ""
            )
            placement_max_critical_edge_fifo = (
                max(critical_edge_fifos) if critical_edge_fifos else ""
            )
            horizontal_cut_values = [
                horizontal_cuts[cut_row] for cut_row in range(max(0, rows - 1))
            ]
            vertical_cut_values = [
                vertical_cuts[cut_col] for cut_col in range(max(0, cols - 1))
            ]
            cut_values = horizontal_cut_values + vertical_cut_values
            if cut_values:
                placement_max_cut_congestion = max(cut_values)
                placement_avg_cut_congestion = sum(cut_values) / len(cut_values)
                placement_p95_cut_congestion = percentile(cut_values, 95)
                placement_max_horizontal_cut_congestion = (
                    max(horizontal_cut_values) if horizontal_cut_values else 0
                )
                placement_max_vertical_cut_congestion = (
                    max(vertical_cut_values) if vertical_cut_values else 0
                )
            total_grid_links = rows * max(0, cols - 1) + cols * max(0, rows - 1)
            if total_grid_links:
                all_link_values = list(estimated_link_demand.values()) + [
                    0 for _ in range(total_grid_links - len(estimated_link_demand))
                ]
                placement_estimated_total_link_demand = sum(all_link_values)
                placement_estimated_max_link_demand = max(all_link_values)
                placement_estimated_avg_link_demand = (
                    placement_estimated_total_link_demand / total_grid_links
                )
                placement_estimated_p95_link_demand = percentile(all_link_values, 95)
                placement_estimated_used_link_ratio = safe_ratio(
                    len(estimated_link_demand), total_grid_links
                )
            routed_path_count = len(routed_path_lengths)
            routed_unreachable_edge_count = unreachable_edges
            if routed_path_lengths:
                routed_path_length_sum = sum(routed_path_lengths)
                routed_avg_path_length = routed_path_length_sum / len(routed_path_lengths)
                routed_max_path_length = max(routed_path_lengths)
                routed_spatial_hop_sum = sum(routed_spatial_hops)
                routed_avg_spatial_hop = routed_spatial_hop_sum / len(routed_spatial_hops)
                routed_max_spatial_hop = max(routed_spatial_hops)
                routed_fifo_sum = sum(routed_fifo_values)
                routed_avg_fifo = routed_fifo_sum / len(routed_fifo_values)
                routed_max_fifo = max(routed_fifo_values)
            if routed_path_count == total_dfg_edges:
                routed_mapped_lp = longest_path_cost(
                    op_position_by_name.keys(), routed_weighted_edges
                )

    empty.update(
        {
            "rows": rows,
            "cols": cols,
            "context_size": context_size,
            "memory_io": mapping.get("memory_io_type", ""),
            "cgra_type": mapping.get("cgra_type", ""),
            "network_type": mapping.get("network_type", ""),
            "placement_cost_model": placement_cost_model,
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
            "placement_edge_count": placement_edge_count,
            "placement_wirelength_sum": placement_wirelength_sum,
            "placement_avg_wirelength": placement_avg_wirelength,
            "placement_max_wirelength": placement_max_wirelength,
            "placement_direct_edge_count": placement_direct_edge_count,
            "placement_direct_edge_ratio": placement_direct_edge_ratio,
            "placement_fifo_sum": placement_fifo_sum,
            "placement_avg_fifo": placement_avg_fifo,
            "placement_max_fifo": placement_max_fifo,
            "placement_cost_sum": placement_cost_sum,
            "placement_avg_cost": placement_avg_cost,
            "placement_max_cost": placement_max_cost,
            "placement_optimal_edge_count": placement_optimal_edge_count,
            "placement_optimal_edge_ratio": placement_optimal_edge_ratio,
            "placement_optimal_distance_count": placement_optimal_distance_count,
            "placement_optimal_distance_ratio": placement_optimal_distance_ratio,
            "placement_fifo_like_sum": placement_fifo_like_sum,
            "placement_avg_fifo_like": placement_avg_fifo_like,
            "placement_max_fifo_like": placement_max_fifo_like,
            "placement_paper_fifo_sum": placement_fifo_like_sum,
            "placement_avg_paper_fifo": placement_avg_fifo_like,
            "placement_max_paper_fifo": placement_max_fifo_like,
            "placement_mesh_hop_sum": placement_mesh_hop_sum,
            "placement_avg_mesh_hop": placement_avg_mesh_hop,
            "placement_max_mesh_hop": placement_max_mesh_hop,
            "placement_mesh_optimal_edge_count": placement_mesh_optimal_edge_count,
            "placement_mesh_optimal_edge_ratio": placement_mesh_optimal_edge_ratio,
            "placement_mesh_fifo_sum": placement_mesh_fifo_sum,
            "placement_avg_mesh_fifo": placement_avg_mesh_fifo,
            "placement_p90_fifo": placement_p90_fifo,
            "placement_p95_fifo": placement_p95_fifo,
            "placement_max_mesh_fifo": placement_max_mesh_fifo,
            "placement_mapped_lp_mesh_hop": placement_mapped_lp_mesh_hop,
            "placement_criticality_weighted_mesh_hop": placement_criticality_weighted_mesh_hop,
            "placement_criticality_weighted_fifo": placement_criticality_weighted_fifo,
            "placement_max_critical_edge_mesh_hop": placement_max_critical_edge_mesh_hop,
            "placement_max_critical_edge_fifo": placement_max_critical_edge_fifo,
            "placement_max_cut_congestion": placement_max_cut_congestion,
            "placement_avg_cut_congestion": placement_avg_cut_congestion,
            "placement_p95_cut_congestion": placement_p95_cut_congestion,
            "placement_max_horizontal_cut_congestion": placement_max_horizontal_cut_congestion,
            "placement_max_vertical_cut_congestion": placement_max_vertical_cut_congestion,
            "placement_estimated_total_link_demand": placement_estimated_total_link_demand,
            "placement_estimated_max_link_demand": placement_estimated_max_link_demand,
            "placement_estimated_avg_link_demand": placement_estimated_avg_link_demand,
            "placement_estimated_p95_link_demand": placement_estimated_p95_link_demand,
            "placement_estimated_used_link_ratio": placement_estimated_used_link_ratio,
            "direct_dfg_edge_count": direct_dfg_edge_count,
            "routed_dfg_edge_count": routed_dfg_edge_count,
            "direct_dfg_edge_ratio": direct_dfg_edge_ratio,
            "routed_path_count": routed_path_count,
            "routed_unreachable_edge_count": routed_unreachable_edge_count,
            "routed_path_length_sum": routed_path_length_sum,
            "routed_avg_path_length": routed_avg_path_length,
            "routed_max_path_length": routed_max_path_length,
            "routed_spatial_hop_sum": routed_spatial_hop_sum,
            "routed_avg_spatial_hop": routed_avg_spatial_hop,
            "routed_max_spatial_hop": routed_max_spatial_hop,
            "routed_fifo_sum": routed_fifo_sum,
            "routed_avg_fifo": routed_avg_fifo,
            "routed_max_fifo": routed_max_fifo,
            "routed_mapped_lp": routed_mapped_lp,
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
    arch_path: Optional[Path] = None,
    evaluation_mode: str = "routing",
) -> dict:
    input_log_path = find_one(run_dir, "input_log_*.json")
    output_log_path = find_one(run_dir, "output_log_*.json")
    mapping_path = find_one(run_dir, "mapping_*.json")
    gurobi_log_path = find_one(run_dir, "gurobi_log_*.log")

    input_log = load_json(input_log_path) if input_log_path else {}
    output_log = load_json(output_log_path) if output_log_path else {}
    if dfg_path is None and input_log.get("dfg_file"):
        dfg_path = Path(input_log["dfg_file"])
    cgra_metrics = mapping_utilization(mapping_path, dfg_path, arch_path)
    gurobi_metrics = parse_gurobi_log(gurobi_log_path)
    placement_log_metrics = parse_placement_log(gurobi_log_path)

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
        "evaluation_mode": evaluation_mode,
        "cgra_type": cgra_metrics["cgra_type"],
        "network_type": cgra_metrics["network_type"],
        "memory_io": cgra_metrics["memory_io"],
        "placement_cost_model": cgra_metrics["placement_cost_model"],
        "rows": cgra_metrics["rows"],
        "cols": cgra_metrics["cols"],
        "context_size": cgra_metrics["context_size"],
        "MII": mii if mii is not None else "",
        "start_II": start_ii if start_ii is not None else "",
        "achieved_II": achieved_ii if achieved_ii is not None else "",
        "II_ratio": ii_ratio,
        "status": status,
        "mapping_time_sec": output_log.get("mapping_time_s", ""),
        "placement_swap_attempts": placement_log_metrics[
            "placement_swap_attempts"
        ],
        "placement_cell_visits": placement_log_metrics["placement_cell_visits"],
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
        "placement_edge_count": cgra_metrics["placement_edge_count"],
        "placement_wirelength_sum": cgra_metrics["placement_wirelength_sum"],
        "placement_avg_wirelength": cgra_metrics["placement_avg_wirelength"],
        "placement_max_wirelength": cgra_metrics["placement_max_wirelength"],
        "placement_direct_edge_count": cgra_metrics["placement_direct_edge_count"],
        "placement_direct_edge_ratio": cgra_metrics["placement_direct_edge_ratio"],
        "placement_fifo_sum": cgra_metrics["placement_fifo_sum"],
        "placement_avg_fifo": cgra_metrics["placement_avg_fifo"],
        "placement_max_fifo": cgra_metrics["placement_max_fifo"],
        "placement_cost_sum": cgra_metrics["placement_cost_sum"],
        "placement_avg_cost": cgra_metrics["placement_avg_cost"],
        "placement_max_cost": cgra_metrics["placement_max_cost"],
        "placement_optimal_edge_count": cgra_metrics["placement_optimal_edge_count"],
        "placement_optimal_edge_ratio": cgra_metrics["placement_optimal_edge_ratio"],
        "placement_optimal_distance_count": cgra_metrics[
            "placement_optimal_distance_count"
        ],
        "placement_optimal_distance_ratio": cgra_metrics[
            "placement_optimal_distance_ratio"
        ],
        "placement_fifo_like_sum": cgra_metrics["placement_fifo_like_sum"],
        "placement_avg_fifo_like": cgra_metrics["placement_avg_fifo_like"],
        "placement_max_fifo_like": cgra_metrics["placement_max_fifo_like"],
        "placement_paper_fifo_sum": cgra_metrics["placement_paper_fifo_sum"],
        "placement_avg_paper_fifo": cgra_metrics["placement_avg_paper_fifo"],
        "placement_max_paper_fifo": cgra_metrics["placement_max_paper_fifo"],
        "placement_mesh_hop_sum": cgra_metrics["placement_mesh_hop_sum"],
        "placement_avg_mesh_hop": cgra_metrics["placement_avg_mesh_hop"],
        "placement_max_mesh_hop": cgra_metrics["placement_max_mesh_hop"],
        "placement_mesh_optimal_edge_count": cgra_metrics[
            "placement_mesh_optimal_edge_count"
        ],
        "placement_mesh_optimal_edge_ratio": cgra_metrics[
            "placement_mesh_optimal_edge_ratio"
        ],
        "placement_mesh_fifo_sum": cgra_metrics["placement_mesh_fifo_sum"],
        "placement_avg_mesh_fifo": cgra_metrics["placement_avg_mesh_fifo"],
        "placement_p90_fifo": cgra_metrics["placement_p90_fifo"],
        "placement_p95_fifo": cgra_metrics["placement_p95_fifo"],
        "placement_max_mesh_fifo": cgra_metrics["placement_max_mesh_fifo"],
        "placement_mapped_lp_mesh_hop": cgra_metrics[
            "placement_mapped_lp_mesh_hop"
        ],
        "placement_criticality_weighted_mesh_hop": cgra_metrics[
            "placement_criticality_weighted_mesh_hop"
        ],
        "placement_criticality_weighted_fifo": cgra_metrics[
            "placement_criticality_weighted_fifo"
        ],
        "placement_max_critical_edge_mesh_hop": cgra_metrics[
            "placement_max_critical_edge_mesh_hop"
        ],
        "placement_max_critical_edge_fifo": cgra_metrics[
            "placement_max_critical_edge_fifo"
        ],
        "placement_max_cut_congestion": cgra_metrics[
            "placement_max_cut_congestion"
        ],
        "placement_avg_cut_congestion": cgra_metrics[
            "placement_avg_cut_congestion"
        ],
        "placement_p95_cut_congestion": cgra_metrics[
            "placement_p95_cut_congestion"
        ],
        "placement_max_horizontal_cut_congestion": cgra_metrics[
            "placement_max_horizontal_cut_congestion"
        ],
        "placement_max_vertical_cut_congestion": cgra_metrics[
            "placement_max_vertical_cut_congestion"
        ],
        "placement_estimated_total_link_demand": cgra_metrics[
            "placement_estimated_total_link_demand"
        ],
        "placement_estimated_max_link_demand": cgra_metrics[
            "placement_estimated_max_link_demand"
        ],
        "placement_estimated_avg_link_demand": cgra_metrics[
            "placement_estimated_avg_link_demand"
        ],
        "placement_estimated_p95_link_demand": cgra_metrics[
            "placement_estimated_p95_link_demand"
        ],
        "placement_estimated_used_link_ratio": cgra_metrics[
            "placement_estimated_used_link_ratio"
        ],
        "direct_dfg_edge_count": cgra_metrics["direct_dfg_edge_count"],
        "routed_dfg_edge_count": cgra_metrics["routed_dfg_edge_count"],
        "direct_dfg_edge_ratio": cgra_metrics["direct_dfg_edge_ratio"],
        "routed_path_count": cgra_metrics["routed_path_count"],
        "routed_unreachable_edge_count": cgra_metrics["routed_unreachable_edge_count"],
        "routed_path_length_sum": cgra_metrics["routed_path_length_sum"],
        "routed_avg_path_length": cgra_metrics["routed_avg_path_length"],
        "routed_max_path_length": cgra_metrics["routed_max_path_length"],
        "routed_spatial_hop_sum": cgra_metrics["routed_spatial_hop_sum"],
        "routed_avg_spatial_hop": cgra_metrics["routed_avg_spatial_hop"],
        "routed_max_spatial_hop": cgra_metrics["routed_max_spatial_hop"],
        "routed_fifo_sum": cgra_metrics["routed_fifo_sum"],
        "routed_avg_fifo": cgra_metrics["routed_avg_fifo"],
        "routed_max_fifo": cgra_metrics["routed_max_fifo"],
        "routed_mapped_lp": cgra_metrics["routed_mapped_lp"],
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
    "mapper_role",
    "placement_method",
    "routing_method",
    "arch_name",
    "evaluation_mode",
    "cgra_type",
    "network_type",
    "memory_io",
    "placement_cost_model",
    "rows",
    "cols",
    "context_size",
    "MII",
    "start_II",
    "achieved_II",
    "II_ratio",
    "status",
    "mapping_time_sec",
    "placement_swap_attempts",
    "placement_cell_visits",
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
    "placement_edge_count",
    "placement_wirelength_sum",
    "placement_avg_wirelength",
    "placement_max_wirelength",
    "placement_direct_edge_count",
    "placement_direct_edge_ratio",
    "placement_fifo_sum",
    "placement_avg_fifo",
    "placement_max_fifo",
    "placement_cost_sum",
    "placement_avg_cost",
    "placement_max_cost",
    "placement_optimal_edge_count",
    "placement_optimal_edge_ratio",
    "placement_optimal_distance_count",
    "placement_optimal_distance_ratio",
    "placement_fifo_like_sum",
    "placement_avg_fifo_like",
    "placement_max_fifo_like",
    "placement_paper_fifo_sum",
    "placement_avg_paper_fifo",
    "placement_max_paper_fifo",
    "placement_mesh_hop_sum",
    "placement_avg_mesh_hop",
    "placement_max_mesh_hop",
    "placement_mesh_optimal_edge_count",
    "placement_mesh_optimal_edge_ratio",
    "placement_mesh_fifo_sum",
    "placement_avg_mesh_fifo",
    "placement_p90_fifo",
    "placement_p95_fifo",
    "placement_max_mesh_fifo",
    "placement_mapped_lp_mesh_hop",
    "placement_criticality_weighted_mesh_hop",
    "placement_criticality_weighted_fifo",
    "placement_max_critical_edge_mesh_hop",
    "placement_max_critical_edge_fifo",
    "placement_max_cut_congestion",
    "placement_avg_cut_congestion",
    "placement_p95_cut_congestion",
    "placement_max_horizontal_cut_congestion",
    "placement_max_vertical_cut_congestion",
    "placement_estimated_total_link_demand",
    "placement_estimated_max_link_demand",
    "placement_estimated_avg_link_demand",
    "placement_estimated_p95_link_demand",
    "placement_estimated_used_link_ratio",
    "blif_net_count",
    "blif_cost_sum",
    "blif_avg_cost",
    "blif_max_cost",
    "blif_optimal_net_count",
    "blif_optimal_net_ratio",
    "blif_paper_fifo_sum",
    "blif_avg_paper_fifo",
    "blif_max_paper_fifo",
    "direct_dfg_edge_count",
    "routed_dfg_edge_count",
    "direct_dfg_edge_ratio",
    "routed_path_count",
    "routed_unreachable_edge_count",
    "routed_path_length_sum",
    "routed_avg_path_length",
    "routed_max_path_length",
    "routed_spatial_hop_sum",
    "routed_avg_spatial_hop",
    "routed_max_spatial_hop",
    "routed_fifo_sum",
    "routed_avg_fifo",
    "routed_max_fifo",
    "routed_mapped_lp",
    "vpr_inner_num",
    "vpr_extra_args",
    "vpr_io_capacity",
    "gurobi_status",
    "objective_value",
    "best_bound",
    "mip_gap",
    "mapping_file",
    "gurobi_log_file",
    "external_reason",
    "external_log_file",
    "run_dir",
    "trial_dir",
    "stdout_file",
    "stderr_file",
    "arch_file",
    "raw_output_dir",
]


def write_metrics_csv(path: Path, rows: Iterable[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in CSV_FIELDS})
