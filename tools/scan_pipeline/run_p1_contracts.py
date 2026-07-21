#!/usr/bin/env python3
"""Run the dependency-free P1/P1B/P2A scan contract suite.

The repository also contains roadmap experiment tests that intentionally require
NumPy/SciPy/Pillow/trimesh/scikit-image. Those tests belong to the separate
`requirements-experiments.txt` environment and must not silently turn NumPy
into a mandatory dependency of the source inspector, world-contract boundary,
render-only source preview, conservative surface-evidence pack, or derivative
catalog.
"""
from __future__ import annotations

from pathlib import Path
import unittest

TEST_FILES = (
    "test_scan_inspect.py",
    "test_scan_ply.py",
    "test_scan_glb_quality.py",
    "test_scan_dataset_inspect.py",
    "test_scan_frames.py",
    "test_scan_world_contracts.py",
    "test_scan_import_bundle.py",
    "test_scan_import_bundle_verify.py",
    "test_scan_source_frame_contract.py",
    "test_scan_owner_gate.py",
    "test_scan_gate_ps1.py",
    "test_scan_preview_pack.py",
    "test_scan_preview_pack_verify.py",
    "test_scan_preview_runtime_contract.py",
    "test_scan_p2a_local_gate_ps1.py",
    "test_scan_surface_evidence.py",
    "test_scan_surface_evidence_verify.py",
    "test_scan_derivative_catalog.py",
    "test_scan_derivative_catalog_verify.py",
)


def build_suite() -> unittest.TestSuite:
    root = Path(__file__).resolve().parents[2]
    test_dir = root / "tests" / "scan_pipeline"
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    for pattern in TEST_FILES:
        discovered = loader.discover(
            start_dir=str(test_dir),
            pattern=pattern,
            top_level_dir=str(test_dir),
        )
        suite.addTests(discovered)
    return suite


def main() -> int:
    result = unittest.TextTestRunner(verbosity=2).run(build_suite())
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
