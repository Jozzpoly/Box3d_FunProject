#!/usr/bin/env python3
"""Validate and apply explicit source-to-lab coordinate-frame contracts.

The contract is deliberately independent from renderer, Box3D and UI types. It
records units, semantic axis roles and one signed-permutation matrix. The matrix
convention is::

    lab_meters = M * ((source_point - local_origin_source) / units_per_meter)

A mirror transform is rejected unless it is declared and explicitly approved.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
from typing import Any, Iterable, Sequence

SCHEMA = "jozz.scan-source-frame"
SCHEMA_VERSION = 1
AXIS_ROLE_NAMES = ("right", "forward", "up")
_AXIS_PATTERN = re.compile(r"^([+-])([XYZ])$")


class FrameContractError(ValueError):
    """Raised when a frame contract is incomplete or internally inconsistent."""


def _rounded(value: float) -> float:
    result = round(float(value), 12)
    return 0.0 if result == -0.0 else result


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")


def contract_sha256(contract: dict[str, Any]) -> str:
    normalized = validate_frame_contract(contract)
    return hashlib.sha256(canonical_json_bytes(normalized)).hexdigest()


def _finite_vector3(value: Any, label: str) -> list[float]:
    if not isinstance(value, list) or len(value) != 3:
        raise FrameContractError(f"{label} must be a three-value array")
    result = [float(component) for component in value]
    if not all(math.isfinite(component) for component in result):
        raise FrameContractError(f"{label} contains non-finite values")
    return [_rounded(component) for component in result]


def _axis_vector(label: Any, field: str) -> tuple[int, int, int]:
    if not isinstance(label, str):
        raise FrameContractError(f"{field} must be a signed axis label such as +X or -Z")
    match = _AXIS_PATTERN.fullmatch(label)
    if not match:
        raise FrameContractError(f"{field} must be a signed axis label such as +X or -Z")
    sign = 1 if match.group(1) == "+" else -1
    index = {"X": 0, "Y": 1, "Z": 2}[match.group(2)]
    vector = [0, 0, 0]
    vector[index] = sign
    return tuple(vector)


def _determinant3(matrix: Sequence[Sequence[float]]) -> float:
    a, b, c = matrix
    return (
        a[0] * (b[1] * c[2] - b[2] * c[1])
        - a[1] * (b[0] * c[2] - b[2] * c[0])
        + a[2] * (b[0] * c[1] - b[1] * c[0])
    )


def _transpose(matrix: Sequence[Sequence[int]]) -> list[list[int]]:
    return [[int(matrix[row][column]) for row in range(3)] for column in range(3)]


def _matmul(a: Sequence[Sequence[int]], b: Sequence[Sequence[int]]) -> list[list[int]]:
    return [
        [sum(int(a[row][k]) * int(b[k][column]) for k in range(3)) for column in range(3)]
        for row in range(3)
    ]


def _matvec(matrix: Sequence[Sequence[int]], vector: Sequence[float]) -> list[float]:
    return [
        _rounded(sum(float(matrix[row][column]) * float(vector[column]) for column in range(3)))
        for row in range(3)
    ]


def _basis_from_roles(roles: dict[str, str], label: str) -> list[list[int]]:
    vectors = [_axis_vector(roles.get(role), f"{label}.axisRoles.{role}") for role in AXIS_ROLE_NAMES]
    absolute_axes = []
    for vector in vectors:
        absolute_axes.append(next(index for index, value in enumerate(vector) if value != 0))
    if len(set(absolute_axes)) != 3:
        raise FrameContractError(f"{label}.axisRoles must assign three distinct coordinate axes")
    # Columns are semantic basis vectors in the fixed order right, forward, up.
    return [[vectors[column][row] for column in range(3)] for row in range(3)]


def _validate_frame(value: Any, label: str, *, require_units: bool) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise FrameContractError(f"{label} must be an object")
    roles = value.get("axisRoles")
    if not isinstance(roles, dict):
        raise FrameContractError(f"{label}.axisRoles must be an object")
    normalized_roles = {role: str(roles.get(role, "")) for role in AXIS_ROLE_NAMES}
    basis = _basis_from_roles(normalized_roles, label)
    determinant = int(round(_determinant3(basis)))
    if determinant not in (-1, 1):
        raise FrameContractError(f"{label}.axisRoles do not form a valid basis")
    derived_handedness = "right" if determinant == 1 else "left"
    declared_handedness = value.get("handedness")
    if declared_handedness not in {"right", "left"}:
        raise FrameContractError(f"{label}.handedness must be right or left")
    if declared_handedness != derived_handedness:
        raise FrameContractError(
            f"{label}.handedness disagrees with axisRoles in right/forward/up basis order"
        )
    result: dict[str, Any] = {
        "handedness": declared_handedness,
        "axisRoles": normalized_roles,
    }
    if require_units:
        units = float(value.get("unitsPerMeter", 0.0))
        if not math.isfinite(units) or units <= 0:
            raise FrameContractError(f"{label}.unitsPerMeter must be finite and positive")
        result["unitsPerMeter"] = _rounded(units)
    return result


def _validate_axis_matrix(value: Any) -> list[list[int]]:
    if not isinstance(value, list) or len(value) != 3:
        raise FrameContractError("sourceToLab.axisMatrix must be a 3x3 array")
    matrix: list[list[int]] = []
    for row_index, row in enumerate(value):
        if not isinstance(row, list) or len(row) != 3:
            raise FrameContractError("sourceToLab.axisMatrix must be a 3x3 array")
        normalized_row: list[int] = []
        for column_index, component in enumerate(row):
            if isinstance(component, bool) or not isinstance(component, (int, float)):
                raise FrameContractError(
                    f"sourceToLab.axisMatrix[{row_index}][{column_index}] must be -1, 0 or 1"
                )
            integer = int(component)
            if float(component) != float(integer) or integer not in (-1, 0, 1):
                raise FrameContractError(
                    f"sourceToLab.axisMatrix[{row_index}][{column_index}] must be -1, 0 or 1"
                )
            normalized_row.append(integer)
        matrix.append(normalized_row)
    if any(sum(abs(value) for value in row) != 1 for row in matrix):
        raise FrameContractError("sourceToLab.axisMatrix must have one signed axis per row")
    if any(sum(abs(matrix[row][column]) for row in range(3)) != 1 for column in range(3)):
        raise FrameContractError("sourceToLab.axisMatrix must have one signed axis per column")
    determinant = int(round(_determinant3(matrix)))
    if determinant not in (-1, 1):
        raise FrameContractError("sourceToLab.axisMatrix must be invertible")
    return matrix


def validate_frame_contract(contract: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(contract, dict):
        raise FrameContractError("frame contract must be an object")
    if contract.get("schema") != SCHEMA or int(contract.get("schemaVersion", 0)) != SCHEMA_VERSION:
        raise FrameContractError("unknown source-frame schema or version")

    source = _validate_frame(contract.get("sourceFrame"), "sourceFrame", require_units=True)
    lab = _validate_frame(contract.get("labFrame"), "labFrame", require_units=False)
    transform = contract.get("sourceToLab")
    if not isinstance(transform, dict):
        raise FrameContractError("sourceToLab must be an object")
    matrix = _validate_axis_matrix(transform.get("axisMatrix"))
    local_origin = _finite_vector3(transform.get("localOriginSource"), "sourceToLab.localOriginSource")

    source_basis = _basis_from_roles(source["axisRoles"], "sourceFrame")
    lab_basis = _basis_from_roles(lab["axisRoles"], "labFrame")
    expected_matrix = _matmul(lab_basis, _transpose(source_basis))
    if matrix != expected_matrix:
        raise FrameContractError("sourceToLab.axisMatrix disagrees with declared semantic axis roles")

    determinant = int(round(_determinant3(matrix)))
    orientation_change = transform.get("orientationChange")
    expected_change = "preserve" if determinant == 1 else "mirror"
    if orientation_change != expected_change:
        raise FrameContractError(
            f"sourceToLab.orientationChange must be {expected_change} for this axisMatrix"
        )
    mirror_approved = transform.get("mirrorApproved") is True
    if determinant == -1 and not mirror_approved:
        raise FrameContractError("mirror source-to-lab transform requires mirrorApproved=true")

    confirmed = contract.get("confirmed") is True
    return {
        "schema": SCHEMA,
        "schemaVersion": SCHEMA_VERSION,
        "confirmed": confirmed,
        "sourceFrame": source,
        "labFrame": lab,
        "sourceToLab": {
            "axisMatrix": matrix,
            "determinant": determinant,
            "orientationChange": expected_change,
            "mirrorApproved": mirror_approved,
            "localOriginSource": local_origin,
        },
    }


def source_to_lab_point(contract: dict[str, Any], point_source: Sequence[float]) -> list[float]:
    normalized = validate_frame_contract(contract)
    point = _finite_vector3(list(point_source), "pointSource")
    origin = normalized["sourceToLab"]["localOriginSource"]
    units = float(normalized["sourceFrame"]["unitsPerMeter"])
    local_source_meters = [(point[index] - origin[index]) / units for index in range(3)]
    return _matvec(normalized["sourceToLab"]["axisMatrix"], local_source_meters)


def lab_to_source_point(contract: dict[str, Any], point_lab_meters: Sequence[float]) -> list[float]:
    normalized = validate_frame_contract(contract)
    point = _finite_vector3(list(point_lab_meters), "pointLabMeters")
    inverse = _transpose(normalized["sourceToLab"]["axisMatrix"])
    local_source_meters = _matvec(inverse, point)
    units = float(normalized["sourceFrame"]["unitsPerMeter"])
    origin = normalized["sourceToLab"]["localOriginSource"]
    return [_rounded(local_source_meters[index] * units + origin[index]) for index in range(3)]


def transform_points_source_to_lab(
    contract: dict[str, Any], points_source: Iterable[Sequence[float]]
) -> list[list[float]]:
    normalized = validate_frame_contract(contract)
    return [source_to_lab_point(normalized, point) for point in points_source]
