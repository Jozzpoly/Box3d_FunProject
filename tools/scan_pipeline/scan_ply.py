#!/usr/bin/env python3
"""Streaming, privacy-preserving PLY point-cloud inspection for P1.

The module intentionally supports the scan package contract: binary little- or
big-endian PLY 1.0 with fixed-size scalar vertex records. Comments and obj_info
are never copied because they may contain georeferencing or private paths.
NumPy is an optional acceleration path; the standard-library backend implements
the same public contract.
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import math
from pathlib import Path
import struct
from typing import Any, Iterator, Sequence

try:  # Optional acceleration only.
    import numpy as _np
except ImportError:  # pragma: no cover - exercised where NumPy is unavailable.
    _np = None

MAX_HEADER_BYTES = 1024 * 1024
DEFAULT_CHUNK_VERTICES = 262_144

_SCALAR_TYPES: dict[str, tuple[str, int, str]] = {
    "char": ("b", 1, "i1"),
    "int8": ("b", 1, "i1"),
    "uchar": ("B", 1, "u1"),
    "uint8": ("B", 1, "u1"),
    "short": ("h", 2, "i2"),
    "int16": ("h", 2, "i2"),
    "ushort": ("H", 2, "u2"),
    "uint16": ("H", 2, "u2"),
    "int": ("i", 4, "i4"),
    "int32": ("i", 4, "i4"),
    "uint": ("I", 4, "u4"),
    "uint32": ("I", 4, "u4"),
    "float": ("f", 4, "f4"),
    "float32": ("f", 4, "f4"),
    "double": ("d", 8, "f8"),
    "float64": ("d", 8, "f8"),
}


class PlyInspectionError(RuntimeError):
    """Raised when input is malformed or outside the intentional P1 subset."""


@dataclass(frozen=True)
class PlyProperty:
    name: str
    type_name: str
    struct_code: str
    size: int
    offset: int
    numpy_code: str


@dataclass(frozen=True)
class PlyElement:
    name: str
    count: int
    properties: tuple[PlyProperty, ...]
    stride: int
    has_list_property: bool


@dataclass(frozen=True)
class PlyHeader:
    format_name: str
    endian: str
    header_size: int
    data_offset: int
    vertex_count: int
    vertex_stride: int
    vertex_properties: tuple[PlyProperty, ...]

    @property
    def property_names(self) -> tuple[str, ...]:
        return tuple(prop.name for prop in self.vertex_properties)


@dataclass(frozen=True)
class VertexChunk:
    """One bounded chunk of read-only coordinate/color sequences."""

    x: Any
    y: Any
    z: Any
    red: Any | None = None
    green: Any | None = None
    blue: Any | None = None

    def __len__(self) -> int:
        return len(self.x)


def rounded(value: float) -> float:
    result = round(float(value), 9)
    return 0.0 if result == -0.0 else result


def vector(values: Sequence[float]) -> list[float]:
    return [rounded(value) for value in values]


def sha256_file(path: Path, block_bytes: int = 4 * 1024 * 1024) -> str:
    if block_bytes <= 0:
        raise ValueError("block_bytes must be positive")
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for block in iter(lambda: handle.read(block_bytes), b""):
            digest.update(block)
    return digest.hexdigest()


def _decode_header_line(raw: bytes) -> str:
    try:
        return raw.decode("ascii").strip()
    except UnicodeDecodeError as exc:
        raise PlyInspectionError("PLY header must be ASCII") from exc


def _parse_scalar_property(tokens: list[str], offset: int) -> PlyProperty:
    if len(tokens) != 3:
        raise PlyInspectionError("scalar property must have type and name")
    type_name = tokens[1].lower()
    if type_name not in _SCALAR_TYPES:
        raise PlyInspectionError(f"unsupported PLY scalar type: {tokens[1]}")
    code, size, numpy_code = _SCALAR_TYPES[type_name]
    name = tokens[2]
    if not name:
        raise PlyInspectionError("property name cannot be empty")
    return PlyProperty(name, type_name, code, size, offset, numpy_code)


def _validate_list_property(tokens: list[str]) -> None:
    if len(tokens) != 5:
        raise PlyInspectionError("list property must have count type, item type and name")
    if tokens[2].lower() not in _SCALAR_TYPES or tokens[3].lower() not in _SCALAR_TYPES:
        raise PlyInspectionError("list property uses an unsupported scalar type")
    if not tokens[4]:
        raise PlyInspectionError("list property name cannot be empty")


def read_ply_header(path: Path, max_header_bytes: int = MAX_HEADER_BYTES) -> PlyHeader:
    """Parse a bounded PLY header without retaining comments or obj_info."""

    path = Path(path)
    if max_header_bytes <= 0:
        raise ValueError("max_header_bytes must be positive")
    try:
        file_size = path.stat().st_size
    except OSError as exc:
        raise PlyInspectionError(f"cannot stat PLY: {path.name}") from exc

    elements: list[dict[str, Any]] = []
    element_names: set[str] = set()
    current: dict[str, Any] | None = None
    format_name: str | None = None
    header_size = 0

    with path.open("rb") as handle:
        first = handle.readline()
        header_size += len(first)
        if first.rstrip(b"\r\n") != b"ply":
            raise PlyInspectionError("missing PLY magic")

        while True:
            if header_size >= max_header_bytes:
                raise PlyInspectionError("PLY header exceeds safety limit")
            raw = handle.readline()
            if not raw:
                raise PlyInspectionError("truncated PLY header")
            header_size += len(raw)
            if header_size > max_header_bytes:
                raise PlyInspectionError("PLY header exceeds safety limit")
            line = _decode_header_line(raw)
            if not line:
                continue
            tokens = line.split()
            keyword = tokens[0].lower()

            if keyword in {"comment", "obj_info"}:
                continue
            if keyword == "format":
                if elements:
                    raise PlyInspectionError("format must be declared before elements")
                if len(tokens) != 3 or tokens[2] != "1.0" or format_name is not None:
                    raise PlyInspectionError("unsupported or duplicate PLY format declaration")
                format_name = tokens[1].lower()
                continue
            if keyword == "element":
                if format_name is None:
                    raise PlyInspectionError("format must be declared before elements")
                if len(tokens) != 3:
                    raise PlyInspectionError("element declaration must have name and count")
                try:
                    count = int(tokens[2])
                except ValueError as exc:
                    raise PlyInspectionError("invalid PLY element count") from exc
                if count < 0:
                    raise PlyInspectionError("negative PLY element count")
                name = tokens[1]
                if name in element_names:
                    raise PlyInspectionError(f"duplicate PLY element: {name}")
                element_names.add(name)
                current = {
                    "name": name,
                    "count": count,
                    "properties": [],
                    "property_names": set(),
                    "has_list": False,
                }
                elements.append(current)
                continue
            if keyword == "property":
                if current is None:
                    raise PlyInspectionError("property declared before element")
                if len(tokens) >= 2 and tokens[1].lower() == "list":
                    _validate_list_property(tokens)
                    if tokens[4] in current["property_names"]:
                        raise PlyInspectionError(f"duplicate property name: {tokens[4]}")
                    current["property_names"].add(tokens[4])
                    current["has_list"] = True
                    continue
                offset = sum(prop.size for prop in current["properties"])
                prop = _parse_scalar_property(tokens, offset)
                if prop.name in current["property_names"]:
                    raise PlyInspectionError(f"duplicate property name: {prop.name}")
                current["property_names"].add(prop.name)
                current["properties"].append(prop)
                continue
            if keyword == "end_header":
                break
            raise PlyInspectionError(f"unsupported PLY header directive: {tokens[0]}")

    if format_name == "binary_little_endian":
        endian = "<"
    elif format_name == "binary_big_endian":
        endian = ">"
    elif format_name == "ascii":
        raise PlyInspectionError("ASCII PLY is intentionally unsupported for large scan data")
    else:
        raise PlyInspectionError("missing or unsupported PLY format")

    parsed_elements: list[PlyElement] = []
    for item in elements:
        properties = tuple(item["properties"])
        stride = sum(prop.size for prop in properties)
        parsed_elements.append(
            PlyElement(
                name=item["name"],
                count=item["count"],
                properties=properties,
                stride=stride,
                has_list_property=bool(item["has_list"]),
            )
        )

    vertex_indices = [
        index for index, element in enumerate(parsed_elements) if element.name == "vertex"
    ]
    if len(vertex_indices) != 1:
        raise PlyInspectionError("PLY must contain exactly one vertex element")
    vertex_index = vertex_indices[0]
    vertex = parsed_elements[vertex_index]
    if vertex.has_list_property:
        raise PlyInspectionError("list properties in vertex records are unsupported")
    if vertex.stride <= 0 and vertex.count:
        raise PlyInspectionError("vertex element has no scalar properties")

    names = {prop.name for prop in vertex.properties}
    if not {"x", "y", "z"}.issubset(names):
        raise PlyInspectionError("vertex element must contain x, y and z")

    data_offset = header_size
    for element in parsed_elements[:vertex_index]:
        if element.has_list_property:
            raise PlyInspectionError("variable-length element before vertex is unsupported")
        if element.count and element.stride <= 0:
            raise PlyInspectionError("nonempty fixed-size element before vertex has zero stride")
        data_offset += element.count * element.stride

    required = data_offset + vertex.count * vertex.stride
    if required > file_size:
        raise PlyInspectionError("truncated PLY vertex payload")

    return PlyHeader(
        format_name=format_name,
        endian=endian,
        header_size=header_size,
        data_offset=data_offset,
        vertex_count=vertex.count,
        vertex_stride=vertex.stride,
        vertex_properties=vertex.properties,
    )


def _numpy_dtype(header: PlyHeader) -> Any:
    assert _np is not None
    fields = []
    for prop in header.vertex_properties:
        code = prop.numpy_code if prop.size == 1 else header.endian + prop.numpy_code
        fields.append((prop.name, code))
    dtype = _np.dtype(fields, align=False)
    if dtype.itemsize != header.vertex_stride:
        raise PlyInspectionError("NumPy dtype does not match PLY vertex stride")
    return dtype


def _chunk_from_numpy(records: Any) -> VertexChunk:
    names = set(records.dtype.names or ())
    color = (
        (records["red"], records["green"], records["blue"])
        if {"red", "green", "blue"}.issubset(names)
        else (None, None, None)
    )
    return VertexChunk(records["x"], records["y"], records["z"], *color)


def _chunk_from_struct(raw: bytes, header: PlyHeader) -> VertexChunk:
    fmt = struct.Struct(header.endian + "".join(prop.struct_code for prop in header.vertex_properties))
    if fmt.size != header.vertex_stride:
        raise PlyInspectionError("struct layout does not match PLY vertex stride")
    index = {prop.name: item for item, prop in enumerate(header.vertex_properties)}
    x: list[float] = []
    y: list[float] = []
    z: list[float] = []
    red: list[int] | None = [] if {"red", "green", "blue"}.issubset(index) else None
    green: list[int] | None = [] if red is not None else None
    blue: list[int] | None = [] if red is not None else None
    for record in fmt.iter_unpack(raw):
        x.append(float(record[index["x"]]))
        y.append(float(record[index["y"]]))
        z.append(float(record[index["z"]]))
        if red is not None and green is not None and blue is not None:
            red.append(int(record[index["red"]]))
            green.append(int(record[index["green"]]))
            blue.append(int(record[index["blue"]]))
    return VertexChunk(x, y, z, red, green, blue)


def iter_vertex_chunks(
    path: Path,
    header: PlyHeader | None = None,
    *,
    chunk_vertices: int = DEFAULT_CHUNK_VERTICES,
    prefer_numpy: bool = True,
) -> Iterator[VertexChunk]:
    """Yield bounded chunks without materializing the full point cloud."""

    if chunk_vertices <= 0:
        raise ValueError("chunk_vertices must be positive")
    path = Path(path)
    header = header or read_ply_header(path)
    use_numpy = prefer_numpy and _np is not None
    dtype = _numpy_dtype(header) if use_numpy else None

    remaining = header.vertex_count
    with path.open("rb") as handle:
        handle.seek(header.data_offset)
        while remaining:
            count = min(chunk_vertices, remaining)
            expected = count * header.vertex_stride
            raw = handle.read(expected)
            if len(raw) != expected:
                raise PlyInspectionError("truncated PLY vertex payload during streaming")
            if use_numpy:
                assert dtype is not None and _np is not None
                records = _np.frombuffer(raw, dtype=dtype, count=count)
                yield _chunk_from_numpy(records)
            else:
                yield _chunk_from_struct(raw, header)
            remaining -= count


def _sequence_min_max(values: Any) -> tuple[float, float]:
    if hasattr(values, "min") and hasattr(values, "max"):
        return float(values.min()), float(values.max())
    return float(min(values)), float(max(values))


def _file_identity(path: Path) -> tuple[int, int]:
    stat = Path(path).stat()
    return int(stat.st_size), int(stat.st_mtime_ns)


def inspect_ply(
    path: Path,
    logical_name: str | None = None,
    *,
    chunk_vertices: int = DEFAULT_CHUNK_VERTICES,
    prefer_numpy: bool = True,
) -> dict[str, Any]:
    """Return a deterministic, privacy-safe summary of one PLY point cloud."""

    path = Path(path)
    identity_before = _file_identity(path)
    header = read_ply_header(path)
    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    observed = 0

    for chunk in iter_vertex_chunks(
        path,
        header,
        chunk_vertices=chunk_vertices,
        prefer_numpy=prefer_numpy,
    ):
        if not len(chunk):
            continue
        for axis, values in enumerate((chunk.x, chunk.y, chunk.z)):
            low, high = _sequence_min_max(values)
            if not math.isfinite(low) or not math.isfinite(high):
                raise PlyInspectionError("PLY contains non-finite XYZ values")
            minimum[axis] = min(minimum[axis], low)
            maximum[axis] = max(maximum[axis], high)
        observed += len(chunk)

    if observed != header.vertex_count:
        raise PlyInspectionError("streamed vertex count differs from header")
    if not observed:
        raise PlyInspectionError("empty point cloud")

    digest = sha256_file(path)
    identity_after = _file_identity(path)
    if identity_after != identity_before:
        raise PlyInspectionError("PLY changed during inspection")

    extent = [maximum[index] - minimum[index] for index in range(3)]
    center = [(minimum[index] + maximum[index]) * 0.5 for index in range(3)]
    xy_area = extent[0] * extent[1]
    density = observed / xy_area if xy_area > 0 else None
    property_by_name = {prop.name: prop for prop in header.vertex_properties}
    rgb_names = {"red", "green", "blue"}
    has_rgb = rgb_names.issubset(property_by_name) and all(
        property_by_name[name].type_name in {"uchar", "uint8"}
        for name in rgb_names
    )

    return {
        "logicalName": (logical_name or path.name).replace("\\", "/"),
        "fileName": path.name,
        "byteLength": identity_after[0],
        "sha256": digest,
        "format": header.format_name,
        "vertexCount": header.vertex_count,
        "vertexStride": header.vertex_stride,
        "properties": [
            {"name": prop.name, "type": prop.type_name}
            for prop in header.vertex_properties
        ],
        "hasRgb": has_rgb,
        "bounds": {
            "min": vector(minimum),
            "max": vector(maximum),
            "extent": vector(extent),
            "center": vector(center),
        },
        "meanDensityXY": rounded(density) if density is not None else None,
        "streaming": {
            "chunkVertices": int(chunk_vertices),
            "backend": "numpy" if prefer_numpy and _np is not None else "stdlib",
        },
        "fileStableDuringInspection": True,
    }
