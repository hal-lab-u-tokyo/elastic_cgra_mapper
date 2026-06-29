#!/usr/bin/env python3
"""Report the edge/net that determines each YOTT VPR-BB FIFO hypothesis."""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from lib import clean_dot_value, clean_node_name, placement_cost, resolve_repo_path  # noqa: E402
from run_vpr_baseline import (  # noqa: E402
    find_net_file,
    find_place_file,
    parse_packed_net_positions,
    parse_place_file,
)

ALIASES = {
    "Cplx8": "cplx8",
    "Fir16": "fir16",
    "FilterRGB": "filter_rgb",
}


def canonical_benchmark(name: str) -> str:
    return ALIASES.get(name, name)


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


def read_dfg_info(path: Path) -> dict:
    nodes = []
    opcodes = {}
    node_set = set()
    raw_edges = []
    opcode_pattern = re.compile(r"opcode\s*=\s*\"?([^\",\]]+)\"?")
    for raw_line in path.read_text().splitlines():
        line = raw_line.strip().rstrip(";")
        if not line or line.startswith("//") or line in {"digraph G {", "}"}:
            continue
        if "->" in line:
            src, rest = line.split("->", 1)
            dst = rest.split("[", 1)[0].strip()
            raw_edges.append((clean_node_name(src), clean_node_name(dst)))
            continue
        if "[opcode=" in line:
            node = clean_node_name(line.split("[", 1)[0])
            match = opcode_pattern.search(line)
            opcode = match.group(1).strip() if match else ""
            nodes.append(node)
            node_set.add(node)
            opcodes[node] = opcode

    predecessors = {node: [] for node in nodes}
    successors = {node: [] for node in nodes}
    edges = []
    for src, dst in raw_edges:
        if src not in node_set or dst not in node_set:
            continue
        edges.append((src, dst))
        successors[src].append(dst)
        predecessors[dst].append(src)
    return {
        "nodes": nodes,
        "edges": edges,
        "opcodes": opcodes,
        "predecessors": predecessors,
        "successors": successors,
    }


def load_positions(row: dict[str, str], repo_root: Path) -> dict[str, tuple[int, int]]:
    run_dir = resolve_repo_path(row["run_dir"], repo_root)
    work_dir = run_dir / "vpr"
    name_map_path = work_dir / "vpr_name_map.json"
    if not name_map_path.exists():
        return {}
    name_map = json.loads(name_map_path.read_text())
    reverse_map = name_map.get("reverse_map") or name_map.get("blif_to_original", {})
    blif_path = work_dir / f"{row['benchmark']}.blif"
    place_path = find_place_file(work_dir, blif_path)
    net_path = find_net_file(work_dir, blif_path)
    top_positions, _, _ = parse_place_file(place_path, reverse_map)
    return parse_packed_net_positions(net_path, top_positions, reverse_map)


def cpu_mapping_routed_fifo(
    edges: list[tuple[str, str]],
    positions: dict[str, tuple[int, int]],
    cost_model: str,
) -> dict[str, object]:
    if any(src not in positions or dst not in positions for src, dst in edges):
        return {"cpu_route_success": False, "cpu_route_edge_steps": {}}

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
        return placement_cost(abs(sc - dc), abs(sr - dr), cost_model)

    edge_steps: dict[tuple[str, str], int] = {}
    for src, dst in sorted(edges, key=initial_cost, reverse=True):
        src_row, src_col = positions[src]
        dst_row, dst_col = positions[dst]
        cur_row, cur_col = src_row, src_col
        count_per_curr = [0] * total_grid
        step = 0

        def can_move(cur: int, nxt: int, direction: int, opposite: int) -> bool:
            if not (0 <= nxt < total_grid):
                return False
            if count_per_curr[nxt] != 0:
                return False
            current_link = grid_route[cur][direction]
            next_link = grid_route[nxt][opposite]
            if current_link and current_link.get(step) not in (None, src):
                return False
            # Match cpu_mapping's asymmetric reverse-link check exactly.
            if step in next_link and next_link[step] == src:
                return False
            return True

        def commit(cur: int, direction: int, delta_row: int, delta_col: int) -> None:
            nonlocal cur_row, cur_col
            grid_route[cur][direction][step] = src
            cur_row += delta_row
            cur_col += delta_col

        success = True
        while cur_row != dst_row or cur_col != dst_col:
            step += 1
            cur = flat(cur_row, cur_col)
            count_per_curr[cur] += 1
            diff_row = dst_row - cur_row
            diff_col = dst_col - cur_col
            right = cur + 1
            left = cur - 1
            down = cur + grid_cols
            top = cur - grid_cols
            neighbor_right = cur_col + 1 < grid_cols and can_move(cur, right, 1, 3)
            neighbor_left = cur_col - 1 >= 0 and can_move(cur, left, 3, 1)
            neighbor_down = cur_row + 1 < grid_rows and can_move(cur, down, 2, 0)
            neighbor_top = cur_row - 1 >= 0 and can_move(cur, top, 0, 2)
            if is_one_hop:
                right2 = cur + 2
                left2 = cur - 2
                down2 = cur + 2 * grid_cols
                top2 = cur - 2 * grid_cols
                moves = [
                    (diff_col > 1 and cur_col + 2 < grid_cols and can_move(cur, right2, 5, 7), 5, 0, 2),
                    (diff_col > 0 and neighbor_right, 1, 0, 1),
                    (diff_col < -1 and cur_col - 2 >= 0 and can_move(cur, left2, 7, 5), 7, 0, -2),
                    (diff_col < 0 and neighbor_left, 3, 0, -1),
                    (diff_row > 1 and cur_row + 2 < grid_rows and can_move(cur, down2, 6, 4), 6, 2, 0),
                    (diff_row > 0 and neighbor_down, 2, 1, 0),
                    (diff_row < -1 and cur_row - 2 >= 0 and can_move(cur, top2, 4, 6), 4, -2, 0),
                    (diff_row < 0 and neighbor_top, 0, -1, 0),
                    (neighbor_right, 1, 0, 1),
                    (neighbor_left, 3, 0, -1),
                    (neighbor_down, 2, 1, 0),
                    (neighbor_top, 0, -1, 0),
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
                    break
            else:
                success = False
                break
        if not success:
            return {"cpu_route_success": False, "cpu_route_edge_steps": {}}
        edge_steps[(src, dst)] = step
    return {"cpu_route_success": True, "cpu_route_edge_steps": edge_steps}


def edge_payload(
    metric: str,
    value: float,
    src: str,
    dst: str,
    positions: dict[str, tuple[int, int]],
    info: dict,
    cost_model: str,
    paper_fifo: float,
) -> dict:
    src_pos = positions.get(src, ("", ""))
    dst_pos = positions.get(dst, ("", ""))
    if src in positions and dst in positions:
        src_row, src_col = positions[src]
        dst_row, dst_col = positions[dst]
        dx = abs(src_col - dst_col)
        dy = abs(src_row - dst_row)
        mesh_hop = dx + dy
        cost = placement_cost(dx, dy, cost_model)
    else:
        dx = dy = mesh_hop = cost = ""
    predecessors = info["predecessors"]
    successors = info["successors"]
    return {
        "metric": metric,
        "paper_fifo": paper_fifo,
        "local_value": value,
        "diff": value - paper_fifo,
        "src": src,
        "dst": dst,
        "src_opcode": info["opcodes"].get(src, ""),
        "dst_opcode": info["opcodes"].get(dst, ""),
        "src_is_input": int(src in predecessors and not predecessors[src]),
        "dst_is_output": int(dst in successors and not successors[dst]),
        "src_row": src_pos[0],
        "src_col": src_pos[1],
        "dst_row": dst_pos[0],
        "dst_col": dst_pos[1],
        "dx": dx,
        "dy": dy,
        "mesh_hop": mesh_hop,
        "placement_cost": cost,
        "fanout_size": "",
        "fanout_sinks": "",
    }


def best_edges_for_metric(
    metric: str,
    edges: list[tuple[str, str]],
    positions: dict[str, tuple[int, int]],
    cost_model: str,
    route_steps: dict[tuple[str, str], int],
) -> list[tuple[float, str, str]]:
    values = []
    for src, dst in edges:
        if src not in positions or dst not in positions:
            continue
        src_row, src_col = positions[src]
        dst_row, dst_col = positions[dst]
        dx = abs(src_col - dst_col)
        dy = abs(src_row - dst_row)
        cost = placement_cost(dx, dy, cost_model)
        mesh_hop = dx + dy
        if metric == "placement_max_cost":
            value = cost
        elif metric == "placement_max_paper_fifo":
            value = max(0, cost - 1)
        elif metric == "placement_max_fifo":
            value = max(0, mesh_hop - 1)
        elif metric == "cpu_route_max_cost":
            if (src, dst) not in route_steps:
                continue
            value = route_steps[(src, dst)]
        elif metric == "cpu_route_max_paper_fifo":
            if (src, dst) not in route_steps:
                continue
            value = max(0, route_steps[(src, dst)] - 1)
        else:
            raise ValueError(f"unsupported metric: {metric}")
        values.append((float(value), src, dst))
    if not values:
        return []
    max_value = max(value for value, _, _ in values)
    return [item for item in values if item[0] == max_value]


def best_nets_for_metric(
    metric: str,
    edges: list[tuple[str, str]],
    positions: dict[str, tuple[int, int]],
    cost_model: str,
) -> list[dict]:
    fanouts: dict[str, list[str]] = defaultdict(list)
    for src, dst in edges:
        fanouts[src].append(dst)

    values = []
    for src, sinks in fanouts.items():
        net_nodes = [src] + sinks
        if any(node not in positions for node in net_nodes):
            continue
        rows = [positions[node][0] for node in net_nodes]
        cols = [positions[node][1] for node in net_nodes]
        dx = max(cols) - min(cols)
        dy = max(rows) - min(rows)
        cost = placement_cost(dx, dy, cost_model)
        value = cost if metric == "vpr_net_max_cost" else max(0, cost - 1)
        values.append(
            {
                "value": float(value),
                "src": src,
                "sinks": sinks,
                "dx": dx,
                "dy": dy,
                "mesh_hop": dx + dy,
                "placement_cost": cost,
            }
        )
    if not values:
        return []
    max_value = max(item["value"] for item in values)
    return [item for item in values if item["value"] == max_value]


def infer_inner(row: dict[str, str]) -> str:
    inner_num = row.get("inner_num") or row.get("vpr_inner_num", "")
    if inner_num:
        return inner_num
    match = re.search(r"inner(\d+(?:\.\d+)?)", row.get("mapper", ""))
    return match.group(1) if match else ""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metrics", required=True)
    parser.add_argument("--paper", default="research/data/papers/yott_figure_paper_metrics.csv")
    parser.add_argument("--output", required=True)
    parser.add_argument("--repo-root", default=Path.cwd())
    parser.add_argument("--only-mapper", action="append", default=[])
    parser.add_argument(
        "--metrics-to-report",
        nargs="*",
        default=[
            "placement_max_cost",
            "placement_max_paper_fifo",
            "placement_max_fifo",
            "vpr_net_max_cost",
            "vpr_net_max_paper_fifo",
            "cpu_route_max_cost",
            "cpu_route_max_paper_fifo",
        ],
    )
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    paper_fifo = read_paper_fifo(resolve_repo_path(args.paper, repo_root))
    rows_out = []
    with resolve_repo_path(args.metrics, repo_root).open(newline="") as f:
        for row in csv.DictReader(f):
            if row.get("status") != "success":
                continue
            if args.only_mapper and row.get("mapper") not in set(args.only_mapper):
                continue
            benchmark = canonical_benchmark(row["benchmark"])
            if benchmark not in paper_fifo:
                continue
            summary_path = resolve_repo_path(row["run_dir"], repo_root) / "summary.json"
            summary = json.loads(summary_path.read_text())
            dfg_path = resolve_repo_path(summary["dfg"], repo_root)
            info = read_dfg_info(dfg_path)
            positions = load_positions(row, repo_root)
            cost_model = row.get("placement_cost_model", "mesh") or "mesh"
            routed = cpu_mapping_routed_fifo(info["edges"], positions, cost_model)
            route_steps = routed.get("cpu_route_edge_steps", {})

            for metric in args.metrics_to_report:
                if metric.startswith("vpr_net_"):
                    winners = best_nets_for_metric(
                        metric, info["edges"], positions, cost_model
                    )
                    for winner in winners:
                        payload = edge_payload(
                            metric,
                            winner["value"],
                            winner["src"],
                            winner["sinks"][0],
                            positions,
                            info,
                            cost_model,
                            paper_fifo[benchmark],
                        )
                        payload.update(
                            {
                                "fanout_size": len(winner["sinks"]),
                                "fanout_sinks": ";".join(winner["sinks"]),
                                "dx": winner["dx"],
                                "dy": winner["dy"],
                                "mesh_hop": winner["mesh_hop"],
                                "placement_cost": winner["placement_cost"],
                            }
                        )
                        rows_out.append(payload | {
                            "benchmark": benchmark,
                            "mapper": row["mapper"],
                            "inner": infer_inner(row),
                            "seed": row.get("seed", ""),
                        })
                    continue

                winners = best_edges_for_metric(
                    metric, info["edges"], positions, cost_model, route_steps
                )
                for value, src, dst in winners:
                    payload = edge_payload(
                        metric,
                        value,
                        src,
                        dst,
                        positions,
                        info,
                        cost_model,
                        paper_fifo[benchmark],
                    )
                    rows_out.append(payload | {
                        "benchmark": benchmark,
                        "mapper": row["mapper"],
                        "inner": infer_inner(row),
                        "seed": row.get("seed", ""),
                    })

    output_path = resolve_repo_path(args.output, repo_root)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "benchmark",
        "mapper",
        "inner",
        "seed",
        "metric",
        "paper_fifo",
        "local_value",
        "diff",
        "src",
        "dst",
        "src_opcode",
        "dst_opcode",
        "src_is_input",
        "dst_is_output",
        "src_row",
        "src_col",
        "dst_row",
        "dst_col",
        "dx",
        "dy",
        "mesh_hop",
        "placement_cost",
        "fanout_size",
        "fanout_sinks",
    ]
    with output_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows_out)
    print(output_path)


if __name__ == "__main__":
    main()
