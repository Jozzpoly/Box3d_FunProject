#!/usr/bin/env python3
"""Independently verify one private source visual preview pack."""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import sys
from typing import Any, Sequence

MODULE_DIR = Path(__file__).resolve().parent


def _load_preview_module() -> Any:
    path = MODULE_DIR / "scan_preview_pack.py"
    spec = importlib.util.spec_from_file_location("_jozz_scan_preview_pack_verify", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


preview = _load_preview_module()


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("preview", type=Path)
    args = parser.parse_args(argv)
    try:
        summary = preview.verify_preview_pack(args.preview)
    except (
        OSError,
        preview.PreviewPackError,
        preview.scan_import_bundle.ImportBundleError,
        preview.scan_frames.FrameContractError,
        preview.scan_world_contracts.WorldContractError,
        preview.scan_inspect.ScanInspectionError,
    ) as exc:
        print(f"scan_preview_pack_verify: ERROR: {exc}", file=sys.stderr)
        return 2
    print(
        "scan_preview_pack_verify: OK | "
        f"path={args.preview} preview_sha256={summary['previewContentSha256']} "
        f"tiles={summary['tileCount']} revision={summary['sourceRevisionId']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
