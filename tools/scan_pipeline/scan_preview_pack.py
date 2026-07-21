#!/usr/bin/env python3
"""Build and verify private render-only scan preview packs.

A preview pack is a disposable projection of one verified scan-import bundle.
It exists only to visualize source GLB geometry in the native sample host. It
is never accepted-world data and never contains collision data.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import math
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import sys
from typing import Any, Iterable, Sequence
import uuid

MODULE_DIR = Path(__file__).resolve().parent


def _load_sibling(name: str) -> Any:
    path = MODULE_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(f"_jozz_preview_{name}", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


scan_import_bundle = _load_sibling("scan_import_bundle")
scan_frames = _load_sibling("scan_frames")
scan_world_contracts = _load_sibling("scan_world_contracts")
scan_inspect = _load_sibling("scan_inspect")

SCHEMA = "jozz.scan-source-visual-preview-pack"
SCHEMA_VERSION = 1
PURPOSE = "SOURCE_VISUAL_PREVIEW_ONLY"
PRIVACY_CLASS = "PRIVATE_LOCAL_ONLY"
MAGIC = b"JSPREV1\0"
BINARY_VERSION = 1
HEADER = struct.Struct("<8sIIII")
VERTEX = struct.Struct("<ffffff")
INDEX = struct.Struct("<I")
MAX_TILES = 64
MAX_VERTICES_PER_TILE = 25_000_000
MAX_INDICES_PER_TILE = 75_000_000
MAX_SOURCE_GLB_BYTES = 2 * 1024 * 1024 * 1024
MAX_TOTAL_BINARY_BYTES = 8 * 1024 * 1024 * 1024

_CAPABILITIES = {
    "sourceGeometryVisible": True,
    "texturesIncluded": False,
    "internalGeometryCorrespondencePassed": False,
    "acceptedWorld": False,
    "collisionReady": False,
}
_TILE_FORMAT = {
    "magic": MAGIC.rstrip(b"\0").decode("ascii"),
    "version": BINARY_VERSION,
    "vertexLayout": "float32 position.xyz + float32 normal.xyz",
    "indexLayout": "uint32 triangle-list",
}
_MANIFEST_KEYS = {
    "schema",
    "schemaVersion",
    "status",
    "privacyClass",
    "purpose",
    "sourceBundleContentSha256",
    "packageId",
    "sourceRevisionId",
    "sourceFrameContractSha256",
    "capabilities",
    "tileFormat",
    "tileCount",
    "globalBoundsLabMeters",
    "tiles",
    "previewContentSha256",
}
_TILE_KEYS = {
    "tileId",
    "vertexCount",
    "indexCount",
    "triangleCount",
    "boundsLabMeters",
    "path",
    "byteLength",
    "sha256",
}


class PreviewPackError(ValueError):
    pass


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _canonical_json_bytes(value: Any) -> bytes:
    return scan_import_bundle.canonical_json_bytes(value)


def _strict_json(path: Path) -> dict[str, Any]:
    return scan_import_bundle.load_json_strict(path)


def _exact_keys(value: dict[str, Any], expected: set[str], label: str) -> None:
    actual = set(value)
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        raise PreviewPackError(f"{label} keys mismatch; missing={missing} extra={extra}")


def _sha(value: Any, label: str) -> str:
    try:
        return scan_import_bundle._sha(value, label)
    except scan_import_bundle.ImportBundleError as exc:
        raise PreviewPackError(str(exc)) from exc


def _safe_label(value: Any) -> str:
    try:
        return scan_import_bundle._label(value, "previewLabel")
    except scan_import_bundle.ImportBundleError as exc:
        raise PreviewPackError(str(exc)) from exc


def _finite(value: Any, label: str) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError) as exc:
        raise PreviewPackError(f"{label} must be numeric") from exc
    if not math.isfinite(result):
        raise PreviewPackError(f"{label} must be finite")
    result = round(result, 9)
    return 0.0 if result == -0.0 else result


def _uint(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise PreviewPackError(f"{label} must be a non-negative integer")
    return value


def _revision(value: Any) -> str:
    if not isinstance(value, str) or not value.startswith("sha256:"):
        raise PreviewPackError("sourceRevisionId must be sha256:<hex>")
    _sha(value[7:], "sourceRevisionId")
    return value


def _axis_vector(value: str) -> tuple[float, float, float]:
    sign = -1.0 if value.startswith("-") else 1.0
    axis = value[-1:]
    if axis == "X":
        return sign, 0.0, 0.0
    if axis == "Y":
        return 0.0, sign, 0.0
    if axis == "Z":
        return 0.0, 0.0, sign
    raise PreviewPackError(f"invalid signed axis {value!r}")


def _mat_vec(
    matrix: Sequence[Sequence[Any]], value: Sequence[float]
) -> tuple[float, float, float]:
    if len(matrix) != 3 or any(len(row) != 3 for row in matrix):
        raise PreviewPackError("sourceToLab.axisMatrix must be 3x3")
    return tuple(
        sum(
            _finite(matrix[row][column], f"axisMatrix[{row}][{column}]")
            * value[column]
            for column in range(3)
        )
        for row in range(3)
    )  # type: ignore[return-value]


def _source_to_lab(
    frame: dict[str, Any], point: Sequence[float]
) -> tuple[float, float, float]:
    units_per_meter = _finite(
        frame["sourceFrame"]["unitsPerMeter"], "sourceFrame.unitsPerMeter"
    )
    if units_per_meter <= 0.0:
        raise PreviewPackError("sourceFrame.unitsPerMeter must be positive")
    origin = frame["sourceToLab"]["localOriginSource"]
    local = tuple(
        (
            _finite(point[index], f"point[{index}]")
            - _finite(origin[index], f"origin[{index}]")
        )
        / units_per_meter
        for index in range(3)
    )
    return _mat_vec(frame["sourceToLab"]["axisMatrix"], local)


def _cross(a: Sequence[float], b: Sequence[float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _sub(a: Sequence[float], b: Sequence[float]) -> tuple[float, float, float]:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def _normalise(
    value: Sequence[float], fallback: Sequence[float]
) -> tuple[float, float, float]:
    length = math.sqrt(sum(component * component for component in value))
    if length <= 1.0e-20:
        return float(fallback[0]), float(fallback[1]), float(fallback[2])
    return value[0] / length, value[1] / length, value[2] / length


def _bounds(points: Iterable[Sequence[float]]) -> dict[str, list[float]]:
    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    count = 0
    for point in points:
        count += 1
        for axis in range(3):
            value = _finite(point[axis], f"bounds.point[{axis}]")
            minimum[axis] = min(minimum[axis], value)
            maximum[axis] = max(maximum[axis], value)
    if count == 0:
        raise PreviewPackError("geometry contains no vertices")
    return {
        "min": [_finite(value, "bounds.min") for value in minimum],
        "max": [_finite(value, "bounds.max") for value in maximum],
    }


def _validate_bounds(value: Any, label: str) -> dict[str, list[float]]:
    if not isinstance(value, dict):
        raise PreviewPackError(f"{label} must be an object")
    _exact_keys(value, {"min", "max"}, label)
    result: dict[str, list[float]] = {}
    for side in ("min", "max"):
        raw = value[side]
        if not isinstance(raw, list) or len(raw) != 3:
            raise PreviewPackError(f"{label}.{side} must contain three values")
        result[side] = [
            _finite(component, f"{label}.{side}[{axis}]")
            for axis, component in enumerate(raw)
        ]
    if any(result["min"][axis] > result["max"][axis] for axis in range(3)):
        raise PreviewPackError(f"{label} min exceeds max")
    return result


def _merge_bounds(
    items: Sequence[dict[str, list[float]]],
) -> dict[str, list[float]]:
    if not items:
        raise PreviewPackError("preview pack requires at least one tile")
    return {
        "min": [min(item["min"][axis] for item in items) for axis in range(3)],
        "max": [max(item["max"][axis] for item in items) for axis in range(3)],
    }


def _extract_geometry(
    glb: bytes, tile_id: int, frame: dict[str, Any]
) -> tuple[bytes, dict[str, Any]]:
    if not 0 <= tile_id <= 0xFFFFFFFF:
        raise PreviewPackError("tile id exceeds binary format")
    document, binary = scan_inspect.parse_glb(glb, f"MipTile_{tile_id}.glb")
    meshes = document.get("meshes", [])
    accessors = document.get("accessors", [])
    positions: list[tuple[float, float, float]] = []
    indices: list[int] = []
    mirror = frame["sourceToLab"]["orientationChange"] == "mirror"

    for node_index, world in scan_inspect.world_nodes(document):
        node = document.get("nodes", [])[node_index]
        if "mesh" not in node:
            continue
        mesh = scan_inspect.checked(meshes, int(node["mesh"]), "mesh")
        for primitive in mesh.get("primitives", []):
            if int(primitive.get("mode", 4)) != 4:
                raise PreviewPackError(
                    f"tile {tile_id}: only TRIANGLES primitives are supported"
                )
            attributes = primitive.get("attributes", {})
            if "POSITION" not in attributes:
                raise PreviewPackError(f"tile {tile_id}: primitive has no POSITION")
            position_index = int(attributes["POSITION"])
            position_accessor = scan_inspect.checked(
                accessors, position_index, "POSITION accessor"
            )
            if (
                position_accessor.get("componentType") != 5126
                or position_accessor.get("type") != "VEC3"
            ):
                raise PreviewPackError(
                    f"tile {tile_id}: POSITION must be float32 VEC3"
                )
            source_positions = list(
                scan_inspect.accessor_values(document, binary, position_index)
            )
            if len(positions) + len(source_positions) > MAX_VERTICES_PER_TILE:
                raise PreviewPackError(f"tile {tile_id}: vertex limit exceeded")
            base = len(positions)
            for point in source_positions:
                source_world = scan_inspect.transform_point(world, point)
                positions.append(_source_to_lab(frame, source_world))

            if "indices" in primitive:
                index_accessor_index = int(primitive["indices"])
                index_accessor = scan_inspect.checked(
                    accessors, index_accessor_index, "index accessor"
                )
                if (
                    index_accessor.get("type") != "SCALAR"
                    or index_accessor.get("componentType") not in (5121, 5123, 5125)
                ):
                    raise PreviewPackError(
                        f"tile {tile_id}: indices must be unsigned SCALAR"
                    )
                source_indices = [
                    int(value[0])
                    for value in scan_inspect.accessor_values(
                        document, binary, index_accessor_index
                    )
                ]
            else:
                source_indices = list(range(len(source_positions)))
            if len(source_indices) % 3 != 0:
                raise PreviewPackError(
                    f"tile {tile_id}: triangle index count is not divisible by three"
                )
            if len(indices) + len(source_indices) > MAX_INDICES_PER_TILE:
                raise PreviewPackError(f"tile {tile_id}: index limit exceeded")
            for offset in range(0, len(source_indices), 3):
                triangle = source_indices[offset : offset + 3]
                if any(value < 0 or value >= len(source_positions) for value in triangle):
                    raise PreviewPackError(f"tile {tile_id}: index out of range")
                if mirror:
                    triangle[1], triangle[2] = triangle[2], triangle[1]
                indices.extend(base + value for value in triangle)

    if not positions or not indices:
        raise PreviewPackError(f"tile {tile_id}: no renderable triangle geometry")

    fallback_up = _axis_vector(frame["labFrame"]["axisRoles"]["up"])
    accumulated = [[0.0, 0.0, 0.0] for _ in positions]
    for offset in range(0, len(indices), 3):
        ia, ib, ic = indices[offset : offset + 3]
        normal = _cross(
            _sub(positions[ib], positions[ia]),
            _sub(positions[ic], positions[ia]),
        )
        if sum(component * component for component in normal) <= 1.0e-20:
            continue
        for index in (ia, ib, ic):
            accumulated[index][0] += normal[0]
            accumulated[index][1] += normal[1]
            accumulated[index][2] += normal[2]
    normals = [_normalise(value, fallback_up) for value in accumulated]

    payload = bytearray(
        HEADER.pack(MAGIC, BINARY_VERSION, tile_id, len(positions), len(indices))
    )
    for position, normal in zip(positions, normals):
        payload.extend(VERTEX.pack(*position, *normal))
    for index in indices:
        payload.extend(INDEX.pack(index))
    return bytes(payload), {
        "tileId": tile_id,
        "vertexCount": len(positions),
        "indexCount": len(indices),
        "triangleCount": len(indices) // 3,
        "boundsLabMeters": _bounds(positions),
    }


def _read_tile(
    data: bytes, expected_tile_id: int | None = None
) -> dict[str, Any]:
    if len(data) < HEADER.size:
        raise PreviewPackError("preview tile is truncated")
    magic, version, tile_id, vertex_count, index_count = HEADER.unpack_from(data)
    if magic != MAGIC or version != BINARY_VERSION:
        raise PreviewPackError("preview tile magic or version mismatch")
    if expected_tile_id is not None and tile_id != expected_tile_id:
        raise PreviewPackError("preview tile id mismatch")
    if vertex_count == 0 or index_count == 0 or index_count % 3 != 0:
        raise PreviewPackError("preview tile contains invalid counts")
    if (
        vertex_count > MAX_VERTICES_PER_TILE
        or index_count > MAX_INDICES_PER_TILE
    ):
        raise PreviewPackError("preview tile exceeds geometry limits")
    expected_size = (
        HEADER.size + vertex_count * VERTEX.size + index_count * INDEX.size
    )
    if len(data) != expected_size:
        raise PreviewPackError("preview tile byte length does not match header")

    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    offset = HEADER.size
    for _ in range(vertex_count):
        values = VERTEX.unpack_from(data, offset)
        offset += VERTEX.size
        if not all(math.isfinite(value) for value in values):
            raise PreviewPackError("preview tile contains non-finite vertex data")
        normal_length = math.sqrt(sum(value * value for value in values[3:6]))
        if abs(normal_length - 1.0) > 1.0e-3:
            raise PreviewPackError("preview tile contains non-unit normal")
        for axis in range(3):
            minimum[axis] = min(minimum[axis], values[axis])
            maximum[axis] = max(maximum[axis], values[axis])
    for _ in range(index_count):
        (index,) = INDEX.unpack_from(data, offset)
        offset += INDEX.size
        if index >= vertex_count:
            raise PreviewPackError("preview tile contains out-of-range index")
    return {
        "tileId": tile_id,
        "vertexCount": vertex_count,
        "indexCount": index_count,
        "triangleCount": index_count // 3,
        "boundsLabMeters": {
            "min": [_finite(value, "tile.bounds.min") for value in minimum],
            "max": [_finite(value, "tile.bounds.max") for value in maximum],
        },
    }


def _manifest_core(
    *,
    bundle_summary: dict[str, Any],
    source_package: dict[str, Any],
    tile_records: Sequence[dict[str, Any]],
) -> dict[str, Any]:
    return {
        "schema": SCHEMA,
        "schemaVersion": SCHEMA_VERSION,
        "status": "COMPLETE",
        "privacyClass": PRIVACY_CLASS,
        "purpose": PURPOSE,
        "sourceBundleContentSha256": bundle_summary["bundleContentSha256"],
        "packageId": source_package["packageId"],
        "sourceRevisionId": source_package["revisionId"],
        "sourceFrameContractSha256": source_package[
            "sourceFrameContractSha256"
        ],
        "capabilities": dict(_CAPABILITIES),
        "tileFormat": dict(_TILE_FORMAT),
        "tileCount": len(tile_records),
        "globalBoundsLabMeters": _merge_bounds(
            [record["boundsLabMeters"] for record in tile_records]
        ),
        "tiles": list(tile_records),
    }


def _validate_manifest(manifest: dict[str, Any]) -> dict[str, Any]:
    _exact_keys(manifest, _MANIFEST_KEYS, "preview manifest")
    if (
        manifest["schema"] != SCHEMA
        or int(manifest["schemaVersion"]) != SCHEMA_VERSION
        or manifest["status"] != "COMPLETE"
        or manifest["privacyClass"] != PRIVACY_CLASS
        or manifest["purpose"] != PURPOSE
    ):
        raise PreviewPackError(
            "invalid preview manifest schema, status, privacy or purpose"
        )
    _sha(manifest["sourceBundleContentSha256"], "sourceBundleContentSha256")
    try:
        scan_world_contracts._stable_id(manifest["packageId"], "packageId")
    except scan_world_contracts.WorldContractError as exc:
        raise PreviewPackError(str(exc)) from exc
    _revision(manifest["sourceRevisionId"])
    _sha(manifest["sourceFrameContractSha256"], "sourceFrameContractSha256")
    if manifest["capabilities"] != _CAPABILITIES:
        raise PreviewPackError("preview capability boundary mismatch")
    if manifest["tileFormat"] != _TILE_FORMAT:
        raise PreviewPackError("preview tile format mismatch")

    expected_hash = _sha(manifest["previewContentSha256"], "previewContentSha256")
    unsigned = dict(manifest)
    unsigned.pop("previewContentSha256")
    if _sha256_bytes(_canonical_json_bytes(unsigned)) != expected_hash:
        raise PreviewPackError("previewContentSha256 mismatch")

    tiles = manifest["tiles"]
    if not isinstance(tiles, list) or not tiles or len(tiles) > MAX_TILES:
        raise PreviewPackError("preview tiles array is invalid")
    observed: list[int] = []
    paths: list[str] = []
    total_bytes = 0
    normalized_bounds: list[dict[str, list[float]]] = []
    for record in tiles:
        if not isinstance(record, dict):
            raise PreviewPackError("preview tile record must be an object")
        _exact_keys(record, _TILE_KEYS, "preview tile record")
        tile_id = _uint(record["tileId"], "tile.tileId")
        if tile_id > 0xFFFFFFFF:
            raise PreviewPackError("tile id exceeds binary format")
        observed.append(tile_id)
        expected_path = f"tiles/tile_{tile_id:03d}.bin"
        if record["path"] != expected_path:
            raise PreviewPackError(
                f"non-canonical preview tile path for {tile_id}"
            )
        paths.append(expected_path)
        vertex_count = _uint(record["vertexCount"], f"tile[{tile_id}].vertexCount")
        index_count = _uint(record["indexCount"], f"tile[{tile_id}].indexCount")
        triangle_count = _uint(
            record["triangleCount"], f"tile[{tile_id}].triangleCount"
        )
        if (
            vertex_count == 0
            or vertex_count > MAX_VERTICES_PER_TILE
            or index_count == 0
            or index_count > MAX_INDICES_PER_TILE
            or index_count != triangle_count * 3
        ):
            raise PreviewPackError(f"tile[{tile_id}] geometry counts are invalid")
        expected_length = (
            HEADER.size + vertex_count * VERTEX.size + index_count * INDEX.size
        )
        byte_length = _uint(record["byteLength"], f"tile[{tile_id}].byteLength")
        if byte_length != expected_length:
            raise PreviewPackError(f"tile[{tile_id}] byteLength mismatch")
        total_bytes += byte_length
        _sha(record["sha256"], f"tile[{tile_id}].sha256")
        normalized_bounds.append(
            _validate_bounds(record["boundsLabMeters"], f"tile[{tile_id}].bounds")
        )

    if observed != sorted(observed) or len(observed) != len(set(observed)):
        raise PreviewPackError("preview tile ids must be sorted and unique")
    if len(paths) != len(set(paths)) or total_bytes > MAX_TOTAL_BINARY_BYTES:
        raise PreviewPackError("preview paths or total byte budget are invalid")
    if manifest["tileCount"] != len(tiles):
        raise PreviewPackError("preview tileCount mismatch")
    expected_global = _merge_bounds(normalized_bounds)
    if _validate_bounds(manifest["globalBoundsLabMeters"], "globalBounds") != expected_global:
        raise PreviewPackError("preview global bounds mismatch")
    return manifest


def _files(root: Path) -> set[str]:
    result: set[str] = set()
    for path in root.rglob("*"):
        if path.is_symlink():
            raise PreviewPackError(f"preview pack contains symlink: {path}")
        if path.is_file():
            result.add(path.relative_to(root).as_posix())
    return result


def verify_preview_pack(root: Path) -> dict[str, Any]:
    root = Path(root)
    if not root.is_dir() or root.is_symlink():
        raise PreviewPackError("preview root must be a real directory")
    complete = root / "COMPLETE.json"
    if not complete.is_file() or complete.is_symlink():
        raise PreviewPackError("preview pack is incomplete")
    manifest = _validate_manifest(_strict_json(complete))
    expected = {record["path"] for record in manifest["tiles"]} | {
        "COMPLETE.json"
    }
    if _files(root) != expected:
        raise PreviewPackError("preview pack contains missing or unexpected files")

    total_bytes = 0
    for record in manifest["tiles"]:
        path = root / PurePosixPath(record["path"])
        if path.stat().st_size != record["byteLength"]:
            raise PreviewPackError(f"byteLength mismatch for {record['path']}")
        if _sha256_file(path) != record["sha256"]:
            raise PreviewPackError(f"SHA-256 mismatch for {record['path']}")
        parsed = _read_tile(path.read_bytes(), record["tileId"])
        for key in (
            "vertexCount",
            "indexCount",
            "triangleCount",
            "boundsLabMeters",
        ):
            if parsed[key] != record[key]:
                raise PreviewPackError(
                    f"tile metadata mismatch for {record['path']}: {key}"
                )
        total_bytes += record["byteLength"]
    if total_bytes > MAX_TOTAL_BINARY_BYTES:
        raise PreviewPackError("preview pack exceeds total byte budget")
    return {
        "previewContentSha256": manifest["previewContentSha256"],
        "tileCount": manifest["tileCount"],
        "sourceRevisionId": manifest["sourceRevisionId"],
        "path": str(root),
    }


def _write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())


def build_preview_pack(
    *,
    bundle: Path,
    source_root: Path,
    output_root: Path,
    label: str = "source-preview",
) -> Path:
    label = _safe_label(label)
    bundle = Path(bundle)
    source_root = Path(source_root)
    output_root = Path(output_root)
    bundle_summary = scan_import_bundle.verify_bundle(bundle)
    source_package = scan_world_contracts.validate_source_package(
        _strict_json(bundle / "private/source_package.json")
    )
    frame = scan_frames.validate_frame_contract(
        _strict_json(bundle / "private/source_frame.json")
    )
    if not frame["confirmed"]:
        raise PreviewPackError(
            "source frame must be owner-confirmed before visual preview"
        )
    if source_package["sourceFrameContract"] != frame:
        raise PreviewPackError("bundle frame differs from source package")
    if source_package["revisionId"] != bundle_summary["sourceRevisionId"]:
        raise PreviewPackError("bundle revision differs from source package")
    if not source_root.is_dir() or source_root.is_symlink():
        raise PreviewPackError("source root must be a real directory")
    tiles = source_package["tiles"]
    if not 0 < len(tiles) <= MAX_TILES:
        raise PreviewPackError("source package tile count is invalid")
    output_root.mkdir(parents=True, exist_ok=True)
    if output_root.is_symlink():
        raise PreviewPackError("output root must not be a symlink")

    staging = output_root / (
        f".{label}.staging-{os.getpid()}-{uuid.uuid4().hex}"
    )
    staging.mkdir()
    records: list[dict[str, Any]] = []
    total_bytes = 0
    try:
        for tile in tiles:
            tile_id = int(tile["tileId"])
            source = tile["glb"]
            source_path = source_root / source["sourceLabel"]
            source_bytes = int(source["byteLength"])
            if source_bytes <= 0 or source_bytes > MAX_SOURCE_GLB_BYTES:
                raise PreviewPackError(
                    f"source GLB byte budget invalid for tile {tile_id}"
                )
            if not source_path.is_file() or source_path.is_symlink():
                raise PreviewPackError(f"missing real GLB for tile {tile_id}")
            if source_path.stat().st_size != source_bytes:
                raise PreviewPackError(
                    f"source GLB byteLength mismatch for tile {tile_id}"
                )
            if _sha256_file(source_path) != source["sha256"]:
                raise PreviewPackError(
                    f"source GLB SHA-256 mismatch for tile {tile_id}"
                )
            payload, geometry = _extract_geometry(
                source_path.read_bytes(), tile_id, frame
            )
            total_bytes += len(payload)
            if total_bytes > MAX_TOTAL_BINARY_BYTES:
                raise PreviewPackError("preview pack exceeds total byte budget")
            relative = f"tiles/tile_{tile_id:03d}.bin"
            _write(staging / PurePosixPath(relative), payload)
            records.append(
                {
                    **geometry,
                    "path": relative,
                    "byteLength": len(payload),
                    "sha256": _sha256_bytes(payload),
                }
            )
        records.sort(key=lambda record: record["tileId"])
        core = _manifest_core(
            bundle_summary=bundle_summary,
            source_package=source_package,
            tile_records=records,
        )
        manifest = dict(core)
        manifest["previewContentSha256"] = _sha256_bytes(
            _canonical_json_bytes(core)
        )
        _validate_manifest(manifest)
        _write(staging / "COMPLETE.json", _canonical_json_bytes(manifest))
        final = output_root / (
            f"{label}-{manifest['previewContentSha256'][:16]}"
        )
        if final.exists():
            existing = verify_preview_pack(final)
            if (
                existing["previewContentSha256"]
                != manifest["previewContentSha256"]
            ):
                raise PreviewPackError(
                    "existing content-addressed preview is inconsistent"
                )
            shutil.rmtree(staging)
            return final
        os.replace(staging, final)
        verify_preview_pack(final)
        return final
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    build = subparsers.add_parser(
        "build", help="build one private render-only preview pack"
    )
    build.add_argument("--bundle", required=True, type=Path)
    build.add_argument("--source-root", required=True, type=Path)
    build.add_argument(
        "--output-root",
        type=Path,
        default=Path("build/scan_pipeline/previews"),
    )
    build.add_argument("--label", default="source-preview")
    verify = subparsers.add_parser(
        "verify", help="verify one existing preview pack"
    )
    verify.add_argument("preview", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "build":
            path = build_preview_pack(
                bundle=args.bundle,
                source_root=args.source_root,
                output_root=args.output_root,
                label=args.label,
            )
            summary = verify_preview_pack(path)
            print(
                "scan_preview_pack: PREVIEW_READY | "
                f"path={path} preview_sha256={summary['previewContentSha256']} "
                f"tiles={summary['tileCount']} revision={summary['sourceRevisionId']}"
            )
        else:
            summary = verify_preview_pack(args.preview)
            print(
                "scan_preview_pack: OK | "
                f"path={args.preview} preview_sha256={summary['previewContentSha256']} "
                f"tiles={summary['tileCount']} revision={summary['sourceRevisionId']}"
            )
        return 0
    except (
        OSError,
        PreviewPackError,
        scan_import_bundle.ImportBundleError,
        scan_frames.FrameContractError,
        scan_world_contracts.WorldContractError,
        scan_inspect.ScanInspectionError,
    ) as exc:
        print(f"scan_preview_pack: ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
