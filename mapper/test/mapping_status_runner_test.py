"""Mapping-result status stays distinct from run and solver diagnostics."""
import csv
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python_tools/experiment_runner/heuristic"))
from lib import CSV_FIELDS, normalize_run, read_mapping_status, write_metrics_csv


class MappingStatusRunnerTest(unittest.TestCase):
    def test_explicit_status_is_authoritative(self):
        for status in ("optimal", "suboptimal", "optimality_unknown", "failure"):
            self.assertEqual(read_mapping_status({"mapping_status": status, "is_success": status == "failure"}), status)
        for invalid in ("success", "", None, 3, []):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                read_mapping_status({"mapping_status": invalid})

    def test_legacy_success_does_not_invent_optimality(self):
        self.assertEqual(read_mapping_status({"is_success": True}), "optimality_unknown")
        self.assertEqual(read_mapping_status({"is_success": False}), "failure")
        self.assertEqual(read_mapping_status({}), "failure")

    def test_solver_log_cannot_promote_staged_or_heuristic_results(self):
        import json
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "gurobi_log_test.log").write_text("Optimal solution found (tolerance 1.00e-04)\n")
            for status in ("optimal", "suboptimal", "optimality_unknown", "failure"):
                (root / "output_log_test.json").write_text(json.dumps({"mapping_status": status}))
                row = normalize_run(root, "fixture", "mapper", "arch", None, None)
                self.assertEqual(row["mapping_status"], status)
                self.assertEqual(row["status"], "optimal" if status == "optimal" else "failed" if status == "failure" else "success")

    def test_csv_appends_status_without_moving_existing_fields(self):
        self.assertEqual(len(CSV_FIELDS), 159)
        self.assertEqual(CSV_FIELDS[-2:], ["raw_output_dir", "mapping_status"])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "metrics.csv"
            write_metrics_csv(path, [{"status": "success"}, {"status": "error"},
                                     {"status": "success", "mapping_status": "suboptimal"}])
            with path.open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual([r["mapping_status"] for r in rows],
                             ["optimality_unknown", "failure", "suboptimal"])
            with self.assertRaises(ValueError):
                write_metrics_csv(path, [{"mapping_status": "bad"}])


if __name__ == "__main__":
    unittest.main()
