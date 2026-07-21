from __future__ import annotations

import importlib.util
import math
from pathlib import Path
import sys
import unittest

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_frames.py"
spec = importlib.util.spec_from_file_location("scan_frames_tested", MODULE_PATH)
assert spec and spec.loader
scan_frames = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = scan_frames
spec.loader.exec_module(scan_frames)


def valid_contract() -> dict[str, object]:
    return {
        "schema": scan_frames.SCHEMA,
        "schemaVersion": scan_frames.SCHEMA_VERSION,
        "confirmed": True,
        "sourceFrame": {
            "unitsPerMeter": 2.0,
            "handedness": "right",
            "axisRoles": {"right": "+X", "forward": "+Y", "up": "+Z"},
        },
        "labFrame": {
            "handedness": "right",
            "axisRoles": {"right": "+X", "forward": "-Z", "up": "+Y"},
        },
        "sourceToLab": {
            "axisMatrix": [[1, 0, 0], [0, 0, 1], [0, -1, 0]],
            "orientationChange": "preserve",
            "mirrorApproved": False,
            "localOriginSource": [100.0, 200.0, 300.0],
        },
    }


class ScanFrameTests(unittest.TestCase):
    def test_valid_contract_normalizes_and_round_trips(self) -> None:
        contract = valid_contract()
        normalized = scan_frames.validate_frame_contract(contract)
        self.assertTrue(normalized["confirmed"])
        self.assertEqual(normalized["sourceToLab"]["determinant"], 1)
        source = [104.0, 206.0, 308.0]
        lab = scan_frames.source_to_lab_point(contract, source)
        self.assertEqual(lab, [2.0, 4.0, -3.0])
        self.assertEqual(scan_frames.lab_to_source_point(contract, lab), source)

    def test_matrix_must_match_declared_axis_roles(self) -> None:
        contract = valid_contract()
        contract["sourceToLab"]["axisMatrix"] = [[1, 0, 0], [0, 0, 1], [0, 1, 0]]
        contract["sourceToLab"]["orientationChange"] = "mirror"
        contract["sourceToLab"]["mirrorApproved"] = True
        with self.assertRaises(scan_frames.FrameContractError):
            scan_frames.validate_frame_contract(contract)

    def test_mirror_requires_explicit_approval(self) -> None:
        contract = valid_contract()
        contract["labFrame"] = {
            "handedness": "left",
            "axisRoles": {"right": "+X", "forward": "+Z", "up": "+Y"},
        }
        contract["sourceToLab"] = {
            "axisMatrix": [[1, 0, 0], [0, 0, 1], [0, 1, 0]],
            "orientationChange": "mirror",
            "mirrorApproved": False,
            "localOriginSource": [0.0, 0.0, 0.0],
        }
        with self.assertRaises(scan_frames.FrameContractError):
            scan_frames.validate_frame_contract(contract)
        contract["sourceToLab"]["mirrorApproved"] = True
        normalized = scan_frames.validate_frame_contract(contract)
        self.assertEqual(normalized["sourceToLab"]["determinant"], -1)

    def test_duplicate_axis_roles_are_rejected(self) -> None:
        contract = valid_contract()
        contract["sourceFrame"]["axisRoles"]["up"] = "+Y"
        with self.assertRaises(scan_frames.FrameContractError):
            scan_frames.validate_frame_contract(contract)

    def test_handedness_must_match_semantic_basis(self) -> None:
        contract = valid_contract()
        contract["sourceFrame"]["handedness"] = "left"
        with self.assertRaises(scan_frames.FrameContractError):
            scan_frames.validate_frame_contract(contract)

    def test_units_must_be_positive_and_finite(self) -> None:
        for invalid in (0.0, -1.0, math.inf, math.nan):
            contract = valid_contract()
            contract["sourceFrame"]["unitsPerMeter"] = invalid
            with self.subTest(invalid=invalid), self.assertRaises(scan_frames.FrameContractError):
                scan_frames.validate_frame_contract(contract)

    def test_hash_is_independent_of_dictionary_insertion_order(self) -> None:
        contract = valid_contract()
        reordered = {
            "sourceToLab": contract["sourceToLab"],
            "labFrame": contract["labFrame"],
            "confirmed": True,
            "schemaVersion": scan_frames.SCHEMA_VERSION,
            "sourceFrame": contract["sourceFrame"],
            "schema": scan_frames.SCHEMA,
        }
        self.assertEqual(
            scan_frames.contract_sha256(contract),
            scan_frames.contract_sha256(reordered),
        )

    def test_nonfinite_origin_is_rejected(self) -> None:
        contract = valid_contract()
        contract["sourceToLab"]["localOriginSource"] = [0.0, math.nan, 0.0]
        with self.assertRaises(scan_frames.FrameContractError):
            scan_frames.validate_frame_contract(contract)


if __name__ == "__main__":
    unittest.main()
