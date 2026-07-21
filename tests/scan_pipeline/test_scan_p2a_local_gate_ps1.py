from pathlib import Path
import unittest

GATE = (
    Path(__file__).parents[2]
    / "tools"
    / "scan_pipeline"
    / "run_p2a_local_gate.ps1"
)


class ScanP2ALocalGatePowerShellContractTests(unittest.TestCase):
    def test_gate_orders_all_code_checks_and_stops_on_failures(self) -> None:
        text = GATE.read_text(encoding="utf-8")
        contracts = 'run_p1_contracts.py'
        configure = 'cmake --preset windows'
        samples_build = 'cmake --build --preset windows-debug --target samples'
        vehicle_gate = '.\\tools\\gate.ps1'

        for token in (
            contracts,
            configure,
            samples_build,
            vehicle_gate,
            '$LASTEXITCODE',
            'build\\bin\\Debug\\samples.exe',
            'P2A_LOCAL_CODE_GATE_PASS',
        ):
            self.assertIn(token, text)

        self.assertLess(text.index(contracts), text.index(configure))
        self.assertLess(text.index(configure), text.index(samples_build))
        self.assertLess(text.index(samples_build), text.index(vehicle_gate))
        self.assertIn('throw "$Label failed with exit code $code"', text)
        self.assertIn('Test-Path -LiteralPath $samples -PathType Leaf', text)
        self.assertIn('does not assert TERRAIN_VISIBLE_PASS', text)


if __name__ == "__main__":
    unittest.main()
