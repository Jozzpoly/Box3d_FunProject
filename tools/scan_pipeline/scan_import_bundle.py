#!/usr/bin/env python3
"""Build and verify content-addressed scan-import evidence bundles.

A bundle links one private P1 inspection to an explicit source frame, an
immutable ScanSourcePackage, an UNREVIEWED WorldImportProposal and one
allow-listed shareable summary. It is not an accepted world patch.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import sys
from typing import Any, Mapping, Sequence
import uuid

MODULE_DIR = Path(__file__).resolve().parent


def _load_sibling(name: str) -> Any:
    path = MODULE_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(f"_jozz_bundle_{name}", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


scan_frames = _load_sibling("scan_frames")
scan_world_contracts = _load_sibling("scan_world_contracts")

SHAREABLE_SCHEMA = "jozz.scan-inspection-shareable"
SHAREABLE_SCHEMA_VERSION = 1
BUNDLE_SCHEMA = "jozz.scan-import-bundle"
BUNDLE_SCHEMA_VERSION = 1
_SAFE_LABEL = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")
_SHA256 = re.compile(r"^[0-9a-f]{64}$")
_DOCUMENT_PATHS = (
    "private/inspection.private.json",
    "private/source_frame.json",
    "private/source_package.json",
    "private/world_import_proposal.json",
    "shareable/inspection.shareable.json",
)


class ImportBundleError(ValueError):
    pass


def _validate_json(value: Any, path: str = "root") -> None:
    if value is None or isinstance(value, (str, bool, int)):
        return
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ImportBundleError(f"{path} contains a non-finite float")
        return
    if isinstance(value, list):
        for index, child in enumerate(value):
            _validate_json(child, f"{path}[{index}]")
        return
    if isinstance(value, dict):
        for key, child in value.items():
            if not isinstance(key, str):
                raise ImportBundleError(f"{path} contains a non-string key")
            _validate_json(child, f"{path}.{key}")
        return
    raise ImportBundleError(f"{path} contains non-JSON value {type(value).__name__}")


def canonical_json_bytes(value: Any) -> bytes:
    _validate_json(value)
    return (
        json.dumps(
            value,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        )
        + "\n"
    ).encode("utf-8")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _strict_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ImportBundleError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json_strict(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(
            Path(path).read_text(encoding="utf-8"),
            object_pairs_hook=_strict_object,
            parse_constant=lambda token: (_ for _ in ()).throw(
                ImportBundleError(f"non-standard JSON constant: {token}")
            ),
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ImportBundleError(f"cannot read JSON from {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ImportBundleError(f"{path} must contain a JSON object")
    _validate_json(value)
    return value


def _label(value: Any, field: str) -> str:
    if not isinstance(value, str) or not _SAFE_LABEL.fullmatch(value):
        raise ImportBundleError(f"{field} must match {_SAFE_LABEL.pattern}")
    return value


def _sha(value: Any, field: str) -> str:
    if not isinstance(value, str) or not _SHA256.fullmatch(value):
        raise ImportBundleError(f"{field} must be lowercase SHA-256")
    return value


def _uint(value: Any, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ImportBundleError(f"{field} must be a non-negative integer")
    return value


def _number(
    value: Any,
    field: str,
    *,
    minimum: float = 0.0,
    maximum: float | None = None,
) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError) as exc:
        raise ImportBundleError(f"{field} must be numeric") from exc
    if not math.isfinite(result) or result < minimum:
        raise ImportBundleError(f"{field} must be finite and >= {minimum}")
    if maximum is not None and result > maximum:
        raise ImportBundleError(f"{field} must be <= {maximum}")
    rounded = round(result, 12)
    return 0.0 if rounded == -0.0 else rounded


def _pair_classification(value: Any, tile_id: int) -> str:
    if value == "strong-match":
        return "bounds-strong-match"
    if value in {"bounds-strong-match", "review", "incompatible"}:
        return str(value)
    raise ImportBundleError(f"unknown pair classification for tile {tile_id}")


def build_shareable_inspection(report: dict[str, Any]) -> dict[str, Any]:
    """Project private evidence through an explicit field allow-list."""

    _validate_json(report, "inspection")
    if report.get("schema") != scan_world_contracts.INSPECTION_SCHEMA:
        raise ImportBundleError("unexpected inspection schema")
    source_version = int(report.get("schemaVersion", 0))
    if source_version < 3:
        raise ImportBundleError("inspection schema is too old")
    dataset_status = report.get("datasetStatus")
    if dataset_status not in {"compatible", "compatible-review", "incompatible"}:
        raise ImportBundleError("invalid datasetStatus")
    # Never copy a source-provided package name into the shareable projection.
    # A sanitized string can still disclose a place/project name.
    automatic = report.get("automaticEvidenceGate")
    if not isinstance(automatic, dict) or not isinstance(automatic.get("passed"), bool):
        raise ImportBundleError("automaticEvidenceGate.passed must be boolean")

    totals_raw = report.get("totals")
    quality_raw = report.get("geometryQuality")
    grid_raw = report.get("evidenceGrid")
    pairs_raw = report.get("pairs")
    if not all(isinstance(value, dict) for value in (totals_raw, quality_raw, grid_raw)):
        raise ImportBundleError("totals, geometryQuality and evidenceGrid must be objects")
    if not isinstance(pairs_raw, list) or not pairs_raw:
        raise ImportBundleError("pairs must be a non-empty array")

    totals = {
        key: _uint(totals_raw.get(key), f"totals.{key}")
        for key in ("glbFiles", "plyFiles", "glbVertices", "glbTriangles", "plyPoints")
    }
    quality = {
        key: _uint(quality_raw.get(key), f"geometryQuality.{key}")
        for key in (
            "triangleCountAnalyzed",
            "degenerateTriangleCount",
            "provisionalLargeTriangleCount",
        )
    }
    quality.update(
        {
            key: _number(quality_raw.get(key), f"geometryQuality.{key}")
            for key in (
                "provisionalLargeEdgeThresholdSourceUnits",
                "maxTriangleEdgeSourceUnits",
                "maxTriangleAreaSourceUnitsSquared",
            )
        }
    )
    backend = grid_raw.get("backend")
    if backend not in {"stdlib", "numpy"}:
        raise ImportBundleError("evidenceGrid.backend must be stdlib or numpy")
    grid = {
        key: _uint(grid_raw.get(key), f"evidenceGrid.{key}")
        for key in (
            "width",
            "height",
            "pointsAccumulated",
            "verifiedSourceCount",
            "occupiedCells",
            "maxPointsPerCell",
            "maxSourceSupport",
        )
    }
    grid.update(
        {
            "backend": backend,
            "occupancyRatio": _number(
                grid_raw.get("occupancyRatio"),
                "evidenceGrid.occupancyRatio",
                maximum=1.0,
            ),
            "verticalSpreadP95SourceUnits": _number(
                grid_raw.get("verticalSpreadP95SourceUnits"),
                "evidenceGrid.verticalSpreadP95SourceUnits",
            ),
        }
    )
    if grid["width"] <= 0 or grid["height"] <= 0:
        raise ImportBundleError("evidence grid dimensions must be positive")
    if grid["occupiedCells"] > grid["width"] * grid["height"]:
        raise ImportBundleError("occupiedCells exceeds grid capacity")
    if grid["pointsAccumulated"] != totals["plyPoints"]:
        raise ImportBundleError("PLY totals disagree with evidence grid")

    pairs: list[dict[str, Any]] = []
    seen: set[int] = set()
    for raw in pairs_raw:
        if not isinstance(raw, dict):
            raise ImportBundleError("pair entry must be an object")
        tile_id = _uint(raw.get("tileId"), "pair.tileId")
        if tile_id in seen:
            raise ImportBundleError(f"duplicate pair tile id {tile_id}")
        seen.add(tile_id)
        pairs.append(
            {
                "tileId": tile_id,
                "classification": _pair_classification(
                    raw.get("classification"), tile_id
                ),
                "evidenceLevel": "BOUNDS_ONLY",
                "normalizedCenterDelta": _number(
                    raw.get("normalizedCenterDelta"),
                    f"pair[{tile_id}].normalizedCenterDelta",
                ),
                "maxExtentRelativeError": _number(
                    raw.get("maxExtentRelativeError"),
                    f"pair[{tile_id}].maxExtentRelativeError",
                ),
                "xyOverlapOfSmaller": _number(
                    raw.get("xyOverlapOfSmaller"),
                    f"pair[{tile_id}].xyOverlapOfSmaller",
                    maximum=1.0,
                ),
                "axisPermutationSuspicion": (
                    raw.get("axisPermutationSuspicion") is True
                ),
            }
        )
    pairs.sort(key=lambda item: item["tileId"])
    if len(pairs) != totals["glbFiles"] or len(pairs) != totals["plyFiles"]:
        raise ImportBundleError("pair count disagrees with source totals")

    return {
        "schema": SHAREABLE_SCHEMA,
        "schemaVersion": SHAREABLE_SCHEMA_VERSION,
        "sourceInspectionSchema": report["schema"],
        "sourceInspectionSchemaVersion": source_version,
        "datasetLabel": "scan-dataset",
        "datasetStatus": dataset_status,
        "automaticEvidenceGatePassed": automatic["passed"],
        "totals": totals,
        "geometryQuality": quality,
        "evidenceGrid": grid,
        "pairEvidence": pairs,
        "capabilities": {
            "sourceInspectionPassed": automatic["passed"],
            "diagnosticPreviewCandidate": (
                automatic["passed"] and dataset_status != "incompatible"
            ),
            "pairingEvidenceLevel": "BOUNDS_ONLY",
            "internalGeometryCorrespondencePassed": False,
        },
        "omittedFreeformWarningCount": (
            len(report["warnings"]) if isinstance(report.get("warnings"), list) else 0
        ),
        "privacy": {
            "sourceCoordinatesIncluded": False,
            "sourceBoundsIncluded": False,
            "sourceHashesIncluded": False,
            "absolutePathsIncluded": False,
            "freeformWarningsIncluded": False,
            "sourceRgbIncluded": False,
            "sourceTexturesIncluded": False,
            "georeferencingStatus": "NOT_ASSERTED",
        },
    }


def build_bundle_documents(
    *,
    package_id: str,
    proposal_id: str,
    inspection_report: dict[str, Any],
    frame_contract: dict[str, Any],
    require_inspection_pass: bool = False,
    require_frame_confirmed: bool = False,
) -> dict[str, dict[str, Any]]:
    _validate_json(inspection_report, "inspection")
    frame = scan_frames.validate_frame_contract(frame_contract)
    package = scan_world_contracts.build_source_package(
        package_id=package_id,
        inspection_report=inspection_report,
        frame_contract=frame,
    )
    proposal = scan_world_contracts.build_world_import_proposal(
        proposal_id=proposal_id,
        source_package=package,
        inspection_report=inspection_report,
    )
    scan_world_contracts.validate_source_package(package)
    scan_world_contracts.validate_world_import_proposal(proposal)
    if require_inspection_pass and not proposal["capabilities"]["sourceInspectionPassed"]:
        raise ImportBundleError("automatic source-inspection gate did not pass")
    if require_frame_confirmed and not frame["confirmed"]:
        raise ImportBundleError("source frame is not confirmed")
    return {
        "private/inspection.private.json": inspection_report,
        "private/source_frame.json": frame,
        "private/source_package.json": package,
        "private/world_import_proposal.json": proposal,
        "shareable/inspection.shareable.json": build_shareable_inspection(
            inspection_report
        ),
    }


def _relative_path(value: Any, field: str) -> str:
    if not isinstance(value, str):
        raise ImportBundleError(f"{field} must be a relative path")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or str(path) != value:
        raise ImportBundleError(f"{field} is unsafe or non-canonical")
    return value


def _validate_documents(
    documents: Mapping[str, dict[str, Any]],
) -> tuple[dict[str, Any], dict[str, Any]]:
    if set(documents) != set(_DOCUMENT_PATHS):
        raise ImportBundleError("bundle document set is incomplete or unexpected")
    inspection = documents["private/inspection.private.json"]
    frame = scan_frames.validate_frame_contract(
        documents["private/source_frame.json"]
    )
    package = scan_world_contracts.validate_source_package(
        documents["private/source_package.json"]
    )
    proposal = scan_world_contracts.validate_world_import_proposal(
        documents["private/world_import_proposal.json"]
    )
    shareable = documents["shareable/inspection.shareable.json"]
    _validate_json(inspection, "inspection")
    _validate_json(shareable, "shareable")
    if package["sourceFrameContract"] != frame:
        raise ImportBundleError("frame document differs from source package")
    if proposal["sourcePackageId"] != package["packageId"]:
        raise ImportBundleError("proposal package mismatch")
    if proposal["sourceRevisionId"] != package["revisionId"]:
        raise ImportBundleError("proposal source revision mismatch")
    if (
        proposal["sourceFrameContractSha256"]
        != package["sourceFrameContractSha256"]
    ):
        raise ImportBundleError("proposal source frame hash mismatch")
    if proposal["inspectionSha256"] != scan_world_contracts.sha256_json(inspection):
        raise ImportBundleError("proposal inspection hash mismatch")
    if shareable != build_shareable_inspection(inspection):
        raise ImportBundleError("shareable report is not the canonical projection")
    return package, proposal


def build_manifest(
    documents: Mapping[str, dict[str, Any]],
) -> tuple[dict[str, Any], dict[str, bytes]]:
    package, proposal = _validate_documents(documents)
    encoded = {
        path: canonical_json_bytes(documents[path]) for path in sorted(documents)
    }
    core = {
        "schema": BUNDLE_SCHEMA,
        "schemaVersion": BUNDLE_SCHEMA_VERSION,
        "status": "COMPLETE",
        "packageId": package["packageId"],
        "sourceRevisionId": package["revisionId"],
        "proposalId": proposal["proposalId"],
        "proposalContentSha256": proposal["proposalContentSha256"],
        "files": [
            {
                "path": path,
                "byteLength": len(encoded[path]),
                "sha256": sha256_bytes(encoded[path]),
                "privacyClass": (
                    "PRIVATE_LOCAL_ONLY"
                    if path.startswith("private/")
                    else "SHAREABLE"
                ),
            }
            for path in sorted(encoded)
        ],
    }
    manifest = dict(core)
    manifest["bundleContentSha256"] = sha256_bytes(canonical_json_bytes(core))
    return manifest, encoded


def _validate_manifest(manifest: dict[str, Any]) -> dict[str, Any]:
    if (
        manifest.get("schema") != BUNDLE_SCHEMA
        or int(manifest.get("schemaVersion", 0)) != BUNDLE_SCHEMA_VERSION
        or manifest.get("status") != "COMPLETE"
    ):
        raise ImportBundleError("invalid bundle schema, version or status")
    expected_hash = _sha(
        manifest.get("bundleContentSha256"), "bundleContentSha256"
    )
    unsigned = dict(manifest)
    unsigned.pop("bundleContentSha256", None)
    if expected_hash != sha256_bytes(canonical_json_bytes(unsigned)):
        raise ImportBundleError("bundleContentSha256 mismatch")
    files = manifest.get("files")
    if not isinstance(files, list):
        raise ImportBundleError("manifest.files must be an array")
    paths: list[str] = []
    for record in files:
        if not isinstance(record, dict):
            raise ImportBundleError("manifest file record must be an object")
        path = _relative_path(record.get("path"), "manifest file path")
        paths.append(path)
        _uint(record.get("byteLength"), f"{path}.byteLength")
        _sha(record.get("sha256"), f"{path}.sha256")
        privacy = (
            "PRIVATE_LOCAL_ONLY" if path.startswith("private/") else "SHAREABLE"
        )
        if record.get("privacyClass") != privacy:
            raise ImportBundleError(f"privacyClass mismatch for {path}")
    if paths != sorted(paths) or len(paths) != len(set(paths)):
        raise ImportBundleError("manifest paths must be sorted and unique")
    if set(paths) != set(_DOCUMENT_PATHS):
        raise ImportBundleError("manifest document set is incomplete or unexpected")
    return manifest


def _bundle_files(root: Path) -> set[str]:
    files: set[str] = set()
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ImportBundleError(f"bundle contains symlink: {path}")
        if path.is_file():
            files.add(path.relative_to(root).as_posix())
    return files


def verify_bundle(root: Path) -> dict[str, Any]:
    root = Path(root)
    if not root.is_dir() or root.is_symlink():
        raise ImportBundleError("bundle root must be a real directory")
    complete = root / "COMPLETE.json"
    if not complete.is_file() or complete.is_symlink():
        raise ImportBundleError("bundle is incomplete: COMPLETE.json missing")
    manifest = _validate_manifest(load_json_strict(complete))
    expected = {record["path"] for record in manifest["files"]}
    actual = _bundle_files(root)
    if actual != expected | {"COMPLETE.json"}:
        raise ImportBundleError("bundle contains missing or unexpected files")

    loaded: dict[str, dict[str, Any]] = {}
    for record in manifest["files"]:
        relative = record["path"]
        path = root / PurePosixPath(relative)
        if path.stat().st_size != record["byteLength"]:
            raise ImportBundleError(f"byteLength mismatch for {relative}")
        if sha256_file(path) != record["sha256"]:
            raise ImportBundleError(f"SHA-256 mismatch for {relative}")
        loaded[relative] = load_json_strict(path)

    inspection = loaded["private/inspection.private.json"]
    frame = scan_frames.validate_frame_contract(
        loaded["private/source_frame.json"]
    )
    package = scan_world_contracts.validate_source_package(
        loaded["private/source_package.json"]
    )
    proposal = scan_world_contracts.validate_world_import_proposal(
        loaded["private/world_import_proposal.json"]
    )
    shareable = loaded["shareable/inspection.shareable.json"]

    if package["sourceFrameContract"] != frame:
        raise ImportBundleError("frame document differs from source package")
    if proposal["sourcePackageId"] != package["packageId"]:
        raise ImportBundleError("proposal package mismatch")
    if proposal["sourceRevisionId"] != package["revisionId"]:
        raise ImportBundleError("proposal source revision mismatch")
    if (
        proposal["sourceFrameContractSha256"]
        != package["sourceFrameContractSha256"]
    ):
        raise ImportBundleError("proposal source frame hash mismatch")
    if proposal["inspectionSha256"] != scan_world_contracts.sha256_json(inspection):
        raise ImportBundleError("proposal inspection hash mismatch")
    if shareable != build_shareable_inspection(inspection):
        raise ImportBundleError("shareable report is not the canonical projection")
    for field, expected_value in (
        ("packageId", package["packageId"]),
        ("sourceRevisionId", package["revisionId"]),
        ("proposalId", proposal["proposalId"]),
        ("proposalContentSha256", proposal["proposalContentSha256"]),
    ):
        if manifest.get(field) != expected_value:
            raise ImportBundleError(f"manifest {field} mismatch")
    return {
        "bundleContentSha256": manifest["bundleContentSha256"],
        "packageId": package["packageId"],
        "sourceRevisionId": package["revisionId"],
        "proposalId": proposal["proposalId"],
        "path": str(root),
    }


def _write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())


def write_bundle_transactionally(
    *,
    documents: Mapping[str, dict[str, Any]],
    output_root: Path,
    bundle_label: str,
) -> Path:
    label = _label(bundle_label, "bundleLabel")
    manifest, encoded = build_manifest(documents)
    root = Path(output_root)
    root.mkdir(parents=True, exist_ok=True)
    if root.is_symlink():
        raise ImportBundleError("output root must not be a symlink")

    bundle_hash = manifest["bundleContentSha256"]
    final = root / f"{label}-{bundle_hash[:16]}"
    if final.exists():
        if verify_bundle(final)["bundleContentSha256"] != bundle_hash:
            raise ImportBundleError("existing content-addressed bundle is inconsistent")
        return final

    staging = root / f".{label}.staging-{os.getpid()}-{uuid.uuid4().hex}"
    staging.mkdir()
    try:
        for relative in sorted(encoded):
            _write(staging / PurePosixPath(relative), encoded[relative])
        _write(staging / "COMPLETE.json", canonical_json_bytes(manifest))
        try:
            os.replace(staging, final)
        except OSError:
            if not final.exists():
                raise
            if verify_bundle(final)["bundleContentSha256"] != bundle_hash:
                raise ImportBundleError(
                    "concurrent publication produced inconsistent content"
                )
            shutil.rmtree(staging, ignore_errors=True)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    verify_bundle(final)
    return final


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inspection", required=True, type=Path)
    parser.add_argument("--frame-contract", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--package-id", required=True)
    parser.add_argument("--proposal-id", required=True)
    parser.add_argument("--bundle-label", required=True)
    parser.add_argument("--require-inspection-pass", action="store_true")
    parser.add_argument("--require-frame-confirmed", action="store_true")
    args = parser.parse_args(argv)
    try:
        documents = build_bundle_documents(
            package_id=args.package_id,
            proposal_id=args.proposal_id,
            inspection_report=load_json_strict(args.inspection),
            frame_contract=load_json_strict(args.frame_contract),
            require_inspection_pass=args.require_inspection_pass,
            require_frame_confirmed=args.require_frame_confirmed,
        )
        output = write_bundle_transactionally(
            documents=documents,
            output_root=args.output_root,
            bundle_label=args.bundle_label,
        )
        summary = verify_bundle(output)
    except (
        OSError,
        ImportBundleError,
        scan_frames.FrameContractError,
        scan_world_contracts.WorldContractError,
    ) as exc:
        print(f"scan_import_bundle: ERROR: {exc}", file=sys.stderr)
        return 2
    print(
        "scan_import_bundle: OK | "
        f"path={output} bundle_sha256={summary['bundleContentSha256']} "
        f"revision={summary['sourceRevisionId']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
