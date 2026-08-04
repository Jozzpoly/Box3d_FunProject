#!/usr/bin/env python3
"""Regression tests for JV repository-hygiene tools.

The integration cases create their own temporary Git repositories. They never
modify the real JV source tree, Git index or history.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

# The suite dynamically imports repository tools. Never leave Python bytecode
# beside tracked sources: a quality gate must not dirty the tree it validates.
sys.dont_write_bytecode = True

REPO = Path(__file__).resolve().parents[1]
DOCS_AUDIT = REPO / "tools" / "docs_audit.py"
EXPORT_SOURCE = REPO / "tools" / "export_source.py"
CORE_DELTA = REPO / "tools" / "jozz_core_delta.py"
REPO_HYGIENE = REPO / "tools" / "repo_hygiene.py"
JV_GATE = REPO / "tools" / "jv_gate.py"
EVIDENCE_RUNNER = REPO / "tools" / "evidence" / "run_regression_tests.py"


def load_evidence_runner():
    spec = importlib.util.spec_from_file_location("jv_evidence_runner_for_test", EVIDENCE_RUNNER)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot import run_regression_tests.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run(*args: str, cwd: Path, check: bool = False) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        list(args), cwd=cwd, text=True, capture_output=True, encoding="utf-8"
    )
    if check and result.returncode != 0:
        raise AssertionError(
            f"command failed ({result.returncode}): {' '.join(args)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def load_docs_audit():
    spec = importlib.util.spec_from_file_location("jv_docs_audit_for_test", DOCS_AUDIT)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot import docs_audit.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class TemporaryGitRepo:
    def __init__(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="jv_hygiene_")
        self.root = Path(self._tmp.name)
        run("git", "init", "-q", cwd=self.root, check=True)
        run("git", "config", "user.name", "JV Hygiene Test", cwd=self.root, check=True)
        run("git", "config", "user.email", "jv-hygiene@example.invalid", cwd=self.root, check=True)

    def close(self) -> None:
        self._tmp.cleanup()

    def write(self, relative: str, content: str | bytes) -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content, encoding="utf-8", newline="\n")
        return path

    def commit(self, message: str) -> str:
        run("git", "add", "-A", cwd=self.root, check=True)
        env = os.environ.copy()
        env.update(
            {
                "GIT_AUTHOR_DATE": "2026-08-04T12:00:00+00:00",
                "GIT_COMMITTER_DATE": "2026-08-04T12:00:00+00:00",
            }
        )
        result = subprocess.run(
            ["git", "commit", "-q", "-m", message],
            cwd=self.root,
            text=True,
            capture_output=True,
            encoding="utf-8",
            env=env,
        )
        if result.returncode != 0:
            raise AssertionError(result.stdout + result.stderr)
        return run("git", "rev-parse", "HEAD", cwd=self.root, check=True).stdout.strip()


class HygieneToolsTest(unittest.TestCase):
    def test_docs_audit_real_tree_is_green(self) -> None:
        result = run(sys.executable, str(DOCS_AUDIT), cwd=REPO)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("docs-audit: OK", result.stdout)

    def test_repo_hygiene_real_tree_is_green(self) -> None:
        result = run(sys.executable, str(REPO_HYGIENE), cwd=REPO)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("repo-hygiene: OK", result.stdout)

    def test_jv_gate_profiles_are_discoverable(self) -> None:
        result = run(sys.executable, str(JV_GATE), "deep", "--list", cwd=REPO)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("proposal completeness", result.stdout)
        self.assertIn("documentation authority and routing", result.stdout)
        shard_keys = re.findall(r"evidence regressions shard ([a-k])", result.stdout)
        self.assertEqual(shard_keys, list("abcdefghijk"))

    def test_jv_gate_resume_is_bounded_and_token_locked(self) -> None:
        first = run(
            sys.executable, str(JV_GATE), "quick", "--stop-after", "1", cwd=REPO
        )
        self.assertEqual(first.returncode, 0, first.stdout + first.stderr)
        self.assertIn("jv-gate: PARTIAL OK", first.stdout)
        match = re.search(r"Proposal token: ([0-9a-f]{40}:[0-9a-f]{40})", first.stdout)
        self.assertIsNotNone(match, first.stdout)
        token = match.group(1)
        self.assertIn("--start-at 2", first.stdout)
        self.assertIn(f"--proposal-token {token}", first.stdout)

        missing = run(
            sys.executable, str(JV_GATE), "quick", "--start-at", "2", "--stop-after", "2", cwd=REPO
        )
        self.assertNotEqual(missing.returncode, 0)
        self.assertIn("requires --proposal-token", missing.stderr)

        wrong = run(
            sys.executable, str(JV_GATE), "quick", "--start-at", "2", "--stop-after", "2",
            "--proposal-token", "0" * 40 + ":" + "0" * 40, cwd=REPO
        )
        self.assertNotEqual(wrong.returncode, 0)
        self.assertIn("does not match", wrong.stderr)

        resumed = run(
            sys.executable, str(JV_GATE), "quick", "--start-at", "2", "--stop-after", "2",
            "--proposal-token", token, cwd=REPO
        )
        self.assertEqual(resumed.returncode, 0, resumed.stdout + resumed.stderr)
        self.assertIn("gates 2..2 passed", resumed.stdout)

    def test_evidence_runner_partitions_the_suite(self) -> None:
        result = run(sys.executable, str(EVIDENCE_RUNNER), "--list", cwd=REPO)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("a: T1-T3 (one process/test, timeout 300s)", result.stdout)
        self.assertIn("e: T12-T14", result.stdout)
        self.assertIn("k: T26+ and extras", result.stdout)
        self.assertIn("test_T26c_non_regular_file_is_rejected", result.stdout)

    def test_evidence_runner_reports_a_hard_timeout(self) -> None:
        runner = load_evidence_runner()
        command = [sys.executable, "-c", "import time; time.sleep(30)"]
        rc = runner.run_command(command, REPO, 1)
        self.assertEqual(rc, 124)


    def test_repo_hygiene_rejects_artifacts_and_windows_collisions(self) -> None:
        repo = TemporaryGitRepo()
        self.addCleanup(repo.close)
        repo.write("tools/repo_hygiene.py", REPO_HYGIENE.read_bytes())
        repo.write("README.md", "root\n")
        repo.write("samples/shaders/.gitkeep", b"")
        repo.commit("clean baseline")

        green = run(sys.executable, str(repo.root / "tools/repo_hygiene.py"), cwd=repo.root)
        self.assertEqual(green.returncode, 0, green.stdout + green.stderr)

        repo.write("build/generated.log", "generated\n")
        repo.write("docs/Readme.md", "one\n")
        repo.write("docs/README.md", "two\n")
        repo.commit("inject repository dirt")

        red = run(sys.executable, str(repo.root / "tools/repo_hygiene.py"), cwd=repo.root)
        self.assertNotEqual(red.returncode, 0)
        self.assertIn("ARTEFAKT", red.stderr)
        self.assertIn("WINDOWS CASE COLLISION", red.stderr)

    def test_repo_hygiene_rejects_duplicate_tracked_blobs(self) -> None:
        repo = TemporaryGitRepo()
        self.addCleanup(repo.close)
        repo.write("tools/repo_hygiene.py", REPO_HYGIENE.read_bytes())
        repo.write("a.txt", "same\n")
        repo.write("b.txt", "same\n")
        repo.commit("duplicate blobs")

        red = run(sys.executable, str(repo.root / "tools/repo_hygiene.py"), cwd=repo.root)
        self.assertNotEqual(red.returncode, 0)
        self.assertIn("DUPLIKAT", red.stderr)

    def test_findings_catalog_is_complete_and_idempotent(self) -> None:
        audit = load_docs_audit()
        catalog = audit.render_findings_catalog()
        ledger = json.loads((REPO / "docs/KOLA_FINDINGS.json").read_text("utf-8"))
        for finding_id in ledger["findings"]:
            self.assertEqual(catalog.count(f"**{finding_id}**"), 1)
        source = "# Doc\n"
        once = audit.replace_findings_catalog(source, catalog)
        twice = audit.replace_findings_catalog(once, catalog)
        self.assertEqual(once, twice)

    def test_export_is_deterministic_and_uses_committed_blobs(self) -> None:
        repo = TemporaryGitRepo()
        self.addCleanup(repo.close)
        repo.write("tools/export_source.py", EXPORT_SOURCE.read_bytes())
        repo.write("README.md", "committed\n")
        repo.write("src/value.txt", "v1\n")
        commit = repo.commit("baseline")

        # Dirty tracked bytes and ignored-looking local data must not enter the ZIP.
        repo.write("src/value.txt", "DIRTY\n")
        repo.write("build/local_session.json", "secret-local-state\n")
        first = repo.root.parent / f"{repo.root.name}-a.zip"
        second = repo.root.parent / f"{repo.root.name}-b.zip"
        self.addCleanup(first.unlink, missing_ok=True)
        self.addCleanup(second.unlink, missing_ok=True)

        for output in (first, second):
            result = run(
                sys.executable,
                str(repo.root / "tools/export_source.py"),
                str(output),
                cwd=repo.root,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("uncommitted changes were intentionally excluded", result.stdout)

        self.assertEqual(hashlib.sha256(first.read_bytes()).digest(), hashlib.sha256(second.read_bytes()).digest())
        self.assertEqual(first.read_bytes(), second.read_bytes())
        with zipfile.ZipFile(first) as archive:
            names = set(archive.namelist())
            self.assertNotIn("box3d/build/local_session.json", names)
            self.assertEqual(archive.read("box3d/src/value.txt"), b"v1\n")
            manifest = json.loads(archive.read("box3d/SOURCE_PACKAGE_MANIFEST.json"))
            self.assertEqual(manifest["commit"], commit)

    def test_core_delta_covers_untracked_core_files(self) -> None:
        repo = TemporaryGitRepo()
        self.addCleanup(repo.close)
        repo.write("tools/jozz_core_delta.py", CORE_DELTA.read_bytes())
        repo.write("docs/owner.md", "# Owner\n")
        repo.write("src/core.c", "int baseline;\n")
        base = repo.commit("baseline")

        repo.write("src/core.c", "int baseline;\nint wheel_patch;\n")
        manifest = {
            "schema": 1,
            "scope": ["src", "include"],
            "comparison_base": base,
            "upstream_reference": base,
            "patches": [
                {
                    "patch_id": "TEST-001",
                    "class": "X",
                    "status": "test",
                    "question": "test bounded ownership",
                    "owner": "docs/owner.md",
                    "files": ["src/core.c"],
                    "activation_boundary": "test only",
                    "zero_delta_contract": "test only",
                    "upstream_update_dry_run": "test only",
                }
            ],
        }
        repo.write("docs/JOZZ_CORE_PATCHES.json", json.dumps(manifest, indent=2) + "\n")
        repo.commit("owned core patch")

        tool = repo.root / "tools/jozz_core_delta.py"
        green = run(sys.executable, str(tool), cwd=repo.root)
        self.assertEqual(green.returncode, 0, green.stdout + green.stderr)
        self.assertIn("core-delta: OK", green.stdout)

        repo.write("src/unowned.c", "int unowned;\n")
        red = run(sys.executable, str(tool), cwd=repo.root)
        self.assertNotEqual(red.returncode, 0)
        self.assertIn("unowned core files: src/unowned.c", red.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
