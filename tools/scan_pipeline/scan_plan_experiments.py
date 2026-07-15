#!/usr/bin/env python3
"""Cross-roadmap experiments for photogrammetry import planning.

This is deliberately an offline validation harness, not production cooking code.
It touches several roadmap risks with real data:
- independent parser/count/bounds cross-check;
- pairwise seam metrics in the Golden Seam Region;
- multi-source top/bottom/continuity DEM hypotheses at 0.50 m and 0.25 m;
- exact shared-edge heightfield splitting with one global quantization range;
- actual 1K/2K texture resize profiles.

Dependencies: numpy, scipy, Pillow. trimesh is optional for parser cross-check.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import heapq
import importlib.util
import io
import json
import math
from pathlib import Path
import time
from typing import Any, Iterable, Sequence

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage
from scipy.spatial import cKDTree

from scan_geometry import (
    deterministic_sample,
    load_region_sources,
    scan_inspect,
    triangle_normals,
)
from scan_ground_filters import (
    DEFAULT_PROFILES,
    export_box3d_obj,
    fill_nearest as filter_fill_nearest,
    profile_metrics as ground_profile_metrics,
    progressive_morphological_ground,
)

GOLDEN_REGION = (-32.0, -32.0, 48.0, 48.0)
SOURCE_COLORS = [
    (48, 110, 190),
    (230, 120, 30),
    (40, 160, 90),
    (150, 80, 180),
    (190, 55, 70),
]


class ExperimentError(RuntimeError):
    pass


def stable_json(data: Any) -> bytes:
    return (json.dumps(data, indent=2, sort_keys=True, ensure_ascii=False) + "\n").encode("utf-8")


def sha256_file(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def percentiles(values: np.ndarray) -> dict[str, float | int | None]:
    values = np.asarray(values, dtype=np.float64)
    values = values[np.isfinite(values)]
    if not len(values):
        return {"count": 0, "median": None, "p90": None, "p95": None, "max": None}
    p = np.percentile(values, [50, 90, 95, 100])
    return {
        "count": int(len(values)),
        "median": float(p[0]),
        "p90": float(p[1]),
        "p95": float(p[2]),
        "max": float(p[3]),
    }


def source_short(name: str) -> str:
    return Path(name).name


def candidate_points(triangles: np.ndarray, maximum: int = 50000) -> np.ndarray:
    if not len(triangles):
        return np.empty((0, 3), dtype=np.float64)
    normals, areas = triangle_normals(triangles)
    slope = np.degrees(np.arccos(np.clip(np.abs(normals[:, 2]), 0.0, 1.0)))
    keep = (areas > 1e-7) & (slope <= 75.0)
    centroids = triangles[keep].mean(axis=1)
    return deterministic_sample(centroids, maximum)


def pairwise_seam_metrics(sources: dict[str, np.ndarray]) -> tuple[list[dict[str, Any]], dict[str, np.ndarray]]:
    """Measure only real XY overlap/contact corridors, not unrelated quadrant interiors."""
    clouds = {name: candidate_points(triangles) for name, triangles in sources.items()}
    metrics: list[dict[str, Any]] = []
    residual_samples: dict[str, np.ndarray] = {}
    names = sorted(clouds)
    corridor_margin = 1.0
    for first_index, first in enumerate(names):
        for second in names[first_index + 1:]:
            a, b = clouds[first], clouds[second]
            if not len(a) or not len(b):
                continue
            a_min, a_max = a[:, :2].min(axis=0), a[:, :2].max(axis=0)
            b_min, b_max = b[:, :2].min(axis=0), b[:, :2].max(axis=0)
            overlap_min = np.maximum(a_min, b_min)
            overlap_max = np.minimum(a_max, b_max)
            overlap_extent = overlap_max - overlap_min
            if np.all(overlap_extent > 0.0):
                topology = "area-overlap"
            elif np.all(overlap_extent >= -corridor_margin):
                topology = "edge-or-corner-contact"
            else:
                topology = "disjoint-in-region"

            corridor_min = overlap_min - corridor_margin
            corridor_max = overlap_max + corridor_margin
            a_mask = np.all((a[:, :2] >= corridor_min) & (a[:, :2] <= corridor_max), axis=1)
            b_mask = np.all((b[:, :2] >= corridor_min) & (b[:, :2] <= corridor_max), axis=1)
            a_corridor, b_corridor = a[a_mask], b[b_mask]
            pair_name = f"{source_short(first)}__{source_short(second)}"
            if not len(a_corridor) or not len(b_corridor):
                residual_samples[pair_name] = np.empty((0, 3), dtype=np.float64)
                metrics.append({
                    "pair": [source_short(first), source_short(second)],
                    "topology": topology,
                    "xyOverlapExtent": overlap_extent.tolist(),
                    "corridorSampleCounts": [int(len(a_corridor)), int(len(b_corridor))],
                    "status": "no-comparable-corridor-samples",
                })
                continue

            tree_b_xy = cKDTree(b_corridor[:, :2])
            xy_distance_ab, indices_ab = tree_b_xy.query(a_corridor[:, :2], k=1, workers=-1)
            accepted_ab = xy_distance_ab <= 1.0
            dz_ab = np.abs(a_corridor[accepted_ab, 2] - b_corridor[indices_ab[accepted_ab], 2])
            distance3d_ab = np.linalg.norm(
                a_corridor[accepted_ab] - b_corridor[indices_ab[accepted_ab]], axis=1
            ) if np.any(accepted_ab) else np.empty(0)

            tree_a_xy = cKDTree(a_corridor[:, :2])
            xy_distance_ba, indices_ba = tree_a_xy.query(b_corridor[:, :2], k=1, workers=-1)
            accepted_ba = xy_distance_ba <= 1.0
            dz_ba = np.abs(b_corridor[accepted_ba, 2] - a_corridor[indices_ba[accepted_ba], 2])
            distance3d_ba = np.linalg.norm(
                b_corridor[accepted_ba] - a_corridor[indices_ba[accepted_ba]], axis=1
            ) if np.any(accepted_ba) else np.empty(0)

            dz = np.concatenate([dz_ab, dz_ba])
            distance3d = np.concatenate([distance3d_ab, distance3d_ba])
            xy_accepted = np.concatenate([xy_distance_ab[accepted_ab], xy_distance_ba[accepted_ba]])
            residual_samples[pair_name] = np.column_stack(
                [a_corridor[accepted_ab, 0], a_corridor[accepted_ab, 1], dz_ab]
            ) if np.any(accepted_ab) else np.empty((0, 3), dtype=np.float64)
            metrics.append({
                "pair": [source_short(first), source_short(second)],
                "topology": topology,
                "xyOverlapExtent": overlap_extent.tolist(),
                "corridorMarginMeters": corridor_margin,
                "corridorSampleCounts": [int(len(a_corridor)), int(len(b_corridor))],
                "status": "measured" if len(dz) else "no-accepted-nearest-xy-matches",
                "nearest3dInsideCorridor": percentiles(distance3d),
                "verticalAtNearestXY": percentiles(dz),
                "horizontalAtAcceptedMatches": percentiles(xy_accepted),
                "horizontalAcceptanceMeters": 1.0,
                "acceptedFractionAtoB": float(np.mean(accepted_ab)) if len(accepted_ab) else 0.0,
                "acceptedFractionBtoA": float(np.mean(accepted_ba)) if len(accepted_ba) else 0.0,
            })
    return metrics, residual_samples


def seam_heatmap(
    residual_samples: dict[str, np.ndarray],
    output: Path,
    region: tuple[float, float, float, float],
    size: int = 720,
) -> None:
    min_x, min_y, max_x, max_y = region
    canvas = Image.new("RGB", (size, size), (246, 246, 246))
    pixels = np.asarray(canvas).copy()
    sum_grid = np.zeros((size, size), dtype=np.float64)
    count_grid = np.zeros((size, size), dtype=np.int32)
    for samples in residual_samples.values():
        if not len(samples):
            continue
        ix = np.clip(((samples[:, 0] - min_x) / (max_x - min_x) * (size - 1)).astype(int), 0, size - 1)
        iy = np.clip(((max_y - samples[:, 1]) / (max_y - min_y) * (size - 1)).astype(int), 0, size - 1)
        np.add.at(sum_grid, (iy, ix), np.minimum(samples[:, 2], 5.0))
        np.add.at(count_grid, (iy, ix), 1)
    valid = count_grid > 0
    average = np.zeros_like(sum_grid)
    average[valid] = sum_grid[valid] / count_grid[valid]
    # Expand sparse samples for readability without changing numeric metrics.
    average = ndimage.maximum_filter(average, size=5)
    valid = ndimage.maximum_filter(valid.astype(np.uint8), size=5).astype(bool)
    normalized = np.clip(average / 1.0, 0.0, 1.0)
    pixels[..., 0] = np.where(valid, 50 + 205 * normalized, pixels[..., 0]).astype(np.uint8)
    pixels[..., 1] = np.where(valid, 190 - 140 * normalized, pixels[..., 1]).astype(np.uint8)
    pixels[..., 2] = np.where(valid, 70 - 40 * normalized, pixels[..., 2]).astype(np.uint8)
    Image.fromarray(pixels, mode="RGB").save(output, optimize=False, compress_level=9)


@dataclass
class RasterSurface:
    source: str
    bottom: np.ndarray
    top: np.ndarray
    hit_count: np.ndarray
    accepted_triangles: int
    rejected_steep: int


def _grid_axis(minimum: float, maximum: float, cell: float) -> np.ndarray:
    cells = int(round((maximum - minimum) / cell))
    if not math.isclose(minimum + cells * cell, maximum, abs_tol=1e-9):
        raise ExperimentError("region extent must be divisible by cell size")
    return np.linspace(minimum, maximum, cells + 1, dtype=np.float64)


def rasterize_surface(
    source: str,
    triangles: np.ndarray,
    region: tuple[float, float, float, float],
    cell: float,
    slope_limit_deg: float = 70.0,
) -> RasterSurface:
    """Rasterize triangle intersections at grid vertices, keeping top and bottom hits."""
    min_x, min_y, max_x, max_y = region
    xs = _grid_axis(min_x, max_x, cell)
    ys = _grid_axis(min_y, max_y, cell)
    bottom = np.full((len(ys), len(xs)), np.nan, dtype=np.float32)
    top = np.full_like(bottom, np.nan)
    count = np.zeros(bottom.shape, dtype=np.uint16)

    normals, areas = triangle_normals(triangles)
    slope = np.degrees(np.arccos(np.clip(np.abs(normals[:, 2]), 0.0, 1.0)))
    keep = (areas > 1e-8) & (slope <= slope_limit_deg)
    selected = triangles[keep]

    for triangle in selected:
        p0, p1, p2 = triangle
        denominator = ((p1[1] - p2[1]) * (p0[0] - p2[0])
                       + (p2[0] - p1[0]) * (p0[1] - p2[1]))
        if abs(denominator) < 1e-12:
            continue
        tri_min = triangle[:, :2].min(axis=0)
        tri_max = triangle[:, :2].max(axis=0)
        x0 = max(0, int(math.ceil((tri_min[0] - min_x) / cell - 1e-9)))
        x1 = min(len(xs) - 1, int(math.floor((tri_max[0] - min_x) / cell + 1e-9)))
        y0 = max(0, int(math.ceil((tri_min[1] - min_y) / cell - 1e-9)))
        y1 = min(len(ys) - 1, int(math.floor((tri_max[1] - min_y) / cell + 1e-9)))
        if x1 < x0 or y1 < y0:
            continue
        grid_x, grid_y = np.meshgrid(xs[x0:x1+1], ys[y0:y1+1])
        w0 = ((p1[1] - p2[1]) * (grid_x - p2[0])
              + (p2[0] - p1[0]) * (grid_y - p2[1])) / denominator
        w1 = ((p2[1] - p0[1]) * (grid_x - p2[0])
              + (p0[0] - p2[0]) * (grid_y - p2[1])) / denominator
        w2 = 1.0 - w0 - w1
        inside = (w0 >= -1e-7) & (w1 >= -1e-7) & (w2 >= -1e-7)
        if not np.any(inside):
            continue
        height = w0 * p0[2] + w1 * p1[2] + w2 * p2[2]
        sub_bottom = bottom[y0:y1+1, x0:x1+1]
        sub_top = top[y0:y1+1, x0:x1+1]
        old_bottom = sub_bottom[inside]
        old_top = sub_top[inside]
        new_height = height[inside].astype(np.float32)
        sub_bottom[inside] = np.where(np.isnan(old_bottom), new_height, np.minimum(old_bottom, new_height))
        sub_top[inside] = np.where(np.isnan(old_top), new_height, np.maximum(old_top, new_height))
        sub_count = count[y0:y1+1, x0:x1+1]
        sub_count[inside] = np.minimum(sub_count[inside].astype(np.uint32) + 1, 65535).astype(np.uint16)

    return RasterSurface(
        source=source,
        bottom=bottom,
        top=top,
        hit_count=count,
        accepted_triangles=int(np.count_nonzero(keep)),
        rejected_steep=int(len(triangles) - np.count_nonzero(keep)),
    )


def candidate_stack(surfaces: Sequence[RasterSurface]) -> tuple[np.ndarray, list[dict[str, str]]]:
    layers: list[np.ndarray] = []
    metadata: list[dict[str, str]] = []
    for surface in surfaces:
        layers.extend([surface.bottom, surface.top])
        metadata.extend([
            {"source": source_short(surface.source), "surface": "bottom"},
            {"source": source_short(surface.source), "surface": "top"},
        ])
    return np.stack(layers, axis=0).astype(np.float32), metadata


def choose_seed(candidates: np.ndarray) -> tuple[int, int, float, int]:
    """Choose a conservative auto-seed near center using the lower candidate envelope."""
    lower = np.min(np.where(np.isfinite(candidates), candidates, np.inf), axis=0)
    lower[~np.isfinite(lower)] = np.nan
    valid = np.isfinite(lower)
    if not np.any(valid):
        raise ExperimentError("no raster candidates")
    height, width = lower.shape
    cy, cx = height // 2, width // 2
    yy, xx = np.indices(lower.shape)
    distance = np.hypot(yy - cy, xx - cx)
    finite_heights = lower[valid]
    p05, p95 = np.percentile(finite_heights, [5, 95])
    plausible = valid & (lower >= p05) & (lower <= p95)
    score = distance + np.where(plausible, 0.0, 1e9)
    sy, sx = np.unravel_index(np.argmin(score), score.shape)
    available = np.where(np.isfinite(candidates[:, sy, sx]))[0]
    layer = int(available[np.argmin(candidates[available, sy, sx])])
    return int(sy), int(sx), float(candidates[layer, sy, sx]), layer


def continuity_ground(
    candidates: np.ndarray,
    cell: float,
    seed: tuple[int, int, float, int] | None = None,
    max_slope_deg: float = 65.0,
    vertical_slack: float = 0.35,
) -> tuple[np.ndarray, np.ndarray, dict[str, Any]]:
    """Grow a single continuous candidate surface from one seed with Dijkstra."""
    layers, height, width = candidates.shape
    if seed is None:
        seed = choose_seed(candidates)
    sy, sx, seed_z, seed_layer = seed
    selected = np.full((height, width), np.nan, dtype=np.float32)
    owner = np.full((height, width), -1, dtype=np.int16)
    cost = np.full((height, width), np.inf, dtype=np.float64)
    selected[sy, sx] = seed_z
    owner[sy, sx] = seed_layer
    cost[sy, sx] = 0.0
    queue: list[tuple[float, int, int]] = [(0.0, sy, sx)]
    max_step_cardinal = math.tan(math.radians(max_slope_deg)) * cell + vertical_slack

    neighbours = [(-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (-1, 1), (1, -1), (1, 1)]
    while queue:
        current_cost, y, x = heapq.heappop(queue)
        if current_cost != cost[y, x]:
            continue
        current_z = float(selected[y, x])
        current_owner = int(owner[y, x])
        for dy, dx in neighbours:
            ny, nx = y + dy, x + dx
            if ny < 0 or nx < 0 or ny >= height or nx >= width:
                continue
            values = candidates[:, ny, nx]
            available = np.where(np.isfinite(values))[0]
            if not len(available):
                continue
            horizontal = cell * (math.sqrt(2.0) if dx and dy else 1.0)
            max_step = max_step_cardinal * (horizontal / cell)
            dz = np.abs(values[available].astype(np.float64) - current_z)
            allowed = dz <= max_step
            if not np.any(allowed):
                continue
            available = available[allowed]
            dz = dz[allowed]
            switch_penalty = np.where(available == current_owner, 0.0, 0.03)
            local_scores = dz + switch_penalty
            best_offset = int(np.argmin(local_scores))
            best_layer = int(available[best_offset])
            new_cost = current_cost + float(local_scores[best_offset]) + 0.001 * horizontal
            if new_cost < cost[ny, nx]:
                cost[ny, nx] = new_cost
                selected[ny, nx] = values[best_layer]
                owner[ny, nx] = best_layer
                heapq.heappush(queue, (new_cost, ny, nx))

    report = {
        "seed": {"gridY": sy, "gridX": sx, "height": seed_z, "layer": seed_layer},
        "coverage": float(np.mean(np.isfinite(selected))),
        "maxSlopeDegrees": max_slope_deg,
        "verticalSlack": vertical_slack,
    }
    return selected, owner, report


def fill_nearest(data: np.ndarray) -> tuple[np.ndarray, float]:
    valid = np.isfinite(data)
    missing_fraction = 1.0 - float(np.mean(valid))
    if not np.any(valid):
        raise ExperimentError("cannot fill an empty DEM")
    if np.all(valid):
        return data.astype(np.float32, copy=True), missing_fraction
    indices = ndimage.distance_transform_edt(~valid, return_distances=False, return_indices=True)
    filled = data[tuple(indices)]
    return filled.astype(np.float32), missing_fraction


def surface_metrics(data: np.ndarray, cell: float) -> dict[str, Any]:
    valid = np.isfinite(data)
    if not np.any(valid):
        return {"coverage": 0.0}
    filled, missing = fill_nearest(data)
    dy, dx = np.gradient(filled.astype(np.float64), cell, cell)
    slope = np.degrees(np.arctan(np.hypot(dx, dy)))
    rough = np.hypot(dx, dy)
    return {
        "coverage": float(np.mean(valid)),
        "missingFilledFraction": missing,
        "height": percentiles(filled[valid]),
        "slopeDegrees": percentiles(slope[valid]),
        "gradient": percentiles(rough[valid]),
    }


def confidence_map(candidates: np.ndarray, selected: np.ndarray) -> np.ndarray:
    valid_selected = np.isfinite(selected)
    delta = np.abs(candidates - selected[None, :, :])
    support = np.sum(np.isfinite(candidates) & (delta <= 0.20), axis=0)
    available = np.sum(np.isfinite(candidates), axis=0)
    confidence = np.divide(support, available, out=np.zeros_like(selected, dtype=np.float32), where=available > 0)
    confidence[~valid_selected] = 0.0
    return confidence


def scalar_png(data: np.ndarray, output: Path, *, invalid=(20, 20, 20), diverging: bool = False) -> None:
    valid = np.isfinite(data)
    canvas = np.zeros((*data.shape, 3), dtype=np.uint8)
    canvas[:, :] = invalid
    if np.any(valid):
        values = data[valid].astype(np.float64)
        low, high = np.percentile(values, [2, 98])
        if high <= low:
            high = low + 1.0
        normalized = np.clip((data - low) / (high - low), 0.0, 1.0)
        safe = np.where(valid, normalized, 0.0)
        if diverging:
            canvas[..., 0] = (255 * safe).astype(np.uint8)
            canvas[..., 1] = (255 * (1.0 - np.abs(safe - 0.5) * 2.0)).astype(np.uint8)
            canvas[..., 2] = (255 * (1.0 - safe)).astype(np.uint8)
        else:
            canvas[..., 0] = (40 + 180 * safe).astype(np.uint8)
            canvas[..., 1] = (60 + 170 * safe).astype(np.uint8)
            canvas[..., 2] = (120 + 120 * (1.0 - safe)).astype(np.uint8)
        canvas[~valid] = invalid
    image = Image.fromarray(np.flipud(canvas), mode="RGB")
    image = image.resize((720, 720), Image.Resampling.NEAREST)
    image.save(output, optimize=False, compress_level=9)


def owner_png(owner: np.ndarray, metadata: Sequence[dict[str, str]], output: Path) -> None:
    canvas = np.zeros((*owner.shape, 3), dtype=np.uint8)
    canvas[:, :] = (20, 20, 20)
    source_to_color: dict[str, tuple[int, int, int]] = {}
    for item in metadata:
        if item["source"] not in source_to_color:
            source_to_color[item["source"]] = SOURCE_COLORS[len(source_to_color) % len(SOURCE_COLORS)]
    for index, item in enumerate(metadata):
        mask = owner == index
        color = source_to_color[item["source"]]
        if item["surface"] == "bottom":
            color = tuple(max(0, channel - 30) for channel in color)
        canvas[mask] = color
    image = Image.fromarray(np.flipud(canvas), mode="RGB")
    image = image.resize((720, 720), Image.Resampling.NEAREST)
    image.save(output, optimize=False, compress_level=9)


def split_quantized_heightfield(
    dem: np.ndarray,
    output: Path,
    cell: float,
    region: tuple[float, float, float, float],
) -> dict[str, Any]:
    filled, missing_fraction = fill_nearest(dem)
    height, width = filled.shape
    if height % 2 != 1 or width % 2 != 1:
        raise ExperimentError("pilot DEM must have odd point dimensions for 2x2 split")
    minimum = float(np.min(filled)); maximum = float(np.max(filled))
    if maximum <= minimum:
        maximum = minimum + 1e-6
    quantized = np.rint((filled - minimum) / (maximum - minimum) * 65535.0).astype(np.uint16)
    mid_y, mid_x = height // 2, width // 2
    chunks = {
        "south_west": quantized[:mid_y+1, :mid_x+1],
        "south_east": quantized[:mid_y+1, mid_x:],
        "north_west": quantized[mid_y:, :mid_x+1],
        "north_east": quantized[mid_y:, mid_x:],
    }
    output.mkdir(parents=True, exist_ok=True)
    for name, chunk in chunks.items():
        chunk.astype("<u2", copy=False).tofile(output / f"{name}.u16")
    edges = {
        "southVertical": bool(np.array_equal(chunks["south_west"][:, -1], chunks["south_east"][:, 0])),
        "northVertical": bool(np.array_equal(chunks["north_west"][:, -1], chunks["north_east"][:, 0])),
        "westHorizontal": bool(np.array_equal(chunks["south_west"][-1, :], chunks["north_west"][0, :])),
        "eastHorizontal": bool(np.array_equal(chunks["south_east"][-1, :], chunks["north_east"][0, :])),
    }
    min_x, min_y, max_x, max_y = region
    manifest = {
        "format": "experimental-u16-heightfield-v1",
        "cellSize": cell,
        "region": [min_x, min_y, max_x, max_y],
        "globalMinHeight": minimum,
        "globalMaxHeight": maximum,
        "sourcePointDimensions": [width, height],
        "chunkPointDimensions": [mid_x + 1, mid_y + 1],
        "missingFilledFraction": missing_fraction,
        "sharedEdgesExact": edges,
        "allSharedEdgesExact": bool(all(edges.values())),
        "quantizationStep": (maximum - minimum) / 65535.0,
    }
    (output / "manifest.json").write_bytes(stable_json(manifest))
    return manifest


def cross_check_trimesh(input_path: Path, inspector_report: dict[str, Any]) -> dict[str, Any]:
    try:
        import trimesh  # type: ignore
    except ImportError:
        return {"status": "skipped", "reason": "trimesh not installed"}
    import tempfile
    checks = []
    by_name = {item["logicalName"]: item for item in inspector_report["files"]}
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        for logical_name, data in scan_inspect.collect(input_path):
            temp_path = root / Path(logical_name).name
            temp_path.write_bytes(data)
            scene = trimesh.load(temp_path, force="scene", process=False)
            vertices = int(sum(len(geometry.vertices) for geometry in scene.geometry.values()))
            triangles = int(sum(len(geometry.faces) for geometry in scene.geometry.values()))
            bounds = np.asarray(scene.bounds, dtype=np.float64)
            expected = by_name[logical_name]
            expected_bounds = np.asarray([expected["worldBounds"]["min"], expected["worldBounds"]["max"]])
            checks.append({
                "source": source_short(logical_name),
                "vertices": vertices,
                "triangles": triangles,
                "vertexMatch": vertices == expected["vertexCount"],
                "triangleMatch": triangles == expected["triangleCount"],
                "boundsMaxAbsError": float(np.max(np.abs(bounds - expected_bounds))),
            })
    return {"status": "completed", "checks": checks, "allMatch": all(
        item["vertexMatch"] and item["triangleMatch"] and item["boundsMaxAbsError"] <= 1e-6
        for item in checks
    )}


def texture_profile(input_path: Path, output: Path, caps: Sequence[int] = (1024, 2048)) -> dict[str, Any]:
    output.mkdir(parents=True, exist_ok=True)
    profiles: dict[str, list[dict[str, Any]]] = {str(cap): [] for cap in caps}
    source_total = 0
    decoded_native = 0
    image_counter = 0
    for logical_name, data in scan_inspect.collect(input_path):
        document, binary = scan_inspect.parse_glb(data, logical_name)
        for image_index, image_def in enumerate(document.get("images", [])):
            if "bufferView" not in image_def:
                continue
            payload = scan_inspect.view_bytes(document, binary, int(image_def["bufferView"]))
            source_total += len(payload)
            with Image.open(io.BytesIO(payload)) as opened:
                opened.load()
                source_image = opened.convert("RGB")
            decoded_native += source_image.width * source_image.height * 4
            largest_cap = max(caps)
            largest = source_image.copy()
            largest.thumbnail((largest_cap, largest_cap), Image.Resampling.LANCZOS)
            for cap in sorted(caps, reverse=True):
                if cap == largest_cap:
                    resized = largest
                else:
                    resized = largest.copy()
                    resized.thumbnail((cap, cap), Image.Resampling.LANCZOS)
                filename = f"{image_counter:02d}_{Path(logical_name).stem}_{image_index}_{cap}.jpg"
                path = output / str(cap) / filename
                path.parent.mkdir(parents=True, exist_ok=True)
                resized.save(path, format="JPEG", quality=88, optimize=False, progressive=False)
                profiles[str(cap)].append({
                    "source": source_short(logical_name),
                    "imageIndex": image_index,
                    "width": resized.width,
                    "height": resized.height,
                    "bytes": path.stat().st_size,
                    "sha256": sha256_file(path),
                    "rgbaMipBytes": int(resized.width * resized.height * 4 * 4 / 3),
                })
            source_image.close()
            largest.close()
            image_counter += 1
    return {
        "imageCount": image_counter,
        "sourceEmbeddedBytes": source_total,
        "nativeDecodedRgbaBytes": decoded_native,
        "profiles": {
            cap: {
                "images": items,
                "encodedBytes": sum(item["bytes"] for item in items),
                "rgbaMipBytes": sum(item["rgbaMipBytes"] for item in items),
            }
            for cap, items in profiles.items()
        },
    }


def run_experiments(
    input_path: Path,
    output: Path,
    name: str,
    *,
    include_core: bool = True,
    include_textures: bool = True,
) -> dict[str, Any]:
    output.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    report: dict[str, Any] = {
        "formatVersion": 1,
        "name": name,
        "region": list(GOLDEN_REGION),
        "stages": {},
    }
    report_path = output / "experiment_report.json"

    def checkpoint() -> None:
        report["elapsedSeconds"] = time.perf_counter() - started
        report_path.write_bytes(stable_json(report))

    if include_core:
        print("[1/5] inspector + parser cross-check", flush=True)
        inspector = scan_inspect.inspect_package(input_path, name)
        report["stages"]["parserCrossCheck"] = cross_check_trimesh(input_path, inspector)
        checkpoint()

        print("[2/5] load Golden Seam Region geometry", flush=True)
        load_start = time.perf_counter()
        sources = load_region_sources(input_path, GOLDEN_REGION)
        report["stages"]["regionGeometry"] = {
            "seconds": time.perf_counter() - load_start,
            "triangleCounts": {source_short(source): int(len(triangles)) for source, triangles in sources.items()},
        }
        checkpoint()

        print("[3/5] pairwise seam measurement", flush=True)
        seam_start = time.perf_counter()
        seam_metrics, seam_samples = pairwise_seam_metrics(sources)
        seam_heatmap(seam_samples, output / "seam_residual_heatmap.png", GOLDEN_REGION)
        report["stages"]["seamMeasurement"] = {
            "seconds": time.perf_counter() - seam_start,
            "pairs": seam_metrics,
        }
        checkpoint()

        dem_reports: dict[str, Any] = {}
        for cell in (0.50, 0.25):
            print(f"[4/5] DEM + heightfield cell={cell:.2f}", flush=True)
            label = f"{cell:.2f}".replace(".", "")
            dem_dir = output / f"dem_{label}"
            dem_dir.mkdir(parents=True, exist_ok=True)
            raster_start = time.perf_counter()
            surfaces = [
                rasterize_surface(source, triangles, GOLDEN_REGION, cell)
                for source, triangles in sorted(sources.items())
            ]
            candidates, metadata = candidate_stack(surfaces)
            continuity, owner, continuity_report = continuity_ground(candidates, cell)
            top = np.max(np.where(np.isfinite(candidates), candidates, -np.inf), axis=0)
            top[~np.isfinite(top)] = np.nan
            bottom = np.min(np.where(np.isfinite(candidates), candidates, np.inf), axis=0)
            bottom[~np.isfinite(bottom)] = np.nan
            confidence = confidence_map(candidates, continuity)
            scalar_png(top, dem_dir / "top_surface.png")
            scalar_png(bottom, dem_dir / "bottom_surface.png")
            scalar_png(continuity, dem_dir / "continuity_surface.png")
            scalar_png(confidence, dem_dir / "confidence.png")
            owner_png(owner, metadata, dem_dir / "owner.png")
            np.save(dem_dir / "top_surface.npy", top, allow_pickle=False)
            np.save(dem_dir / "bottom_surface.npy", bottom, allow_pickle=False)
            np.save(dem_dir / "continuity_surface.npy", continuity, allow_pickle=False)
            np.save(dem_dir / "owner.npy", owner, allow_pickle=False)

            morphology_reports: dict[str, Any] = {}
            filled_continuity, _ = filter_fill_nearest(continuity)
            for morphology_profile in DEFAULT_PROFILES:
                filtered = progressive_morphological_ground(continuity, cell, morphology_profile)
                ground = filtered["ground"]
                object_height = filtered["objectHeight"]
                profile_dir = dem_dir / f"morphology_{morphology_profile.name}"
                profile_dir.mkdir(parents=True, exist_ok=True)
                np.save(profile_dir / "ground.npy", ground, allow_pickle=False)
                np.save(profile_dir / "object_height.npy", object_height, allow_pickle=False)
                scalar_png(ground, profile_dir / "ground.png")
                scalar_png(object_height, profile_dir / "object_height.png")
                profile_heightfield = split_quantized_heightfield(
                    ground, profile_dir / "heightfield_2x2", cell, GOLDEN_REGION
                )
                box3d_manifest = export_box3d_obj(
                    ground, GOLDEN_REGION, cell, profile_dir / "box3d_ground.obj"
                )
                morphology_reports[morphology_profile.name] = {
                    "filter": filtered["metadata"],
                    "metrics": ground_profile_metrics(
                        filled_continuity, ground, object_height, cell
                    ),
                    "heightfield": profile_heightfield,
                    "box3dDebugMesh": box3d_manifest,
                }

            heightfield = split_quantized_heightfield(
                continuity, dem_dir / "heightfield_2x2", cell, GOLDEN_REGION
            )
            dem_reports[str(cell)] = {
                "seconds": time.perf_counter() - raster_start,
                "gridPoints": [int(continuity.shape[1]), int(continuity.shape[0])],
                "sources": [{
                    "source": source_short(surface.source),
                    "acceptedTriangles": surface.accepted_triangles,
                    "rejectedTriangles": surface.rejected_steep,
                    "coverage": float(np.mean(np.isfinite(surface.bottom))),
                } for surface in surfaces],
                "candidateLayers": metadata,
                "topMetrics": surface_metrics(top, cell),
                "bottomMetrics": surface_metrics(bottom, cell),
                "continuityMetrics": surface_metrics(continuity, cell),
                "continuity": continuity_report,
                "confidence": percentiles(confidence[np.isfinite(continuity)]),
                "heightfield": heightfield,
                "morphologyProfiles": morphology_reports,
            }
            report["stages"]["groundPilot"] = dem_reports
            checkpoint()
    else:
        report["stages"]["core"] = {"status": "skipped"}
        checkpoint()

    if include_textures:
        print("[5/5] actual texture resize profiles", flush=True)
        texture_start = time.perf_counter()
        report["stages"]["textureProfiles"] = texture_profile(
            input_path, output / "texture_profiles"
        )
        report["stages"]["textureProfiles"]["seconds"] = time.perf_counter() - texture_start
        checkpoint()
    else:
        report["stages"]["textureProfiles"] = {"status": "skipped"}
        checkpoint()

    report["totalSeconds"] = time.perf_counter() - started
    report_path.write_bytes(stable_json(report))
    report["reportSha256"] = sha256_file(report_path)
    return report


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--name", default="photogrammetry-plan-experiment")
    parser.add_argument("--only", choices=("all", "core", "textures"), default="all")
    arguments = parser.parse_args(argv)
    try:
        report = run_experiments(
            arguments.input, arguments.output, arguments.name,
            include_core=arguments.only in ("all", "core"),
            include_textures=arguments.only in ("all", "textures"),
        )
    except (ExperimentError, scan_inspect.ScanInspectionError, OSError, ValueError) as exc:
        print(f"scan_plan_experiments: ERROR: {exc}")
        return 1
    print(
        "scan_plan_experiments: OK "
        f"seconds={report['totalSeconds']:.2f} "
        f"report={arguments.output / 'experiment_report.json'}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
