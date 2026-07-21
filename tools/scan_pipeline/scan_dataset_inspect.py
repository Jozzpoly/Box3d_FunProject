#!/usr/bin/env python3
"""P1 dataset inspector for paired photogrammetry GLB and PLY sources.

This tool combines evidence; it does not cook geometry, classify ground or
modify source data. Output images are abstract diagnostic maps, never RGB
reconstructions of the private location.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import itertools
import json
import math
from pathlib import Path
import re
import sys
from typing import Any, Iterable, Sequence

MODULE_DIR = Path(__file__).resolve().parent


def _load_sibling(name: str) -> Any:
    path = MODULE_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(name, module)
    spec.loader.exec_module(module)
    return module


scan_inspect = _load_sibling("scan_inspect")
scan_ply = _load_sibling("scan_ply")
_np = scan_ply._np

SCHEMA = "jozz.scan-dataset-inspection"
SCHEMA_VERSION = 2
DEFAULT_GRID_SIZE = 256
TILE_PATTERN = re.compile(r"(?i)(?:mip)?tile[_-]?(\d+)")


class DatasetInspectionError(RuntimeError):
    pass


def rounded(value: float) -> float:
    result = round(float(value), 9)
    return 0.0 if result == -0.0 else result


def vector(values: Sequence[float]) -> list[float]:
    return [rounded(value) for value in values]


def safe_label(value: str) -> str:
    label = Path(value.replace("\\", "/")).name.strip()
    if not label or label in {".", ".."}:
        return "scan-dataset"
    return "".join(char if char.isalnum() or char in "-_." else "_" for char in label)


def collect_files(root: Path, suffix: str) -> list[Path]:
    root = Path(root)
    if not root.exists():
        raise DatasetInspectionError("input does not exist")
    if root.is_file():
        return [root] if root.suffix.lower() == suffix else []
    return sorted(
        (path for path in root.rglob("*") if path.is_file() and path.suffix.lower() == suffix),
        key=lambda path: path.relative_to(root).as_posix().lower(),
    )


def logical_name(root: Path, path: Path) -> str:
    if root.is_dir():
        return path.relative_to(root).as_posix()
    return path.name


def tile_id(name: str) -> int:
    match = TILE_PATTERN.search(Path(name).stem)
    if not match:
        raise DatasetInspectionError(f"cannot derive tile id from {Path(name).name}")
    return int(match.group(1))


def _accessor_item_size(accessor: dict[str, Any]) -> int:
    component_type = int(accessor.get("componentType", 0))
    type_name = accessor.get("type")
    if component_type not in scan_inspect.COMPONENTS or type_name not in scan_inspect.TYPE_SIZE:
        raise DatasetInspectionError("unsupported GLB accessor format")
    return scan_inspect.COMPONENTS[component_type][1] * scan_inspect.TYPE_SIZE[type_name]


def validate_glb_accessors(data: bytes, logical: str) -> None:
    """Close a legacy inspector gap: accessors must stay inside their own view."""

    doc, binary = scan_inspect.parse_glb(data, logical)
    views = doc.get("bufferViews", [])
    for index, accessor in enumerate(doc.get("accessors", [])):
        if "sparse" in accessor or "bufferView" not in accessor:
            raise DatasetInspectionError(f"{Path(logical).name}: accessor {index} is sparse or viewless")
        view_index = int(accessor["bufferView"])
        view = scan_inspect.checked(views, view_index, "bufferView")
        if int(view.get("buffer", 0)) != 0:
            raise DatasetInspectionError(f"{Path(logical).name}: accessor {index} uses nonzero buffer")
        view_start = int(view.get("byteOffset", 0))
        view_length = int(view.get("byteLength", 0))
        view_end = view_start + view_length
        if view_start < 0 or view_length < 0 or view_end > len(binary):
            raise DatasetInspectionError(f"{Path(logical).name}: bufferView {view_index} exceeds BIN")
        item_size = _accessor_item_size(accessor)
        stride = int(view.get("byteStride", item_size))
        count = int(accessor.get("count", 0))
        accessor_offset = int(accessor.get("byteOffset", 0))
        required = accessor_offset + ((count - 1) * stride + item_size if count else 0)
        if count < 0 or accessor_offset < 0 or stride < item_size or required > view_length:
            raise DatasetInspectionError(
                f"{Path(logical).name}: accessor {index} exceeds its bufferView"
            )


def inspect_glb_file(root: Path, path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    name = logical_name(root, path)
    validate_glb_accessors(data, name)
    result = scan_inspect.inspect_glb_bytes(data, name)
    result["tileId"] = tile_id(name)
    return result


def inspect_ply_file(
    root: Path,
    path: Path,
    *,
    chunk_vertices: int,
    prefer_numpy: bool,
) -> dict[str, Any]:
    result = scan_ply.inspect_ply(
        path,
        logical_name(root, path),
        chunk_vertices=chunk_vertices,
        prefer_numpy=prefer_numpy,
    )
    result["tileId"] = tile_id(result["logicalName"])
    return result


def unique_by_tile(items: Iterable[dict[str, Any]], label: str) -> dict[int, dict[str, Any]]:
    result: dict[int, dict[str, Any]] = {}
    for item in items:
        identifier = int(item["tileId"])
        if identifier in result:
            raise DatasetInspectionError(f"duplicate {label} tile id {identifier}")
        result[identifier] = item
    return result


def xy_overlap(a: dict[str, Any], b: dict[str, Any]) -> tuple[float, float]:
    amin, amax = a["min"], a["max"]
    bmin, bmax = b["min"], b["max"]
    width = max(0.0, min(amax[0], bmax[0]) - max(amin[0], bmin[0]))
    height = max(0.0, min(amax[1], bmax[1]) - max(amin[1], bmin[1]))
    intersection = width * height
    area_a = max(0.0, (amax[0] - amin[0]) * (amax[1] - amin[1]))
    area_b = max(0.0, (bmax[0] - bmin[0]) * (bmax[1] - bmin[1]))
    denominator = min(area_a, area_b)
    return intersection, intersection / denominator if denominator > 0 else 0.0


def compare_pair(glb: dict[str, Any], ply: dict[str, Any]) -> dict[str, Any]:
    gb = glb["worldBounds"]
    pb = ply["bounds"]
    center_delta = [pb["center"][i] - gb["center"][i] for i in range(3)]
    extent_error = [
        abs(pb["extent"][i] - gb["extent"][i])
        / max(abs(pb["extent"][i]), abs(gb["extent"][i]), 1e-9)
        for i in range(3)
    ]
    diagonal = math.sqrt(sum(value * value for value in gb["extent"]))
    normalized_center_delta = math.sqrt(sum(value * value for value in center_delta)) / max(diagonal, 1.0)
    overlap_area, overlap_ratio = xy_overlap(gb, pb)
    max_extent_error = max(extent_error)

    if normalized_center_delta <= 0.02 and max_extent_error <= 0.05 and overlap_ratio >= 0.90:
        classification = "strong-match"
        reason = "centers, extents and XY coverage agree tightly"
    elif normalized_center_delta <= 0.15 and max_extent_error <= 0.30 and overlap_ratio >= 0.40:
        classification = "review"
        reason = "same spatial tile, but bounds differ enough to require manual review"
    else:
        classification = "incompatible"
        reason = "bounds do not provide sufficient evidence of a shared spatial tile"

    return {
        "tileId": int(glb["tileId"]),
        "glbFile": glb["fileName"],
        "plyFile": ply["fileName"],
        "classification": classification,
        "reason": reason,
        "centerDelta": vector(center_delta),
        "normalizedCenterDelta": rounded(normalized_center_delta),
        "extentRelativeError": vector(extent_error),
        "maxExtentRelativeError": rounded(max_extent_error),
        "xyOverlapArea": rounded(overlap_area),
        "xyOverlapOfSmaller": rounded(overlap_ratio),
    }


def union_bounds(items: Iterable[dict[str, Any]], key: str) -> dict[str, list[float]]:
    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    for item in items:
        bounds = item[key]
        for axis in range(3):
            minimum[axis] = min(minimum[axis], float(bounds["min"][axis]))
            maximum[axis] = max(maximum[axis], float(bounds["max"][axis]))
    if not all(math.isfinite(value) for value in minimum + maximum):
        raise DatasetInspectionError("cannot compute dataset bounds")
    extent = [maximum[i] - minimum[i] for i in range(3)]
    return {
        "min": vector(minimum),
        "max": vector(maximum),
        "extent": vector(extent),
        "center": vector([(minimum[i] + maximum[i]) * 0.5 for i in range(3)]),
    }


class GridEvidence:
    def __init__(self, bounds: dict[str, list[float]], size: int):
        if not 16 <= size <= 2048:
            raise DatasetInspectionError("grid size must be between 16 and 2048")
        self.bounds = bounds
        self.width = self.height = int(size)
        self.cell_count = self.width * self.height
        self.xmin, self.ymin = bounds["min"][:2]
        self.xmax, self.ymax = bounds["max"][:2]
        self.xextent = self.xmax - self.xmin
        self.yextent = self.ymax - self.ymin
        if self.xextent <= 0 or self.yextent <= 0:
            raise DatasetInspectionError("dataset has zero XY extent")
        if _np is not None:
            self.count = _np.zeros(self.cell_count, dtype=_np.uint64)
            self.zmin = _np.full(self.cell_count, _np.inf, dtype=_np.float64)
            self.zmax = _np.full(self.cell_count, -_np.inf, dtype=_np.float64)
            self.support = _np.zeros(self.cell_count, dtype=_np.uint16)
        else:
            self.count = [0] * self.cell_count
            self.zmin = [math.inf] * self.cell_count
            self.zmax = [-math.inf] * self.cell_count
            self.support = [0] * self.cell_count

    def _flat_numpy(self, chunk: Any) -> tuple[Any, Any]:
        assert _np is not None
        x = _np.asarray(chunk.x, dtype=_np.float64)
        y = _np.asarray(chunk.y, dtype=_np.float64)
        z = _np.asarray(chunk.z, dtype=_np.float64)
        ix = _np.floor((x - self.xmin) / self.xextent * self.width).astype(_np.int64)
        iy = _np.floor((y - self.ymin) / self.yextent * self.height).astype(_np.int64)
        _np.clip(ix, 0, self.width - 1, out=ix)
        _np.clip(iy, 0, self.height - 1, out=iy)
        return iy * self.width + ix, z

    def add_source(self, path: Path, *, chunk_vertices: int, prefer_numpy: bool) -> None:
        header = scan_ply.read_ply_header(path)
        use_numpy = prefer_numpy and _np is not None
        seen = _np.zeros(self.cell_count, dtype=bool) if use_numpy else set()
        for chunk in scan_ply.iter_vertex_chunks(
            path,
            header,
            chunk_vertices=chunk_vertices,
            prefer_numpy=prefer_numpy,
        ):
            if use_numpy:
                assert _np is not None
                flat, z = self._flat_numpy(chunk)
                self.count += _np.bincount(flat, minlength=self.cell_count).astype(_np.uint64)
                _np.minimum.at(self.zmin, flat, z)
                _np.maximum.at(self.zmax, flat, z)
                seen[_np.unique(flat)] = True
            else:
                for x, y, z in zip(chunk.x, chunk.y, chunk.z):
                    ix = min(self.width - 1, max(0, int((x - self.xmin) / self.xextent * self.width)))
                    iy = min(self.height - 1, max(0, int((y - self.ymin) / self.yextent * self.height)))
                    flat = iy * self.width + ix
                    self.count[flat] += 1
                    self.zmin[flat] = min(self.zmin[flat], float(z))
                    self.zmax[flat] = max(self.zmax[flat], float(z))
                    seen.add(flat)
        if use_numpy:
            self.support += seen.astype(self.support.dtype)
        else:
            for flat in seen:
                self.support[flat] += 1

    def finite_spreads(self) -> list[float]:
        if _np is not None and hasattr(self.count, "dtype"):
            valid = self.count > 0
            return [float(value) for value in (self.zmax[valid] - self.zmin[valid]).tolist()]
        return [
            float(self.zmax[i] - self.zmin[i])
            for i in range(self.cell_count)
            if self.count[i] > 0
        ]

    def summary(self) -> dict[str, Any]:
        counts = self.count.tolist() if hasattr(self.count, "tolist") else self.count
        supports = self.support.tolist() if hasattr(self.support, "tolist") else self.support
        occupied = sum(1 for value in counts if value)
        spreads = sorted(self.finite_spreads())
        p95 = spreads[min(len(spreads) - 1, int((len(spreads) - 1) * 0.95))] if spreads else 0.0
        return {
            "width": self.width,
            "height": self.height,
            "occupiedCells": occupied,
            "occupancyRatio": rounded(occupied / self.cell_count),
            "maxPointsPerCell": int(max(counts, default=0)),
            "maxSourceSupport": int(max(supports, default=0)),
            "verticalSpreadP95": rounded(p95),
        }


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int((len(ordered) - 1) * fraction))]


def _grid_values(values: Any) -> list[float]:
    raw = values.tolist() if hasattr(values, "tolist") else values
    return [float(value) for value in raw]


def grid_png(grid: GridEvidence, values: list[float], path: Path, mode: str) -> None:
    canvas = scan_inspect.Canvas(grid.width, grid.height, (255, 255, 255))
    nonzero = [value for value in values if value > 0 and math.isfinite(value)]
    if mode == "density":
        scale = math.log1p(max(nonzero, default=1.0))
        color = lambda value: (255 - int(190 * math.log1p(max(0.0, value)) / scale), 255 - int(105 * math.log1p(max(0.0, value)) / scale), 255)
    elif mode == "spread":
        scale = max(percentile(nonzero, 0.95), 1e-9)
        color = lambda value: (255, 255 - int(200 * min(max(value, 0.0) / scale, 1.0)), 255 - int(245 * min(max(value, 0.0) / scale, 1.0)))
    elif mode == "support":
        maximum = max(nonzero, default=1.0)
        color = lambda value: (255 - int(210 * min(value / maximum, 1.0)), 255 - int(70 * min(value / maximum, 1.0)), 255 - int(20 * min(value / maximum, 1.0)))
    else:
        raise ValueError(mode)
    for flat, value in enumerate(values):
        if value <= 0 or not math.isfinite(value):
            continue
        x = flat % grid.width
        y = grid.height - 1 - flat // grid.width
        canvas.pixel(x, y, color(value))
    canvas.png(path)


def source_layout_png(report: dict[str, Any], path: Path) -> None:
    canvas = scan_inspect.Canvas(1000, 800, (248, 248, 248))
    plot = (45, 35, 790, 755)
    canvas.fill(*plot, (255, 255, 255))
    canvas.rect(*plot, (20, 20, 20), 3)
    bounds = report["globalBounds"]
    xmin, ymin = bounds["min"][:2]
    xmax, ymax = bounds["max"][:2]
    padx = max((xmax - xmin) * 0.03, 1.0)
    pady = max((ymax - ymin) * 0.03, 1.0)
    xmin -= padx; xmax += padx; ymin -= pady; ymax += pady
    px = lambda x: int(plot[0] + (x - xmin) / (xmax - xmin) * (plot[2] - plot[0]))
    py = lambda y: int(plot[3] - (y - ymin) / (ymax - ymin) * (plot[3] - plot[1]))
    for row, pair in enumerate(report["pairs"]):
        glb = pair["glbBounds"]
        ply = pair["plyBounds"]
        outline, fill = scan_inspect.PALETTE[row % len(scan_inspect.PALETTE)]
        gx0, gx1 = px(glb["min"][0]), px(glb["max"][0])
        gy0, gy1 = py(glb["max"][1]), py(glb["min"][1])
        canvas.fill(gx0, gy0, gx1, gy1, fill)
        canvas.rect(gx0, gy0, gx1, gy1, outline, 2)
        px0, px1 = px(ply["min"][0]), px(ply["max"][0])
        py0, py1 = py(ply["max"][1]), py(ply["min"][1])
        canvas.rect(px0, py0, px1, py1, (15, 15, 15), 2)
    canvas.png(path)


def alignment_png(report: dict[str, Any], path: Path) -> None:
    canvas = scan_inspect.Canvas(1000, 520, (250, 250, 250))
    pairs = report["pairs"]
    for row, pair in enumerate(pairs):
        y = 35 + row * 62
        center = min(pair["normalizedCenterDelta"] / 0.15, 1.0)
        extent = min(pair["maxExtentRelativeError"] / 0.30, 1.0)
        overlap = min(max(pair["xyOverlapOfSmaller"], 0.0), 1.0)
        canvas.fill(180, y, 180 + int(center * 230), y + 14, (220, 90, 90))
        canvas.fill(180, y + 18, 180 + int(extent * 230), y + 32, (230, 155, 65))
        canvas.fill(520, y, 520 + int(overlap * 400), y + 32, (75, 170, 105))
        canvas.rect(180, y, 410, y + 32, (30, 30, 30), 1)
        canvas.rect(520, y, 920, y + 32, (30, 30, 30), 1)
    canvas.png(path)


def markdown(report: dict[str, Any]) -> str:
    totals = report["totals"]
    lines = [
        f"# Scan dataset inspection — {report['packageName']}",
        "",
        "P1 evidence report. It is not a runtime import and does not approve terrain collision.",
        "",
        "## Gate status",
        "",
        f"- dataset status: `{report['datasetStatus']}`",
        f"- P2 unblocked: `{'yes' if report['p2Unblocked'] else 'no'}`",
        f"- scale confirmed: `{'yes' if report['scaleConfirmed'] else 'no'}`",
        f"- axes confirmed: `{'yes' if report['axesConfirmed'] else 'no'}`",
        "",
        "## Totals",
        "",
        "| Metric | Value |",
        "|---|---:|",
        f"| GLB files | {totals['glbFiles']} |",
        f"| PLY files | {totals['plyFiles']} |",
        f"| GLB vertices | {totals['glbVertices']} |",
        f"| GLB triangles | {totals['glbTriangles']} |",
        f"| PLY points | {totals['plyPoints']} |",
        "",
        "## GLB ↔ PLY pairs",
        "",
        "| Tile | GLB | PLY | Classification | Center Δ / diag | Max extent error | XY overlap |",
        "|---:|---|---|---|---:|---:|---:|",
    ]
    for pair in report["pairs"]:
        lines.append(
            f"| {pair['tileId']} | {pair['glbFile']} | {pair['plyFile']} | {pair['classification']} | "
            f"{pair['normalizedCenterDelta']:.6f} | {pair['maxExtentRelativeError']:.6f} | {pair['xyOverlapOfSmaller']:.6f} |"
        )
    lines += [
        "",
        "## Evidence grid",
        "",
        f"- dimensions: `{report['evidenceGrid']['width']} × {report['evidenceGrid']['height']}`",
        f"- occupied cells: `{report['evidenceGrid']['occupiedCells']}`",
        f"- max source support: `{report['evidenceGrid']['maxSourceSupport']}`",
        f"- vertical spread P95: `{report['evidenceGrid']['verticalSpreadP95']}`",
        "",
        "## Diagnostic artifacts",
        "",
        "- `source_layout.png`",
        "- `point_density.png`",
        "- `vertical_spread.png`",
        "- `source_support.png`",
        "- `glb_ply_alignment.png`",
        "- `inspection.json`",
        "",
        "All images are abstract evidence maps. No source texture or PLY RGB is reproduced.",
        "",
    ]
    if report["warnings"]:
        lines += ["## Warnings", ""] + [f"- {warning}" for warning in report["warnings"]] + [""]
    return "\n".join(lines)


def inspect_dataset(
    input_root: Path,
    *,
    package_name: str | None = None,
    expected_glb: int | None = None,
    expected_ply: int | None = None,
    grid_size: int = DEFAULT_GRID_SIZE,
    chunk_vertices: int = scan_ply.DEFAULT_CHUNK_VERTICES,
    prefer_numpy: bool = True,
) -> tuple[dict[str, Any], GridEvidence]:
    input_root = Path(input_root)
    glb_paths = collect_files(input_root, ".glb")
    ply_paths = collect_files(input_root, ".ply")
    if not glb_paths:
        raise DatasetInspectionError("no GLB files found")
    if not ply_paths:
        raise DatasetInspectionError("no PLY files found")
    if expected_glb is not None and len(glb_paths) != expected_glb:
        raise DatasetInspectionError(f"expected {expected_glb} GLB files, found {len(glb_paths)}")
    if expected_ply is not None and len(ply_paths) != expected_ply:
        raise DatasetInspectionError(f"expected {expected_ply} PLY files, found {len(ply_paths)}")

    glb_files = [inspect_glb_file(input_root, path) for path in glb_paths]
    ply_files = [
        inspect_ply_file(
            input_root,
            path,
            chunk_vertices=chunk_vertices,
            prefer_numpy=prefer_numpy,
        )
        for path in ply_paths
    ]
    glb_by_tile = unique_by_tile(glb_files, "GLB")
    ply_by_tile = unique_by_tile(ply_files, "PLY")
    if set(glb_by_tile) != set(ply_by_tile):
        missing_glb = sorted(set(ply_by_tile) - set(glb_by_tile))
        missing_ply = sorted(set(glb_by_tile) - set(ply_by_tile))
        raise DatasetInspectionError(
            f"unpaired tile ids; missing GLB={missing_glb}, missing PLY={missing_ply}"
        )

    comparisons = []
    for identifier in sorted(glb_by_tile):
        comparison = compare_pair(glb_by_tile[identifier], ply_by_tile[identifier])
        comparison["glbBounds"] = glb_by_tile[identifier]["worldBounds"]
        comparison["plyBounds"] = ply_by_tile[identifier]["bounds"]
        comparisons.append(comparison)

    if any(pair["classification"] == "incompatible" for pair in comparisons):
        dataset_status = "incompatible"
    elif any(pair["classification"] == "review" for pair in comparisons):
        dataset_status = "compatible-review"
    else:
        dataset_status = "compatible"

    glb_bounds = union_bounds(glb_files, "worldBounds")
    ply_bounds = union_bounds(ply_files, "bounds")
    global_bounds = {
        "min": vector([min(glb_bounds["min"][i], ply_bounds["min"][i]) for i in range(3)]),
        "max": vector([max(glb_bounds["max"][i], ply_bounds["max"][i]) for i in range(3)]),
    }
    global_bounds["extent"] = vector([global_bounds["max"][i] - global_bounds["min"][i] for i in range(3)])
    global_bounds["center"] = vector([(global_bounds["min"][i] + global_bounds["max"][i]) * 0.5 for i in range(3)])

    grid = GridEvidence(ply_bounds, grid_size)
    for path in ply_paths:
        grid.add_source(path, chunk_vertices=chunk_vertices, prefer_numpy=prefer_numpy)

    warnings = []
    if dataset_status == "compatible-review":
        warnings.append("One or more GLB/PLY pairs require manual bounds review before P2.")
    if dataset_status == "incompatible":
        warnings.append("At least one GLB/PLY pair is spatially incompatible; P2 remains blocked.")
    if not any(file["hasNormals"] for file in glb_files):
        warnings.append("GLB sources contain no NORMAL attributes; visual normals must be generated later.")
    warnings.append("Scale and axis orientation still require a known-distance and visual confirmation.")

    totals = {
        "glbFiles": len(glb_files),
        "plyFiles": len(ply_files),
        "glbBytes": sum(int(file["byteLength"]) for file in glb_files),
        "plyBytes": sum(int(file["byteLength"]) for file in ply_files),
        "glbVertices": sum(int(file["vertexCount"]) for file in glb_files),
        "glbTriangles": sum(int(file["triangleCount"]) for file in glb_files),
        "plyPoints": sum(int(file["vertexCount"]) for file in ply_files),
    }
    report = {
        "schema": SCHEMA,
        "schemaVersion": SCHEMA_VERSION,
        "packageName": safe_label(package_name or input_root.stem),
        "datasetStatus": dataset_status,
        "p2Unblocked": False,
        "scaleConfirmed": False,
        "axesConfirmed": False,
        "axisHypothesis": {"horizontal": ["X", "Y"], "up": "Z"},
        "totals": totals,
        "globalBounds": global_bounds,
        "glbBounds": glb_bounds,
        "plyBounds": ply_bounds,
        "pairs": comparisons,
        "glbFiles": glb_files,
        "plyFiles": ply_files,
        "evidenceGrid": grid.summary(),
        "warnings": warnings,
        "privacy": {
            "sourceRgbRendered": False,
            "plyCommentsCopied": False,
            "absolutePathsIncluded": False,
            "georeferencingIncluded": False,
        },
    }
    return report, grid


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_outputs(report: dict[str, Any], grid: GridEvidence, output: Path) -> dict[str, str]:
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    json_path = output / "inspection.json"
    json_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    (output / "inspection.md").write_text(markdown(report), encoding="utf-8", newline="\n")
    source_layout_png(report, output / "source_layout.png")
    alignment_png(report, output / "glb_ply_alignment.png")
    counts = _grid_values(grid.count)
    supports = _grid_values(grid.support)
    zmin = _grid_values(grid.zmin)
    zmax = _grid_values(grid.zmax)
    spreads = [zmax[i] - zmin[i] if counts[i] > 0 else 0.0 for i in range(grid.cell_count)]
    grid_png(grid, counts, output / "point_density.png", "density")
    grid_png(grid, spreads, output / "vertical_spread.png", "spread")
    grid_png(grid, supports, output / "source_support.png", "support")
    names = [
        "inspection.json",
        "inspection.md",
        "source_layout.png",
        "point_density.png",
        "vertical_spread.png",
        "source_support.png",
        "glb_ply_alignment.png",
    ]
    return {name: sha256_file(output / name) for name in names}


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--name")
    parser.add_argument("--expected-glb", type=int)
    parser.add_argument("--expected-ply", type=int)
    parser.add_argument("--grid-size", type=int, default=DEFAULT_GRID_SIZE)
    parser.add_argument("--chunk-vertices", type=int, default=scan_ply.DEFAULT_CHUNK_VERTICES)
    parser.add_argument("--no-numpy", action="store_true")
    args = parser.parse_args(argv)
    try:
        report, grid = inspect_dataset(
            args.input,
            package_name=args.name,
            expected_glb=args.expected_glb,
            expected_ply=args.expected_ply,
            grid_size=args.grid_size,
            chunk_vertices=args.chunk_vertices,
            prefer_numpy=not args.no_numpy,
        )
        hashes = write_outputs(report, grid, args.output)
    except (OSError, DatasetInspectionError, scan_inspect.ScanInspectionError, scan_ply.PlyInspectionError) as exc:
        print(f"scan_dataset_inspect: ERROR: {exc}", file=sys.stderr)
        return 2
    totals = report["totals"]
    print(
        "scan_dataset_inspect: OK | "
        f"status={report['datasetStatus']} glb={totals['glbFiles']} ply={totals['plyFiles']} "
        f"points={totals['plyPoints']} triangles={totals['glbTriangles']} "
        f"inspection_sha256={hashes['inspection.json']}"
    )
    return 3 if report["datasetStatus"] == "incompatible" else 0


if __name__ == "__main__":
    raise SystemExit(main())
