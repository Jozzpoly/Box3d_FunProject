#!/usr/bin/env python3
"""Independently verify one private scan derivative catalog."""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import sys
from typing import Any, Sequence

MODULE_DIR = Path(__file__).resolve().parent


def _load_module() -> Any:
    path = MODULE_DIR / "scan_derivative_catalog.py"
    spec = importlib.util.spec_from_file_location("_jozz_derivative_catalog_verify", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


catalog = _load_module()


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path)
    args = parser.parse_args(argv)
    try:
        document = catalog.validate_catalog(catalog._strict_json(args.catalog))
    except (
        OSError,
        catalog.DerivativeCatalogError,
        catalog.scan_import_bundle.ImportBundleError,
        catalog.scan_preview_pack.PreviewPackError,
        catalog.scan_surface_evidence.SurfaceEvidenceError,
    ) as exc:
        print(f"scan_derivative_catalog_verify: ERROR: {exc}", file=sys.stderr)
        return 2
    print(
        "scan_derivative_catalog_verify: OK | "
        f"path={args.catalog} catalog_sha256={document['catalogContentSha256']} "
        f"revision={document['sourceRevisionId']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
