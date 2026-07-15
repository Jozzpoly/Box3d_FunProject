from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest

import numpy as np

ROOT = Path(__file__).parents[2]
TOOLS = ROOT / "tools" / "scan_pipeline"


def load_module(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, TOOLS / filename)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    import sys
    sys.path.insert(0, str(TOOLS))
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


experiments = load_module("scan_plan_experiments", "scan_plan_experiments.py")
filters = load_module("scan_ground_filters", "scan_ground_filters.py")
drive = load_module("scan_drive_probe", "scan_drive_probe.py")
chunk = load_module("scan_chunk_probe", "scan_chunk_probe.py")


def plane_triangles(z_offset: float = 0.0) -> np.ndarray:
    return np.array([
        [[0.0, 0.0, z_offset], [2.0, 0.0, 2.0 + z_offset], [0.0, 2.0, 2.0 + z_offset]],
        [[2.0, 0.0, 2.0 + z_offset], [2.0, 2.0, 4.0 + z_offset], [0.0, 2.0, 2.0 + z_offset]],
    ], dtype=np.float64)


class ScanPlanExperimentTests(unittest.TestCase):
    def test_triangle_rasterizer_matches_plane(self) -> None:
        surface = experiments.rasterize_surface(
            "plane.glb", plane_triangles(), (0.0, 0.0, 2.0, 2.0), 1.0, slope_limit_deg=80.0
        )
        expected = np.array([
            [0.0, 1.0, 2.0],
            [1.0, 2.0, 3.0],
            [2.0, 3.0, 4.0],
        ], dtype=np.float32)
        np.testing.assert_allclose(surface.bottom, expected, atol=1e-6)
        np.testing.assert_allclose(surface.top, expected, atol=1e-6)

    def test_pairwise_seam_metrics_recover_vertical_offset(self) -> None:
        first = np.repeat(plane_triangles(0.0), 100, axis=0)
        second = np.repeat(plane_triangles(0.2), 100, axis=0)
        metrics, _ = experiments.pairwise_seam_metrics({"a.glb": first, "b.glb": second})
        self.assertEqual(len(metrics), 1)
        self.assertEqual(metrics[0]["status"], "measured")
        self.assertAlmostEqual(metrics[0]["verticalAtNearestXY"]["median"], 0.2, places=5)

    def test_heightfield_split_copies_exact_shared_edges(self) -> None:
        dem = np.arange(25, dtype=np.float32).reshape(5, 5)
        with tempfile.TemporaryDirectory() as temporary:
            manifest = experiments.split_quantized_heightfield(
                dem, Path(temporary), 1.0, (0.0, 0.0, 4.0, 4.0)
            )
            self.assertTrue(manifest["allSharedEdgesExact"])
            self.assertEqual(manifest["chunkPointDimensions"], [3, 3])

    def test_morphology_suppresses_compact_elevated_object(self) -> None:
        y, x = np.mgrid[0:41, 0:41]
        terrain = (0.02 * x + 0.01 * y).astype(np.float32)
        surface = terrain.copy()
        surface[15:26, 15:26] += 3.0
        result = filters.progressive_morphological_ground(
            surface, 0.5, filters.DEFAULT_PROFILES[1]
        )
        ground = result["ground"]
        object_height = result["objectHeight"]
        self.assertGreater(float(np.median(object_height[17:24, 17:24])), 2.5)
        self.assertLess(float(np.median(np.abs(ground - terrain))), 0.15)

    def test_box3d_obj_export_has_expected_counts_and_mapping(self) -> None:
        ground = np.zeros((3, 3), dtype=np.float32)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "pilot.obj"
            manifest = filters.export_box3d_obj(
                ground, (0.0, 0.0, 2.0, 2.0), 1.0, path,
                origin_source_xyz=(1.0, 1.0, 0.0),
            )
            self.assertEqual(manifest["vertexCount"], 9)
            self.assertEqual(manifest["triangleCount"], 8)
            text = path.read_text(encoding="utf-8")
            self.assertIn("v -1.000000 0.000000 1.000000", text)
            self.assertIn("f 1 2 4", text)
            vertices = []
            faces = []
            for line in text.splitlines():
                if line.startswith("v "):
                    vertices.append([float(value) for value in line.split()[1:]])
                elif line.startswith("f "):
                    faces.append([int(value) - 1 for value in line.split()[1:]])
            triangle = np.asarray(vertices)[faces[0]]
            normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
            self.assertGreater(normal[1], 0.0)

    def test_drive_probe_flat_surface_is_quiet(self) -> None:
        height = np.zeros((81, 81), dtype=np.float64)
        path = {"name": "flat", "start": [-15.0, 0.0], "end": [15.0, 0.0]}
        result, _ = drive.evaluate_path(
            height, path, (-20.0, -20.0, 20.0, 20.0), 0.5,
            wheelbase=2.5, track=2.1, total_travel=0.7,
        )
        self.assertEqual(result["status"], "measured")
        self.assertAlmostEqual(result["wheelHeightStepMeters"]["max"], 0.0)
        self.assertAlmostEqual(result["articulationSpanMeters"]["max"], 0.0)
        self.assertTrue(drive.summarize_surface([result], 0.7)["provisionalPass"])

    def test_drive_probe_detects_cross_axle_step(self) -> None:
        height = np.zeros((81, 81), dtype=np.float64)
        # A diagonal quadrant step twists the rectangular wheel footprint.
        height[40:, 40:] = 1.0
        path = {"name": "step", "start": [-10.0, 0.0], "end": [10.0, 0.0]}
        result, _ = drive.evaluate_path(
            height, path, (-20.0, -20.0, 20.0, 20.0), 0.5,
            wheelbase=2.5, track=2.1, total_travel=0.7,
        )
        self.assertGreater(result["wheelHeightStepMeters"]["max"], 0.15)
        self.assertGreater(result["articulationSpanMeters"]["max"], 0.45)


    def test_chunk_assignment_duplicates_only_boundary_crossers(self) -> None:
        triangles = np.array([
            [[1.0, 1.0, 0.0], [2.0, 1.0, 0.0], [1.0, 2.0, 0.0]],
            [[9.5, 1.0, 0.0], [10.5, 1.0, 0.0], [9.5, 2.0, 0.0]],
        ], dtype=np.float64)
        from collections import defaultdict
        counts = defaultdict(int)
        assignments, crossing = chunk.add_batch_counts(
            counts, triangles, origin_x=0.0, origin_y=0.0, chunk_size=10.0
        )
        self.assertEqual(crossing, 1)
        self.assertEqual(assignments, 3)
        self.assertEqual(counts[(0, 0)], 2)
        self.assertEqual(counts[(1, 0)], 1)



if __name__ == "__main__":
    unittest.main()
