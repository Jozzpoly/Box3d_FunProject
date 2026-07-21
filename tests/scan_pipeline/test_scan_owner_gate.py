from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_owner_gate.py"
spec = importlib.util.spec_from_file_location("scan_owner_gate_tested", MODULE_PATH)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


def report(*, status="compatible-review", passed=True, marker=None):
    value = {
        "schema": module.INSPECTION_SCHEMA,
        "schemaVersion": 3,
        "datasetStatus": status,
        "automaticEvidenceGate": {"passed": passed},
        "totals": {"glbFiles": 7, "plyFiles": 7},
        "pairs": [{"tileId": index} for index in range(7)],
    }
    if marker is not None:
        value["testMarker"] = marker
    return value


def write_report(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, sort_keys=True) + "\n", encoding="utf-8")


class ScanOwnerGateTests(unittest.TestCase):
    def test_identical_passing_reports_are_auto_selected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_report(root / "a" / "inspection.json", report())
            write_report(root / "b" / "inspection.json", report())
            selected, copies = module.select_identical_passing_candidate(
                root, expected_glb=7, expected_ply=7
            )
            self.assertEqual(copies, 2)
            self.assertEqual(selected["path"], root / "a" / "inspection.json")
            self.assertTrue(selected["automaticEvidenceGatePassed"])

    def test_different_passing_reports_require_explicit_selection(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_report(root / "a" / "inspection.json", report(marker="a"))
            write_report(root / "b" / "inspection.json", report(marker="b"))
            with self.assertRaises(module.OwnerGateError):
                module.select_identical_passing_candidate(
                    root, expected_glb=7, expected_ply=7
                )

    def test_failed_or_incompatible_reports_are_not_selected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_report(root / "a" / "inspection.json", report(passed=False))
            write_report(root / "b" / "inspection.json", report(status="incompatible"))
            with self.assertRaises(module.OwnerGateError):
                module.select_identical_passing_candidate(
                    root, expected_glb=7, expected_ply=7
                )

    def test_parse_bundle_path_handles_spaces(self) -> None:
        path = module.parse_bundle_path(
            "scan_import_bundle: OK | path=C:\\Project Files\\bundles\\scan-abc "
            "bundle_sha256=0123 revision=4567"
        )
        self.assertEqual(str(path), "C:\\Project Files\\bundles\\scan-abc")

    def test_receipt_omits_paths_hashes_and_coordinates(self) -> None:
        candidate = {
            "schemaVersion": 3,
            "datasetStatus": "compatible-review",
            "automaticEvidenceGatePassed": True,
            "glbFiles": 7,
            "plyFiles": 7,
            "pairCount": 7,
            "path": Path("private/location/inspection.json"),
            "sha256": "a" * 64,
        }
        receipt = module.build_receipt(
            candidate=candidate,
            identical_copy_count=2,
            privacy_review_acknowledged=False,
        )
        encoded = json.dumps(receipt, sort_keys=True)
        self.assertNotIn("private/location", encoded)
        self.assertNotIn("a" * 64, encoded)
        self.assertEqual(
            receipt["status"],
            "P1B_TECHNICAL_PASS_PRIVACY_REVIEW_REQUIRED",
        )
        self.assertEqual(receipt["privacyReview"]["status"], "PENDING")


if __name__ == "__main__":
    unittest.main()
