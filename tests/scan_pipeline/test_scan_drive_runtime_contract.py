from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).parents[2]
DRIVE_CPP = ROOT / "samples" / "jozz_vehicle_scan_drive_lab.cpp"
DRIVE_HEADER = ROOT / "samples" / "jozz_vehicle_scan_drive_lab.h"
GEOMETRY_CPP = ROOT / "samples" / "jozz_vehicle_scan_geometry.cpp"
GEOMETRY_HEADER = ROOT / "samples" / "jozz_vehicle_scan_geometry.h"
PREVIEW_PACK_CPP = ROOT / "samples" / "jozz_scan_preview_pack.cpp"
SAMPLE_REGISTRY = ROOT / "samples" / "sample_jozz_vehicle_lab.cpp"
CMAKE = ROOT / "samples" / "CMakeLists.txt"

SKIN_CPP = ROOT / "samples" / "jozz_vehicle_m6_visual_skin.cpp"
RIG_LAB_INTERNAL = ROOT / "samples" / "jozz_vehicle_m6_rig_lab_internal.h"

NEW_FILES = (
    "jozz_vehicle_scan_drive_lab.cpp",
    "jozz_vehicle_scan_drive_lab.h",
    "jozz_vehicle_scan_geometry.cpp",
    "jozz_vehicle_scan_geometry.h",
    "jozz_vehicle_m6_visual_skin.cpp",
    "jozz_vehicle_m6_visual_skin.h",
)


class ScanDriveCollisionContractTests(unittest.TestCase):
    """P2B is the POSITIVE counterpart of test_scan_preview_runtime_contract:
    P2A must stay render-only, and the collision that P2A refuses to create has
    to live here instead - as a real static mesh the M6 car can drive on."""

    def test_drive_sample_builds_static_scan_mesh_collision(self) -> None:
        drive = DRIVE_CPP.read_text(encoding="utf-8")
        for token in (
            "b3CreateMesh(",
            "b3CreateMeshShape(",
            "b3DestroyMesh(",  # the mesh must outlive the shape, then be freed
            "JOZZ_M6_TERRAIN_CATEGORY",  # so the M6 split wheel envelope collides
            "CreateJozzVehicleM6(",  # the CURRENT car, never the archival M5
        ):
            self.assertIn(token, drive, f"P2B drive sample lost {token}")
        self.assertNotIn("CreateJozzVehicleM5", drive)

    def test_drive_sample_reuses_shared_reader_and_pack_discovery(self) -> None:
        # Collision geometry and textured visuals must come from the SAME pack,
        # resolved by the same discovery helper the P2A lab uses.
        drive = DRIVE_CPP.read_text(encoding="utf-8")
        self.assertIn("LoadJozzScanPackGeometry(", drive)
        self.assertIn("FindJozzActiveScanPreviewPack(", drive)

    def test_geometry_reader_stays_pure(self) -> None:
        # The reader is what lets the physics sample share P2A's bytes without
        # dragging physics into P2A. It must stay free of physics, renderer and
        # UI dependencies so it can be linked anywhere.
        reader = GEOMETRY_CPP.read_text(encoding="utf-8") + GEOMETRY_HEADER.read_text(encoding="utf-8")
        for include in ('#include "imgui.h"', '#include "gfx/', '#include "sample.h"', '#include "box3d/box3d.h"'):
            self.assertNotIn(include, reader, f"scan geometry reader gained {include}")
        for symbol in (
            "b3CreateBody",
            "b3CreateMeshShape",
            "b3ShapeDef",
            "b3BodyDef",
            "b3World_",
            "RegisterTexturedMesh",
            "MeshHandle",
        ):
            self.assertNotIn(symbol, reader, f"scan geometry reader crossed a boundary via {symbol}")

    def test_headless_self_verification_hook_exists(self) -> None:
        # The system that keeps future scans cheap: a machine-checkable "the car
        # stayed on the surface" line, no human at the keyboard.
        drive = DRIVE_CPP.read_text(encoding="utf-8")
        self.assertIn("JOZZ_SCANDRIVE_SETTLE_DUMP", drive)
        self.assertIn("JOZZ_SCANDRIVE_SETTLE ", drive)
        self.assertIn("wheel_contacts", drive)

    def test_preview_pack_still_creates_no_collision(self) -> None:
        preview = PREVIEW_PACK_CPP.read_text(encoding="utf-8")
        self.assertNotIn("b3CreateMeshShape", preview)
        self.assertNotIn("b3CreateMesh(", preview)

    def test_drive_sample_is_registered(self) -> None:
        registry = SAMPLE_REGISTRY.read_text(encoding="utf-8")
        self.assertIn('"P2B Scan Drive (M6)"', registry)
        self.assertIn("CreateJozzVehicleScanDriveLab", registry)
        self.assertIn("CreateJozzVehicleScanDriveLab", DRIVE_HEADER.read_text(encoding="utf-8"))

    def test_drive_sample_wears_the_real_model_not_debug_primitives(self) -> None:
        # The car on the scan must be Jozz's actual model - body, wheels AND the
        # articulated suspension - not the debug box and cylinders.
        drive = DRIVE_CPP.read_text(encoding="utf-8")
        self.assertIn("JozzVehicleM6VisualSkin", drive)
        # Without the rest-pose bake the suspension silently does not draw.
        self.assertIn("AttachToVehicle(", drive)
        self.assertIn("SetShapeHidden(", drive)  # collision primitives hide under the skin

        skin = SKIN_CPP.read_text(encoding="utf-8")
        for token in ("LoadSkinnedGltf(", "DrawPartBetween(", "DrawTelescopingDamper("):
            self.assertIn(token, skin, f"visual skin lost {token}")

    def test_car_setup_files_match_the_workshop(self) -> None:
        # The scan sample reads the M6 workshop's own session + presets so the
        # car you drive is the car you tuned. Those paths are duplicated (the
        # lab's header is private), so pin them together here - a rename on
        # either side must not silently split the two into separate setups.
        drive = DRIVE_CPP.read_text(encoding="utf-8")
        internal = RIG_LAB_INTERNAL.read_text(encoding="utf-8")
        for path in ("build/jozz_vehicle_m6_session.json", "assets/vehicle_presets"):
            self.assertIn(path, internal, f"workshop no longer uses {path}")
            self.assertIn(path, drive, f"scan drive sample no longer reads {path}")
        # Presets must load with deterministic (factory-overridden) semantics,
        # never in place - see LoadJozzVehicleM6PresetConfig's header comment.
        self.assertIn("LoadJozzVehicleM6PresetConfig(", drive)
        self.assertIn("SanitizeJozzVehicleM6Config(", drive)

    def test_restart_does_not_wipe_tuning(self) -> None:
        # "R" reconstructs the sample from scratch (SelectSample deletes the old
        # instance before building the new one), so a load with no matching save
        # silently discards every dial the user touched. This is the exact bug
        # the workshop's session file exists to prevent; a second sample with a
        # tuning UI reintroduces it unless it saves on the way out too.
        drive = DRIVE_CPP.read_text(encoding="utf-8")
        self.assertIn("SaveJozzVehicleM6Config(", drive, "P2B never persists its tuning: R will wipe it")
        self.assertIn("kScanSessionFilePath", drive)
        # View toggles + the drop point must NOT live in the vehicle config, or
        # loading a preset would also flip the user's view and teleport them.
        self.assertIn("kScanViewStatePath", drive)
        for key in ("showTextures=", "spawnAnchorX="):
            self.assertIn(key, drive, f"view state lost {key}")

    def test_scan_tuning_never_silently_overwrites_the_workshop(self) -> None:
        # P2B keeps its own session file. Writing the workshop's on every exit
        # would mean driving the scan quietly rewrites the setup you built
        # there - the same class of quiet data loss, one level up.
        # Scoped to the DESTRUCTOR: that is the automatic path. Writing the
        # workshop file from an explicit button is a deliberate, user-initiated
        # push and must stay allowed.
        drive = DRIVE_CPP.read_text(encoding="utf-8")
        start = drive.index("~JozzVehicleScanDriveLab()")
        destructor = drive[start : drive.index("\n\t}", start)]
        self.assertIn(
            "SaveJozzVehicleM6Config( m_config, kScanSessionFilePath )",
            destructor,
            "the destructor must persist the scan session, or R wipes the tuning",
        )
        self.assertNotIn(
            "kM6SessionFilePath",
            destructor,
            "driving the scan must not silently rewrite the workshop's saved setup",
        )

    def test_new_files_are_samples_target_only(self) -> None:
        cmake = CMAKE.read_text(encoding="utf-8")
        core_start = cmake.index("set(JOZZ_VEHICLE_CORE_FILES")
        core_end = cmake.index("set(SAMPLE_FILES", core_start)
        core = cmake[core_start:core_end]
        validator_start = cmake.index("add_executable(jozz_vehicle_validation")
        validator_end = cmake.index(")", validator_start)
        validator = cmake[validator_start:validator_end]
        for filename in NEW_FILES:
            self.assertIn(filename, cmake, f"{filename} is not built at all")
            self.assertNotIn(filename, core)
            self.assertNotIn(filename, validator)


if __name__ == "__main__":
    unittest.main()
