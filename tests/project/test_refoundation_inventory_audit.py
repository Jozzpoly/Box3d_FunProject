from __future__ import annotations

import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
PROJECT_TOOLS = ROOT / "tools" / "project"
sys.path.insert(0, str(PROJECT_TOOLS))

import refoundation_inventory_audit


class RefoundationInventoryAuditTests(unittest.TestCase):
    def fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name) / "repo"
        root.mkdir()
        for relative in (
            refoundation_inventory_audit.INVENTORY_PATH,
            refoundation_inventory_audit.BRANCH_PLAN_PATH,
            refoundation_inventory_audit.REPORT_PATH,
            refoundation_inventory_audit.SCAN_START_PATH,
            refoundation_inventory_audit.TECH_DEBT_PATH,
            refoundation_inventory_audit.COMPATIBILITY_README_PATH,
            refoundation_inventory_audit.PROJECT_DIRECTION_PATH,
        ):
            source = ROOT / relative
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        return temporary, root

    def load_json(self, root: Path, relative: str) -> dict:
        return json.loads((root / relative).read_text(encoding="utf-8"))

    def write_json(self, root: Path, relative: str, value: dict) -> None:
        (root / relative).write_text(
            json.dumps(value, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )

    def test_current_repository_passes(self) -> None:
        self.assertEqual(refoundation_inventory_audit.audit_refoundation_inventory(ROOT), [])

    def test_branch_hard_maximum_cannot_be_raised(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["branchReduction"]["ownerTargetMaximum"] = 12
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_REDUCTION_DRIFT:ownerTargetMaximum:12", errors)

    def test_further_deletion_cannot_be_pre_authorized(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["branchReduction"]["furtherDeletionAuthorized"] = True
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_REDUCTION_DRIFT:furtherDeletionAuthorized:True", errors)

    def test_fourth_branch_cannot_be_smuggled_into_current_state(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["snapshot"]["remoteRefState"]["branches"].append("agent/old-branch")
        inventory["snapshot"]["remoteRefState"]["branchCount"] = 4
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("INVENTORY_REMOTE_REF_COUNT_DRIFT", errors)
        self.assertTrue(any(error.startswith("INVENTORY_REMOTE_BRANCH_SET_DRIFT:") for error in errors))

    def test_deleted_authority_branch_cannot_return(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["snapshot"]["authoritativeProductBranch"] = (
            "agent/scan-terrain-r1b-consolidated-integration"
        )
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "INVENTORY_SNAPSHOT_DRIFT:authoritativeProductBranch:agent/scan-terrain-r1b-consolidated-integration",
            errors,
        )

    def test_current_project_branch_cannot_disappear(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        plan = self.load_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH)
        plan["currentBranches"] = [
            record
            for record in plan["currentBranches"]
            if record["name"] != refoundation_inventory_audit.CURRENT_PROJECT_BRANCH
        ]
        self.write_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH, plan)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertTrue(any(error.startswith("BRANCH_PLAN_CURRENT_SET_DRIFT:") for error in errors))

    def test_current_project_branch_must_remain_dynamic_until_integration(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        plan = self.load_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH)
        for record in plan["currentBranches"]:
            if record["name"] == refoundation_inventory_audit.CURRENT_PROJECT_BRANCH:
                record["shaPolicy"] = "EXACT_REMOTE_SHA"
        self.write_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH, plan)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_PLAN_CURRENT_HEAD_POLICY_DRIFT", errors)

    def test_retention_tag_debt_cannot_be_hidden(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        plan = self.load_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH)
        plan["retentionDebt"]["missingTags"] = []
        plan["finalState"]["retentionTaggingComplete"] = True
        self.write_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH, plan)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "BRANCH_PLAN_RETENTION_TAG_DRIFT:milestone/terrain-visible-2026-07-22:None",
            errors,
        )
        self.assertIn("BRANCH_PLAN_FINAL_STATE_DRIFT:retentionTaggingComplete:True", errors)

    def test_retention_tag_target_is_exact(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        plan = self.load_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH)
        plan["retentionDebt"]["missingTags"][1]["target"] = "0" * 40
        self.write_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH, plan)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "BRANCH_PLAN_RETENTION_TAG_DRIFT:evidence/surface-foundation-pr7:" + "0" * 40,
            errors,
        )

    def test_required_domain_cannot_disappear(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["domains"] = [
            domain
            for domain in inventory["domains"]
            if domain["id"] != "synthetic-engineering-world"
        ]
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "INVENTORY_REQUIRED_DOMAINS_MISSING:synthetic-engineering-world",
            errors,
        )

    def test_pr_lineage_cannot_drop_parked_surface_evidence(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["pullRequestLineage"] = [
            record for record in inventory["pullRequestLineage"] if record["pr"] != 7
        ]
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("INVENTORY_PR_LINEAGE_INCOMPLETE:7", errors)

    def test_current_integration_pr_cannot_disappear(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["pullRequestLineage"] = [
            record for record in inventory["pullRequestLineage"] if record["pr"] != 17
        ]
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("INVENTORY_PR_LINEAGE_INCOMPLETE:17", errors)

    def test_historical_scan_tutorial_cannot_become_current_again(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / refoundation_inventory_audit.SCAN_START_PATH
        path.write_text(
            path.read_text(encoding="utf-8")
            + "\n**Status:** `P1A_REAL_INSPECTION_LOCAL_PASS / P1B_OWNER_GATE_HARDENING`\n"
            + "Następne po tej bramce\n",
            encoding="utf-8",
        )
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "SCAN_START_STALE_IMPERATIVE:**Status:** `P1A_REAL_INSPECTION_LOCAL_PASS / P1B_OWNER_GATE_HARDENING`",
            errors,
        )
        self.assertIn("SCAN_START_STALE_IMPERATIVE:Następne po tej bramce", errors)

    def test_direct_push_rule_cannot_return_to_vehicle_debt(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / refoundation_inventory_audit.TECH_DEBT_PATH
        path.write_text(
            path.read_text(encoding="utf-8")
            + "\nagenci odtąd samodzielnie commitują i pushują\n"
            + "na `jozz-vehicle-sandbox-m0`\n",
            encoding="utf-8",
        )
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("TECH_DEBT_STALE_OPERATIONAL_RULE", errors)

    def test_conflicting_upstream_default_pr_template_cannot_return(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / refoundation_inventory_audit.LEGACY_PR_TEMPLATE
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("Pull requests for Box3D code are not accepted.\n", encoding="utf-8")
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "LEGACY_CONFLICTING_PR_TEMPLATE_PRESENT:.github/pull_request_template.md",
            errors,
        )


if __name__ == "__main__":
    unittest.main()
