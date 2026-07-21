from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import sys
import unittest

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_world_contracts.py"
spec = importlib.util.spec_from_file_location("scan_world_contracts_tested", MODULE_PATH)
assert spec and spec.loader
contracts = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = contracts
spec.loader.exec_module(contracts)


def frame_contract() -> dict[str, object]:
    return {
        "schema": contracts.scan_frames.SCHEMA,
        "schemaVersion": contracts.scan_frames.SCHEMA_VERSION,
        "confirmed": True,
        "sourceFrame": {
            "unitsPerMeter": 1.0,
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
            "localOriginSource": [0.0, 0.0, 0.0],
        },
    }


def source(tile_id: int, suffix: str, digest_char: str) -> dict[str, object]:
    return {
        "tileId": tile_id,
        "sourceLabel": f"MipTile_{tile_id}.{suffix}",
        "sha256": digest_char * 64,
        "byteLength": 100 + tile_id,
    }


def inspection() -> dict[str, object]:
    return {
        "schema": contracts.INSPECTION_SCHEMA,
        "schemaVersion": 3,
        "datasetStatus": "compatible-review",
        "automaticEvidenceGate": {"passed": True},
        "glbFiles": [source(1, "glb", "b"), source(0, "glb", "a")],
        "plyFiles": [source(0, "ply", "c"), source(1, "ply", "d")],
        "pairs": [
            {
                "tileId": 1,
                "classification": "review",
                "normalizedCenterDelta": 0.03,
                "maxExtentRelativeError": 0.08,
                "xyOverlapOfSmaller": 0.91,
                "axisPermutationSuspicion": False,
            },
            {
                "tileId": 0,
                "classification": "strong-match",
                "normalizedCenterDelta": 0.001,
                "maxExtentRelativeError": 0.01,
                "xyOverlapOfSmaller": 0.99,
                "axisPermutationSuspicion": False,
            },
        ],
        "warnings": ["fixture warning"],
    }


class ScanWorldContractTests(unittest.TestCase):
    def test_source_package_has_stable_identity_and_sorted_tiles(self) -> None:
        package = contracts.build_source_package(
            package_id="scan/example",
            inspection_report=inspection(),
            frame_contract=frame_contract(),
        )
        self.assertEqual(package["packageId"], "scan/example")
        self.assertTrue(package["revisionId"].startswith("sha256:"))
        self.assertEqual([tile["tileId"] for tile in package["tiles"]], [0, 1])
        self.assertEqual(package["tiles"][0]["stableTileId"], "scan/example/tile/0")
        contracts.validate_source_package(package)

    def test_revision_changes_when_source_content_changes(self) -> None:
        original = contracts.build_source_package(
            package_id="scan/example",
            inspection_report=inspection(),
            frame_contract=frame_contract(),
        )
        changed_report = inspection()
        changed_report["plyFiles"][0]["sha256"] = "e" * 64
        changed = contracts.build_source_package(
            package_id="scan/example",
            inspection_report=changed_report,
            frame_contract=frame_contract(),
        )
        self.assertEqual(original["packageId"], changed["packageId"])
        self.assertNotEqual(original["revisionId"], changed["revisionId"])

    def test_duplicate_or_unpaired_tiles_are_rejected(self) -> None:
        duplicate = inspection()
        duplicate["glbFiles"].append(copy.deepcopy(duplicate["glbFiles"][0]))
        with self.assertRaises(contracts.WorldContractError):
            contracts.build_source_package(
                package_id="scan/example",
                inspection_report=duplicate,
                frame_contract=frame_contract(),
            )
        unpaired = inspection()
        unpaired["plyFiles"].pop()
        with self.assertRaises(contracts.WorldContractError):
            contracts.build_source_package(
                package_id="scan/example",
                inspection_report=unpaired,
                frame_contract=frame_contract(),
            )

    def test_proposal_downgrades_strong_match_to_bounds_only_evidence(self) -> None:
        report = inspection()
        package = contracts.build_source_package(
            package_id="scan/example",
            inspection_report=report,
            frame_contract=frame_contract(),
        )
        proposal = contracts.build_world_import_proposal(
            proposal_id="proposal/example-r1",
            source_package=package,
            inspection_report=report,
        )
        self.assertEqual(proposal["status"], "UNREVIEWED")
        self.assertEqual(proposal["manualDecisions"], [])
        self.assertEqual(proposal["pairEvidence"][0]["classification"], "bounds-strong-match")
        self.assertEqual(proposal["pairEvidence"][0]["evidenceLevel"], "BOUNDS_ONLY")
        self.assertFalse(proposal["capabilities"]["internalGeometryCorrespondencePassed"])
        self.assertFalse(proposal["capabilities"]["acceptedWorldPatchReady"])
        contracts.validate_world_import_proposal(proposal)

    def test_proposal_hash_detects_mutation(self) -> None:
        report = inspection()
        package = contracts.build_source_package(
            package_id="scan/example",
            inspection_report=report,
            frame_contract=frame_contract(),
        )
        proposal = contracts.build_world_import_proposal(
            proposal_id="proposal/example-r1",
            source_package=package,
            inspection_report=report,
        )
        proposal["warnings"].append("mutated")
        with self.assertRaises(contracts.WorldContractError):
            contracts.validate_world_import_proposal(proposal)

    def test_backend_handles_are_rejected(self) -> None:
        report = inspection()
        report["gpuHandle"] = 7
        with self.assertRaises(contracts.WorldContractError):
            contracts.build_source_package(
                package_id="scan/example",
                inspection_report=report,
                frame_contract=frame_contract(),
            )

    def test_invalid_stable_id_is_rejected(self) -> None:
        with self.assertRaises(contracts.WorldContractError):
            contracts.build_source_package(
                package_id="Scan With Spaces",
                inspection_report=inspection(),
                frame_contract=frame_contract(),
            )


if __name__ == "__main__":
    unittest.main()
