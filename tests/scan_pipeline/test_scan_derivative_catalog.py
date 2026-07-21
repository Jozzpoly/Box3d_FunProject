from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
CATALOG_PATH = ROOT / "tools" / "scan_pipeline" / "scan_derivative_catalog.py"
SURFACE_TEST_PATH = Path(__file__).with_name("test_scan_surface_evidence.py")
PREVIEW_TEST_PATH = Path(__file__).with_name("test_scan_preview_pack.py")


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


catalog = _load("scan_derivative_catalog_tested", CATALOG_PATH)
surface_fixture = _load("surface_fixture_catalog", SURFACE_TEST_PATH)
preview_fixture = _load("preview_fixture_catalog", PREVIEW_TEST_PATH)


def _rehash(document: dict[str, object]) -> None:
    unsigned = dict(document)
    unsigned.pop("catalogContentSha256", None)
    document["catalogContentSha256"] = catalog._sha256_bytes(
        catalog._canonical_json_bytes(unsigned)
    )


def _build_matching_derivatives(root: Path) -> tuple[Path, Path]:
    """Build exact preview and surface evidence from one verified source revision."""
    bundle, receipt, source_root = surface_fixture.fixture(root)
    preview = preview_fixture.preview.build_preview_pack(
        bundle=bundle,
        owner_gate_receipt=receipt,
        source_root=source_root,
        output_root=root / "preview",
    )
    evidence = surface_fixture.surface.build_surface_evidence_pack(
        bundle=bundle,
        owner_gate_receipt=receipt,
        source_root=source_root,
        output_root=root / "surface",
        cell_size_meters=0.4,
        chunk_vertices=2,
    )
    return preview, evidence


class ScanDerivativeCatalogTests(unittest.TestCase):
    def test_exact_only_catalog_preserves_future_gates(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            preview = preview_fixture.build_fixture_preview(Path(temporary))
            manifest = catalog._strict_json(preview / "COMPLETE.json")
            document = catalog.build_catalog_document(preview_manifest=manifest)
            by_kind = {entry["kind"]: entry for entry in document["entries"]}
            self.assertEqual(by_kind[catalog.EXACT_VISUAL]["status"], "READY")
            self.assertEqual(by_kind[catalog.OPTIMIZED_VISUAL]["status"], "NOT_BUILT")
            self.assertEqual(by_kind[catalog.SURFACE_EVIDENCE]["status"], "NOT_BUILT")
            self.assertEqual(
                by_kind[catalog.ACCEPTED_SURFACE]["status"],
                "BLOCKED_MISSING_SURFACE_EVIDENCE",
            )
            self.assertEqual(
                by_kind[catalog.COLLISION_PROJECTION]["status"],
                "BLOCKED_ACCEPTED_SURFACE",
            )
            self.assertEqual(document["runtimePolicies"]["renderInterestCenter"], "CAMERA")
            self.assertEqual(document["runtimePolicies"]["physicsInterestCenter"], "VEHICLE")

    def test_surface_evidence_becomes_queryable_but_not_authoritative(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            preview, evidence = _build_matching_derivatives(Path(temporary))
            document = catalog.build_catalog_document(
                preview_manifest=catalog._strict_json(preview / "COMPLETE.json"),
                surface_manifest=catalog._strict_json(evidence / "COMPLETE.json"),
            )
            by_kind = {entry["kind"]: entry for entry in document["entries"]}
            surface = by_kind[catalog.SURFACE_EVIDENCE]
            self.assertEqual(surface["status"], "READY")
            self.assertTrue(surface["capabilities"]["surfaceQueryable"])
            self.assertFalse(surface["capabilities"]["acceptedWorld"])
            self.assertFalse(surface["capabilities"]["collisionReady"])
            self.assertEqual(
                by_kind[catalog.ACCEPTED_SURFACE]["status"],
                "BLOCKED_OWNER_REVIEW",
            )

    def test_collision_cannot_bypass_accepted_surface_after_rehash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            preview = preview_fixture.build_fixture_preview(Path(temporary))
            document = catalog.build_catalog_document(
                preview_manifest=catalog._strict_json(preview / "COMPLETE.json")
            )
            collision = next(
                entry
                for entry in document["entries"]
                if entry["kind"] == catalog.COLLISION_PROJECTION
            )
            collision["status"] = "READY"
            collision["contentSha256"] = "a" * 64
            collision["resourceSummary"] = {"assetBytes": 1}
            collision["blockingReason"] = None
            collision["capabilities"]["collisionReady"] = True
            _rehash(document)
            with self.assertRaises(catalog.DerivativeCatalogError):
                catalog.validate_catalog(document)

    def test_surface_and_preview_revision_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            preview, evidence = _build_matching_derivatives(Path(temporary))
            preview_manifest = catalog._strict_json(preview / "COMPLETE.json")
            surface_manifest = catalog._strict_json(evidence / "COMPLETE.json")
            self.assertEqual(
                preview_manifest["sourceRevisionId"],
                surface_manifest["sourceRevisionId"],
            )
            self.assertEqual(
                preview_manifest["sourceBundleContentSha256"],
                surface_manifest["sourceBundleContentSha256"],
            )
            surface_manifest["sourceRevisionId"] = "sha256:" + "f" * 64
            unsigned = dict(surface_manifest)
            unsigned.pop("surfaceEvidenceContentSha256")
            surface_manifest["surfaceEvidenceContentSha256"] = catalog._sha256_bytes(
                catalog._canonical_json_bytes(unsigned)
            )
            with self.assertRaises(catalog.DerivativeCatalogError):
                catalog.build_catalog_document(
                    preview_manifest=preview_manifest,
                    surface_manifest=surface_manifest,
                )

    def test_runtime_policy_or_graph_edge_mutation_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            preview = preview_fixture.build_fixture_preview(Path(temporary))
            document = catalog.build_catalog_document(
                preview_manifest=catalog._strict_json(preview / "COMPLETE.json")
            )
            document["runtimePolicies"]["physicsInterestCenter"] = "CAMERA"
            _rehash(document)
            with self.assertRaises(catalog.DerivativeCatalogError):
                catalog.validate_catalog(document)

        with tempfile.TemporaryDirectory() as temporary:
            preview = preview_fixture.build_fixture_preview(Path(temporary))
            document = catalog.build_catalog_document(
                preview_manifest=catalog._strict_json(preview / "COMPLETE.json")
            )
            collision = next(
                entry
                for entry in document["entries"]
                if entry["kind"] == catalog.COLLISION_PROJECTION
            )
            collision["parents"] = [catalog.SOURCE_REVISION]
            _rehash(document)
            with self.assertRaises(catalog.DerivativeCatalogError):
                catalog.validate_catalog(document)

    def test_resource_accounting_is_derived_from_payloads(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            preview, evidence = _build_matching_derivatives(Path(temporary))
            preview_manifest = catalog._strict_json(preview / "COMPLETE.json")
            surface_manifest = catalog._strict_json(evidence / "COMPLETE.json")
            document = catalog.build_catalog_document(
                preview_manifest=preview_manifest,
                surface_manifest=surface_manifest,
            )
            exact = document["entries"][0]
            surface = document["entries"][2]
            self.assertEqual(
                exact["resourceSummary"]["assetBytes"],
                sum(tile["byteLength"] for tile in preview_manifest["tiles"]),
            )
            self.assertEqual(
                exact["resourceSummary"]["triangleCount"],
                sum(tile["triangleCount"] for tile in preview_manifest["tiles"]),
            )
            self.assertEqual(
                surface["resourceSummary"]["assetBytes"],
                surface_manifest["surfaceByteLength"],
            )
            self.assertEqual(
                surface["resourceSummary"]["observedCellCount"]
                + surface["resourceSummary"]["unknownCellCount"],
                surface["resourceSummary"]["cellCount"],
            )

    def test_catalog_writer_refuses_silent_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            preview = preview_fixture.build_fixture_preview(root / "preview")
            output = root / "catalog.json"
            first = catalog.write_catalog(preview=preview, output=output)
            self.assertEqual(first, output)
            with self.assertRaises(catalog.DerivativeCatalogError):
                catalog.write_catalog(preview=preview, output=output)
            catalog.write_catalog(preview=preview, output=output, force=True)
            catalog.validate_catalog(catalog._strict_json(output))


if __name__ == "__main__":
    unittest.main()
