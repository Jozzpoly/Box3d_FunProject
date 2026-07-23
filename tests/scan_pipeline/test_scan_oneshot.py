"""Regression coverage for the one-command scan import orchestrator.

These tests exercise the exact wiring that previously crashed at runtime with an
undefined ``_discover_single_bundle`` name. Every subprocess call is mocked, so
no real pipeline runs, nothing is built, and nothing is written outside a
temporary directory.
"""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import types
import unittest
from unittest import mock

_MODULE_PATH = (
    Path(__file__).resolve().parents[2]
    / "tools"
    / "scan_pipeline"
    / "scan_oneshot.py"
)
_spec = importlib.util.spec_from_file_location("_scan_oneshot_under_test", _MODULE_PATH)
assert _spec and _spec.loader
oneshot = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(oneshot)


class ParseBundlePathTests(unittest.TestCase):
    def test_parses_bundle_from_pass_line(self) -> None:
        stdout = (
            "scan_owner_gate: P1B_BUNDLE_PASS | "
            "bundle=/x/build/scan_pipeline/bundles/bundle-abc123 "
            "receipt=/x/build/scan_pipeline/p1b_owner_gate_receipt.local.json\n"
        )
        self.assertEqual(
            oneshot._parse_bundle_path(stdout),
            Path("/x/build/scan_pipeline/bundles/bundle-abc123"),
        )

    def test_parses_bundle_from_technical_pass_line(self) -> None:
        stdout = (
            "scan_owner_gate: TECHNICAL_PASS / PRIVACY_REVIEW_REQUIRED | "
            "bundle=/data/bundles/b1 receipt=/data/r.json\n"
        )
        self.assertEqual(oneshot._parse_bundle_path(stdout), Path("/data/bundles/b1"))

    def test_missing_bundle_line_raises(self) -> None:
        with self.assertRaises(oneshot.OneShotError):
            oneshot._parse_bundle_path("no parseable output here\n")


class PersistPackEnvTests(unittest.TestCase):
    def test_pins_env_from_active_preview(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            pack = root / "previews" / "source-preview-xyz"
            active = root / "ACTIVE_PREVIEW.json"
            active.write_text(json.dumps({"previewPath": str(pack)}), encoding="utf-8")
            success = types.SimpleNamespace(returncode=0, stdout="", stderr="")
            with mock.patch.dict(oneshot.os.environ, {}, clear=False), mock.patch.object(
                oneshot.subprocess, "run", return_value=success
            ), contextlib.redirect_stdout(io.StringIO()):
                oneshot._persist_pack_env(active)
                self.assertEqual(
                    oneshot.os.environ.get("JOZZ_SCAN_PREVIEW_PACK"),
                    str(pack.resolve()),
                )


class OrchestrationReachesPreviewStepTests(unittest.TestCase):
    """Executes ``main()`` through the previously-broken bundle-discovery line.

    ``--reviewed-privacy`` here only reaches the synthetic step-4 code path with
    fully mocked tools; it is not a real shareable-privacy acknowledgement.
    """

    def test_reaches_flow_with_the_exact_parsed_bundle(self) -> None:
        recorded: list[list[str]] = []
        bundle_name = "bundle-deadbeefcafe"

        def fake_run(argv, capture_output=True, text=True, **kwargs):
            recorded.append([str(part) for part in argv])
            if any("scan_owner_gate.py" in str(part) for part in argv):
                stdout = (
                    "scan_owner_gate: P1B_BUNDLE_PASS | "
                    f"bundle=/pipe/bundles/{bundle_name} "
                    "receipt=/pipe/p1b_owner_gate_receipt.local.json\n"
                )
            else:
                stdout = ""
            return types.SimpleNamespace(returncode=0, stdout=stdout, stderr="")

        with tempfile.TemporaryDirectory() as dataset, tempfile.TemporaryDirectory() as pipe:
            with mock.patch.object(
                oneshot.subprocess, "run", side_effect=fake_run
            ), contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(
                io.StringIO()
            ):
                code = oneshot.main(
                    [
                        "--dataset", dataset,
                        "--pipeline-root", pipe,
                        "--reviewed-privacy",
                        "--no-launch",
                    ]
                )

        self.assertEqual(code, 0)
        flow_calls = [
            call
            for call in recorded
            if any("scan_real_terrain_flow.py" in part for part in call)
        ]
        self.assertTrue(flow_calls, "preview-pack flow was never reached (step 4 broke)")
        argv = flow_calls[0]
        self.assertIn("--bundle", argv)
        self.assertIn(bundle_name, argv[argv.index("--bundle") + 1])


if __name__ == "__main__":
    unittest.main()
