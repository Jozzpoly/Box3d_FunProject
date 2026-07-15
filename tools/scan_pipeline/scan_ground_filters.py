#!/usr/bin/env python3
"""Experimental ground filtering and Box3D-oriented export helpers."""
from __future__ import annotations

from dataclasses import dataclass
import json
import math
from pathlib import Path
from typing import Any, Sequence

import numpy as np
from scipy import ndimage


@dataclass(frozen=True)
class MorphologyProfile:
    name: str
    windows_m: tuple[float, ...]
    base_threshold_m: float
    threshold_slope: float


DEFAULT_PROFILES = (
    MorphologyProfile("gentle", (2.0, 4.0, 8.0), 0.25, 0.12),
    MorphologyProfile("balanced", (2.0, 4.0, 8.0, 16.0), 0.15, 0.08),
    MorphologyProfile("aggressive", (3.0, 6.0, 12.0, 24.0), 0.10, 0.06),
)


def fill_nearest(surface: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    valid = np.isfinite(surface)
    if not np.any(valid):
        raise ValueError("surface has no finite cells")
    if np.all(valid):
        return surface.astype(np.float32, copy=True), valid
    indices = ndimage.distance_transform_edt(~valid, return_distances=False, return_indices=True)
    return surface[tuple(indices)].astype(np.float32), valid


def progressive_morphological_ground(
    surface: np.ndarray,
    cell_size: float,
    profile: MorphologyProfile,
) -> dict[str, np.ndarray | dict[str, Any]]:
    """Suppress elevated compact structures through progressive grey opening.

    This is a hypothesis generator, not semantic truth. The returned object_height
    is especially useful for manual review: roofs/trees should become bright,
    while broad terrain should stay near zero.
    """
    original, originally_valid = fill_nearest(surface)
    ground = original.copy()
    passes: list[dict[str, Any]] = []
    cumulative_mask = np.zeros_like(originally_valid)

    for window_m in profile.windows_m:
        window_cells = max(3, int(math.ceil(window_m / cell_size)))
        if window_cells % 2 == 0:
            window_cells += 1
        opened = ndimage.grey_opening(ground, size=(window_cells, window_cells), mode="nearest")
        threshold = profile.base_threshold_m + profile.threshold_slope * window_m
        delta = ground - opened
        elevated = delta > threshold
        ground[elevated] = opened[elevated]
        cumulative_mask |= elevated
        passes.append({
            "windowMeters": window_m,
            "windowCells": window_cells,
            "thresholdMeters": threshold,
            "changedCells": int(np.count_nonzero(elevated)),
        })

    object_height = np.maximum(original - ground, 0.0).astype(np.float32)
    return {
        "ground": ground.astype(np.float32),
        "objectHeight": object_height,
        "objectMask": cumulative_mask,
        "originallyValid": originally_valid,
        "metadata": {
            "profile": profile.name,
            "windowsMeters": list(profile.windows_m),
            "baseThresholdMeters": profile.base_threshold_m,
            "thresholdSlope": profile.threshold_slope,
            "passes": passes,
        },
    }


def slope_degrees(surface: np.ndarray, cell_size: float) -> np.ndarray:
    dy, dx = np.gradient(surface.astype(np.float64), cell_size, cell_size)
    return np.degrees(np.arctan(np.hypot(dx, dy)))


def profile_metrics(
    original: np.ndarray,
    ground: np.ndarray,
    object_height: np.ndarray,
    cell_size: float,
) -> dict[str, Any]:
    slope_before = slope_degrees(original, cell_size)
    slope_after = slope_degrees(ground, cell_size)
    removed = object_height
    return {
        "removedFractionOver0_5m": float(np.mean(removed > 0.5)),
        "removedFractionOver1m": float(np.mean(removed > 1.0)),
        "removedFractionOver2m": float(np.mean(removed > 2.0)),
        "removedVolumeCubicMeters": float(np.sum(removed) * cell_size * cell_size),
        "removedHeightPercentiles": _percentiles(removed),
        "slopeBeforeDegrees": _percentiles(slope_before),
        "slopeAfterDegrees": _percentiles(slope_after),
        "groundDifferencePercentiles": _percentiles(np.abs(original - ground)),
    }


def _percentiles(values: np.ndarray) -> dict[str, float]:
    values = np.asarray(values, dtype=np.float64)
    finite = values[np.isfinite(values)]
    if not len(finite):
        return {"median": math.nan, "p90": math.nan, "p95": math.nan, "p99": math.nan, "max": math.nan}
    result = np.percentile(finite, [50, 90, 95, 99, 100])
    return {
        "median": float(result[0]),
        "p90": float(result[1]),
        "p95": float(result[2]),
        "p99": float(result[3]),
        "max": float(result[4]),
    }


def source_to_box3d_matrix(origin_source_xyz: Sequence[float]) -> list[list[float]]:
    """Map source X/Y horizontal + Z-up to Box3D X/Z horizontal + Y-up.

    boxX = sourceX - originX
    boxY = sourceZ - originZ
    boxZ = -(sourceY - originY)
    """
    ox, oy, oz = map(float, origin_source_xyz)
    return [
        [1.0, 0.0, 0.0, -ox],
        [0.0, 0.0, 1.0, -oz],
        [0.0, -1.0, 0.0, oy],
        [0.0, 0.0, 0.0, 1.0],
    ]


def export_box3d_obj(
    ground: np.ndarray,
    region_xy: Sequence[float],
    cell_size: float,
    path: Path,
    *,
    origin_source_xyz: Sequence[float] | None = None,
) -> dict[str, Any]:
    """Export a debug OBJ using the proposed right-handed source->Box3D mapping."""
    min_x, min_y, max_x, max_y = map(float, region_xy)
    rows, columns = ground.shape
    expected_columns = int(round((max_x - min_x) / cell_size)) + 1
    expected_rows = int(round((max_y - min_y) / cell_size)) + 1
    if (columns, rows) != (expected_columns, expected_rows):
        raise ValueError("ground dimensions do not match region/cell size")
    if origin_source_xyz is None:
        origin_source_xyz = (
            (min_x + max_x) * 0.5,
            (min_y + max_y) * 0.5,
            float(np.median(ground)),
        )
    ox, oy, oz = map(float, origin_source_xyz)
    xs = np.linspace(min_x, max_x, columns)
    ys = np.linspace(min_y, max_y, rows)

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("# Experimental Box3D-oriented pilot ground\n")
        stream.write(f"# source origin {ox:.9f} {oy:.9f} {oz:.9f}\n")
        for row, source_y in enumerate(ys):
            for column, source_x in enumerate(xs):
                source_z = float(ground[row, column])
                box_x = source_x - ox
                box_y = source_z - oz
                box_z = -(source_y - oy)
                stream.write(f"v {box_x:.6f} {box_y:.6f} {box_z:.6f}\n")
        for row in range(rows - 1):
            for column in range(columns - 1):
                a = row * columns + column + 1
                b = a + 1
                c = a + columns
                d = c + 1
                # Winding chosen for upward-facing normals in Box3D Y-up.
                stream.write(f"f {a} {b} {c}\n")
                stream.write(f"f {b} {d} {c}\n")

    manifest = {
        "format": "experimental-box3d-ground-obj-v1",
        "path": path.name,
        "sourceRegionXY": [min_x, min_y, max_x, max_y],
        "cellSize": cell_size,
        "pointDimensions": [columns, rows],
        "vertexCount": columns * rows,
        "triangleCount": (columns - 1) * (rows - 1) * 2,
        "sourceOriginXYZ": [ox, oy, oz],
        "sourceToBox3D": source_to_box3d_matrix((ox, oy, oz)),
    }
    path.with_suffix(".json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return manifest
