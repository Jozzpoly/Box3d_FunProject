#!/usr/bin/env python3
"""Verify one previously published scan-import evidence bundle.

This command is intentionally read-only. It rejects incomplete directories,
symlinks, unexpected files, hash mismatches and cross-document inconsistencies.
"""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import sys
from typing import Any, Sequence

MODULE_DIR = Path(__file__).resolve().parent


def _load_bundle_module() -> Any:
    path = MODULE_DIR / "scan_import_bundle.py"
    spec = importlib.util.spec_from_file_location("_jozz_scan_import_bundle_verify", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


scan_import_bundle = _load_bundle_module()


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args(argv)
    try:
        summary = scan_import_bundle.verify_bundle(args.bundle)
    except (
        OSError,
        scan_import_bundle.ImportBundleError,
        scan_import_bundle.scan_frames.FrameContractError,
        scan_import_bundle.scan_world_contracts.WorldContractError,
    ) as exc:
        print(f"scan_import_bundle_verify: ERROR: {exc}", file=sys.stderr)
        return 2
    print(
        "scan_import_bundle_verify: OK | "
        f"path={args.bundle} bundle_sha256={summary['bundleContentSha256']} "
        f"package={summary['packageId']} revision={summary['sourceRevisionId']} "
        f"proposal={summary['proposalId']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
