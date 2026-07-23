from __future__ import annotations

import base64
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

MODULE_PATH = (
    Path(__file__).parents[2]
    / "tools"
    / "scan_pipeline"
    / "scan_preview_pack.py"
)
spec = importlib.util.spec_from_file_location("scan_preview_pack_tested", MODULE_PATH)
assert spec and spec.loader
preview = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = preview
spec.loader.exec_module(preview)

# Building textures needs an image backend (OpenCV or Pillow). The module import
# and the verify path stay dependency-free, so tests that actually encode a
# texture skip cleanly where no backend is installed (e.g. the dependency-free
# canonical CI runner).
_HAS_TEXTURE_BACKEND = preview.texture_encoding_available()
_needs_texture_backend = unittest.skipUnless(
    _HAS_TEXTURE_BACKEND, "requires an image backend (opencv-python or Pillow)"
)


# A tiny 2x2 baseColor PNG (cv2-decodable) embedded so the synthetic tile has a
# real material texture, matching the v2 textured preview pack contract.
_TEXTURE_PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAFUlEQVQImWPmOsG1"
    "/psyMxAzMDAAACSlBHPSchLSAAAAAElFTkSuQmCC"
)


def make_glb(*, mode: int = 4) -> bytes:
    positions = struct.pack(
        "<9f",
        1001.0,
        2002.0,
        3003.0,
        1002.0,
        2002.0,
        3003.0,
        1001.0,
        2003.0,
        3003.0,
    )
    texcoords = struct.pack("<6f", 0.0, 0.0, 1.0, 0.0, 0.0, 1.0)
    indices = struct.pack("<3H", 0, 1, 2)
    prefix = positions + texcoords + indices
    prefix += b"\0" * ((4 - len(prefix) % 4) % 4)
    image_offset = len(prefix)
    binary = prefix + _TEXTURE_PNG
    binary += b"\0" * ((4 - len(binary) % 4) % 4)
    document = {
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(positions)},
            {
                "buffer": 0,
                "byteOffset": len(positions),
                "byteLength": len(texcoords),
            },
            {
                "buffer": 0,
                "byteOffset": len(positions) + len(texcoords),
                "byteLength": len(indices),
            },
            {
                "buffer": 0,
                "byteOffset": image_offset,
                "byteLength": len(_TEXTURE_PNG),
            },
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
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": 3,
                "type": "VEC2",
            },
            {
                "bufferView": 2,
                "componentType": 5123,
                "count": 3,
                "type": "SCALAR",
            },
        ],
        "images": [{"bufferView": 3, "mimeType": "image/png"}],
        "textures": [{"source": 0}],
        "materials": [
            {"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}
        ],
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {"POSITION": 0, "TEXCOORD_0": 1},
                        "indices": 2,
                        "material": 0,
                        "mode": mode,
                    }
                ]
            }
        ],
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
            "maxTriangleEdgeSourceUnits": 1.5,
            "maxTriangleAreaSourceUnitsSquared": 0.5,
        },
        "evidenceGrid": {
            "width": 4,
            "height": 4,
            "backend": "stdlib",
            "pointsAccumulated": 3,
            "verifiedSourceCount": 1,
            "occupiedCells": 3,
            "occupancyRatio": 3.0 / 16.0,
            "maxPointsPerCell": 1,
            "maxSourceSupport": 1,
            "verticalSpreadP95SourceUnits": 0.0,
        },
        "glbFiles": [
            {
                "tileId": 0,
                "sourceLabel": "MipTile_0.glb",
                "sha256": hashlib.sha256(glb).hexdigest(),
                "byteLength": len(glb),
            }
        ],
        "plyFiles": [
            {
                "tileId": 0,
                "sourceLabel": "MipTile_0.ply",
                "sha256": "c" * 64,
                "byteLength": 12,
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
                "centerDelta": [0.0, 0.0, 0.0],
                "glbBounds": {
                    "min": [1001, 2002, 3003],
                    "max": [1002, 2003, 3003],
                },
                "plyBounds": {
                    "min": [1001, 2002, 3003],
                    "max": [1002, 2003, 3003],
                },
            }
        ],
        "globalBounds": {
            "min": [1001, 2002, 3003],
            "max": [1002, 2003, 3003],
        },
        "warnings": [],
    }


def _write_receipt(
    root: Path,
    bundle: Path,
    *,
    acknowledged: bool = True,
    bundle_hash: str | None = None,
    revision: str | None = None,
) -> Path:
    summary = preview.scan_import_bundle.verify_bundle(bundle)
    receipt = preview.scan_owner_gate.build_receipt(
        candidate={
            "schemaVersion": 3,
            "datasetStatus": "compatible",
            "automaticEvidenceGatePassed": True,
            "glbFiles": 1,
            "plyFiles": 1,
            "pairCount": 1,
        },
        identical_copy_count=2,
        privacy_review_acknowledged=acknowledged,
        bundle_content_sha256=(
            bundle_hash or summary["bundleContentSha256"]
        ),
        source_revision_id=(revision or summary["sourceRevisionId"]),
    )
    path = root / "p1b_owner_gate_receipt.local.json"
    preview.scan_owner_gate.write_json_atomic(path, receipt)
    return path


def fixture(
    root: Path,
    *,
    mode: int = 4,
    confirmed: bool = True,
    receipt_acknowledged: bool = True,
) -> tuple[Path, Path, Path]:
    glb = make_glb(mode=mode)
    documents = preview.scan_import_bundle.build_bundle_documents(
        package_id="scan/test",
        proposal_id="proposal/test/revision-1",
        inspection_report=report(glb),
        frame_contract=frame(confirmed=confirmed),
        require_inspection_pass=True,
    )
    bundle = preview.scan_import_bundle.write_bundle_transactionally(
        documents=documents,
        output_root=root / "bundles",
        bundle_label="fixture",
    )
    receipt = _write_receipt(
        root, bundle, acknowledged=receipt_acknowledged
    )
    source = root / "source"
    source.mkdir()
    (source / "MipTile_0.glb").write_bytes(glb)
    return bundle, receipt, source


def build_fixture_preview(
    root: Path,
    *,
    mode: int = 4,
    confirmed: bool = True,
    receipt_acknowledged: bool = True,
    label: str = "source-preview",
    level_x_degrees: float = 0.0,
    level_z_degrees: float = 0.0,
) -> Path:
    bundle, receipt, source = fixture(
        root,
        mode=mode,
        confirmed=confirmed,
        receipt_acknowledged=receipt_acknowledged,
    )
    return preview.build_preview_pack(
        bundle=bundle,
        owner_gate_receipt=receipt,
        source_root=source,
        output_root=root / "previews",
        label=label,
        level_x_degrees=level_x_degrees,
        level_z_degrees=level_z_degrees,
    )


class ScanPreviewPackTests(unittest.TestCase):
    @_needs_texture_backend
    def test_builds_verified_textured_pack(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = build_fixture_preview(root)
            manifest = preview._strict_json(output / "COMPLETE.json")
            self.assertEqual(preview.verify_preview_pack(output)["tileCount"], 1)
            self.assertEqual(manifest["purpose"], preview.PURPOSE)
            self.assertFalse(manifest["capabilities"]["acceptedWorld"])
            self.assertFalse(manifest["capabilities"]["collisionReady"])
            self.assertTrue(manifest["capabilities"]["texturesIncluded"])
            tile = manifest["tiles"][0]
            self.assertEqual(tile["groupCount"], 1)
            group = tile["groups"][0]
            self.assertEqual(
                group["texturePath"], "textures/tile_000_group_000.png"
            )
            self.assertLessEqual(group["textureWidth"], preview.MAX_TEXTURE_DIM)
            self.assertLessEqual(group["textureHeight"], preview.MAX_TEXTURE_DIM)

    @_needs_texture_backend
    def test_frame_is_baked_into_lab_space(self) -> None:
        groups = preview._extract_tile_groups(make_glb(), 0, frame())
        self.assertEqual(len(groups), 1)
        self.assertEqual(groups[0]["positions"][0], (1.0, 3.0, -2.0))
        record = preview._read_tile(preview._serialize_tile(0, groups), 0)
        self.assertEqual(
            record["boundsLabMeters"]["min"], [1.0, 3.0, -3.0]
        )
        self.assertEqual(
            record["boundsLabMeters"]["max"], [2.0, 3.0, -2.0]
        )

    def test_unconfirmed_frame_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source = fixture(root, confirmed=False)
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle,
                    owner_gate_receipt=receipt,
                    source_root=source,
                    output_root=root / "previews",
                )

    def test_pending_or_wrong_receipt_is_rejected(self) -> None:
        with self.subTest("privacy pending"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source = fixture(
                root, receipt_acknowledged=False
            )
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle,
                    owner_gate_receipt=receipt,
                    source_root=source,
                    output_root=root / "previews",
                )

        with self.subTest("wrong bundle binding"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, _, source = fixture(root)
            receipt = _write_receipt(
                root, bundle, bundle_hash="e" * 64
            )
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle,
                    owner_gate_receipt=receipt,
                    source_root=source,
                    output_root=root / "previews",
                )

    def test_source_hash_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source = fixture(root)
            path = source / "MipTile_0.glb"
            damaged = bytearray(path.read_bytes())
            damaged[-1] ^= 1
            path.write_bytes(damaged)
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle,
                    owner_gate_receipt=receipt,
                    source_root=source,
                    output_root=root / "previews",
                )

    def test_non_triangle_primitive_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source = fixture(root, mode=1)
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle,
                    owner_gate_receipt=receipt,
                    source_root=source,
                    output_root=root / "previews",
                )

    def test_unsafe_label_is_rejected_before_path_creation(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source = fixture(root)
            with self.assertRaises(preview.PreviewPackError):
                preview.build_preview_pack(
                    bundle=bundle,
                    owner_gate_receipt=receipt,
                    source_root=source,
                    output_root=root / "previews",
                    label="../escape",
                )
            self.assertFalse((root / "escape").exists())

    @_needs_texture_backend
    def test_publication_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source = fixture(root)
            first = preview.build_preview_pack(
                bundle=bundle,
                owner_gate_receipt=receipt,
                source_root=source,
                output_root=root / "previews",
            )
            second = preview.build_preview_pack(
                bundle=bundle,
                owner_gate_receipt=receipt,
                source_root=source,
                output_root=root / "previews",
            )
            self.assertEqual(first, second)

    @_needs_texture_backend
    def test_tamper_and_extra_file_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = build_fixture_preview(root)
            tile = output / "tiles" / "tile_000.bin"
            tile.write_bytes(tile.read_bytes() + b"x")
            with self.assertRaises(preview.PreviewPackError):
                preview.verify_preview_pack(output)

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = build_fixture_preview(root)
            (output / "unexpected.txt").write_text("no", encoding="utf-8")
            with self.assertRaises(preview.PreviewPackError):
                preview.verify_preview_pack(output)

    @_needs_texture_backend
    def test_capability_or_extra_field_overclaim_is_rejected_after_rehash(self) -> None:
        with self.subTest("capability"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = build_fixture_preview(root)
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

        with self.subTest("extra top-level claim"), tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = build_fixture_preview(root)
            manifest = preview._strict_json(output / "COMPLETE.json")
            manifest["acceptedWorldPatch"] = {"ready": True}
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

    @_needs_texture_backend
    def test_binary_index_range_is_validated(self) -> None:
        groups = preview._extract_tile_groups(make_glb(), 0, frame())
        payload = preview._serialize_tile(0, groups)
        damaged = bytearray(payload)
        struct.pack_into("<I", damaged, len(damaged) - 4, 99)
        with self.assertRaises(preview.PreviewPackError):
            preview._read_tile(bytes(damaged), 0)


class LevelingCorrectionTests(unittest.TestCase):
    """Per-scan leveling: a small lab-space tilt applied after the axis matrix."""

    def test_zero_leveling_is_identity_none(self) -> None:
        self.assertIsNone(preview._leveling_matrix(0.0, 0.0))

    def test_leveling_matrix_rotates_about_lab_x(self) -> None:
        matrix = preview._leveling_matrix(90.0, 0.0)
        assert matrix is not None
        rotated = preview._mat_vec(matrix, (0.0, 1.0, 0.0))
        for got, expected in zip(rotated, (0.0, 0.0, 1.0)):
            self.assertAlmostEqual(got, expected, places=6)

    def test_out_of_range_leveling_is_rejected(self) -> None:
        with self.assertRaises(preview.PreviewPackError):
            preview._leveling_record(90.0, 0.0)
        with self.assertRaises(preview.PreviewPackError):
            preview._leveling_record(0.0, -50.0)

    @_needs_texture_backend
    def test_leveling_rotates_the_baked_positions(self) -> None:
        flat = preview._extract_tile_groups(make_glb(), 0, frame())
        matrix = preview._leveling_matrix(-3.9, -4.1)
        assert matrix is not None
        tilted = preview._extract_tile_groups(make_glb(), 0, frame(), matrix)
        self.assertEqual(len(flat[0]["positions"]), len(tilted[0]["positions"]))
        for flat_pos, tilted_pos in zip(
            flat[0]["positions"], tilted[0]["positions"]
        ):
            expected = preview._mat_vec(matrix, flat_pos)
            for got, want in zip(tilted_pos, expected):
                self.assertAlmostEqual(got, want, places=6)

    @_needs_texture_backend
    def test_identity_leveling_is_recorded_as_not_applied(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = build_fixture_preview(Path(temporary))
            manifest = preview._strict_json(output / "COMPLETE.json")
            self.assertEqual(
                manifest["levelingCorrection"],
                {
                    "labAxisDegrees": {"x": 0.0, "z": 0.0},
                    "order": preview.LEVELING_ORDER,
                    "applied": False,
                },
            )

    @_needs_texture_backend
    def test_per_scan_leveling_is_recorded_and_verified(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = build_fixture_preview(
                Path(temporary), level_x_degrees=-3.9, level_z_degrees=-4.1
            )
            manifest = preview._strict_json(output / "COMPLETE.json")
            self.assertEqual(
                manifest["levelingCorrection"],
                {
                    "labAxisDegrees": {"x": -3.9, "z": -4.1},
                    "order": preview.LEVELING_ORDER,
                    "applied": True,
                },
            )
            self.assertEqual(preview.verify_preview_pack(output)["tileCount"], 1)

    @_needs_texture_backend
    def test_inconsistent_leveling_is_rejected_after_rehash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = build_fixture_preview(Path(temporary))
            manifest = preview._strict_json(output / "COMPLETE.json")
            # Claim leveling was applied while both angles are zero.
            manifest["levelingCorrection"]["applied"] = True
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


if __name__ == "__main__":
    unittest.main()
