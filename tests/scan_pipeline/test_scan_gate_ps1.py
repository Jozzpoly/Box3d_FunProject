from pathlib import Path
import unittest

GATE = Path(__file__).parents[2] / "tools" / "gate.ps1"


class ScanGatePowerShellContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = GATE.read_text(encoding="utf-8")

    def test_fresh_worktree_is_configured_before_build(self) -> None:
        self.assertIn("cmake --preset windows", self.text)
        self.assertIn("CMakeCache.txt", self.text)
        self.assertLess(
            self.text.index("cmake --preset windows"),
            self.text.index("cmake --build --preset windows-debug"),
        )

    def test_cmake_exit_code_is_a_hard_gate(self) -> None:
        self.assertIn("$buildExit = $LASTEXITCODE", self.text)
        self.assertIn("if ($buildExit -ne 0)", self.text)

    def test_executables_are_rooted_and_must_exist(self) -> None:
        self.assertIn('Join-Path $buildRoot "bin\\Debug\\jozz_vehicle_validation.exe"', self.text)
        self.assertIn("build exited 0 but did not produce", self.text)


if __name__ == "__main__":
    unittest.main()
