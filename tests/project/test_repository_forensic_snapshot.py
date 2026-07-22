from __future__ import annotations

from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).parents[2]
PROJECT_TOOLS = ROOT / "tools" / "project"
sys.path.insert(0, str(PROJECT_TOOLS))

import repository_forensic_snapshot


class RepositoryForensicSnapshotTests(unittest.TestCase):
    def test_critical_path_classification(self) -> None:
        cases = {
            "src/world.c": "upstream-engine-core",
            "include/box3d/box3d.h": "upstream-engine-core",
            "samples/host/camera.cpp": "shared-native-host",
            "samples/jozz_vehicle_m6_suspension_rig.cpp": "vehicle-physics-and-rig",
            "samples/jozz_scan_preview_pack.cpp": "scan-source-geometry-preview-v1",
            "tools/scan_pipeline/scan_preview_pack.py": "scan-pipeline",
            "docs/scan_import/CURRENT_STATE.md": "scan-pipeline",
            ".automation/CONTROL.yaml": "automation-and-governance",
            "tools/project/repository_audit.py": "repository-governance",
            "docs/TECH_DEBT_PL.md": "vehicle-documentation",
        }
        for path, expected in cases.items():
            with self.subTest(path=path):
                self.assertEqual(repository_forensic_snapshot._classify_path(path), expected)

    def test_unknown_path_stays_unclassified(self) -> None:
        self.assertEqual(
            repository_forensic_snapshot._classify_path("mystery/new_domain.xyz"),
            "unclassified-root-or-other",
        )

    def test_remote_parser_is_strict_and_sorted(self) -> None:
        parsed = repository_forensic_snapshot._parse_remote_refs(
            "b" * 40 + "\trefs/heads/zeta\n" + "a" * 40 + "\trefs/heads/alpha\n",
            "refs/heads/",
        )
        self.assertEqual([record["name"] for record in parsed], ["alpha", "zeta"])
        with self.assertRaises(ValueError):
            repository_forensic_snapshot._parse_remote_refs("malformed\n", "refs/heads/")

    def test_inventory_lineage_preserves_divergent_surface_branch(self) -> None:
        inventory = repository_forensic_snapshot._load_inventory()
        lineage = repository_forensic_snapshot._lineage_by_branch(inventory)
        surface = lineage["agent/p2a-scan-derivatives-foundation"]
        self.assertEqual(surface["retention"], "PRESERVE_BY_TAG_OR_COMMIT")
        self.assertIn(7, surface["prs"])

    def test_unknown_remote_branch_is_not_assumed_deletable(self) -> None:
        inventory = repository_forensic_snapshot._load_inventory()
        annotated = repository_forensic_snapshot._annotate_branches(
            [{"name": "mystery/untracked-lineage", "sha": "f" * 40}],
            inventory,
        )
        self.assertEqual(annotated[0]["retention"], "UNCLASSIFIED_REMOTE_BRANCH")
        self.assertFalse(annotated[0]["preferredKeep"])

    def test_durable_baselines_are_kept(self) -> None:
        inventory = repository_forensic_snapshot._load_inventory()
        annotated = repository_forensic_snapshot._annotate_branches(
            [
                {"name": "main", "sha": "1" * 40},
                {"name": "jozz-vehicle-sandbox-m0", "sha": "2" * 40},
            ],
            inventory,
        )
        self.assertEqual([record["retention"] for record in annotated], ["KEEP_BASELINE", "KEEP_BASELINE"])
        self.assertTrue(all(record["preferredKeep"] for record in annotated))


if __name__ == "__main__":
    unittest.main()
