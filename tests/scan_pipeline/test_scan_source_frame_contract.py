from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import types
import unittest

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_source_frame_contract.py"
spec = importlib.util.spec_from_file_location("scan_source_frame_contract_tested", MODULE_PATH)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


class ScanSourceFrameContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.original_loader = module._load_scan_frames
        module._load_scan_frames = lambda: types.SimpleNamespace(
            validate_frame_contract=self._normalize
        )

    def tearDown(self) -> None:
        module._load_scan_frames = self.original_loader

    @staticmethod
    def _normalize(contract):
        result = dict(contract)
        result["sourceToLab"] = dict(contract["sourceToLab"])
        result["sourceToLab"]["determinant"] = module._determinant(
            contract["sourceToLab"]["axisMatrix"]
        )
        return result

    def test_build_derives_expected_matrix_without_confirming(self) -> None:
        contract = module.build_contract(
            source_units_per_meter=2.0,
            source_axis_roles={"right": "+X", "forward": "+Y", "up": "+Z"},
            lab_axis_roles={"right": "+X", "forward": "-Z", "up": "+Y"},
            local_origin_source=[100.0, 200.0, 300.0],
            confirmed=False,
            mirror_approved=False,
        )
        self.assertFalse(contract["confirmed"])
        self.assertEqual(
            contract["sourceToLab"]["axisMatrix"],
            [[1, 0, 0], [0, 0, 1], [0, -1, 0]],
        )
        self.assertEqual(contract["sourceToLab"]["orientationChange"], "preserve")
        self.assertEqual(contract["sourceFrame"]["handedness"], "right")

    def test_mirror_requires_explicit_approval(self) -> None:
        with self.assertRaises(module.SourceFrameCliError):
            module.build_contract(
                source_units_per_meter=1.0,
                source_axis_roles={"right": "+X", "forward": "+Y", "up": "+Z"},
                lab_axis_roles={"right": "+X", "forward": "+Z", "up": "+Y"},
                local_origin_source=[0.0, 0.0, 0.0],
                confirmed=False,
                mirror_approved=False,
            )

    def test_duplicate_axis_roles_are_rejected(self) -> None:
        with self.assertRaises(module.SourceFrameCliError):
            module.build_contract(
                source_units_per_meter=1.0,
                source_axis_roles={"right": "+X", "forward": "+Y", "up": "+Y"},
                lab_axis_roles={"right": "+X", "forward": "-Z", "up": "+Y"},
                local_origin_source=[0.0, 0.0, 0.0],
                confirmed=False,
                mirror_approved=False,
            )

    def test_writer_refuses_silent_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "frame.json"
            module.write_contract(path, {"a": 1}, force=False)
            with self.assertRaises(module.SourceFrameCliError):
                module.write_contract(path, {"a": 2}, force=False)
            module.write_contract(path, {"a": 2}, force=True)
            self.assertIn('"a": 2', path.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
