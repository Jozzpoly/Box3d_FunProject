from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

MODULE_PATH = Path(__file__).parents[2] / "tools" / "scan_pipeline" / "scan_ply.py"
spec = importlib.util.spec_from_file_location("scan_ply_tested", MODULE_PATH)
assert spec and spec.loader
scan_ply = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = scan_ply
spec.loader.exec_module(scan_ply)


def build_ply(
    points: list[tuple[float, float, float, int, int, int]],
    *,
    endian: str = "<",
    comment: str | None = None,
    extra_vertex_line: str | None = None,
    format_name: str | None = None,
) -> bytes:
    if format_name is None:
        format_name = "binary_little_endian" if endian == "<" else "binary_big_endian"
    lines = ["ply", f"format {format_name} 1.0"]
    if comment is not None:
        lines.append(f"comment {comment}")
    lines += [
        f"element vertex {len(points)}",
        "property float x",
        "property float y",
        "property float z",
        "property uchar red",
        "property uchar green",
        "property uchar blue",
    ]
    if extra_vertex_line:
        lines.append(extra_vertex_line)
    lines.append("end_header")
    header = ("\n".join(lines) + "\n").encode("ascii")
    if format_name == "ascii":
        body = "".join(
            f"{x} {y} {z} {r} {g} {b}\n"
            for x, y, z, r, g, b in points
        ).encode("ascii")
    else:
        body = b"".join(struct.pack(endian + "3f3B", *point) for point in points)
    return header + body


class ScanPlyTests(unittest.TestCase):
    def test_little_endian_counts_bounds_rgb_and_hash(self) -> None:
        points = [(-2.0, 4.0, 8.0, 1, 2, 3), (3.5, -1.0, 10.0, 4, 5, 6)]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "MipTile_0.ply"
            path.write_bytes(build_ply(points))
            report = scan_ply.inspect_ply(
                path,
                "safe/MipTile_0.ply",
                prefer_numpy=False,
                chunk_vertices=1,
            )
            self.assertEqual(report["vertexCount"], 2)
            self.assertEqual(report["bounds"]["min"], [-2.0, -1.0, 8.0])
            self.assertEqual(report["bounds"]["max"], [3.5, 4.0, 10.0])
            self.assertTrue(report["hasRgb"])
            self.assertTrue(report["fileStableDuringInspection"])
            self.assertEqual(report["streaming"]["backend"], "stdlib")
            self.assertEqual(report["sha256"], scan_ply.sha256_file(path))

    def test_big_endian_is_supported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "MipTile_1.ply"
            path.write_bytes(build_ply([(1.25, 2.5, -3.75, 7, 8, 9)], endian=">"))
            report = scan_ply.inspect_ply(path, prefer_numpy=False)
            self.assertEqual(report["format"], "binary_big_endian")
            self.assertEqual(report["bounds"]["center"], [1.25, 2.5, -3.75])

    def test_chunks_are_bounded_and_deterministic(self) -> None:
        points = [(float(i), float(i * 2), float(-i), i, i, i) for i in range(7)]
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "chunked.ply"
            path.write_bytes(build_ply(points))
            header = scan_ply.read_ply_header(path)
            first = list(
                scan_ply.iter_vertex_chunks(
                    path,
                    header,
                    chunk_vertices=3,
                    prefer_numpy=False,
                )
            )
            second = list(
                scan_ply.iter_vertex_chunks(
                    path,
                    header,
                    chunk_vertices=3,
                    prefer_numpy=False,
                )
            )
            self.assertEqual([len(chunk) for chunk in first], [3, 3, 1])
            self.assertEqual([chunk.x for chunk in first], [chunk.x for chunk in second])
            self.assertEqual(sum(len(chunk) for chunk in first), len(points))

    def test_fixed_size_element_before_vertex_is_skipped_exactly(self) -> None:
        header = (
            "ply\n"
            "format binary_little_endian 1.0\n"
            "element metadata 1\n"
            "property uchar flag\n"
            "element vertex 1\n"
            "property float x\n"
            "property float y\n"
            "property float z\n"
            "end_header\n"
        ).encode("ascii")
        raw = header + struct.pack("<B3f", 7, 1.0, 2.0, 3.0)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "prefixed.ply"
            path.write_bytes(raw)
            parsed = scan_ply.read_ply_header(path)
            self.assertEqual(parsed.data_offset, len(header) + 1)
            report = scan_ply.inspect_ply(path, prefer_numpy=False)
            self.assertEqual(report["bounds"]["center"], [1.0, 2.0, 3.0])

    def test_nonempty_zero_stride_element_before_vertex_is_rejected(self) -> None:
        raw = (
            "ply\n"
            "format binary_little_endian 1.0\n"
            "element metadata 1\n"
            "element vertex 1\n"
            "property float x\n"
            "property float y\n"
            "property float z\n"
            "end_header\n"
        ).encode("ascii") + struct.pack("<3f", 1.0, 2.0, 3.0)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "zero-stride.ply"
            path.write_bytes(raw)
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(path)

    def test_comments_and_obj_info_are_not_copied(self) -> None:
        secret = "C:/Users/Private/location 50.123 20.456"
        payload = build_ply([(0.0, 0.0, 0.0, 1, 1, 1)], comment=secret)
        payload = payload.replace(
            b"element vertex",
            f"obj_info {secret}\nelement vertex".encode("ascii"),
        )
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "private.ply"
            path.write_bytes(payload)
            report = scan_ply.inspect_ply(path, prefer_numpy=False)
            self.assertNotIn(secret, repr(report))
            self.assertNotIn(str(Path(temporary)), repr(report))

    def test_ascii_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "ascii.ply"
            path.write_bytes(
                build_ply(
                    [(0.0, 0.0, 0.0, 0, 0, 0)],
                    format_name="ascii",
                )
            )
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(path)

    def test_vertex_list_property_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "list.ply"
            path.write_bytes(
                build_ply(
                    [(0.0, 0.0, 0.0, 0, 0, 0)],
                    extra_vertex_line="property list uchar int neighbours",
                )
            )
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(path)

    def test_malformed_list_property_is_rejected(self) -> None:
        raw = (
            "ply\n"
            "format binary_little_endian 1.0\n"
            "element face 1\n"
            "property list uchar\n"
            "element vertex 1\n"
            "property float x\n"
            "property float y\n"
            "property float z\n"
            "end_header\n"
        ).encode("ascii") + struct.pack("<3f", 1.0, 2.0, 3.0)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "malformed-list.ply"
            path.write_bytes(raw)
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(path)

    def test_duplicate_element_and_property_are_rejected(self) -> None:
        duplicate_element = (
            "ply\nformat binary_little_endian 1.0\n"
            "element vertex 1\nproperty float x\nproperty float y\nproperty float z\n"
            "element vertex 1\nproperty float x\nproperty float y\nproperty float z\n"
            "end_header\n"
        ).encode("ascii") + struct.pack("<6f", 1, 2, 3, 4, 5, 6)
        duplicate_property = (
            "ply\nformat binary_little_endian 1.0\n"
            "element vertex 1\nproperty float x\nproperty float x\nproperty float y\nproperty float z\n"
            "end_header\n"
        ).encode("ascii") + struct.pack("<4f", 1, 2, 3, 4)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            a = root / "duplicate-element.ply"
            b = root / "duplicate-property.ply"
            a.write_bytes(duplicate_element)
            b.write_bytes(duplicate_property)
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(a)
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(b)

    def test_rgb_names_with_wrong_types_are_not_reported_as_rgb8(self) -> None:
        raw = (
            "ply\nformat binary_little_endian 1.0\n"
            "element vertex 1\n"
            "property float x\nproperty float y\nproperty float z\n"
            "property float red\nproperty float green\nproperty float blue\n"
            "end_header\n"
        ).encode("ascii") + struct.pack("<6f", 1.0, 2.0, 3.0, 0.1, 0.2, 0.3)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "float-rgb.ply"
            path.write_bytes(raw)
            report = scan_ply.inspect_ply(path, prefer_numpy=False)
            self.assertFalse(report["hasRgb"])

    def test_missing_xyz_is_rejected(self) -> None:
        raw = (
            "ply\nformat binary_little_endian 1.0\nelement vertex 1\n"
            "property float x\nproperty float y\nend_header\n"
        ).encode("ascii") + struct.pack("<2f", 1.0, 2.0)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "missing_z.ply"
            path.write_bytes(raw)
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(path)

    def test_truncated_payload_is_rejected_before_streaming(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "truncated.ply"
            path.write_bytes(build_ply([(1.0, 2.0, 3.0, 4, 5, 6)])[:-2])
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(path)

    def test_header_limit_is_enforced(self) -> None:
        raw = (
            b"ply\nformat binary_little_endian 1.0\n"
            + b"comment xxxxxxxxxx\n" * 100
            + b"end_header\n"
        )
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "huge_header.ply"
            path.write_bytes(raw)
            with self.assertRaises(scan_ply.PlyInspectionError):
                scan_ply.read_ply_header(path, max_header_bytes=64)

    def test_file_change_during_inspection_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "mutating.ply"
            path.write_bytes(build_ply([(1.0, 2.0, 3.0, 4, 5, 6)]))
            original_hash = scan_ply.sha256_file

            def mutating_hash(target: Path, block_bytes: int = 4 * 1024 * 1024) -> str:
                digest = original_hash(target, block_bytes)
                with target.open("ab") as handle:
                    handle.write(b"x")
                return digest

            scan_ply.sha256_file = mutating_hash
            try:
                with self.assertRaises(scan_ply.PlyInspectionError):
                    scan_ply.inspect_ply(path, prefer_numpy=False)
            finally:
                scan_ply.sha256_file = original_hash


if __name__ == "__main__":
    unittest.main()
