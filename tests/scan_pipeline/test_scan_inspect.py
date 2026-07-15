from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zlib
import binascii

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_inspect.py"
spec = importlib.util.spec_from_file_location("scan_inspect", MODULE_PATH)
assert spec and spec.loader
scan_inspect = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scan_inspect)


def png_1x1_rgba() -> bytes:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", binascii.crc32(kind + payload) & 0xFFFFFFFF)
    raw = b"\x00\xff\x00\x00\xff"
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


def pad4(data: bytes, pad: bytes = b"\x00") -> bytes:
    return data + pad * ((-len(data)) % 4)


def build_fixture_glb(*, invalid_accessor: bool = False) -> bytes:
    positions_a = struct.pack("<9f", 0, 0, 0, 2, 0, 0, 0, 3, 0)
    indices_a = struct.pack("<3H", 0, 1, 2)
    positions_b = struct.pack("<9f", 1, 1, 1, 2, 1, 1, 1, 2, 1)
    indices_b = struct.pack("<3H", 0, 1, 2)
    image = png_1x1_rgba()

    parts = []
    views = []
    offset = 0
    for payload, target in [
        (positions_a, 34962),
        (indices_a, 34963),
        (positions_b, 34962),
        (indices_b, 34963),
        (image, None),
    ]:
        aligned = pad4(payload)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(payload), **({"target": target} if target else {})})
        parts.append(aligned)
        offset += len(aligned)
    bin_chunk = b"".join(parts)

    accessors = [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [2, 3, 0]},
        {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
        {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC3", "min": [1, 1, 1], "max": [2, 2, 1]},
        {"bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR"},
    ]
    if invalid_accessor:
        accessors[1]["byteOffset"] = 999999

    document = {
        "asset": {"version": "2.0", "generator": "unit-test"},
        "scene": 0,
        "scenes": [{"nodes": [0, 1]}],
        "nodes": [
            {"mesh": 0, "translation": [10, 0, 0]},
            {"mesh": 1, "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 5, 0, 1]},
        ],
        "meshes": [
            {"name": "A", "primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 0}, "indices": 1, "material": 0}]},
            {"name": "B", "primitives": [{"attributes": {"POSITION": 2}, "indices": 3, "material": 1}]},
        ],
        "materials": [
            {"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}},
            {"pbrMetallicRoughness": {"baseColorTexture": {"index": 1}}},
        ],
        "textures": [{"source": 0}, {"source": 0}],
        "images": [{"bufferView": 4, "mimeType": "image/png"}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(bin_chunk)}],
    }
    json_chunk = pad4(json.dumps(document, separators=(",", ":")).encode("utf-8"), b" ")
    total = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
    return (
        struct.pack("<4sII", b"glTF", 2, total)
        + struct.pack("<II", len(json_chunk), 0x4E4F534A)
        + json_chunk
        + struct.pack("<II", len(bin_chunk), 0x004E4942)
        + bin_chunk
    )


class ScanInspectTests(unittest.TestCase):
    def test_fixture_counts_bounds_and_image(self) -> None:
        report = scan_inspect.inspect_glb_bytes(build_fixture_glb(), "fixture.glb")
        self.assertEqual(report["vertexCount"], 6)
        self.assertEqual(report["triangleCount"], 2)
        self.assertEqual(report["materialCount"], 2)
        self.assertEqual(report["imageCount"], 1)
        self.assertEqual(report["images"][0]["width"], 1)
        self.assertEqual(report["images"][0]["height"], 1)
        self.assertEqual(report["worldBounds"]["min"], [1.0, 0.0, 0.0])
        self.assertEqual(report["worldBounds"]["max"], [12.0, 7.0, 1.0])
        self.assertFalse(report["hasNormals"])

    def test_invalid_header_is_rejected(self) -> None:
        with self.assertRaises(scan_inspect.ScanInspectionError):
            scan_inspect.inspect_glb_bytes(b"not a glb", "bad.glb")

    def test_accessor_out_of_range_is_rejected(self) -> None:
        with self.assertRaises(scan_inspect.ScanInspectionError):
            scan_inspect.inspect_glb_bytes(build_fixture_glb(invalid_accessor=True), "bad_accessor.glb")

    def test_package_json_and_png_outputs_are_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "fixture.glb").write_bytes(build_fixture_glb())
            report_a = scan_inspect.inspect_package(root, "fixture-package")
            report_b = scan_inspect.inspect_package(root, "fixture-package")
            self.assertEqual(report_a, report_b)
            out_a = root / "out-a"
            out_b = root / "out-b"
            files_a = scan_inspect.write_outputs(report_a, out_a)
            files_b = scan_inspect.write_outputs(report_b, out_b)
            self.assertEqual(files_a["jsonSha256"], files_b["jsonSha256"])
            self.assertEqual((out_a / "source_layout.png").read_bytes(), (out_b / "source_layout.png").read_bytes())
            self.assertEqual((out_a / "texture_inventory.png").read_bytes(), (out_b / "texture_inventory.png").read_bytes())


if __name__ == "__main__":
    unittest.main()
