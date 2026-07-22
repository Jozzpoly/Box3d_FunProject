#!/usr/bin/env python3
"""Resolve nested private GLB/PLY sources into a verified canonical local view."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
from typing import Any, Sequence
import uuid

MODULE_DIR = Path(__file__).resolve().parent
SCHEMA = "jozz.scan-source-resolution-receipt"
SCHEMA_VERSION = 1
RECEIPT = "RESOLUTION.private.json"
KINDS = ("glb", "ply")
NAME = re.compile(r"(?i)^MipTile_(\d+)\.(glb|ply)$")
SHA = re.compile(r"^[0-9a-f]{64}$")
CHUNK = 4 * 1024 * 1024


def _load(name: str) -> Any:
    path = MODULE_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(f"_jozz_sources_{name}", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


scan_import_bundle = _load("scan_import_bundle")
scan_world_contracts = _load("scan_world_contracts")


class SourceAssetResolutionError(ValueError):
    pass


def _json(value: Any) -> bytes:
    return scan_import_bundle.canonical_json_bytes(value)


def _sha_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _strict(path: Path) -> dict[str, Any]:
    return scan_import_bundle.load_json_strict(path)


def _real_root(path: Path) -> Path:
    path = Path(path)
    if not path.is_dir() or path.is_symlink():
        raise SourceAssetResolutionError("private source root must be a real directory")
    try:
        return path.resolve(strict=True)
    except OSError as exc:
        raise SourceAssetResolutionError("private source root cannot be resolved") from exc


def _files(root: Path) -> list[Path]:
    result: list[Path] = []
    for current, directories, filenames in os.walk(root, followlinks=False):
        base = Path(current)
        for name in directories:
            if (base / name).is_symlink():
                raise SourceAssetResolutionError("private source root contains a symlink")
        for name in filenames:
            path = base / name
            if path.is_symlink():
                raise SourceAssetResolutionError("private source root contains a symlink")
            if path.is_file():
                result.append(path)
    return result


def _identity(stat: os.stat_result) -> tuple[int, int, int, int]:
    return (
        int(stat.st_size),
        int(stat.st_mtime_ns),
        int(getattr(stat, "st_dev", 0)),
        int(getattr(stat, "st_ino", 0)),
    )


def _hash(path: Path) -> tuple[str, tuple[int, int, int, int]]:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        before = _identity(os.fstat(handle.fileno()))
        for block in iter(lambda: handle.read(CHUNK), b""):
            digest.update(block)
        after = _identity(os.fstat(handle.fileno()))
    if before != after:
        raise SourceAssetResolutionError("private source changed while hashing")
    return digest.hexdigest(), after


def _copy(path: Path, output: Path, size: int, expected_sha: str) -> None:
    digest = hashlib.sha256()
    count = 0
    with path.open("rb") as source, output.open("xb") as target:
        before = _identity(os.fstat(source.fileno()))
        while True:
            block = source.read(CHUNK)
            if not block:
                break
            digest.update(block)
            target.write(block)
            count += len(block)
        target.flush()
        os.fsync(target.fileno())
        after = _identity(os.fstat(source.fileno()))
    if before != after:
        raise SourceAssetResolutionError("private source changed during materialization")
    if count != size or digest.hexdigest() != expected_sha:
        raise SourceAssetResolutionError("private source identity changed before materialization")


def _expected(package: dict[str, Any], kinds: Sequence[str]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    package = scan_world_contracts.validate_source_package(package)
    kinds = sorted(set(kinds))
    if not kinds or any(kind not in KINDS for kind in kinds):
        raise SourceAssetResolutionError("asset kinds must be glb and/or ply")
    records = []
    for tile in package["tiles"]:
        for kind in kinds:
            source = tile[kind]
            records.append(
                {
                    "kind": kind,
                    "tileId": int(tile["tileId"]),
                    "sourceLabel": source["sourceLabel"],
                    "byteLength": int(source["byteLength"]),
                    "sha256": source["sha256"],
                }
            )
    return package, records


def _resolve(expected: list[dict[str, Any]], roots: list[Path]) -> list[dict[str, Any]]:
    index: dict[tuple[str, int, int], list[tuple[int, Path, str]]] = {}
    for root_id, root in enumerate(roots):
        for path in _files(root):
            match = NAME.fullmatch(path.name)
            if not match:
                continue
            key = (match.group(2).lower(), int(match.group(1)), int(path.stat().st_size))
            index.setdefault(key, []).append((root_id, path, path.relative_to(root).as_posix()))
    resolved = []
    for record in expected:
        key = (record["kind"], record["tileId"], record["byteLength"])
        matches = []
        for root_id, path, relative in index.get(key, []):
            digest, identity = _hash(path)
            if digest == record["sha256"]:
                matches.append((root_id, path, relative, identity))
        label = f"{record['kind'].upper()} tile {record['tileId']}"
        if not matches:
            raise SourceAssetResolutionError(f"{label} has no exact private source match")
        if len(matches) != 1:
            raise SourceAssetResolutionError(f"{label} has multiple exact private source matches")
        root_id, path, relative, identity = matches[0]
        resolved.append(
            {
                **record,
                "path": path,
                "privateRootId": f"root-{root_id}",
                "privateRelativePath": relative,
                "sourceIdentity": {
                    "byteLength": identity[0],
                    "mtimeNs": identity[1],
                    "device": identity[2],
                    "inode": identity[3],
                },
            }
        )
    return sorted(resolved, key=lambda item: (item["tileId"], item["kind"]))


def _manifest(bundle: dict[str, Any], package: dict[str, Any], resolved: list[dict[str, Any]]) -> dict[str, Any]:
    assets = [
        {
            key: item[key]
            for key in (
                "kind",
                "tileId",
                "sourceLabel",
                "byteLength",
                "sha256",
                "privateRootId",
                "privateRelativePath",
                "sourceIdentity",
            )
        }
        for item in resolved
    ]
    core = {
        "schema": SCHEMA,
        "schemaVersion": SCHEMA_VERSION,
        "status": "COMPLETE",
        "privacyClass": "PRIVATE_LOCAL_ONLY",
        "sourceBundleContentSha256": bundle["bundleContentSha256"],
        "packageId": package["packageId"],
        "sourceRevisionId": package["revisionId"],
        "policy": {
            "recursive": True,
            "followSymlinks": False,
            "requireUniquePhysicalMatch": True,
            "materialization": "VERIFIED_PRIVATE_COPY",
        },
        "assetCount": len(assets),
        "assets": assets,
    }
    return {**core, "resolutionContentSha256": _sha_bytes(_json(core))}


def verify_source_view(root: Path) -> dict[str, Any]:
    root = Path(root)
    if not root.is_dir() or root.is_symlink():
        raise SourceAssetResolutionError("source view must be a real directory")
    receipt = _strict(root / RECEIPT)
    if receipt.get("schema") != SCHEMA or receipt.get("schemaVersion") != SCHEMA_VERSION:
        raise SourceAssetResolutionError("resolution receipt identity mismatch")
    unsigned = dict(receipt)
    observed = unsigned.pop("resolutionContentSha256", None)
    if not isinstance(observed, str) or not SHA.fullmatch(observed) or _sha_bytes(_json(unsigned)) != observed:
        raise SourceAssetResolutionError("resolution content hash mismatch")
    expected_files = {RECEIPT}
    seen = set()
    for record in receipt.get("assets", []):
        key = (record.get("kind"), record.get("tileId"))
        if key in seen:
            raise SourceAssetResolutionError("resolution contains duplicate assets")
        seen.add(key)
        label = record.get("sourceLabel")
        if label != f"MipTile_{record.get('tileId')}.{record.get('kind')}":
            raise SourceAssetResolutionError("resolution source label is not canonical")
        relative = record.get("privateRelativePath")
        logical = PurePosixPath(relative) if isinstance(relative, str) else PurePosixPath("..")
        if logical.is_absolute() or ".." in logical.parts or str(logical) != relative:
            raise SourceAssetResolutionError("resolution private relative path is unsafe")
        expected_files.add(label)
        path = root / label
        if not path.is_file() or path.is_symlink():
            raise SourceAssetResolutionError("materialized private source is missing or linked")
        digest, identity = _hash(path)
        if identity[0] != record.get("byteLength") or digest != record.get("sha256"):
            raise SourceAssetResolutionError("materialized private source identity mismatch")
    actual = {path.relative_to(root).as_posix() for path in _files(root)}
    if actual != expected_files:
        raise SourceAssetResolutionError("source view contains missing or unexpected files")
    return {
        "path": str(root),
        "assetCount": len(seen),
        "sourceRevisionId": receipt["sourceRevisionId"],
        "resolutionContentSha256": observed,
    }


def resolve_source_package(
    *,
    source_package: dict[str, Any],
    bundle_summary: dict[str, Any],
    source_roots: Sequence[Path],
    output_root: Path,
    kinds: Sequence[str] = KINDS,
) -> Path:
    package, expected = _expected(source_package, kinds)
    if package["revisionId"] != bundle_summary.get("sourceRevisionId"):
        raise SourceAssetResolutionError("bundle revision differs from source package")
    bundle_hash = bundle_summary.get("bundleContentSha256")
    if not isinstance(bundle_hash, str) or not SHA.fullmatch(bundle_hash):
        raise SourceAssetResolutionError("bundle content hash is invalid")
    roots = [_real_root(root) for root in source_roots]
    if not roots:
        raise SourceAssetResolutionError("at least one private source root is required")
    resolved = _resolve(expected, roots)
    manifest = _manifest(bundle_summary, package, resolved)
    output_root = Path(output_root)
    output_root.mkdir(parents=True, exist_ok=True)
    if output_root.is_symlink():
        raise SourceAssetResolutionError("source view output root must not be a symlink")
    final = output_root / f"source-view-{manifest['resolutionContentSha256'][:16]}"
    if final.exists():
        if verify_source_view(final)["resolutionContentSha256"] != manifest["resolutionContentSha256"]:
            raise SourceAssetResolutionError("existing source view is inconsistent")
        return final
    staging = output_root / f".source-view.staging-{os.getpid()}-{uuid.uuid4().hex}"
    staging.mkdir()
    try:
        for record in resolved:
            _copy(record["path"], staging / record["sourceLabel"], record["byteLength"], record["sha256"])
        (staging / RECEIPT).write_bytes(_json(manifest))
        os.replace(staging, final)
        verify_source_view(final)
        return final
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise


def resolve_from_bundle(*, bundle: Path, source_roots: Sequence[Path], output_root: Path) -> Path:
    bundle = Path(bundle)
    summary = scan_import_bundle.verify_bundle(bundle)
    package = scan_world_contracts.validate_source_package(
        _strict(bundle / "private/source_package.json")
    )
    return resolve_source_package(
        source_package=package,
        bundle_summary=summary,
        source_roots=source_roots,
        output_root=output_root,
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("resolve")
    build.add_argument("--bundle", required=True, type=Path)
    build.add_argument("--source-root", required=True, action="append", type=Path)
    build.add_argument("--output-root", type=Path, default=Path("build/scan_pipeline/resolved-sources"))
    check = sub.add_parser("verify")
    check.add_argument("source_view", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "resolve":
            path = resolve_from_bundle(
                bundle=args.bundle,
                source_roots=args.source_root,
                output_root=args.output_root,
            )
            summary = verify_source_view(path)
        else:
            summary = verify_source_view(args.source_view)
            path = Path(summary["path"])
        print(
            "scan_source_assets: SOURCE_VIEW_READY | "
            f"view={path.name} assets={summary['assetCount']} "
            f"resolution_sha256={summary['resolutionContentSha256']}"
        )
        return 0
    except (
        OSError,
        SourceAssetResolutionError,
        scan_import_bundle.ImportBundleError,
        scan_world_contracts.WorldContractError,
    ) as exc:
        print(f"scan_source_assets: ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
