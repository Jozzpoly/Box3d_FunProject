from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).parents[2]
PACK_CPP = ROOT / "samples" / "jozz_scan_preview_pack.cpp"
LAB_CPP = ROOT / "samples" / "jozz_scan_preview_lab.cpp"
LAB_HEADER = ROOT / "samples" / "jozz_scan_preview_lab.h"
PACK_HEADER = ROOT / "samples" / "jozz_scan_preview_pack.h"
SAMPLE_REGISTRY = ROOT / "samples" / "sample_jozz_vehicle_lab.cpp"
CMAKE = ROOT / "samples" / "CMakeLists.txt"


class ScanPreviewRuntimeContractTests(unittest.TestCase):
    def test_runtime_remains_render_only_and_cannot_create_physics(self) -> None:
        runtime = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (PACK_CPP, LAB_CPP, PACK_HEADER, LAB_HEADER)
        )
        forbidden = (
            "b3CreateBody",
            "b3DestroyBody",
            "b3CreateHullShape",
            "b3CreateMeshShape",
            "b3CreateHeightFieldShape",
            "b3CreateCompoundShape",
            "b3CreateSphereShape",
            "b3CreateCapsuleShape",
            "b3CreatePlaneShape",
            "b3ShapeDef",
            "b3BodyDef",
            "b3HeightFieldData",
            "AddGroundBox(",
            "CreateJozzWorldGround(",
        )
        for token in forbidden:
            self.assertNotIn(token, runtime, f"P2A runtime crossed physics boundary via {token}")

        self.assertIn("SOURCE EVIDENCE ONLY", runtime)
        self.assertIn("No collision shapes.", runtime)
        self.assertIn("Not an accepted world patch.", runtime)
        self.assertIn("SOURCE_VISUAL_PREVIEW_ONLY", runtime)
        self.assertIn("collisionReady", runtime)
        self.assertIn("acceptedWorld", runtime)

    def test_preview_is_explicitly_registered_only_in_samples_target(self) -> None:
        cmake = CMAKE.read_text(encoding="utf-8")
        registry = SAMPLE_REGISTRY.read_text(encoding="utf-8")
        for filename in (
            "jozz_scan_preview_lab.cpp",
            "jozz_scan_preview_lab.h",
            "jozz_scan_preview_pack.cpp",
            "jozz_scan_preview_pack.h",
        ):
            self.assertIn(filename, cmake)

        self.assertIn('"P2A Scan Source Preview"', registry)
        self.assertIn("CreateJozzScanSourcePreviewLab", registry)

        core_start = cmake.index("set(JOZZ_VEHICLE_CORE_FILES")
        core_end = cmake.index("set(SAMPLE_FILES", core_start)
        core = cmake[core_start:core_end]
        self.assertNotIn("jozz_scan_preview", core)

        validator_start = cmake.index("add_executable(jozz_vehicle_validation")
        validator_end = cmake.index(")", validator_start)
        validator = cmake[validator_start:validator_end]
        self.assertNotIn("jozz_scan_preview", validator)


if __name__ == "__main__":
    unittest.main()
