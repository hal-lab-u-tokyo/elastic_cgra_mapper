import sys
import os
sys.path.append(os.pardir)
from entity import *
from io_lib import *
from visualizer import Visualizer

if __name__ == "__main__":
    args = sys.argv
    if len(args) == 1:
        print("ERROR: invalid arguments")
        raise ValueError
    file_name: str = args[1]

    output_dir = os.path.dirname(
        os.path.abspath(__file__)) + "/output"

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)


    mapping: Mapping = read_mapping_from_json(file_name)

    Visualizer.visualize_mapping(mapping, output_dir)
