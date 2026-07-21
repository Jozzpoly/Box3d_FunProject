from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
VERIFY = ROOT / "tools" / "scan_pipeline" / "scan_surface_evidence_verify.py"
FIXTURE_TEST = Path(__file__).with_name("test_scan_surface_evidence.py")

spec = importlib.util.spec_from_file_location("surface_fixture_verify", FIXTURE_TEST)
assert spec and spec.loader
fixture = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = fixture
spec.loader.exec_module(fixture)


class ScanSurfaceEvidenceVerifyTests(unittest.TestCase):
    def test_cli_verifies_complete_pack(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = fixture.build(Path(temporary))
            result = subprocess.run(
                [sys.executable, str(VERIFY), str(output)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("scan_surface_evidence_verify: OK", result.stdout)

    def test_cli_rejects_tampered_pack(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = fixture.build(Path(temporary))
            binary = output / "surface.bin"
            binary.write_bytes(binary.read_bytes() + b"x")
            result = subprocess.run(
                [sys.executable, str(VERIFY), str(output)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("ERROR", result.stderr)

    def test_cli_rejects_missing_pack(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run(
                [sys.executable, str(VERIFY), str(Path(temporary) / "missing")],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("ERROR", result.stderr)


if __name__ == "__main__":
    unittest.main()
