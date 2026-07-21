#!/usr/bin/env python3
"""Build and verify conservative private surface-evidence rasters from PLY tiles.

This is evidence, not ground truth. Empty cells remain UNKNOWN. The builder never
fills holes, smooths heights, classifies ground, creates collision, or mutates an
accepted world. It is a disposable derivative of one verified source revision.
"""
from __future__ import annotations

from array import array
import argparse
import hashlib
import importlib.util
import math
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import sys
from typing import Any, Iterator, Sequence
import uuid

MODULE_DIR = Path(__file__).resolve().parent


def _load_sibling(name: str) -> Any:
    path = MODULE_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(f"_jozz_surface_{name}", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


scan_import_bundle = _load_sibling("scan_import_bundle")
scan_world_contracts = _load_sibling("scan_world_contracts")
scan_frames = _load_sibling("scan_frames")
scan_owner_gate = _load_sibling("scan_owner_gate")
scan_preview_pack = _load_sibling("scan_preview_pack")
scan_ply = _load_sibling("scan_ply")

SCHEMA = "jozz.scan-surface-evidence-pack"
SCHEMA_VERSION = 1
PURPOSE = "SURFACE_EVIDENCE_ONLY"
PRIVACY_CLASS = "PRIVATE_LOCAL_ONLY"
MAGIC = b"JSSURF1\0"
BINARY_VERSION = 1
HEADER = struct.Struct("<8sIIIfff")
CELL = struct.Struct("<ffIQBBH")
CANONICAL_NAN_BITS = 0x7FC00000
CANONICAL_NAN = struct.unpack("<f", struct.pack("<I", CANONICAL_NAN_BITS))[0]
MAX_TILES = 64
MAX_CELLS = 8_000_000
MAX_BINARY_BYTES = HEADER.size + MAX_CELLS * CELL.size
UNKNOWN = 0
OBSERVED_SURFACE_EVIDENCE = 1
CLASSIFICATIONS = {
    UNKNOWN: "UNKNOWN",
    OBSERVED_SURFACE_EVIDENCE: "OBSERVED_SURFACE_EVIDENCE",
}
QUALITY_ALGORITHM = {
    "name": "support-and-spread-evidence-quality",
    "version": 1,
    "meaning": "relative evidence quality only; never ground confidence",
    "supportSaturation": 16,
    "sourceSaturation": 4,
    "verticalSpreadPenaltyCells": 4.0,
}
_CAPABILITIES = {
    "surfaceEvidenceAvailable": True,
    "holesPreserved": True,
    "groundClassified": False,
    "acceptedWorld": False,
    "collisionReady": False,
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
    "grid",
    "qualityAlgorithm",
    "tileBitOrdinals",
    "statistics",
    "surfacePath",
    "surfaceByteLength",
    "surfaceSha256",
    "surfaceEvidenceContentSha256",
}
_GRID_KEYS = {"originLabXZ", "cellSizeMeters", "width", "height", "upAxis"}
_STATS_KEYS = {
    "sourcePointCount",
    "observedCellCount",
    "unknownCellCount",
    "multiSourceCellCount",
    "maxSupportCount",
    "heightMinLabMeters",
    "heightMaxLabMeters",
}
_TILE_BIT_KEYS = {"bitOrdinal", "tileId"}


class SurfaceEvidenceError(ValueError):
    pass


def _canonical_json_bytes(value: Any) -> bytes:
    return scan_import_bundle.canonical_json_bytes(value)


def _strict_json(path: Path) -> dict[str, Any]:
    return scan_import_bundle.load_json_strict(path)


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(4 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _exact_keys(value: dict[str, Any], expected: set[str], label: str) -> None:
    actual = set(value)
    if actual != expected:
        raise SurfaceEvidenceError(
            f"{label} keys mismatch; missing={sorted(expected - actual)} "
            f"extra={sorted(actual - expected)}"
        )


def _uint(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise SurfaceEvidenceError(f"{label} must be a non-negative integer")
    return value


def _finite(value: Any, label: str) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError) as exc:
        raise SurfaceEvidenceError(f"{label} must be numeric") from exc
    if not math.isfinite(result):
        raise SurfaceEvidenceError(f"{label} must be finite")
    result = round(result, 9)
    return 0.0 if result == -0.0 else result


def _f32(value: Any, label: str) -> float:
    """Return the exact finite Python value represented by serialized float32."""
    number = _finite(value, label)
    try:
        result = struct.unpack("<f", struct.pack("<f", number))[0]
    except (OverflowError, struct.error) as exc:
        raise SurfaceEvidenceError(f"{label} exceeds float32 range") from exc
    if not math.isfinite(result):
        raise SurfaceEvidenceError(f"{label} is not finite after float32 conversion")
    return _finite(result, label)


def _sha(value: Any, label: str) -> str:
    try:
        return scan_import_bundle._sha(value, label)
    except scan_import_bundle.ImportBundleError as exc:
        raise SurfaceEvidenceError(str(exc)) from exc


def _revision(value: Any) -> str:
    if not isinstance(value, str) or not value.startswith("sha256:"):
        raise SurfaceEvidenceError("sourceRevisionId must be sha256:<hex>")
    _sha(value[7:], "sourceRevisionId")
    return value


def _file_identity(path: Path) -> tuple[int, int]:
    stat = path.stat()
    return int(stat.st_size), int(stat.st_mtime_ns)


def _quality(support: int, source_count: int, spread: float, cell_size: float) -> int:
    support_term = min(support, 16) / 16.0
    source_term = min(source_count, 4) / 4.0
    spread_term = min(
        max(spread / max(cell_size, 1.0e-9), 0.0),
        QUALITY_ALGORITHM["verticalSpreadPenaltyCells"],
    ) / QUALITY_ALGORITHM["verticalSpreadPenaltyCells"]
    value = round(
        255.0
        * (
            0.55 * support_term
            + 0.35 * source_term
            + 0.10 * (1.0 - spread_term)
        )
    )
    return max(1, min(255, int(value)))


def _iter_lab_points(
    path: Path,
    frame: dict[str, Any],
    *,
    chunk_vertices: int,
) -> Iterator[tuple[float, float, float]]:
    header = scan_ply.read_ply_header(path)
    for chunk in scan_ply.iter_vertex_chunks(
        path,
        header,
        chunk_vertices=chunk_vertices,
    ):
        for x, y, z in zip(chunk.x, chunk.y, chunk.z):
            point = scan_preview_pack._source_to_lab(
                frame,
                (float(x), float(y), float(z)),
            )
            if not all(math.isfinite(component) for component in point):
                raise SurfaceEvidenceError("PLY contains non-finite transformed point")
            yield point


def _safe_source_path(source_root: Path, source_label: Any, tile_id: int) -> Path:
    if not isinstance(source_label, str) or not source_label:
        raise SurfaceEvidenceError(f"invalid PLY sourceLabel for tile {tile_id}")
    logical = PurePosixPath(source_label)
    if logical.is_absolute() or len(logical.parts) != 1 or logical.name != source_label:
        raise SurfaceEvidenceError(f"unsafe PLY sourceLabel for tile {tile_id}")
    path = source_root / logical.name
    try:
        if path.resolve().parent != source_root.resolve():
            raise SurfaceEvidenceError(f"PLY path escapes source root for tile {tile_id}")
    except OSError as exc:
        raise SurfaceEvidenceError(f"cannot resolve PLY path for tile {tile_id}") from exc
    return path


def _source_files(
    source_package: dict[str, Any],
    source_root: Path,
) -> list[tuple[int, Path, dict[str, Any], tuple[int, int]]]:
    result: list[tuple[int, Path, dict[str, Any], tuple[int, int]]] = []
    for tile in source_package["tiles"]:
        tile_id = int(tile["tileId"])
        source = tile["ply"]
        path = _safe_source_path(source_root, source["sourceLabel"], tile_id)
        if not path.is_file() or path.is_symlink():
            raise SurfaceEvidenceError(f"missing real PLY for tile {tile_id}")
        expected_bytes = int(source["byteLength"])
        if expected_bytes <= 0 or path.stat().st_size != expected_bytes:
            raise SurfaceEvidenceError(
                f"source PLY byteLength mismatch for tile {tile_id}"
            )
        identity = _file_identity(path)
        if _sha256_file(path) != source["sha256"]:
            raise SurfaceEvidenceError(f"source PLY SHA-256 mismatch for tile {tile_id}")
        if _file_identity(path) != identity:
            raise SurfaceEvidenceError(f"source PLY changed while hashing: {path.name}")
        result.append((tile_id, path, source, identity))
    result.sort(key=lambda item: item[0])
    if not 0 < len(result) <= MAX_TILES:
        raise SurfaceEvidenceError("source package tile count is invalid")
    return result


def _write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())


def _build_binary(
    files: list[tuple[int, Path, dict[str, Any], tuple[int, int]]],
    frame: dict[str, Any],
    *,
    cell_size: float,
    chunk_vertices: int,
) -> tuple[bytes, dict[str, Any], list[dict[str, int]]]:
    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    point_count = 0
    for _, path, _, _ in files:
        for point in _iter_lab_points(
            path,
            frame,
            chunk_vertices=chunk_vertices,
        ):
            for axis in range(3):
                minimum[axis] = min(minimum[axis], point[axis])
                maximum[axis] = max(maximum[axis], point[axis])
            point_count += 1
    if point_count == 0:
        raise SurfaceEvidenceError("surface evidence requires at least one point")

    cell_size_f32 = _f32(cell_size, "cellSizeMeters")
    if cell_size_f32 <= 0.0:
        raise SurfaceEvidenceError("cellSizeMeters must remain positive as float32")
    origin_x = _f32(minimum[0], "origin.x")
    origin_z = _f32(minimum[2], "origin.z")
    width = int(math.floor((maximum[0] - origin_x) / cell_size_f32)) + 1
    height = int(math.floor((maximum[2] - origin_z) / cell_size_f32)) + 1
    cell_count = width * height
    if width <= 0 or height <= 0 or cell_count > MAX_CELLS:
        raise SurfaceEvidenceError(
            f"surface grid exceeds safety limit: {width}x{height}={cell_count} cells"
        )

    mins = array("f", [CANONICAL_NAN]) * cell_count
    maxs = array("f", [CANONICAL_NAN]) * cell_count
    support = array("I", [0]) * cell_count
    masks = array("Q", [0]) * cell_count
    tile_bits = [
        {"bitOrdinal": ordinal, "tileId": tile_id}
        for ordinal, (tile_id, _, _, _) in enumerate(files)
    ]

    for ordinal, (_, path, _, _) in enumerate(files):
        bit = 1 << ordinal
        for point in _iter_lab_points(
            path,
            frame,
            chunk_vertices=chunk_vertices,
        ):
            ix = min(
                width - 1,
                max(0, int(math.floor((point[0] - origin_x) / cell_size_f32))),
            )
            iz = min(
                height - 1,
                max(0, int(math.floor((point[2] - origin_z) / cell_size_f32))),
            )
            index = iz * width + ix
            vertical = _f32(point[1], "point.y")
            if support[index] == 0:
                mins[index] = vertical
                maxs[index] = vertical
            else:
                mins[index] = min(mins[index], vertical)
                maxs[index] = max(maxs[index], vertical)
            if support[index] == 0xFFFFFFFF:
                raise SurfaceEvidenceError("surface support counter overflow")
            support[index] += 1
            masks[index] |= bit

    for _, path, _, identity in files:
        if _file_identity(path) != identity:
            raise SurfaceEvidenceError(f"source PLY changed during build: {path.name}")

    payload = bytearray(
        HEADER.pack(
            MAGIC,
            BINARY_VERSION,
            width,
            height,
            cell_size_f32,
            origin_x,
            origin_z,
        )
    )
    observed = 0
    multi = 0
    max_support = 0
    observed_min = math.inf
    observed_max = -math.inf
    support_total = 0
    for index in range(cell_count):
        count = int(support[index])
        if count == 0:
            payload.extend(
                CELL.pack(
                    CANONICAL_NAN,
                    CANONICAL_NAN,
                    0,
                    0,
                    0,
                    UNKNOWN,
                    0,
                )
            )
            continue
        observed += 1
        support_total += count
        source_count = int(masks[index]).bit_count()
        if source_count > 1:
            multi += 1
        max_support = max(max_support, count)
        low = _f32(mins[index], "cell.low")
        high = _f32(maxs[index], "cell.high")
        observed_min = min(observed_min, low)
        observed_max = max(observed_max, high)
        quality = _quality(count, source_count, high - low, cell_size_f32)
        payload.extend(
            CELL.pack(
                low,
                high,
                count,
                int(masks[index]),
                quality,
                OBSERVED_SURFACE_EVIDENCE,
                0,
            )
        )
    if support_total != point_count:
        raise SurfaceEvidenceError("surface support total differs from source point count")
    if len(payload) > MAX_BINARY_BYTES:
        raise SurfaceEvidenceError("surface binary exceeds byte budget")

    metadata = {
        "grid": {
            "originLabXZ": [origin_x, origin_z],
            "cellSizeMeters": cell_size_f32,
            "width": width,
            "height": height,
            "upAxis": "+Y",
        },
        "statistics": {
            "sourcePointCount": point_count,
            "observedCellCount": observed,
            "unknownCellCount": cell_count - observed,
            "multiSourceCellCount": multi,
            "maxSupportCount": max_support,
            "heightMinLabMeters": _f32(observed_min, "heightMin"),
            "heightMaxLabMeters": _f32(observed_max, "heightMax"),
        },
    }
    return bytes(payload), metadata, tile_bits


def _manifest_core(
    *,
    bundle_summary: dict[str, Any],
    source_package: dict[str, Any],
    metadata: dict[str, Any],
    tile_bits: list[dict[str, int]],
    binary: bytes,
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
        "sourceFrameContractSha256": source_package["sourceFrameContractSha256"],
        "capabilities": dict(_CAPABILITIES),
        "grid": metadata["grid"],
        "qualityAlgorithm": dict(QUALITY_ALGORITHM),
        "tileBitOrdinals": tile_bits,
        "statistics": metadata["statistics"],
        "surfacePath": "surface.bin",
        "surfaceByteLength": len(binary),
        "surfaceSha256": _sha256_bytes(binary),
    }


def _validate_manifest(manifest: dict[str, Any]) -> dict[str, Any]:
    _exact_keys(manifest, _MANIFEST_KEYS, "surface manifest")
    if (
        manifest["schema"] != SCHEMA
        or _uint(manifest["schemaVersion"], "schemaVersion") != SCHEMA_VERSION
        or manifest["status"] != "COMPLETE"
        or manifest["privacyClass"] != PRIVACY_CLASS
        or manifest["purpose"] != PURPOSE
    ):
        raise SurfaceEvidenceError("invalid surface manifest identity")
    _sha(manifest["sourceBundleContentSha256"], "sourceBundleContentSha256")
    try:
        scan_world_contracts._stable_id(manifest["packageId"], "packageId")
    except scan_world_contracts.WorldContractError as exc:
        raise SurfaceEvidenceError(str(exc)) from exc
    _revision(manifest["sourceRevisionId"])
    _sha(manifest["sourceFrameContractSha256"], "sourceFrameContractSha256")
    if manifest["capabilities"] != _CAPABILITIES:
        raise SurfaceEvidenceError("surface capability boundary mismatch")
    if manifest["qualityAlgorithm"] != QUALITY_ALGORITHM:
        raise SurfaceEvidenceError("surface quality algorithm mismatch")

    grid = manifest["grid"]
    if not isinstance(grid, dict):
        raise SurfaceEvidenceError("grid must be an object")
    _exact_keys(grid, _GRID_KEYS, "grid")
    origin = grid["originLabXZ"]
    if not isinstance(origin, list) or len(origin) != 2:
        raise SurfaceEvidenceError("grid.originLabXZ must contain two values")
    canonical_origin = [
        _f32(origin[0], "grid.origin.x"),
        _f32(origin[1], "grid.origin.z"),
    ]
    if origin != canonical_origin:
        raise SurfaceEvidenceError("grid origin must be canonical float32")
    cell_size = _f32(grid["cellSizeMeters"], "grid.cellSizeMeters")
    if grid["cellSizeMeters"] != cell_size:
        raise SurfaceEvidenceError("grid cell size must be canonical float32")
    width = _uint(grid["width"], "grid.width")
    height = _uint(grid["height"], "grid.height")
    if (
        cell_size <= 0
        or width == 0
        or height == 0
        or width * height > MAX_CELLS
        or grid["upAxis"] != "+Y"
    ):
        raise SurfaceEvidenceError("invalid surface grid")

    bits = manifest["tileBitOrdinals"]
    if not isinstance(bits, list) or not bits or len(bits) > MAX_TILES:
        raise SurfaceEvidenceError("invalid tile bit mapping")
    observed_ids: list[int] = []
    for ordinal, item in enumerate(bits):
        if not isinstance(item, dict):
            raise SurfaceEvidenceError("tile bit record must be an object")
        _exact_keys(item, _TILE_BIT_KEYS, "tile bit record")
        if _uint(item["bitOrdinal"], "bitOrdinal") != ordinal:
            raise SurfaceEvidenceError("tile bit ordinals must be contiguous")
        observed_ids.append(_uint(item["tileId"], "tileId"))
    if observed_ids != sorted(observed_ids) or len(observed_ids) != len(
        set(observed_ids)
    ):
        raise SurfaceEvidenceError("tile ids must be sorted and unique")

    stats = manifest["statistics"]
    if not isinstance(stats, dict):
        raise SurfaceEvidenceError("statistics must be an object")
    _exact_keys(stats, _STATS_KEYS, "statistics")
    for key in (
        "sourcePointCount",
        "observedCellCount",
        "unknownCellCount",
        "multiSourceCellCount",
        "maxSupportCount",
    ):
        _uint(stats[key], f"statistics.{key}")
    for key in ("heightMinLabMeters", "heightMaxLabMeters"):
        if stats[key] != _f32(stats[key], f"statistics.{key}"):
            raise SurfaceEvidenceError(f"statistics.{key} must be canonical float32")
    if (
        stats["sourcePointCount"] == 0
        or stats["observedCellCount"] == 0
        or stats["observedCellCount"] + stats["unknownCellCount"]
        != width * height
        or stats["multiSourceCellCount"] > stats["observedCellCount"]
        or stats["heightMinLabMeters"] > stats["heightMaxLabMeters"]
    ):
        raise SurfaceEvidenceError("surface statistics do not cover grid")

    if manifest["surfacePath"] != "surface.bin":
        raise SurfaceEvidenceError("surface path must be canonical")
    expected_length = HEADER.size + width * height * CELL.size
    if (
        _uint(manifest["surfaceByteLength"], "surfaceByteLength")
        != expected_length
        or expected_length > MAX_BINARY_BYTES
    ):
        raise SurfaceEvidenceError("surface byte length mismatch")
    _sha(manifest["surfaceSha256"], "surfaceSha256")
    expected_hash = _sha(
        manifest["surfaceEvidenceContentSha256"],
        "surfaceEvidenceContentSha256",
    )
    unsigned = dict(manifest)
    unsigned.pop("surfaceEvidenceContentSha256")
    if _sha256_bytes(_canonical_json_bytes(unsigned)) != expected_hash:
        raise SurfaceEvidenceError("surfaceEvidenceContentSha256 mismatch")
    return manifest


def _pack_files(root: Path) -> set[str]:
    files: set[str] = set()
    for path in root.rglob("*"):
        if path.is_symlink():
            raise SurfaceEvidenceError(f"surface pack contains symlink: {path}")
        if path.is_file():
            files.add(path.relative_to(root).as_posix())
    return files


def verify_surface_evidence_pack(root: Path) -> dict[str, Any]:
    root = Path(root)
    if not root.is_dir() or root.is_symlink():
        raise SurfaceEvidenceError("surface root must be a real directory")
    complete = root / "COMPLETE.json"
    binary_path = root / "surface.bin"
    if (
        not complete.is_file()
        or complete.is_symlink()
        or not binary_path.is_file()
        or binary_path.is_symlink()
    ):
        raise SurfaceEvidenceError("surface pack is incomplete")
    if _pack_files(root) != {"COMPLETE.json", "surface.bin"}:
        raise SurfaceEvidenceError("surface pack contains missing or unexpected files")

    manifest = _validate_manifest(_strict_json(complete))
    if (
        binary_path.stat().st_size != manifest["surfaceByteLength"]
        or _sha256_file(binary_path) != manifest["surfaceSha256"]
    ):
        raise SurfaceEvidenceError("surface binary identity mismatch")

    valid_mask = (1 << len(manifest["tileBitOrdinals"])) - 1
    observed = 0
    unknown = 0
    multi = 0
    max_support = 0
    support_total = 0
    min_height = math.inf
    max_height = -math.inf
    with binary_path.open("rb") as handle:
        raw = handle.read(HEADER.size)
        if len(raw) != HEADER.size:
            raise SurfaceEvidenceError("surface header is truncated")
        magic, version, width, height, cell_size, origin_x, origin_z = HEADER.unpack(
            raw
        )
        grid = manifest["grid"]
        if (
            magic != MAGIC
            or version != BINARY_VERSION
            or width != grid["width"]
            or height != grid["height"]
            or _f32(cell_size, "header.cellSize") != grid["cellSizeMeters"]
            or [
                _f32(origin_x, "header.originX"),
                _f32(origin_z, "header.originZ"),
            ]
            != grid["originLabXZ"]
        ):
            raise SurfaceEvidenceError("surface binary header mismatch")

        for _ in range(width * height):
            raw = handle.read(CELL.size)
            if len(raw) != CELL.size:
                raise SurfaceEvidenceError("surface cell payload is truncated")
            low, high, support, mask, quality, classification, reserved = CELL.unpack(
                raw
            )
            if reserved != 0 or mask & ~valid_mask:
                raise SurfaceEvidenceError(
                    "surface cell contains invalid reserved data or source mask"
                )
            if classification == UNKNOWN:
                if (
                    support != 0
                    or mask != 0
                    or quality != 0
                    or struct.unpack("<I", struct.pack("<f", low))[0]
                    != CANONICAL_NAN_BITS
                    or struct.unpack("<I", struct.pack("<f", high))[0]
                    != CANONICAL_NAN_BITS
                ):
                    raise SurfaceEvidenceError("UNKNOWN cell is not canonical")
                unknown += 1
                continue
            if classification != OBSERVED_SURFACE_EVIDENCE:
                raise SurfaceEvidenceError("unknown surface classification")
            if (
                support == 0
                or mask == 0
                or quality == 0
                or not math.isfinite(low)
                or not math.isfinite(high)
                or low > high
            ):
                raise SurfaceEvidenceError("observed surface cell is invalid")
            source_count = int(mask).bit_count()
            if quality != _quality(support, source_count, high - low, cell_size):
                raise SurfaceEvidenceError("surface evidence quality mismatch")
            observed += 1
            support_total += int(support)
            multi += int(source_count > 1)
            max_support = max(max_support, int(support))
            min_height = min(min_height, float(low))
            max_height = max(max_height, float(high))
        if handle.read(1):
            raise SurfaceEvidenceError("surface binary has trailing bytes")

    stats = manifest["statistics"]
    actual = {
        "sourcePointCount": support_total,
        "observedCellCount": observed,
        "unknownCellCount": unknown,
        "multiSourceCellCount": multi,
        "maxSupportCount": max_support,
        "heightMinLabMeters": _f32(min_height, "actualMin"),
        "heightMaxLabMeters": _f32(max_height, "actualMax"),
    }
    for key, value in actual.items():
        if stats[key] != value:
            raise SurfaceEvidenceError(f"surface statistics mismatch: {key}")
    return {
        "surfaceEvidenceContentSha256": manifest[
            "surfaceEvidenceContentSha256"
        ],
        "sourceRevisionId": manifest["sourceRevisionId"],
        "width": width,
        "height": height,
        "observedCellCount": observed,
        "path": str(root),
    }


def build_surface_evidence_pack(
    *,
    bundle: Path,
    owner_gate_receipt: Path,
    source_root: Path,
    output_root: Path,
    cell_size_meters: float = 1.0,
    chunk_vertices: int = scan_ply.DEFAULT_CHUNK_VERTICES,
    label: str = "surface-evidence",
) -> Path:
    try:
        label = scan_import_bundle._label(label, "surfaceLabel")
    except scan_import_bundle.ImportBundleError as exc:
        raise SurfaceEvidenceError(str(exc)) from exc
    cell_size = _f32(cell_size_meters, "cellSizeMeters")
    if cell_size <= 0.0:
        raise SurfaceEvidenceError("cellSizeMeters must be positive")
    if chunk_vertices <= 0:
        raise SurfaceEvidenceError("chunkVertices must be positive")

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
    if (
        not frame["confirmed"]
        or source_package["sourceFrameContract"] != frame
        or source_package["revisionId"] != bundle_summary["sourceRevisionId"]
    ):
        raise SurfaceEvidenceError(
            "surface evidence requires the exact owner-confirmed bundle frame"
        )
    scan_preview_pack._validate_owner_gate_receipt(
        owner_gate_receipt,
        bundle_summary=bundle_summary,
        source_package=source_package,
    )
    if not source_root.is_dir() or source_root.is_symlink():
        raise SurfaceEvidenceError("source root must be a real directory")
    files = _source_files(source_package, source_root)
    binary, metadata, tile_bits = _build_binary(
        files,
        frame,
        cell_size=cell_size,
        chunk_vertices=chunk_vertices,
    )
    core = _manifest_core(
        bundle_summary=bundle_summary,
        source_package=source_package,
        metadata=metadata,
        tile_bits=tile_bits,
        binary=binary,
    )
    manifest = dict(core)
    manifest["surfaceEvidenceContentSha256"] = _sha256_bytes(
        _canonical_json_bytes(core)
    )
    _validate_manifest(manifest)

    output_root.mkdir(parents=True, exist_ok=True)
    if output_root.is_symlink():
        raise SurfaceEvidenceError("output root must not be a symlink")
    staging = output_root / (
        f".{label}.staging-{os.getpid()}-{uuid.uuid4().hex}"
    )
    staging.mkdir()
    try:
        _write(staging / "surface.bin", binary)
        _write(staging / "COMPLETE.json", _canonical_json_bytes(manifest))
        final = output_root / (
            f"{label}-{manifest['surfaceEvidenceContentSha256'][:16]}"
        )
        if final.exists():
            existing = verify_surface_evidence_pack(final)
            if (
                existing["surfaceEvidenceContentSha256"]
                != manifest["surfaceEvidenceContentSha256"]
            ):
                raise SurfaceEvidenceError(
                    "existing content-addressed surface pack is inconsistent"
                )
            shutil.rmtree(staging)
            return final
        os.replace(staging, final)
        verify_surface_evidence_pack(final)
        return final
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("build")
    build.add_argument("--bundle", required=True, type=Path)
    build.add_argument("--owner-gate-receipt", required=True, type=Path)
    build.add_argument("--source-root", required=True, type=Path)
    build.add_argument(
        "--output-root",
        type=Path,
        default=Path("build/scan_pipeline/surface-evidence"),
    )
    build.add_argument("--cell-size-meters", type=float, default=1.0)
    build.add_argument(
        "--chunk-vertices",
        type=int,
        default=scan_ply.DEFAULT_CHUNK_VERTICES,
    )
    build.add_argument("--label", default="surface-evidence")
    verify = sub.add_parser("verify")
    verify.add_argument("surface", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "build":
            path = build_surface_evidence_pack(
                bundle=args.bundle,
                owner_gate_receipt=args.owner_gate_receipt,
                source_root=args.source_root,
                output_root=args.output_root,
                cell_size_meters=args.cell_size_meters,
                chunk_vertices=args.chunk_vertices,
                label=args.label,
            )
            summary = verify_surface_evidence_pack(path)
        else:
            summary = verify_surface_evidence_pack(args.surface)
        print(
            "scan_surface_evidence: OK | "
            f"path={summary['path']} "
            f"surface_sha256={summary['surfaceEvidenceContentSha256']} "
            f"grid={summary['width']}x{summary['height']} "
            f"observed={summary['observedCellCount']} "
            f"revision={summary['sourceRevisionId']}"
        )
        return 0
    except (
        OSError,
        SurfaceEvidenceError,
        scan_import_bundle.ImportBundleError,
        scan_world_contracts.WorldContractError,
        scan_frames.FrameContractError,
        scan_owner_gate.OwnerGateError,
        scan_preview_pack.PreviewPackError,
        scan_ply.PlyInspectionError,
    ) as exc:
        print(f"scan_surface_evidence: ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
