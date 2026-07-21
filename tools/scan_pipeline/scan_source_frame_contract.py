#!/usr/bin/env python3
"""Create or validate explicit source-to-lab frame contracts.

The command derives handedness and the signed-permutation matrix from semantic
axis roles. It never infers source axes from scan bounds and never marks a
contract confirmed unless the owner passes ``--confirmed`` explicitly.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import sys
from typing import Any, Mapping, Sequence
import uuid

MODULE_DIR = Path(__file__).resolve().parent
_AXIS_PATTERN = re.compile(r"^([+-])([XYZ])$")
_AXIS_ROLES = ("right", "forward", "up")


class SourceFrameCliError(ValueError):
    pass


def _load_scan_frames() -> Any:
    path = MODULE_DIR / "scan_frames.py"
    spec = importlib.util.spec_from_file_location("_jozz_source_frame_cli_frames", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _strict_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise SourceFrameCliError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json_strict(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(
            Path(path).read_text(encoding="utf-8"),
            object_pairs_hook=_strict_object,
            parse_constant=lambda token: (_ for _ in ()).throw(
                SourceFrameCliError(f"non-standard JSON constant: {token}")
            ),
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise SourceFrameCliError(f"cannot read JSON from {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise SourceFrameCliError("source-frame file must contain a JSON object")
    return value


def _axis_vector(label: str, field: str) -> tuple[int, int, int]:
    match = _AXIS_PATTERN.fullmatch(label)
    if not match:
        raise SourceFrameCliError(f"{field} must be a signed axis such as +X or -Z")
    sign = 1 if match.group(1) == "+" else -1
    index = {"X": 0, "Y": 1, "Z": 2}[match.group(2)]
    result = [0, 0, 0]
    result[index] = sign
    return tuple(result)


def _basis(axis_roles: Mapping[str, str], label: str) -> list[list[int]]:
    vectors = [_axis_vector(str(axis_roles.get(role, "")), f"{label}.{role}") for role in _AXIS_ROLES]
    coordinates = [next(i for i, value in enumerate(vector) if value) for vector in vectors]
    if len(set(coordinates)) != 3:
        raise SourceFrameCliError(f"{label} must assign three distinct coordinate axes")
    return [[vectors[column][row] for column in range(3)] for row in range(3)]


def _transpose(matrix: Sequence[Sequence[int]]) -> list[list[int]]:
    return [[int(matrix[row][column]) for row in range(3)] for column in range(3)]


def _matmul(a: Sequence[Sequence[int]], b: Sequence[Sequence[int]]) -> list[list[int]]:
    return [
        [sum(int(a[row][k]) * int(b[k][column]) for k in range(3)) for column in range(3)]
        for row in range(3)
    ]


def _determinant(matrix: Sequence[Sequence[int]]) -> int:
    a, b, c = matrix
    return int(
        round(
            a[0] * (b[1] * c[2] - b[2] * c[1])
            - a[1] * (b[0] * c[2] - b[2] * c[0])
            + a[2] * (b[0] * c[1] - b[1] * c[0])
        )
    )


def _handedness(basis: Sequence[Sequence[int]]) -> str:
    determinant = _determinant(basis)
    if determinant not in (-1, 1):
        raise SourceFrameCliError("axis roles do not form an invertible basis")
    return "right" if determinant == 1 else "left"


def _finite_origin(values: Sequence[float]) -> list[float]:
    if len(values) != 3:
        raise SourceFrameCliError("local origin requires exactly three values")
    result = [float(value) for value in values]
    if not all(math.isfinite(value) for value in result):
        raise SourceFrameCliError("local origin contains a non-finite value")
    return result


def build_contract(
    *,
    source_units_per_meter: float,
    source_axis_roles: Mapping[str, str],
    lab_axis_roles: Mapping[str, str],
    local_origin_source: Sequence[float],
    confirmed: bool,
    mirror_approved: bool,
) -> dict[str, Any]:
    units = float(source_units_per_meter)
    if not math.isfinite(units) or units <= 0:
        raise SourceFrameCliError("source units per meter must be finite and positive")

    source_basis = _basis(source_axis_roles, "source axis roles")
    lab_basis = _basis(lab_axis_roles, "lab axis roles")
    matrix = _matmul(lab_basis, _transpose(source_basis))
    determinant = _determinant(matrix)
    orientation = "preserve" if determinant == 1 else "mirror"
    if orientation == "mirror" and not mirror_approved:
        raise SourceFrameCliError(
            "derived transform mirrors orientation; rerun only after review with --mirror-approved"
        )

    contract = {
        "schema": "jozz.scan-source-frame",
        "schemaVersion": 1,
        "confirmed": bool(confirmed),
        "sourceFrame": {
            "unitsPerMeter": units,
            "handedness": _handedness(source_basis),
            "axisRoles": {role: str(source_axis_roles[role]) for role in _AXIS_ROLES},
        },
        "labFrame": {
            "handedness": _handedness(lab_basis),
            "axisRoles": {role: str(lab_axis_roles[role]) for role in _AXIS_ROLES},
        },
        "sourceToLab": {
            "axisMatrix": matrix,
            "orientationChange": orientation,
            "mirrorApproved": bool(mirror_approved),
            "localOriginSource": _finite_origin(local_origin_source),
        },
    }
    return _load_scan_frames().validate_frame_contract(contract)


def write_contract(path: Path, contract: dict[str, Any], *, force: bool) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not force:
        raise SourceFrameCliError(f"refusing to overwrite existing file: {path}")
    payload = (json.dumps(contract, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode("utf-8")
    temporary = path.with_name(f".{path.name}.tmp-{os.getpid()}-{uuid.uuid4().hex}")
    try:
        with temporary.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def _axis_roles(args: argparse.Namespace, prefix: str) -> dict[str, str]:
    return {
        "right": getattr(args, f"{prefix}_right"),
        "forward": getattr(args, f"{prefix}_forward"),
        "up": getattr(args, f"{prefix}_up"),
    }


def _create(args: argparse.Namespace) -> int:
    contract = build_contract(
        source_units_per_meter=args.source_units_per_meter,
        source_axis_roles=_axis_roles(args, "source"),
        lab_axis_roles=_axis_roles(args, "lab"),
        local_origin_source=args.local_origin_source,
        confirmed=args.confirmed,
        mirror_approved=args.mirror_approved,
    )
    write_contract(args.output, contract, force=args.force)
    transform = contract["sourceToLab"]
    print(
        "scan_source_frame_contract: OK | "
        f"path={args.output} confirmed={str(contract['confirmed']).lower()} "
        f"determinant={transform['determinant']} orientation={transform['orientationChange']}"
    )
    if not contract["confirmed"]:
        print("scan_source_frame_contract: REVIEW_REQUIRED | contract remains unconfirmed")
    return 0


def _validate(args: argparse.Namespace) -> int:
    contract = _load_scan_frames().validate_frame_contract(load_json_strict(args.contract))
    if args.require_confirmed and not contract["confirmed"]:
        raise SourceFrameCliError("source-frame contract is valid but not owner-confirmed")
    transform = contract["sourceToLab"]
    print(
        "scan_source_frame_contract: OK | "
        f"path={args.contract} confirmed={str(contract['confirmed']).lower()} "
        f"determinant={transform['determinant']} orientation={transform['orientationChange']}"
    )
    return 0


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser("create", help="derive and write one frame contract")
    create.add_argument("--output", required=True, type=Path)
    create.add_argument("--source-units-per-meter", required=True, type=float)
    create.add_argument("--source-right", required=True)
    create.add_argument("--source-forward", required=True)
    create.add_argument("--source-up", required=True)
    create.add_argument("--local-origin-source", required=True, nargs=3, type=float, metavar=("X", "Y", "Z"))
    create.add_argument("--lab-right", default="+X")
    create.add_argument("--lab-forward", default="-Z")
    create.add_argument("--lab-up", default="+Y")
    create.add_argument("--confirmed", action="store_true")
    create.add_argument("--mirror-approved", action="store_true")
    create.add_argument("--force", action="store_true")
    create.set_defaults(handler=_create)

    validate = subparsers.add_parser("validate", help="validate an existing frame contract")
    validate.add_argument("contract", type=Path)
    validate.add_argument("--require-confirmed", action="store_true")
    validate.set_defaults(handler=_validate)

    args = parser.parse_args(argv)
    try:
        return int(args.handler(args))
    except (OSError, SourceFrameCliError, ValueError) as exc:
        print(f"scan_source_frame_contract: ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
