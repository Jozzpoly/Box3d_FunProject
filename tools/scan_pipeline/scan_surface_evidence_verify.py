#!/usr/bin/env python3
"""Independently verify one private conservative surface-evidence pack."""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import sys
from typing import Any, Sequence

MODULE_DIR = Path(__file__).resolve().parent


def _load_module() -> Any:
    path = MODULE_DIR / "scan_surface_evidence.py"
    spec = importlib.util.spec_from_file_location("_jozz_surface_evidence_verify", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


surface = _load_module()


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    args = parser.parse_args(argv)
    try:
        summary = surface.verify_surface_evidence_pack(args.evidence)
    except (
        OSError,
        surface.SurfaceEvidenceError,
        surface.scan_import_bundle.ImportBundleError,
        surface.scan_frames.FrameContractError,
        surface.scan_world_contracts.WorldContractError,
        surface.scan_ply.PlyInspectionError,
        surface.scan_preview_pack.PreviewPackError,
    ) as exc:
        print(f"scan_surface_evidence_verify: ERROR: {exc}", file=sys.stderr)
        return 2
    print(
        "scan_surface_evidence_verify: OK | "
        f"path={args.evidence} evidence_sha256={summary['surfaceEvidenceContentSha256']} "
        f"grid={summary['width']}x{summary['height']} observed={summary['observedCellCount']} "
        f"revision={summary['sourceRevisionId']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
