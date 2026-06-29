#!/usr/bin/env python3
"""Compare YOTT VPR-BB FIFO hypotheses against per-benchmark paper values."""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from collections import defaultdict, deque
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from lib import clean_node_name, resolve_repo_path  # noqa: E402
from run_vpr_baseline import (  # noqa: E402
    find_net_file,
    find_place_file,
    parse_packed_net_positions,
    parse_place_file,
    placement_cost,
)


DEFAULT_METRICS = (
    "research/results/placement2d/probes/yott_vpr_multi_metric_policy/"
    "20260628-143157_inner_grid_sweep/metrics.csv"
)
DEFAULT_PAPER = "research/data/papers/yott_figure_paper_metrics.csv"
DEFAULT_OUTPUT = (
    "research/results/placement2d/probes/yott_vpr_bb_candidate_reproduction/"
    "20260628-143157_inner_grid_sweep_summary/yott_vpr_bb_fifo_hypotheses.csv"
)

ALIASES = {
    "Cplx8": "cplx8",
    "Fir16": "fir16",
    "FilterRGB": "filter_rgb",
}


def canonical_benchmark(name: str) -> str:
    return ALIASES.get(name, name)


def as_float(row: dict[str, str], key: str) -> float | None:
    value = row.get(key, "")
    if value in ("", None):
        return None
    return float(value)


def read_paper_fifo(path: Path) -> dict[str, float]:
    values = {}
    with path.open(newline="") as f:
        for row in csv.DictReader(f):
            if row.get("method") != "VPR_BB":
                continue
            if row.get("metric") != "maximal_fifo_size":
                continue
            values[canonical_benchmark(row["benchmark"])] = float(row["value"])
    return values


def read_dfg_edges(path: Path) -> tuple[list[str], list[tuple[str, str]]]:
    nodes = []
    node_set = set()
    edges = []
    for raw_line in path.read_text().splitlines():
        line = raw_line.strip().rstrip(";")
        if not line or line.startswith("//") or line in {"digraph G {", "}"}:
            continue
        if "->" in line:
            src, rest = line.split("->", 1)
            dst = rest.split("[", 1)[0].strip()
            edges.append((clean_node_name(src), clean_node_name(dst)))
            continue
        if "[opcode=" in line:
            node = clean_node_name(line.split("[", 1)[0])
            nodes.append(node)
            node_set.add(node)
    edges = [(src, dst) for src, dst in edges if src in node_set and dst in node_set]
    return nodes, edges


def fanout_net_max_cost(
    edges: list[tuple[str, str]],
    positions: dict[str, tuple[int, int]],
    cost_model: str,
    shifted: bool = False,
) -> float | None:
    """Max source-fanout bounding-box cost.

    VPR's bounding-box placer optimizes nets, not individual DFG edges.  A DFG
    node with fanout becomes one VPR net whose bounding box covers the source
    and every sink.  This metric tests whether Fig. 21(b) follows that net-level
    convention instead of the edge-level convention used by YOTO/YOTT outputs.
    """
    fanouts: dict[str, list[str]] = defaultdict(list)
    for src, dst in edges:
        fanouts[src].append(dst)

    values = []
    for src, dsts in fanouts.items():
        net_nodes = [src] + dsts
        if any(node not in positions for node in net_nodes):
            continue
        rows = [positions[node][0] for node in net_nodes]
        cols = [positions[node][1] for node in net_nodes]
        value = placement_cost(max(cols) - min(cols), max(rows) - min(rows), cost_model)
        if shifted:
            value = max(0, value - 1)
        values.append(float(value))
    if not values:
        return None
    return max(values)


def fanout_branch_max_cost(
    edges: list[tuple[str, str]],
    positions: dict[str, tuple[int, int]],
    cost_model: str,
    shifted: bool = False,
) -> float | None:
    """Max fanout-tree branch cost with an optimal single branch point.

    VPR optimizes fanout nets as route trees, while the DFG-level metrics see
    every source-sink pair independently.  This cheap proxy tests whether a
    branch point can explain cases where the paper FIFO is lower than the
    source-to-sink bounding-box maximum.
    """
    fanouts: dict[str, list[str]] = defaultdict(list)
    for src, dst in edges:
        fanouts[src].append(dst)

    values = []
    for src, dsts in fanouts.items():
        net_nodes = [src] + dsts
        if any(node not in positions for node in net_nodes):
            continue
        if len(dsts) == 1:
            src_row, src_col = positions[src]
            dst_row, dst_col = positions[dsts[0]]
            value = placement_cost(
                abs(src_col - dst_col),
                abs(src_row - dst_row),
                cost_model,
            )
        else:
            rows = [positions[node][0] for node in net_nodes]
            cols = [positions[node][1] for node in net_nodes]
            best_value = None
            for branch_row in range(min(rows), max(rows) + 1):
                for branch_col in range(min(cols), max(cols) + 1):
                    src_row, src_col = positions[src]
                    branch_costs = [
                        placement_cost(
                            abs(src_col - branch_col),
                            abs(src_row - branch_row),
                            cost_model,
                        )
                    ]
                    for dst in dsts:
                        dst_row, dst_col = positions[dst]
                        branch_costs.append(
                            placement_cost(
                                abs(dst_col - branch_col),
                                abs(dst_row - branch_row),
                                cost_model,
                            )
                        )
                    candidate = max(branch_costs)
                    if best_value is None or candidate < best_value:
                        best_value = candidate
            value = best_value if best_value is not None else 0
        if shifted:
            value = max(0, value - 1)
        values.append(float(value))
    if not values:
        return None
    return max(values)


def load_positions(row: dict[str, str], repo_root: Path) -> dict[str, tuple[int, int]]:
    run_dir = resolve_repo_path(row["run_dir"], repo_root)
    work_dir = run_dir / "vpr"
    dfg = resolve_repo_path(row["raw_output_dir"], repo_root)
    name_map_path = work_dir / "vpr_name_map.json"
    if not name_map_path.exists():
        return {}
    import json

    name_map = json.loads(name_map_path.read_text())
    reverse_map = name_map.get("reverse_map") or name_map.get("blif_to_original", {})
    # `find_place_file` only needs a path with the benchmark stem.
    blif_path = work_dir / f"{row['benchmark']}.blif"
    place_path = find_place_file(work_dir, blif_path)
    net_path = find_net_file(work_dir, blif_path)
    top_positions, _, _ = parse_place_file(place_path, reverse_map)
    return parse_packed_net_positions(net_path, top_positions, reverse_map)


def longest_arrival_fifo(
    nodes: list[str],
    edges: list[tuple[str, str]],
    positions: dict[str, tuple[int, int]],
    cost_model: str,
    edge_latency_mode: str,
) -> float | None:
    successors = {node: [] for node in nodes}
    indegree = {node: 0 for node in nodes}
    for src, dst in edges:
        if src not in successors or dst not in indegree:
            continue
        successors[src].append(dst)
        indegree[dst] += 1
    queue = deque(node for node in nodes if indegree[node] == 0)
    topo_order = []
    while queue:
        node = queue.popleft()
        topo_order.append(node)
        for succ in successors[node]:
            indegree[succ] -= 1
            if indegree[succ] == 0:
                queue.append(succ)
    if len(topo_order) != len(nodes):
        return None

    incoming_arrivals: dict[str, list[float]] = defaultdict(list)
    arrival = {node: 0.0 for node in nodes}
    max_fifo = 0.0

    def edge_delay(src: str, dst: str) -> float:
        if src not in positions or dst not in positions:
            return 0.0
        sr, sc = positions[src]
        dr, dc = positions[dst]
        mesh = abs(sr - dr) + abs(sc - dc)
        cost = placement_cost(abs(sr - dr), abs(sc - dc), cost_model)
        if edge_latency_mode == "cost":
            return float(cost)
        if edge_latency_mode == "paper_fifo":
            return float(max(0, cost - 1))
        if edge_latency_mode == "mesh":
            return float(mesh)
        if edge_latency_mode == "mesh_fifo":
            return float(max(0, mesh - 1))
        raise ValueError(f"unknown edge latency mode: {edge_latency_mode}")

    for node in topo_order:
        arrivals = incoming_arrivals.get(node, [])
        if arrivals:
            latest = max(arrivals)
            max_fifo = max(max_fifo, max(latest - value for value in arrivals))
            arrival[node] = latest
        for succ in successors[node]:
            incoming_arrivals[succ].append(arrival[node] + edge_delay(node, succ))
    return max_fifo


def scheduled_arrival_fifo(
    nodes: list[str],
    edges: list[tuple[str, str]],
    positions: dict[str, tuple[int, int]],
    cost_model: str,
    edge_latency_mode: str,
    op_latency: float = 1.0,
    exclude_graph_io_edges: bool = False,
) -> float | None:
    """Input-alignment FIFO with a simple per-operation pipeline latency.

    The placement-only edge maximum cannot raise FIR-style reconvergent paths
    above their longest physical edge.  A FIFO scheduler, however, may need to
    delay an early input until another input arrives through a deeper operation
    chain.  This probe keeps the model intentionally small: every operation has
    the same latency, and only the maximum input skew at each consumer is
    reported.
    """
    successors = {node: [] for node in nodes}
    predecessors = {node: [] for node in nodes}
    indegree = {node: 0 for node in nodes}
    for src, dst in edges:
        if src not in successors or dst not in indegree:
            continue
        successors[src].append(dst)
        predecessors[dst].append(src)
        indegree[dst] += 1
    queue = deque(node for node in nodes if indegree[node] == 0)
    topo_order = []
    while queue:
        node = queue.popleft()
        topo_order.append(node)
        for succ in successors[node]:
            indegree[succ] -= 1
            if indegree[succ] == 0:
                queue.append(succ)
    if len(topo_order) != len(nodes):
        return None

    def edge_delay(src: str, dst: str) -> float:
        if src not in positions or dst not in positions:
            return 0.0
        sr, sc = positions[src]
        dr, dc = positions[dst]
        mesh = abs(sr - dr) + abs(sc - dc)
        cost = placement_cost(abs(sc - dc), abs(sr - dr), cost_model)
        if edge_latency_mode == "cost":
            return float(cost)
        if edge_latency_mode == "paper_fifo":
            return float(max(0, cost - 1))
        if edge_latency_mode == "mesh":
            return float(mesh)
        if edge_latency_mode == "mesh_fifo":
            return float(max(0, mesh - 1))
        raise ValueError(f"unknown edge latency mode: {edge_latency_mode}")

    incoming_arrivals: dict[str, list[tuple[tuple[str, str], float]]] = defaultdict(list)
    ready_time = {node: 0.0 for node in nodes}
    max_fifo = 0.0
    graph_sources = {node for node in nodes if not predecessors[node]}
    graph_sinks = {node for node in nodes if not successors[node]}
    for node in topo_order:
        arrivals = incoming_arrivals.get(node, [])
        if arrivals:
            latest = max(value for _, value in arrivals)
            for (src, dst), value in arrivals:
                if exclude_graph_io_edges and (src in graph_sources or dst in graph_sinks):
                    continue
                max_fifo = max(max_fifo, latest - value)
            ready_time[node] = latest
        departure = ready_time[node] + op_latency
        for succ in successors[node]:
            incoming_arrivals[succ].append(
                ((node, succ), departure + edge_delay(node, succ))
            )
    return max_fifo


def longest_arrival_fifo_from_edge_delays(
    nodes: list[str],
    edges: list[tuple[str, str]],
    edge_delays: dict[tuple[str, str], float],
) -> float | None:
    successors = {node: [] for node in nodes}
    indegree = {node: 0 for node in nodes}
    for src, dst in edges:
        if src not in successors or dst not in indegree:
            continue
        successors[src].append(dst)
        indegree[dst] += 1
    queue = deque(node for node in nodes if indegree[node] == 0)
    topo_order = []
    while queue:
        node = queue.popleft()
        topo_order.append(node)
        for succ in successors[node]:
            indegree[succ] -= 1
            if indegree[succ] == 0:
                queue.append(succ)
    if len(topo_order) != len(nodes):
        return None

    incoming_arrivals: dict[str, list[float]] = defaultdict(list)
    arrival = {node: 0.0 for node in nodes}
    max_fifo = 0.0
    for node in topo_order:
        arrivals = incoming_arrivals.get(node, [])
        if arrivals:
            latest = max(arrivals)
            max_fifo = max(max_fifo, max(latest - value for value in arrivals))
            arrival[node] = latest
        for succ in successors[node]:
            incoming_arrivals[succ].append(
                arrival[node] + float(edge_delays.get((node, succ), 0.0))
            )
    return max_fifo


def original_aligned_fifo_from_edge_delays(
    nodes: list[str],
    edges: list[tuple[str, str]],
    edge_extra_delays: dict[tuple[str, str], float],
    op_latency: float = 1.0,
) -> float | None:
    """FIFO model matching the timing rule described in YOTT Section 2.2.

    The paper first computes logical input arrival times on the original DFG.
    After placement/routing, FIFOs must preserve those relative input-arrival
    differences.  For each consumer input we therefore align
    (mapped_arrival - original_arrival), not the mapped arrivals alone.
    """

    successors = {node: [] for node in nodes}
    indegree = {node: 0 for node in nodes}
    for src, dst in edges:
        if src not in successors or dst not in indegree:
            continue
        successors[src].append(dst)
        indegree[dst] += 1
    queue = deque(node for node in nodes if indegree[node] == 0)
    topo_order = []
    while queue:
        node = queue.popleft()
        topo_order.append(node)
        for succ in successors[node]:
            indegree[succ] -= 1
            if indegree[succ] == 0:
                queue.append(succ)
    if len(topo_order) != len(nodes):
        return None

    incoming_original: dict[str, list[tuple[tuple[str, str], float]]] = defaultdict(list)
    incoming_mapped: dict[str, list[tuple[tuple[str, str], float]]] = defaultdict(list)
    original_ready = {node: 0.0 for node in nodes}
    mapped_ready = {node: 0.0 for node in nodes}
    max_fifo = 0.0

    for node in topo_order:
        original_arrivals = incoming_original.get(node, [])
        mapped_arrivals = incoming_mapped.get(node, [])
        if original_arrivals and mapped_arrivals:
            original_by_edge = dict(original_arrivals)
            offsets = []
            for edge, mapped_arrival in mapped_arrivals:
                original_arrival = original_by_edge.get(edge)
                if original_arrival is None:
                    continue
                offsets.append(mapped_arrival - original_arrival)
            if offsets:
                target_offset = max(offsets)
                max_fifo = max(max_fifo, max(target_offset - offset for offset in offsets))
                original_ready[node] = max(value for _, value in original_arrivals)
                mapped_ready[node] = original_ready[node] + target_offset

        for succ in successors[node]:
            edge = (node, succ)
            incoming_original[succ].append(
                (edge, original_ready[node] + op_latency)
            )
            incoming_mapped[succ].append(
                (
                    edge,
                    mapped_ready[node]
                    + op_latency
                    + float(edge_extra_delays.get(edge, 0.0)),
                )
            )

    return max_fifo


def cpu_mapping_routed_fifo(
    edges: list[tuple[str, str]],
    positions: dict[str, tuple[int, int]],
    cost_model: str,
    sort_by_cost: bool = True,
) -> dict[str, object]:
    """Mimic cpu_mapping's greedy router and report the routed max edge length.

    The author code sorts edges by the placement cost, routes long edges first,
    and then overwrites each edge cost with the number of routing steps. This is
    a closer hypothesis for Fig. 21/22 than a pure placement-distance proxy.
    """

    if any(src not in positions or dst not in positions for src, dst in edges):
        return {
            "cpu_route_success": False,
            "cpu_route_max_cost": None,
            "cpu_route_max_paper_fifo": None,
            "cpu_route_edge_steps": {},
        }

    max_row = max(row for row, _ in positions.values())
    max_col = max(col for _, col in positions.values())
    grid_rows = max_row + 1
    grid_cols = max_col + 1
    total_grid = grid_rows * grid_cols
    is_one_hop = cost_model in {"one_hop_axis2", "cpu_mapping_1hop"}
    direction_count = 8 if is_one_hop else 4
    grid_route: list[list[dict[int, str]]] = [
        [dict() for _ in range(direction_count)] for _ in range(total_grid)
    ]

    def flat(row: int, col: int) -> int:
        return row * grid_cols + col

    def initial_cost(edge: tuple[str, str]) -> int:
        src, dst = edge
        sr, sc = positions[src]
        dr, dc = positions[dst]
        return placement_cost(abs(sr - dr), abs(sc - dc), cost_model)

    routed_steps = []
    edge_steps: dict[tuple[str, str], int] = {}
    route_edges = sorted(edges, key=initial_cost, reverse=True) if sort_by_cost else edges
    for src, dst in route_edges:
        src_row, src_col = positions[src]
        dst_row, dst_col = positions[dst]
        cur_row, cur_col = src_row, src_col
        count_per_curr = [0] * total_grid
        step = 0
        success = True

        def can_move(cur: int, nxt: int, direction: int, opposite: int) -> bool:
            if not (0 <= nxt < total_grid):
                return False
            if count_per_curr[nxt] != 0:
                return False
            current_link = grid_route[cur][direction]
            next_link = grid_route[nxt][opposite]
            if current_link and current_link.get(step) not in (None, src):
                return False
            # cpu_mapping's published router uses an asymmetric check here:
            # the outgoing link may be reused by the same source, while the
            # reverse link at the next PE is rejected only for the same source.
            if step in next_link and next_link[step] == src:
                return False
            return True

        def commit(cur: int, direction: int, delta_row: int, delta_col: int) -> None:
            nonlocal cur_row, cur_col
            grid_route[cur][direction][step] = src
            cur_row += delta_row
            cur_col += delta_col

        while cur_row != dst_row or cur_col != dst_col:
            step += 1
            cur = flat(cur_row, cur_col)
            count_per_curr[cur] += 1
            diff_row = dst_row - cur_row
            diff_col = dst_col - cur_col
            changed = False

            right = cur + 1
            left = cur - 1
            down = cur + grid_cols
            top = cur - grid_cols
            neighbor_right = (
                cur_col + 1 < grid_cols and can_move(cur, right, 1, 3)
            )
            neighbor_left = cur_col - 1 >= 0 and can_move(cur, left, 3, 1)
            neighbor_down = cur_row + 1 < grid_rows and can_move(cur, down, 2, 0)
            neighbor_top = cur_row - 1 >= 0 and can_move(cur, top, 0, 2)

            if is_one_hop:
                right2 = cur + 2
                left2 = cur - 2
                down2 = cur + 2 * grid_cols
                top2 = cur - 2 * grid_cols
                neighbor_right2 = (
                    cur_col + 2 < grid_cols and can_move(cur, right2, 5, 7)
                )
                neighbor_left2 = cur_col - 2 >= 0 and can_move(cur, left2, 7, 5)
                neighbor_down2 = (
                    cur_row + 2 < grid_rows and can_move(cur, down2, 6, 4)
                )
                neighbor_top2 = cur_row - 2 >= 0 and can_move(cur, top2, 4, 6)

                moves = [
                    (diff_col > 1 and neighbor_right2, 5, 0, 2),
                    (diff_col > 0 and neighbor_right, 1, 0, 1),
                    (diff_col < -1 and neighbor_left2, 7, 0, -2),
                    (diff_col < 0 and neighbor_left, 3, 0, -1),
                    (diff_row > 1 and neighbor_down2, 6, 2, 0),
                    (diff_row > 0 and neighbor_down, 2, 1, 0),
                    (diff_row < -1 and neighbor_top2, 4, -2, 0),
                    (diff_row < 0 and neighbor_top, 0, -1, 0),
                    (neighbor_right, 1, 0, 1),
                    (neighbor_left, 3, 0, -1),
                    (neighbor_down, 2, 1, 0),
                    (neighbor_top, 0, -1, 0),
                    (neighbor_right2, 5, 0, 2),
                    (neighbor_left2, 7, 0, -2),
                    (neighbor_down2, 6, 2, 0),
                    (neighbor_top2, 4, -2, 0),
                ]
            else:
                moves = [
                    (diff_col > 0 and neighbor_right, 1, 0, 1),
                    (diff_col < 0 and neighbor_left, 3, 0, -1),
                    (diff_row > 0 and neighbor_down, 2, 1, 0),
                    (diff_row < 0 and neighbor_top, 0, -1, 0),
                    (neighbor_right, 1, 0, 1),
                    (neighbor_left, 3, 0, -1),
                    (neighbor_down, 2, 1, 0),
                    (neighbor_top, 0, -1, 0),
                ]

            for allowed, direction, delta_row, delta_col in moves:
                if allowed:
                    commit(cur, direction, delta_row, delta_col)
                    changed = True
                    break

            if not changed:
                success = False
                break

        if not success:
            return {
                "cpu_route_success": False,
                "cpu_route_max_cost": None,
                "cpu_route_max_paper_fifo": None,
                "cpu_route_edge_steps": {},
            }
        routed_steps.append(step)
        edge_steps[(src, dst)] = step

    max_cost = max(routed_steps) if routed_steps else 0
    return {
        "cpu_route_success": True,
        "cpu_route_max_cost": max_cost,
        "cpu_route_max_paper_fifo": max(0, max_cost - 1),
        "cpu_route_edge_steps": edge_steps,
    }


def summarize(rows: list[dict[str, object]], output_path: Path) -> None:
    by_candidate: dict[str, list[dict[str, object]]] = defaultdict(list)
    for row in rows:
        by_candidate[str(row["candidate"])].append(row)

    summary_lines = [
        "# YOTT VPR-BB FIFO Hypotheses",
        "",
        "| candidate | metric | MAE | RMSE | exact | under | over |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    metric_names = [
        "placement_max_cost",
        "placement_max_paper_fifo",
        "placement_max_fifo",
        "vpr_net_max_cost",
        "vpr_net_max_paper_fifo",
        "fanout_branch_max_cost",
        "fanout_branch_max_paper_fifo",
        "cpu_route_max_cost",
        "cpu_route_max_paper_fifo",
        "cpu_route_order_max_cost",
        "cpu_route_order_max_paper_fifo",
        "arrival_cost_fifo",
        "arrival_paper_fifo",
        "arrival_mesh_fifo",
        "arrival_op1_cost_fifo",
        "arrival_op1_paper_fifo",
        "arrival_op1_paper_fifo_no_graph_io",
        "arrival_route_cost_fifo",
        "arrival_route_paper_fifo",
        "aligned_placement_paper_fifo",
        "aligned_route_paper_fifo",
    ]
    for candidate in sorted(by_candidate):
        bucket = by_candidate[candidate]
        for metric in metric_names:
            diffs = [
                float(row[metric]) - float(row["paper_fifo"])
                for row in bucket
                if row.get(metric) not in ("", None)
            ]
            if not diffs:
                continue
            mae = sum(abs(value) for value in diffs) / len(diffs)
            rmse = math.sqrt(sum(value * value for value in diffs) / len(diffs))
            exact = sum(1 for value in diffs if abs(value) < 1e-9)
            under = sum(1 for value in diffs if value < -1e-9)
            over = sum(1 for value in diffs if value > 1e-9)
            summary_lines.append(
                f"| {candidate} | {metric} | {mae:.2f} | {rmse:.2f} | {exact} | {under} | {over} |"
            )
    output_path.with_suffix(".md").write_text("\n".join(summary_lines) + "\n")


def plot_selected_hypotheses(rows: list[dict[str, object]], output_path: Path) -> None:
    import matplotlib.pyplot as plt

    selected = [
        ("strict_no_corners, mapper=vpr_bb, inner=5, seed=1", "placement_max_cost", "strict inner=5, max cost"),
        ("strict_no_corners, mapper=vpr_bb, inner=5, seed=1", "cpu_route_max_cost", "strict inner=5, routed cost"),
        ("strict_no_corners, mapper=vpr_bb, inner=5, seed=1", "arrival_paper_fifo", "strict inner=5, arrival FIFO"),
        ("fit_io_no_corners, mapper=vpr_bb_inner5_seed10, inner=5, seed=10", "placement_max_cost", "fit-I/O inner=5 seed=10"),
    ]
    by_benchmark = {}
    for row in rows:
        by_benchmark.setdefault(str(row["benchmark"]), {})[
            (str(row["candidate"]), "paper_fifo")
        ] = float(row["paper_fifo"])
        for _, metric, _ in selected:
            if row.get(metric) not in ("", None):
                by_benchmark[str(row["benchmark"])][
                    (str(row["candidate"]), metric)
                ] = float(row[metric])

    benchmarks = list(dict.fromkeys(str(row["benchmark"]) for row in rows))
    x = list(range(len(benchmarks)))
    fig, ax = plt.subplots(figsize=(12.0, 4.8))
    paper_values = [
        next(
            (
                values[key]
                for key in values
                if key[1] == "paper_fifo"
            ),
            math.nan,
        )
        for values in (by_benchmark[b] for b in benchmarks)
    ]
    ax.plot(x, paper_values, color="#111111", linewidth=2.2, marker="o", markersize=3.5, label="Paper VPR-BB")
    colors = ["#4C78A8", "#F58518", "#54A24B", "#B279A2"]
    markers = ["s", "^", "D", "v"]
    for (candidate, metric, label), color, marker in zip(selected, colors, markers):
        values = [by_benchmark[b].get((candidate, metric), math.nan) for b in benchmarks]
        if all(math.isnan(value) for value in values):
            continue
        ax.plot(
            x,
            values,
            linewidth=1.5,
            marker=marker,
            markersize=3.2,
            color=color,
            label=label,
        )
    ax.set_ylabel("Maximal FIFO Size")
    ax.set_xlabel("Benchmarks")
    ax.set_xticks(x)
    ax.set_xticklabels([str(index + 1) for index in x], fontsize=8)
    ax.grid(axis="y", color="#D0D0D0", linewidth=0.6, alpha=0.8)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.legend(frameon=False, ncols=2, fontsize=8)
    fig.tight_layout()
    fig.savefig(output_path.with_name(output_path.stem + "_selected.png"), dpi=300)
    fig.savefig(output_path.with_name(output_path.stem + "_selected.svg"))
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metrics", default=DEFAULT_METRICS)
    parser.add_argument("--paper", default=DEFAULT_PAPER)
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    parser.add_argument("--repo-root", default=Path.cwd())
    parser.add_argument("--no-plot", action="store_true")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    paper_fifo = read_paper_fifo(resolve_repo_path(args.paper, repo_root))
    output_path = resolve_repo_path(args.output, repo_root)
    rows_out = []
    with resolve_repo_path(args.metrics, repo_root).open(newline="") as f:
        for row in csv.DictReader(f):
            benchmark = canonical_benchmark(row["benchmark"])
            if benchmark not in paper_fifo:
                continue
            if row.get("status") != "success":
                continue
            dfg_path = resolve_repo_path(row["raw_output_dir"], repo_root)
            # raw_output_dir points at the VPR work directory; summary keeps the DFG path.
            summary_path = resolve_repo_path(row["run_dir"], repo_root) / "summary.json"
            import json

            summary = json.loads(summary_path.read_text())
            dfg_path = resolve_repo_path(summary["dfg"], repo_root)
            nodes, edges = read_dfg_edges(dfg_path)
            positions = load_positions(row, repo_root)
            cost_model = row.get("placement_cost_model", "mesh") or "mesh"
            routed_fifo = cpu_mapping_routed_fifo(edges, positions, cost_model)
            routed_fifo_order = cpu_mapping_routed_fifo(
                edges, positions, cost_model, sort_by_cost=False
            )
            route_steps = routed_fifo.get("cpu_route_edge_steps", {})
            route_cost_arrival = None
            route_paper_arrival = None
            aligned_route_paper_fifo = None
            if route_steps:
                route_cost_arrival = longest_arrival_fifo_from_edge_delays(
                    nodes,
                    edges,
                    {edge: float(delay) for edge, delay in route_steps.items()},
                )
                route_paper_arrival = longest_arrival_fifo_from_edge_delays(
                    nodes,
                    edges,
                    {edge: float(max(0, delay - 1)) for edge, delay in route_steps.items()},
                )
                aligned_route_paper_fifo = original_aligned_fifo_from_edge_delays(
                    nodes,
                    edges,
                    {edge: float(max(0, delay - 1)) for edge, delay in route_steps.items()},
                )
            inner_num = row.get("inner_num") or row.get("vpr_inner_num", "")
            if not inner_num:
                match = re.search(r"inner(\d+(?:\.\d+)?)", row.get("mapper", ""))
                if match:
                    inner_num = match.group(1)
            io_capacity = row.get("vpr_io_capacity", "")
            capacity_suffix = f", io_capacity={io_capacity}" if io_capacity else ""
            seed = row.get("seed", "")
            seed_suffix = f", seed={seed}" if seed else ""
            benchmark_set = row.get("benchmark_set", "")
            set_prefix = f"{benchmark_set}, " if benchmark_set else ""
            candidate = (
                f"{set_prefix}{row.get('arch_policy') or row['arch_name']}, "
                f"mapper={row.get('mapper', '')}, inner={inner_num}{capacity_suffix}{seed_suffix}"
            )
            rows_out.append(
                {
                    "benchmark": benchmark,
                    "candidate": candidate,
                    "paper_fifo": paper_fifo[benchmark],
                    "placement_max_cost": as_float(row, "placement_max_cost"),
                    "placement_max_paper_fifo": as_float(row, "placement_max_paper_fifo"),
                    "placement_max_fifo": as_float(row, "placement_max_fifo"),
                    "vpr_net_max_cost": fanout_net_max_cost(
                        edges, positions, cost_model, shifted=False
                    ),
                    "vpr_net_max_paper_fifo": fanout_net_max_cost(
                        edges, positions, cost_model, shifted=True
                    ),
                    "fanout_branch_max_cost": fanout_branch_max_cost(
                        edges, positions, cost_model, shifted=False
                    ),
                    "fanout_branch_max_paper_fifo": fanout_branch_max_cost(
                        edges, positions, cost_model, shifted=True
                    ),
                    "cpu_route_success": routed_fifo["cpu_route_success"],
                    "cpu_route_max_cost": routed_fifo["cpu_route_max_cost"],
                    "cpu_route_max_paper_fifo": routed_fifo["cpu_route_max_paper_fifo"],
                    "cpu_route_order_success": routed_fifo_order["cpu_route_success"],
                    "cpu_route_order_max_cost": routed_fifo_order["cpu_route_max_cost"],
                    "cpu_route_order_max_paper_fifo": routed_fifo_order["cpu_route_max_paper_fifo"],
                    "arrival_cost_fifo": longest_arrival_fifo(
                        nodes, edges, positions, cost_model, "cost"
                    ),
                    "arrival_paper_fifo": longest_arrival_fifo(
                        nodes, edges, positions, cost_model, "paper_fifo"
                    ),
                    "arrival_mesh_fifo": longest_arrival_fifo(
                        nodes, edges, positions, cost_model, "mesh_fifo"
                    ),
                    "arrival_op1_cost_fifo": scheduled_arrival_fifo(
                        nodes, edges, positions, cost_model, "cost", op_latency=1.0
                    ),
                    "arrival_op1_paper_fifo": scheduled_arrival_fifo(
                        nodes, edges, positions, cost_model, "paper_fifo", op_latency=1.0
                    ),
                    "arrival_op1_paper_fifo_no_graph_io": scheduled_arrival_fifo(
                        nodes,
                        edges,
                        positions,
                        cost_model,
                        "paper_fifo",
                        op_latency=1.0,
                        exclude_graph_io_edges=True,
                    ),
                    "arrival_route_cost_fifo": route_cost_arrival,
                    "arrival_route_paper_fifo": route_paper_arrival,
                    "aligned_placement_paper_fifo": original_aligned_fifo_from_edge_delays(
                        nodes,
                        edges,
                        {
                            edge: float(
                                max(
                                    0,
                                    placement_cost(
                                        abs(positions[edge[0]][1] - positions[edge[1]][1]),
                                        abs(positions[edge[0]][0] - positions[edge[1]][0]),
                                        cost_model,
                                    )
                                    - 1,
                                )
                            )
                            for edge in edges
                            if edge[0] in positions and edge[1] in positions
                        },
                    ),
                    "aligned_route_paper_fifo": aligned_route_paper_fifo,
                }
            )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "benchmark",
        "candidate",
        "paper_fifo",
        "placement_max_cost",
        "placement_max_paper_fifo",
        "placement_max_fifo",
        "vpr_net_max_cost",
        "vpr_net_max_paper_fifo",
        "fanout_branch_max_cost",
        "fanout_branch_max_paper_fifo",
        "cpu_route_success",
        "cpu_route_max_cost",
        "cpu_route_max_paper_fifo",
        "cpu_route_order_success",
        "cpu_route_order_max_cost",
        "cpu_route_order_max_paper_fifo",
        "arrival_cost_fifo",
        "arrival_paper_fifo",
        "arrival_mesh_fifo",
        "arrival_op1_cost_fifo",
        "arrival_op1_paper_fifo",
        "arrival_op1_paper_fifo_no_graph_io",
        "arrival_route_cost_fifo",
        "arrival_route_paper_fifo",
        "aligned_placement_paper_fifo",
        "aligned_route_paper_fifo",
    ]
    with output_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows_out)
    summarize(rows_out, output_path)
    if not args.no_plot:
        plot_selected_hypotheses(rows_out, output_path)
    print(output_path)
    print(output_path.with_suffix(".md"))


if __name__ == "__main__":
    main()
