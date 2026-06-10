import sys
import os
sys.path.append(os.pardir)
import math
import re
import matplotlib.pyplot as plt
import matplotlib.patches as pat
import networkx as nx
import pydot
from matplotlib.backends.backend_pdf import PdfPages

from entity import *

# reference: GenMap ConfDrawer.py
# https://github.com/hal-lab-u-tokyo/GenMap/blob/master/ConfDrawer.py

# drawing setting
contexts_per_page = 4
page_column_num = 2
pe_margin = 0.06
pe_size = 1 - pe_margin * 2
arrow_setting = dict(arrowstyle="-|>", color="#555555", lw=0.55,
                     mutation_scale=7, shrinkA=9, shrinkB=9, alpha=0.45)

operation_colors = {
    "compute": "#b9d9ff",
    "memory": "#c8ecd0",
    "const": "#fff1a8",
    "route": "#e9e9e9",
    "control": "#f7c8b8",
    "address": "#d8ccff",
    "unknown": "#f0f0f0",
    "nop": "#ffffff",
}

dfg_legend_items = [
    ("compute", "ALU ops"),
    ("memory", "load/store"),
    ("const", "constant"),
    ("control", "control"),
    ("address", "address"),
    ("unknown", "unknown"),
]

mapping_legend_items = [
    ("compute", "compute"),
    ("memory", "memory"),
    ("const", "const"),
    ("control", "control"),
    ("route", "route"),
]

operation_labels = {
    OperationType.Add: "add",
    OperationType.FAdd: "fadd",
    OperationType.Sub: "sub",
    OperationType.Mul: "mul",
    OperationType.FMul: "fmul",
    OperationType.Div: "div",
    OperationType.Const: "const",
    OperationType.Load: "load",
    OperationType.Output: "out",
    OperationType.Store: "store",
    OperationType.Route: "rt",
    OperationType.Or: "or",
    OperationType.Shift: "shift",
    OperationType.icmp: "icmp",
    OperationType.Loop: "loop",
    OperationType.Select: "sel",
    OperationType.Cmpgt: "cmpgt",
    OperationType.Cmpge: "cmpge",
    OperationType.Cmpeq: "cmpeq",
}


class Visualizer():
    @staticmethod
    def visualize_mapping(mapping: Mapping, output_dir: str):
        os.makedirs(output_dir, exist_ok=True)
        output_path = os.path.join(output_dir, "result.pdf")

        with PdfPages(output_path) as pdf:
            for page_start in range(0, mapping.context_size, contexts_per_page):
                page_context_ids = list(range(
                    page_start, min(page_start + contexts_per_page, mapping.context_size)))
                subplot_column_num = min(page_column_num, len(page_context_ids))
                subplot_row_num = math.ceil(len(page_context_ids) / subplot_column_num)

                context_width = max(4.2, mapping.column_num * 0.75)
                context_height = max(3.8, mapping.row_num * 0.75)
                fig, axes = plt.subplots(
                    subplot_row_num,
                    subplot_column_num,
                    figsize=(context_width * subplot_column_num,
                             context_height * subplot_row_num),
                    squeeze=False)

                fig.suptitle(
                    "Mapping visualization "
                    f"({mapping.row_num}x{mapping.column_num}, "
                    f"context {page_context_ids[0]}-{page_context_ids[-1]})",
                    fontsize=12)

                flat_axes = axes.flatten()
                for ax, context_id in zip(flat_axes, page_context_ids):
                    Visualizer.visualize_mapping_one_context(mapping, context_id, ax)

                for ax in flat_axes[len(page_context_ids):]:
                    ax.set_axis_off()

                Visualizer.__add_legend(fig)
                fig.tight_layout(rect=[0.0, 0.03, 1.0, 0.95])
                pdf.savefig(fig)
                plt.close(fig)

    @staticmethod
    def visualize_dfg(dot_file_path: str, output_dir: str):
        os.makedirs(output_dir, exist_ok=True)
        graph = Visualizer.__read_dfg_dot(dot_file_path)
        output_path = os.path.join(
            output_dir, os.path.splitext(os.path.basename(dot_file_path))[0] + "_dfg.pdf")
        svg_output_path = os.path.splitext(output_path)[0] + ".svg"

        try:
            Visualizer.__write_graphviz_dfg(
                graph, dot_file_path, output_path, svg_output_path)
        except Exception:
            Visualizer.__write_matplotlib_dfg(
                graph, dot_file_path, output_path, svg_output_path)

        return output_path

    @staticmethod
    def __write_matplotlib_dfg(graph, dot_file_path, output_path, svg_output_path=None):
        pos = Visualizer.__layered_dfg_layout(graph)
        feedback_edges = Visualizer.__dfg_feedback_edge_keys(graph)
        figsize = Visualizer.__layered_dfg_figure_size(pos)
        fig, ax = plt.subplots(figsize=figsize)
        ax.set_axis_off()
        ax.set_aspect("equal")
        ax.set_title(
            f"DFG: {os.path.basename(dot_file_path)} "
            f"({graph.number_of_nodes()} nodes, {graph.number_of_edges()} edges)",
            fontsize=12, pad=12)

        Visualizer.__draw_layered_dfg_edges(graph, pos, feedback_edges, ax)
        Visualizer.__draw_layered_dfg_nodes(graph, pos, ax)
        Visualizer.__add_dfg_legend(fig)
        Visualizer.__set_layered_dfg_limits(pos, ax)

        fig.tight_layout(rect=[0.0, 0.05, 1.0, 0.95])
        fig.savefig(output_path, bbox_inches="tight")
        if svg_output_path:
            fig.savefig(svg_output_path, bbox_inches="tight")
        plt.close(fig)

    @staticmethod
    def visualize_mapping_one_context(mapping: Mapping, context_id: int, ax):
        ax.set_xbound(0, mapping.column_num)
        ax.set_ybound(0, mapping.row_num)
        ax.set_aspect("equal")
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_title(f"context {context_id}", fontsize=10, pad=6)
        ax.set_facecolor("#fbfbfb")
        for spine in ax.spines.values():
            spine.set_visible(False)

        row_num = mapping.row_num
        column_num = mapping.column_num

        def create_xy_from_row_id_and_column_id(row_id, column_id, row_num):
            return (column_id, row_num - 1 - row_id)

        PE_id_to_patch = {}
        active_PE_num = 0

        for row_id in range(row_num):
            for column_id in range(column_num):
                tmp_PE_config = mapping.PE_array[row_id][column_id].config_list[context_id]

                x, y = create_xy_from_row_id_and_column_id(row_id, column_id, row_num)

                # add PE and opcode
                PE_operation_type = tmp_PE_config.operation_type
                color = Visualizer.__operation_color(PE_operation_type)
                pe = Visualizer.__make_PE_patch((x, y), color)
                ax.add_patch(pe)

                if PE_operation_type != OperationType.Nop:
                    active_PE_num += 1
                    label = Visualizer.__make_operation_label(
                        PE_operation_type, tmp_PE_config.operation_name)
                    ax.text(x + 0.5, y + 0.5, label,
                            ha="center", va="center",
                            fontsize=Visualizer.__label_font_size(row_num, column_num),
                            linespacing=0.9, clip_on=True)

                PE_id_to_patch[(row_id, column_id)] = pe

        for row_id in range(row_num):
            for column_id in range(column_num):
                tmp_PE_patch = PE_id_to_patch[(row_id, column_id)]
                tmp_PE_config = mapping.PE_array[row_id][column_id].config_list[context_id]

                for from_config_id in tmp_PE_config.from_config_id:
                    from_PE_id = (from_config_id.row_id, from_config_id.column_id)
                    if from_PE_id == (row_id, column_id):
                        continue
                    from_PE_patch = PE_id_to_patch[from_PE_id]
                    ax.annotate("", xy=Visualizer.__get_center(tmp_PE_patch),
                                xytext=Visualizer.__get_center(from_PE_patch),
                                arrowprops=arrow_setting)

        utilization = active_PE_num / (row_num * column_num)
        ax.set_title(
            f"context {context_id} | active {active_PE_num}/{row_num * column_num} ({utilization:.0%})",
            fontsize=9, pad=6)

    @staticmethod
    def __make_PE_patch(coord, color):
        """Makes a square for PE
            Args:
                coord (tuple): coordinate of the PE
                color (str): color of the PE
            Returns:
                patch of matplotlib: a square
        """
        x, y = coord
        return pat.Rectangle(xy=(x + pe_margin, y + pe_margin),
                             width=pe_size, height=pe_size,
                             angle=0, facecolor=color, edgecolor="#333333",
                             linewidth=0.6)

    @staticmethod
    def __get_center(patch):
        """Calculates center coordinate of patch
        """
        if isinstance(patch, pat.Rectangle):
            width = patch.get_width()
            height = patch.get_height()
            x = patch.get_x()
            y = patch.get_y()
            return (x + width / 2, y + height / 2)
        elif isinstance(patch, pat.RegularPolygon):
            return patch.xy
        else:
            xy = patch.get_xy()
            x_list = [x for x, y in xy]
            y_list = [y for x, y in xy]
            min_x = min(x_list)
            max_x = max(x_list)
            min_y = min(y_list)
            max_y = max(y_list)
            return (min_x + (max_x - min_x) / 2, min_y + (max_y - min_y) / 2)

    @staticmethod
    def __operation_color(operation_type):
        if operation_type == OperationType.Nop:
            return operation_colors["nop"]
        if operation_type == OperationType.Route:
            return operation_colors["route"]
        if operation_type == OperationType.Const:
            return operation_colors["const"]
        if operation_type.is_memory_access_op():
            return operation_colors["memory"]
        if operation_type in [OperationType.Or, OperationType.Shift, OperationType.icmp,
                              OperationType.Loop, OperationType.Select,
                              OperationType.Cmpgt, OperationType.Cmpge, OperationType.Cmpeq]:
            return operation_colors["control"]
        return operation_colors["compute"]

    @staticmethod
    def __operation_label(operation_type):
        return operation_labels.get(operation_type, operation_type.name.lower())

    @staticmethod
    def __make_operation_label(operation_type, operation_name):
        op_label = Visualizer.__operation_label(operation_type)
        short_name = Visualizer.__shorten_operation_name(op_label, operation_name)
        if not short_name:
            return op_label
        return f"{op_label}\n{short_name}"

    @staticmethod
    def __shorten_operation_name(op_label, operation_name):
        if not operation_name or operation_name in ["nop", "route"]:
            return ""

        name = operation_name
        for prefix in [f"{op_label}_", "output_", "load_", "store_", "const_", "route_"]:
            if name.startswith(prefix):
                name = name[len(prefix):]
                break

        if name == op_label or name == operation_name:
            return ""
        if name.startswith("Node0x"):
            return "N" + name[-4:]
        if len(name) > 8:
            return name[:3] + "..." + name[-3:]
        return name

    @staticmethod
    def __label_font_size(row_num, column_num):
        longest_side = max(row_num, column_num)
        if longest_side <= 4:
            return 8
        if longest_side <= 6:
            return 7
        return 6

    @staticmethod
    def __add_legend(fig):
        legend_items = [
            pat.Patch(facecolor=operation_colors[category], edgecolor="#333333", label=label)
            for category, label in mapping_legend_items
        ]
        legend = fig.legend(handles=legend_items, loc="lower center",
                            ncol=len(legend_items), frameon=False, fontsize=8,
                            borderpad=0.45, handlelength=1.4,
                            columnspacing=1.1, handletextpad=0.45)

    @staticmethod
    def __write_graphviz_dfg(graph, dot_file_path, pdf_output_path, svg_output_path):
        gv_output_path = os.path.splitext(pdf_output_path)[0] + ".gv"
        basename = os.path.basename(dot_file_path)
        title = f"DFG: {basename} ({graph.number_of_nodes()} nodes, {graph.number_of_edges()} edges)"
        feedback_edges = Visualizer.__dfg_feedback_edge_keys(graph)

        dot = pydot.Dot(
            graph_name="DFG",
            graph_type="digraph",
            strict=False,
            rankdir="TB",
            splines="spline",
            overlap="false",
            concentrate="false",
            outputorder="edgesfirst",
            bgcolor="white",
            pad="0.12",
            margin="0.04",
            nodesep="0.55",
            ranksep="0.85",
            newrank="true",
            label=title,
            labelloc="t",
            fontname="Helvetica",
            fontsize="18",
        )
        dot.set_node_defaults(
            shape="box",
            style='"rounded,filled"',
            fontname="Helvetica",
            fontsize="11",
            color="#333333",
            penwidth="1.0",
            margin="0.07",
            fixedsize="false",
        )
        dot.set_edge_defaults(
            color="#5a5a5a",
            arrowsize="0.65",
            penwidth="1.0",
            fontname="Helvetica",
            fontsize="8",
            fontcolor="#666666",
        )

        for node, attrs in graph.nodes(data=True):
            opcode = attrs.get("opcode", "unknown")
            category = attrs.get("category", "unknown")
            dot.add_node(pydot.Node(
                Visualizer.__quote_dot_id(node),
                label=Visualizer.__graphviz_node_label(node, opcode),
                fillcolor=operation_colors.get(category, operation_colors["unknown"]),
            ))

        show_edge_labels = graph.number_of_edges() <= 28
        for src, dst, key, attrs in graph.edges(keys=True, data=True):
            edge_kwargs = {}
            label = Visualizer.__make_dfg_edge_label(attrs)
            if show_edge_labels and label:
                edge_kwargs["xlabel"] = label

            if (src, dst, key) in feedback_edges:
                edge_kwargs.update({
                    "constraint": "false",
                    "style": "dashed",
                    "color": "#8a8a8a",
                    "penwidth": "0.9",
                })

            dot.add_edge(pydot.Edge(
                Visualizer.__quote_dot_id(src),
                Visualizer.__quote_dot_id(dst),
                **edge_kwargs))

        Visualizer.__add_graphviz_dfg_legend(dot, graph, bool(feedback_edges))
        dot.write_raw(gv_output_path)
        dot.write_pdf(pdf_output_path)
        dot.write_svg(svg_output_path)

    @staticmethod
    def __dfg_feedback_edge_keys(graph):
        layout_graph = nx.DiGraph()
        for node, attrs in graph.nodes(data=True):
            layout_graph.add_node(node, **attrs)

        feedback_edges = set()
        for src, dst, key in graph.edges(keys=True):
            if src == dst or nx.has_path(layout_graph, dst, src):
                feedback_edges.add((src, dst, key))
                continue
            layout_graph.add_edge(src, dst)

        return feedback_edges

    @staticmethod
    def __graphviz_node_label(node_name, opcode):
        short_name = Visualizer.__shorten_dfg_node_name(node_name, opcode)
        if not short_name:
            return opcode
        return f"{opcode}\\n{short_name}"

    @staticmethod
    def __add_graphviz_dfg_legend(dot, graph, has_feedback_edges):
        legend_label = Visualizer.__graphviz_legend_label(graph, has_feedback_edges)
        anchor_nodes = Visualizer.__dfg_layout_sink_nodes(graph)
        legend_rank = pydot.Subgraph(graph_name="legend_rank", rank="sink")
        for node in anchor_nodes:
            legend_rank.add_node(pydot.Node(Visualizer.__quote_dot_id(node)))
        legend_rank.add_node(pydot.Node(
            "legend_spacer",
            label="",
            shape="point",
            style="invis",
            width="0.8",
            height="0.01",
        ))
        legend_rank.add_node(pydot.Node(
            "legend_table",
            label=legend_label,
            shape="plain",
            style='""',
            color="transparent",
            fillcolor="transparent",
            margin="0",
        ))
        dot.add_subgraph(legend_rank)

        if anchor_nodes:
            dot.add_edge(pydot.Edge(
                Visualizer.__quote_dot_id(anchor_nodes[-1]),
                "legend_spacer",
                style="invis",
                weight="30",
            ))
            dot.add_edge(pydot.Edge(
                "legend_spacer",
                "legend_table",
                style="invis",
                weight="30",
            ))

    @staticmethod
    def __graphviz_legend_label(graph, has_feedback_edges):
        present_categories = {
            attrs.get("category", "unknown")
            for _, attrs in graph.nodes(data=True)
        }
        items = [
            (category, label)
            for category, label in dfg_legend_items
            if category in present_categories
        ]

        rows = []
        for category, description in items:
            color = operation_colors[category]
            rows.append(
                '<TR>'
                f'<TD WIDTH="30" HEIGHT="14" BORDER="0" FIXEDSIZE="TRUE" BGCOLOR="{color}"></TD>'
                f'<TD BORDER="0" ALIGN="LEFT"><FONT POINT-SIZE="10">{description}</FONT></TD>'
                '</TR>')
        if has_feedback_edges:
            rows.append(
                '<TR>'
                '<TD BORDER="0" ALIGN="CENTER"><FONT POINT-SIZE="10" COLOR="#8a8a8a">- -</FONT></TD>'
                '<TD BORDER="0" ALIGN="LEFT"><FONT POINT-SIZE="10">feedback edge</FONT></TD>'
                '</TR>')
        return '<' + (
            '<TABLE BORDER="0" CELLBORDER="0" CELLSPACING="2" CELLPADDING="1">'
            + "".join(rows) +
            '</TABLE>'
        ) + '>'

    @staticmethod
    def __dfg_layout_sink_nodes(graph):
        layout_graph = Visualizer.__make_acyclic_dfg_layout_graph(graph)
        sink_nodes = [
            node for node in layout_graph.nodes
            if layout_graph.out_degree(node) == 0
        ]
        return sorted(sink_nodes, key=Visualizer.__dfg_node_sort_key)

    @staticmethod
    def __quote_dot_id(value):
        return '"' + str(value).replace("\\", "\\\\").replace('"', '\\"') + '"'

    @staticmethod
    def __read_dfg_dot(dot_file_path):
        raw_graph = nx.nx_pydot.read_dot(dot_file_path)
        graph = nx.MultiDiGraph()

        for node_name, attrs in raw_graph.nodes(data=True):
            clean_name = Visualizer.__clean_dot_value(node_name)
            if Visualizer.__is_empty_dot_node(clean_name):
                continue
            opcode = Visualizer.__infer_dfg_opcode(clean_name, attrs)
            graph.add_node(clean_name, opcode=opcode,
                           category=Visualizer.__dfg_operation_category(opcode))

        for src, dst, attrs in raw_graph.edges(data=True):
            clean_src = Visualizer.__clean_dot_value(src)
            clean_dst = Visualizer.__clean_dot_value(dst)
            if Visualizer.__is_empty_dot_node(clean_src) or Visualizer.__is_empty_dot_node(clean_dst):
                continue
            if clean_src not in graph:
                graph.add_node(clean_src, opcode="unknown", category="unknown")
            if clean_dst not in graph:
                graph.add_node(clean_dst, opcode="unknown", category="unknown")
            graph.add_edge(clean_src, clean_dst, **{
                key: Visualizer.__clean_dot_value(value)
                for key, value in attrs.items()
            })

        return graph

    @staticmethod
    def __layered_dfg_layout(graph):
        dag = Visualizer.__make_acyclic_dfg_layout_graph(graph)
        if not dag.nodes:
            return {}

        ranks = {}
        for node in nx.topological_sort(dag):
            predecessor_ranks = [
                ranks[pred] + 1 for pred in dag.predecessors(node)
                if pred in ranks
            ]
            ranks[node] = max(predecessor_ranks) if predecessor_ranks else 0

        layers = {}
        for node in dag.nodes:
            layers.setdefault(ranks.get(node, 0), []).append(node)

        for layer_nodes in layers.values():
            layer_nodes.sort(key=Visualizer.__dfg_node_sort_key)

        order = Visualizer.__layer_order_map(layers)
        for _ in range(8):
            for rank in sorted(layers.keys())[1:]:
                layers[rank].sort(key=lambda node: (
                    Visualizer.__neighbor_order_average(dag.predecessors(node), order),
                    Visualizer.__dfg_node_sort_key(node),
                ))
                order = Visualizer.__layer_order_map(layers)

            for rank in sorted(layers.keys(), reverse=True)[1:]:
                layers[rank].sort(key=lambda node: (
                    Visualizer.__neighbor_order_average(dag.successors(node), order),
                    Visualizer.__dfg_node_sort_key(node),
                ))
                order = Visualizer.__layer_order_map(layers)

        x_gap = 2.65
        y_gap = 1.72
        positions = {}
        for rank in sorted(layers.keys()):
            layer_nodes = layers[rank]
            layer_width = (len(layer_nodes) - 1) * x_gap
            for index, node in enumerate(layer_nodes):
                positions[node] = (index * x_gap - layer_width / 2, -rank * y_gap)

        return Visualizer.__relax_layered_positions(dag, positions)

    @staticmethod
    def __layer_order_map(layers):
        return {
            node: index
            for _, layer_nodes in layers.items()
            for index, node in enumerate(layer_nodes)
        }

    @staticmethod
    def __neighbor_order_average(neighbors, order):
        values = [order[neighbor] for neighbor in neighbors if neighbor in order]
        if not values:
            return 0
        return sum(values) / len(values)

    @staticmethod
    def __dfg_node_sort_key(node):
        name = Visualizer.__clean_dot_value(node)
        node_match = re.match(r"Node(\d+)", name)
        if node_match:
            return (0, int(node_match.group(1)), name)
        suffix_match = re.search(r"(\d+)$", name)
        if suffix_match:
            return (1, int(suffix_match.group(1)), name)
        return (2, 0, name)

    @staticmethod
    def __relax_layered_positions(dag, positions):
        relaxed_positions = {
            node: [float(x), float(y)]
            for node, (x, y) in positions.items()
        }
        y_by_node = {node: y for node, (_, y) in positions.items()}

        for _ in range(12):
            for node in dag.nodes:
                neighbors = list(dag.predecessors(node)) + list(dag.successors(node))
                neighbor_x = [
                    relaxed_positions[neighbor][0]
                    for neighbor in neighbors
                    if neighbor in relaxed_positions
                ]
                if not neighbor_x:
                    continue
                target_x = sum(neighbor_x) / len(neighbor_x)
                relaxed_positions[node][0] = (
                    relaxed_positions[node][0] * 0.72 + target_x * 0.28)

            Visualizer.__separate_nodes_within_layers(relaxed_positions, y_by_node)

        return {node: tuple(value) for node, value in relaxed_positions.items()}

    @staticmethod
    def __separate_nodes_within_layers(positions, y_by_node):
        min_x_distance = 1.8
        layers = {}
        for node, y in y_by_node.items():
            layers.setdefault(y, []).append(node)

        for layer_nodes in layers.values():
            layer_nodes.sort(key=lambda node: positions[node][0])
            for index in range(1, len(layer_nodes)):
                left = layer_nodes[index - 1]
                current = layer_nodes[index]
                required_x = positions[left][0] + min_x_distance
                if positions[current][0] < required_x:
                    positions[current][0] = required_x

            center = sum(positions[node][0] for node in layer_nodes) / len(layer_nodes)
            for node in layer_nodes:
                positions[node][0] -= center

    @staticmethod
    def __layered_dfg_figure_size(pos):
        if not pos:
            return (8, 5)
        xs = [x for x, _ in pos.values()]
        ys = [y for _, y in pos.values()]
        width_units = max(xs) - min(xs) + 4
        height_units = max(ys) - min(ys) + 3
        return (
            min(18, max(8, width_units * 0.72)),
            min(14, max(5, height_units * 0.72)),
        )

    @staticmethod
    def __draw_layered_dfg_edges(graph, pos, feedback_edges, ax):
        box_width, box_height = Visualizer.__layered_dfg_node_box_size(graph)
        for src, dst, key, attrs in graph.edges(keys=True, data=True):
            if src not in pos or dst not in pos:
                continue
            is_feedback = (src, dst, key) in feedback_edges
            start = Visualizer.__box_boundary_point(pos[src], pos[dst], box_width, box_height)
            end = Visualizer.__box_boundary_point(pos[dst], pos[src], box_width, box_height)
            arrow = pat.FancyArrowPatch(
                start, end,
                arrowstyle="-|>",
                mutation_scale=8,
                linewidth=0.65 if not is_feedback else 0.75,
                color="#555555" if not is_feedback else "#8a8a8a",
                linestyle="-" if not is_feedback else (0, (4, 2.4)),
                alpha=0.46 if not is_feedback else 0.75,
                connectionstyle="arc3,rad=-0.18" if is_feedback else "arc3,rad=0.04",
                zorder=1)
            ax.add_patch(arrow)

    @staticmethod
    def __draw_layered_dfg_nodes(graph, pos, ax):
        box_width, box_height = Visualizer.__layered_dfg_node_box_size(graph)
        for node, attrs in graph.nodes(data=True):
            if node not in pos:
                continue
            x, y = pos[node]
            category = attrs.get("category", "unknown")
            node_patch = pat.FancyBboxPatch(
                (x - box_width / 2, y - box_height / 2),
                box_width,
                box_height,
                boxstyle="round,pad=0.045,rounding_size=0.085",
                facecolor=operation_colors.get(category, operation_colors["unknown"]),
                edgecolor="#333333",
                linewidth=0.85,
                zorder=2)
            ax.add_patch(node_patch)
            ax.text(
                x, y,
                Visualizer.__make_dfg_node_label(node, attrs.get("opcode", "unknown")),
                ha="center",
                va="center",
                fontsize=Visualizer.__layered_dfg_font_size(graph),
                linespacing=0.88,
                color="#111111",
                zorder=3)

    @staticmethod
    def __layered_dfg_node_box_size(graph):
        if graph.number_of_nodes() <= 35:
            return (1.12, 0.58)
        if graph.number_of_nodes() <= 80:
            return (1.0, 0.52)
        return (0.9, 0.48)

    @staticmethod
    def __layered_dfg_font_size(graph):
        if graph.number_of_nodes() <= 35:
            return 7
        if graph.number_of_nodes() <= 80:
            return 6
        return 5

    @staticmethod
    def __box_boundary_point(center, target, box_width, box_height):
        cx, cy = center
        tx, ty = target
        dx = tx - cx
        dy = ty - cy
        if dx == 0 and dy == 0:
            return (cx, cy)

        scale = max(abs(dx) / (box_width / 2), abs(dy) / (box_height / 2))
        return (cx + dx / scale, cy + dy / scale)

    @staticmethod
    def __set_layered_dfg_limits(pos, ax):
        if not pos:
            return
        xs = [x for x, _ in pos.values()]
        ys = [y for _, y in pos.values()]
        ax.set_xlim(min(xs) - 2.0, max(xs) + 2.0)
        ax.set_ylim(min(ys) - 1.4, max(ys) + 1.4)

    @staticmethod
    def __dfg_layout(graph):
        try:
            layout_graph = Visualizer.__make_acyclic_dfg_layout_graph(graph)
            layout_graph.graph["graph"] = {
                "ranksep": "1.1",
                "nodesep": "0.9",
                "overlap": "false",
                "splines": "true",
            }
            return Visualizer.__spread_dfg_positions(
                nx.nx_pydot.graphviz_layout(layout_graph, prog="dot"))
        except Exception:
            simple_graph = nx.DiGraph(graph)
            if nx.is_directed_acyclic_graph(simple_graph):
                for layer, generation in enumerate(nx.topological_generations(simple_graph)):
                    for node in generation:
                        simple_graph.nodes[node]["layer"] = layer
                return nx.multipartite_layout(simple_graph, subset_key="layer", align="horizontal")
            return nx.spring_layout(simple_graph, seed=7)

    @staticmethod
    def __make_acyclic_dfg_layout_graph(graph):
        layout_graph = nx.DiGraph()
        for node, attrs in graph.nodes(data=True):
            layout_graph.add_node(node, **attrs)

        for src, dst in graph.edges():
            if src == dst:
                continue
            if nx.has_path(layout_graph, dst, src):
                continue
            layout_graph.add_edge(src, dst)

        return layout_graph

    @staticmethod
    def __spread_dfg_positions(pos):
        nodes = list(pos.keys())
        if len(nodes) <= 1:
            return pos

        min_distance = 90.0
        positions = {
            node: [float(pos[node][0]), float(pos[node][1])]
            for node in nodes
        }

        for _ in range(80):
            moved = False
            for i, node_a in enumerate(nodes):
                for node_b in nodes[i + 1:]:
                    dx = positions[node_b][0] - positions[node_a][0]
                    dy = positions[node_b][1] - positions[node_a][1]
                    distance = math.hypot(dx, dy)
                    if distance >= min_distance:
                        continue

                    if distance == 0:
                        dx = 1.0
                        dy = 0.0
                        distance = 1.0

                    shift = (min_distance - distance) / 2
                    ux = dx / distance
                    uy = dy / distance
                    positions[node_a][0] -= ux * shift
                    positions[node_a][1] -= uy * shift
                    positions[node_b][0] += ux * shift
                    positions[node_b][1] += uy * shift
                    moved = True
            if not moved:
                break

        return {node: tuple(value) for node, value in positions.items()}

    @staticmethod
    def __dfg_figure_size(graph):
        node_num = max(1, graph.number_of_nodes())
        edge_num = max(1, graph.number_of_edges())
        width = min(22, max(10, 2.8 * math.sqrt(node_num) + 0.12 * edge_num))
        height = min(18, max(7, 2.2 * math.sqrt(node_num) + 0.08 * edge_num))
        return (width, height)

    @staticmethod
    def __draw_dfg_edges(graph, pos, ax):
        nx.draw_networkx_edges(
            graph, pos, ax=ax, edge_color="#555555", width=0.8,
            alpha=0.42, arrows=True, arrowstyle="-|>", arrowsize=10,
            min_source_margin=12, min_target_margin=14,
            connectionstyle="arc3,rad=0.04")

    @staticmethod
    def __draw_dfg_nodes(graph, pos, ax):
        for category in ["compute", "memory", "const", "control", "address", "unknown"]:
            nodes = [
                node for node, attrs in graph.nodes(data=True)
                if attrs.get("category") == category
            ]
            if not nodes:
                continue
            nx.draw_networkx_nodes(
                graph, pos, nodelist=nodes, ax=ax,
                node_color=operation_colors[category], edgecolors="#333333",
                linewidths=0.8, node_size=Visualizer.__dfg_node_size(graph))

    @staticmethod
    def __draw_dfg_labels(graph, pos, ax):
        labels = {
            node: Visualizer.__make_dfg_node_label(node, attrs.get("opcode", "unknown"))
            for node, attrs in graph.nodes(data=True)
        }
        nx.draw_networkx_labels(
            graph, pos, labels=labels, ax=ax,
            font_size=Visualizer.__dfg_font_size(graph),
            font_color="#111111", verticalalignment="center")

    @staticmethod
    def __draw_dfg_edge_labels(graph, pos, ax):
        if graph.number_of_edges() > 24:
            return
        edge_labels = {}
        for src, dst, attrs in graph.edges(data=True):
            label = Visualizer.__make_dfg_edge_label(attrs)
            if label:
                edge_labels[(src, dst)] = label
        if edge_labels:
            nx.draw_networkx_edge_labels(
                nx.DiGraph(graph), pos, edge_labels=edge_labels, ax=ax,
                font_size=6, font_color="#666666", rotate=False,
                bbox=dict(alpha=0.0, edgecolor="none"))

    @staticmethod
    def __dfg_node_size(graph):
        node_num = graph.number_of_nodes()
        if node_num <= 30:
            return 620
        if node_num <= 70:
            return 520
        return 400

    @staticmethod
    def __dfg_font_size(graph):
        node_num = graph.number_of_nodes()
        if node_num <= 30:
            return 6
        if node_num <= 70:
            return 6
        return 5

    @staticmethod
    def __make_dfg_node_label(node_name, opcode):
        short_name = Visualizer.__shorten_dfg_node_name(node_name, opcode)
        if not short_name:
            return opcode
        return f"{opcode}\n{short_name}"

    @staticmethod
    def __shorten_dfg_node_name(node_name, opcode):
        name = Visualizer.__clean_dot_value(node_name)
        if not name or name == opcode:
            return ""
        node_match = re.match(r"Node(\d+)", name)
        if node_match:
            return "n" + node_match.group(1)
        suffix_match = re.match(r"[A-Za-z_]+(\d+)$", name)
        if suffix_match:
            return suffix_match.group(1)
        if len(name) > 10:
            return name[:4] + "..." + name[-4:]
        return name

    @staticmethod
    def __make_dfg_edge_label(attrs):
        label_parts = []
        for key in ["operand", "distance", "dist"]:
            value = attrs.get(key, "")
            if value != "":
                label_parts.append(f"{key[0]}={value}")
        return ", ".join(label_parts)

    @staticmethod
    def __infer_dfg_opcode(node_name, attrs):
        opcode = Visualizer.__clean_dot_value(attrs.get("opcode", ""))
        if opcode:
            return opcode.lower()

        label = Visualizer.__clean_dot_value(attrs.get("label", ""))
        label_map = {
            "+": "add",
            "-": "sub",
            "x": "mul",
            "×": "mul",
            "*": "mul",
            "ld": "load",
            "st": "store",
            "eq": "icmp",
            "Φ": "phi",
            "phi": "phi",
            "br": "br",
        }
        if label in label_map:
            return label_map[label]

        lower_name = node_name.lower()
        for candidate in [
            "getelementptr", "icmpge", "icmpgt", "icmpeq", "icmp",
            "select", "shift", "store", "load", "const",
            "fadd", "fsub", "fmul", "add", "sub", "mul", "div",
            "phi", "br", "or", "loop",
        ]:
            if candidate in lower_name:
                if candidate == "getelementptr":
                    return "gep"
                return candidate
        return "unknown"

    @staticmethod
    def __dfg_operation_category(opcode):
        if opcode in ["load", "store", "output"]:
            return "memory"
        if opcode == "const":
            return "const"
        if opcode in ["gep", "getelementptr"]:
            return "address"
        if opcode in ["phi", "br", "icmp", "icmpgt", "icmpge", "icmpeq",
                      "cmpgt", "cmpge", "cmpeq", "select", "shift", "or", "loop"]:
            return "control"
        if opcode in ["add", "fadd", "sub", "fsub", "mul", "fmul", "div"]:
            return "compute"
        return "unknown"

    @staticmethod
    def __add_dfg_legend(fig):
        legend_items = [
            pat.Patch(facecolor=operation_colors[category], edgecolor="#333333", label=label)
            for category, label in dfg_legend_items
        ]
        legend = fig.legend(handles=legend_items, loc="lower center",
                            ncol=len(legend_items), frameon=False, fontsize=8,
                            borderpad=0.45, handlelength=1.4,
                            columnspacing=1.0, handletextpad=0.45)

    @staticmethod
    def __clean_dot_value(value):
        if isinstance(value, list):
            value = value[0] if value else ""
        return str(value).strip().strip('"')

    @staticmethod
    def __is_empty_dot_node(node_name):
        return node_name.strip() in ["", "\\n", "\n"]
