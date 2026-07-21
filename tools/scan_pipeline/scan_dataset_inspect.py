#!/usr/bin/env python3
"""P1 dataset inspector for paired photogrammetry GLB and PLY sources.

The tool combines evidence only. It does not cook geometry, classify ground or
modify source data. Output images are abstract diagnostic maps and never RGB
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
    module_name = f"_jozz_dataset_{name}"
    spec = importlib.util.spec_from_file_location(module_name, path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


scan_inspect = _load_sibling("scan_inspect")
scan_ply = _load_sibling("scan_ply")
scan_glb_quality = _load_sibling("scan_glb_quality")
_np = scan_ply._np

SCHEMA = "jozz.scan-dataset-inspection"
SCHEMA_VERSION = 3
REVIEW_SCHEMA = "jozz.scan-p1-review"
REVIEW_SCHEMA_VERSION = 1
DEFAULT_GRID_SIZE = 256
TILE_PATTERN = re.compile(r"(?i)(?:mip)?tile[_-]?(\d+)")
AXIS_NAMES = ("X", "Y", "Z")
PALETTE = (
    ((58, 120, 180), (202, 222, 240)),
    ((230, 126, 34), (248, 220, 196)),
    ((46, 160, 90), (199, 235, 213)),
    ((160, 75, 180), (226, 205, 235)),
    ((190, 55, 70), (241, 203, 208)),
    ((100, 105, 110), (220, 222, 224)),
    ((35, 155, 165), (194, 231, 233)),
    ((210, 165, 35), (244, 231, 190)),
    ((95, 80, 195), (213, 207, 241)),
    ((205, 95, 155), (242, 207, 228)),
    ((90, 145, 45), (216, 235, 200)),
    ((185, 95, 40), (238, 211, 194)),
)


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
    cleaned = "".join(char if char.isalnum() or char in "-_." else "_" for char in label)
    return cleaned[:64] or "scan-dataset"


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


def tile_id_from_path(path: Path) -> int:
    match = TILE_PATTERN.fullmatch(path.stem)
    if not match:
        raise DatasetInspectionError(
            f"cannot derive a strict Tile ID from {path.name}; expected MipTile_N"
        )
    return int(match.group(1))


def canonical_source_label(tile_id: int, suffix: str) -> str:
    return f"MipTile_{tile_id}{suffix.lower()}"


def inspect_glb_file(
    path: Path,
    *,
    prefer_numpy: bool,
    triangle_chunk: int,
) -> dict[str, Any]:
    identifier = tile_id_from_path(path)
    data = path.read_bytes()
    result = scan_glb_quality.inspect_glb_quality(
        data,
        canonical_source_label(identifier, ".glb"),
        prefer_numpy=prefer_numpy,
        triangle_chunk=triangle_chunk,
    )
    result["tileId"] = identifier
    return result


def inspect_ply_file(
    path: Path,
    *,
    chunk_vertices: int,
    prefer_numpy: bool,
) -> dict[str, Any]:
    identifier = tile_id_from_path(path)
    raw = scan_ply.inspect_ply(
        path,
        canonical_source_label(identifier, ".ply"),
        chunk_vertices=chunk_vertices,
        prefer_numpy=prefer_numpy,
    )
    return {
        "tileId": identifier,
        "sourceLabel": canonical_source_label(identifier, ".ply"),
        "byteLength": int(raw["byteLength"]),
        "sha256": raw["sha256"],
        "format": raw["format"],
        "vertexCount": int(raw["vertexCount"]),
        "vertexStride": int(raw["vertexStride"]),
        "properties": list(raw["properties"]),
        "hasRgb": bool(raw["hasRgb"]),
        "bounds": raw["bounds"],
        "meanDensityXY": raw["meanDensityXY"],
        "streaming": dict(raw["streaming"]),
        "fileStableDuringInspection": bool(raw["fileStableDuringInspection"]),
    }


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


def _extent_error(reference: Sequence[float], candidate: Sequence[float]) -> list[float]:
    return [
        abs(float(candidate[i]) - float(reference[i]))
        / max(abs(float(candidate[i])), abs(float(reference[i])), 1.0e-9)
        for i in range(3)
    ]


def _best_extent_permutation(
    glb_extent: Sequence[float],
    ply_extent: Sequence[float],
) -> tuple[tuple[int, int, int], float]:
    best = (0, 1, 2)
    best_error = math.inf
    for permutation in itertools.permutations(range(3)):
        candidate = [float(ply_extent[permutation[i]]) for i in range(3)]
        error = max(_extent_error(glb_extent, candidate))
        if error < best_error - 1.0e-12:
            best = permutation
            best_error = error
    return best, best_error


def compare_pair(glb: dict[str, Any], ply: dict[str, Any]) -> dict[str, Any]:
    gb = glb["worldBounds"]
    pb = ply["bounds"]
    center_delta = [pb["center"][i] - gb["center"][i] for i in range(3)]
    extent_error = _extent_error(gb["extent"], pb["extent"])
    diagonal = math.sqrt(sum(float(value) * float(value) for value in gb["extent"]))
    normalized_center_delta = math.sqrt(
        sum(float(value) * float(value) for value in center_delta)
    ) / max(diagonal, 1.0)
    overlap_area, overlap_ratio = xy_overlap(gb, pb)
    max_extent_error = max(extent_error)
    best_permutation, best_permutation_error = _best_extent_permutation(
        gb["extent"], pb["extent"]
    )
    axis_permutation_suspicion = (
        best_permutation != (0, 1, 2)
        and best_permutation_error + 0.05 < max_extent_error
    )
    spatially_plausible = (
        normalized_center_delta <= 0.15
        and overlap_ratio >= 0.40
    )

    if (
        normalized_center_delta <= 0.02
        and max_extent_error <= 0.05
        and overlap_ratio >= 0.90
        and not axis_permutation_suspicion
    ):
        classification = "strong-match"
        reason = "centers, extents and XY coverage agree tightly"
    elif spatially_plausible and (
        max_extent_error <= 0.30
        or (axis_permutation_suspicion and best_permutation_error <= 0.30)
    ):
        classification = "review"
        reason = (
            "a different extent-axis permutation fits materially better"
            if axis_permutation_suspicion
            else "same spatial tile, but bounds differ enough to require manual review"
        )
    else:
        classification = "incompatible"
        reason = "bounds do not provide sufficient evidence of a shared spatial tile"

    return {
        "tileId": int(glb["tileId"]),
        "glbSource": glb["sourceLabel"],
        "plySource": ply["sourceLabel"],
        "classification": classification,
        "reason": reason,
        "centerDelta": vector(center_delta),
        "normalizedCenterDelta": rounded(normalized_center_delta),
        "extentRelativeError": vector(extent_error),
        "maxExtentRelativeError": rounded(max_extent_error),
        "xyOverlapArea": rounded(overlap_area),
        "xyOverlapOfSmaller": rounded(overlap_ratio),
        "bestExtentAxisPermutation": [AXIS_NAMES[index] for index in best_permutation],
        "bestExtentPermutationError": rounded(best_permutation_error),
        "axisPermutationSuspicion": axis_permutation_suspicion,
        "spatiallyPlausible": spatially_plausible,
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
    def __init__(
        self,
        bounds: dict[str, list[float]],
        size: int,
        *,
        prefer_numpy: bool,
    ):
        use_numpy = prefer_numpy and _np is not None
        maximum_size = 2048 if use_numpy else 512
        if not 16 <= size <= maximum_size:
            raise DatasetInspectionError(
                f"grid size must be between 16 and {maximum_size} for this backend"
            )
        self.use_numpy = use_numpy
        self.backend = "numpy" if use_numpy else "stdlib"
        self.bounds = bounds
        self.width = self.height = int(size)
        self.cell_count = self.width * self.height
        self.xmin, self.ymin = bounds["min"][:2]
        self.xmax, self.ymax = bounds["max"][:2]
        self.xextent = self.xmax - self.xmin
        self.yextent = self.ymax - self.ymin
        self.points_accumulated = 0
        if self.xextent <= 0 or self.yextent <= 0:
            raise DatasetInspectionError("dataset has zero XY extent")
        if use_numpy:
            assert _np is not None
            self.count = _np.zeros(self.cell_count, dtype=_np.uint64)
            self.zmin = _np.full(self.cell_count, _np.inf, dtype=_np.float64)
            self.zmax = _np.full(self.cell_count, -_np.inf, dtype=_np.float64)
            self.support = _np.zeros(self.cell_count, dtype=_np.uint32)
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

    def add_source(self, path: Path, *, chunk_vertices: int) -> None:
        header = scan_ply.read_ply_header(path)
        seen = _np.zeros(self.cell_count, dtype=bool) if self.use_numpy else set()
        source_points = 0
        for chunk in scan_ply.iter_vertex_chunks(
            path,
            header,
            chunk_vertices=chunk_vertices,
            prefer_numpy=self.use_numpy,
        ):
            source_points += len(chunk)
            if self.use_numpy:
                assert _np is not None
                flat, z = self._flat_numpy(chunk)
                self.count += _np.bincount(flat, minlength=self.cell_count).astype(_np.uint64)
                _np.minimum.at(self.zmin, flat, z)
                _np.maximum.at(self.zmax, flat, z)
                seen[_np.unique(flat)] = True
            else:
                for x, y, z in zip(chunk.x, chunk.y, chunk.z):
                    ix = math.floor((float(x) - self.xmin) / self.xextent * self.width)
                    iy = math.floor((float(y) - self.ymin) / self.yextent * self.height)
                    ix = min(self.width - 1, max(0, ix))
                    iy = min(self.height - 1, max(0, iy))
                    flat = iy * self.width + ix
                    self.count[flat] += 1
                    self.zmin[flat] = min(self.zmin[flat], float(z))
                    self.zmax[flat] = max(self.zmax[flat], float(z))
                    seen.add(flat)
        if source_points != header.vertex_count:
            raise DatasetInspectionError("evidence grid point count differs from PLY header")
        self.points_accumulated += source_points
        if self.use_numpy:
            self.support += seen.astype(self.support.dtype)
        else:
            for flat in seen:
                self.support[flat] += 1

    def finite_spreads(self) -> list[float]:
        if self.use_numpy:
            valid = self.count > 0
            return [float(value) for value in (self.zmax[valid] - self.zmin[valid]).tolist()]
        return [
            float(self.zmax[index] - self.zmin[index])
            for index in range(self.cell_count)
            if self.count[index] > 0
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
            "backend": self.backend,
            "pointsAccumulated": int(self.points_accumulated),
            "occupiedCells": occupied,
            "occupancyRatio": rounded(occupied / self.cell_count),
            "maxPointsPerCell": int(max(counts, default=0)),
            "maxSourceSupport": int(max(supports, default=0)),
            "verticalSpreadP95SourceUnits": rounded(p95),
        }


def _finite_point(value: Any, label: str) -> list[float]:
    if not isinstance(value, list) or len(value) != 3:
        raise DatasetInspectionError(f"{label} must be a three-value array")
    point = [float(component) for component in value]
    if not all(math.isfinite(component) for component in point):
        raise DatasetInspectionError(f"{label} contains non-finite values")
    return point


def evaluate_review_contract(
    contract: dict[str, Any] | None,
    contract_sha256: str | None,
    pairs: Sequence[dict[str, Any]],
) -> dict[str, Any]:
    required_review_ids = sorted(
        int(pair["tileId"])
        for pair in pairs
        if pair["classification"] == "review"
    )
    result = {
        "present": contract is not None,
        "sha256": contract_sha256,
        "ownerApproved": False,
        "scaleConfirmed": False,
        "axesConfirmed": False,
        "reviewPairsApproved": not required_review_ids,
        "requiredReviewTileIds": required_review_ids,
        "approvedReviewTileIds": [],
        "sourceUnitsPerMeter": None,
        "expectedSourceUnitsPerMeter": None,
        "scaleRelativeError": None,
    }
    if contract is None:
        return result
    if not isinstance(contract, dict):
        raise DatasetInspectionError("review contract must be a JSON object")
    if contract.get("schema") != REVIEW_SCHEMA or int(contract.get("schemaVersion", 0)) != REVIEW_SCHEMA_VERSION:
        raise DatasetInspectionError("unknown P1 review contract schema or version")

    owner_approved = contract.get("ownerApproved") is True
    known = contract.get("knownDistance")
    if not isinstance(known, dict):
        raise DatasetInspectionError("review contract requires knownDistance")
    point_a = _finite_point(known.get("pointA"), "knownDistance.pointA")
    point_b = _finite_point(known.get("pointB"), "knownDistance.pointB")
    meters = float(known.get("meters", 0.0))
    expected_units = float(known.get("expectedSourceUnitsPerMeter", 1.0))
    tolerance = float(known.get("maxRelativeError", 0.02))
    if not math.isfinite(meters) or meters <= 0:
        raise DatasetInspectionError("knownDistance.meters must be positive")
    if not math.isfinite(expected_units) or expected_units <= 0:
        raise DatasetInspectionError("expectedSourceUnitsPerMeter must be positive")
    if not math.isfinite(tolerance) or not 0 <= tolerance <= 0.25:
        raise DatasetInspectionError("maxRelativeError must be between 0 and 0.25")
    source_distance = math.sqrt(
        sum((point_b[index] - point_a[index]) ** 2 for index in range(3))
    )
    if source_distance <= 0:
        raise DatasetInspectionError("known-distance source points must differ")
    measured_units = source_distance / meters
    scale_error = abs(measured_units - expected_units) / expected_units

    axes = contract.get("axes")
    if not isinstance(axes, dict):
        raise DatasetInspectionError("review contract requires axes")
    horizontal = axes.get("horizontal")
    up = axes.get("up")
    axes_declared = horizontal == ["X", "Y"] and up == "Z" and axes.get("confirmed") is True

    approved_ids_raw = contract.get("approvedReviewTileIds", [])
    if not isinstance(approved_ids_raw, list) or not all(isinstance(value, int) for value in approved_ids_raw):
        raise DatasetInspectionError("approvedReviewTileIds must be an integer array")
    approved_ids = sorted(set(int(value) for value in approved_ids_raw))
    known_ids = {int(pair["tileId"]) for pair in pairs}
    unknown_ids = sorted(set(approved_ids) - known_ids)
    if unknown_ids:
        raise DatasetInspectionError(f"review contract references unknown Tile IDs: {unknown_ids}")

    result.update(
        {
            "ownerApproved": owner_approved,
            "scaleConfirmed": owner_approved and scale_error <= tolerance,
            "axesConfirmed": owner_approved and axes_declared,
            "reviewPairsApproved": owner_approved and set(required_review_ids).issubset(approved_ids),
            "approvedReviewTileIds": approved_ids,
            "sourceUnitsPerMeter": rounded(measured_units),
            "expectedSourceUnitsPerMeter": rounded(expected_units),
            "scaleRelativeError": rounded(scale_error),
            "scaleTolerance": rounded(tolerance),
            "knownDistanceMeters": rounded(meters),
        }
    )
    return result


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
        color = lambda value: (
            255 - int(190 * math.log1p(max(0.0, value)) / scale),
            255 - int(105 * math.log1p(max(0.0, value)) / scale),
            255,
        )
    elif mode == "spread":
        scale = max(percentile(nonzero, 0.95), 1.0e-9)
        color = lambda value: (
            255,
            255 - int(200 * min(max(value, 0.0) / scale, 1.0)),
            255 - int(245 * min(max(value, 0.0) / scale, 1.0)),
        )
    elif mode == "support":
        maximum = max(nonzero, default=1.0)
        color = lambda value: (
            255 - int(210 * min(value / maximum, 1.0)),
            255 - int(70 * min(value / maximum, 1.0)),
            255 - int(20 * min(value / maximum, 1.0)),
        )
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
    xmin -= padx
    xmax += padx
    ymin -= pady
    ymax += pady
    px = lambda x: int(plot[0] + (x - xmin) / (xmax - xmin) * (plot[2] - plot[0]))
    py = lambda y: int(plot[3] - (y - ymin) / (ymax - ymin) * (plot[3] - plot[1]))
    for row, pair in enumerate(report["pairs"]):
        glb = pair["glbBounds"]
        ply = pair["plyBounds"]
        outline, fill = PALETTE[row % len(PALETTE)]
        gx0, gx1 = px(glb["min"][0]), px(glb["max"][0])
        gy0, gy1 = py(glb["max"][1]), py(glb["min"][1])
        canvas.fill(gx0, gy0, gx1, gy1, fill)
        canvas.rect(gx0, gy0, gx1, gy1, outline, 2)
        px0, px1 = px(ply["min"][0]), px(ply["max"][0])
        py0, py1 = py(ply["max"][1]), py(ply["min"][1])
        canvas.rect(px0, py0, px1, py1, (15, 15, 15), 2)
    canvas.png(path)


def alignment_png(report: dict[str, Any], path: Path) -> None:
    height = max(220, 70 + len(report["pairs"]) * 62)
    canvas = scan_inspect.Canvas(1000, height, (250, 250, 250))
    for row, pair in enumerate(report["pairs"]):
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
    quality = report["geometryQuality"]
    gate = report["p2Gate"]
    lines = [
        f"# Scan dataset inspection — {report['packageName']}",
        "",
        "P1 evidence report. It is not a runtime import and does not approve terrain collision.",
        "",
        "## Gate status",
        "",
        f"- dataset status: `{report['datasetStatus']}`",
        f"- P2 unblocked: `{'yes' if report['p2Unblocked'] else 'no'}`",
        f"- scale confirmed: `{'yes' if gate['scaleConfirmed'] else 'no'}`",
        f"- axes confirmed: `{'yes' if gate['axesConfirmed'] else 'no'}`",
        f"- review pairs approved: `{'yes' if gate['reviewPairsApproved'] else 'no'}`",
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
        f"| Degenerate triangles | {quality['degenerateTriangleCount']} |",
        f"| Provisional long-edge triangles | {quality['provisionalLargeTriangleCount']} |",
        "",
        "## GLB ↔ PLY pairs",
        "",
        "| Tile | GLB | PLY | Classification | Center Δ / diag | Max extent error | XY overlap |",
        "|---:|---|---|---|---:|---:|---:|",
    ]
    for pair in report["pairs"]:
        lines.append(
            f"| {pair['tileId']} | {pair['glbSource']} | {pair['plySource']} | {pair['classification']} | "
            f"{pair['normalizedCenterDelta']:.6f} | {pair['maxExtentRelativeError']:.6f} | {pair['xyOverlapOfSmaller']:.6f} |"
        )
    lines += [
        "",
        "## Evidence grid",
        "",
        f"- dimensions: `{report['evidenceGrid']['width']} × {report['evidenceGrid']['height']}`",
        f"- backend: `{report['evidenceGrid']['backend']}`",
        f"- points accumulated: `{report['evidenceGrid']['pointsAccumulated']}`",
        f"- occupied cells: `{report['evidenceGrid']['occupiedCells']}`",
        f"- max source support: `{report['evidenceGrid']['maxSourceSupport']}`",
        f"- vertical spread P95: `{report['evidenceGrid']['verticalSpreadP95SourceUnits']}` source units",
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
    triangle_chunk: int = scan_glb_quality.TRIANGLE_CHUNK,
    prefer_numpy: bool = True,
    review_contract: dict[str, Any] | None = None,
    review_contract_sha256: str | None = None,
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

    glb_files = [
        inspect_glb_file(
            path,
            prefer_numpy=prefer_numpy,
            triangle_chunk=triangle_chunk,
        )
        for path in glb_paths
    ]
    ply_files = [
        inspect_ply_file(
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
    global_bounds["extent"] = vector(
        [global_bounds["max"][i] - global_bounds["min"][i] for i in range(3)]
    )
    global_bounds["center"] = vector(
        [(global_bounds["min"][i] + global_bounds["max"][i]) * 0.5 for i in range(3)]
    )

    grid = GridEvidence(ply_bounds, grid_size, prefer_numpy=prefer_numpy)
    for path in ply_paths:
        grid.add_source(path, chunk_vertices=chunk_vertices)

    review = evaluate_review_contract(
        review_contract,
        review_contract_sha256,
        comparisons,
    )
    p2_unblocked = (
        dataset_status != "incompatible"
        and review["scaleConfirmed"]
        and review["axesConfirmed"]
        and review["reviewPairsApproved"]
    )

    aggregate_edge_counts = scan_glb_quality._empty_edge_threshold_counts()
    for file in glb_files:
        for key, value in file["geometryQuality"]["edgeThresholdCountsSourceUnits"].items():
            aggregate_edge_counts[key] += int(value)
    geometry_quality = {
        "triangleCountAnalyzed": sum(
            int(file["geometryQuality"]["triangleCountAnalyzed"])
            for file in glb_files
        ),
        "degenerateTriangleCount": sum(
            int(file["geometryQuality"]["degenerateTriangleCount"])
            for file in glb_files
        ),
        "provisionalLargeTriangleCount": sum(
            int(file["geometryQuality"]["provisionalLargeTriangleCount"])
            for file in glb_files
        ),
        "provisionalLargeEdgeThresholdSourceUnits": scan_glb_quality.PROVISIONAL_LARGE_EDGE_SOURCE_UNITS,
        "edgeThresholdCountsSourceUnits": aggregate_edge_counts,
        "maxTriangleEdgeSourceUnits": rounded(
            max(
                (float(file["geometryQuality"]["maxTriangleEdgeSourceUnits"]) for file in glb_files),
                default=0.0,
            )
        ),
        "maxTriangleAreaSourceUnitsSquared": rounded(
            max(
                (float(file["geometryQuality"]["maxTriangleAreaSourceUnitsSquared"]) for file in glb_files),
                default=0.0,
            )
        ),
    }

    warnings = []
    if dataset_status == "compatible-review":
        warnings.append("One or more GLB/PLY pairs require explicit owner review before P2.")
    if dataset_status == "incompatible":
        warnings.append("At least one GLB/PLY pair is spatially incompatible; P2 remains blocked.")
    if any(pair["axisPermutationSuspicion"] for pair in comparisons):
        warnings.append("At least one spatially plausible pair has a materially better non-XYZ extent permutation.")
    for file in glb_files:
        warnings.extend(f"{file['sourceLabel']}: {warning}" for warning in file["warnings"])
    if review_contract is None:
        warnings.append("No P1 review contract was supplied; scale, axes and review approvals remain unconfirmed.")
    elif not p2_unblocked:
        warnings.append("The P1 review contract is present but does not satisfy every P2 gate.")

    totals = {
        "glbFiles": len(glb_files),
        "plyFiles": len(ply_files),
        "glbBytes": sum(int(file["byteLength"]) for file in glb_files),
        "plyBytes": sum(int(file["byteLength"]) for file in ply_files),
        "glbVertices": sum(int(file["vertexCount"]) for file in glb_files),
        "glbTriangles": sum(int(file["triangleCount"]) for file in glb_files),
        "plyPoints": sum(int(file["vertexCount"]) for file in ply_files),
    }
    if grid.points_accumulated != totals["plyPoints"]:
        raise DatasetInspectionError("evidence grid total differs from inspected PLY point total")
    if geometry_quality["triangleCountAnalyzed"] != totals["glbTriangles"]:
        warnings.append("Not every GLB triangle was analyzed; inspect primitive modes before P2.")

    report = {
        "schema": SCHEMA,
        "schemaVersion": SCHEMA_VERSION,
        "packageName": safe_label(package_name or "scan-dataset"),
        "datasetStatus": dataset_status,
        "p2Unblocked": p2_unblocked,
        "scaleConfirmed": bool(review["scaleConfirmed"]),
        "axesConfirmed": bool(review["axesConfirmed"]),
        "axisHypothesis": {"horizontal": ["X", "Y"], "up": "Z"},
        "p2Gate": review,
        "totals": totals,
        "geometryQuality": geometry_quality,
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
            "sourceTexturesRendered": False,
            "plyCommentsCopied": False,
            "absolutePathsIncluded": False,
            "originalSourceNamesIncluded": False,
            "georeferencingIncluded": False,
            "canonicalSourceLabelsOnly": True,
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
    spreads = [zmax[index] - zmin[index] if counts[index] > 0 else 0.0 for index in range(grid.cell_count)]
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


def _load_review_contract(path: Path | None) -> tuple[dict[str, Any] | None, str | None]:
    if path is None:
        return None, None
    raw = Path(path).read_bytes()
    try:
        contract = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise DatasetInspectionError(f"invalid review contract JSON: {exc}") from exc
    return contract, hashlib.sha256(raw).hexdigest()


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--name")
    parser.add_argument("--expected-glb", type=int)
    parser.add_argument("--expected-ply", type=int)
    parser.add_argument("--grid-size", type=int, default=DEFAULT_GRID_SIZE)
    parser.add_argument("--chunk-vertices", type=int, default=scan_ply.DEFAULT_CHUNK_VERTICES)
    parser.add_argument("--triangle-chunk", type=int, default=scan_glb_quality.TRIANGLE_CHUNK)
    parser.add_argument("--review-contract", type=Path)
    parser.add_argument("--require-p2-ready", action="store_true")
    parser.add_argument("--no-numpy", action="store_true")
    args = parser.parse_args(argv)
    try:
        review_contract, review_sha = _load_review_contract(args.review_contract)
        report, grid = inspect_dataset(
            args.input,
            package_name=args.name,
            expected_glb=args.expected_glb,
            expected_ply=args.expected_ply,
            grid_size=args.grid_size,
            chunk_vertices=args.chunk_vertices,
            triangle_chunk=args.triangle_chunk,
            prefer_numpy=not args.no_numpy,
            review_contract=review_contract,
            review_contract_sha256=review_sha,
        )
        hashes = write_outputs(report, grid, args.output)
    except (
        OSError,
        DatasetInspectionError,
        scan_inspect.ScanInspectionError,
        scan_ply.PlyInspectionError,
        scan_glb_quality.GlbQualityError,
        scan_glb_quality.scan_inspect.ScanInspectionError,
    ) as exc:
        print(f"scan_dataset_inspect: ERROR: {exc}", file=sys.stderr)
        return 2
    totals = report["totals"]
    print(
        "scan_dataset_inspect: OK | "
        f"status={report['datasetStatus']} p2_ready={report['p2Unblocked']} "
        f"glb={totals['glbFiles']} ply={totals['plyFiles']} "
        f"points={totals['plyPoints']} triangles={totals['glbTriangles']} "
        f"inspection_sha256={hashes['inspection.json']}"
    )
    if report["datasetStatus"] == "incompatible":
        return 3
    if args.require_p2_ready and not report["p2Unblocked"]:
        return 4
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
