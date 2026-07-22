from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).parents[2]
RUNNER = ROOT / "tools" / "scan_pipeline" / "run_real_terrain_flow.ps1"


class ScanRealTerrainFlowPowerShellContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = RUNNER.read_text(encoding="utf-8")

    def test_runner_is_one_resumable_owner_entrypoint(self) -> None:
        for token in (
            "scan_real_terrain_flow.py",
            '"continue"',
            '"--pipeline-root"',
            '"--source-root"',
            '"--launch"',
            "resumable / fail-closed",
            "$LASTEXITCODE",
        ):
            self.assertIn(token, self.text)

    def test_source_root_is_optional_after_first_persisted_run(self) -> None:
        self.assertIn("[string]$SourceRoot", self.text)
        self.assertIn("IsNullOrWhiteSpace($SourceRoot)", self.text)
        self.assertNotIn("Parameter(Mandatory", self.text)

    def test_runner_does_not_copy_or_request_hashes(self) -> None:
        for forbidden in (
            "expected-sha256",
            "bundle-sha256",
            "sourceRevisionId",
            "proposal_sha256",
        ):
            self.assertNotIn(forbidden, self.text)


if __name__ == "__main__":
    unittest.main()
