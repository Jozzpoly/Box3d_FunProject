from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_preview_pack.py"
spec = importlib.util.spec_from_file_location("scan_preview_pack_tested", MODULE_PATH)
assert spec and spec.loader
preview = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = preview
spec.loader.exec_module(preview)


def make_glb(*, mode: int = 4) -> bytes:
    positions = struct.pack(
        "<9f",
        1001.0, 2002.0, 3003.0,
        1002.0, 2002.0, 3003.0,
        1001.0, 2003.0, 3003.0,
    )
    indices = struct.pack("<3H", 0, 1, 2)
    binary = positions + indices
    binary += b"\0" * ((4 - len(binary) % 4) % 4)
    document = {
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(positions)},
            {"buffer": 0, "byteOffset": len(positions), "byteLength": len(indices)},
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3",
                "min": [1001.0, 2002.0, 3003.0],
                "max": [1002.0, 2003.0, 3003.0],
            },
            {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "mode": mode}]}],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    json_bytes = json.dumps(document, separators=(",", ":")).encode("utf-8")
    json_bytes += b" " * ((4 - len(json_bytes) % 4) % 4)
    total = 12 + 8 + len(json_bytes) + 8 + len(binary)
    return (
        struct.pack("<4sII", b"glTF", 2, total)
        + struct.pack("<II", len(json_bytes), preview.scan_inspect.JSON_CHUNK)
        + json_bytes
        + struct.pack("<II", len(binary), preview.scan_inspect.BIN_CHUNK)
        + binary
    )


def frame(*, confirmed: bool = True) -> dict[str, object]:
    return {
        "schema": preview.scan_frames.SCHEMA,
        "schemaVersion": preview.scan_frames.SCHEMA_VERSION,
        "confirmed": confirmed,
        "sourceFrame": {
            "handedness": "right",
            "axisRoles": {"right": "+X", "forward": "+Y", "up": "+Z"},
            "unitsPerMeter": 1.0,
        },
        "labFrame": {
            "handedness": "right",
            "axisRoles": {"right": "+X", "forward": "-Z", "up": "+Y"},
        },
        "sourceToLab": {
            "axisMatrix": [[1, 0, 0], [0, 0, 1], [0, -1, 0]],
            "orientationChange": "preserve",
            "mirrorApproved": False,
            "localOriginSource": [1000.0, 2000.0, 3000.0],
        },
    }


def report(glb: bytes) -> dict[str, object]:
    return {
        "schema": preview.scan_world_contracts.INSPECTION_SCHEMA,
        "schemaVersion": 3,
        "packageName": "private-test",
        "datasetStatus": "compatible",
        "automaticEvidenceGate": {"passed": True},
        "totals": {
            "glbFiles": 1, "plyFiles": 1, "glbVertices": 3,
            "glbTriangles": 1, "plyPoints": 3,
        },
        "geometryQuality": {
            "triangleCountAnalyzed": 1,
            "degenerateTriangleCount": 0,
            "provisionalLargeTriangleCount": 0,
            "provisionalLargeEdgeThresholdSourceUnits": 10.0,
            "maxTriangleEdgeSourceUnits": 1.5,
            "maxTriangleAreaSourceUnitsSquared": 0.5,
        },
        "evidenceGrid": {
            "width": 4, "height": 4, "backend": "stdlib",
            "pointsAccumulated": 3, "verifiedSourceCount": 1,
            "occupiedCells": 3, "occupancyRatio": 3.0 / 16.0,
            "maxPointsPerCell": 1, "maxSourceSupport": 1,
            "verticalSpreadP95SourceUnits": 0.0,
        },
        "glbFiles": [{
            "tileId": 0, "sourceLabel": "MipTile_0.glb",
            "sha256": hashlib.sha256(glb).hexdigest(), "byteLength": len(glb),
        }],
        "plyFiles": [{
            "tileId": 0, "sourceLabel": "MipTile_0.ply",
            "sha256": "c" * 64, "byteLength": 12,
        }],
        "pairs": [{
            "tileId": 0, "classification": "strong-match",
            "normalizedCenterDelta": 0.0, "maxExtentRelativeError": 0.0,
            "xyOverlapOfSmaller": 1.0, "axisPermutationSuspicion": False,
            "centerDelta": [0.0, 0.0, 0.0],
            "glbBounds": {"min": [1001, 2002, 3003], "max": [1002, 2003, 3003]},
            "plyBounds": {"min": [1001, 2002, 3003], "max": [1002, 2003, 3003]},
        }],
        "globalBounds": {"min": [1001, 2002, 3003], "max": [1002, 2003, 3003]},
        "warnings": [],
    }


def fixture(root: Path, *, mode: int = 4, confirmed: bool = True) -> tuple[Path, Path]:
    glb = make_glb(mode=mode)
    documents = preview.scan_import_bundle.build_bundle_documents(
        package_id="scan/test",
        proposal_id="proposal/test/revision-1",
        inspection_report=report(glb),
        frame_contract=frame(confirmed=confirmed),
        require_inspection_pass=True,
    )
    bundle = preview.scan_import_bundle.write_bundle_transactionally(
        documents=documents, output_root=root / "bundles", bundle_label="fixture"
    )
    source = root / "source"
    source.mkdir()
    (source / "MipTile_0.glb").write_bytes(glb)
    return bundle, source


class ScanPreviewPackTests(unittest.TestCase):
    def test_builds_verified_geometry_only_pack(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = fixture(root)
            output = preview.build_preview_pack(
                bundle=bundle, source_root=source, output_root=root / "previews"
            )
            manifest = preview._strict_json(output / "COMPLETE.json")
            self.assertEqual(preview.verify_preview_pack(output)["tileCount"], 1)
            self.assertEqual(manifest["purpose"], preview.PURPOSE)
            self.assertFalse(manifest["capabilities"]["acceptedWorld"])
            self.assertFalse(manifest["capabilities"]["collisionReady"])
            self.assertFalse(manifest["capabilities"]["texturesIncluded"])

    def test_frame_is_baked_into_lab_space(self) -> None:
        payload, metadata = preview._extract_geometry(make_glb(), 0, frame())
        first = preview.VERTEX.unpack_from(payload, preview.HEADER.size)
        self.assertEqual(first[:3], (1.0, 3.0, -2.0))
        self.assertEqual(metadata["boundsLabMeters"]["min"], [1.0, 3.0, -3.0])
        self.assertEqual(metadata["boundsLabMeters"]["max"], [2.0, 3.0, -2.0])

    def test_unconfirmed_frame_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = fixture(root, confirmed=False)
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle, source_root=source, output_root=root / "previews"
                )

    def test_source_hash_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = fixture(root)
            path = source / "MipTile_0.glb"
            damaged = bytearray(path.read_bytes())
            damaged[-1] ^= 1
            path.write_bytes(damaged)
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle, source_root=source, output_root=root / "previews"
                )

    def test_non_triangle_primitive_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = fixture(root, mode=1)
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle, source_root=source, output_root=root / "previews"
                )

    def test_publication_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = fixture(root)
            first = preview.build_preview_pack(
                bundle=bundle, source_root=source, output_root=root / "previews"
            )
            second = preview.build_preview_pack(
                bundle=bundle, source_root=source, output_root=root / "previews"
            )
            self.assertEqual(first, second)

    def test_tamper_and_extra_file_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = fixture(root)
            output = preview.build_preview_pack(
                bundle=bundle, source_root=source, output_root=root / "previews"
            )
            tile = output / "tiles" / "tile_000.bin"
            tile.write_bytes(tile.read_bytes() + b"x")
            with self.assertRaises(preview.PreviewPackError):
                preview.verify_preview_pack(output)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = fixture(root)
            output = preview.build_preview_pack(
                bundle=bundle, source_root=source, output_root=root / "previews"
            )
            (output / "unexpected.txt").write_text("no", encoding="utf-8")
            with self.assertRaises(preview.PreviewPackError):
                preview.verify_preview_pack(output)

    def test_capability_overclaim_is_rejected_after_rehash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, source = fixture(root)
            output = preview.build_preview_pack(
                bundle=bundle, source_root=source, output_root=root / "previews"
            )
            manifest = preview._strict_json(output / "COMPLETE.json")
            manifest["capabilities"]["collisionReady"] = True
            unsigned = dict(manifest)
            unsigned.pop("previewContentSha256")
            manifest["previewContentSha256"] = preview._sha256_bytes(
                preview._canonical_json_bytes(unsigned)
            )
            (output / "COMPLETE.json").write_bytes(
                preview._canonical_json_bytes(manifest)
            )
            with self.assertRaises(preview.PreviewPackError):
                preview.verify_preview_pack(output)

    def test_binary_index_range_is_validated(self) -> None:
        payload, _ = preview._extract_geometry(make_glb(), 0, frame())
        damaged = bytearray(payload)
        struct.pack_into("<I", damaged, len(damaged) - 4, 99)
        with self.assertRaises(preview.PreviewPackError):
            preview._read_tile(bytes(damaged), 0)


if __name__ == "__main__":
    unittest.main()
