#!/usr/bin/env python3
"""Deterministic, dependency-free inspector for glTF 2.0 GLB scan packages.

P1 evidence tool only. It does not cook runtime geometry, classify ground or
modify source assets. Outputs stable JSON/Markdown and two simple diagnostic
PNGs using Python's standard library.
"""
from __future__ import annotations

import argparse
import binascii
import hashlib
import itertools
import json
import math
from pathlib import Path
import struct
import sys
from typing import Any, Iterator, Sequence
import zipfile
import zlib

MAGIC = b"glTF"
JSON_CHUNK = 0x4E4F534A
BIN_CHUNK = 0x004E4942
GOLDEN_SEAM_MIN = [-32.0, -32.0]
GOLDEN_SEAM_MAX = [48.0, 48.0]
COMPONENTS = {
    5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2),
    5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4),
}
TYPE_SIZE = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
             "MAT2": 4, "MAT3": 9, "MAT4": 16}
PALETTE = [
    ((58, 120, 180), (202, 222, 240)),
    ((230, 126, 34), (248, 220, 196)),
    ((46, 160, 90), (199, 235, 213)),
    ((160, 75, 180), (226, 205, 235)),
    ((190, 55, 70), (241, 203, 208)),
    ((100, 105, 110), (220, 222, 224)),
]


class ScanInspectionError(RuntimeError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def rounded(value: float) -> float:
    value = round(float(value), 9)
    return 0.0 if value == -0.0 else value


def vector(values: Sequence[float]) -> list[float]:
    return [rounded(value) for value in values]


def identity() -> tuple[float, ...]:
    return (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)


def mat_mul(a: Sequence[float], b: Sequence[float]) -> tuple[float, ...]:
    return tuple(sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4))
                 for r in range(4) for c in range(4))


def node_matrix(node: dict[str, Any]) -> tuple[float, ...]:
    if "matrix" in node:
        values = node["matrix"]
        if len(values) != 16:
            raise ScanInspectionError("node matrix must have 16 values")
        return tuple(float(values[c * 4 + r]) for r in range(4) for c in range(4))
    tx, ty, tz = map(float, node.get("translation", [0, 0, 0]))
    sx, sy, sz = map(float, node.get("scale", [1, 1, 1]))
    x, y, z, w = map(float, node.get("rotation", [0, 0, 0, 1]))
    length = math.sqrt(x*x + y*y + z*z + w*w)
    if length <= 1e-20:
        raise ScanInspectionError("zero-length quaternion")
    x, y, z, w = x/length, y/length, z/length, w/length
    rotation = (
        1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w), 0,
        2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w), 0,
        2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y), 0,
        0, 0, 0, 1,
    )
    scale = (sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, sz, 0, 0, 0, 0, 1)
    translate = (1, 0, 0, tx, 0, 1, 0, ty, 0, 0, 1, tz, 0, 0, 0, 1)
    return mat_mul(mat_mul(translate, rotation), scale)


def transform_point(matrix: Sequence[float], point: Sequence[float]) -> tuple[float, float, float]:
    x, y, z = map(float, point)
    return (
        matrix[0]*x + matrix[1]*y + matrix[2]*z + matrix[3],
        matrix[4]*x + matrix[5]*y + matrix[6]*z + matrix[7],
        matrix[8]*x + matrix[9]*y + matrix[10]*z + matrix[11],
    )


def empty_bounds() -> list[list[float]]:
    return [[math.inf, math.inf, math.inf], [-math.inf, -math.inf, -math.inf]]


def add_point(bounds: list[list[float]], point: Sequence[float]) -> None:
    for axis in range(3):
        bounds[0][axis] = min(bounds[0][axis], float(point[axis]))
        bounds[1][axis] = max(bounds[1][axis], float(point[axis]))


def add_bounds(target: list[list[float]], source: Sequence[Sequence[float]]) -> None:
    add_point(target, source[0]); add_point(target, source[1])


def finish_bounds(bounds: Sequence[Sequence[float]]) -> dict[str, list[float]]:
    if not all(math.isfinite(value) for side in bounds for value in side):
        raise ScanInspectionError("no renderable bounds")
    minimum, maximum = vector(bounds[0]), vector(bounds[1])
    return {
        "min": minimum,
        "max": maximum,
        "extent": vector([maximum[i] - minimum[i] for i in range(3)]),
        "center": vector([(minimum[i] + maximum[i]) / 2 for i in range(3)]),
    }


def transform_bounds(minimum: Sequence[float], maximum: Sequence[float], matrix: Sequence[float]) -> list[list[float]]:
    result = empty_bounds()
    for point in itertools.product(*[(minimum[i], maximum[i]) for i in range(3)]):
        add_point(result, transform_point(matrix, point))
    return result


def checked(items: Sequence[Any], index: Any, label: str) -> Any:
    if not isinstance(index, int) or not 0 <= index < len(items):
        raise ScanInspectionError(f"invalid {label} index {index!r}")
    return items[index]


def parse_glb(data: bytes, name: str) -> tuple[dict[str, Any], bytes]:
    if len(data) < 20:
        raise ScanInspectionError(f"{name}: file too short")
    magic, version, declared = struct.unpack_from("<4sII", data)
    if magic != MAGIC or version != 2 or declared != len(data):
        raise ScanInspectionError(f"{name}: invalid GLB header")
    offset, document, binary = 12, None, b""
    while offset < len(data):
        if offset + 8 > len(data):
            raise ScanInspectionError(f"{name}: truncated chunk header")
        length, kind = struct.unpack_from("<II", data, offset); offset += 8
        payload = data[offset:offset+length]; offset += length
        if len(payload) != length:
            raise ScanInspectionError(f"{name}: truncated chunk")
        if kind == JSON_CHUNK:
            if document is not None:
                raise ScanInspectionError(f"{name}: duplicate JSON chunk")
            try:
                document = json.loads(payload.decode("utf-8").rstrip("\x00 \r\n\t"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                raise ScanInspectionError(f"{name}: invalid JSON: {exc}") from exc
        elif kind == BIN_CHUNK:
            if binary:
                raise ScanInspectionError(f"{name}: duplicate BIN chunk")
            binary = payload
    if document is None or document.get("asset", {}).get("version") != "2.0":
        raise ScanInspectionError(f"{name}: missing glTF 2.0 JSON")
    return document, binary


def view_bytes(doc: dict[str, Any], binary: bytes, index: int) -> bytes:
    view = checked(doc.get("bufferViews", []), index, "bufferView")
    if int(view.get("buffer", 0)) != 0:
        raise ScanInspectionError("P1 supports only GLB buffer 0")
    start = int(view.get("byteOffset", 0)); end = start + int(view.get("byteLength", 0))
    if start < 0 or end > len(binary):
        raise ScanInspectionError(f"bufferView {index} exceeds BIN chunk")
    return binary[start:end]


def accessor_values(doc: dict[str, Any], binary: bytes, index: int) -> Iterator[tuple[Any, ...]]:
    accessor = checked(doc.get("accessors", []), index, "accessor")
    if "sparse" in accessor or "bufferView" not in accessor:
        raise ScanInspectionError("sparse or viewless accessor is unsupported in P1")
    view = checked(doc.get("bufferViews", []), int(accessor["bufferView"]), "bufferView")
    component_type = int(accessor.get("componentType", 0)); type_name = accessor.get("type")
    if component_type not in COMPONENTS or type_name not in TYPE_SIZE:
        raise ScanInspectionError("unsupported accessor format")
    code, component_bytes = COMPONENTS[component_type]
    width = TYPE_SIZE[type_name]; item_bytes = component_bytes * width
    stride = int(view.get("byteStride", item_bytes)); count = int(accessor.get("count", 0))
    start = int(view.get("byteOffset", 0)) + int(accessor.get("byteOffset", 0))
    required = start + (count - 1) * stride + item_bytes if count else start
    if start < 0 or stride < item_bytes or required > len(binary):
        raise ScanInspectionError(f"accessor {index} exceeds BIN chunk")
    fmt = "<" + code * width
    for item in range(count):
        yield struct.unpack_from(fmt, binary, start + item * stride)


def accessor_bounds(doc: dict[str, Any], binary: bytes, index: int) -> tuple[list[float], list[float]]:
    accessor = checked(doc.get("accessors", []), index, "accessor")
    if len(accessor.get("min", [])) == 3 and len(accessor.get("max", [])) == 3:
        return list(map(float, accessor["min"])), list(map(float, accessor["max"]))
    result = empty_bounds()
    for value in accessor_values(doc, binary, index):
        if len(value) != 3:
            raise ScanInspectionError("POSITION must be VEC3")
        add_point(result, value)
    done = finish_bounds(result)
    return done["min"], done["max"]


def image_size(data: bytes) -> tuple[int, int, str] | None:
    if data.startswith(b"\x89PNG\r\n\x1a\n") and len(data) >= 24:
        width, height = struct.unpack_from(">II", data, 16)
        return width, height, "image/png"
    if not data.startswith(b"\xff\xd8"):
        return None
    offset = 2; sof = {0xC0,0xC1,0xC2,0xC3,0xC5,0xC6,0xC7,0xC9,0xCA,0xCB,0xCD,0xCE,0xCF}
    while offset + 4 <= len(data):
        if data[offset] != 0xFF:
            offset += 1; continue
        while offset < len(data) and data[offset] == 0xFF: offset += 1
        if offset >= len(data): break
        marker = data[offset]; offset += 1
        if marker in (0xD8, 0xD9) or 0xD0 <= marker <= 0xD7: continue
        if offset + 2 > len(data): break
        length = struct.unpack_from(">H", data, offset)[0]
        if length < 2 or offset + length > len(data): break
        if marker in sof and length >= 7:
            height, width = struct.unpack_from(">HH", data, offset + 3)
            return width, height, "image/jpeg"
        offset += length
    return None


def world_nodes(doc: dict[str, Any]) -> list[tuple[int, tuple[float, ...]]]:
    nodes = doc.get("nodes", []); scenes = doc.get("scenes", [])
    if scenes:
        roots = checked(scenes, int(doc.get("scene", 0)), "scene").get("nodes", [])
    else:
        children = {int(c) for node in nodes for c in node.get("children", [])}
        roots = [i for i in range(len(nodes)) if i not in children]
    output: list[tuple[int, tuple[float, ...]]] = []; active: set[int] = set()
    def visit(index: int, parent: Sequence[float]) -> None:
        node = checked(nodes, index, "node")
        if index in active: raise ScanInspectionError("node cycle")
        active.add(index); world = mat_mul(parent, node_matrix(node)); output.append((index, world))
        for child in node.get("children", []): visit(int(child), world)
        active.remove(index)
    for root in roots: visit(int(root), identity())
    return output


def base_color_image(doc: dict[str, Any], material_index: int | None) -> int | None:
    if material_index is None: return None
    material = checked(doc.get("materials", []), material_index, "material")
    base = material.get("pbrMetallicRoughness", {}).get("baseColorTexture")
    if not isinstance(base, dict) or "index" not in base: return None
    texture = checked(doc.get("textures", []), int(base["index"]), "texture")
    return int(texture["source"]) if "source" in texture else None


def inspect_glb_bytes(data: bytes, logical_name: str) -> dict[str, Any]:
    doc, binary = parse_glb(data, logical_name)
    images = []
    for index, image in enumerate(doc.get("images", [])):
        raw = view_bytes(doc, binary, int(image["bufferView"])) if "bufferView" in image else b""
        dimensions = image_size(raw) if raw else None
        images.append({
            "index": index, "name": image.get("name"), "storage": "bufferView" if raw else "uri",
            "mimeType": image.get("mimeType") or (dimensions[2] if dimensions else None),
            "width": dimensions[0] if dimensions else None,
            "height": dimensions[1] if dimensions else None,
            "byteLength": len(raw) if raw else None,
            "sha256": sha256(raw) if raw else None,
        })
    accessors = doc.get("accessors", []); meshes = doc.get("meshes", [])
    attributes: set[str] = set(); primitives = []; bounds = empty_bounds()
    vertices = indices = triangles = mesh_instances = 0
    for node_index, world in world_nodes(doc):
        node = doc.get("nodes", [])[node_index]
        if "mesh" not in node: continue
        mesh_index = int(node["mesh"]); mesh = checked(meshes, mesh_index, "mesh"); mesh_instances += 1
        for primitive_index, primitive in enumerate(mesh.get("primitives", [])):
            attrs = primitive.get("attributes", {}); attributes.update(map(str, attrs))
            if "POSITION" not in attrs: raise ScanInspectionError("primitive has no POSITION")
            position_index = int(attrs["POSITION"]); position = checked(accessors, position_index, "accessor")
            vertex_count = int(position.get("count", 0)); minimum, maximum = accessor_bounds(doc, binary, position_index)
            world_bounds = transform_bounds(minimum, maximum, world); add_bounds(bounds, world_bounds)
            index_index = int(primitive["indices"]) if "indices" in primitive else None
            index_count = vertex_count
            if index_index is not None:
                index_accessor = checked(accessors, index_index, "accessor")
                if index_accessor.get("type") != "SCALAR": raise ScanInspectionError("indices must be SCALAR")
                index_count = int(index_accessor.get("count", 0)); next(accessor_values(doc, binary, index_index), None)
            mode = int(primitive.get("mode", 4)); triangle_count = index_count // 3 if mode == 4 else None
            material = int(primitive["material"]) if "material" in primitive else None
            vertices += vertex_count; indices += index_count; triangles += triangle_count or 0
            primitives.append({
                "nodeIndex": node_index, "meshIndex": mesh_index, "primitiveIndex": primitive_index,
                "attributes": sorted(map(str, attrs)), "vertexCount": vertex_count,
                "indexCount": index_count, "triangleCount": triangle_count,
                "materialIndex": material, "baseColorImageIndex": base_color_image(doc, material),
                "worldBounds": finish_bounds(world_bounds),
            })
    return {
        "logicalName": logical_name.replace("\\", "/"), "fileName": Path(logical_name).name,
        "byteLength": len(data), "sha256": sha256(data), "glbVersion": 2,
        "generator": doc.get("asset", {}).get("generator"),
        "sceneCount": len(doc.get("scenes", [])), "nodeCount": len(doc.get("nodes", [])),
        "meshCount": len(meshes), "meshInstanceCount": mesh_instances,
        "primitiveCount": len(primitives), "vertexCount": vertices,
        "indexCount": indices, "triangleCount": triangles,
        "materialCount": len(doc.get("materials", [])), "textureCount": len(doc.get("textures", [])),
        "imageCount": len(images), "accessorCount": len(accessors),
        "bufferViewCount": len(doc.get("bufferViews", [])), "attributes": sorted(attributes),
        "hasNormals": "NORMAL" in attributes, "hasTangents": "TANGENT" in attributes,
        "hasTexcoord0": "TEXCOORD_0" in attributes, "worldBounds": finish_bounds(bounds),
        "extensionsUsed": sorted(doc.get("extensionsUsed", [])),
        "extensionsRequired": sorted(doc.get("extensionsRequired", [])),
        "images": images, "primitives": primitives,
    }


def collect(input_path: Path) -> list[tuple[str, bytes]]:
    if not input_path.exists(): raise ScanInspectionError(f"input does not exist: {input_path}")
    if input_path.is_file() and input_path.suffix.lower() == ".zip":
        with zipfile.ZipFile(input_path) as archive:
            names = sorted(info.filename for info in archive.infolist()
                           if not info.is_dir() and info.filename.lower().endswith(".glb"))
            return [(name, archive.read(name)) for name in names]
    if input_path.is_file() and input_path.suffix.lower() == ".glb":
        return [(input_path.name, input_path.read_bytes())]
    if input_path.is_dir():
        return [(path.relative_to(input_path).as_posix(), path.read_bytes())
                for path in sorted(input_path.rglob("*.glb"), key=lambda p: p.as_posix().lower())]
    raise ScanInspectionError("input must be GLB, directory or ZIP")


def overlap_xy(a: dict[str, Any], b: dict[str, Any]) -> dict[str, Any] | None:
    minimum = [max(a["worldBounds"]["min"][i], b["worldBounds"]["min"][i]) for i in range(2)]
    maximum = [min(a["worldBounds"]["max"][i], b["worldBounds"]["max"][i]) for i in range(2)]
    extent = [maximum[i] - minimum[i] for i in range(2)]
    if min(extent) <= 0: return None
    return {"min": vector(minimum), "max": vector(maximum), "extent": vector(extent),
            "area": rounded(extent[0] * extent[1])}


def inspect_package(input_path: Path, package_name: str | None = None) -> dict[str, Any]:
    sources = collect(input_path)
    if not sources: raise ScanInspectionError("no GLB files found")
    files = sorted((inspect_glb_bytes(data, name) for name, data in sources),
                   key=lambda item: item["logicalName"].lower())
    bounds = empty_bounds()
    for file in files: add_bounds(bounds, [file["worldBounds"]["min"], file["worldBounds"]["max"]])
    median_x = sorted(file["worldBounds"]["extent"][0] for file in files)[len(files)//2]
    median_y = sorted(file["worldBounds"]["extent"][1] for file in files)[len(files)//2]
    for file in files:
        ratios = [file["worldBounds"]["extent"][0]/median_x, file["worldBounds"]["extent"][1]/median_y]
        seam = min(ratios) < 0.2
        file["classification"] = {"seamStripCandidate": seam,
                                  "extentRatioToTypicalXY": vector(ratios),
                                  "reason": "one horizontal extent is below 20% of package median" if seam else None}
    overlaps = []
    for a, b in itertools.combinations(files, 2):
        area = overlap_xy(a, b)
        if area: overlaps.append({"a": a["fileName"], "b": b["fileName"], "xy": area})
    histogram: dict[str, int] = {}; image_bytes = 0
    for file in files:
        for image in file["images"]:
            if image["width"] and image["height"]:
                key = f'{image["width"]}x{image["height"]}'; histogram[key] = histogram.get(key, 0) + 1
            image_bytes += int(image["byteLength"] or 0)
    total = lambda key: sum(int(file[key]) for file in files)
    totals = {"fileCount": len(files), "byteLength": total("byteLength"),
              "vertexCount": total("vertexCount"), "indexCount": total("indexCount"),
              "triangleCount": total("triangleCount"), "meshCount": total("meshCount"),
              "meshInstanceCount": total("meshInstanceCount"), "primitiveCount": total("primitiveCount"),
              "materialCount": total("materialCount"), "textureCount": total("textureCount"),
              "imageCount": total("imageCount"), "embeddedImageByteLength": image_bytes}
    all_attributes = sorted({attribute for file in files for attribute in file["attributes"]})
    warnings = []
    if "NORMAL" not in all_attributes: warnings.append("No NORMAL attributes; generate visual normals offline.")
    if "TANGENT" not in all_attributes: warnings.append("No TANGENT attributes; tangent-space normal mapping is unavailable as-is.")
    if any(file["classification"]["seamStripCandidate"] for file in files):
        warnings.append("A seam-strip source exists; do not assume a regular 2x2 source layout.")
    return {
        "schema": "jozz.scan-inspection", "schemaVersion": 1,
        "packageName": package_name or input_path.stem,
        "sourceKind": "zip" if input_path.suffix.lower() == ".zip" else "directory" if input_path.is_dir() else "glb",
        "totals": totals, "attributes": all_attributes,
        "hasNormals": "NORMAL" in all_attributes, "hasTangents": "TANGENT" in all_attributes,
        "hasTexcoord0": "TEXCOORD_0" in all_attributes, "globalBounds": finish_bounds(bounds),
        "assumedHorizontalAxesForP1": ["X", "Y"], "upAxisHypothesis": "Z",
        "upAxisConfirmed": False, "scaleConfirmed": False,
        "goldenSeamRegionSourceXY": {"min": GOLDEN_SEAM_MIN, "max": GOLDEN_SEAM_MAX,
                                     "purpose": "first visual seam, residual, DEM and drive pilot"},
        "imageResolutionHistogram": dict(sorted(histogram.items())), "overlapsXY": overlaps,
        "files": files, "warnings": warnings,
    }


class Canvas:
    def __init__(self, width: int, height: int, color=(248, 248, 248)):
        self.width, self.height = width, height
        self.data = bytearray(bytes(color) * width * height)
    def pixel(self, x: int, y: int, color: tuple[int, int, int]) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            offset = (y*self.width+x)*3; self.data[offset:offset+3] = bytes(color)
    def fill(self, x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int]) -> None:
        x0, x1 = sorted((max(0, x0), min(self.width-1, x1))); y0, y1 = sorted((max(0, y0), min(self.height-1, y1)))
        if x0 > x1 or y0 > y1: return
        row = bytes(color) * (x1-x0+1)
        for y in range(y0, y1+1):
            start = (y*self.width+x0)*3; self.data[start:start+len(row)] = row
    def rect(self, x0: int, y0: int, x1: int, y1: int, color: tuple[int, int, int], width=2) -> None:
        self.fill(x0, y0, x1, y0+width-1, color); self.fill(x0, y1-width+1, x1, y1, color)
        self.fill(x0, y0, x0+width-1, y1, color); self.fill(x1-width+1, y0, x1, y1, color)
    def png(self, path: Path) -> None:
        raw = b"".join(b"\0" + bytes(self.data[y*self.width*3:(y+1)*self.width*3]) for y in range(self.height))
        def chunk(kind: bytes, payload: bytes) -> bytes:
            return struct.pack(">I", len(payload))+kind+payload+struct.pack(">I", binascii.crc32(kind+payload)&0xffffffff)
        path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR", struct.pack(">IIBBBBB", self.width,self.height,8,2,0,0,0))
                         +chunk(b"IDAT", zlib.compress(raw, 9))+chunk(b"IEND", b""))


def layout_png(report: dict[str, Any], path: Path) -> None:
    canvas = Canvas(1000, 800); plot = (50, 35, 780, 750); canvas.fill(*plot, (255,255,255)); canvas.rect(*plot, (20,20,20), 3)
    bounds = report["globalBounds"]; xmin,ymin = bounds["min"][:2]; xmax,ymax = bounds["max"][:2]
    padx=max((xmax-xmin)*.03,1); pady=max((ymax-ymin)*.03,1); xmin-=padx; xmax+=padx; ymin-=pady; ymax+=pady
    px=lambda x:int(plot[0]+(x-xmin)/(xmax-xmin)*(plot[2]-plot[0])); py=lambda y:int(plot[3]-(y-ymin)/(ymax-ymin)*(plot[3]-plot[1]))
    ordered=sorted(enumerate(report["files"]), key=lambda item:item[1]["worldBounds"]["extent"][0]*item[1]["worldBounds"]["extent"][1], reverse=True)
    for index,file in ordered:
        outline,fill=PALETTE[index%len(PALETTE)]; b=file["worldBounds"]
        x0,x1=px(b["min"][0]),px(b["max"][0]); y0,y1=py(b["max"][1]),py(b["min"][1])
        canvas.fill(x0,y0,x1,y1,fill); canvas.rect(x0,y0,x1,y1,outline,3)
        # legend swatches preserve source order; textual mapping lives in inspection.md
        ly=55+index*55; canvas.fill(825,ly,865,ly+30,fill); canvas.rect(825,ly,865,ly+30,outline,2)
    canvas.rect(px(GOLDEN_SEAM_MIN[0]),py(GOLDEN_SEAM_MAX[1]),px(GOLDEN_SEAM_MAX[0]),py(GOLDEN_SEAM_MIN[1]),(0,0,0),5)
    canvas.png(path)


def texture_png(report: dict[str, Any], path: Path) -> None:
    images=[(file["fileName"],image) for file in report["files"] for image in file["images"]]
    images.sort(key=lambda item:(-(item[1]["width"] or 0)*(item[1]["height"] or 0),item[0],item[1]["index"]))
    canvas=Canvas(1000,700); maximum=max(((image["width"] or 0)*(image["height"] or 0) for _,image in images),default=1)
    for row,(_,image) in enumerate(images):
        y=30+row*35; pixels=(image["width"] or 0)*(image["height"] or 0); width=int(pixels/maximum*900)
        outline,fill=PALETTE[row%len(PALETTE)]; canvas.fill(50,y,50+max(1,width),y+22,fill); canvas.rect(50,y,50+max(1,width),y+22,outline,1)
    canvas.png(path)


def markdown(report: dict[str, Any]) -> str:
    t=report["totals"]; b=report["globalBounds"]
    lines=[f"# Scan inspection — {report['packageName']}","","Deterministic P1 evidence report; not a runtime import.","","## Totals","",
           "| Metric | Value |","|---|---:|",f"| GLB files | {t['fileCount']} |",f"| Bytes | {t['byteLength']} |",
           f"| Vertices | {t['vertexCount']} |",f"| Indices | {t['indexCount']} |",f"| Triangles | {t['triangleCount']} |",
           f"| Meshes / primitives | {t['meshCount']} / {t['primitiveCount']} |",f"| Materials / images | {t['materialCount']} / {t['imageCount']} |",
           f"| Embedded image bytes | {t['embeddedImageByteLength']} |","","## Global bounds","",f"- min: `{b['min']}`",f"- max: `{b['max']}`",f"- extent: `{b['extent']}`",
           "- P1 horizontal-axis assumption: `X/Y`","- up-axis hypothesis: `Z` — **not confirmed**","","## Golden Seam Region","","`source X=-32..48, source Y=-32..48`","",
           "## Source files and layout legend","","The PNG legend swatches use the same top-to-bottom order as this table.","",
           "| # | File | Vertices | Triangles | Bounds XY extent | Seam strip |","|---:|---|---:|---:|---:|---|"]
    for index,file in enumerate(report["files"]):
        ex,ey=file["worldBounds"]["extent"][:2]
        lines.append(f"| {index} | {file['fileName']} | {file['vertexCount']} | {file['triangleCount']} | {ex:.6f} × {ey:.6f} | {'YES' if file['classification']['seamStripCandidate'] else 'no'} |")
    lines += ["","## Image resolution histogram",""]+[f"- `{key}`: {value}" for key,value in report["imageResolutionHistogram"].items()]
    lines += ["","## Warnings",""]+[f"- {warning}" for warning in report["warnings"]]
    lines += ["","## Diagnostic files","","- `source_layout.png`","- `texture_inventory.png`","- `inspection.json`",""]
    return "\n".join(lines)


def write_outputs(report: dict[str, Any], output: Path) -> dict[str, str]:
    output.mkdir(parents=True, exist_ok=True); json_path=output/"inspection.json"
    json_path.write_text(json.dumps(report,ensure_ascii=False,indent=2,sort_keys=True)+"\n",encoding="utf-8",newline="\n")
    (output/"inspection.md").write_text(markdown(report),encoding="utf-8",newline="\n")
    layout_png(report,output/"source_layout.png"); texture_png(report,output/"texture_inventory.png")
    return {"jsonSha256":sha256(json_path.read_bytes())}


def main(argv: Sequence[str] | None = None) -> int:
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument("--input",required=True,type=Path); parser.add_argument("--output",required=True,type=Path); parser.add_argument("--name")
    args=parser.parse_args(argv)
    try:
        report=inspect_package(args.input,args.name); outputs=write_outputs(report,args.output)
    except (OSError,zipfile.BadZipFile,ScanInspectionError) as exc:
        print(f"scan_inspect: ERROR: {exc}",file=sys.stderr); return 2
    t=report["totals"]; seam=','.join(file["fileName"] for file in report["files"] if file["classification"]["seamStripCandidate"]) or 'none'
    print(f"scan_inspect: OK | files={t['fileCount']} vertices={t['vertexCount']} triangles={t['triangleCount']} materials={t['materialCount']} images={t['imageCount']} seam_candidates={seam} json_sha256={outputs['jsonSha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
