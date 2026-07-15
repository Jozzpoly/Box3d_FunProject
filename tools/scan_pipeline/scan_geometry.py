#!/usr/bin/env python3
"""Experimental NumPy geometry adapter for photogrammetry GLB sources.

This module deliberately stays outside runtime code. It reuses the P1 structural
parser, then exposes transformed triangles for seam, DEM and heightfield probes.
"""
from __future__ import annotations

from dataclasses import dataclass
import importlib.util
from pathlib import Path
from typing import Any, Iterator, Sequence

import numpy as np

_INSPECTOR_PATH = Path(__file__).with_name("scan_inspect.py")
_spec = importlib.util.spec_from_file_location("scan_inspect", _INSPECTOR_PATH)
if _spec is None or _spec.loader is None:  # pragma: no cover - import guard
    raise RuntimeError(f"cannot load {_INSPECTOR_PATH}")
scan_inspect = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(scan_inspect)

_COMPONENT_DTYPES = {
    5120: np.dtype("<i1"),
    5121: np.dtype("<u1"),
    5122: np.dtype("<i2"),
    5123: np.dtype("<u2"),
    5125: np.dtype("<u4"),
    5126: np.dtype("<f4"),
}
_TYPE_WIDTH = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}


@dataclass(frozen=True)
class TriangleBatch:
    source: str
    mesh_index: int
    primitive_index: int
    material_index: int | None
    triangles: np.ndarray  # shape (N, 3, 3), source/world coordinates

    @property
    def count(self) -> int:
        return int(self.triangles.shape[0])


def _checked(items: Sequence[Any], index: int, label: str) -> Any:
    if index < 0 or index >= len(items):
        raise scan_inspect.ScanInspectionError(f"invalid {label} index {index}")
    return items[index]


def accessor_numpy(document: dict[str, Any], binary: bytes, accessor_index: int) -> np.ndarray:
    """Return an accessor as a dense NumPy array, respecting byteStride."""
    accessor = _checked(document.get("accessors", []), accessor_index, "accessor")
    if "sparse" in accessor or "bufferView" not in accessor:
        raise scan_inspect.ScanInspectionError("sparse/viewless accessors are unsupported in experiments")
    view = _checked(document.get("bufferViews", []), int(accessor["bufferView"]), "bufferView")
    if int(view.get("buffer", 0)) != 0:
        raise scan_inspect.ScanInspectionError("only GLB buffer 0 is supported")

    component_type = int(accessor.get("componentType", 0))
    type_name = accessor.get("type")
    if component_type not in _COMPONENT_DTYPES or type_name not in _TYPE_WIDTH:
        raise scan_inspect.ScanInspectionError("unsupported accessor format")

    scalar_dtype = _COMPONENT_DTYPES[component_type]
    width = _TYPE_WIDTH[type_name]
    count = int(accessor.get("count", 0))
    item_bytes = scalar_dtype.itemsize * width
    stride = int(view.get("byteStride", item_bytes))
    offset = int(view.get("byteOffset", 0)) + int(accessor.get("byteOffset", 0))
    required = offset + (count - 1) * stride + item_bytes if count else offset
    if offset < 0 or stride < item_bytes or required > len(binary):
        raise scan_inspect.ScanInspectionError(f"accessor {accessor_index} exceeds BIN chunk")

    if stride == item_bytes:
        result = np.frombuffer(binary, dtype=scalar_dtype, count=count * width, offset=offset)
        result = result.reshape(count, width)
    else:
        packed_dtype = np.dtype({
            "names": ["value"],
            "formats": [(scalar_dtype, (width,))],
            "offsets": [0],
            "itemsize": stride,
        })
        result = np.frombuffer(binary, dtype=packed_dtype, count=count, offset=offset)["value"]

    if accessor.get("normalized"):
        if component_type in (5120, 5122):
            maximum = float(np.iinfo(scalar_dtype).max)
            result = np.maximum(result.astype(np.float64) / maximum, -1.0)
        elif component_type in (5121, 5123):
            result = result.astype(np.float64) / float(np.iinfo(scalar_dtype).max)
    return np.asarray(result)


def transform_points(points: np.ndarray, matrix: Sequence[float]) -> np.ndarray:
    matrix_np = np.asarray(matrix, dtype=np.float64).reshape(4, 4)
    homogeneous = np.concatenate(
        [np.asarray(points, dtype=np.float64), np.ones((len(points), 1), dtype=np.float64)],
        axis=1,
    )
    return (homogeneous @ matrix_np.T)[:, :3]


def _node_mesh_instances(document: dict[str, Any]) -> Iterator[tuple[int, int, Sequence[float]]]:
    nodes = document.get("nodes", [])
    for node_index, world_matrix in scan_inspect.world_nodes(document):
        node = nodes[node_index]
        if "mesh" in node:
            yield node_index, int(node["mesh"]), world_matrix


def iter_triangle_batches(
    source_name: str,
    data: bytes,
    region_xy: tuple[float, float, float, float] | None = None,
) -> Iterator[TriangleBatch]:
    """Yield transformed triangle batches, optionally clipped by triangle XY AABB."""
    document, binary = scan_inspect.parse_glb(data, source_name)
    meshes = document.get("meshes", [])
    for _node_index, mesh_index, world_matrix in _node_mesh_instances(document):
        mesh = _checked(meshes, mesh_index, "mesh")
        for primitive_index, primitive in enumerate(mesh.get("primitives", [])):
            mode = int(primitive.get("mode", 4))
            if mode != 4:
                raise scan_inspect.ScanInspectionError(
                    f"{source_name}: primitive mode {mode} is not TRIANGLES"
                )
            attributes = primitive.get("attributes", {})
            if "POSITION" not in attributes:
                continue
            positions = accessor_numpy(document, binary, int(attributes["POSITION"]))
            if positions.shape[1] != 3:
                raise scan_inspect.ScanInspectionError("POSITION is not VEC3")
            positions = transform_points(positions, world_matrix)

            if "indices" in primitive:
                indices = accessor_numpy(document, binary, int(primitive["indices"])).reshape(-1)
                if indices.size % 3:
                    raise scan_inspect.ScanInspectionError("triangle index count is not divisible by 3")
                faces = indices.reshape(-1, 3).astype(np.int64, copy=False)
            else:
                if len(positions) % 3:
                    raise scan_inspect.ScanInspectionError("non-indexed triangle vertex count is not divisible by 3")
                faces = np.arange(len(positions), dtype=np.int64).reshape(-1, 3)
            if faces.size and (faces.min() < 0 or faces.max() >= len(positions)):
                raise scan_inspect.ScanInspectionError("triangle index out of range")
            triangles = positions[faces]

            if region_xy is not None and len(triangles):
                min_x, min_y, max_x, max_y = map(float, region_xy)
                tri_min = triangles[:, :, :2].min(axis=1)
                tri_max = triangles[:, :, :2].max(axis=1)
                keep = (
                    (tri_max[:, 0] >= min_x)
                    & (tri_min[:, 0] <= max_x)
                    & (tri_max[:, 1] >= min_y)
                    & (tri_min[:, 1] <= max_y)
                )
                triangles = triangles[keep]
            if len(triangles):
                yield TriangleBatch(
                    source=source_name,
                    mesh_index=mesh_index,
                    primitive_index=primitive_index,
                    material_index=(int(primitive["material"]) if "material" in primitive else None),
                    triangles=np.ascontiguousarray(triangles, dtype=np.float64),
                )


def load_region_sources(
    input_path: Path,
    region_xy: tuple[float, float, float, float],
) -> dict[str, np.ndarray]:
    """Collect all triangles intersecting region_xy, grouped by logical GLB source."""
    grouped: dict[str, list[np.ndarray]] = {}
    for logical_name, data in scan_inspect.collect(input_path):
        batches = [batch.triangles for batch in iter_triangle_batches(logical_name, data, region_xy)]
        grouped[logical_name] = (
            np.concatenate(batches, axis=0) if batches else np.empty((0, 3, 3), dtype=np.float64)
        )
    return grouped


def triangle_normals(triangles: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    edges_a = triangles[:, 1] - triangles[:, 0]
    edges_b = triangles[:, 2] - triangles[:, 0]
    raw = np.cross(edges_a, edges_b)
    lengths = np.linalg.norm(raw, axis=1)
    normals = np.divide(raw, lengths[:, None], out=np.zeros_like(raw), where=lengths[:, None] > 1e-15)
    areas = lengths * 0.5
    return normals, areas


def deterministic_sample(points: np.ndarray, maximum: int) -> np.ndarray:
    if len(points) <= maximum:
        return points
    indices = np.linspace(0, len(points) - 1, maximum, dtype=np.int64)
    return points[indices]
