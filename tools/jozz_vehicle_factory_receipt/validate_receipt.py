#!/usr/bin/env python3
"""Fail-closed validation for the native-generated JV web factory receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
from pathlib import Path
from typing import Any


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def run_git(root: Path, *args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(root), *args], text=True, encoding="utf-8"
    ).strip()


def finite_positive(value: Any, label: str) -> float:
    require(isinstance(value, (int, float)) and not isinstance(value, bool), f"{label} must be numeric")
    numeric = float(value)
    require(math.isfinite(numeric) and numeric > 0.0, f"{label} must be finite and positive")
    return numeric


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("receipt", type=Path)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()

    root = args.repo_root.resolve()
    receipt = json.loads(args.receipt.read_text(encoding="utf-8"))
    require(receipt.get("format") == "jv-web-factory-receipt", "unsupported receipt format")
    require(receipt.get("schemaVersion") == 1, "unsupported receipt schemaVersion")

    source = receipt["source"]
    require(source["repository"] == "Jozzpoly/Box3d_FunProject", "wrong source repository")
    require(source["commit"] == run_git(root, "rev-parse", "HEAD"), "source commit does not match checkout")
    require(source["dirty"] is False, "tracked native source was dirty during packaging")
    for file_row in source["files"]:
        path = root / file_row["path"]
        data = path.read_bytes()
        require(file_row["bytes"] == len(data), f"byte count mismatch: {file_row['path']}")
        require(file_row["sha256"] == hashlib.sha256(data).hexdigest(), f"sha mismatch: {file_row['path']}")
        require(
            file_row["gitBlob"] == run_git(root, "hash-object", file_row["path"]),
            f"git blob mismatch: {file_row['path']}",
        )

    payload = receipt["payload"]
    require(payload["format"] == "jv-web-factory-payload", "wrong payload format")
    require(payload["schemaVersion"] == 1, "wrong payload schema version")
    require(payload["fieldSource"] == "SaveJozzVehicleM6Config/JozzFieldDesc", "wrong field source")
    require(payload["sanitizerChanged"] is False, "factory config required sanitizer changes")
    require(payload["factoryConfig"] == payload["sanitizedConfig"], "factory and sanitized config differ")

    schema = receipt["fieldSchema"]
    paths = [row["path"] for row in schema]
    require(len(paths) == len(set(paths)), "duplicate serialized field path")
    require(receipt["serializedFieldCount"] == len(paths), "serialized field count mismatch")
    require(len(paths) >= 65, f"unexpectedly small serialized schema: {len(paths)}")
    for forbidden in (
        "rackTravel",
        "filterGroupIndex",
        "wheelEnvelope.radius",
        "wheelEnvelope.width",
        "wheelEnvelope.terrainCategoryBits",
    ):
        require(forbidden not in paths, f"non-serialized field leaked into config schema: {forbidden}")

    config = payload["factoryConfig"]
    derived = payload["derived"]
    runtime = payload["runtimeOnly"]
    features = payload["features"]
    solver = payload["solverProfile"]
    assets = payload["assetResolution"]

    require(config["frontRigType"] == 1 and config["rearRigType"] == 1, "factory topology is not double wishbone")
    require(features["activeFrontRigType"] == 1 and features["activeRearRigType"] == 1, "feature topology mismatch")
    require(features["activeWheelEnvelopeMode"] in features["supportedWheelEnvelopeModes"], "unsupported active wheel mode")
    require(features["rackCenteringAssistEnabled"] is False, "rack centering assist must default OFF")
    require(features["uprightAssistEnabled"] is False, "upright assist must default OFF")
    require(float(config["rackCenteringHertz"]) == 0.0, "rackCenteringHertz must be zero")
    require(config["uprightAssist"] is False, "uprightAssist must be false")

    require(solver == {
        "gravity": [0, -10, 0],
        "fixedDt": 1.0 / 60.0,
        "substeps": 4,
        "contactHertz": 30,
        "contactDampingRatio": 10,
        "contactSpeed": 3,
        "enableContinuous": False,
        "workerCount": 0,
    }, "solver profile mismatch")

    rack_travel = finite_positive(derived["rackTravel"], "rackTravel")
    dead_point = finite_positive(derived["steeringDeadPointDegrees"], "steeringDeadPointDegrees")
    require(dead_point > float(config["maxSteeringAngleDegrees"]), "max steer reaches/passes dead point")
    require(rack_travel < 0.5, "rackTravel outside plausible fixture range")
    require(isinstance(runtime["filterGroupIndex"], int) and runtime["filterGroupIndex"] < 0, "runtime group must be negative")

    wheel_radius = finite_positive(derived["wheelRadius"], "derived wheelRadius")
    wheel_width = finite_positive(derived["wheelWidth"], "derived wheelWidth")
    require(math.isclose(wheel_radius, float(assets["wheelRadius"]), rel_tol=0.0, abs_tol=1e-6), "wheel radius provenance mismatch")
    require(math.isclose(wheel_width, float(assets["wheelWidth"]), rel_tol=0.0, abs_tol=1e-6), "wheel width provenance mismatch")
    require(assets["metadataLoadedFromRuntimeReport"] is True, "asset runtime report was not loaded")
    require(assets["wheelDimensionsFallbackUsed"] is False, "wheel dimensions used built-in fallback")
    require(assets["travelHintFallbackUsed"] is False, "suspension travel hint used built-in fallback")
    require(assets["trailingArmContractLoaded"] is True, "trailing-arm contract was not loaded")
    require(assets["trailingArmFallbackUsed"] is False, "trailing-arm import used built-in fallback")

    canonical_payload = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    require(
        receipt["payloadReceipt"]["canonicalSha256"] == hashlib.sha256(canonical_payload).hexdigest(),
        "canonical payload hash mismatch",
    )

    print("F3 native factory receipt: PASS")
    print(f"source commit: {source['commit']}")
    print(f"serialized fields: {len(paths)}")
    print(f"wheel radius/width: {wheel_radius:.9f} / {wheel_width:.9f}")
    print(f"rack travel/dead point: {rack_travel:.9f} / {dead_point:.6f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
