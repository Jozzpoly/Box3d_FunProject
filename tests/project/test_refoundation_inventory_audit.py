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

    def test_inventory_branch_hard_maximum_cannot_be_raised(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["branchReduction"]["ownerTargetMaximum"] = 12
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_REDUCTION_DRIFT:ownerTargetMaximum:12", errors)

    def test_inventory_deletion_cannot_be_pre_authorized(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["branchReduction"]["deletionAuthorized"] = True
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_REDUCTION_DRIFT:deletionAuthorized:True", errors)

    def test_verified_remote_presence_cannot_be_downgraded(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["branchReduction"]["remotePresence"] = "UNVERIFIED"
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_REDUCTION_DRIFT:remotePresence:UNVERIFIED", errors)

    def test_tracked_tree_coverage_cannot_be_relaxed(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["snapshot"]["trackedTreeSnapshot"]["coverageComplete"] = False
        inventory["snapshot"]["trackedTreeSnapshot"]["unclassifiedPathCount"] = 1
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("INVENTORY_TRACKED_TREE_COVERAGE_INCOMPLETE", errors)

    def test_inventory_divergent_preservation_tag_target_is_exact(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["branchReduction"]["preservationTags"][1]["target"] = "0" * 40
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "INVENTORY_PRESERVATION_TAG_DRIFT:evidence/surface-foundation-pr7:" + "0" * 40,
            errors,
        )

    def test_exact_branch_plan_cannot_drop_one_remote_branch(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        plan = self.load_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH)
        plan["branches"] = [
            record for record in plan["branches"]
            if record["name"] != "agent/p2a-scan-derivatives-foundation"
        ]
        self.write_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH, plan)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_PLAN_COUNT_DRIFT:17", errors)
        self.assertIn(
            "BRANCH_PLAN_REMOTE_BRANCHES_MISSING:agent/p2a-scan-derivatives-foundation",
            errors,
        )

    def test_branch_plan_deletion_cannot_be_pre_authorized(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        plan = self.load_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH)
        plan["ownerPolicy"]["deletionAuthorized"] = True
        self.write_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH, plan)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_PLAN_AUTHORITY_OVERCLAIM:deletionAuthorized", errors)

    def test_only_refoundation_review_branch_may_have_dynamic_sha(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        plan = self.load_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH)
        for record in plan["branches"]:
            if record["name"] == "agent/p1-dataset-inspector-staging":
                record["shaPolicy"] = "DYNAMIC_REVIEW_HEAD_UNTIL_INTEGRATION"
        self.write_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH, plan)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "BRANCH_PLAN_EXACT_SHA_POLICY_MISSING:agent/p1-dataset-inspector-staging:DYNAMIC_REVIEW_HEAD_UNTIL_INTEGRATION",
            errors,
        )

    def test_refoundation_review_branch_must_remain_dynamic_until_integration(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        plan = self.load_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH)
        for record in plan["branches"]:
            if record["name"] == "agent/project-refoundation-audit-v1":
                record["shaPolicy"] = "EXACT_REMOTE_SHA"
        self.write_json(root, refoundation_inventory_audit.BRANCH_PLAN_PATH, plan)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_PLAN_DYNAMIC_REVIEW_POLICY_DRIFT", errors)

    def test_required_domain_cannot_disappear(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["domains"] = [
            domain for domain in inventory["domains"]
            if domain["id"] != "synthetic-engineering-world"
        ]
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "INVENTORY_REQUIRED_DOMAINS_MISSING:synthetic-engineering-world",
            errors,
        )

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

    def test_pr_lineage_cannot_drop_parked_surface_branch(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_json(root, refoundation_inventory_audit.INVENTORY_PATH)
        inventory["pullRequestLineage"] = [
            record for record in inventory["pullRequestLineage"] if record["pr"] != 7
        ]
        self.write_json(root, refoundation_inventory_audit.INVENTORY_PATH, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("INVENTORY_PR_LINEAGE_INCOMPLETE:7", errors)

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
