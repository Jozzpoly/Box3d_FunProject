from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import sys
import unittest

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_glb_quality.py"
spec = importlib.util.spec_from_file_location("scan_glb_quality_tested", MODULE_PATH)
assert spec and spec.loader
scan_glb_quality = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = scan_glb_quality
spec.loader.exec_module(scan_glb_quality)


def pad4(data: bytes, padding: bytes = b"\x00") -> bytes:
    return data + padding * ((-len(data)) % 4)


def build_glb(
    positions: list[tuple[float, float, float]],
    indices: list[int],
    *,
    index_component_type: int = 5123,
    declared_min: list[float] | None = None,
    declared_max: list[float] | None = None,
    extra_orphan_mesh_node: bool = False,
) -> bytes:
    position_bytes = b"".join(struct.pack("<3f", *point) for point in positions)
    if index_component_type == 5121:
        index_code = "B"
    elif index_component_type == 5123:
        index_code = "H"
    elif index_component_type == 5125:
        index_code = "I"
    elif index_component_type == 5126:
        index_code = "f"
    else:
        raise ValueError(index_component_type)
    index_bytes = b"".join(struct.pack("<" + index_code, value) for value in indices)
    position_offset = 0
    index_offset = len(pad4(position_bytes))
    binary = pad4(position_bytes) + pad4(index_bytes)
    minimum = declared_min if declared_min is not None else [min(point[i] for point in positions) for i in range(3)]
    maximum = declared_max if declared_max is not None else [max(point[i] for point in positions) for i in range(3)]
    nodes = [{"mesh": 0}]
    if extra_orphan_mesh_node:
        nodes.append({"mesh": 0, "translation": [50.0, 0.0, 0.0]})
    document = {
        "asset": {"version": "2.0", "generator": "private-generator-must-not-leak"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": len(positions),
                "type": "VEC3",
                "min": minimum,
                "max": maximum,
            },
            {
                "bufferView": 1,
                "componentType": index_component_type,
                "count": len(indices),
                "type": "SCALAR",
            },
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": position_offset, "byteLength": len(position_bytes)},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": len(index_bytes)},
        ],
        "buffers": [{"byteLength": len(binary)}],
    }
    json_chunk = pad4(json.dumps(document, separators=(",", ":")).encode("utf-8"), b" ")
    total = 12 + 8 + len(json_chunk) + 8 + len(binary)
    return (
        struct.pack("<4sII", b"glTF", 2, total)
        + struct.pack("<II", len(json_chunk), 0x4E4F534A)
        + json_chunk
        + struct.pack("<II", len(binary), 0x004E4942)
        + binary
    )


class ScanGlbQualityTests(unittest.TestCase):
    def test_valid_triangle_is_scanned_from_actual_accessor_data(self) -> None:
        data = build_glb([(0.0, 0.0, 0.0), (2.0, 0.0, 0.0), (0.0, 3.0, 0.0)], [0, 1, 2])
        report = scan_glb_quality.inspect_glb_quality(data, "MipTile_0.glb", prefer_numpy=False)
        self.assertEqual(report["triangleCount"], 1)
        self.assertEqual(report["geometryQuality"]["triangleCountAnalyzed"], 1)
        self.assertEqual(report["geometryQuality"]["degenerateTriangleCount"], 0)
        self.assertEqual(report["worldBounds"]["max"], [2.0, 3.0, 0.0])
        self.assertTrue(report["primitives"][0]["quality"]["declaredPositionBounds"]["matchesActual"])
        self.assertNotIn("private-generator", repr(report))

    def test_degenerate_triangle_is_counted(self) -> None:
        data = build_glb([(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (2.0, 0.0, 0.0)], [0, 1, 2])
        report = scan_glb_quality.inspect_glb_quality(data, "MipTile_1.glb", prefer_numpy=False)
        self.assertEqual(report["geometryQuality"]["degenerateTriangleCount"], 1)

    def test_long_edge_histogram_is_reported_in_source_units(self) -> None:
        data = build_glb([(0.0, 0.0, 0.0), (100.0, 0.0, 0.0), (0.0, 100.0, 0.0)], [0, 1, 2])
        report = scan_glb_quality.inspect_glb_quality(data, "MipTile_2.glb", prefer_numpy=False)
        quality = report["geometryQuality"]
        self.assertEqual(quality["provisionalLargeTriangleCount"], 1)
        self.assertEqual(quality["edgeThresholdCountsSourceUnits"]["gt_100"], 1)
        self.assertGreater(quality["maxTriangleEdgeSourceUnits"], 100.0)
        self.assertNotIn("Meters", repr(quality))

    def test_invalid_float_indices_are_rejected(self) -> None:
        data = build_glb(
            [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)],
            [0, 1, 2],
            index_component_type=5126,
        )
        with self.assertRaises(scan_glb_quality.GlbQualityError):
            scan_glb_quality.inspect_glb_quality(data, "bad.glb", prefer_numpy=False)

    def test_out_of_range_index_is_rejected(self) -> None:
        data = build_glb(
            [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)],
            [0, 1, 9],
        )
        with self.assertRaises(scan_glb_quality.GlbQualityError):
            scan_glb_quality.inspect_glb_quality(data, "bad-index.glb", prefer_numpy=False)

    def test_declared_bounds_mismatch_does_not_replace_actual_bounds(self) -> None:
        data = build_glb(
            [(0.0, 0.0, 0.0), (2.0, 0.0, 0.0), (0.0, 3.0, 0.0)],
            [0, 1, 2],
            declared_min=[-999.0, -999.0, -999.0],
            declared_max=[999.0, 999.0, 999.0],
        )
        report = scan_glb_quality.inspect_glb_quality(data, "MipTile_3.glb", prefer_numpy=False)
        evidence = report["primitives"][0]["quality"]["declaredPositionBounds"]
        self.assertFalse(evidence["matchesActual"])
        self.assertEqual(report["worldBounds"]["min"], [0.0, 0.0, 0.0])
        self.assertEqual(report["worldBounds"]["max"], [2.0, 3.0, 0.0])

    def test_orphan_mesh_node_is_explicit(self) -> None:
        data = build_glb(
            [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)],
            [0, 1, 2],
            extra_orphan_mesh_node=True,
        )
        report = scan_glb_quality.inspect_glb_quality(data, "MipTile_4.glb", prefer_numpy=False)
        self.assertEqual(report["sceneSummary"]["orphanNodeIndices"], [1])
        self.assertEqual(report["sceneSummary"]["defaultUnreachableMeshNodeIndices"], [1])

    @unittest.skipUnless(scan_glb_quality._np is not None, "NumPy is optional")
    def test_numpy_and_stdlib_geometry_results_agree(self) -> None:
        data = build_glb(
            [(0.0, 0.0, 0.0), (2.0, 0.0, 0.0), (0.0, 3.0, 0.0)],
            [0, 1, 2],
        )
        fast = scan_glb_quality.inspect_glb_quality(data, "MipTile_5.glb", prefer_numpy=True)
        slow = scan_glb_quality.inspect_glb_quality(data, "MipTile_5.glb", prefer_numpy=False)
        self.assertEqual(fast["worldBounds"], slow["worldBounds"])
        self.assertEqual(fast["geometryQuality"], slow["geometryQuality"])


if __name__ == "__main__":
    unittest.main()
