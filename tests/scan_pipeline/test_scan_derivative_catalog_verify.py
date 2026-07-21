from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
VERIFY = ROOT / "tools" / "scan_pipeline" / "scan_derivative_catalog_verify.py"
CATALOG_TEST = Path(__file__).with_name("test_scan_derivative_catalog.py")

spec = importlib.util.spec_from_file_location("catalog_fixture_verify", CATALOG_TEST)
assert spec and spec.loader
fixture = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = fixture
spec.loader.exec_module(fixture)


class ScanDerivativeCatalogVerifyTests(unittest.TestCase):
    def test_cli_verifies_complete_catalog(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            preview = fixture.preview_fixture.build_fixture_preview(root / "preview")
            output = fixture.catalog.write_catalog(
                preview=preview,
                output=root / "catalog.json",
            )
            result = subprocess.run(
                [sys.executable, str(VERIFY), str(output)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("scan_derivative_catalog_verify: OK", result.stdout)

    def test_cli_rejects_tampered_catalog(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            preview = fixture.preview_fixture.build_fixture_preview(root / "preview")
            output = fixture.catalog.write_catalog(
                preview=preview,
                output=root / "catalog.json",
            )
            document = fixture.catalog._strict_json(output)
            document["runtimePolicies"]["physicsInterestCenter"] = "CAMERA"
            output.write_bytes(fixture.catalog._canonical_json_bytes(document))
            result = subprocess.run(
                [sys.executable, str(VERIFY), str(output)],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("ERROR", result.stderr)

    def test_cli_rejects_missing_catalog(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run(
                [sys.executable, str(VERIFY), str(Path(temporary) / "missing.json")],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("ERROR", result.stderr)


if __name__ == "__main__":
    unittest.main()
