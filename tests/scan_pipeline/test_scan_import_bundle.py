from __future__ import annotations

import copy
import importlib.util
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

MODULE_PATH = (
    Path(__file__).parents[2]
    / "tools"
    / "scan_pipeline"
    / "scan_import_bundle.py"
)
spec = importlib.util.spec_from_file_location("scan_import_bundle_tested", MODULE_PATH)
assert spec and spec.loader
bundle = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = bundle
spec.loader.exec_module(bundle)


def frame_contract(*, confirmed: bool = True) -> dict[str, object]:
    return {
        "schema": bundle.scan_frames.SCHEMA,
        "schemaVersion": bundle.scan_frames.SCHEMA_VERSION,
        "confirmed": confirmed,
        "sourceFrame": {
            "handedness": "right",
            "axisRoles": {"right": "+X", "forward": "+Y", "up": "+Z"},
            "unitsPerMeter": 1.0,
        },
        "labFrame": {
            "handedness": "right",
            "axisRoles": {"right": "+X", "forward": "+Y", "up": "+Z"},
        },
        "sourceToLab": {
            "axisMatrix": [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
            "orientationChange": "preserve",
            "mirrorApproved": False,
            "localOriginSource": [1000.0, 2000.0, 3000.0],
        },
    }


def inspection_report(
    *,
    passed: bool = True,
    package_name: str = "private-place-name",
) -> dict[str, object]:
    return {
        "schema": bundle.scan_world_contracts.INSPECTION_SCHEMA,
        "schemaVersion": 3,
        "packageName": package_name,
        "datasetStatus": "compatible-review",
        "automaticEvidenceGate": {"passed": passed},
        "totals": {
            "glbFiles": 2,
            "plyFiles": 2,
            "glbVertices": 6,
            "glbTriangles": 2,
            "plyPoints": 6,
        },
        "geometryQuality": {
            "triangleCountAnalyzed": 2,
            "degenerateTriangleCount": 0,
            "provisionalLargeTriangleCount": 0,
            "provisionalLargeEdgeThresholdSourceUnits": 10.0,
            "maxTriangleEdgeSourceUnits": 3.0,
            "maxTriangleAreaSourceUnitsSquared": 2.5,
        },
        "evidenceGrid": {
            "width": 16,
            "height": 16,
            "backend": "stdlib",
            "pointsAccumulated": 6,
            "verifiedSourceCount": 2,
            "occupiedCells": 5,
            "occupancyRatio": 5.0 / 256.0,
            "maxPointsPerCell": 2,
            "maxSourceSupport": 2,
            "verticalSpreadP95SourceUnits": 0.5,
        },
        "glbFiles": [
            {
                "tileId": 0,
                "sourceLabel": "MipTile_0.glb",
                "sha256": "a" * 64,
                "byteLength": 100,
            },
            {
                "tileId": 1,
                "sourceLabel": "MipTile_1.glb",
                "sha256": "b" * 64,
                "byteLength": 110,
            },
        ],
        "plyFiles": [
            {
                "tileId": 0,
                "sourceLabel": "MipTile_0.ply",
                "sha256": "c" * 64,
                "byteLength": 200,
            },
            {
                "tileId": 1,
                "sourceLabel": "MipTile_1.ply",
                "sha256": "d" * 64,
                "byteLength": 210,
            },
        ],
        "pairs": [
            {
                "tileId": 0,
                "classification": "strong-match",
                "normalizedCenterDelta": 0.01,
                "maxExtentRelativeError": 0.02,
                "xyOverlapOfSmaller": 0.99,
                "axisPermutationSuspicion": False,
                "centerDelta": [12.3, 45.6, 78.9],
                "glbBounds": {"min": [1000, 2000, 3000], "max": [1003, 2003, 3003]},
                "plyBounds": {"min": [1000, 2000, 3000], "max": [1003, 2003, 3003]},
            },
            {
                "tileId": 1,
                "classification": "review",
                "normalizedCenterDelta": 0.08,
                "maxExtentRelativeError": 0.20,
                "xyOverlapOfSmaller": 0.72,
                "axisPermutationSuspicion": True,
                "centerDelta": [91.2, 34.5, 67.8],
                "glbBounds": {"min": [1010, 2010, 3010], "max": [1013, 2013, 3013]},
                "plyBounds": {"min": [1010, 2010, 3010], "max": [1013, 2013, 3013]},
            },
        ],
        "globalBounds": {
            "min": [1000.0, 2000.0, 3000.0],
            "max": [1013.0, 2013.0, 3013.0],
        },
        "warnings": [
            "C:/Users/Jozz/private-location 50.12345 20.54321",
            "private-place-name requires review",
        ],
    }


def build_documents(
    *,
    passed: bool = True,
    confirmed: bool = True,
) -> dict[str, dict[str, object]]:
    return bundle.build_bundle_documents(
        package_id="scan/test-package",
        proposal_id="proposal/test-package/revision-1",
        inspection_report=inspection_report(passed=passed),
        frame_contract=frame_contract(confirmed=confirmed),
    )


class ScanImportBundleTests(unittest.TestCase):
    def test_builds_linked_private_documents_and_unreviewed_proposal(self) -> None:
        documents = build_documents()
        package = documents["private/source_package.json"]
        proposal = documents["private/world_import_proposal.json"]
        self.assertEqual(package["privacyClass"], "PRIVATE_LOCAL_ONLY")
        self.assertEqual(proposal["status"], "UNREVIEWED")
        self.assertEqual(proposal["manualDecisions"], [])
        self.assertEqual(proposal["sourceRevisionId"], package["revisionId"])
        self.assertFalse(
            proposal["capabilities"]["internalGeometryCorrespondencePassed"]
        )
        self.assertFalse(proposal["capabilities"]["acceptedWorldPatchReady"])

    def test_shareable_projection_omits_names_coordinates_hashes_and_warnings(self) -> None:
        shareable = bundle.build_shareable_inspection(inspection_report())
        encoded = json.dumps(shareable, sort_keys=True)
        for secret in (
            "private-place-name",
            "C:/Users/Jozz",
            "50.12345",
            "20.54321",
            "globalBounds",
            "glbBounds",
            "plyBounds",
            "centerDelta",
            "a" * 64,
            "MipTile_0.glb",
        ):
            self.assertNotIn(secret, encoded)
        self.assertEqual(shareable["datasetLabel"], "scan-dataset")
        self.assertEqual(
            [pair["classification"] for pair in shareable["pairEvidence"]],
            ["bounds-strong-match", "review"],
        )
        self.assertEqual(shareable["omittedFreeformWarningCount"], 2)
        self.assertEqual(
            shareable["privacy"]["georeferencingStatus"], "NOT_ASSERTED"
        )

    def test_transactional_bundle_is_complete_verifiable_and_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            documents = build_documents()
            first = bundle.write_bundle_transactionally(
                documents=documents,
                output_root=root,
                bundle_label="fixture",
            )
            second = bundle.write_bundle_transactionally(
                documents=documents,
                output_root=root,
                bundle_label="fixture",
            )
            self.assertEqual(first, second)
            summary = bundle.verify_bundle(first)
            self.assertEqual(summary["packageId"], "scan/test-package")
            self.assertTrue((first / "COMPLETE.json").is_file())
            actual = {
                path.relative_to(first).as_posix()
                for path in first.rglob("*")
                if path.is_file()
            }
            self.assertEqual(
                actual,
                set(bundle._DOCUMENT_PATHS) | {"COMPLETE.json"},
            )

    def test_tampered_file_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = bundle.write_bundle_transactionally(
                documents=build_documents(),
                output_root=Path(temporary),
                bundle_label="fixture",
            )
            target = output / "shareable" / "inspection.shareable.json"
            target.write_bytes(target.read_bytes() + b" ")
            with self.assertRaises(bundle.ImportBundleError):
                bundle.verify_bundle(output)

    def test_existing_corrupt_content_addressed_bundle_is_not_overwritten(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            documents = build_documents()
            output = bundle.write_bundle_transactionally(
                documents=documents,
                output_root=root,
                bundle_label="fixture",
            )
            (output / "private" / "source_frame.json").write_text(
                "{}\n", encoding="utf-8"
            )
            with self.assertRaises(bundle.ImportBundleError):
                bundle.write_bundle_transactionally(
                    documents=documents,
                    output_root=root,
                    bundle_label="fixture",
                )

    def test_unexpected_extra_file_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = bundle.write_bundle_transactionally(
                documents=build_documents(),
                output_root=Path(temporary),
                bundle_label="fixture",
            )
            (output / "unexpected.txt").write_text("not allowed", encoding="utf-8")
            with self.assertRaises(bundle.ImportBundleError):
                bundle.verify_bundle(output)

    def test_directory_without_complete_marker_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            incomplete = Path(temporary) / ".fixture.staging-interrupted"
            incomplete.mkdir()
            (incomplete / "partial.json").write_text("{}\n", encoding="utf-8")
            with self.assertRaises(bundle.ImportBundleError):
                bundle.verify_bundle(incomplete)

    def test_duplicate_json_keys_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "duplicate.json"
            path.write_text('{"schema":1,"schema":2}\n', encoding="utf-8")
            with self.assertRaises(bundle.ImportBundleError):
                bundle.load_json_strict(path)

    def test_nonfinite_private_report_is_rejected_before_hashing(self) -> None:
        report = inspection_report()
        report["evidenceGrid"]["occupancyRatio"] = math.nan
        with self.assertRaises(bundle.ImportBundleError):
            bundle.build_bundle_documents(
                package_id="scan/test-package",
                proposal_id="proposal/test-package/revision-1",
                inspection_report=report,
                frame_contract=frame_contract(),
            )

    def test_optional_promotion_gates_are_enforced(self) -> None:
        with self.assertRaises(bundle.ImportBundleError):
            bundle.build_bundle_documents(
                package_id="scan/test-package",
                proposal_id="proposal/test-package/revision-1",
                inspection_report=inspection_report(passed=False),
                frame_contract=frame_contract(),
                require_inspection_pass=True,
            )
        with self.assertRaises(bundle.ImportBundleError):
            bundle.build_bundle_documents(
                package_id="scan/test-package",
                proposal_id="proposal/test-package/revision-1",
                inspection_report=inspection_report(),
                frame_contract=frame_contract(confirmed=False),
                require_frame_confirmed=True,
            )

    def test_invalid_bundle_label_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(bundle.ImportBundleError):
                bundle.write_bundle_transactionally(
                    documents=build_documents(),
                    output_root=Path(temporary),
                    bundle_label="../escape",
                )

    def test_cross_document_revision_mismatch_is_rejected_before_write(self) -> None:
        documents = build_documents()
        proposal = documents["private/world_import_proposal.json"]
        proposal["sourceRevisionId"] = "sha256:" + "0" * 64
        unsigned = dict(proposal)
        unsigned.pop("proposalContentSha256")
        proposal["proposalContentSha256"] = (
            bundle.scan_world_contracts.sha256_json(unsigned)
        )
        with self.assertRaises(bundle.ImportBundleError):
            bundle.build_manifest(documents)

    def test_noncanonical_shareable_document_is_rejected_before_write(self) -> None:
        documents = build_documents()
        documents["shareable/inspection.shareable.json"]["privateNote"] = (
            "C:/Users/Jozz/private"
        )
        with self.assertRaises(bundle.ImportBundleError):
            bundle.build_manifest(documents)


if __name__ == "__main__":
    unittest.main()
