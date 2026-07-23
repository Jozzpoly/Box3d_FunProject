#!/usr/bin/env python3
"""Resume the owner-local path from a verified P1B bundle to native preview.

Private paths stay below ignored build/. The flow discovers existing artifacts,
resolves nested sources, builds and verifies the exact preview, writes an active
selection and can launch the native host. It never grants TERRAIN_VISIBLE_PASS.
"""
from __future__ import annotations

import argparse
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Sequence
import uuid

MODULE_DIR = Path(__file__).resolve().parent
DEFAULT_ROOT = Path("build/scan_pipeline")
STATE_SCHEMA = "jozz.scan-real-terrain-flow-state"
ACTIVE_SCHEMA = "jozz.scan-active-preview"
RECEIPT_SCHEMA = "jozz.scan-p1b-owner-gate-receipt"
SHA256 = re.compile(r"^[0-9a-f]{64}$")
REVISION = re.compile(r"^sha256:[0-9a-f]{64}$")


def _load(name: str) -> Any:
    path = MODULE_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(f"_jozz_real_flow_{name}", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


scan_import_bundle = _load("scan_import_bundle")
scan_preview_pack = _load("scan_preview_pack")
scan_source_assets = _load("scan_source_assets")


class RealTerrainFlowError(ValueError):
    pass


def _json(path: Path) -> dict[str, Any]:
    return scan_import_bundle.load_json_strict(path)


def _write(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp-{os.getpid()}-{uuid.uuid4().hex}")
    try:
        with temporary.open("xb") as handle:
            handle.write(scan_import_bundle.canonical_json_bytes(value))
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def _paths(root: Path) -> dict[str, Path]:
    return {
        "resolved": root / "resolved-sources",
        "previews": root / "previews",
        "config": root / "real-terrain-flow" / "CONFIG.local.json",
        "state": root / "real-terrain-flow" / "STATE.local.json",
        "active": root / "ACTIVE_PREVIEW.json",
    }


def _receipt_binding(path: Path) -> dict[str, str]:
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise RealTerrainFlowError("bundle-bound P1B owner-gate receipt is missing")
    document = _json(path)
    binding = document.get("bundle")
    if (
        document.get("schema") != RECEIPT_SCHEMA
        or document.get("schemaVersion") != 2
        or document.get("status") != "P1B_BUNDLE_PASS"
        or document.get("privacyClass") != "PRIVATE_LOCAL_ONLY"
        or not isinstance(binding, dict)
        or binding.get("internalVerificationPassed") is not True
        or binding.get("independentVerificationPassed") is not True
    ):
        raise RealTerrainFlowError("owner-gate receipt is not a completed P1B PASS receipt")
    bundle_hash = binding.get("bundleContentSha256")
    revision = binding.get("sourceRevisionId")
    if not isinstance(bundle_hash, str) or not SHA256.fullmatch(bundle_hash):
        raise RealTerrainFlowError("owner-gate receipt bundle hash is invalid")
    if not isinstance(revision, str) or not REVISION.fullmatch(revision):
        raise RealTerrainFlowError("owner-gate receipt source revision is invalid")
    return {
        "bundleContentSha256": bundle_hash,
        "sourceRevisionId": revision,
    }


def _receipt_candidates(root: Path, explicit: Path | None) -> list[tuple[Path, dict[str, str]]]:
    paths = [Path(explicit)] if explicit is not None else sorted(
        {
            path
            for pattern in ("p1b_owner_gate_receipt.local.json", "*receipt*.json")
            for path in root.rglob(pattern)
        },
        key=lambda path: path.as_posix().lower(),
    )
    valid: list[tuple[Path, dict[str, str]]] = []
    for path in paths:
        try:
            valid.append((path, _receipt_binding(path)))
        except (OSError, RealTerrainFlowError, scan_import_bundle.ImportBundleError):
            if explicit is not None:
                raise
    if not valid:
        raise RealTerrainFlowError("no completed bundle-bound P1B receipt was found")
    return valid


def _bundle_candidates(root: Path, explicit: Path | None) -> list[tuple[Path, dict[str, Any]]]:
    paths = [Path(explicit)] if explicit is not None else [
        complete.parent for complete in sorted(root.rglob("COMPLETE.json"))
    ]
    valid: list[tuple[Path, dict[str, Any]]] = []
    for path in paths:
        try:
            valid.append((path, scan_import_bundle.verify_bundle(path)))
        except Exception:
            if explicit is not None:
                raise RealTerrainFlowError("explicit P1B bundle failed verification")
    if not valid:
        raise RealTerrainFlowError("no verified P1B bundle was found")
    return valid


def _discover_bundle_and_receipt(
    root: Path,
    explicit_bundle: Path | None,
    explicit_receipt: Path | None,
) -> tuple[Path, Path]:
    root = Path(root)
    if not root.is_dir() or root.is_symlink():
        raise RealTerrainFlowError("scan pipeline root is not available")
    receipts = _receipt_candidates(root, explicit_receipt)
    bundles = _bundle_candidates(root, explicit_bundle)
    matches: list[tuple[Path, Path]] = []
    for receipt_path, binding in receipts:
        for bundle_path, summary in bundles:
            if (
                summary.get("bundleContentSha256") == binding["bundleContentSha256"]
                and summary.get("sourceRevisionId") == binding["sourceRevisionId"]
            ):
                matches.append((bundle_path, receipt_path))
    unique = sorted(
        set(matches),
        key=lambda pair: (pair[0].as_posix().lower(), pair[1].as_posix().lower()),
    )
    if not unique:
        raise RealTerrainFlowError("no verified P1B bundle matches a completed owner-gate receipt")
    if len(unique) != 1:
        raise RealTerrainFlowError("multiple exact bundle/receipt pairs require explicit selection")
    return unique[0]


def _source_roots(config: Path, supplied: Sequence[Path]) -> list[Path]:
    if supplied:
        roots = [Path(path).resolve() for path in supplied]
        _write(
            config,
            {
                "schema": "jozz.scan-real-terrain-flow-config",
                "schemaVersion": 1,
                "privacyClass": "PRIVATE_LOCAL_ONLY",
                "sourceRoots": [str(path) for path in roots],
            },
        )
        return roots
    if not config.is_file() or config.is_symlink():
        raise RealTerrainFlowError(
            "private source root is not configured; run continue once with --source-root"
        )
    roots = _json(config).get("sourceRoots")
    if not isinstance(roots, list) or not roots or not all(isinstance(item, str) for item in roots):
        raise RealTerrainFlowError("private source-root configuration is invalid")
    return [Path(item) for item in roots]


def _active(path: Path) -> tuple[Path, dict[str, Any]]:
    if not path.is_file() or path.is_symlink():
        raise RealTerrainFlowError("ACTIVE_PREVIEW.json is missing")
    document = _json(path)
    if (
        document.get("schema") != ACTIVE_SCHEMA
        or document.get("schemaVersion") != 1
        or document.get("privacyClass") != "PRIVATE_LOCAL_ONLY"
    ):
        raise RealTerrainFlowError("ACTIVE_PREVIEW.json identity is invalid")
    preview = Path(str(document.get("previewPath", "")))
    summary = scan_preview_pack.verify_preview_pack(preview)
    for key in ("previewContentSha256", "sourceRevisionId", "tileCount"):
        if document.get(key) != summary.get(key):
            raise RealTerrainFlowError(f"ACTIVE_PREVIEW.json is stale: {key}")
    return preview, summary


def _samples(repo: Path) -> Path:
    for relative in (
        "build/bin/Debug/samples.exe",
        "build/bin/Release/samples.exe",
        "build/bin/samples.exe",
    ):
        path = repo / relative
        if path.is_file() and not path.is_symlink():
            return path
    raise RealTerrainFlowError("native samples.exe is not built")


def launch(active_path: Path) -> None:
    preview, summary = _active(active_path)
    repo = MODULE_DIR.parents[1]
    environment = dict(os.environ)
    environment["JOZZ_SCAN_PREVIEW_PACK"] = str(preview)
    subprocess.Popen([str(_samples(repo))], cwd=repo, env=environment)
    print(
        "scan_real_terrain_flow: NATIVE_HOST_LAUNCHED | "
        f"tiles={summary['tileCount']} revision={summary['sourceRevisionId']}"
    )
    print(
        "scan_real_terrain_flow: VISUAL_REVIEW_REQUIRED | "
        "open Jozz Vehicle -> P2A Source Visual Preview and inspect orientation, scale, coverage and seams"
    )


def continue_flow(args: argparse.Namespace) -> int:
    paths = _paths(args.pipeline_root)
    bundle, receipt = _discover_bundle_and_receipt(
        args.pipeline_root,
        Path(args.bundle) if args.bundle else None,
        Path(args.owner_gate_receipt) if args.owner_gate_receipt else None,
    )
    roots = _source_roots(paths["config"], args.source_root or [])
    source_view = scan_source_assets.resolve_from_bundle(
        bundle=bundle,
        source_roots=roots,
        output_root=paths["resolved"],
    )
    preview = scan_preview_pack.build_preview_pack(
        bundle=bundle,
        owner_gate_receipt=receipt,
        source_root=source_view,
        output_root=paths["previews"],
        label="source-preview",
        level_x_degrees=args.level_x_deg,
        level_z_degrees=args.level_z_deg,
    )
    summary = scan_preview_pack.verify_preview_pack(preview)
    if summary["tileCount"] != 7:
        raise RealTerrainFlowError("real preview must contain exactly seven tiles")
    _write(
        paths["active"],
        {
            "schema": ACTIVE_SCHEMA,
            "schemaVersion": 1,
            "privacyClass": "PRIVATE_LOCAL_ONLY",
            "previewPath": str(preview.resolve()),
            "previewContentSha256": summary["previewContentSha256"],
            "sourceRevisionId": summary["sourceRevisionId"],
            "tileCount": summary["tileCount"],
        },
    )
    _write(
        paths["state"],
        {
            "schema": STATE_SCHEMA,
            "schemaVersion": 1,
            "privacyClass": "PRIVATE_LOCAL_ONLY",
            "stage": "VISUAL_REVIEW_PENDING",
            "bundlePath": str(bundle.resolve()),
            "ownerGateReceiptPath": str(receipt.resolve()),
            "sourceViewPath": str(source_view.resolve()),
            "previewPath": str(preview.resolve()),
            "previewContentSha256": summary["previewContentSha256"],
            "sourceRevisionId": summary["sourceRevisionId"],
            "tileCount": summary["tileCount"],
            "terrainVisiblePass": False,
        },
    )
    print(
        "scan_real_terrain_flow: REAL_PREVIEW_PACK_READY | "
        f"tiles={summary['tileCount']} preview_sha256={summary['previewContentSha256']}"
    )
    print(
        "scan_real_terrain_flow: STATUS | VISUAL_REVIEW_PENDING; "
        "TERRAIN_VISIBLE_PASS has not been granted"
    )
    if args.launch:
        launch(paths["active"])
    else:
        print(
            "scan_real_terrain_flow: NEXT | "
            "python tools/scan_pipeline/scan_real_terrain_flow.py launch-preview"
        )
    return 0


def status(args: argparse.Namespace) -> int:
    state = _paths(args.pipeline_root)["state"]
    if not state.is_file():
        print("scan_real_terrain_flow: STATUS | NOT_PREPARED")
        return 0
    document = _json(state)
    print(
        "scan_real_terrain_flow: STATUS | "
        f"stage={document.get('stage', 'UNKNOWN')} tiles={document.get('tileCount', 0)} "
        f"terrain_visible_pass={str(document.get('terrainVisiblePass') is True).lower()}"
    )
    return 0


def verify(args: argparse.Namespace) -> int:
    paths = _paths(args.pipeline_root)
    preview, summary = _active(paths["active"])
    state = _json(paths["state"])
    source = scan_source_assets.verify_source_view(Path(str(state.get("sourceViewPath", ""))))
    if source["sourceRevisionId"] != summary["sourceRevisionId"]:
        raise RealTerrainFlowError("active preview and source view revisions differ")
    print(
        "scan_real_terrain_flow: VERIFIED | "
        f"tiles={summary['tileCount']} preview={preview.name} "
        f"resolution_sha256={source['resolutionContentSha256']}"
    )
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pipeline-root", type=Path, default=DEFAULT_ROOT)
    sub = parser.add_subparsers(dest="command", required=True)
    continuation = sub.add_parser("continue")
    continuation.add_argument("--bundle", type=Path)
    continuation.add_argument("--owner-gate-receipt", type=Path)
    continuation.add_argument("--source-root", type=Path, action="append")
    # Per-scan leveling tilt (degrees) applied after the axis matrix; 0 = none.
    continuation.add_argument("--level-x-deg", type=float, default=0.0)
    continuation.add_argument("--level-z-deg", type=float, default=0.0)
    continuation.add_argument("--launch", action="store_true")
    continuation.set_defaults(handler=continue_flow)
    show = sub.add_parser("status")
    show.set_defaults(handler=status)
    check = sub.add_parser("verify")
    check.set_defaults(handler=verify)
    run = sub.add_parser("launch-preview")
    run.set_defaults(handler=lambda args: (launch(_paths(args.pipeline_root)["active"]), 0)[1])
    args = parser.parse_args(argv)
    try:
        return int(args.handler(args))
    except (
        OSError,
        RealTerrainFlowError,
        scan_import_bundle.ImportBundleError,
        scan_preview_pack.PreviewPackError,
        scan_source_assets.SourceAssetResolutionError,
    ) as exc:
        print(f"scan_real_terrain_flow: ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
