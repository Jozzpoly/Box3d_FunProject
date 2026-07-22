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
            ".github/workflows/build.yml": "upstream-github-metadata-and-ci",
            "samples/host/camera.cpp": "shared-native-host",
            "samples/gfx/renderer.c": "shared-native-renderer",
            "samples/shaders/shapes/cube.glsl": "shared-native-renderer",
            "samples/jozz_vehicle_m6_suspension_rig.cpp": "vehicle-physics-rig-and-world",
            "samples/sample_jozz_vehicle_lab.cpp": "vehicle-registration",
            "samples/jozz_scan_preview_pack.cpp": "scan-source-geometry-preview-v1",
            "tools/scan_pipeline/scan_preview_pack.py": "scan-pipeline",
            "docs/scan_import/CURRENT_STATE.md": "scan-pipeline",
            "assets/reports/asset_audit_latest.json": "vehicle-asset-evidence",
            "tools/gate.ps1": "vehicle-tooling-and-gates",
            ".automation/CONTROL.yaml": "automation-and-governance",
            "tools/project/repository_audit.py": "repository-governance",
            "docs/TECH_DEBT_PL.md": "vehicle-documentation",
            "docs/PROJECT_DIRECTION_PL.md": "vehicle-world-and-project-history",
            "docs/simulation.md": "upstream-documentation",
            ".github/pull_request_template.md": "legacy-conflicting-governance",
        }
        for path, expected in cases.items():
            with self.subTest(path=path):
                self.assertEqual(repository_forensic_snapshot._classify_path(path), expected)

    def test_current_tracked_tree_has_complete_coverage(self) -> None:
        unclassified = [
            path
            for path in repository_forensic_snapshot._tracked_paths()
            if repository_forensic_snapshot._classify_path(path).startswith("unclassified-")
        ]
        self.assertEqual(unclassified, [])

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

    def test_exact_branch_plan_contains_only_three_current_refs(self) -> None:
        plan = repository_forensic_snapshot._load_branch_plan()
        branches = plan["currentBranches"]
        self.assertEqual(plan["schemaVersion"], 3)
        self.assertEqual(plan["ownerPolicy"]["currentBranchCount"], 3)
        self.assertEqual(len(branches), 3)
        self.assertEqual(
            {record["name"] for record in branches},
            {"main", "jozz-vehicle-sandbox-m0", "agent/project-refoundation-audit-v1"},
        )
        self.assertFalse(plan["ownerPolicy"]["furtherDeletionAuthorized"])
        self.assertFalse(plan["finalState"]["retentionTaggingComplete"])

    def test_current_three_branches_are_classified(self) -> None:
        inventory = repository_forensic_snapshot._load_inventory()
        annotated = repository_forensic_snapshot._annotate_branches(
            [
                {"name": "main", "sha": "f84f8c32c7d5de17c3cf88f63029b242b115c9de"},
                {
                    "name": "jozz-vehicle-sandbox-m0",
                    "sha": "445db8801297714c4cb344cc9a6ce6cfe753cba7",
                },
                {
                    "name": "agent/project-refoundation-audit-v1",
                    "sha": "f" * 40,
                },
            ],
            inventory,
        )
        self.assertEqual(
            [record["retention"] for record in annotated],
            ["KEEP_BASELINE", "KEEP_BASELINE", "KEEP_ACTIVE"],
        )
        self.assertTrue(all(record["preferredKeep"] for record in annotated))
        self.assertEqual(
            [record["plannedShaMatches"] for record in annotated],
            [True, True, None],
        )
        self.assertEqual(
            [record["dynamicCurrentHead"] for record in annotated],
            [False, False, True],
        )

    def test_unknown_remote_branch_is_not_assumed_deletable(self) -> None:
        inventory = repository_forensic_snapshot._load_inventory()
        annotated = repository_forensic_snapshot._annotate_branches(
            [{"name": "mystery/untracked-lineage", "sha": "f" * 40}],
            inventory,
        )
        self.assertEqual(annotated[0]["retention"], "UNCLASSIFIED_REMOTE_BRANCH")
        self.assertFalse(annotated[0]["preferredKeep"])
        self.assertIsNone(annotated[0]["plannedShaMatches"])

    def test_moved_baseline_head_is_reported_as_plan_mismatch(self) -> None:
        inventory = repository_forensic_snapshot._load_inventory()
        annotated = repository_forensic_snapshot._annotate_branches(
            [{"name": "main", "sha": "0" * 40}],
            inventory,
        )
        self.assertFalse(annotated[0]["plannedShaMatches"])

    def test_retention_tag_debt_remains_exact(self) -> None:
        plan = repository_forensic_snapshot._load_branch_plan()
        debt = plan["retentionDebt"]
        targets = {record["name"]: record["target"] for record in debt["missingTags"]}
        self.assertEqual(
            targets,
            {
                "milestone/terrain-visible-2026-07-22": "33099413bf8f44adbe1d635f9e10bdf2d0b5c321",
                "evidence/surface-foundation-pr7": "9aacc752f331d0d47c4c9c3f6fe82c63466f592c",
                "evidence/owner-flow-pr8": "a36e3d2f4c76f35d138a7e8b0aa11f7889e69e90",
            },
        )


if __name__ == "__main__":
    unittest.main()
