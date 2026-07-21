from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
MODULE_PATH = ROOT / "tools" / "scan_pipeline" / "scan_surface_evidence.py"
spec = importlib.util.spec_from_file_location("scan_surface_evidence_tested", MODULE_PATH)
assert spec and spec.loader
surface = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = surface
spec.loader.exec_module(surface)

PREVIEW_TEST_PATH = Path(__file__).with_name("test_scan_preview_pack.py")
preview_spec = importlib.util.spec_from_file_location("scan_surface_preview_fixtures", PREVIEW_TEST_PATH)
assert preview_spec and preview_spec.loader
fixtures = importlib.util.module_from_spec(preview_spec)
sys.modules[preview_spec.name] = fixtures
preview_spec.loader.exec_module(fixtures)


def make_ply(points: list[tuple[float, float, float]]) -> bytes:
    header = (
        "ply\n"
        "format binary_little_endian 1.0\n"
        f"element vertex {len(points)}\n"
        "property float x\n"
        "property float y\n"
        "property float z\n"
        "end_header\n"
    ).encode("ascii")
    return header + b"".join(struct.pack("<fff", *point) for point in points)


def fixture(root: Path, *, points: list[tuple[float, float, float]] | None = None) -> tuple[Path, Path, Path]:
    glb = fixtures.make_glb()
    points = points or [
        (1001.0, 2002.0, 3003.0),
        (1002.0, 2002.0, 3003.0),
        (1001.0, 2003.0, 3003.0),
    ]
    ply = make_ply(points)
    report = fixtures.report(glb)
    report["totals"]["plyPoints"] = len(points)
    report["plyFiles"] = [
        {
            "tileId": 0,
            "sourceLabel": "MipTile_0.ply",
            "sha256": hashlib.sha256(ply).hexdigest(),
            "byteLength": len(ply),
        }
    ]
    documents = surface.scan_import_bundle.build_bundle_documents(
        package_id="scan/surface-test",
        proposal_id="proposal/surface-test/revision-1",
        inspection_report=report,
        frame_contract=fixtures.frame(confirmed=True),
        require_inspection_pass=True,
    )
    bundle = surface.scan_import_bundle.write_bundle_transactionally(
        documents=documents,
        output_root=root / "bundles",
        bundle_label="fixture",
    )
    receipt = fixtures._write_receipt(root, bundle)
    source_root = root / "source"
    source_root.mkdir()
    (source_root / "MipTile_0.glb").write_bytes(glb)
    (source_root / "MipTile_0.ply").write_bytes(ply)
    return bundle, receipt, source_root


def build(root: Path, *, cell_size: float = 0.4) -> Path:
    bundle, receipt, source_root = fixture(root)
    return surface.build_surface_evidence_pack(
        bundle=bundle,
        owner_gate_receipt=receipt,
        source_root=source_root,
        output_root=root / "surface",
        cell_size_meters=cell_size,
        chunk_vertices=2,
    )


def rewrite_manifest(pack: Path, manifest: dict[str, object]) -> None:
    unsigned = dict(manifest)
    unsigned.pop("surfaceEvidenceContentSha256", None)
    manifest["surfaceEvidenceContentSha256"] = surface._sha256_bytes(
        surface._canonical_json_bytes(unsigned)
    )
    (pack / "COMPLETE.json").write_bytes(surface._canonical_json_bytes(manifest))


class ScanSurfaceEvidenceTests(unittest.TestCase):
    def test_builds_verified_evidence_and_preserves_holes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = build(Path(temporary))
            summary = surface.verify_surface_evidence_pack(pack)
            manifest = surface._strict_json(pack / "COMPLETE.json")
            self.assertEqual((summary["width"], summary["height"]), (3, 3))
            self.assertEqual(summary["observedCellCount"], 3)
            self.assertEqual(manifest["statistics"]["unknownCellCount"], 6)
            self.assertEqual(manifest["purpose"], surface.PURPOSE)
            self.assertFalse(manifest["capabilities"]["groundClassified"])
            self.assertFalse(manifest["capabilities"]["collisionReady"])
            self.assertTrue(manifest["capabilities"]["holesPreserved"])

    def test_unknown_cells_are_canonical_and_never_filled(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = build(Path(temporary))
            data = (pack / "surface.bin").read_bytes()
            unknown = 0
            for index in range(9):
                values = surface.CELL.unpack_from(data, surface.HEADER.size + index * surface.CELL.size)
                if values[5] == surface.UNKNOWN:
                    unknown += 1
                    self.assertEqual(values[2:6], (0, 0, 0, surface.UNKNOWN))
                    self.assertEqual(
                        struct.unpack("<I", struct.pack("<f", values[0]))[0],
                        surface.CANONICAL_NAN_BITS,
                    )
            self.assertEqual(unknown, 6)

    def test_publication_is_content_addressed_and_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source_root = fixture(root)
            kwargs = dict(
                bundle=bundle,
                owner_gate_receipt=receipt,
                source_root=source_root,
                output_root=root / "surface",
                cell_size_meters=0.4,
                chunk_vertices=2,
            )
            first = surface.build_surface_evidence_pack(**kwargs)
            second = surface.build_surface_evidence_pack(**kwargs)
            self.assertEqual(first, second)
            self.assertTrue(first.name.endswith(surface.verify_surface_evidence_pack(first)["surfaceEvidenceContentSha256"][:16]))

    def test_source_hash_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source_root = fixture(root)
            path = source_root / "MipTile_0.ply"
            path.write_bytes(path.read_bytes() + b"x")
            with self.assertRaises(surface.SurfaceEvidenceError):
                surface.build_surface_evidence_pack(
                    bundle=bundle,
                    owner_gate_receipt=receipt,
                    source_root=source_root,
                    output_root=root / "surface",
                )

    def test_grid_cell_safety_limit_is_enforced(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle, receipt, source_root = fixture(root)
            with self.assertRaises(surface.SurfaceEvidenceError):
                surface.build_surface_evidence_pack(
                    bundle=bundle,
                    owner_gate_receipt=receipt,
                    source_root=source_root,
                    output_root=root / "surface",
                    cell_size_meters=0.00001,
                )

    def test_capability_overclaim_is_rejected_after_rehash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = build(Path(temporary))
            manifest = surface._strict_json(pack / "COMPLETE.json")
            manifest["capabilities"]["collisionReady"] = True
            rewrite_manifest(pack, manifest)
            with self.assertRaises(surface.SurfaceEvidenceError):
                surface.verify_surface_evidence_pack(pack)

    def test_invalid_source_mask_is_rejected_after_full_rehash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = build(Path(temporary))
            binary_path = pack / "surface.bin"
            data = bytearray(binary_path.read_bytes())
            for index in range(9):
                offset = surface.HEADER.size + index * surface.CELL.size
                values = list(surface.CELL.unpack_from(data, offset))
                if values[5] == surface.OBSERVED_SURFACE_EVIDENCE:
                    values[3] = 1 << 5
                    surface.CELL.pack_into(data, offset, *values)
                    break
            binary_path.write_bytes(data)
            manifest = surface._strict_json(pack / "COMPLETE.json")
            manifest["surfaceSha256"] = hashlib.sha256(data).hexdigest()
            rewrite_manifest(pack, manifest)
            with self.assertRaises(surface.SurfaceEvidenceError):
                surface.verify_surface_evidence_pack(pack)

    def test_noncanonical_unknown_cell_is_rejected_after_full_rehash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = build(Path(temporary))
            binary_path = pack / "surface.bin"
            data = bytearray(binary_path.read_bytes())
            for index in range(9):
                offset = surface.HEADER.size + index * surface.CELL.size
                values = list(surface.CELL.unpack_from(data, offset))
                if values[5] == surface.UNKNOWN:
                    values[0] = 0.0
                    surface.CELL.pack_into(data, offset, *values)
                    break
            binary_path.write_bytes(data)
            manifest = surface._strict_json(pack / "COMPLETE.json")
            manifest["surfaceSha256"] = hashlib.sha256(data).hexdigest()
            rewrite_manifest(pack, manifest)
            with self.assertRaises(surface.SurfaceEvidenceError):
                surface.verify_surface_evidence_pack(pack)

    def test_quality_is_evidence_only_and_recomputed_by_verifier(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = build(Path(temporary))
            data = bytearray((pack / "surface.bin").read_bytes())
            for index in range(9):
                offset = surface.HEADER.size + index * surface.CELL.size
                values = list(surface.CELL.unpack_from(data, offset))
                if values[5] == surface.OBSERVED_SURFACE_EVIDENCE:
                    self.assertGreater(values[4], 0)
                    values[4] = 255 if values[4] != 255 else 1
                    surface.CELL.pack_into(data, offset, *values)
                    break
            binary_path = pack / "surface.bin"
            binary_path.write_bytes(data)
            manifest = surface._strict_json(pack / "COMPLETE.json")
            manifest["surfaceSha256"] = hashlib.sha256(data).hexdigest()
            rewrite_manifest(pack, manifest)
            with self.assertRaises(surface.SurfaceEvidenceError):
                surface.verify_surface_evidence_pack(pack)


if __name__ == "__main__":
    unittest.main()
