from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

MODULE_PATH = (
    Path(__file__).parents[2]
    / "tools"
    / "scan_pipeline"
    / "scan_source_assets.py"
)
spec = importlib.util.spec_from_file_location("scan_source_assets_tested", MODULE_PATH)
assert spec and spec.loader
assets = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = assets
spec.loader.exec_module(assets)


def frame() -> dict[str, object]:
    return {
        "schema": "jozz.scan-source-frame",
        "schemaVersion": 1,
        "confirmed": True,
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
            "localOriginSource": [0.0, 0.0, 0.0],
        },
    }


def package_for(glb: dict[int, bytes], ply: dict[int, bytes]) -> dict[str, object]:
    inspection = {
        "schema": "jozz.scan-dataset-inspection",
        "schemaVersion": 3,
        "glbFiles": [
            {
                "tileId": tile_id,
                "sourceLabel": f"MipTile_{tile_id}.glb",
                "byteLength": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
            for tile_id, data in sorted(glb.items())
        ],
        "plyFiles": [
            {
                "tileId": tile_id,
                "sourceLabel": f"MipTile_{tile_id}.ply",
                "byteLength": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
            for tile_id, data in sorted(ply.items())
        ],
        "pairs": [{"tileId": tile_id} for tile_id in sorted(glb)],
    }
    return assets.scan_world_contracts.build_source_package(
        package_id="scan/resolver-test",
        inspection_report=inspection,
        frame_contract=frame(),
    )


def fixture(root: Path, count: int = 7) -> tuple[dict[str, object], Path]:
    glb = {tile_id: f"glb-{tile_id}".encode() for tile_id in range(count)}
    ply = {tile_id: f"ply-{tile_id}".encode() for tile_id in range(count)}
    source = root / "private-parent"
    for tile_id in range(count):
        glb_path = source / "model-glb" / "Data" / f"MipTile_{tile_id}" / f"MipTile_{tile_id}.glb"
        ply_path = source / "model-ply" / "Data" / f"MipTile_{tile_id}" / f"MipTile_{tile_id}.ply"
        glb_path.parent.mkdir(parents=True, exist_ok=True)
        ply_path.parent.mkdir(parents=True, exist_ok=True)
        glb_path.write_bytes(glb[tile_id])
        ply_path.write_bytes(ply[tile_id])
    (source / "noise.txt").write_text("ignored", encoding="utf-8")
    return package_for(glb, ply), source


def bundle_summary(package: dict[str, object]) -> dict[str, str]:
    return {
        "bundleContentSha256": "b" * 64,
        "sourceRevisionId": str(package["revisionId"]),
    }


class ScanSourceAssetsTests(unittest.TestCase):
    def test_nested_real_like_seven_plus_seven_materializes_canonical_view(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package, source = fixture(root)
            view = assets.resolve_source_package(
                source_package=package,
                bundle_summary=bundle_summary(package),
                source_roots=[source],
                output_root=root / "resolved",
            )
            summary = assets.verify_source_view(view)
            self.assertEqual(summary["assetCount"], 14)
            for tile_id in range(7):
                self.assertEqual((view / f"MipTile_{tile_id}.glb").read_bytes(), f"glb-{tile_id}".encode())
                self.assertEqual((view / f"MipTile_{tile_id}.ply").read_bytes(), f"ply-{tile_id}".encode())

    def test_repeated_resolution_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package, source = fixture(root, count=2)
            kwargs = dict(
                source_package=package,
                bundle_summary=bundle_summary(package),
                source_roots=[source],
                output_root=root / "resolved",
            )
            self.assertEqual(
                assets.resolve_source_package(**kwargs),
                assets.resolve_source_package(**kwargs),
            )

    def test_duplicate_exact_match_is_ambiguous(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package, source = fixture(root, count=1)
            duplicate = source / "duplicate" / "MipTile_0.glb"
            duplicate.parent.mkdir()
            duplicate.write_bytes(b"glb-0")
            with self.assertRaisesRegex(
                assets.SourceAssetResolutionError,
                "multiple exact private source matches",
            ):
                assets.resolve_source_package(
                    source_package=package,
                    bundle_summary=bundle_summary(package),
                    source_roots=[source],
                    output_root=root / "resolved",
                )

    def test_wrong_hash_does_not_match(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package, source = fixture(root, count=1)
            path = source / "model-glb" / "Data" / "MipTile_0" / "MipTile_0.glb"
            path.write_bytes(b"xxxxx")
            with self.assertRaisesRegex(
                assets.SourceAssetResolutionError,
                "no exact private source match",
            ):
                assets.resolve_source_package(
                    source_package=package,
                    bundle_summary=bundle_summary(package),
                    source_roots=[source],
                    output_root=root / "resolved",
                )

    def test_symlink_is_rejected_without_leaking_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package, source = fixture(root, count=1)
            target = source / "noise.txt"
            link = source / "private-linked-file"
            try:
                link.symlink_to(target)
            except (OSError, NotImplementedError):
                self.skipTest("symlinks are unavailable")
            with self.assertRaises(assets.SourceAssetResolutionError) as context:
                assets.resolve_source_package(
                    source_package=package,
                    bundle_summary=bundle_summary(package),
                    source_roots=[source],
                    output_root=root / "resolved",
                )
            self.assertNotIn(str(source), str(context.exception))
            self.assertIn("symlink", str(context.exception))

    def test_bundle_revision_mismatch_is_rejected_before_scanning(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package, source = fixture(root, count=1)
            with self.assertRaisesRegex(
                assets.SourceAssetResolutionError,
                "bundle revision differs",
            ):
                assets.resolve_source_package(
                    source_package=package,
                    bundle_summary={
                        "bundleContentSha256": "b" * 64,
                        "sourceRevisionId": "sha256:" + "0" * 64,
                    },
                    source_roots=[source],
                    output_root=root / "resolved",
                )


if __name__ == "__main__":
    unittest.main()
