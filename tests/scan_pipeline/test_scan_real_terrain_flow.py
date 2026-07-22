from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
FLOW = ROOT / "tools" / "scan_pipeline" / "scan_real_terrain_flow.py"
spec = importlib.util.spec_from_file_location("scan_real_terrain_flow_tested", FLOW)
assert spec and spec.loader
flow = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = flow
spec.loader.exec_module(flow)


def receipt(bundle_hash: str, revision: str) -> dict[str, object]:
    return {
        "schema": flow.RECEIPT_SCHEMA,
        "schemaVersion": 2,
        "status": "P1B_BUNDLE_PASS",
        "privacyClass": "PRIVATE_LOCAL_ONLY",
        "bundle": {
            "internalVerificationPassed": True,
            "independentVerificationPassed": True,
            "bundleContentSha256": bundle_hash,
            "sourceRevisionId": revision,
        },
    }


class ScanRealTerrainFlowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = FLOW.read_text(encoding="utf-8")

    def test_flow_owns_resolution_preview_selection_and_resume_state(self) -> None:
        for token in (
            "scan_source_assets.resolve_from_bundle",
            "scan_preview_pack.build_preview_pack",
            "ACTIVE_PREVIEW.json",
            "STATE.local.json",
            "CONFIG.local.json",
            "JOZZ_SCAN_PREVIEW_PACK",
            "VISUAL_REVIEW_PENDING",
        ):
            self.assertIn(token, self.text)

    def test_flow_never_grants_terrain_visible_pass(self) -> None:
        self.assertIn('"terrainVisiblePass": False', self.text)
        self.assertIn("TERRAIN_VISIBLE_PASS has not been granted", self.text)
        self.assertNotIn('"terrainVisiblePass": True', self.text)

    def test_owner_is_not_required_to_copy_hashes_between_commands(self) -> None:
        self.assertNotIn("expected-sha256", self.text)
        self.assertNotIn("bundle-sha256", self.text)
        self.assertIn("_discover_bundle_and_receipt", self.text)
        self.assertIn("_active", self.text)

    def test_receipt_selects_exact_bundle_from_multiple_historical_bundles(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = root / "history" / "bundle-a"
            second = root / "current" / "bundle-b"
            first.mkdir(parents=True)
            second.mkdir(parents=True)
            (first / "COMPLETE.json").write_text("{}", encoding="utf-8")
            (second / "COMPLETE.json").write_text("{}", encoding="utf-8")
            revision_a = "sha256:" + "1" * 64
            revision_b = "sha256:" + "2" * 64
            receipt_path = root / "receipts" / "p1b_owner_gate_receipt.local.json"
            receipt_path.parent.mkdir()
            receipt_path.write_text(
                json.dumps(receipt("b" * 64, revision_b)),
                encoding="utf-8",
            )

            original = flow.scan_import_bundle.verify_bundle
            try:
                flow.scan_import_bundle.verify_bundle = lambda path: (
                    {
                        "bundleContentSha256": "a" * 64,
                        "sourceRevisionId": revision_a,
                    }
                    if Path(path) == first
                    else {
                        "bundleContentSha256": "b" * 64,
                        "sourceRevisionId": revision_b,
                    }
                )
                bundle, selected_receipt = flow._discover_bundle_and_receipt(
                    root,
                    None,
                    None,
                )
            finally:
                flow.scan_import_bundle.verify_bundle = original

            self.assertEqual(bundle, second)
            self.assertEqual(selected_receipt, receipt_path)

    def test_receipt_without_matching_bundle_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = root / "bundle"
            bundle.mkdir()
            (bundle / "COMPLETE.json").write_text("{}", encoding="utf-8")
            receipt_path = root / "p1b_owner_gate_receipt.local.json"
            receipt_path.write_text(
                json.dumps(receipt("b" * 64, "sha256:" + "2" * 64)),
                encoding="utf-8",
            )
            original = flow.scan_import_bundle.verify_bundle
            try:
                flow.scan_import_bundle.verify_bundle = lambda path: {
                    "bundleContentSha256": "a" * 64,
                    "sourceRevisionId": "sha256:" + "1" * 64,
                }
                with self.assertRaisesRegex(
                    flow.RealTerrainFlowError,
                    "no verified P1B bundle matches",
                ):
                    flow._discover_bundle_and_receipt(root, None, None)
            finally:
                flow.scan_import_bundle.verify_bundle = original


if __name__ == "__main__":
    unittest.main()
