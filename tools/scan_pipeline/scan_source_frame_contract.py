#!/usr/bin/env python3
"""Create, propose, confirm or validate explicit source-to-lab frame contracts.

The command derives handedness and the signed-permutation matrix from semantic
axis roles. It never infers source axes from scan bounds and never marks a
contract confirmed without a separate explicit owner action.

The owner-safe flow deliberately keeps private origin coordinates and local file
paths out of command arguments and console output:

1. ``propose-from-inspection`` derives only the local origin from verified global
   bounds and writes an unconfirmed proposal.
2. ``confirm`` requires the exact canonical proposal content hash and changes
   only ``confirmed: false`` to ``confirmed: true``.

Mirrored proposals are not accepted by this flow. They require a separate,
explicitly reviewed procedure through the legacy ``create --mirror-approved``
path so mirror approval can never be added during confirmation.
"""
from __future__ import annotations

import argparse
import copy
import hmac
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
_SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
_INSPECTION_SCHEMA = "jozz.scan-dataset-inspection"
_MIN_INSPECTION_SCHEMA_VERSION = 3
_ORIGIN_POLICY = "GLOBAL_BOUNDS_CENTER"
_ORIGIN_POLICY_CLI = "global-bounds-center"


class SourceFrameCliError(ValueError):
    pass


def _load_scan_frames() -> Any:
    path = MODULE_DIR / "scan_frames.py"
    spec = importlib.util.spec_from_file_location(
        "_jozz_source_frame_cli_frames", path
    )
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
        raise SourceFrameCliError(f"{path} must contain a JSON object")
    return value


def load_private_inspection_json(path: Path) -> dict[str, Any]:
    """Load inspection data without echoing its private local path on failure."""
    try:
        return load_json_strict(path)
    except SourceFrameCliError as exc:
        text = str(exc)
        if text.startswith("duplicate JSON key:") or text.startswith(
            "non-standard JSON constant:"
        ):
            raise
        raise SourceFrameCliError("cannot read private inspection JSON") from exc


def _axis_vector(label: str, field: str) -> tuple[int, int, int]:
    match = _AXIS_PATTERN.fullmatch(label)
    if not match:
        raise SourceFrameCliError(
            f"{field} must be a signed axis such as +X or -Z"
        )
    sign = 1 if match.group(1) == "+" else -1
    index = {"X": 0, "Y": 1, "Z": 2}[match.group(2)]
    result = [0, 0, 0]
    result[index] = sign
    return tuple(result)


def _basis(axis_roles: Mapping[str, str], label: str) -> list[list[int]]:
    vectors = [
        _axis_vector(str(axis_roles.get(role, "")), f"{label}.{role}")
        for role in _AXIS_ROLES
    ]
    coordinates = [
        next(i for i, value in enumerate(vector) if value) for vector in vectors
    ]
    if len(set(coordinates)) != 3:
        raise SourceFrameCliError(
            f"{label} must assign three distinct coordinate axes"
        )
    return [
        [vectors[column][row] for column in range(3)] for row in range(3)
    ]


def _transpose(matrix: Sequence[Sequence[int]]) -> list[list[int]]:
    return [
        [int(matrix[row][column]) for row in range(3)] for column in range(3)
    ]


def _matmul(
    a: Sequence[Sequence[int]], b: Sequence[Sequence[int]]
) -> list[list[int]]:
    return [
        [
            sum(
                int(a[row][k]) * int(b[k][column]) for k in range(3)
            )
            for column in range(3)
        ]
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
    return [0.0 if value == -0.0 else value for value in result]


def contract_sha256(contract: dict[str, Any]) -> str:
    return _load_scan_frames().contract_sha256(contract)


def _sha256(value: str, field: str) -> str:
    if not isinstance(value, str) or not _SHA256_PATTERN.fullmatch(value):
        raise SourceFrameCliError(f"{field} must be lowercase SHA-256")
    return value


def _finite_vector3(value: Any, field: str) -> list[float]:
    if not isinstance(value, list) or len(value) != 3:
        raise SourceFrameCliError(f"{field} must contain exactly three values")
    result: list[float] = []
    for index, raw in enumerate(value):
        if isinstance(raw, bool):
            raise SourceFrameCliError(f"{field}[{index}] must be numeric")
        try:
            number = float(raw)
        except (TypeError, ValueError) as exc:
            raise SourceFrameCliError(
                f"{field}[{index}] must be numeric"
            ) from exc
        if not math.isfinite(number):
            raise SourceFrameCliError(f"{field}[{index}] must be finite")
        result.append(0.0 if number == -0.0 else number)
    return result


def _inspection_global_bounds_center(inspection: dict[str, Any]) -> list[float]:
    if inspection.get("schema") != _INSPECTION_SCHEMA:
        raise SourceFrameCliError("unexpected inspection report schema")
    version = inspection.get("schemaVersion")
    if isinstance(version, bool) or not isinstance(version, int):
        raise SourceFrameCliError("inspection schemaVersion must be an integer")
    if version < _MIN_INSPECTION_SCHEMA_VERSION:
        raise SourceFrameCliError("inspection report schema is too old")
    if inspection.get("datasetStatus") not in {
        "compatible",
        "compatible-review",
    }:
        raise SourceFrameCliError(
            "inspection must be compatible or compatible-review"
        )
    automatic = inspection.get("automaticEvidenceGate")
    if not isinstance(automatic, dict) or automatic.get("passed") is not True:
        raise SourceFrameCliError(
            "inspection automatic evidence gate must pass before frame proposal"
        )

    bounds = inspection.get("globalBounds")
    if not isinstance(bounds, dict) or set(bounds) != {"min", "max"}:
        raise SourceFrameCliError(
            "inspection globalBounds must contain exactly min and max"
        )
    minimum = _finite_vector3(bounds["min"], "globalBounds.min")
    maximum = _finite_vector3(bounds["max"], "globalBounds.max")
    center: list[float] = []
    for axis in range(3):
        if minimum[axis] > maximum[axis]:
            raise SourceFrameCliError(
                f"globalBounds are reversed on axis {axis}"
            )
        value = minimum[axis] + 0.5 * (maximum[axis] - minimum[axis])
        if not math.isfinite(value):
            raise SourceFrameCliError("globalBounds center is not finite")
        center.append(0.0 if value == -0.0 else value)
    return center


def _require_canonical_contract(
    document: dict[str, Any], *, label: str
) -> dict[str, Any]:
    normalized = _load_scan_frames().validate_frame_contract(document)
    if document != normalized:
        raise SourceFrameCliError(
            f"{label} must be an exact canonical source-frame document"
        )
    return normalized


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
        raise SourceFrameCliError(
            "source units per meter must be finite and positive"
        )

    source_basis = _basis(source_axis_roles, "source axis roles")
    lab_basis = _basis(lab_axis_roles, "lab axis roles")
    matrix = _matmul(lab_basis, _transpose(source_basis))
    determinant = _determinant(matrix)
    orientation = "preserve" if determinant == 1 else "mirror"
    if orientation == "mirror" and not mirror_approved:
        raise SourceFrameCliError(
            "derived transform mirrors orientation; rerun only after review "
            "with --mirror-approved"
        )

    contract = {
        "schema": "jozz.scan-source-frame",
        "schemaVersion": 1,
        "confirmed": bool(confirmed),
        "sourceFrame": {
            "unitsPerMeter": units,
            "handedness": _handedness(source_basis),
            "axisRoles": {
                role: str(source_axis_roles[role]) for role in _AXIS_ROLES
            },
        },
        "labFrame": {
            "handedness": _handedness(lab_basis),
            "axisRoles": {
                role: str(lab_axis_roles[role]) for role in _AXIS_ROLES
            },
        },
        "sourceToLab": {
            "axisMatrix": matrix,
            "orientationChange": orientation,
            "mirrorApproved": bool(mirror_approved),
            "localOriginSource": _finite_origin(local_origin_source),
        },
    }
    return _load_scan_frames().validate_frame_contract(contract)


def propose_contract_from_inspection(
    *,
    inspection: dict[str, Any],
    source_units_per_meter: float,
    source_axis_roles: Mapping[str, str],
    lab_axis_roles: Mapping[str, str],
) -> dict[str, Any]:
    """Build one unconfirmed, non-mirrored proposal from private report bounds."""
    origin = _inspection_global_bounds_center(inspection)
    contract = build_contract(
        source_units_per_meter=source_units_per_meter,
        source_axis_roles=source_axis_roles,
        lab_axis_roles=lab_axis_roles,
        local_origin_source=origin,
        confirmed=False,
        mirror_approved=False,
    )
    transform = contract["sourceToLab"]
    if (
        transform["orientationChange"] != "preserve"
        or transform["mirrorApproved"] is not False
        or transform["determinant"] != 1
    ):
        raise SourceFrameCliError(
            "owner-safe proposal flow accepts only non-mirrored transforms"
        )
    return _require_canonical_contract(contract, label="proposal")


def confirm_contract(
    proposal: dict[str, Any], *, expected_sha256: str
) -> dict[str, Any]:
    """Confirm the exact proposal while changing only its confirmed flag."""
    expected = _sha256(expected_sha256, "expectedSha256")
    normalized = _require_canonical_contract(proposal, label="proposal")
    if normalized["confirmed"] is not False:
        raise SourceFrameCliError("proposal is already confirmed")
    transform = normalized["sourceToLab"]
    if (
        transform["orientationChange"] != "preserve"
        or transform["mirrorApproved"] is not False
        or transform["determinant"] != 1
    ):
        raise SourceFrameCliError(
            "owner-safe confirmation refuses mirrored proposals"
        )
    observed = contract_sha256(normalized)
    if not hmac.compare_digest(observed, expected):
        raise SourceFrameCliError("proposal SHA-256 does not match expected value")

    confirmed = copy.deepcopy(normalized)
    confirmed["confirmed"] = True
    confirmed = _load_scan_frames().validate_frame_contract(confirmed)

    comparison = copy.deepcopy(confirmed)
    comparison["confirmed"] = False
    if comparison != normalized:
        raise SourceFrameCliError(
            "confirmation attempted to change fields other than confirmed"
        )
    return _require_canonical_contract(confirmed, label="confirmed contract")


def write_contract(
    path: Path, contract: dict[str, Any], *, force: bool
) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not force:
        raise SourceFrameCliError(f"refusing to overwrite existing file: {path}")
    payload = (
        json.dumps(
            contract,
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
            allow_nan=False,
        )
        + "\n"
    ).encode("utf-8")
    temporary = path.with_name(
        f".{path.name}.tmp-{os.getpid()}-{uuid.uuid4().hex}"
    )
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
        f"determinant={transform['determinant']} "
        f"orientation={transform['orientationChange']}"
    )
    if not contract["confirmed"]:
        print(
            "scan_source_frame_contract: REVIEW_REQUIRED | "
            "contract remains unconfirmed"
        )
    return 0


def _propose_from_inspection(args: argparse.Namespace) -> int:
    if args.origin_policy != _ORIGIN_POLICY_CLI:
        raise SourceFrameCliError("unsupported origin policy")
    inspection = load_private_inspection_json(args.inspection)
    contract = propose_contract_from_inspection(
        inspection=inspection,
        source_units_per_meter=args.source_units_per_meter,
        source_axis_roles=_axis_roles(args, "source"),
        lab_axis_roles=_axis_roles(args, "lab"),
    )
    write_contract(args.output, contract, force=False)
    transform = contract["sourceToLab"]
    proposal_hash = contract_sha256(contract)
    print(
        "scan_source_frame_contract: PROPOSAL_READY | "
        "confirmed=false "
        f"origin_policy={_ORIGIN_POLICY} "
        f"units_per_meter={contract['sourceFrame']['unitsPerMeter']} "
        f"determinant={transform['determinant']} "
        f"orientation={transform['orientationChange']} "
        f"proposal_sha256={proposal_hash}"
    )
    print(
        "scan_source_frame_contract: REVIEW_REQUIRED | "
        "review units, semantic axes, orientation and proposal SHA-256; "
        "private origin values and local paths were not printed"
    )
    return 0


def _confirm(args: argparse.Namespace) -> int:
    proposal = load_json_strict(args.proposal)
    confirmed = confirm_contract(
        proposal,
        expected_sha256=args.expected_sha256,
    )
    write_contract(args.output, confirmed, force=False)
    transform = confirmed["sourceToLab"]
    print(
        "scan_source_frame_contract: CONFIRMED | "
        "confirmed=true "
        f"proposal_sha256={args.expected_sha256} "
        f"confirmed_sha256={contract_sha256(confirmed)} "
        f"determinant={transform['determinant']} "
        f"orientation={transform['orientationChange']}"
    )
    return 0


def _validate(args: argparse.Namespace) -> int:
    contract = _load_scan_frames().validate_frame_contract(
        load_json_strict(args.contract)
    )
    if args.require_confirmed and not contract["confirmed"]:
        raise SourceFrameCliError(
            "source-frame contract is valid but not owner-confirmed"
        )
    transform = contract["sourceToLab"]
    print(
        "scan_source_frame_contract: OK | "
        f"path={args.contract} confirmed={str(contract['confirmed']).lower()} "
        f"determinant={transform['determinant']} "
        f"orientation={transform['orientationChange']}"
    )
    return 0


def _add_axis_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--source-units-per-meter", required=True, type=float)
    parser.add_argument("--source-right", required=True)
    parser.add_argument("--source-forward", required=True)
    parser.add_argument("--source-up", required=True)
    parser.add_argument("--lab-right", default="+X")
    parser.add_argument("--lab-forward", default="-Z")
    parser.add_argument("--lab-up", default="+Y")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser(
        "create", help="derive and write one frame contract"
    )
    _add_axis_arguments(create)
    create.add_argument("--output", required=True, type=Path)
    create.add_argument(
        "--local-origin-source",
        required=True,
        nargs=3,
        type=float,
        metavar=("X", "Y", "Z"),
    )
    create.add_argument("--confirmed", action="store_true")
    create.add_argument("--mirror-approved", action="store_true")
    create.add_argument("--force", action="store_true")
    create.set_defaults(handler=_create)

    propose = subparsers.add_parser(
        "propose-from-inspection",
        help="derive a private unconfirmed frame proposal without printing origin",
    )
    _add_axis_arguments(propose)
    propose.add_argument("--inspection", required=True, type=Path)
    propose.add_argument("--output", required=True, type=Path)
    propose.add_argument(
        "--origin-policy",
        required=True,
        choices=[_ORIGIN_POLICY_CLI],
    )
    propose.set_defaults(handler=_propose_from_inspection)

    confirm = subparsers.add_parser(
        "confirm",
        help="confirm one exact non-mirrored proposal by SHA-256",
    )
    confirm.add_argument("--proposal", required=True, type=Path)
    confirm.add_argument("--expected-sha256", required=True)
    confirm.add_argument("--output", required=True, type=Path)
    confirm.set_defaults(handler=_confirm)

    validate = subparsers.add_parser(
        "validate", help="validate an existing frame contract"
    )
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
