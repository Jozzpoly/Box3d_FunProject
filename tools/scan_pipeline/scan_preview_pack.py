#!/usr/bin/env python3
"""Build and verify private render-only scan preview packs.

A preview pack is a disposable projection of one verified scan-import bundle.
It exists only to visualize source GLB geometry in the native sample host. It
is never accepted-world data and never contains collision data. Real preview
publication additionally requires a bundle-bound P1B owner-gate PASS receipt.

Format v2 adds per-material texture groups: source UV (TEXCOORD_0) is carried
per vertex and the source baseColor image of each material is downscaled to at
most 1024 px on the longest side and stored as a PNG beside the geometry. This
keeps the render honest to the source colours while staying a private, local,
render-only artifact. Textures remain absent from any shareable review file.
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
scan_owner_gate = _load_sibling("scan_owner_gate")

SCHEMA = "jozz.scan-source-visual-preview-pack"
SCHEMA_VERSION = 2
PURPOSE = "SOURCE_VISUAL_PREVIEW_ONLY"
PRIVACY_CLASS = "PRIVATE_LOCAL_ONLY"
MAGIC = b"JSPREV2\0"
BINARY_VERSION = 2
# Tile header: magic, version, tileId, groupCount.
HEADER = struct.Struct("<8sIII")
# Per-group descriptor in the header table: vertexCount, indexCount.
GROUP = struct.Struct("<II")
# Per-vertex: position.xyz + normal.xyz + uv.xy (all float32).
VERTEX = struct.Struct("<ffffffff")
INDEX = struct.Struct("<I")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
MAX_TILES = 64
MAX_GROUPS_PER_TILE = 4096
MAX_VERTICES_PER_TILE = 25_000_000
MAX_INDICES_PER_TILE = 75_000_000
MAX_TEXTURE_DIM = 1024
MAX_TEXTURE_BYTES = 16 * 1024 * 1024
MAX_SOURCE_GLB_BYTES = 2 * 1024 * 1024 * 1024
MAX_TOTAL_BINARY_BYTES = 8 * 1024 * 1024 * 1024
# Per-scan leveling is a small calibration tilt, not a reorientation (that is the
# integer axis matrix's job). Cap it well below a right angle.
MAX_LEVEL_DEGREES = 45.0
# The only rotation order the pack records/applies: about lab +X first, then +Z.
LEVELING_ORDER = "Rz*Rx"

_CAPABILITIES = {
    "sourceGeometryVisible": True,
    "texturesIncluded": True,
    "internalGeometryCorrespondencePassed": False,
    "acceptedWorld": False,
    "collisionReady": False,
}
_TILE_FORMAT = {
    "magic": MAGIC.rstrip(b"\0").decode("ascii"),
    "version": BINARY_VERSION,
    "groupLayout": "int32 groupCount, then (vertexCount,indexCount) table",
    "vertexLayout": "float32 position.xyz + float32 normal.xyz + float32 uv.xy",
    "indexLayout": "uint32 triangle-list, group-local",
    "textureLayout": "baseColor png rgba8, longest side <= 1024",
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
    "levelingCorrection",
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
    "groupCount",
    "boundsLabMeters",
    "path",
    "byteLength",
    "sha256",
    "groups",
}
_GROUP_KEYS = {
    "groupId",
    "vertexCount",
    "indexCount",
    "triangleCount",
    "texturePath",
    "textureByteLength",
    "textureSha256",
    "textureWidth",
    "textureHeight",
}
_RECEIPT_KEYS = {
    "schema",
    "schemaVersion",
    "status",
    "privacyClass",
    "inspection",
    "sourceFrameConfirmed",
    "bundle",
    "privacyReview",
    "privacy",
}
_RECEIPT_INSPECTION_KEYS = {
    "schemaVersion",
    "datasetStatus",
    "automaticEvidenceGatePassed",
    "glbFiles",
    "plyFiles",
    "pairCount",
    "byteIdenticalCopyCount",
}
_RECEIPT_BUNDLE_KEYS = {
    "internalVerificationPassed",
    "independentVerificationPassed",
    "bundleContentSha256",
    "sourceRevisionId",
}
_RECEIPT_REVIEW_KEYS = {"status", "reviewTargetRelativePath"}
_RECEIPT_PRIVACY_KEYS = {
    "sourceCoordinatesIncluded",
    "sourceBoundsIncluded",
    "sourceNamesIncluded",
    "sourcePathsIncluded",
    "sourceFileHashesIncluded",
    "bundleFingerprintIncluded",
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
        raise PreviewPackError(
            f"{label} keys mismatch; missing={missing} extra={extra}"
        )


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


def _rounded_degrees(value: Any, label: str) -> float:
    number = _finite(value, label)
    if abs(number) > MAX_LEVEL_DEGREES:
        raise PreviewPackError(
            f"{label} must be within +/-{MAX_LEVEL_DEGREES:g} degrees; leveling is "
            "a small calibration, not a reorientation"
        )
    rounded = round(number, 6)
    return 0.0 if rounded == 0.0 else rounded


def _leveling_matrix(
    x_degrees: float, z_degrees: float
) -> tuple[tuple[float, float, float], ...] | None:
    """Continuous lab-space tilt applied AFTER the integer axis permutation.

    Rotates a lab-metre point by Rz(z_degrees) * Rx(x_degrees): first about lab
    +X, then about lab +Z. Returns None for the zero/identity case so the common
    path skips the extra multiply."""
    if x_degrees == 0.0 and z_degrees == 0.0:
        return None
    ax = math.radians(x_degrees)
    az = math.radians(z_degrees)
    cx, sx = math.cos(ax), math.sin(ax)
    cz, sz = math.cos(az), math.sin(az)
    rx = ((1.0, 0.0, 0.0), (0.0, cx, -sx), (0.0, sx, cx))
    rz = ((cz, -sz, 0.0), (sz, cz, 0.0), (0.0, 0.0, 1.0))
    return tuple(
        tuple(
            sum(rz[row][k] * rx[k][column] for k in range(3)) for column in range(3)
        )
        for row in range(3)
    )


def _leveling_record(x_degrees: float, z_degrees: float) -> dict[str, Any]:
    x = _rounded_degrees(x_degrees, "levelingCorrection.labAxisDegrees.x")
    z = _rounded_degrees(z_degrees, "levelingCorrection.labAxisDegrees.z")
    return {
        "labAxisDegrees": {"x": x, "z": z},
        "order": LEVELING_ORDER,
        "applied": bool(x != 0.0 or z != 0.0),
    }


def _validate_leveling(value: Any) -> None:
    if not isinstance(value, dict):
        raise PreviewPackError("levelingCorrection must be an object")
    _exact_keys(value, {"labAxisDegrees", "order", "applied"}, "levelingCorrection")
    if value["order"] != LEVELING_ORDER:
        raise PreviewPackError("levelingCorrection.order is unsupported")
    axes = value["labAxisDegrees"]
    if not isinstance(axes, dict):
        raise PreviewPackError("levelingCorrection.labAxisDegrees must be an object")
    _exact_keys(axes, {"x", "z"}, "levelingCorrection.labAxisDegrees")
    x = _rounded_degrees(axes["x"], "levelingCorrection.labAxisDegrees.x")
    z = _rounded_degrees(axes["z"], "levelingCorrection.labAxisDegrees.z")
    applied = value["applied"]
    if not isinstance(applied, bool):
        raise PreviewPackError("levelingCorrection.applied must be a boolean")
    if applied != (x != 0.0 or z != 0.0):
        raise PreviewPackError("levelingCorrection.applied disagrees with the angles")


def _source_to_lab(
    frame: dict[str, Any],
    point: Sequence[float],
    leveling: Sequence[Sequence[float]] | None = None,
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
    lab = _mat_vec(frame["sourceToLab"]["axisMatrix"], local)
    if leveling is not None:
        lab = _mat_vec(leveling, lab)
    return lab


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


def _validate_owner_gate_receipt(
    path: Path,
    *,
    bundle_summary: dict[str, Any],
    source_package: dict[str, Any],
) -> dict[str, Any]:
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise PreviewPackError("owner-gate receipt must be a real local file")
    receipt = _strict_json(path)
    _exact_keys(receipt, _RECEIPT_KEYS, "owner-gate receipt")
    if (
        receipt["schema"] != scan_owner_gate.RECEIPT_SCHEMA
        or _uint(receipt["schemaVersion"], "receipt.schemaVersion")
        != scan_owner_gate.RECEIPT_SCHEMA_VERSION
        or receipt["status"] != "P1B_BUNDLE_PASS"
        or receipt["privacyClass"] != "PRIVATE_LOCAL_ONLY"
        or receipt["sourceFrameConfirmed"] is not True
    ):
        raise PreviewPackError("owner-gate receipt is not a P1B_BUNDLE_PASS v2")

    inspection = receipt["inspection"]
    if not isinstance(inspection, dict):
        raise PreviewPackError("receipt.inspection must be an object")
    _exact_keys(inspection, _RECEIPT_INSPECTION_KEYS, "receipt.inspection")
    tile_count = len(source_package["tiles"])
    if (
        inspection["automaticEvidenceGatePassed"] is not True
        or _uint(inspection["glbFiles"], "receipt.inspection.glbFiles")
        != tile_count
        or _uint(inspection["plyFiles"], "receipt.inspection.plyFiles")
        != tile_count
        or _uint(inspection["pairCount"], "receipt.inspection.pairCount")
        != tile_count
        or _uint(
            inspection["byteIdenticalCopyCount"],
            "receipt.inspection.byteIdenticalCopyCount",
        )
        < 1
    ):
        raise PreviewPackError("owner-gate receipt inspection coverage mismatch")

    binding = receipt["bundle"]
    if not isinstance(binding, dict):
        raise PreviewPackError("receipt.bundle must be an object")
    _exact_keys(binding, _RECEIPT_BUNDLE_KEYS, "receipt.bundle")
    if (
        binding["internalVerificationPassed"] is not True
        or binding["independentVerificationPassed"] is not True
        or _sha(binding["bundleContentSha256"], "receipt.bundleContentSha256")
        != bundle_summary["bundleContentSha256"]
        or _revision(binding["sourceRevisionId"])
        != source_package["revisionId"]
    ):
        raise PreviewPackError("owner-gate receipt does not bind this bundle revision")

    review = receipt["privacyReview"]
    if not isinstance(review, dict):
        raise PreviewPackError("receipt.privacyReview must be an object")
    _exact_keys(review, _RECEIPT_REVIEW_KEYS, "receipt.privacyReview")
    if (
        review["status"] != "ACKNOWLEDGED"
        or review["reviewTargetRelativePath"]
        != "shareable/inspection.shareable.json"
    ):
        raise PreviewPackError("owner-gate privacy review is not acknowledged")

    privacy = receipt["privacy"]
    if not isinstance(privacy, dict):
        raise PreviewPackError("receipt.privacy must be an object")
    _exact_keys(privacy, _RECEIPT_PRIVACY_KEYS, "receipt.privacy")
    expected_privacy = {
        "sourceCoordinatesIncluded": False,
        "sourceBoundsIncluded": False,
        "sourceNamesIncluded": False,
        "sourcePathsIncluded": False,
        "sourceFileHashesIncluded": False,
        "bundleFingerprintIncluded": True,
    }
    if privacy != expected_privacy:
        raise PreviewPackError("owner-gate receipt privacy boundary mismatch")
    return receipt


def _png_dimensions(data: bytes) -> tuple[int, int]:
    """Parse width/height from a PNG IHDR without an image library."""
    if (
        len(data) < 24
        or data[:8] != PNG_SIGNATURE
        or data[12:16] != b"IHDR"
    ):
        raise PreviewPackError("texture is not a PNG with a leading IHDR chunk")
    width = int.from_bytes(data[16:20], "big")
    height = int.from_bytes(data[20:24], "big")
    if width <= 0 or height <= 0:
        raise PreviewPackError("texture PNG has non-positive dimensions")
    return width, height


def _encode_texture_cv2(image_bytes: bytes) -> bytes:
    """Downscale + re-encode a baseColor image with OpenCV. cv2 treats the array
    as BGR(A), so a standard PNG decoder recovers correct RGBA."""
    import numpy as np
    import cv2

    raw = np.frombuffer(image_bytes, dtype=np.uint8)
    bgr = cv2.imdecode(raw, cv2.IMREAD_COLOR)
    if bgr is None or bgr.ndim != 3 or bgr.shape[2] != 3:
        raise PreviewPackError("baseColor image failed to decode")
    height, width = int(bgr.shape[0]), int(bgr.shape[1])
    longest = max(width, height)
    if longest > MAX_TEXTURE_DIM:
        scale = MAX_TEXTURE_DIM / float(longest)
        width = max(1, round(width * scale))
        height = max(1, round(height * scale))
        bgr = cv2.resize(bgr, (width, height), interpolation=cv2.INTER_AREA)
    bgra = cv2.cvtColor(bgr, cv2.COLOR_BGR2BGRA)
    ok, buffer = cv2.imencode(".png", bgra, [cv2.IMWRITE_PNG_COMPRESSION, 6])
    if not ok:
        raise PreviewPackError("PNG re-encode failed")
    return buffer.tobytes()


def _encode_texture_pil(image_bytes: bytes) -> bytes:
    """Downscale + re-encode a baseColor image with Pillow as a standard RGBA PNG."""
    import io

    from PIL import Image

    with Image.open(io.BytesIO(image_bytes)) as source:
        image = source.convert("RGBA")
    width, height = image.size
    longest = max(width, height)
    if longest > MAX_TEXTURE_DIM:
        scale = MAX_TEXTURE_DIM / float(longest)
        width = max(1, round(width * scale))
        height = max(1, round(height * scale))
        image = image.resize((width, height), Image.LANCZOS)
    buffer = io.BytesIO()
    image.save(buffer, format="PNG")
    return buffer.getvalue()


def _select_texture_encoder():
    """Pick an image backend for the build path. Import + verify never need one;
    only building textures does, so the choice is made lazily and either OpenCV
    or Pillow satisfies it."""
    try:
        import numpy  # noqa: F401
        import cv2  # noqa: F401

        return _encode_texture_cv2
    except Exception:
        pass
    try:
        from PIL import Image  # noqa: F401

        return _encode_texture_pil
    except Exception:
        pass
    return None


def texture_encoding_available() -> bool:
    """True when a build-time image backend (OpenCV or Pillow) is importable."""
    return _select_texture_encoder() is not None


def _encode_texture(image_bytes: bytes, label: str) -> tuple[bytes, int, int]:
    """Decode a source baseColor image, cap the longest side at 1024 px and
    re-encode as a standard RGBA PNG using whichever image backend is present."""
    encoder = _select_texture_encoder()
    if encoder is None:
        raise PreviewPackError(
            "building textures needs an image library (opencv-python or Pillow)"
        )
    try:
        png = encoder(image_bytes)
    except PreviewPackError as exc:
        raise PreviewPackError(f"{label}: {exc}") from exc
    except Exception as exc:  # pragma: no cover - backend dependent
        raise PreviewPackError(
            f"{label}: baseColor image failed to process: {exc}"
        ) from exc
    encoded_width, encoded_height = _png_dimensions(png)
    if (
        encoded_width > MAX_TEXTURE_DIM
        or encoded_height > MAX_TEXTURE_DIM
        or len(png) > MAX_TEXTURE_BYTES
    ):
        raise PreviewPackError(f"{label}: encoded texture violates limits")
    return png, encoded_width, encoded_height


def _base_color_image_bytes(
    document: dict[str, Any], binary: bytes, material_index: int, tile_id: int
) -> bytes:
    materials = document.get("materials", [])
    material = scan_inspect.checked(materials, material_index, "material")
    pbr = material.get("pbrMetallicRoughness")
    if not isinstance(pbr, dict) or "baseColorTexture" not in pbr:
        raise PreviewPackError(
            f"tile {tile_id}: material {material_index} has no baseColorTexture"
        )
    base_color = pbr["baseColorTexture"]
    if not isinstance(base_color, dict) or "index" not in base_color:
        raise PreviewPackError(
            f"tile {tile_id}: material {material_index} baseColorTexture is malformed"
        )
    texcoord = int(base_color.get("texCoord", 0))
    if texcoord != 0:
        raise PreviewPackError(
            f"tile {tile_id}: material {material_index} uses non-zero texCoord set"
        )
    textures = document.get("textures", [])
    texture = scan_inspect.checked(textures, int(base_color["index"]), "texture")
    if "source" not in texture:
        raise PreviewPackError(
            f"tile {tile_id}: texture for material {material_index} has no image source"
        )
    images = document.get("images", [])
    image = scan_inspect.checked(images, int(texture["source"]), "image")
    if "bufferView" not in image:
        raise PreviewPackError(
            f"tile {tile_id}: image for material {material_index} is not embedded"
        )
    buffer_views = document.get("bufferViews", [])
    view = scan_inspect.checked(
        buffer_views, int(image["bufferView"]), "image bufferView"
    )
    offset = int(view.get("byteOffset", 0))
    length = int(view.get("byteLength", 0))
    if offset < 0 or length <= 0 or offset + length > len(binary):
        raise PreviewPackError(
            f"tile {tile_id}: image bufferView for material {material_index} is out of range"
        )
    return binary[offset : offset + length]


def _extract_tile_groups(
    glb: bytes,
    tile_id: int,
    frame: dict[str, Any],
    leveling: Sequence[Sequence[float]] | None = None,
) -> list[dict[str, Any]]:
    """Split one source tile into per-material textured groups in lab metres."""
    if not 0 <= tile_id <= 0xFFFFFFFF:
        raise PreviewPackError("tile id exceeds binary format")
    document, binary = scan_inspect.parse_glb(glb, f"MipTile_{tile_id}.glb")
    meshes = document.get("meshes", [])
    accessors = document.get("accessors", [])
    mirror = frame["sourceToLab"]["orientationChange"] == "mirror"
    fallback_up = _axis_vector(frame["labFrame"]["axisRoles"]["up"])
    if leveling is not None:
        fallback_up = _mat_vec(leveling, fallback_up)

    # Accumulate geometry per material index; each becomes a textured group.
    buckets: dict[int, dict[str, list[Any]]] = {}

    def bucket(material_index: int) -> dict[str, list[Any]]:
        return buckets.setdefault(
            material_index, {"positions": [], "uvs": [], "indices": []}
        )

    total_vertices = 0
    total_indices = 0
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
            if "TEXCOORD_0" not in attributes:
                raise PreviewPackError(
                    f"tile {tile_id}: primitive has no TEXCOORD_0 for texturing"
                )
            if "material" not in primitive:
                raise PreviewPackError(
                    f"tile {tile_id}: primitive has no material for texturing"
                )
            material_index = int(primitive["material"])

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
            texcoord_index = int(attributes["TEXCOORD_0"])
            texcoord_accessor = scan_inspect.checked(
                accessors, texcoord_index, "TEXCOORD_0 accessor"
            )
            if (
                texcoord_accessor.get("componentType") != 5126
                or texcoord_accessor.get("type") != "VEC2"
            ):
                raise PreviewPackError(
                    f"tile {tile_id}: TEXCOORD_0 must be float32 VEC2"
                )

            source_positions = list(
                scan_inspect.accessor_values(document, binary, position_index)
            )
            source_uvs = list(
                scan_inspect.accessor_values(document, binary, texcoord_index)
            )
            if len(source_uvs) != len(source_positions):
                raise PreviewPackError(
                    f"tile {tile_id}: POSITION and TEXCOORD_0 count mismatch"
                )
            total_vertices += len(source_positions)
            if total_vertices > MAX_VERTICES_PER_TILE:
                raise PreviewPackError(f"tile {tile_id}: vertex limit exceeded")

            group = bucket(material_index)
            base = len(group["positions"])
            for point in source_positions:
                source_world = scan_inspect.transform_point(world, point)
                group["positions"].append(
                    _source_to_lab(frame, source_world, leveling)
                )
            for uv in source_uvs:
                group["uvs"].append(
                    (_finite(uv[0], "uv.u"), _finite(uv[1], "uv.v"))
                )

            if "indices" in primitive:
                index_accessor_index = int(primitive["indices"])
                index_accessor = scan_inspect.checked(
                    accessors, index_accessor_index, "index accessor"
                )
                if (
                    index_accessor.get("type") != "SCALAR"
                    or index_accessor.get("componentType")
                    not in (5121, 5123, 5125)
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
            total_indices += len(source_indices)
            if total_indices > MAX_INDICES_PER_TILE:
                raise PreviewPackError(f"tile {tile_id}: index limit exceeded")
            for offset in range(0, len(source_indices), 3):
                triangle = source_indices[offset : offset + 3]
                if any(
                    value < 0 or value >= len(source_positions)
                    for value in triangle
                ):
                    raise PreviewPackError(f"tile {tile_id}: index out of range")
                if mirror:
                    triangle[1], triangle[2] = triangle[2], triangle[1]
                group["indices"].extend(base + value for value in triangle)

    if not buckets:
        raise PreviewPackError(f"tile {tile_id}: no renderable triangle geometry")

    groups: list[dict[str, Any]] = []
    for group_id, material_index in enumerate(sorted(buckets)):
        raw = buckets[material_index]
        positions = raw["positions"]
        uvs = raw["uvs"]
        indices = raw["indices"]
        if not positions or not indices:
            raise PreviewPackError(
                f"tile {tile_id}: material {material_index} has empty geometry"
            )
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

        image_bytes = _base_color_image_bytes(
            document, binary, material_index, tile_id
        )
        texture_png, texture_width, texture_height = _encode_texture(
            image_bytes, f"tile {tile_id} material {material_index}"
        )
        groups.append(
            {
                "groupId": group_id,
                "positions": positions,
                "normals": normals,
                "uvs": uvs,
                "indices": indices,
                "texturePng": texture_png,
                "textureWidth": texture_width,
                "textureHeight": texture_height,
            }
        )
    return groups


def _serialize_tile(tile_id: int, groups: Sequence[dict[str, Any]]) -> bytes:
    if not 0 < len(groups) <= MAX_GROUPS_PER_TILE:
        raise PreviewPackError(f"tile {tile_id}: invalid group count")
    payload = bytearray(HEADER.pack(MAGIC, BINARY_VERSION, tile_id, len(groups)))
    for group in groups:
        payload.extend(
            GROUP.pack(len(group["positions"]), len(group["indices"]))
        )
    for group in groups:
        for position, normal, uv in zip(
            group["positions"], group["normals"], group["uvs"]
        ):
            payload.extend(VERTEX.pack(*position, *normal, *uv))
        for index in group["indices"]:
            payload.extend(INDEX.pack(index))
    return bytes(payload)


def _read_tile(
    data: bytes, expected_tile_id: int | None = None
) -> dict[str, Any]:
    """Structurally validate a v2 tile binary and derive its geometry record."""
    if len(data) < HEADER.size:
        raise PreviewPackError("preview tile is truncated")
    magic, version, tile_id, group_count = HEADER.unpack_from(data)
    if magic != MAGIC or version != BINARY_VERSION:
        raise PreviewPackError("preview tile magic or version mismatch")
    if expected_tile_id is not None and tile_id != expected_tile_id:
        raise PreviewPackError("preview tile id mismatch")
    if not 0 < group_count <= MAX_GROUPS_PER_TILE:
        raise PreviewPackError("preview tile has an invalid group count")

    offset = HEADER.size
    descriptors: list[tuple[int, int]] = []
    total_vertices = 0
    total_indices = 0
    for _ in range(group_count):
        if offset + GROUP.size > len(data):
            raise PreviewPackError("preview tile group table is truncated")
        vertex_count, index_count = GROUP.unpack_from(data, offset)
        offset += GROUP.size
        if (
            vertex_count < 3
            or index_count < 3
            or index_count % 3 != 0
        ):
            raise PreviewPackError("preview tile group has invalid counts")
        total_vertices += vertex_count
        total_indices += index_count
        if (
            total_vertices > MAX_VERTICES_PER_TILE
            or total_indices > MAX_INDICES_PER_TILE
        ):
            raise PreviewPackError("preview tile exceeds geometry limits")
        descriptors.append((vertex_count, index_count))

    expected_size = offset + sum(
        vertex_count * VERTEX.size + index_count * INDEX.size
        for vertex_count, index_count in descriptors
    )
    if len(data) != expected_size:
        raise PreviewPackError("preview tile byte length does not match header")

    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    group_records: list[dict[str, Any]] = []
    for group_id, (vertex_count, index_count) in enumerate(descriptors):
        for _ in range(vertex_count):
            values = VERTEX.unpack_from(data, offset)
            offset += VERTEX.size
            if not all(math.isfinite(number) for number in values):
                raise PreviewPackError(
                    "preview tile contains non-finite vertex data"
                )
            normal_length = math.sqrt(
                sum(number * number for number in values[3:6])
            )
            if abs(normal_length - 1.0) > 1.0e-3:
                raise PreviewPackError("preview tile contains non-unit normal")
            for axis in range(3):
                minimum[axis] = min(minimum[axis], values[axis])
                maximum[axis] = max(maximum[axis], values[axis])
        for _ in range(index_count):
            (index,) = INDEX.unpack_from(data, offset)
            offset += INDEX.size
            if index >= vertex_count:
                raise PreviewPackError(
                    "preview tile contains out-of-range index"
                )
        group_records.append(
            {
                "groupId": group_id,
                "vertexCount": vertex_count,
                "indexCount": index_count,
                "triangleCount": index_count // 3,
            }
        )

    return {
        "tileId": tile_id,
        "vertexCount": total_vertices,
        "indexCount": total_indices,
        "triangleCount": total_indices // 3,
        "groupCount": group_count,
        "boundsLabMeters": {
            "min": [_finite(number, "tile.bounds.min") for number in minimum],
            "max": [_finite(number, "tile.bounds.max") for number in maximum],
        },
        "groupGeometry": group_records,
    }


def _manifest_core(
    *,
    bundle_summary: dict[str, Any],
    source_package: dict[str, Any],
    tile_records: Sequence[dict[str, Any]],
    leveling: dict[str, Any],
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
        "levelingCorrection": {
            "labAxisDegrees": dict(leveling["labAxisDegrees"]),
            "order": leveling["order"],
            "applied": leveling["applied"],
        },
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
        or _uint(manifest["schemaVersion"], "schemaVersion") != SCHEMA_VERSION
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
    _validate_leveling(manifest["levelingCorrection"])

    expected_hash = _sha(
        manifest["previewContentSha256"], "previewContentSha256"
    )
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
        vertex_count = _uint(
            record["vertexCount"], f"tile[{tile_id}].vertexCount"
        )
        index_count = _uint(
            record["indexCount"], f"tile[{tile_id}].indexCount"
        )
        triangle_count = _uint(
            record["triangleCount"], f"tile[{tile_id}].triangleCount"
        )
        group_count = _uint(
            record["groupCount"], f"tile[{tile_id}].groupCount"
        )
        if (
            vertex_count == 0
            or vertex_count > MAX_VERTICES_PER_TILE
            or index_count == 0
            or index_count > MAX_INDICES_PER_TILE
            or index_count != triangle_count * 3
            or not 0 < group_count <= MAX_GROUPS_PER_TILE
        ):
            raise PreviewPackError(f"tile[{tile_id}] geometry counts are invalid")

        groups = record["groups"]
        if not isinstance(groups, list) or len(groups) != group_count:
            raise PreviewPackError(f"tile[{tile_id}] groups array is invalid")
        group_vertices = 0
        group_indices = 0
        payload_bytes = 0
        for group_id, group in enumerate(groups):
            if not isinstance(group, dict):
                raise PreviewPackError(
                    f"tile[{tile_id}] group[{group_id}] must be an object"
                )
            _exact_keys(group, _GROUP_KEYS, f"tile[{tile_id}].group[{group_id}]")
            if (
                _uint(group["groupId"], f"tile[{tile_id}].group[{group_id}].groupId")
                != group_id
            ):
                raise PreviewPackError(
                    f"tile[{tile_id}] group ids must be sequential from zero"
                )
            gv = _uint(
                group["vertexCount"], f"tile[{tile_id}].group[{group_id}].vertexCount"
            )
            gi = _uint(
                group["indexCount"], f"tile[{tile_id}].group[{group_id}].indexCount"
            )
            gt = _uint(
                group["triangleCount"], f"tile[{tile_id}].group[{group_id}].triangleCount"
            )
            if gv < 3 or gi < 3 or gi % 3 != 0 or gi != gt * 3:
                raise PreviewPackError(
                    f"tile[{tile_id}] group[{group_id}] geometry counts are invalid"
                )
            group_vertices += gv
            group_indices += gi
            payload_bytes += gv * VERTEX.size + gi * INDEX.size
            expected_texture = f"textures/tile_{tile_id:03d}_group_{group_id:03d}.png"
            if group["texturePath"] != expected_texture:
                raise PreviewPackError(
                    f"tile[{tile_id}] group[{group_id}] non-canonical texture path"
                )
            paths.append(expected_texture)
            tw = _uint(
                group["textureWidth"], f"tile[{tile_id}].group[{group_id}].textureWidth"
            )
            th = _uint(
                group["textureHeight"], f"tile[{tile_id}].group[{group_id}].textureHeight"
            )
            if tw == 0 or th == 0 or tw > MAX_TEXTURE_DIM or th > MAX_TEXTURE_DIM:
                raise PreviewPackError(
                    f"tile[{tile_id}] group[{group_id}] texture dimensions out of range"
                )
            tb = _uint(
                group["textureByteLength"],
                f"tile[{tile_id}].group[{group_id}].textureByteLength",
            )
            if tb == 0 or tb > MAX_TEXTURE_BYTES:
                raise PreviewPackError(
                    f"tile[{tile_id}] group[{group_id}] texture byte length out of range"
                )
            _sha(
                group["textureSha256"],
                f"tile[{tile_id}].group[{group_id}].textureSha256",
            )
        if group_vertices != vertex_count or group_indices != index_count:
            raise PreviewPackError(
                f"tile[{tile_id}] group counts do not sum to tile totals"
            )

        expected_length = HEADER.size + group_count * GROUP.size + payload_bytes
        byte_length = _uint(
            record["byteLength"], f"tile[{tile_id}].byteLength"
        )
        if byte_length != expected_length:
            raise PreviewPackError(f"tile[{tile_id}] byteLength mismatch")
        total_bytes += byte_length
        _sha(record["sha256"], f"tile[{tile_id}].sha256")
        normalized_bounds.append(
            _validate_bounds(
                record["boundsLabMeters"], f"tile[{tile_id}].bounds"
            )
        )

    if observed != sorted(observed) or len(observed) != len(set(observed)):
        raise PreviewPackError("preview tile ids must be sorted and unique")
    if len(paths) != len(set(paths)) or total_bytes > MAX_TOTAL_BINARY_BYTES:
        raise PreviewPackError("preview paths or total byte budget are invalid")
    tile_count = _uint(manifest["tileCount"], "tileCount")
    if tile_count != len(tiles):
        raise PreviewPackError("preview tileCount mismatch")
    expected_global = _merge_bounds(normalized_bounds)
    if (
        _validate_bounds(manifest["globalBoundsLabMeters"], "globalBounds")
        != expected_global
    ):
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
    expected = {"COMPLETE.json"}
    for record in manifest["tiles"]:
        expected.add(record["path"])
        for group in record["groups"]:
            expected.add(group["texturePath"])
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
            "groupCount",
            "boundsLabMeters",
        ):
            if parsed[key] != record[key]:
                raise PreviewPackError(
                    f"tile metadata mismatch for {record['path']}: {key}"
                )
        for group_id, group in enumerate(record["groups"]):
            geometry = parsed["groupGeometry"][group_id]
            for key in ("vertexCount", "indexCount", "triangleCount"):
                if group[key] != geometry[key]:
                    raise PreviewPackError(
                        f"group geometry mismatch for {record['path']} group {group_id}: {key}"
                    )
            texture_path = root / PurePosixPath(group["texturePath"])
            if (
                not texture_path.is_file()
                or texture_path.is_symlink()
            ):
                raise PreviewPackError(
                    f"missing texture file {group['texturePath']}"
                )
            texture_bytes = texture_path.read_bytes()
            if len(texture_bytes) != group["textureByteLength"]:
                raise PreviewPackError(
                    f"texture byteLength mismatch for {group['texturePath']}"
                )
            if _sha256_bytes(texture_bytes) != group["textureSha256"]:
                raise PreviewPackError(
                    f"texture SHA-256 mismatch for {group['texturePath']}"
                )
            width, height = _png_dimensions(texture_bytes)
            if width != group["textureWidth"] or height != group["textureHeight"]:
                raise PreviewPackError(
                    f"texture dimensions mismatch for {group['texturePath']}"
                )
            total_bytes += group["textureByteLength"]
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
    owner_gate_receipt: Path,
    source_root: Path,
    output_root: Path,
    label: str = "source-preview",
    level_x_degrees: float = 0.0,
    level_z_degrees: float = 0.0,
) -> Path:
    label = _safe_label(label)
    leveling_record = _leveling_record(level_x_degrees, level_z_degrees)
    leveling_matrix = _leveling_matrix(
        leveling_record["labAxisDegrees"]["x"],
        leveling_record["labAxisDegrees"]["z"],
    )
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
    _validate_owner_gate_receipt(
        owner_gate_receipt,
        bundle_summary=bundle_summary,
        source_package=source_package,
    )
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
            groups = _extract_tile_groups(
                source_path.read_bytes(), tile_id, frame, leveling_matrix
            )
            payload = _serialize_tile(tile_id, groups)
            geometry = _read_tile(payload, tile_id)
            total_bytes += len(payload)
            if total_bytes > MAX_TOTAL_BINARY_BYTES:
                raise PreviewPackError("preview pack exceeds total byte budget")
            relative = f"tiles/tile_{tile_id:03d}.bin"
            _write(staging / PurePosixPath(relative), payload)

            group_records: list[dict[str, Any]] = []
            for group in groups:
                group_id = int(group["groupId"])
                texture_png = group["texturePng"]
                texture_relative = (
                    f"textures/tile_{tile_id:03d}_group_{group_id:03d}.png"
                )
                total_bytes += len(texture_png)
                if total_bytes > MAX_TOTAL_BINARY_BYTES:
                    raise PreviewPackError(
                        "preview pack exceeds total byte budget"
                    )
                _write(staging / PurePosixPath(texture_relative), texture_png)
                geometry_group = geometry["groupGeometry"][group_id]
                group_records.append(
                    {
                        "groupId": group_id,
                        "vertexCount": geometry_group["vertexCount"],
                        "indexCount": geometry_group["indexCount"],
                        "triangleCount": geometry_group["triangleCount"],
                        "texturePath": texture_relative,
                        "textureByteLength": len(texture_png),
                        "textureSha256": _sha256_bytes(texture_png),
                        "textureWidth": int(group["textureWidth"]),
                        "textureHeight": int(group["textureHeight"]),
                    }
                )

            records.append(
                {
                    "tileId": geometry["tileId"],
                    "vertexCount": geometry["vertexCount"],
                    "indexCount": geometry["indexCount"],
                    "triangleCount": geometry["triangleCount"],
                    "groupCount": geometry["groupCount"],
                    "boundsLabMeters": geometry["boundsLabMeters"],
                    "path": relative,
                    "byteLength": len(payload),
                    "sha256": _sha256_bytes(payload),
                    "groups": group_records,
                }
            )
        records.sort(key=lambda record: record["tileId"])
        core = _manifest_core(
            bundle_summary=bundle_summary,
            source_package=source_package,
            tile_records=records,
            leveling=leveling_record,
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
    build.add_argument("--owner-gate-receipt", required=True, type=Path)
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
                owner_gate_receipt=args.owner_gate_receipt,
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
        scan_owner_gate.OwnerGateError,
    ) as exc:
        print(f"scan_preview_pack: ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
