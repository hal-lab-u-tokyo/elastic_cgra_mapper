import sys
import os
sys.path.append(os.pardir)
from visualizer import Visualizer


if __name__ == "__main__":
    args = sys.argv
    if len(args) < 2:
        print("ERROR: invalid arguments")
        print("usage: python3 dfg_visualize_main.py <dfg.dot> [output_dir]")
        raise ValueError

    file_name: str = args[1]
    output_dir = args[2] if len(args) >= 3 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "output")

    output_path = Visualizer.visualize_dfg(file_name, output_dir)
    print(output_path)
