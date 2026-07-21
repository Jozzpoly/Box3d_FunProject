from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HELPER_PATH = Path(__file__).with_name("test_scan_preview_pack.py")
helper_spec = importlib.util.spec_from_file_location("_scan_preview_fixture", HELPER_PATH)
assert helper_spec and helper_spec.loader
helper = importlib.util.module_from_spec(helper_spec)
sys.modules[helper_spec.name] = helper
helper_spec.loader.exec_module(helper)

VERIFY = (
    Path(__file__).parents[2]
    / "tools"
    / "scan_pipeline"
    / "scan_preview_pack_verify.py"
)


class ScanPreviewPackVerifyTests(unittest.TestCase):
    def test_cli_verifies_complete_preview(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = helper.fixture(root)
            output = helper.preview.build_preview_pack(
                bundle=bundle,
                source_root=source,
                output_root=root / "previews",
            )
            result = subprocess.run(
                [sys.executable, str(VERIFY), str(output)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("scan_preview_pack_verify: OK", result.stdout)

    def test_cli_rejects_missing_preview(self) -> None:
        result = subprocess.run(
            [sys.executable, str(VERIFY), "missing-preview-pack"],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("scan_preview_pack_verify: ERROR", result.stderr)

    def test_cli_rejects_tampered_preview(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = helper.fixture(root)
            output = helper.preview.build_preview_pack(
                bundle=bundle,
                source_root=source,
                output_root=root / "previews",
            )
            tile = output / "tiles" / "tile_000.bin"
            tile.write_bytes(tile.read_bytes() + b"x")
            result = subprocess.run(
                [sys.executable, str(VERIFY), str(output)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("scan_preview_pack_verify: ERROR", result.stderr)


if __name__ == "__main__":
    unittest.main()
