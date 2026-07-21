#!/usr/bin/env python3
"""Build and verify a closed catalog of scan-derived products and legal gates.

The catalog is metadata only. It records which disposable derivatives exist,
their exact source bindings and resource cost, while making illegal shortcuts
(source evidence directly becoming collision or accepted-world truth) invalid.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import sys
from typing import Any, Sequence

MODULE_DIR = Path(__file__).resolve().parent


def _load_sibling(name: str) -> Any:
    path = MODULE_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(f"_jozz_catalog_{name}", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


scan_import_bundle = _load_sibling("scan_import_bundle")
scan_preview_pack = _load_sibling("scan_preview_pack")
scan_surface_evidence = _load_sibling("scan_surface_evidence")
scan_world_contracts = _load_sibling("scan_world_contracts")

SCHEMA = "jozz.scan-derivative-catalog"
SCHEMA_VERSION = 1
PRIVACY_CLASS = "PRIVATE_LOCAL_ONLY"
PURPOSE = "DERIVATIVE_GRAPH_AND_RESOURCE_ACCOUNTING"

SOURCE_REVISION = "SOURCE_REVISION"
EXACT_VISUAL = "EXACT_VISUAL_PREVIEW"
OPTIMIZED_VISUAL = "OPTIMIZED_VISUAL"
SURFACE_EVIDENCE = "SURFACE_EVIDENCE"
ACCEPTED_SURFACE = "ACCEPTED_SURFACE"
COLLISION_PROJECTION = "COLLISION_PROJECTION"

_ENTRY_ORDER = (
    EXACT_VISUAL,
    OPTIMIZED_VISUAL,
    SURFACE_EVIDENCE,
    ACCEPTED_SURFACE,
    COLLISION_PROJECTION,
)
_ALLOWED_PARENTS = {
    EXACT_VISUAL: [SOURCE_REVISION],
    OPTIMIZED_VISUAL: [EXACT_VISUAL],
    SURFACE_EVIDENCE: [SOURCE_REVISION],
    ACCEPTED_SURFACE: [SURFACE_EVIDENCE],
    COLLISION_PROJECTION: [ACCEPTED_SURFACE],
}
_RUNTIME_POLICIES = {
    "renderInterestCenter": "CAMERA",
    "physicsInterestCenter": "VEHICLE",
    "coarseFirstVisualLoadingAllowed": True,
    "exactEvidenceRemainsSeparatelyAddressable": True,
    "unknownSurfacePolicy": "NOT_COLLIDABLE_UNTIL_REVIEWED",
    "runtimeParsesSourceGlbOrPly": False,
    "offlineCookingRequired": True,
    "visualOptimizationDecision": "MEASURE_EXACT_PREVIEW_FIRST",
    "geometryAndTextureBudgetsSeparated": True,
}
_CAPABILITY_KEYS = {
    "renderable",
    "optimized",
    "sourceEvidenceExact",
    "surfaceQueryable",
    "acceptedWorld",
    "collisionReady",
}
_CATALOG_KEYS = {
    "schema",
    "schemaVersion",
    "status",
    "privacyClass",
    "purpose",
    "sourceBundleContentSha256",
    "packageId",
    "sourceRevisionId",
    "sourceFrameContractSha256",
    "runtimePolicies",
    "entries",
    "catalogContentSha256",
}
_ENTRY_KEYS = {
    "kind",
    "status",
    "contentSha256",
    "parents",
    "resourceSummary",
    "capabilities",
    "blockingReason",
}
_EXACT_RESOURCE_KEYS = {
    "tileCount",
    "vertexCount",
    "triangleCount",
    "assetBytes",
    "estimatedResidentBytes",
}
_SURFACE_RESOURCE_KEYS = {
    "cellCount",
    "observedCellCount",
    "unknownCellCount",
    "assetBytes",
    "estimatedResidentBytes",
}


class DerivativeCatalogError(ValueError):
    pass


def _canonical_json_bytes(value: Any) -> bytes:
    return scan_import_bundle.canonical_json_bytes(value)


def _strict_json(path: Path) -> dict[str, Any]:
    return scan_import_bundle.load_json_strict(path)


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _sha(value: Any, label: str) -> str:
    try:
        return scan_import_bundle._sha(value, label)
    except scan_import_bundle.ImportBundleError as exc:
        raise DerivativeCatalogError(str(exc)) from exc


def _revision(value: Any) -> str:
    if not isinstance(value, str) or not value.startswith("sha256:"):
        raise DerivativeCatalogError("sourceRevisionId must be sha256:<hex>")
    _sha(value[7:], "sourceRevisionId")
    return value


def _uint(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise DerivativeCatalogError(f"{label} must be a non-negative integer")
    return value


def _exact_keys(value: dict[str, Any], expected: set[str], label: str) -> None:
    actual = set(value)
    if actual != expected:
        raise DerivativeCatalogError(
            f"{label} keys mismatch; missing={sorted(expected-actual)} extra={sorted(actual-expected)}"
        )


def _capabilities(
    *,
    renderable: bool = False,
    optimized: bool = False,
    source_exact: bool = False,
    surface_queryable: bool = False,
    accepted: bool = False,
    collision: bool = False,
) -> dict[str, bool]:
    return {
        "renderable": renderable,
        "optimized": optimized,
        "sourceEvidenceExact": source_exact,
        "surfaceQueryable": surface_queryable,
        "acceptedWorld": accepted,
        "collisionReady": collision,
    }


def _exact_visual_entry(manifest: dict[str, Any]) -> dict[str, Any]:
    total_bytes = sum(int(tile["byteLength"]) for tile in manifest["tiles"])
    total_vertices = sum(int(tile["vertexCount"]) for tile in manifest["tiles"])
    total_triangles = sum(int(tile["triangleCount"]) for tile in manifest["tiles"])
    return {
        "kind": EXACT_VISUAL,
        "status": "READY",
        "contentSha256": manifest["previewContentSha256"],
        "parents": [SOURCE_REVISION],
        "resourceSummary": {
            "tileCount": manifest["tileCount"],
            "vertexCount": total_vertices,
            "triangleCount": total_triangles,
            "assetBytes": total_bytes,
            "estimatedResidentBytes": total_bytes,
        },
        "capabilities": _capabilities(renderable=True, source_exact=True),
        "blockingReason": None,
    }


def _optimized_visual_entry() -> dict[str, Any]:
    return {
        "kind": OPTIMIZED_VISUAL,
        "status": "NOT_BUILT",
        "contentSha256": None,
        "parents": [EXACT_VISUAL],
        "resourceSummary": None,
        "capabilities": _capabilities(optimized=True),
        "blockingReason": "MEASURE_EXACT_PREVIEW_BEFORE_LOD_COOKING",
    }


def _surface_entry(manifest: dict[str, Any] | None) -> dict[str, Any]:
    if manifest is None:
        return {
            "kind": SURFACE_EVIDENCE,
            "status": "NOT_BUILT",
            "contentSha256": None,
            "parents": [SOURCE_REVISION],
            "resourceSummary": None,
            "capabilities": _capabilities(),
            "blockingReason": "BUILD_CONSERVATIVE_PLY_SURFACE_EVIDENCE",
        }
    width = int(manifest["grid"]["width"])
    height = int(manifest["grid"]["height"])
    return {
        "kind": SURFACE_EVIDENCE,
        "status": "READY",
        "contentSha256": manifest["surfaceEvidenceContentSha256"],
        "parents": [SOURCE_REVISION],
        "resourceSummary": {
            "cellCount": width * height,
            "observedCellCount": manifest["statistics"]["observedCellCount"],
            "unknownCellCount": manifest["statistics"]["unknownCellCount"],
            "assetBytes": manifest["surfaceByteLength"],
            "estimatedResidentBytes": manifest["surfaceByteLength"],
        },
        "capabilities": _capabilities(surface_queryable=True),
        "blockingReason": None,
    }


def _blocked_entry(kind: str, parent: str, status: str, reason: str) -> dict[str, Any]:
    return {
        "kind": kind,
        "status": status,
        "contentSha256": None,
        "parents": [parent],
        "resourceSummary": None,
        "capabilities": _capabilities(),
        "blockingReason": reason,
    }


def build_catalog_document(
    *,
    preview_manifest: dict[str, Any],
    surface_manifest: dict[str, Any] | None = None,
) -> dict[str, Any]:
    preview = scan_preview_pack._validate_manifest(dict(preview_manifest))
    surface = (
        scan_surface_evidence._validate_manifest(dict(surface_manifest))
        if surface_manifest is not None
        else None
    )
    if surface is not None:
        for key in (
            "sourceBundleContentSha256",
            "packageId",
            "sourceRevisionId",
            "sourceFrameContractSha256",
        ):
            if surface[key] != preview[key]:
                raise DerivativeCatalogError(
                    f"surface evidence and preview disagree on {key}"
                )
    entries = [
        _exact_visual_entry(preview),
        _optimized_visual_entry(),
        _surface_entry(surface),
        _blocked_entry(
            ACCEPTED_SURFACE,
            SURFACE_EVIDENCE,
            "BLOCKED_OWNER_REVIEW" if surface is not None else "BLOCKED_MISSING_SURFACE_EVIDENCE",
            "OWNER_REVIEW_AND_AUTHORED_DECISIONS_REQUIRED" if surface is not None else "SURFACE_EVIDENCE_REQUIRED",
        ),
        _blocked_entry(
            COLLISION_PROJECTION,
            ACCEPTED_SURFACE,
            "BLOCKED_ACCEPTED_SURFACE",
            "ACCEPTED_SURFACE_REQUIRED",
        ),
    ]
    core = {
        "schema": SCHEMA,
        "schemaVersion": SCHEMA_VERSION,
        "status": "COMPLETE",
        "privacyClass": PRIVACY_CLASS,
        "purpose": PURPOSE,
        "sourceBundleContentSha256": preview["sourceBundleContentSha256"],
        "packageId": preview["packageId"],
        "sourceRevisionId": preview["sourceRevisionId"],
        "sourceFrameContractSha256": preview["sourceFrameContractSha256"],
        "runtimePolicies": dict(_RUNTIME_POLICIES),
        "entries": entries,
    }
    document = dict(core)
    document["catalogContentSha256"] = _sha256_bytes(_canonical_json_bytes(core))
    return validate_catalog(document)


def validate_catalog(document: dict[str, Any]) -> dict[str, Any]:
    _exact_keys(document, _CATALOG_KEYS, "derivative catalog")
    if (
        document["schema"] != SCHEMA
        or _uint(document["schemaVersion"], "schemaVersion") != SCHEMA_VERSION
        or document["status"] != "COMPLETE"
        or document["privacyClass"] != PRIVACY_CLASS
        or document["purpose"] != PURPOSE
    ):
        raise DerivativeCatalogError("invalid derivative catalog identity")
    _sha(document["sourceBundleContentSha256"], "sourceBundleContentSha256")
    try:
        scan_world_contracts._stable_id(document["packageId"], "packageId")
    except scan_world_contracts.WorldContractError as exc:
        raise DerivativeCatalogError(str(exc)) from exc
    _revision(document["sourceRevisionId"])
    _sha(document["sourceFrameContractSha256"], "sourceFrameContractSha256")
    if document["runtimePolicies"] != _RUNTIME_POLICIES:
        raise DerivativeCatalogError("runtime policy boundary mismatch")
    expected_hash = _sha(document["catalogContentSha256"], "catalogContentSha256")
    unsigned = dict(document)
    unsigned.pop("catalogContentSha256")
    if _sha256_bytes(_canonical_json_bytes(unsigned)) != expected_hash:
        raise DerivativeCatalogError("catalogContentSha256 mismatch")

    entries = document["entries"]
    if not isinstance(entries, list) or len(entries) != len(_ENTRY_ORDER):
        raise DerivativeCatalogError("catalog entry list is invalid")
    by_kind: dict[str, dict[str, Any]] = {}
    allowed_statuses = {
        EXACT_VISUAL: {"READY"},
        OPTIMIZED_VISUAL: {"NOT_BUILT"},
        SURFACE_EVIDENCE: {"READY", "NOT_BUILT"},
        ACCEPTED_SURFACE: {"BLOCKED_OWNER_REVIEW", "BLOCKED_MISSING_SURFACE_EVIDENCE"},
        COLLISION_PROJECTION: {"BLOCKED_ACCEPTED_SURFACE"},
    }
    for expected_kind, entry in zip(_ENTRY_ORDER, entries):
        if not isinstance(entry, dict):
            raise DerivativeCatalogError("catalog entry must be an object")
        _exact_keys(entry, _ENTRY_KEYS, f"entry[{expected_kind}]")
        if entry["kind"] != expected_kind or entry["parents"] != _ALLOWED_PARENTS[expected_kind]:
            raise DerivativeCatalogError(f"illegal derivative graph edge for {expected_kind}")
        status = entry["status"]
        if status not in allowed_statuses[expected_kind]:
            raise DerivativeCatalogError(f"unsupported status for {expected_kind}: {status}")
        if expected_kind in by_kind:
            raise DerivativeCatalogError("duplicate derivative kind")
        by_kind[expected_kind] = entry
        capabilities = entry["capabilities"]
        if not isinstance(capabilities, dict):
            raise DerivativeCatalogError("entry capabilities must be an object")
        _exact_keys(capabilities, _CAPABILITY_KEYS, "entry capabilities")
        if any(not isinstance(value, bool) for value in capabilities.values()):
            raise DerivativeCatalogError("entry capabilities must be boolean")
        if status == "READY":
            _sha(entry["contentSha256"], f"entry[{expected_kind}].contentSha256")
            if entry["blockingReason"] is not None or not isinstance(entry["resourceSummary"], dict):
                raise DerivativeCatalogError("READY derivative has blocker or no resources")
        else:
            if entry["contentSha256"] is not None or entry["resourceSummary"] is not None:
                raise DerivativeCatalogError("non-ready derivative cannot claim content/resources")
            if not isinstance(entry["blockingReason"], str) or not entry["blockingReason"]:
                raise DerivativeCatalogError("blocked derivative needs a reason")

    exact = by_kind[EXACT_VISUAL]
    optimized = by_kind[OPTIMIZED_VISUAL]
    surface = by_kind[SURFACE_EVIDENCE]
    accepted = by_kind[ACCEPTED_SURFACE]
    collision = by_kind[COLLISION_PROJECTION]
    if exact["capabilities"] != _capabilities(renderable=True, source_exact=True):
        raise DerivativeCatalogError("exact visual capability boundary mismatch")
    if optimized["capabilities"] != _capabilities(optimized=True):
        raise DerivativeCatalogError("optimized visual capability boundary mismatch")
    if surface["capabilities"] != _capabilities(surface_queryable=surface["status"] == "READY"):
        raise DerivativeCatalogError("surface evidence capability boundary mismatch")
    if accepted["capabilities"] != _capabilities() or collision["capabilities"] != _capabilities():
        raise DerivativeCatalogError("blocked authority/collision capability overclaim")

    exact_resources = exact["resourceSummary"]
    _exact_keys(exact_resources, _EXACT_RESOURCE_KEYS, "exact visual resources")
    for key, value in exact_resources.items():
        _uint(value, f"exactResources.{key}")
    if exact_resources["assetBytes"] != exact_resources["estimatedResidentBytes"]:
        raise DerivativeCatalogError("v1 exact resident estimate must equal asset bytes")
    if surface["status"] == "READY":
        resources = surface["resourceSummary"]
        _exact_keys(resources, _SURFACE_RESOURCE_KEYS, "surface resources")
        for key, value in resources.items():
            _uint(value, f"surfaceResources.{key}")
        if resources["observedCellCount"] + resources["unknownCellCount"] != resources["cellCount"]:
            raise DerivativeCatalogError("surface resource cell coverage mismatch")
        if resources["assetBytes"] != resources["estimatedResidentBytes"]:
            raise DerivativeCatalogError("v1 surface resident estimate must equal asset bytes")
        if accepted["status"] != "BLOCKED_OWNER_REVIEW":
            raise DerivativeCatalogError("ready surface evidence must lead to owner review")
    elif accepted["status"] != "BLOCKED_MISSING_SURFACE_EVIDENCE":
        raise DerivativeCatalogError("missing surface evidence blocker mismatch")
    if collision["status"] != "BLOCKED_ACCEPTED_SURFACE":
        raise DerivativeCatalogError("collision cannot bypass accepted surface")
    return document


def write_catalog(
    *,
    preview: Path,
    output: Path,
    surface_evidence: Path | None = None,
    force: bool = False,
) -> Path:
    preview_summary = scan_preview_pack.verify_preview_pack(preview)
    preview_manifest = _strict_json(Path(preview) / "COMPLETE.json")
    surface_manifest = None
    if surface_evidence is not None:
        scan_surface_evidence.verify_surface_evidence_pack(surface_evidence)
        surface_manifest = _strict_json(Path(surface_evidence) / "COMPLETE.json")
    document = build_catalog_document(
        preview_manifest=preview_manifest,
        surface_manifest=surface_manifest,
    )
    if document["sourceRevisionId"] != preview_summary["sourceRevisionId"]:
        raise DerivativeCatalogError("preview verifier revision differs from catalog")
    output = Path(output)
    if output.exists() and not force:
        raise DerivativeCatalogError("catalog output exists; pass --force to replace")
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.parent.is_symlink():
        raise DerivativeCatalogError("catalog output parent must not be a symlink")
    temporary = output.with_name(f".{output.name}.tmp-{os.getpid()}")
    try:
        temporary.write_bytes(_canonical_json_bytes(document))
        os.replace(temporary, output)
    finally:
        if temporary.exists():
            temporary.unlink()
    validate_catalog(_strict_json(output))
    return output


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("build")
    build.add_argument("--preview", required=True, type=Path)
    build.add_argument("--surface-evidence", type=Path)
    build.add_argument("--output", type=Path, default=Path("build/scan_pipeline/derivative_catalog.json"))
    build.add_argument("--force", action="store_true")
    verify = sub.add_parser("verify")
    verify.add_argument("catalog", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "build":
            path = write_catalog(
                preview=args.preview,
                surface_evidence=args.surface_evidence,
                output=args.output,
                force=args.force,
            )
            document = validate_catalog(_strict_json(path))
        else:
            path = args.catalog
            document = validate_catalog(_strict_json(path))
        print(
            "scan_derivative_catalog: OK | "
            f"path={path} catalog_sha256={document['catalogContentSha256']} "
            f"revision={document['sourceRevisionId']}"
        )
        return 0
    except (
        OSError,
        DerivativeCatalogError,
        scan_import_bundle.ImportBundleError,
        scan_preview_pack.PreviewPackError,
        scan_surface_evidence.SurfaceEvidenceError,
    ) as exc:
        print(f"scan_derivative_catalog: ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
