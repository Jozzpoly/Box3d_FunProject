#!/usr/bin/env python3
"""Backend-neutral contracts for scan source packages and world import proposals.

These helpers deliberately stop before authored world acceptance, rendering,
collision cooking or runtime creation. They turn deterministic inspection
results into an immutable source revision and an explicitly unreviewed proposal.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
from typing import Any, Iterable

MODULE_DIR = Path(__file__).resolve().parent


def _load_sibling(name: str) -> Any:
    path = MODULE_DIR / f"{name}.py"
    module_name = f"_jozz_world_contract_{name}"
    spec = importlib.util.spec_from_file_location(module_name, path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


scan_frames = _load_sibling("scan_frames")

SOURCE_PACKAGE_SCHEMA = "jozz.scan-source-package"
SOURCE_PACKAGE_SCHEMA_VERSION = 1
WORLD_PROPOSAL_SCHEMA = "jozz.world-import-proposal"
WORLD_PROPOSAL_SCHEMA_VERSION = 1
INSPECTION_SCHEMA = "jozz.scan-dataset-inspection"
_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9._:/-]{0,127}$")
_FORBIDDEN_KEY_TOKENS = (
    "b3body",
    "b3shape",
    "b3joint",
    "nativehandle",
    "gpuhandle",
    "imguiid",
    "sokolhandle",
    "d3dhandle",
)


class WorldContractError(ValueError):
    """Raised when source-package or import-proposal data violates the contract."""


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def sha256_json(value: Any) -> str:
    return hashlib.sha256(canonical_json_bytes(value)).hexdigest()


def _stable_id(value: Any, label: str) -> str:
    if not isinstance(value, str) or not _ID_PATTERN.fullmatch(value):
        raise WorldContractError(
            f"{label} must match {_ID_PATTERN.pattern} and remain independent of filenames/native handles"
        )
    return value


def _positive_int(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise WorldContractError(f"{label} must be a non-negative integer")
    return value


def _sha256(value: Any, label: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
        raise WorldContractError(f"{label} must be a lowercase SHA-256 hex string")
    return value


def _reject_backend_handles(value: Any, path: str = "root") -> None:
    if isinstance(value, dict):
        for key, nested in value.items():
            lowered = str(key).replace("_", "").replace("-", "").lower()
            if any(token in lowered for token in _FORBIDDEN_KEY_TOKENS):
                raise WorldContractError(f"backend/native handle field is forbidden at {path}.{key}")
            _reject_backend_handles(nested, f"{path}.{key}")
    elif isinstance(value, list):
        for index, nested in enumerate(value):
            _reject_backend_handles(nested, f"{path}[{index}]")


def _validate_inspection(report: Any) -> dict[str, Any]:
    if not isinstance(report, dict):
        raise WorldContractError("inspection report must be an object")
    if report.get("schema") != INSPECTION_SCHEMA:
        raise WorldContractError("unexpected inspection report schema")
    version = int(report.get("schemaVersion", 0))
    if version < 3:
        raise WorldContractError("inspection report schema is too old for source-package conversion")
    if not isinstance(report.get("glbFiles"), list) or not isinstance(report.get("plyFiles"), list):
        raise WorldContractError("inspection report must contain glbFiles and plyFiles arrays")
    if not isinstance(report.get("pairs"), list):
        raise WorldContractError("inspection report must contain pairs array")
    _reject_backend_handles(report, "inspection")
    return report


def _source_record(item: dict[str, Any], expected_suffix: str) -> dict[str, Any]:
    tile_id = _positive_int(item.get("tileId"), f"{expected_suffix}.tileId")
    label = item.get("sourceLabel")
    if not isinstance(label, str) or label != f"MipTile_{tile_id}.{expected_suffix}":
        raise WorldContractError(f"{expected_suffix} source label must be canonical for tile {tile_id}")
    return {
        "sourceLabel": label,
        "sha256": _sha256(item.get("sha256"), f"{label}.sha256"),
        "byteLength": _positive_int(item.get("byteLength"), f"{label}.byteLength"),
    }


def _sources_by_tile(items: Iterable[dict[str, Any]], suffix: str) -> dict[int, dict[str, Any]]:
    result: dict[int, dict[str, Any]] = {}
    for raw in items:
        if not isinstance(raw, dict):
            raise WorldContractError(f"{suffix} source entry must be an object")
        tile_id = _positive_int(raw.get("tileId"), f"{suffix}.tileId")
        if tile_id in result:
            raise WorldContractError(f"duplicate {suffix} tile id {tile_id}")
        result[tile_id] = _source_record(raw, suffix)
    return result


def build_source_package(
    *,
    package_id: str,
    inspection_report: dict[str, Any],
    frame_contract: dict[str, Any],
) -> dict[str, Any]:
    """Build one immutable source revision from inspection evidence.

    ``packageId`` is stable across reimports. ``revisionId`` is content-derived
    from source hashes and the normalized frame contract.
    """

    report = _validate_inspection(inspection_report)
    normalized_frame = scan_frames.validate_frame_contract(frame_contract)
    package_id = _stable_id(package_id, "packageId")
    glb = _sources_by_tile(report["glbFiles"], "glb")
    ply = _sources_by_tile(report["plyFiles"], "ply")
    if set(glb) != set(ply):
        raise WorldContractError(
            f"source package requires paired GLB/PLY tile IDs; glb={sorted(glb)}, ply={sorted(ply)}"
        )

    tiles = [
        {
            "tileId": tile_id,
            "stableTileId": f"{package_id}/tile/{tile_id}",
            "glb": glb[tile_id],
            "ply": ply[tile_id],
        }
        for tile_id in sorted(glb)
    ]
    revision_payload = {
        "packageId": package_id,
        "frameContract": normalized_frame,
        "tiles": tiles,
    }
    revision_hash = sha256_json(revision_payload)
    package = {
        "schema": SOURCE_PACKAGE_SCHEMA,
        "schemaVersion": SOURCE_PACKAGE_SCHEMA_VERSION,
        "packageId": package_id,
        "revisionId": f"sha256:{revision_hash}",
        "privacyClass": "PRIVATE_LOCAL_ONLY",
        "sourceFrameContract": normalized_frame,
        "sourceFrameContractSha256": scan_frames.contract_sha256(normalized_frame),
        "inspection": {
            "schema": report["schema"],
            "schemaVersion": int(report["schemaVersion"]),
            "datasetStatus": str(report.get("datasetStatus", "unknown")),
            "automaticEvidenceGatePassed": bool(
                isinstance(report.get("automaticEvidenceGate"), dict)
                and report["automaticEvidenceGate"].get("passed") is True
            ),
        },
        "tiles": tiles,
    }
    _reject_backend_handles(package, "sourcePackage")
    return package


def validate_source_package(package: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(package, dict):
        raise WorldContractError("source package must be an object")
    if package.get("schema") != SOURCE_PACKAGE_SCHEMA or int(package.get("schemaVersion", 0)) != SOURCE_PACKAGE_SCHEMA_VERSION:
        raise WorldContractError("unknown source package schema or version")
    _stable_id(package.get("packageId"), "packageId")
    revision = package.get("revisionId")
    if not isinstance(revision, str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", revision):
        raise WorldContractError("revisionId must be sha256:<hex>")
    if package.get("privacyClass") != "PRIVATE_LOCAL_ONLY":
        raise WorldContractError("source package privacyClass must remain PRIVATE_LOCAL_ONLY")
    normalized_frame = scan_frames.validate_frame_contract(package.get("sourceFrameContract"))
    expected_frame_hash = scan_frames.contract_sha256(normalized_frame)
    if package.get("sourceFrameContractSha256") != expected_frame_hash:
        raise WorldContractError("sourceFrameContractSha256 does not match normalized frame contract")
    if not isinstance(package.get("tiles"), list) or not package["tiles"]:
        raise WorldContractError("source package requires at least one tile")
    seen: set[int] = set()
    for tile in package["tiles"]:
        if not isinstance(tile, dict):
            raise WorldContractError("source package tile must be an object")
        tile_id = _positive_int(tile.get("tileId"), "tile.tileId")
        if tile_id in seen:
            raise WorldContractError(f"duplicate source package tile id {tile_id}")
        seen.add(tile_id)
        if tile.get("stableTileId") != f"{package['packageId']}/tile/{tile_id}":
            raise WorldContractError(f"stableTileId mismatch for tile {tile_id}")
        _source_record({"tileId": tile_id, **dict(tile.get("glb", {}))}, "glb")
        _source_record({"tileId": tile_id, **dict(tile.get("ply", {}))}, "ply")
    _reject_backend_handles(package, "sourcePackage")
    return package


def build_world_import_proposal(
    *,
    proposal_id: str,
    source_package: dict[str, Any],
    inspection_report: dict[str, Any],
) -> dict[str, Any]:
    """Create an explicitly unreviewed, bounds-evidence proposal.

    Manual approvals, ground truth, collision data and runtime handles are
    intentionally absent. A future review document must reference the proposal
    content hash instead of mutating this record.
    """

    package = validate_source_package(source_package)
    report = _validate_inspection(inspection_report)
    proposal_id = _stable_id(proposal_id, "proposalId")
    pairs: list[dict[str, Any]] = []
    for raw in sorted(report["pairs"], key=lambda item: int(item["tileId"])):
        if not isinstance(raw, dict):
            raise WorldContractError("pair evidence entry must be an object")
        tile_id = _positive_int(raw.get("tileId"), "pair.tileId")
        classification = raw.get("classification")
        if classification not in {"strong-match", "bounds-strong-match", "review", "incompatible"}:
            raise WorldContractError(f"unknown pair classification for tile {tile_id}")
        pairs.append(
            {
                "tileId": tile_id,
                "stableTileId": f"{package['packageId']}/tile/{tile_id}",
                "classification": "bounds-strong-match" if classification == "strong-match" else classification,
                "evidenceLevel": "BOUNDS_ONLY",
                "normalizedCenterDelta": float(raw.get("normalizedCenterDelta", 0.0)),
                "maxExtentRelativeError": float(raw.get("maxExtentRelativeError", 0.0)),
                "xyOverlapOfSmaller": float(raw.get("xyOverlapOfSmaller", 0.0)),
                "axisPermutationSuspicion": raw.get("axisPermutationSuspicion") is True,
            }
        )

    payload = {
        "schema": WORLD_PROPOSAL_SCHEMA,
        "schemaVersion": WORLD_PROPOSAL_SCHEMA_VERSION,
        "proposalId": proposal_id,
        "status": "UNREVIEWED",
        "privacyClass": "PRIVATE_LOCAL_ONLY",
        "sourcePackageId": package["packageId"],
        "sourceRevisionId": package["revisionId"],
        "sourceFrameContractSha256": package["sourceFrameContractSha256"],
        "inspectionSha256": sha256_json(report),
        "pairEvidence": pairs,
        "capabilities": {
            "sourceInspectionPassed": package["inspection"]["automaticEvidenceGatePassed"],
            "sourceFrameConfirmed": package["sourceFrameContract"]["confirmed"],
            "pairingEvidenceLevel": "BOUNDS_ONLY",
            "internalGeometryCorrespondencePassed": False,
            "acceptedWorldPatchReady": False,
            "collisionProjectionReady": False,
        },
        "manualDecisions": [],
        "warnings": [str(value) for value in report.get("warnings", [])],
    }
    _reject_backend_handles(payload, "worldImportProposal")
    payload["proposalContentSha256"] = sha256_json(payload)
    return payload


def validate_world_import_proposal(proposal: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(proposal, dict):
        raise WorldContractError("world import proposal must be an object")
    if proposal.get("schema") != WORLD_PROPOSAL_SCHEMA or int(proposal.get("schemaVersion", 0)) != WORLD_PROPOSAL_SCHEMA_VERSION:
        raise WorldContractError("unknown world import proposal schema or version")
    _stable_id(proposal.get("proposalId"), "proposalId")
    if proposal.get("status") != "UNREVIEWED":
        raise WorldContractError("v1 world import proposal must remain UNREVIEWED")
    if proposal.get("privacyClass") != "PRIVATE_LOCAL_ONLY":
        raise WorldContractError("world import proposal privacyClass must remain PRIVATE_LOCAL_ONLY")
    if proposal.get("manualDecisions") != []:
        raise WorldContractError("manual decisions belong to a separate review document")
    if not isinstance(proposal.get("pairEvidence"), list):
        raise WorldContractError("pairEvidence must be an array")
    expected_hash = proposal.get("proposalContentSha256")
    unsigned = dict(proposal)
    unsigned.pop("proposalContentSha256", None)
    if expected_hash != sha256_json(unsigned):
        raise WorldContractError("proposalContentSha256 mismatch")
    _reject_backend_handles(proposal, "worldImportProposal")
    return proposal
