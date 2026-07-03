#!/usr/bin/env python3
"""
Asset audit tool for Jozz Vehicle Box3D Native.

Usage:
  python tools/asset_audit.py
  python tools/asset_audit.py --assets-dir assets/source --out-dir assets/reports

This is a small validator/audit helper for Blockbench glTF files. It is not the
runtime importer.
"""

from __future__ import annotations

import argparse
import base64
import json
import math
from collections import Counter
from pathlib import Path
from struct import unpack_from
from typing import Any

SEMANTIC_PREFIXES = ("Socket_", "Axis_", "Marker_", "Part_", "Chassis_")
COMPONENT_INFO = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
TYPE_COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT2": 4, "MAT3": 9, "MAT4": 16}


def mat_identity() -> list[list[float]]:
    return [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]


def mat_mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def transform_point(m: list[list[float]], p: list[float]) -> list[float]:
    x, y, z = p
    return [
        m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3],
        m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3],
        m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3],
    ]


def node_matrix(node: dict[str, Any]) -> list[list[float]]:
    if "matrix" in node:
        raw = [float(v) for v in node["matrix"]]
        # glTF matrices are column-major.
        return [[raw[c * 4 + r] for c in range(4)] for r in range(4)]

    tx, ty, tz = [float(v) for v in node.get("translation", [0, 0, 0])]
    sx, sy, sz = [float(v) for v in node.get("scale", [1, 1, 1])]
    x, y, z, w = [float(v) for v in node.get("rotation", [0, 0, 0, 1])]
    n = math.sqrt(x * x + y * y + z * z + w * w)
    if n > 0:
        x, y, z, w = x / n, y / n, z / n, w / n

    r00 = 1 - 2 * y * y - 2 * z * z
    r01 = 2 * x * y - 2 * z * w
    r02 = 2 * x * z + 2 * y * w
    r10 = 2 * x * y + 2 * z * w
    r11 = 1 - 2 * x * x - 2 * z * z
    r12 = 2 * y * z - 2 * x * w
    r20 = 2 * x * z - 2 * y * w
    r21 = 2 * y * z + 2 * x * w
    r22 = 1 - 2 * x * x - 2 * y * y

    return [[r00 * sx, r01 * sy, r02 * sz, tx], [r10 * sx, r11 * sy, r12 * sz, ty], [r20 * sx, r21 * sy, r22 * sz, tz], [0.0, 0.0, 0.0, 1.0]]


def compute_world_matrices(gltf: dict[str, Any]) -> tuple[list[int], list[list[list[float]]]]:
    nodes = gltf.get("nodes", [])
    child_ids = {child for node in nodes for child in node.get("children", [])}
    roots = [i for i in range(len(nodes)) if i not in child_ids]
    local = [node_matrix(node) for node in nodes]
    world = [mat_identity() for _ in nodes]

    def walk(index: int, parent: list[list[float]]) -> None:
        world[index] = mat_mul(parent, local[index])
        for child in nodes[index].get("children", []):
            walk(child, world[index])

    for root in roots:
        walk(root, mat_identity())
    return roots, world


def load_buffers(gltf: dict[str, Any], base_dir: Path) -> list[bytes]:
    buffers = []
    for buffer in gltf.get("buffers", []):
        uri = buffer.get("uri", "")
        if uri.startswith("data:"):
            buffers.append(base64.b64decode(uri.split(",", 1)[1]))
        elif uri:
            buffers.append((base_dir / uri).read_bytes())
        else:
            buffers.append(b"")
    return buffers


def accessor_values(gltf: dict[str, Any], buffers: list[bytes], accessor_index: int) -> list[list[float]]:
    accessor = gltf["accessors"][accessor_index]
    view = gltf["bufferViews"][accessor["bufferView"]]
    fmt, component_size = COMPONENT_INFO[accessor["componentType"]]
    components = TYPE_COMPONENTS[accessor["type"]]
    count = accessor["count"]
    stride = view.get("byteStride", component_size * components)
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    data = buffers[view["buffer"]]
    values = []
    for i in range(count):
        base = offset + i * stride
        values.append([float(unpack_from("<" + fmt, data, base + c * component_size)[0]) for c in range(components)])
    return values


def audit_file(path: Path) -> dict[str, Any]:
    gltf = json.loads(path.read_text(encoding="utf-8"))
    nodes = gltf.get("nodes", [])
    roots, world = compute_world_matrices(gltf)
    names = Counter(node.get("name", "") for node in nodes)
    duplicates = {name: count for name, count in names.items() if name and count > 1}

    semantic_nodes = []
    for index, node in enumerate(nodes):
        name = node.get("name", "")
        if name.startswith(SEMANTIC_PREFIXES):
            semantic_nodes.append({"node": index, "name": name, "world_position": [round(v, 6) for v in transform_point(world[index], [0.0, 0.0, 0.0])]})

    vertex_count = 0
    triangle_count = 0
    primitive_count = 0
    world_min = [math.inf, math.inf, math.inf]
    world_max = [-math.inf, -math.inf, -math.inf]
    bounds: dict[str, Any]

    try:
        buffers = load_buffers(gltf, path.parent)
        for node_index, node in enumerate(nodes):
            if "mesh" not in node:
                continue
            for primitive in gltf["meshes"][node["mesh"]].get("primitives", []):
                primitive_count += 1
                attrs = primitive.get("attributes", {})
                if "POSITION" not in attrs:
                    continue
                positions = accessor_values(gltf, buffers, attrs["POSITION"])
                vertex_count += len(positions)
                if primitive.get("mode", 4) == 4 and "indices" in primitive:
                    triangle_count += gltf["accessors"][primitive["indices"]]["count"] // 3
                for p in positions:
                    wp = transform_point(world[node_index], p)
                    for axis in range(3):
                        world_min[axis] = min(world_min[axis], wp[axis])
                        world_max[axis] = max(world_max[axis], wp[axis])
        bounds = {"min": [round(v, 6) for v in world_min], "max": [round(v, 6) for v in world_max]}
    except Exception as exc:
        bounds = {"error": str(exc)}

    return {
        "file": path.name,
        "generator": gltf.get("asset", {}).get("generator"),
        "nodes": len(nodes),
        "meshes": len(gltf.get("meshes", [])),
        "skins": len(gltf.get("skins", [])),
        "animations": len(gltf.get("animations", [])),
        "root_nodes": [{"node": i, "name": nodes[i].get("name")} for i in roots],
        "duplicate_node_names": duplicates,
        "semantic_nodes": semantic_nodes,
        "vertex_count": vertex_count,
        "triangle_count": triangle_count,
        "primitive_count": primitive_count,
        "world_bounds": bounds,
    }


def write_markdown(reports: list[dict[str, Any]], path: Path) -> None:
    lines = ["# Asset Audit Report", "", "| File | Verts | Tris | Nodes | Meshes | Skins | Duplicate names | Semantic nodes |", "|---|---:|---:|---:|---:|---:|---|---:|"]
    for report in reports:
        dupes = ", ".join(f"{name} x{count}" for name, count in report["duplicate_node_names"].items()) or "none"
        lines.append(f"| `{report['file']}` | {report['vertex_count']} | {report['triangle_count']} | {report['nodes']} | {report['meshes']} | {report['skins']} | {dupes} | {len(report['semantic_nodes'])} |")

    lines += ["", "## Semantic nodes", ""]
    for report in reports:
        lines += [f"### `{report['file']}`", ""]
        if not report["semantic_nodes"]:
            lines += ["No semantic nodes detected.", ""]
            continue
        lines += ["| Node | Name | World position |", "|---:|---|---|"]
        for node in report["semantic_nodes"]:
            lines.append(f"| {node['node']} | `{node['name']}` | `{node['world_position']}` |")
        lines.append("")

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets-dir", default="assets/source")
    parser.add_argument("--out-dir", default="assets/reports")
    args = parser.parse_args()

    assets_dir = Path(args.assets_dir)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    reports = [audit_file(path) for path in sorted(assets_dir.glob("*.gltf"))]
    (out_dir / "asset_audit_latest.json").write_text(json.dumps(reports, indent=2), encoding="utf-8")
    write_markdown(reports, out_dir / "asset_audit_latest.md")

    for report in reports:
        duplicate_status = "duplicate-names" if report["duplicate_node_names"] else "unique-names"
        print(f"{report['file']}: {report['vertex_count']} verts, {report['triangle_count']} tris, {duplicate_status}")
    print(f"Wrote {out_dir / 'asset_audit_latest.json'}")
    print(f"Wrote {out_dir / 'asset_audit_latest.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
