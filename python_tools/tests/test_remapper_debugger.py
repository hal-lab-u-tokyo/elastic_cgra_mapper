import csv
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from experiment_runner.remapper_debugger import read_commands, write_launch_json


class Config:
    database_path = "/db"
    kernel_dir_path = "/kernels"
    remapper_timeout_s = 123


class RemapperDebuggerTest(unittest.TestCase):
    def test_analyzer_csv_becomes_command_and_launch_block(self):
        row = {
            "benchmark_name": "fixed_fir_pro",
            "cgra_row": "6", "cgra_column": "7",
            "cgra_memory_io": "all", "cgra_type": "elastic",
            "cgra_network_type": "orthogonal",
            "cgra_local_reg_size": "1", "cgra_context_size": "4",
            "cgra_loop_controllers": "[]", "mapping_succeed": "False",
            "remapper_type": "DP", "remapper_time_s": "",
            "parallel_num": "", "mapping_type_num": "",
            "num_available_mappings": "-1", "database_mapping_files_num": "5",
        }
        with tempfile.TemporaryDirectory() as directory:
            csv_path = os.path.join(directory, "failed.csv")
            with open(csv_path, "w", newline="") as output:
                writer = csv.DictWriter(output, fieldnames=row)
                writer.writeheader()
                writer.writerow(row)
            commands = read_commands(csv_path, Config(), directory, "/repo/build/remapping")
            self.assertEqual(len(commands), 1)
            self.assertEqual(commands[0].cgra.row, 6)
            self.assertEqual(commands[0].cgra.column, 7)
            self.assertEqual(commands[0].num_available_mappings, -1)
            self.assertEqual(commands[0].program, "/repo/build/remapping")
            self.assertEqual(commands[0].cgra.loop_controller_list, [])

            launch_path = os.path.join(directory, "launch.json")
            write_launch_json(commands, launch_path, "/repo/build/remapping")
            with open(launch_path) as launch_file:
                launch = json.load(launch_file)
            configuration = launch["configurations"][0]
            self.assertEqual(configuration["program"], "/repo/build/remapping")
            self.assertIn("--database_dir", configuration["args"])
            self.assertIn("--remapper_mode", configuration["args"])
            self.assertIn("2", configuration["args"])

    def test_missing_required_column_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            csv_path = os.path.join(directory, "failed.csv")
            with open(csv_path, "w", newline="") as output:
                output.write("benchmark_name\nfixed_fir_pro\n")
            with self.assertRaisesRegex(ValueError, "Missing CSV columns"):
                read_commands(csv_path, Config(), directory)


if __name__ == "__main__":
    unittest.main()
