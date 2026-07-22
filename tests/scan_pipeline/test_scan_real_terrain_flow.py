from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).parents[2]
FLOW = ROOT / "tools" / "scan_pipeline" / "scan_real_terrain_flow.py"


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
        self.assertIn("_discover_bundle", self.text)
        self.assertIn("_active", self.text)


if __name__ == "__main__":
    unittest.main()
