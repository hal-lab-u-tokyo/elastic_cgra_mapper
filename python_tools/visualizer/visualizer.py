import sys
import os
sys.path.append(os.pardir)
import matplotlib.pyplot as plt
import matplotlib.patches as pat

from entity import *

# reference: GenMap ConfDrawer.py
# https://github.com/hal-lab-u-tokyo/GenMap/blob/master/ConfDrawer.py

# drawing setting
pe_margin = 0.15
pe_color = "skyblue"
alu_scale = 0.3
alu_color = "lightcoral"
pe_size = 1 - pe_margin * 2
arrow_setting = dict(facecolor='black', width=0.8,
                     headwidth=4.0, headlength=4.0, shrink=0.01)
architecture_arrow_setting = dict(
    arrowstyle="->", color="#9e9e9e", linewidth=0.35,
    alpha=0.28, mutation_scale=5, shrinkA=8, shrinkB=8)
active_arrow_setting = dict(
    arrowstyle="->", color="black", linewidth=1.2,
    mutation_scale=8, shrinkA=9, shrinkB=9)
dropped_arrow_setting = dict(
    arrowstyle="->", color="#d32f2f", linewidth=1.2,
    linestyle="dashed", mutation_scale=8, shrinkA=9, shrinkB=9)


class Visualizer():
    @staticmethod
    def visualize_mapping(mapping: Mapping, output_dir: str):
        architecture_node_list = getattr(mapping, "architecture_node_list", [])
        if architecture_node_list:
            min_column = min(node.column_id for node in architecture_node_list)
            max_column = max(node.column_id for node in architecture_node_list)
            min_row = min(node.row_id for node in architecture_node_list)
            max_row = max(node.row_id for node in architecture_node_list)
            row_span = max_row - min_row + 1
        else:
            extra_pe_list = getattr(mapping, "extra_PE_list", [])
            min_column = min([0] + [pe.column_id for pe in extra_pe_list])
            max_column = max([mapping.column_num - 1] +
                             [pe.column_id for pe in extra_pe_list])
            row_span = mapping.row_num
        column_span = max_column - min_column + 1
        fig = plt.figure(figsize=(max(6, column_span * mapping.context_size),
                                  max(4, row_span)))

        for context_id in range(mapping.context_size):
            Visualizer.visualize_mapping_one_context(mapping, context_id, fig)

        plt.tight_layout()
        plt.savefig(output_dir + "/result.pdf", bbox_inches="tight")
        plt.close(fig)

    @staticmethod
    def visualize_mapping_one_context(mapping: Mapping, context_id: int, fig):
        if getattr(mapping, "architecture_node_list", []):
            Visualizer.visualize_explicit_mapping_one_context(
                mapping, context_id, fig)
            return

        ax = fig.add_subplot(1, mapping.context_size, context_id + 1)
        ax.set_xbound(0, mapping.column_num)
        ax.set_ybound(0, mapping.row_num)
        plt.tick_params(labelbottom=False, labelleft=False, labelright=False,
                        labeltop=False, bottom=False, left=False, right=False, top=False)

        row_num = mapping.row_num
        column_num = mapping.column_num
        extra_pe_list = getattr(mapping, "extra_PE_list", [])
        min_column = min([0] + [pe.column_id for pe in extra_pe_list])
        max_column = max([column_num - 1] +
                         [pe.column_id for pe in extra_pe_list])
        column_span = max_column - min_column + 1
        ax.set_xbound(0, column_span)

        def create_PE_id(row_id, column_id):
            return (row_id, column_id)

        def create_xy_from_row_id_and_column_id(row_id, column_id, row_num):
            return (column_id - min_column, row_num - 1 - row_id)

        def get_all_pe_list():
            result = []
            for row_id in range(row_num):
                for column_id in range(column_num):
                    result.append(mapping.PE_array[row_id][column_id])
            result.extend(extra_pe_list)
            return result

        PE_id_to_patch = {}

        for pe_ele in get_all_pe_list():
            row_id = pe_ele.row_id
            column_id = pe_ele.column_id
            tmp_PE_config = pe_ele.config_list[context_id]

            x, y = create_xy_from_row_id_and_column_id(row_id, column_id, row_num)

            # add PE and opcode
            PE_operation_type = tmp_PE_config.operation_type
            if PE_operation_type != OperationType.Nop:
                color = pe_color
                op_name = tmp_PE_config.operation_name
                ax.annotate(op_name, xy=(x + 1 - pe_margin * 3,
                            y + 1 - pe_margin * 2), size=12)
            else:
                color = "white"
            pe = Visualizer.__make_PE_patch((x, y), color)
            ax.add_patch(pe)

            # add ALU
            alu = Visualizer.__make_ALU_patch((x, y))
            ax.add_patch(alu)

            PE_id = create_PE_id(row_id, column_id)
            PE_id_to_patch[PE_id] = alu

        for pe_ele in get_all_pe_list():
            row_id = pe_ele.row_id
            column_id = pe_ele.column_id
            tmp_PE_id = create_PE_id(row_id, column_id)
            tmp_PE_patch = PE_id_to_patch[tmp_PE_id]
            tmp_PE_config = pe_ele.config_list[context_id]

            for from_config_id in tmp_PE_config.from_config_id:
                from_PE_id = create_PE_id(
                    from_config_id.row_id, from_config_id.column_id)
                if from_PE_id not in PE_id_to_patch:
                    continue
                from_PE_patch = PE_id_to_patch[from_PE_id]
                ax.annotate("", xy=Visualizer.__get_center(tmp_PE_patch),
                            xytext=Visualizer.__get_center(from_PE_patch),
                            arrowprops=arrow_setting)

    @staticmethod
    def visualize_explicit_mapping_one_context(
            mapping: Mapping, context_id: int, fig):
        ax = fig.add_subplot(1, mapping.context_size, context_id + 1)
        context_nodes = [
            node for node in mapping.architecture_node_list
            if node.context_id == context_id
        ]
        node_by_id = {node.node_id: node for node in context_nodes}

        min_column = min(node.column_id for node in context_nodes)
        max_column = max(node.column_id for node in context_nodes)
        min_row = min(node.row_id for node in context_nodes)
        max_row = max(node.row_id for node in context_nodes)
        column_span = max_column - min_column + 1
        row_span = max_row - min_row + 1

        ax.set_xlim(-0.1, column_span + 0.1)
        ax.set_ylim(-0.1, row_span + 0.1)
        ax.set_aspect("equal")
        ax.set_title(f"context {context_id}")
        ax.tick_params(labelbottom=False, labelleft=False, labelright=False,
                       labeltop=False, bottom=False, left=False, right=False,
                       top=False)

        def create_xy(node):
            return (node.column_id - min_column, max_row - node.row_id)

        def create_center(node):
            x, y = create_xy(node)
            return (x + 0.5, y + 0.5)

        def get_config(node):
            pe = mapping.PE_dict.get((node.row_id, node.column_id))
            if pe is None:
                return None
            for config in pe.config_list:
                if config.context_id == context_id:
                    return config
            return None

        # Draw the complete explicit MRRG as a light background.
        for edge in mapping.architecture_edge_list:
            if (edge.from_node_id not in node_by_id or
                    edge.to_node_id not in node_by_id):
                continue
            ax.annotate(
                "", xy=create_center(node_by_id[edge.to_node_id]),
                xytext=create_center(node_by_id[edge.from_node_id]),
                arrowprops=architecture_arrow_setting, zorder=0)

        node_by_position = {}
        for node in context_nodes:
            config = get_config(node)
            is_mapped = (config is not None and
                         config.operation_type != OperationType.Nop)
            patch = Visualizer.__make_architecture_node_patch(
                create_xy(node), node, is_mapped)
            ax.add_patch(patch)
            node_by_position[(node.row_id, node.column_id)] = node

            label = node.node_id
            if is_mapped:
                operation_name = config.operation_name.replace("\\n", "\n")
                short_name = operation_name.splitlines()[0]
                label += ("\n" + short_name + ":" +
                          OperationType.to_string(config.operation_type))
            x, y = create_center(node)
            ax.annotate(label, xy=(x, y), ha="center", va="center",
                        fontsize=4.5, zorder=3)

        # Overlay the actually used mapping edges.
        for node in context_nodes:
            config = get_config(node)
            if config is None:
                continue
            for from_config_id in config.from_config_id:
                if from_config_id.context_id != context_id:
                    continue
                from_node = node_by_position.get(
                    (from_config_id.row_id, from_config_id.column_id))
                if from_node is None:
                    continue
                ax.annotate(
                    "", xy=create_center(node), xytext=create_center(from_node),
                    arrowprops=active_arrow_setting, zorder=4)
            for from_config_id in config.dropped_from_config_id:
                if from_config_id.context_id != context_id:
                    continue
                from_node = node_by_position.get(
                    (from_config_id.row_id, from_config_id.column_id))
                if from_node is None:
                    continue
                ax.annotate(
                    "", xy=create_center(node), xytext=create_center(from_node),
                    arrowprops=dropped_arrow_setting, zorder=4)

        ax.text(0.01, 0.01,
                "gray: physical connection / black: mapped route / red: dropped",
                transform=ax.transAxes, fontsize=6, color="#555555")

    @staticmethod
    def __make_architecture_node_patch(coord, node, is_mapped):
        x, y = coord
        if OperationType.SPM in node.supported_operations:
            color = "#a5d6a7" if is_mapped else "#eef8f0"
            return pat.FancyBboxPatch(
                (x + pe_margin, y + pe_margin), pe_size, pe_size,
                boxstyle="round,pad=0.02,rounding_size=0.18",
                facecolor=color, edgecolor="#37844a", linewidth=1.2,
                zorder=2)
        if OperationType.Loop in node.supported_operations:
            color = "#d1b3e8" if is_mapped else "#f4ecfa"
            return pat.FancyBboxPatch(
                (x + pe_margin, y + pe_margin), pe_size, pe_size,
                boxstyle="round,pad=0.02,rounding_size=0.08",
                facecolor=color, edgecolor="#7547a8", linewidth=1.2,
                zorder=2)
        color = pe_color if is_mapped else "white"
        return pat.Rectangle(
            xy=(x + pe_margin, y + pe_margin), width=pe_size,
            height=pe_size, angle=0, facecolor=color, edgecolor="black",
            linewidth=0.8, zorder=2)

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
                             angle=0, facecolor=color, edgecolor="black")

    @staticmethod
    def __make_ALU_patch(coord):
        """Makes a patch for ALU
            Args:
                coord (tuple): coordinate of the PE
            Returns:
                patch of matplotlib: an ALU
        """
        pos = (coord[0] + 0.5, coord[1] + 0.4)
        x = [0.0, 0.4, 0.5, 0.6, 1.0, 0.8, 0.2]
        y = [0.0, 0.0, 0.2, 0.0, 0.0, 0.7, 0.7, 0.0]

        x = [v * alu_scale + pos[0] for v in x]
        y = [v * alu_scale + pos[1] for v in y]

        return pat.Polygon(xy=list(zip(x, y)), color=alu_color)

    @staticmethod
    def __get_center(patch):
        """Calculates center coordinate of patch
        """
        if isinstance(patch, plt.Rectangle):
            width = patch.get_width()
            height = patch.get_width()
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
