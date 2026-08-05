#!/usr/bin/env python3
"""Run exactly one WHEEL-SOFT-03 Q2 case and validate its artifacts."""
from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import sys
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SCHEMA = "jv-wheel-soft-q2/v1"
REQUIRED_OBJECTS = ("rig", "softness", "equilibrium", "static_window", "response_window", "performance", "manifold")


def finite_tree(value: Any) -> bool:
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return True
    if isinstance(value, (int, float)):
        return math.isfinite(float(value))
    if isinstance(value, list):
        return all(finite_tree(item) for item in value)
    if isinstance(value, dict):
        return all(isinstance(key, str) and finite_tree(item) for key, item in value.items())
    return False


def binary_candidates() -> list[Path]:
    name = "jv_wheel_soft_q2.exe" if os.name == "nt" else "jv_wheel_soft_q2"
    result: list[Path] = []
    env = os.environ.get("JV_WHEEL_SOFT_Q2_BIN")
    if env:
        result.append(Path(env))
    for directory in (
        ROOT / "build-research" / "bin" / "Release",
        ROOT / "build-research" / "bin" / "RelWithDebInfo",
        ROOT / "build-research" / "bin" / "Debug",
        ROOT / "build-research" / "bin",
        ROOT / "build-soft-q2" / "bin" / "Release",
        ROOT / "build-soft-q2" / "bin",
    ):
        result.append(directory / name)
    return result


def resolve_binary(explicit: str | None) -> Path:
    if explicit:
        path = Path(explicit).resolve()
        if path.is_file():
            return path
        raise FileNotFoundError(f"nie znaleziono binarium Q2: {path}")
    for path in binary_candidates():
        if path.is_file():
            return path.resolve()
    searched = "\n  - ".join(str(path) for path in binary_candidates())
    raise FileNotFoundError(f"nie znaleziono jv_wheel_soft_q2; sprawdzono:\n  - {searched}")


def validate_metrics(metrics: Any, variant: str, hertz_scale: float) -> list[str]:
    errors: list[str] = []
    if not isinstance(metrics, dict):
        return ["metrics.json: wymagany obiekt JSON"]
    if metrics.get("schema") != SCHEMA:
        errors.append(f"schema: oczekiwano {SCHEMA!r}, jest {metrics.get('schema')!r}")
    if metrics.get("variant") != variant:
        errors.append(f"variant: oczekiwano {variant!r}, jest {metrics.get('variant')!r}")
    scale = metrics.get("wheel_contact_hertz_scale")
    if not isinstance(scale, (int, float)) or isinstance(scale, bool) or not math.isfinite(float(scale)):
        errors.append("wheel_contact_hertz_scale: wymagana skończona liczba")
    elif abs(float(scale) - hertz_scale) > 1.0e-12:
        errors.append(f"wheel_contact_hertz_scale: oczekiwano {hertz_scale}, jest {scale}")
    if metrics.get("finite") is not True:
        errors.append("finite: runner musi jawnie potwierdzić true")
    for key in REQUIRED_OBJECTS:
        if not isinstance(metrics.get(key), dict):
            errors.append(f"{key}: wymagany obiekt")
    manifold = metrics.get("manifold")
    if isinstance(manifold, dict) and manifold.get("topology_drift_steps") != 0:
        errors.append(f"manifold.topology_drift_steps: oczekiwano 0, jest {manifold.get('topology_drift_steps')!r}")
    if not finite_tree(metrics):
        errors.append("metrics.json zawiera NaN, Infinity albo nieobsługiwany typ")
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary")
    parser.add_argument("--case-dir", required=True)
    parser.add_argument("--variant", required=True)
    parser.add_argument("--hertz-scale", required=True, type=float)
    args = parser.parse_args(argv)

    case_dir = Path(args.case_dir).resolve()
    case_dir.mkdir(parents=True, exist_ok=True)
    for name in ("metrics.json", "metrics.json.tmp", "trace.csv"):
        path = case_dir / name
        if path.exists():
            path.unlink()

    try:
        binary = resolve_binary(args.binary)
    except (FileNotFoundError, OSError) as exc:
        print(f"WHEEL-SOFT-Q2 ERROR: {exc}", file=sys.stderr)
        return 2

    command = [
        str(binary),
        "--case-dir", str(case_dir),
        "--variant", args.variant,
        "--hertz-scale", format(args.hertz_scale, ".17g"),
    ]
    result = subprocess.run(command, cwd=str(ROOT), check=False)
    if result.returncode != 0:
        return result.returncode

    metrics_path = case_dir / "metrics.json"
    trace_path = case_dir / "trace.csv"
    if not metrics_path.is_file() or not trace_path.is_file():
        print("WHEEL-SOFT-Q2 ERROR: runner nie dostarczył metrics.json i trace.csv", file=sys.stderr)
        return 2
    try:
        metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"WHEEL-SOFT-Q2 ERROR: nie można odczytać metrics.json: {exc}", file=sys.stderr)
        return 2
    errors = validate_metrics(metrics, args.variant, args.hertz_scale)
    if errors:
        print("WHEEL-SOFT-Q2 ERROR: metrics.json nie przeszedł kontraktu:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
