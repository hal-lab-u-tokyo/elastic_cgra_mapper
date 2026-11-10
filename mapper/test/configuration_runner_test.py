"""Runner overrides must respect both mapper-configuration schemas."""
import copy
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python_tools/experiment_runner/heuristic"))
from lib import apply_algorithm_overrides


class ConfigurationRunnerTest(unittest.TestCase):
    def setUp(self):
        self.defaults = json.loads((ROOT / "data/mapper_config.json").read_text())

    def test_common_overrides_stay_in_execution(self):
        original = copy.deepcopy(self.defaults["Algorithm"])
        apply_algorithm_overrides(self.defaults, {"type": "Placement2DSAMapper", "timeout_s": 10, "log_file_path": "unused"}, placement_only=True)
        self.assertEqual(self.defaults["Algorithm"]["PRISA"], original["PRISA"])
        self.assertTrue(self.defaults["Algorithm"]["SA"]["placement_only"])
        self.assertNotIn("timeout_s", self.defaults["Exec"])
        self.assertNotIn("log_file_path", self.defaults["Exec"])
        self.assertNotIn("placement_only", self.defaults["Exec"])
        self.assertEqual(self.defaults["Exec"]["type"], "Placement2DSAMapper")

    def test_family_override_does_not_touch_unselected_families(self):
        variants = {"ILPMapper": "ILP", "Placement2DSAMapper": "SA",
                    "ModuloPhysicalPRISAManhattanMapper": "PRISA", "YOTO": "YOTO",
                    "YOTT": "YOTT", "Placement2DCPUMappingYOTTCore": "YOTTCore",
                    "Placement2DCPUMappingYOTTCoreBottleneckRepair": "YOTTCoreRepair",
                    "PlacementFirstHeuristicMapper": "PlacementFirst"}
        for name, family in variants.items():
            with self.subTest(name=name):
                config = copy.deepcopy(self.defaults)
                apply_algorithm_overrides(config, {"type": name, "random_seed": 0})
                self.assertEqual(config["Algorithm"][family]["random_seed"], 0)
                for other in config["Algorithm"]:
                    if other != family:
                        self.assertEqual(config["Algorithm"][other], self.defaults["Algorithm"][other])
                self.assertNotIn("random_seed", config["Exec"])

    def test_inherent_placement_does_not_gain_an_unused_mode_flag(self):
        for name in ("Placement2DArraySAMapper", "Placement2DCPUMappingYOTTCore", "ILPMapper", "ModuloSAMapper"):
            config = copy.deepcopy(self.defaults)
            config["Exec"]["type"] = name
            before = copy.deepcopy(config)
            apply_algorithm_overrides(config, {}, placement_only=True)
            self.assertEqual(config, before)

    def test_legacy_input_preserved(self):
        config = {"Algorithm": {"type": "SA", "max_trials": 7}}
        apply_algorithm_overrides(config, {"trace_trials": False, "random_seed": 0})
        self.assertEqual(config, {"Algorithm": {"type": "SA", "max_trials": 7,
                                               "trace_trials": False, "random_seed": 0}})

    def test_unknown_custom_family_requires_explicit_configuration(self):
        with self.assertRaises(ValueError):
            apply_algorithm_overrides(self.defaults, {"type": "CustomMapper", "max_trials": 1})


if __name__ == "__main__":
    unittest.main()
