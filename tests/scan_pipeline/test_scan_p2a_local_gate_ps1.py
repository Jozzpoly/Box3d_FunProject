from pathlib import Path
import unittest

GATE = (
    Path(__file__).parents[2]
    / "tools"
    / "scan_pipeline"
    / "run_p2a_local_gate.ps1"
)


class ScanP2ALocalGatePowerShellContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = GATE.read_text(encoding="utf-8")

    def test_gate_orders_all_code_checks_and_stops_on_failures(self) -> None:
        contracts = "run_p1_contracts.py"
        configure = "cmake --preset windows"
        samples_build = "cmake --build --preset windows-debug --target samples"
        vehicle_gate = "tools\\gate.ps1"

        for token in (
            contracts,
            configure,
            samples_build,
            vehicle_gate,
            "$LASTEXITCODE",
            "build\\bin\\Debug\\samples.exe",
            "P2A_LOCAL_CODE_GATE_PASS",
        ):
            self.assertIn(token, self.text)

        self.assertLess(self.text.index(contracts), self.text.index(configure))
        self.assertLess(self.text.index(configure), self.text.index(samples_build))
        self.assertLess(self.text.index(samples_build), self.text.index(vehicle_gate))
        self.assertIn('throw "$Label failed with exit code $code"', self.text)
        self.assertIn("Test-Path -LiteralPath $samples -PathType Leaf", self.text)
        self.assertIn("does not assert TERRAIN_VISIBLE_PASS", self.text)

    def test_vehicle_gate_isolated_from_successful_native_stderr(self) -> None:
        for token in (
            "$powerShellHost",
            "Start-Process",
            "-Wait",
            "-PassThru",
            "-NoNewWindow",
            "$process.ExitCode",
            "successful smoke summary",
        ):
            self.assertIn(token, self.text)

        self.assertNotIn('& ".\\tools\\gate.ps1"', self.text)
        self.assertIn(
            'throw "Existing vehicle/build/smoke gate failed with exit code $($process.ExitCode)"',
            self.text,
        )


if __name__ == "__main__":
    unittest.main()
