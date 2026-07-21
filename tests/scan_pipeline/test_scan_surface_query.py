from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
QUERY_PATH = ROOT / "tools" / "scan_pipeline" / "scan_surface_query.py"
SURFACE_TEST_PATH = Path(__file__).with_name("test_scan_surface_evidence.py")


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


query = _load("scan_surface_query_tested", QUERY_PATH)
fixture = _load("scan_surface_query_fixture", SURFACE_TEST_PATH)


class ScanSurfaceQueryTests(unittest.TestCase):
    def test_observed_query_returns_exact_evidence_and_provenance(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = fixture.build(Path(temporary))
            with query.SurfaceEvidenceQuery(pack) as view:
                observed = list(view.iter_observed())
                self.assertEqual(len(observed), 3)
                sample = observed[0]
                self.assertTrue(sample.is_observed)
                self.assertIsNotNone(sample.lowest_height_meters)
                self.assertIsNotNone(sample.highest_height_meters)
                self.assertGreater(sample.support_count, 0)
                self.assertEqual(sample.source_tile_ids, (0,))
                self.assertGreater(sample.evidence_quality, 0)

    def test_unknown_cell_stays_unknown_and_has_no_height(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = fixture.build(Path(temporary))
            with query.SurfaceEvidenceQuery(pack) as view:
                unknown = None
                for cell_z in range(view.height):
                    for cell_x in range(view.width):
                        x = view.origin_x_meters + (cell_x + 0.5) * view.cell_size_meters
                        z = view.origin_z_meters + (cell_z + 0.5) * view.cell_size_meters
                        sample = view.sample(x, z)
                        if sample.status == query.UNKNOWN:
                            unknown = (x, z, sample)
                            break
                    if unknown:
                        break
                self.assertIsNotNone(unknown)
                x, z, sample = unknown
                self.assertFalse(sample.is_observed)
                self.assertIsNone(sample.lowest_height_meters)
                self.assertIsNone(view.observed_lowest_height(x, z))
                self.assertEqual(sample.support_count, 0)
                self.assertEqual(sample.source_tile_ids, ())

    def test_outside_query_is_explicit(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = fixture.build(Path(temporary))
            with query.SurfaceEvidenceQuery(pack) as view:
                sample = view.sample(
                    view.origin_x_meters - view.cell_size_meters,
                    view.origin_z_meters,
                )
                self.assertEqual(sample.status, query.OUTSIDE)
                self.assertIsNone(sample.cell_x)
                self.assertIsNone(sample.lowest_height_meters)

    def test_nonfinite_coordinates_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = fixture.build(Path(temporary))
            with query.SurfaceEvidenceQuery(pack) as view:
                with self.assertRaises(query.SurfaceQueryError):
                    view.sample(float("nan"), 0.0)
                with self.assertRaises(query.SurfaceQueryError):
                    view.sample(0.0, float("inf"))

    def test_tampered_pack_is_rejected_before_mapping(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            pack = fixture.build(Path(temporary))
            binary = pack / "surface.bin"
            damaged = bytearray(binary.read_bytes())
            damaged[-1] ^= 1
            binary.write_bytes(damaged)
            with self.assertRaises(query.surface.SurfaceEvidenceError):
                query.SurfaceEvidenceQuery(pack)


if __name__ == "__main__":
    unittest.main()
