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

import repository_audit


class RepositoryAuditTests(unittest.TestCase):
    def fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name) / "repo"
        root.mkdir()
        for relative in repository_audit.REQUIRED_FILES:
            source = ROOT / relative
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        automation_common = ROOT / "tools" / "automation" / "common.py"
        destination_common = root / "tools" / "automation" / "common.py"
        destination_common.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(automation_common, destination_common)
        return temporary, root

    def test_current_repository_passes(self) -> None:
        self.assertEqual(repository_audit.audit_repository(ROOT), [])

    def test_wrong_global_rules_file_is_detected(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / ".automation" / "CONTROL.yaml"
        control = json.loads(path.read_text(encoding="utf-8"))
        control["authority"]["global_agent_rules"] = "README_FOR_AGENTS.md"
        path.write_text(json.dumps(control, indent=2), encoding="utf-8")
        errors = repository_audit.audit_repository(root)
        self.assertIn("GLOBAL_RULES_NOT_AGENTS", errors)

    def test_authoritative_branch_drift_is_detected(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / "docs" / "scan_import" / "CURRENT_STATE.md"
        text = path.read_text(encoding="utf-8").replace(
            repository_audit.ACTIVE_BRANCH,
            "agent/stale-branch",
            1,
        )
        path.write_text(text, encoding="utf-8")
        errors = repository_audit.audit_repository(root)
        self.assertIn("ACTIVE_BRANCH_DRIFT:current:agent/stale-branch", errors)

    def test_upstream_only_readme_is_detected(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / "README.md"
        path.write_text("# Box3D\n", encoding="utf-8")
        errors = repository_audit.audit_repository(root)
        self.assertTrue(any(value.startswith("MISSING_MARKER:ROOT_README") for value in errors))

    def test_legacy_focused_workflow_is_detected(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / repository_audit.LEGACY_WORKFLOW
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("name: legacy\non:\n  workflow_dispatch:\n", encoding="utf-8")
        errors = repository_audit.audit_repository(root)
        self.assertIn(
            f"LEGACY_WORKFLOW_STILL_PRESENT:{repository_audit.LEGACY_WORKFLOW}",
            errors,
        )

    def test_unexpected_github_schedule_is_detected(self) -> None:
        temporary, root = self.fixture()
        self.addCleanup(temporary.cleanup)
        path = root / ".github" / "workflows" / "repository-governance.yml"
        text = path.read_text(encoding="utf-8").replace(
            "  workflow_dispatch:\n",
            "  schedule:\n    - cron: '0 * * * *'\n  workflow_dispatch:\n",
        )
        path.write_text(text, encoding="utf-8")
        errors = repository_audit.audit_repository(root)
        self.assertIn(
            "UNEXPECTED_GITHUB_SCHEDULE:.github/workflows/repository-governance.yml",
            errors,
        )


if __name__ == "__main__":
    unittest.main()
