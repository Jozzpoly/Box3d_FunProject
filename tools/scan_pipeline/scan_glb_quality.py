#!/usr/bin/env python3
"""Strict, privacy-safe GLB geometry evidence for photogrammetry P1.

This module sits in front of the legacy dependency-free GLB inspector. It
validates accessor layouts and primitive semantics, verifies POSITION bounds
from actual data, and scans triangle quality without cooking runtime geometry
or copying private source names and textures into the report.
"""
from __future__ import annotations

import hashlib
import importlib.util
import math
from pathlib import Path
import struct
import sys
from typing import Any, Iterable, Sequence

try:  # Optional acceleration only.
    import numpy as _np
except ImportError:  # pragma: no cover - exercised where NumPy is unavailable.
    _np = None

MODULE_DIR = Path(__file__).resolve().parent
TRIANGLE_CHUNK = 131_072
PROVISIONAL_LARGE_EDGE_SOURCE_UNITS = 10.0
EDGE_THRESHOLDS_SOURCE_UNITS = (1.0, 5.0, 10.0, 25.0, 50.0, 100.0)
DEGENERATE_RELATIVE_EPSILON = 1.0e-12

_COMPONENT_NUMPY = {
    5120: "i1",
    5121: "u1",
    5122: "i2",
    5123: "u2",
    5125: "u4",
    5126: "f4",
}
_ALLOWED_INDEX_COMPONENTS = {5121, 5123, 5125}
_ALLOWED_TYPES = {"SCALAR", "VEC2", "VEC3", "VEC4"}


class GlbQualityError(RuntimeError):
    """Raised when GLB data is malformed or outside the P1 contract."""


def _load_scan_inspect() -> Any:
    path = MODULE_DIR / "scan_inspect.py"
    name = "_jozz_scan_inspect_quality"
    spec = importlib.util.spec_from_file_location(name, path)
    if not spec or not spec.loader:
        raise RuntimeError("cannot load scan_inspect.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


scan_inspect = _load_scan_inspect()


def rounded(value: float) -> float:
    result = round(float(value), 9)
    return 0.0 if result == -0.0 else result


def vector(values: Sequence[float]) -> list[float]:
    return [rounded(value) for value in values]


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def accessor_layout(doc: dict[str, Any], binary: bytes, index: int) -> dict[str, Any]:
    accessor = scan_inspect.checked(doc.get("accessors", []), index, "accessor")
    if "sparse" in accessor or "bufferView" not in accessor:
        raise GlbQualityError(f"accessor {index} is sparse or viewless")

    component_type = int(accessor.get("componentType", 0))
    type_name = str(accessor.get("type", ""))
    if component_type not in scan_inspect.COMPONENTS or component_type not in _COMPONENT_NUMPY:
        raise GlbQualityError(f"accessor {index} has unsupported component type")
    if type_name not in _ALLOWED_TYPES:
        raise GlbQualityError(f"accessor {index} has unsupported type {type_name!r}")

    count = int(accessor.get("count", 0))
    if count <= 0:
        raise GlbQualityError(f"accessor {index} must have a positive count")

    view_index = int(accessor["bufferView"])
    view = scan_inspect.checked(doc.get("bufferViews", []), view_index, "bufferView")
    if int(view.get("buffer", 0)) != 0:
        raise GlbQualityError(f"accessor {index} uses nonzero GLB buffer")

    component_code, component_bytes = scan_inspect.COMPONENTS[component_type]
    width = int(scan_inspect.TYPE_SIZE[type_name])
    item_bytes = component_bytes * width
    view_start = int(view.get("byteOffset", 0))
    view_length = int(view.get("byteLength", 0))
    view_end = view_start + view_length
    accessor_offset = int(accessor.get("byteOffset", 0))
    stride = int(view.get("byteStride", item_bytes))

    if view_start < 0 or view_length < 0 or view_end > len(binary):
        raise GlbQualityError(f"bufferView {view_index} exceeds the GLB BIN chunk")
    if accessor_offset < 0 or accessor_offset % component_bytes != 0:
        raise GlbQualityError(f"accessor {index} has an invalid byte offset")
    if stride < item_bytes or stride % component_bytes != 0:
        raise GlbQualityError(f"accessor {index} has an invalid byte stride")
    if "byteStride" in view and not 4 <= stride <= 252:
        raise GlbQualityError(f"accessor {index} byte stride is outside glTF limits")

    required_in_view = accessor_offset + (count - 1) * stride + item_bytes
    if required_in_view > view_length:
        raise GlbQualityError(f"accessor {index} exceeds its own bufferView")

    return {
        "index": index,
        "componentType": component_type,
        "componentCode": component_code,
        "componentBytes": component_bytes,
        "type": type_name,
        "width": width,
        "count": count,
        "itemBytes": item_bytes,
        "stride": stride,
        "start": view_start + accessor_offset,
        "viewIndex": view_index,
        "hasExplicitStride": "byteStride" in view,
        "normalized": bool(accessor.get("normalized", False)),
    }


class AccessorReader:
    def __init__(self, binary: bytes, layout: dict[str, Any]):
        self.binary = binary
        self.layout = layout
        self.unpacker = struct.Struct(
            "<" + str(layout["componentCode"]) * int(layout["width"])
        )

    def __len__(self) -> int:
        return int(self.layout["count"])

    def get(self, index: int) -> tuple[Any, ...]:
        if not 0 <= index < len(self):
            raise GlbQualityError(f"accessor index {index} is out of range")
        offset = int(self.layout["start"]) + index * int(self.layout["stride"])
        return self.unpacker.unpack_from(self.binary, offset)


def _numpy_accessor(binary: bytes, layout: dict[str, Any]) -> Any:
    if _np is None:
        raise RuntimeError("NumPy is unavailable")
    dtype = _np.dtype("<" + _COMPONENT_NUMPY[int(layout["componentType"])])
    return _np.ndarray(
        shape=(int(layout["count"]), int(layout["width"])),
        dtype=dtype,
        buffer=binary,
        offset=int(layout["start"]),
        strides=(int(layout["stride"]), int(layout["componentBytes"])),
    )


def _actual_vec3_bounds(
    binary: bytes,
    layout: dict[str, Any],
    *,
    prefer_numpy: bool,
) -> tuple[list[float], list[float]]:
    if layout["type"] != "VEC3":
        raise GlbQualityError("POSITION accessor must be VEC3")

    if prefer_numpy and _np is not None:
        values = _numpy_accessor(binary, layout).astype(_np.float64, copy=False)
        if not bool(_np.isfinite(values).all()):
            raise GlbQualityError("POSITION accessor contains non-finite values")
        return values.min(axis=0).tolist(), values.max(axis=0).tolist()

    reader = AccessorReader(binary, layout)
    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    for item in range(len(reader)):
        point = reader.get(item)
        if not all(math.isfinite(float(value)) for value in point):
            raise GlbQualityError("POSITION accessor contains non-finite values")
        for axis in range(3):
            value = float(point[axis])
            minimum[axis] = min(minimum[axis], value)
            maximum[axis] = max(maximum[axis], value)
    return minimum, maximum


def _declared_bounds_evidence(
    accessor: dict[str, Any],
    actual_minimum: Sequence[float],
    actual_maximum: Sequence[float],
) -> dict[str, Any]:
    declared_minimum = accessor.get("min")
    declared_maximum = accessor.get("max")
    if not (
        isinstance(declared_minimum, list)
        and isinstance(declared_maximum, list)
        and len(declared_minimum) == 3
        and len(declared_maximum) == 3
    ):
        return {
            "present": False,
            "matchesActual": False,
            "maxAbsoluteDelta": None,
        }

    deltas = [
        abs(float(declared_minimum[i]) - float(actual_minimum[i]))
        for i in range(3)
    ] + [
        abs(float(declared_maximum[i]) - float(actual_maximum[i]))
        for i in range(3)
    ]
    extent = [float(actual_maximum[i]) - float(actual_minimum[i]) for i in range(3)]
    tolerance = max(1.0e-6, math.sqrt(sum(value * value for value in extent)) * 1.0e-7)
    maximum_delta = max(deltas)
    return {
        "present": True,
        "matchesActual": maximum_delta <= tolerance,
        "maxAbsoluteDelta": rounded(maximum_delta),
        "tolerance": rounded(tolerance),
    }


def _distance(a: Sequence[float], b: Sequence[float]) -> float:
    return math.sqrt(sum((float(a[i]) - float(b[i])) ** 2 for i in range(3)))


def _cross_length(a: Sequence[float], b: Sequence[float], c: Sequence[float]) -> float:
    ux, uy, uz = (float(b[i]) - float(a[i]) for i in range(3))
    vx, vy, vz = (float(c[i]) - float(a[i]) for i in range(3))
    cx = uy * vz - uz * vy
    cy = uz * vx - ux * vz
    cz = ux * vy - uy * vx
    return math.sqrt(cx * cx + cy * cy + cz * cz)


def _degenerate_area2_threshold(world_bounds: dict[str, list[float]]) -> float:
    diagonal = math.sqrt(sum(value * value for value in world_bounds["extent"]))
    return max(1.0e-12, diagonal * diagonal * DEGENERATE_RELATIVE_EPSILON)


def _empty_edge_threshold_counts() -> dict[str, int]:
    return {f"gt_{threshold:g}": 0 for threshold in EDGE_THRESHOLDS_SOURCE_UNITS}


def _analyze_triangles_numpy(
    positions: Any,
    indices: Any,
    world: Sequence[float],
    degenerate_area2_threshold: float,
    triangle_chunk: int,
) -> dict[str, Any]:
    assert _np is not None
    linear = _np.asarray([world[0:3], world[4:7], world[8:11]], dtype=_np.float64)
    translation = _np.asarray([world[3], world[7], world[11]], dtype=_np.float64)
    triangle_count = int(indices.size // 3)
    degenerate = 0
    provisional_large = 0
    edge_counts = _empty_edge_threshold_counts()
    maximum_edge = 0.0
    maximum_area = 0.0

    for first in range(0, triangle_count, triangle_chunk):
        last = min(triangle_count, first + triangle_chunk)
        tri_indices = indices[first * 3:last * 3].reshape(-1, 3).astype(_np.int64, copy=False)
        if bool((tri_indices < 0).any()) or bool((tri_indices >= positions.shape[0]).any()):
            raise GlbQualityError("triangle index is outside POSITION accessor")
        tri = positions[tri_indices].astype(_np.float64, copy=False)
        tri = tri @ linear.T + translation
        e01 = tri[:, 1] - tri[:, 0]
        e12 = tri[:, 2] - tri[:, 1]
        e20 = tri[:, 0] - tri[:, 2]
        lengths = _np.sqrt(
            _np.stack(
                [
                    _np.einsum("ij,ij->i", e01, e01),
                    _np.einsum("ij,ij->i", e12, e12),
                    _np.einsum("ij,ij->i", e20, e20),
                ],
                axis=1,
            )
        )
        max_edges = lengths.max(axis=1)
        cross = _np.cross(e01, tri[:, 2] - tri[:, 0])
        area2 = _np.sqrt(_np.einsum("ij,ij->i", cross, cross))
        repeated = (
            (tri_indices[:, 0] == tri_indices[:, 1])
            | (tri_indices[:, 1] == tri_indices[:, 2])
            | (tri_indices[:, 2] == tri_indices[:, 0])
        )
        degenerate += int(_np.count_nonzero(repeated | (area2 <= degenerate_area2_threshold)))
        provisional_large += int(_np.count_nonzero(max_edges > PROVISIONAL_LARGE_EDGE_SOURCE_UNITS))
        for threshold in EDGE_THRESHOLDS_SOURCE_UNITS:
            edge_counts[f"gt_{threshold:g}"] += int(_np.count_nonzero(max_edges > threshold))
        maximum_edge = max(maximum_edge, float(max_edges.max()))
        maximum_area = max(maximum_area, float((area2 * 0.5).max()))

    return {
        "triangleCountAnalyzed": triangle_count,
        "degenerateTriangleCount": degenerate,
        "provisionalLargeTriangleCount": provisional_large,
        "edgeThresholdCountsSourceUnits": edge_counts,
        "maxTriangleEdgeSourceUnits": rounded(maximum_edge),
        "maxTriangleAreaSourceUnitsSquared": rounded(maximum_area),
    }


def _analyze_triangles_stdlib(
    position_reader: AccessorReader,
    index_reader: AccessorReader | None,
    triangle_count: int,
    world: Sequence[float],
    degenerate_area2_threshold: float,
) -> dict[str, Any]:
    degenerate = 0
    provisional_large = 0
    edge_counts = _empty_edge_threshold_counts()
    maximum_edge = 0.0
    maximum_area = 0.0

    for triangle in range(triangle_count):
        if index_reader is None:
            ids = (triangle * 3, triangle * 3 + 1, triangle * 3 + 2)
        else:
            ids = tuple(int(index_reader.get(triangle * 3 + corner)[0]) for corner in range(3))
        if any(index < 0 or index >= len(position_reader) for index in ids):
            raise GlbQualityError("triangle index is outside POSITION accessor")
        points = [scan_inspect.transform_point(world, position_reader.get(index)) for index in ids]
        edge = max(
            _distance(points[0], points[1]),
            _distance(points[1], points[2]),
            _distance(points[2], points[0]),
        )
        area2 = _cross_length(points[0], points[1], points[2])
        if len(set(ids)) < 3 or area2 <= degenerate_area2_threshold:
            degenerate += 1
        if edge > PROVISIONAL_LARGE_EDGE_SOURCE_UNITS:
            provisional_large += 1
        for threshold in EDGE_THRESHOLDS_SOURCE_UNITS:
            if edge > threshold:
                edge_counts[f"gt_{threshold:g}"] += 1
        maximum_edge = max(maximum_edge, edge)
        maximum_area = max(maximum_area, area2 * 0.5)

    return {
        "triangleCountAnalyzed": triangle_count,
        "degenerateTriangleCount": degenerate,
        "provisionalLargeTriangleCount": provisional_large,
        "edgeThresholdCountsSourceUnits": edge_counts,
        "maxTriangleEdgeSourceUnits": rounded(maximum_edge),
        "maxTriangleAreaSourceUnitsSquared": rounded(maximum_area),
    }


def _validate_vertex_attribute(
    semantic: str,
    layout: dict[str, Any],
    position_count: int,
) -> None:
    if int(layout["count"]) != position_count:
        raise GlbQualityError(f"vertex attribute {semantic} count differs from POSITION")
    if semantic == "NORMAL" and (
        layout["componentType"] != 5126
        or layout["type"] != "VEC3"
        or layout["normalized"]
    ):
        raise GlbQualityError("NORMAL must be a non-normalized FLOAT VEC3 accessor")
    if semantic == "TEXCOORD_0":
        if layout["type"] != "VEC2":
            raise GlbQualityError("TEXCOORD_0 must be VEC2")
        component_type = int(layout["componentType"])
        normalized = bool(layout["normalized"])
        if component_type == 5126 and normalized:
            raise GlbQualityError("FLOAT TEXCOORD_0 cannot be normalized")
        if component_type not in {5126, 5121, 5123}:
            raise GlbQualityError("TEXCOORD_0 has an unsupported component type")
        if component_type in {5121, 5123} and not normalized:
            raise GlbQualityError("integer TEXCOORD_0 must be normalized")


def analyze_triangle_primitive(
    doc: dict[str, Any],
    binary: bytes,
    primitive: dict[str, Any],
    world: Sequence[float],
    world_bounds: dict[str, list[float]],
    actual_minimum: Sequence[float],
    actual_maximum: Sequence[float],
    *,
    prefer_numpy: bool,
    triangle_chunk: int,
) -> dict[str, Any]:
    mode = int(primitive.get("mode", 4))
    attributes = primitive.get("attributes", {})
    if "POSITION" not in attributes:
        raise GlbQualityError("primitive has no POSITION accessor")

    position_index = int(attributes["POSITION"])
    position_accessor = scan_inspect.checked(doc.get("accessors", []), position_index, "accessor")
    position_layout = accessor_layout(doc, binary, position_index)
    if (
        position_layout["componentType"] != 5126
        or position_layout["type"] != "VEC3"
        or position_layout["normalized"]
    ):
        raise GlbQualityError("POSITION must be a non-normalized FLOAT VEC3 accessor")

    for semantic, accessor_index in attributes.items():
        if semantic == "POSITION":
            continue
        _validate_vertex_attribute(
            str(semantic),
            accessor_layout(doc, binary, int(accessor_index)),
            int(position_layout["count"]),
        )

    declared_evidence = _declared_bounds_evidence(
        position_accessor,
        actual_minimum,
        actual_maximum,
    )
    if mode != 4:
        return {
            "analyzed": False,
            "reason": f"primitive mode {mode} is not TRIANGLES",
            "declaredPositionBounds": declared_evidence,
        }

    index_reader: AccessorReader | None = None
    index_values: Any
    if "indices" in primitive:
        index_layout = accessor_layout(doc, binary, int(primitive["indices"]))
        if index_layout["hasExplicitStride"]:
            raise GlbQualityError("index accessors cannot use a strided bufferView")
        if (
            index_layout["type"] != "SCALAR"
            or index_layout["componentType"] not in _ALLOWED_INDEX_COMPONENTS
            or index_layout["normalized"]
        ):
            raise GlbQualityError("indices must be an unsigned, non-normalized SCALAR accessor")
        index_count = int(index_layout["count"])
        if index_count % 3 != 0:
            raise GlbQualityError("TRIANGLES index count must be divisible by three")
        if prefer_numpy and _np is not None:
            index_values = _numpy_accessor(binary, index_layout).reshape(-1)
        else:
            index_reader = AccessorReader(binary, index_layout)
            index_values = None
    else:
        index_count = int(position_layout["count"])
        if index_count % 3 != 0:
            raise GlbQualityError("non-indexed TRIANGLES vertex count must be divisible by three")
        if prefer_numpy and _np is not None:
            index_values = _np.arange(index_count, dtype=_np.uint64)
        else:
            index_values = None

    triangle_count = index_count // 3
    degenerate_threshold = _degenerate_area2_threshold(world_bounds)
    if prefer_numpy and _np is not None:
        result = _analyze_triangles_numpy(
            _numpy_accessor(binary, position_layout),
            index_values,
            world,
            degenerate_threshold,
            triangle_chunk,
        )
        backend = "numpy"
    else:
        result = _analyze_triangles_stdlib(
            AccessorReader(binary, position_layout),
            index_reader,
            triangle_count,
            world,
            degenerate_threshold,
        )
        backend = "stdlib"

    result.update(
        {
            "analyzed": True,
            "backend": backend,
            "provisionalLargeEdgeThresholdSourceUnits": PROVISIONAL_LARGE_EDGE_SOURCE_UNITS,
            "degenerateArea2ThresholdSourceUnitsSquared": rounded(degenerate_threshold),
            "declaredPositionBounds": declared_evidence,
        }
    )
    return result


def _reachable_nodes(doc: dict[str, Any], roots: Iterable[int]) -> set[int]:
    nodes = doc.get("nodes", [])
    result: set[int] = set()
    active: set[int] = set()

    def visit(index: int) -> None:
        scan_inspect.checked(nodes, index, "node")
        if index in active:
            raise GlbQualityError("node graph contains a cycle")
        if index in result:
            return
        active.add(index)
        result.add(index)
        for child in nodes[index].get("children", []):
            visit(int(child))
        active.remove(index)

    for root in roots:
        visit(int(root))
    return result


def scene_summary(doc: dict[str, Any]) -> dict[str, Any]:
    nodes = doc.get("nodes", [])
    scenes = doc.get("scenes", [])
    summaries = []
    union: set[int] = set()

    if scenes:
        default_scene = int(doc.get("scene", 0))
        scan_inspect.checked(scenes, default_scene, "scene")
        for index, scene in enumerate(scenes):
            reachable = _reachable_nodes(doc, scene.get("nodes", []))
            union.update(reachable)
            summaries.append(
                {
                    "sceneIndex": index,
                    "rootCount": len(scene.get("nodes", [])),
                    "reachableNodeCount": len(reachable),
                    "reachableMeshNodeCount": sum(1 for node in reachable if "mesh" in nodes[node]),
                }
            )
        default_reachable = _reachable_nodes(doc, scenes[default_scene].get("nodes", []))
    else:
        children = {int(child) for node in nodes for child in node.get("children", [])}
        roots = [index for index in range(len(nodes)) if index not in children]
        default_scene = None
        default_reachable = _reachable_nodes(doc, roots)
        union.update(default_reachable)
        summaries.append(
            {
                "sceneIndex": None,
                "rootCount": len(roots),
                "reachableNodeCount": len(default_reachable),
                "reachableMeshNodeCount": sum(1 for node in default_reachable if "mesh" in nodes[node]),
            }
        )

    orphan_nodes = sorted(set(range(len(nodes))) - union)
    default_unreachable_mesh_nodes = sorted(
        index
        for index, node in enumerate(nodes)
        if "mesh" in node and index not in default_reachable
    )
    identity = scan_inspect.identity()
    non_identity = 0
    for node in nodes:
        matrix = scan_inspect.node_matrix(node)
        if any(abs(float(matrix[i]) - float(identity[i])) > 1.0e-12 for i in range(16)):
            non_identity += 1

    return {
        "sceneCount": len(scenes),
        "defaultSceneIndex": default_scene,
        "nodeCount": len(nodes),
        "nonIdentityNodeTransformCount": non_identity,
        "orphanNodeIndices": orphan_nodes,
        "defaultUnreachableMeshNodeIndices": default_unreachable_mesh_nodes,
        "scenes": summaries,
    }


def inspect_glb_quality(
    data: bytes,
    source_label: str,
    *,
    prefer_numpy: bool = True,
    triangle_chunk: int = TRIANGLE_CHUNK,
) -> dict[str, Any]:
    if triangle_chunk <= 0:
        raise ValueError("triangle_chunk must be positive")

    doc, binary = scan_inspect.parse_glb(data, source_label)
    buffers = doc.get("buffers", [])
    if len(buffers) != 1:
        raise GlbQualityError("P1 GLB contract requires exactly one embedded buffer")
    declared_buffer_bytes = int(buffers[0].get("byteLength", -1))
    if declared_buffer_bytes < 0 or declared_buffer_bytes > len(binary):
        raise GlbQualityError("GLB buffer byteLength exceeds the BIN chunk")

    layouts = [accessor_layout(doc, binary, index) for index in range(len(doc.get("accessors", [])))]
    legacy = scan_inspect.inspect_glb_bytes(data, source_label)
    scene_info = scene_summary(doc)
    meshes = doc.get("meshes", [])
    primitive_reports = []
    world_bounds = scan_inspect.empty_bounds()
    local_bounds_cache: dict[int, tuple[list[float], list[float]]] = {}

    for node_index, world in scan_inspect.world_nodes(doc):
        node = doc.get("nodes", [])[node_index]
        if "mesh" not in node:
            continue
        mesh_index = int(node["mesh"])
        mesh = scan_inspect.checked(meshes, mesh_index, "mesh")
        for primitive_index, primitive in enumerate(mesh.get("primitives", [])):
            attributes = primitive.get("attributes", {})
            if "POSITION" not in attributes:
                raise GlbQualityError("primitive has no POSITION accessor")
            position_index = int(attributes["POSITION"])
            position_layout = layouts[position_index]
            if position_index not in local_bounds_cache:
                local_bounds_cache[position_index] = _actual_vec3_bounds(
                    binary,
                    position_layout,
                    prefer_numpy=prefer_numpy,
                )
            actual_minimum, actual_maximum = local_bounds_cache[position_index]
            transformed = scan_inspect.transform_bounds(actual_minimum, actual_maximum, world)
            scan_inspect.add_bounds(world_bounds, transformed)
            finished_world_bounds = scan_inspect.finish_bounds(transformed)
            quality = analyze_triangle_primitive(
                doc,
                binary,
                primitive,
                world,
                finished_world_bounds,
                actual_minimum,
                actual_maximum,
                prefer_numpy=prefer_numpy,
                triangle_chunk=triangle_chunk,
            )
            material_index = int(primitive["material"]) if "material" in primitive else None
            primitive_reports.append(
                {
                    "nodeIndex": node_index,
                    "meshIndex": mesh_index,
                    "primitiveIndex": primitive_index,
                    "mode": int(primitive.get("mode", 4)),
                    "attributes": sorted(str(name) for name in attributes),
                    "positionAccessor": position_index,
                    "indexAccessor": int(primitive["indices"]) if "indices" in primitive else None,
                    "materialIndex": material_index,
                    "baseColorImageIndex": scan_inspect.base_color_image(doc, material_index),
                    "worldMatrix": vector(world),
                    "worldBounds": finished_world_bounds,
                    "quality": quality,
                }
            )

    final_bounds = scan_inspect.finish_bounds(world_bounds)
    analyzed = [item["quality"] for item in primitive_reports if item["quality"].get("analyzed")]
    aggregate_edges = _empty_edge_threshold_counts()
    for item in analyzed:
        for key, value in item["edgeThresholdCountsSourceUnits"].items():
            aggregate_edges[key] += int(value)
    geometry_totals = {
        "triangleCountAnalyzed": sum(int(item["triangleCountAnalyzed"]) for item in analyzed),
        "degenerateTriangleCount": sum(int(item["degenerateTriangleCount"]) for item in analyzed),
        "provisionalLargeTriangleCount": sum(int(item["provisionalLargeTriangleCount"]) for item in analyzed),
        "provisionalLargeEdgeThresholdSourceUnits": PROVISIONAL_LARGE_EDGE_SOURCE_UNITS,
        "edgeThresholdCountsSourceUnits": aggregate_edges,
        "maxTriangleEdgeSourceUnits": rounded(max((float(item["maxTriangleEdgeSourceUnits"]) for item in analyzed), default=0.0)),
        "maxTriangleAreaSourceUnitsSquared": rounded(max((float(item["maxTriangleAreaSourceUnitsSquared"]) for item in analyzed), default=0.0)),
        "unanalyzedPrimitiveCount": len(primitive_reports) - len(analyzed),
    }

    warnings = []
    if not legacy["hasNormals"]:
        warnings.append("No NORMAL attributes; visual normals must be generated later.")
    if not legacy["hasTangents"]:
        warnings.append("No TANGENT attributes; tangent-space normal mapping is unavailable as-is.")
    if geometry_totals["degenerateTriangleCount"]:
        warnings.append("Degenerate triangles exist and must not be treated as reliable surface evidence.")
    if geometry_totals["provisionalLargeTriangleCount"]:
        warnings.append("Long-edge triangles exist in source units and may indicate spikes or bridges across holes.")
    if scene_info["orphanNodeIndices"]:
        warnings.append("The GLB contains orphan nodes not reachable from any scene.")
    if scene_info["defaultUnreachableMeshNodeIndices"]:
        warnings.append("Some mesh nodes are not reachable from the default scene.")
    if any(
        item["quality"].get("declaredPositionBounds", {}).get("present")
        and not item["quality"]["declaredPositionBounds"]["matchesActual"]
        for item in primitive_reports
    ):
        warnings.append("At least one declared POSITION min/max differs from actual accessor data.")

    images = [
        {
            "index": int(image["index"]),
            "storage": image["storage"],
            "mimeType": image["mimeType"],
            "width": image["width"],
            "height": image["height"],
            "byteLength": image["byteLength"],
            "sha256": image["sha256"],
        }
        for image in legacy["images"]
    ]
    accessor_formats = [
        {
            "index": int(layout["index"]),
            "componentType": int(layout["componentType"]),
            "type": layout["type"],
            "count": int(layout["count"]),
            "normalized": bool(layout["normalized"]),
            "byteStride": int(layout["stride"]),
            "bufferView": int(layout["viewIndex"]),
        }
        for layout in layouts
    ]

    return {
        "sourceLabel": source_label,
        "byteLength": len(data),
        "sha256": _sha256(data),
        "glbVersion": 2,
        "sceneSummary": scene_info,
        "meshCount": int(legacy["meshCount"]),
        "meshInstanceCount": int(legacy["meshInstanceCount"]),
        "primitiveCount": len(primitive_reports),
        "vertexCount": int(legacy["vertexCount"]),
        "indexCount": int(legacy["indexCount"]),
        "triangleCount": int(legacy["triangleCount"]),
        "materialCount": int(legacy["materialCount"]),
        "textureCount": int(legacy["textureCount"]),
        "imageCount": len(images),
        "attributes": list(legacy["attributes"]),
        "hasNormals": bool(legacy["hasNormals"]),
        "hasTangents": bool(legacy["hasTangents"]),
        "hasTexcoord0": bool(legacy["hasTexcoord0"]),
        "extensionsUsed": list(legacy["extensionsUsed"]),
        "extensionsRequired": list(legacy["extensionsRequired"]),
        "worldBounds": final_bounds,
        "accessorFormats": accessor_formats,
        "images": images,
        "primitives": primitive_reports,
        "geometryQuality": geometry_totals,
        "analysisBackend": "numpy" if prefer_numpy and _np is not None else "stdlib",
        "warnings": warnings,
    }
