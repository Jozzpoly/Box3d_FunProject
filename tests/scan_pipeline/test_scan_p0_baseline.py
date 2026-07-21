from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

MODULE_PATH = Path(__file__).resolve().parents[2] / "tools" / "scan_pipeline" / "scan_p0_baseline.py"
SPEC = importlib.util.spec_from_file_location("scan_p0_baseline", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
scan_p0 = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(scan_p0)


class ScanP0BaselineTests(unittest.TestCase):
    def test_sha256_file_is_stable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "sample.bin"
            path.write_bytes(b"jozz-scan-baseline")
            self.assertEqual(
                scan_p0.sha256_file(path),
                "0971299ed1d4336810d8e3d14c82aa778d7fc8f92fef760a6c40a581b557da13",
            )

    def test_scan_must_live_under_local_assets_scans(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            allowed = root / "local_assets" / "scans" / "Model_skanu.rar"
            allowed.parent.mkdir(parents=True)
            allowed.write_bytes(b"rar")
            resolved, relative = scan_p0.ensure_scan_path(root, allowed)
            self.assertEqual(resolved, allowed.resolve())
            self.assertEqual(relative, "local_assets/scans/Model_skanu.rar")

            outside = root / "Model_skanu.rar"
            outside.write_bytes(b"rar")
            with self.assertRaises(scan_p0.BaselineError):
                scan_p0.ensure_scan_path(root, outside)

    def test_report_privacy_rejects_absolute_repo_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            safe = {"scan": "local_assets/scans/Model_skanu.rar"}
            scan_p0.assert_report_private(safe, root)
            unsafe = {"scan": str(root / "local_assets" / "scans" / "Model_skanu.rar")}
            with self.assertRaises(scan_p0.BaselineError):
                scan_p0.assert_report_private(unsafe, root)

    def test_summary_uses_last_gate_line(self) -> None:
        lines = [
            "BRAMKA: FAIL @ build",
            "noise",
            "BRAMKA: build 3/3 OK - walidator OK - test PASS - smoke 0 err",
        ]
        self.assertEqual(
            scan_p0.sanitize_summary_line(lines),
            "BRAMKA: build 3/3 OK - walidator OK - test PASS - smoke 0 err",
        )

    def test_gate_summary_requires_all_success_tokens(self) -> None:
        self.assertTrue(
            scan_p0.gate_summary_is_pass(
                "BRAMKA: build 3/3 OK - walidator OK - test PASS - smoke 0 err"
            )
        )
        self.assertFalse(scan_p0.gate_summary_is_pass("BRAMKA: build 3/3 OK"))
        self.assertFalse(scan_p0.gate_summary_is_pass(None))

    def test_output_must_remain_under_build(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            inside = scan_p0.ensure_output_root(
                root, Path("build/scan_pipeline/p0_baseline")
            )
            self.assertEqual(inside, root / "build" / "scan_pipeline" / "p0_baseline")
            with self.assertRaises(scan_p0.BaselineError):
                scan_p0.ensure_output_root(root, root / "outside")

    def test_stale_baseline_artifact_is_not_copied(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "old.txt"
            destination = root / "copy.txt"
            source.write_text("old", encoding="utf-8")
            previous = scan_p0._file_stamp(source)
            self.assertIsNone(
                scan_p0._copy_if_updated(
                    source, destination, previous_stamp=previous
                )
            )
            self.assertFalse(destination.exists())

    def test_updated_baseline_artifact_is_copied(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "baseline.txt"
            destination = root / "copy.txt"
            source.write_text("before", encoding="utf-8")
            previous = scan_p0._file_stamp(source)
            source.write_text("after gate", encoding="utf-8")
            copied = scan_p0._copy_if_updated(
                source, destination, previous_stamp=previous
            )
            self.assertIsNotNone(copied)
            self.assertEqual(destination.read_text(encoding="utf-8"), "after gate")

    def test_report_writer_does_not_store_absolute_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary).resolve()
            output = root / "build"
            output.mkdir()
            report = {
                "format": scan_p0.FORMAT_ID,
                "status": "PASS",
                "failureReasons": [],
                "repository": {
                    "branch": "photogrammetry/import-v2-foundation",
                    "commitSha": "a" * 40,
                    "expectedBaseBranch": scan_p0.DEFAULT_BASE_BRANCH,
                    "expectedBaseCommit": scan_p0.DEFAULT_BASE_COMMIT,
                },
                "build": {"preset": "windows-debug"},
                "scanArchive": {
                    "relativePath": "local_assets/scans/Model_skanu.rar",
                    "sha256": "b" * 64,
                    "sizeBytes": 10,
                },
                "git": {"cleanBefore": True, "cleanAfter": True},
                "gate": {
                    "status": "PASS",
                    "durationSeconds": 1.0,
                    "summaryLine": "BRAMKA: build 3/3 OK - walidator OK - test PASS - smoke 0 err",
                },
                "artifacts": {},
            }
            scan_p0._write_reports(output, report, root)
            encoded = (output / "p0_baseline.json").read_text(encoding="utf-8")
            self.assertNotIn(str(root), encoded)
            self.assertNotIn("rootMarker", encoded)
            json.loads(encoded)


if __name__ == "__main__":
    unittest.main()
