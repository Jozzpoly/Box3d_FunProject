from __future__ import annotations

import binascii
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_dataset_inspect.py"
spec = importlib.util.spec_from_file_location("scan_dataset_inspect_tested", MODULE_PATH)
assert spec and spec.loader
scan_dataset = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = scan_dataset
spec.loader.exec_module(scan_dataset)


def pad4(data: bytes, padding: bytes = b"\x00") -> bytes:
    return data + padding * ((-len(data)) % 4)


def build_glb(offset_x: float, *, invalid_index_accessor: bool = False) -> bytes:
    positions = struct.pack("<9f", offset_x, 0.0, 1.0, offset_x + 2.0, 0.0, 2.0, offset_x, 3.0, 4.0)
    indices = struct.pack("<3H", 0, 1, 2)
    extra = b"X" * 32
    position_offset = 0
    index_offset = len(pad4(positions))
    extra_offset = index_offset + len(pad4(indices))
    binary = pad4(positions) + pad4(indices) + extra
    index_accessor_offset = 8 if invalid_index_accessor else 0
    document = {
        "asset": {"version": "2.0", "generator": "p1-test"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": 3,
                "type": "VEC3",
                "min": [offset_x, 0.0, 1.0],
                "max": [offset_x + 2.0, 3.0, 4.0],
            },
            {
                "bufferView": 1,
                "byteOffset": index_accessor_offset,
                "componentType": 5123,
                "count": 3,
                "type": "SCALAR",
            },
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": position_offset, "byteLength": len(positions)},
            {"buffer": 0, "byteOffset": index_offset, "byteLength": len(indices)},
            {"buffer": 0, "byteOffset": extra_offset, "byteLength": len(extra)},
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


def build_ply(offset_x: float, *, secret: str | None = None) -> bytes:
    points = [
        (offset_x, 0.0, 1.0, 10, 20, 30),
        (offset_x + 2.0, 0.0, 2.0, 40, 50, 60),
        (offset_x, 3.0, 4.0, 70, 80, 90),
    ]
    lines = ["ply", "format binary_little_endian 1.0"]
    if secret:
        lines += [f"comment {secret}", f"obj_info {secret}"]
    lines += [
        f"element vertex {len(points)}",
        "property float x",
        "property float y",
        "property float z",
        "property uchar red",
        "property uchar green",
        "property uchar blue",
        "end_header",
    ]
    return ("\n".join(lines) + "\n").encode("ascii") + b"".join(
        struct.pack("<3f3B", *point) for point in points
    )


def write_pair(root: Path, tile: int, offset_x: float, *, ply_offset: float | None = None, secret: str | None = None) -> None:
    glb_dir = root / "model-glb" / "Data" / f"MipTile_{tile}"
    ply_dir = root / "model-ply" / "Data" / f"MipTile_{tile}"
    glb_dir.mkdir(parents=True, exist_ok=True)
    ply_dir.mkdir(parents=True, exist_ok=True)
    (glb_dir / f"MipTile_{tile}.glb").write_bytes(build_glb(offset_x))
    (ply_dir / f"MipTile_{tile}.ply").write_bytes(build_ply(offset_x if ply_offset is None else ply_offset, secret=secret))


class ScanDatasetInspectTests(unittest.TestCase):
    def test_two_paired_tiles_are_strong_matches(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_pair(root, 0, 0.0)
            write_pair(root, 1, 10.0)
            report, grid = scan_dataset.inspect_dataset(
                root,
                package_name="fixture",
                expected_glb=2,
                expected_ply=2,
                grid_size=32,
                chunk_vertices=2,
                prefer_numpy=False,
            )
            self.assertEqual(report["datasetStatus"], "compatible")
            self.assertEqual([pair["classification"] for pair in report["pairs"]], ["strong-match", "strong-match"])
            self.assertEqual(report["totals"]["plyPoints"], 6)
            self.assertEqual(report["totals"]["glbTriangles"], 2)
            self.assertFalse(report["p2Unblocked"])
            self.assertFalse(report["scaleConfirmed"])
            self.assertGreater(grid.summary()["occupiedCells"], 0)

    def test_outputs_are_byte_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "input"
            write_pair(root, 0, 0.0)
            write_pair(root, 1, 10.0)
            report_a, grid_a = scan_dataset.inspect_dataset(root, grid_size=32, chunk_vertices=2, prefer_numpy=False)
            report_b, grid_b = scan_dataset.inspect_dataset(root, grid_size=32, chunk_vertices=2, prefer_numpy=False)
            out_a = Path(temporary) / "out-a"
            out_b = Path(temporary) / "out-b"
            hashes_a = scan_dataset.write_outputs(report_a, grid_a, out_a)
            hashes_b = scan_dataset.write_outputs(report_b, grid_b, out_b)
            self.assertEqual(hashes_a, hashes_b)
            self.assertEqual(set(hashes_a), {
                "inspection.json",
                "inspection.md",
                "source_layout.png",
                "point_density.png",
                "vertical_spread.png",
                "source_support.png",
                "glb_ply_alignment.png",
            })
            for name in hashes_a:
                self.assertEqual((out_a / name).read_bytes(), (out_b / name).read_bytes())

    def test_private_comments_and_absolute_root_are_absent(self) -> None:
        secret = "C:/Users/Jozz/private 50.123 20.456"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "private-input"
            write_pair(root, 0, 0.0, secret=secret)
            report, grid = scan_dataset.inspect_dataset(root, grid_size=16, prefer_numpy=False)
            output = Path(temporary) / "out"
            scan_dataset.write_outputs(report, grid, output)
            combined = (output / "inspection.json").read_text("utf-8") + (output / "inspection.md").read_text("utf-8")
            self.assertNotIn(secret, combined)
            self.assertNotIn(str(root), combined)
            self.assertFalse(report["privacy"]["sourceRgbRendered"])
            self.assertFalse(report["privacy"]["georeferencingIncluded"])

    def test_unpaired_tile_ids_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_pair(root, 0, 0.0)
            extra = root / "model-ply" / "Data" / "MipTile_2"
            extra.mkdir(parents=True)
            (extra / "MipTile_2.ply").write_bytes(build_ply(20.0))
            with self.assertRaises(scan_dataset.DatasetInspectionError):
                scan_dataset.inspect_dataset(root, prefer_numpy=False)

    def test_spatially_distant_ply_blocks_p2(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_pair(root, 0, 0.0, ply_offset=1000.0)
            report, _ = scan_dataset.inspect_dataset(root, grid_size=16, prefer_numpy=False)
            self.assertEqual(report["datasetStatus"], "incompatible")
            self.assertEqual(report["pairs"][0]["classification"], "incompatible")
            self.assertFalse(report["p2Unblocked"])

    def test_expected_counts_are_a_hard_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            write_pair(root, 0, 0.0)
            with self.assertRaises(scan_dataset.DatasetInspectionError):
                scan_dataset.inspect_dataset(root, expected_glb=7, expected_ply=7, prefer_numpy=False)

    def test_accessor_must_stay_inside_its_own_buffer_view(self) -> None:
        with self.assertRaises(scan_dataset.DatasetInspectionError):
            scan_dataset.validate_glb_accessors(build_glb(0.0, invalid_index_accessor=True), "bad.glb")


if __name__ == "__main__":
    unittest.main()
