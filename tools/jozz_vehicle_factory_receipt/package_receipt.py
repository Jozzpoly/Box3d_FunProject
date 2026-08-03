#!/usr/bin/env python3
"""Package the runtime-generated JV factory payload with exact source provenance."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
from typing import Any

RELEVANT_PATHS = (
    "samples/jozz_vehicle_m6_config_io.cpp",
    "samples/jozz_vehicle_m6_suspension_rig.cpp",
    "samples/jozz_vehicle_m6_suspension_rig.h",
    "samples/jozz_vehicle_asset_dimensions.cpp",
    "samples/jozz_vehicle_asset_metadata.cpp",
    "samples/jozz_vehicle_m7_suspension_import.cpp",
    "assets/source/Offroad_Big_Wheels.gltf",
    "assets/contracts/offroad_big_wheel.asset.json",
    "assets/reports/asset_audit_latest.json",
    "assets/contracts/one_sided_wheel_mount.asset.json",
)


def run_git(root: Path, *args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(root), *args], text=True, encoding="utf-8"
    ).strip()


def tracked_tree_is_dirty(root: Path) -> bool:
    result = subprocess.run(
        ["git", "-C", str(root), "diff", "--quiet", "HEAD", "--"],
        check=False,
    )
    if result.returncode not in (0, 1):
        raise RuntimeError(f"git diff failed with exit code {result.returncode}")
    return result.returncode == 1


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def describe_value(value: Any) -> str:
    if isinstance(value, bool):
        return "bool"
    if isinstance(value, int):
        return "int"
    if isinstance(value, float):
        return "float"
    if isinstance(value, str):
        return "string"
    if (
        isinstance(value, list)
        and len(value) == 3
        and all(isinstance(item, (int, float)) and not isinstance(item, bool) for item in value)
    ):
        return "vec3"
    if isinstance(value, list):
        return "array"
    if isinstance(value, dict):
        return "object"
    raise TypeError(f"unsupported schema value: {type(value).__name__}")


def collect_field_schema(value: Any, prefix: str = "") -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    if not isinstance(value, dict):
        raise TypeError("factoryConfig must be an object")
    for key, child in value.items():
        path = f"{prefix}.{key}" if prefix else key
        kind = describe_value(child)
        if kind == "object":
            rows.extend(collect_field_schema(child, path))
        else:
            rows.append({"path": path, "type": kind, "source": "native-config-writer"})
    return rows


def file_receipt(root: Path, relative_path: str) -> dict[str, Any]:
    path = root / relative_path
    data = path.read_bytes()
    return {
        "path": relative_path,
        "gitBlob": run_git(root, "hash-object", relative_path),
        "sha256": sha256_bytes(data),
        "bytes": len(data),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("payload", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()

    root = args.repo_root.resolve()
    payload_bytes = args.payload.read_bytes()
    payload = json.loads(payload_bytes)
    if payload.get("format") != "jv-web-factory-payload" or payload.get("schemaVersion") != 1:
        raise SystemExit("unexpected native payload format")

    field_schema = collect_field_schema(payload["factoryConfig"])
    canonical_payload = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode(
        "utf-8"
    )
    source_commit = run_git(root, "rev-parse", "HEAD")
    source_branch = os.environ.get("GITHUB_REF_NAME") or run_git(root, "rev-parse", "--abbrev-ref", "HEAD")

    receipt = {
        "format": "jv-web-factory-receipt",
        "schemaVersion": 1,
        "source": {
            "repository": "Jozzpoly/Box3d_FunProject",
            "branch": source_branch,
            "commit": source_commit,
            "dirty": tracked_tree_is_dirty(root),
            "files": [file_receipt(root, path) for path in RELEVANT_PATHS],
        },
        "payloadReceipt": {
            "rawSha256": sha256_bytes(payload_bytes),
            "canonicalSha256": sha256_bytes(canonical_payload),
            "bytes": len(payload_bytes),
        },
        "fieldSchema": field_schema,
        "serializedFieldCount": len(field_schema),
        "derivedFields": [
            "rackTravel",
            "steeringDeadPointDegrees",
            "wheelRadius",
            "wheelWidth",
            "terrainCategoryBitsHex",
            "minimumTorusSegments",
        ],
        "runtimeOnlyFields": ["filterGroupIndex"],
        "payload": payload,
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(receipt, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8"
    )
    print(f"packaged factory receipt: {args.output}")
    print(f"source commit: {source_commit}")
    print(f"serialized fields: {len(field_schema)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
