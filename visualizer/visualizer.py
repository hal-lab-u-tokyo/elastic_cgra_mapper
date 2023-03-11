import matplotlib.pyplot as plt
import matplotlib.patches as pat
import networkx as nx
import pulp

from mapping import *

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


class Visualizer():
    @staticmethod
    def visualize_mapping(mapping: Mapping, output_dir: str):
        fig = plt.figure(figsize=(mapping.column_num *
                         mapping.context_size, mapping.row_num))

        for context_id in range(mapping.context_size):
            Visualizer.visualize_mapping_one_context(mapping, context_id, fig)

        plt.savefig(output_dir + "/result.png")

    @staticmethod
    def visualize_mapping_one_context(mapping: Mapping, context_id: int, fig):
        ax = fig.add_subplot(1, mapping.context_size, context_id + 1)
        ax.set_xbound(0, mapping.column_num)
        ax.set_ybound(0, mapping.row_num)
        plt.tick_params(labelbottom=False, labelleft=False, labelright=False,
                        labeltop=False, bottom=False, left=False, right=False, top=False)

        row_num = mapping.row_num
        column_num = mapping.column_num

        def create_PE_id(column_id, row_id):
            return column_id * 100 + row_id

        def create_xy_from_row_id_and_column_id(row_id, column_id, row_num):
            return (column_id, row_num - 1 - row_id)

        PE_id_to_patch = {}

        for row_id in range(row_num):
            for column_id in range(column_num):
                tmp_PE_config = mapping.PE_array[row_id][column_id].config_list[context_id]

                x, y = create_xy_from_row_id_and_column_id(row_id, column_id, row_num)

                # add PE and opcode
                PE_operation_type = tmp_PE_config.operation_type
                if PE_operation_type != OperationType.Nop:
                    color = pe_color
                    opcode = OperationType.to_string(PE_operation_type)
                    ax.annotate(opcode, xy=(x + 1 - pe_margin * 3,
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

        for row_id in range(row_num):
            for column_id in range(column_num):
                x, y = create_xy_from_row_id_and_column_id(row_id, column_id, row_num)
                tmp_PE_id = create_PE_id(row_id, column_id)
                tmp_PE_patch = PE_id_to_patch[tmp_PE_id]
                tmp_PE_config = mapping.PE_array[row_id][column_id].config_list[context_id]

                for from_config_id in tmp_PE_config.from_config_id:
                    from_PE_id = create_PE_id(
                        from_config_id.row_id, from_config_id.column_id)
                    from_PE_patch = PE_id_to_patch[from_PE_id]
                    ax.annotate("", xy=Visualizer.__get_center(tmp_PE_patch),
                                xytext=Visualizer.__get_center(from_PE_patch),
                                arrowprops=arrow_setting)

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
