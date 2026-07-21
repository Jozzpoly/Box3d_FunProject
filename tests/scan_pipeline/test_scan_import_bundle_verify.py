from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
BUNDLE_PATH = ROOT / "tools" / "scan_pipeline" / "scan_import_bundle.py"
VERIFY_PATH = ROOT / "tools" / "scan_pipeline" / "scan_import_bundle_verify.py"

spec = importlib.util.spec_from_file_location("scan_import_bundle_verify_fixture", BUNDLE_PATH)
assert spec and spec.loader
bundle = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = bundle
spec.loader.exec_module(bundle)


def frame_contract() -> dict[str, object]:
    return {
        "schema": bundle.scan_frames.SCHEMA,
        "schemaVersion": bundle.scan_frames.SCHEMA_VERSION,
        "confirmed": True,
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
            "localOriginSource": [0.0, 0.0, 0.0],
        },
    }


def inspection_report() -> dict[str, object]:
    return {
        "schema": bundle.scan_world_contracts.INSPECTION_SCHEMA,
        "schemaVersion": 3,
        "packageName": "private-fixture",
        "datasetStatus": "compatible",
        "automaticEvidenceGate": {"passed": True},
        "totals": {
            "glbFiles": 1,
            "plyFiles": 1,
            "glbVertices": 3,
            "glbTriangles": 1,
            "plyPoints": 3,
        },
        "geometryQuality": {
            "triangleCountAnalyzed": 1,
            "degenerateTriangleCount": 0,
            "provisionalLargeTriangleCount": 0,
            "provisionalLargeEdgeThresholdSourceUnits": 10.0,
            "maxTriangleEdgeSourceUnits": 2.0,
            "maxTriangleAreaSourceUnitsSquared": 1.0,
        },
        "evidenceGrid": {
            "width": 16,
            "height": 16,
            "backend": "stdlib",
            "pointsAccumulated": 3,
            "verifiedSourceCount": 1,
            "occupiedCells": 3,
            "occupancyRatio": 3.0 / 256.0,
            "maxPointsPerCell": 1,
            "maxSourceSupport": 1,
            "verticalSpreadP95SourceUnits": 0.0,
        },
        "glbFiles": [
            {
                "tileId": 0,
                "sourceLabel": "MipTile_0.glb",
                "sha256": "a" * 64,
                "byteLength": 100,
            }
        ],
        "plyFiles": [
            {
                "tileId": 0,
                "sourceLabel": "MipTile_0.ply",
                "sha256": "b" * 64,
                "byteLength": 200,
            }
        ],
        "pairs": [
            {
                "tileId": 0,
                "classification": "strong-match",
                "normalizedCenterDelta": 0.0,
                "maxExtentRelativeError": 0.0,
                "xyOverlapOfSmaller": 1.0,
                "axisPermutationSuspicion": False,
            }
        ],
        "warnings": [],
    }


def create_bundle(root: Path) -> Path:
    documents = bundle.build_bundle_documents(
        package_id="scan/verify-fixture",
        proposal_id="proposal/verify-fixture/revision-1",
        inspection_report=inspection_report(),
        frame_contract=frame_contract(),
        require_inspection_pass=True,
        require_frame_confirmed=True,
    )
    return bundle.write_bundle_transactionally(
        documents=documents,
        output_root=root,
        bundle_label="verify-fixture",
    )


class ScanImportBundleVerifyTests(unittest.TestCase):
    def run_verify(self, path: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(VERIFY_PATH), str(path)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )

    def test_cli_verifies_complete_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = create_bundle(Path(temporary))
            result = self.run_verify(output)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("scan_import_bundle_verify: OK", result.stdout)
            self.assertIn("scan/verify-fixture", result.stdout)
            self.assertIn("proposal/verify-fixture/revision-1", result.stdout)

    def test_cli_rejects_tampered_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = create_bundle(Path(temporary))
            target = output / "shareable" / "inspection.shareable.json"
            target.write_bytes(target.read_bytes() + b" ")
            result = self.run_verify(output)
            self.assertEqual(result.returncode, 2)
            self.assertIn("scan_import_bundle_verify: ERROR", result.stderr)

    def test_cli_rejects_missing_bundle(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            result = self.run_verify(Path(temporary) / "missing")
            self.assertEqual(result.returncode, 2)
            self.assertIn("scan_import_bundle_verify: ERROR", result.stderr)


if __name__ == "__main__":
    unittest.main()
