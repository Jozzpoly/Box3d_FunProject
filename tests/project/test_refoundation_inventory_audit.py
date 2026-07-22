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
            refoundation_inventory_audit.REPORT_PATH,
            refoundation_inventory_audit.SCAN_START_PATH,
            refoundation_inventory_audit.TECH_DEBT_PATH,
        ):
            source = ROOT / relative
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        return temporary, root

    def load_inventory(self, root: Path) -> dict:
        path = root / refoundation_inventory_audit.INVENTORY_PATH
        return json.loads(path.read_text(encoding="utf-8"))

    def write_inventory(self, root: Path, inventory: dict) -> None:
        path = root / refoundation_inventory_audit.INVENTORY_PATH
        path.write_text(json.dumps(inventory, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    def test_current_repository_passes(self) -> None:
        self.assertEqual(refoundation_inventory_audit.audit_refoundation_inventory(ROOT), [])

    def test_branch_hard_maximum_cannot_be_raised(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_inventory(root)
        inventory["branchReduction"]["ownerTargetMaximum"] = 12
        self.write_inventory(root, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_HARD_MAXIMUM_DRIFT", errors)

    def test_branch_deletion_cannot_be_pre_authorized(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_inventory(root)
        inventory["branchReduction"]["deletionAuthorized"] = True
        self.write_inventory(root, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_DELETION_AUTHORITY_OVERCLAIM", errors)

    def test_remote_presence_cannot_be_claimed_before_enumeration(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_inventory(root)
        inventory["branchReduction"]["remotePresence"] = "VERIFIED"
        self.write_inventory(root, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("BRANCH_REMOTE_PRESENCE_OVERCLAIM", errors)

    def test_divergent_preservation_tag_target_is_exact(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_inventory(root)
        tags = inventory["branchReduction"]["provisionalPreservationTags"]
        tags[1]["target"] = "0" * 40
        self.write_inventory(root, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn(
            "BRANCH_PRESERVATION_TAG_DRIFT:evidence/surface-foundation-pr7:" + "0" * 40,
            errors,
        )

    def test_required_domain_cannot_disappear(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        inventory = self.load_inventory(root)
        inventory["domains"] = [
            domain for domain in inventory["domains"] if domain["id"] != "synthetic-engineering-world"
        ]
        self.write_inventory(root, inventory)
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
            + "\nStatus: P1B_OWNER_GATE_HARDENING\nNastępne po tej bramce\n",
            encoding="utf-8",
        )
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("SCAN_START_STALE_IMPERATIVE:P1B_OWNER_GATE_HARDENING", errors)
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
        inventory = self.load_inventory(root)
        inventory["pullRequestLineage"] = [
            record for record in inventory["pullRequestLineage"] if record["pr"] != 7
        ]
        self.write_inventory(root, inventory)
        errors = refoundation_inventory_audit.audit_refoundation_inventory(root)
        self.assertIn("INVENTORY_PR_LINEAGE_INCOMPLETE:7", errors)


if __name__ == "__main__":
    unittest.main()
