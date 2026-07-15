#!/usr/bin/env python3
"""Kinematic four-wheel probe for experimental photogrammetry heightfields.

This is not a Box3D simulation. It uses the accepted M6 default footprint
(wheelbase 2.5 m, track 2.1 m, total suspension travel hint 0.7 m) to expose
terrain inputs likely to cause impossible articulation or violent wheel motion
before runtime integration.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any, Sequence

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

DEFAULT_WHEELBASE = 2.50
DEFAULT_TRACK = 2.10
DEFAULT_TOTAL_TRAVEL = 0.70
DEFAULT_REGION = (-32.0, -32.0, 48.0, 48.0)


def stable_json(data: Any) -> bytes:
    return (json.dumps(data, indent=2, sort_keys=True) + "\n").encode("utf-8")


def percentiles(values: np.ndarray) -> dict[str, float | int | None]:
    finite = np.asarray(values, dtype=np.float64)
    finite = finite[np.isfinite(finite)]
    if not len(finite):
        return {"count": 0, "median": None, "p90": None, "p95": None, "p99": None, "max": None}
    result = np.percentile(np.abs(finite), [50, 90, 95, 99, 100])
    return {
        "count": int(len(finite)),
        "median": float(result[0]),
        "p90": float(result[1]),
        "p95": float(result[2]),
        "p99": float(result[3]),
        "max": float(result[4]),
    }


def fill_nearest(data: np.ndarray) -> np.ndarray:
    valid = np.isfinite(data)
    if not np.any(valid):
        raise ValueError("heightfield contains no finite samples")
    if np.all(valid):
        return data.astype(np.float64)
    indices = ndimage.distance_transform_edt(~valid, return_distances=False, return_indices=True)
    return data[tuple(indices)].astype(np.float64)


def bilinear_sample(
    height: np.ndarray,
    x: np.ndarray,
    y: np.ndarray,
    region: Sequence[float],
    cell_size: float,
) -> np.ndarray:
    min_x, min_y, max_x, max_y = map(float, region)
    gx = (x - min_x) / cell_size
    gy = (y - min_y) / cell_size
    x0 = np.floor(gx).astype(np.int64)
    y0 = np.floor(gy).astype(np.int64)
    x1 = x0 + 1
    y1 = y0 + 1
    valid = (
        (x0 >= 0) & (y0 >= 0)
        & (x1 < height.shape[1]) & (y1 < height.shape[0])
        & (x >= min_x) & (x <= max_x) & (y >= min_y) & (y <= max_y)
    )
    result = np.full(np.shape(x), np.nan, dtype=np.float64)
    if not np.any(valid):
        return result
    tx = gx[valid] - x0[valid]
    ty = gy[valid] - y0[valid]
    h00 = height[y0[valid], x0[valid]]
    h10 = height[y0[valid], x1[valid]]
    h01 = height[y1[valid], x0[valid]]
    h11 = height[y1[valid], x1[valid]]
    result[valid] = (
        h00 * (1.0 - tx) * (1.0 - ty)
        + h10 * tx * (1.0 - ty)
        + h01 * (1.0 - tx) * ty
        + h11 * tx * ty
    )
    return result


def detected_seam_paths(
    owner: np.ndarray,
    region: Sequence[float],
    cell_size: float,
    margin: float = 4.0,
) -> list[dict[str, Any]]:
    """Create paths crossing the strongest source ownership boundaries."""
    source = np.where(owner >= 0, owner // 2, -1)
    min_x, min_y, max_x, max_y = map(float, region)
    paths: list[dict[str, Any]] = []

    vertical = (source[:, 1:] != source[:, :-1]) & (source[:, 1:] >= 0) & (source[:, :-1] >= 0)
    vertical_counts = vertical.sum(axis=0)
    for rank, column in enumerate(np.argsort(vertical_counts)[::-1][:2]):
        rows = np.where(vertical[:, column])[0]
        if not len(rows) or vertical_counts[column] < 3:
            continue
        row = int(np.median(rows))
        seam_x = min_x + (column + 0.5) * cell_size
        path_y = min_y + row * cell_size
        paths.append({
            "name": f"cross_vertical_seam_{rank+1}",
            "start": [max(min_x + margin, seam_x - 20.0), path_y],
            "end": [min(max_x - margin, seam_x + 20.0), path_y],
            "detectedSeam": {"axis": "x", "coordinate": seam_x, "supportCells": int(vertical_counts[column])},
        })

    horizontal = (source[1:, :] != source[:-1, :]) & (source[1:, :] >= 0) & (source[:-1, :] >= 0)
    horizontal_counts = horizontal.sum(axis=1)
    for rank, row in enumerate(np.argsort(horizontal_counts)[::-1][:2]):
        columns = np.where(horizontal[row, :])[0]
        if not len(columns) or horizontal_counts[row] < 3:
            continue
        column = int(np.median(columns))
        seam_y = min_y + (row + 0.5) * cell_size
        path_x = min_x + column * cell_size
        paths.append({
            "name": f"cross_horizontal_seam_{rank+1}",
            "start": [path_x, max(min_y + margin, seam_y - 20.0)],
            "end": [path_x, min(max_y - margin, seam_y + 20.0)],
            "detectedSeam": {"axis": "y", "coordinate": seam_y, "supportCells": int(horizontal_counts[row])},
        })

    paths.extend([
        {
            "name": "golden_region_east_west",
            "start": [min_x + margin, (min_y + max_y) * 0.5],
            "end": [max_x - margin, (min_y + max_y) * 0.5],
        },
        {
            "name": "golden_region_north_south",
            "start": [(min_x + max_x) * 0.5, min_y + margin],
            "end": [(min_x + max_x) * 0.5, max_y - margin],
        },
        {
            "name": "golden_region_diagonal",
            "start": [min_x + margin, min_y + margin],
            "end": [max_x - margin, max_y - margin],
        },
    ])

    unique: list[dict[str, Any]] = []
    seen: set[tuple[float, float, float, float]] = set()
    for path in paths:
        key = tuple(round(value, 3) for point in (path["start"], path["end"]) for value in point)
        if key not in seen:
            seen.add(key)
            unique.append(path)
    return unique


def path_samples(start: Sequence[float], end: Sequence[float], spacing: float) -> tuple[np.ndarray, np.ndarray]:
    start_np = np.asarray(start, dtype=np.float64)
    end_np = np.asarray(end, dtype=np.float64)
    length = float(np.linalg.norm(end_np - start_np))
    count = max(2, int(math.ceil(length / spacing)) + 1)
    t = np.linspace(0.0, 1.0, count)
    centers = start_np[None, :] * (1.0 - t[:, None]) + end_np[None, :] * t[:, None]
    distance = t * length
    return centers, distance


def wheel_positions(
    centers: np.ndarray,
    heading: np.ndarray,
    wheelbase: float,
    track: float,
) -> np.ndarray:
    forward = heading / np.linalg.norm(heading)
    left = np.array([-forward[1], forward[0]])
    offsets = np.array([
        forward * (wheelbase * 0.5) + left * (track * 0.5),   # front-left
        forward * (wheelbase * 0.5) - left * (track * 0.5),   # front-right
        -forward * (wheelbase * 0.5) + left * (track * 0.5),  # rear-left
        -forward * (wheelbase * 0.5) - left * (track * 0.5),  # rear-right
    ])
    return centers[:, None, :] + offsets[None, :, :]


def evaluate_path(
    height: np.ndarray,
    path: dict[str, Any],
    region: Sequence[float],
    cell_size: float,
    wheelbase: float,
    track: float,
    total_travel: float,
    spacing: float = 0.10,
) -> tuple[dict[str, Any], dict[str, np.ndarray]]:
    centers, distance = path_samples(path["start"], path["end"], spacing)
    heading = np.asarray(path["end"], dtype=np.float64) - np.asarray(path["start"], dtype=np.float64)
    wheels_xy = wheel_positions(centers, heading, wheelbase, track)
    wheel_heights = np.column_stack([
        bilinear_sample(height, wheels_xy[:, wheel, 0], wheels_xy[:, wheel, 1], region, cell_size)
        for wheel in range(4)
    ])
    valid = np.all(np.isfinite(wheel_heights), axis=1)
    distance = distance[valid]
    centers = centers[valid]
    wheels_xy = wheels_xy[valid]
    wheel_heights = wheel_heights[valid]
    if len(distance) < 3:
        return {"name": path["name"], "status": "insufficient-valid-samples"}, {}

    front = wheel_heights[:, :2].mean(axis=1)
    rear = wheel_heights[:, 2:].mean(axis=1)
    left = wheel_heights[:, [0, 2]].mean(axis=1)
    right = wheel_heights[:, [1, 3]].mean(axis=1)
    pitch = np.degrees(np.arctan2(front - rear, wheelbase))
    roll = np.degrees(np.arctan2(left - right, track))

    # For a rectangular footprint, this cross term is the non-planar/twist input.
    twist = 0.25 * (
        wheel_heights[:, 0] - wheel_heights[:, 1]
        - wheel_heights[:, 2] + wheel_heights[:, 3]
    )
    articulation_span = np.abs(twist) * 2.0
    footprint_range = wheel_heights.max(axis=1) - wheel_heights.min(axis=1)

    ds = float(np.median(np.diff(distance)))
    wheel_step = np.diff(wheel_heights, axis=0)
    center_height = wheel_heights.mean(axis=1)
    center_gradient = np.gradient(center_height, ds)
    center_curvature = np.gradient(center_gradient, ds)

    acceleration: dict[str, Any] = {}
    for speed in (5.0, 15.0, 30.0):
        acceleration[f"{int(speed)}mps"] = percentiles(center_curvature * speed * speed)

    result = {
        "name": path["name"],
        "status": "measured",
        "start": path["start"],
        "end": path["end"],
        "detectedSeam": path.get("detectedSeam"),
        "pathLengthMeters": float(distance[-1] - distance[0]),
        "sampleSpacingMeters": ds,
        "validSamples": int(len(distance)),
        "wheelHeightStepMeters": percentiles(wheel_step),
        "pitchDegrees": percentiles(pitch),
        "rollDegrees": percentiles(roll),
        "articulationSpanMeters": percentiles(articulation_span),
        "footprintHeightRangeMeters": percentiles(footprint_range),
        "centerVerticalAccelerationMetersPerSecond2": acceleration,
        "travelBudgetMeters": total_travel,
        "travelBudgetExceededFraction": float(np.mean(articulation_span > total_travel)),
        "halfTravelExceededFraction": float(np.mean(articulation_span > total_travel * 0.5)),
    }
    traces = {
        "distance": distance,
        "centers": centers,
        "wheelsXY": wheels_xy,
        "wheelHeights": wheel_heights,
        "pitch": pitch,
        "roll": roll,
        "articulation": articulation_span,
    }
    return result, traces


def overlay_paths(
    height: np.ndarray,
    paths: Sequence[dict[str, Any]],
    region: Sequence[float],
    output: Path,
) -> None:
    valid = np.isfinite(height)
    low, high = np.percentile(height[valid], [2, 98])
    normalized = np.clip((height - low) / max(high - low, 1e-6), 0.0, 1.0)
    safe = np.where(valid, normalized, 0.0)
    rgb = np.stack([
        40 + 180 * safe,
        60 + 170 * safe,
        120 + 120 * (1.0 - safe),
    ], axis=-1).astype(np.uint8)
    image = Image.fromarray(np.flipud(rgb), mode="RGB").resize((900, 900), Image.Resampling.BILINEAR)
    draw = ImageDraw.Draw(image)
    min_x, min_y, max_x, max_y = map(float, region)

    def pixel(point: Sequence[float]) -> tuple[float, float]:
        x = (float(point[0]) - min_x) / (max_x - min_x) * 899
        y = (max_y - float(point[1])) / (max_y - min_y) * 899
        return x, y

    for index, path in enumerate(paths):
        color = (255, 255, 255) if index % 2 == 0 else (255, 80, 20)
        draw.line([pixel(path["start"]), pixel(path["end"])], fill=color, width=4)
        x, y = pixel(path["start"])
        draw.ellipse((x-5, y-5, x+5, y+5), fill=color)
    image.save(output, optimize=False, compress_level=9)


def run_probe(
    core_output: Path,
    output: Path,
    wheelbase: float,
    track: float,
    total_travel: float,
) -> dict[str, Any]:
    output.mkdir(parents=True, exist_ok=True)
    report: dict[str, Any] = {
        "formatVersion": 1,
        "vehicleFootprint": {
            "wheelbaseMeters": wheelbase,
            "trackMeters": track,
            "totalSuspensionTravelHintMeters": total_travel,
            "source": "JozzVehicleM6DefaultConfig defaults",
        },
        "region": list(DEFAULT_REGION),
        "profiles": {},
    }

    for cell_label, cell in (("050", 0.50), ("025", 0.25)):
        dem_root = core_output / f"dem_{cell_label}"
        owner = np.load(dem_root / "owner.npy")
        paths = detected_seam_paths(owner, DEFAULT_REGION, cell)
        cell_report: dict[str, Any] = {"cellSize": cell, "paths": paths, "surfaces": {}}
        surfaces = {
            "continuity": dem_root / "continuity_surface.npy",
            "morphology_gentle": dem_root / "morphology_gentle" / "ground.npy",
            "morphology_balanced": dem_root / "morphology_balanced" / "ground.npy",
            "morphology_aggressive": dem_root / "morphology_aggressive" / "ground.npy",
        }
        for surface_name, surface_path in surfaces.items():
            height = fill_nearest(np.load(surface_path))
            surface_results = []
            trace_directory = output / cell_label / surface_name
            trace_directory.mkdir(parents=True, exist_ok=True)
            overlay_paths(height, paths, DEFAULT_REGION, trace_directory / "paths.png")
            worst_score = -1.0
            worst_trace: dict[str, np.ndarray] | None = None
            worst_name = ""
            for path in paths:
                result, traces = evaluate_path(
                    height, path, DEFAULT_REGION, cell,
                    wheelbase, track, total_travel,
                )
                surface_results.append(result)
                if result.get("status") == "measured":
                    score = float(result["articulationSpanMeters"]["p95"] or 0.0)
                    score += 0.1 * float(result["wheelHeightStepMeters"]["p95"] or 0.0)
                    if score > worst_score:
                        worst_score = score
                        worst_trace = traces
                        worst_name = result["name"]
            if worst_trace is not None:
                np.savez_compressed(
                    trace_directory / "worst_path_trace.npz",
                    **worst_trace,
                )
            cell_report["surfaces"][surface_name] = {
                "paths": surface_results,
                "worstPath": worst_name,
                "worstScore": worst_score,
                "summary": summarize_surface(surface_results, total_travel),
            }
        report["profiles"][cell_label] = cell_report

    (output / "drive_probe_report.json").write_bytes(stable_json(report))
    return report


def summarize_surface(paths: Sequence[dict[str, Any]], total_travel: float) -> dict[str, Any]:
    measured = [path for path in paths if path.get("status") == "measured"]
    if not measured:
        return {"status": "no-measured-paths"}

    def maximum(field: str, percentile: str = "p95") -> float:
        return max(float(path[field][percentile] or 0.0) for path in measured)

    return {
        "measuredPathCount": len(measured),
        "worstP95WheelStepMeters": maximum("wheelHeightStepMeters"),
        "worstP95PitchDegrees": maximum("pitchDegrees"),
        "worstP95RollDegrees": maximum("rollDegrees"),
        "worstP95ArticulationSpanMeters": maximum("articulationSpanMeters"),
        "maxTravelBudgetExceededFraction": max(path["travelBudgetExceededFraction"] for path in measured),
        "maxHalfTravelExceededFraction": max(path["halfTravelExceededFraction"] for path in measured),
        "travelBudgetMeters": total_travel,
        "provisionalPass": (
            maximum("wheelHeightStepMeters") <= 0.15
            and maximum("articulationSpanMeters") <= total_travel
            and max(path["travelBudgetExceededFraction"] for path in measured) == 0.0
        ),
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core-output", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--wheelbase", type=float, default=DEFAULT_WHEELBASE)
    parser.add_argument("--track", type=float, default=DEFAULT_TRACK)
    parser.add_argument("--total-travel", type=float, default=DEFAULT_TOTAL_TRAVEL)
    arguments = parser.parse_args(argv)
    report = run_probe(
        arguments.core_output, arguments.output,
        arguments.wheelbase, arguments.track, arguments.total_travel,
    )
    print(
        "scan_drive_probe: OK "
        f"profiles={sum(len(item['surfaces']) for item in report['profiles'].values())} "
        f"report={arguments.output / 'drive_probe_report.json'}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
