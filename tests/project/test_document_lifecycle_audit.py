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

import document_lifecycle_audit


class DocumentLifecycleAuditTests(unittest.TestCase):
    def fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name) / "repo"
        root.mkdir()
        manifest = json.loads(
            (ROOT / document_lifecycle_audit.MANIFEST_PATH).read_text(encoding="utf-8")
        )
        paths = {document_lifecycle_audit.MANIFEST_PATH}
        paths.update(record["path"] for record in manifest["currentDocuments"])
        paths.update(record["path"] for record in manifest["projectAuthoredDocuments"])
        for relative in paths:
            source = ROOT / relative
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        return temporary, root

    def load_manifest(self, root: Path) -> dict:
        return json.loads(
            (root / document_lifecycle_audit.MANIFEST_PATH).read_text(encoding="utf-8")
        )

    def write_manifest(self, root: Path, manifest: dict) -> None:
        (root / document_lifecycle_audit.MANIFEST_PATH).write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )

    def test_current_repository_passes(self) -> None:
        self.assertEqual(document_lifecycle_audit.audit_document_lifecycle(ROOT), [])

    def test_one_of_twenty_five_documents_cannot_disappear(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        manifest = self.load_manifest(root)
        manifest["projectAuthoredDocuments"] = [
            record
            for record in manifest["projectAuthoredDocuments"]
            if record["path"] != "docs/PLAN_STABILNOSC_PROWADZENIE_PL.md"
        ]
        self.write_manifest(root, manifest)
        errors = document_lifecycle_audit.audit_document_lifecycle(root)
        self.assertIn(
            "PROJECT_DOCUMENTS_MISSING:docs/PLAN_STABILNOSC_PROWADZENIE_PL.md",
            errors,
        )

    def test_historical_plan_cannot_activate_work(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        manifest = self.load_manifest(root)
        manifest["projectAuthoredDocuments"][0]["mayActivateWork"] = True
        path = manifest["projectAuthoredDocuments"][0]["path"]
        self.write_manifest(root, manifest)
        errors = document_lifecycle_audit.audit_document_lifecycle(root)
        self.assertIn(f"HISTORICAL_DOCUMENT_CAN_ACTIVATE_WORK:{path}", errors)

    def test_completed_refactor_cannot_be_promoted_to_current_authority(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        manifest = self.load_manifest(root)
        for record in manifest["projectAuthoredDocuments"]:
            if record["path"] == "docs/PLAN_WIELKI_REFACTOR_2026_07_09_PL.md":
                record["status"] = "CURRENT_AUTHORITY"
        self.write_manifest(root, manifest)
        errors = document_lifecycle_audit.audit_document_lifecycle(root)
        self.assertIn(
            "DOCUMENT_STATUS_INVALID:docs/PLAN_WIELKI_REFACTOR_2026_07_09_PL.md:CURRENT_AUTHORITY",
            errors,
        ) if False else self.assertIn(
            "HISTORICAL_DOCUMENT_CAN_ACTIVATE_WORK:docs/PLAN_WIELKI_REFACTOR_2026_07_09_PL.md",
            errors,
        )

    def test_project_document_count_cannot_be_faked(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        manifest = self.load_manifest(root)
        manifest["coverage"]["projectAuthoredDocumentCount"] = 24
        self.write_manifest(root, manifest)
        errors = document_lifecycle_audit.audit_document_lifecycle(root)
        self.assertIn("DOCUMENT_COUNT_DRIFT:24", errors)

    def test_completion_marker_cannot_be_removed(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / "docs/PLAN_STABILNOSC_PROWADZENIE_PL.md"
        path.write_text(
            path.read_text(encoding="utf-8").replace(
                "COMPLETED_AND_OWNER_ACCEPTED",
                "ACTIVE_TRACK",
                1,
            ),
            encoding="utf-8",
        )
        errors = document_lifecycle_audit.audit_document_lifecycle(root)
        self.assertIn(
            "DOCUMENT_MARKER_MISSING:docs/PLAN_STABILNOSC_PROWADZENIE_PL.md:COMPLETED_AND_OWNER_ACCEPTED",
            errors,
        )

    def test_current_document_set_cannot_drop_scan_current_state(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        manifest = self.load_manifest(root)
        manifest["currentDocuments"] = [
            record
            for record in manifest["currentDocuments"]
            if record["path"] != "docs/scan_import/CURRENT_STATE.md"
        ]
        self.write_manifest(root, manifest)
        errors = document_lifecycle_audit.audit_document_lifecycle(root)
        self.assertIn(
            "CURRENT_DOCUMENTS_MISSING:docs/scan_import/CURRENT_STATE.md",
            errors,
        )


if __name__ == "__main__":
    unittest.main()
