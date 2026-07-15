#!/usr/bin/env python3
"""Offline chunk-size probe for the complete photogrammetry world."""
from __future__ import annotations

import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
from typing import Any, Sequence

import numpy as np

from scan_geometry import iter_triangle_batches, scan_inspect


def stable_json(data: Any) -> bytes:
    return (json.dumps(data, indent=2, sort_keys=True) + "\n").encode("utf-8")


def add_batch_counts(
    counts: dict[tuple[int, int], int],
    triangles: np.ndarray,
    origin_x: float,
    origin_y: float,
    chunk_size: float,
) -> tuple[int, int]:
    tri_min = triangles[:, :, :2].min(axis=1)
    tri_max = triangles[:, :, :2].max(axis=1)
    ix0 = np.floor((tri_min[:, 0] - origin_x) / chunk_size).astype(np.int64)
    iy0 = np.floor((tri_min[:, 1] - origin_y) / chunk_size).astype(np.int64)
    ix1 = np.floor((tri_max[:, 0] - origin_x) / chunk_size).astype(np.int64)
    iy1 = np.floor((tri_max[:, 1] - origin_y) / chunk_size).astype(np.int64)

    single = (ix0 == ix1) & (iy0 == iy1)
    if np.any(single):
        pairs = np.column_stack([ix0[single], iy0[single]])
        unique, frequencies = np.unique(pairs, axis=0, return_counts=True)
        for pair, frequency in zip(unique, frequencies, strict=True):
            counts[(int(pair[0]), int(pair[1]))] += int(frequency)

    crossing_indices = np.where(~single)[0]
    assignments = int(np.count_nonzero(single))
    for index in crossing_indices:
        for iy in range(int(iy0[index]), int(iy1[index]) + 1):
            for ix in range(int(ix0[index]), int(ix1[index]) + 1):
                counts[(ix, iy)] += 1
                assignments += 1
    return assignments, int(len(crossing_indices))


def probe_chunks(
    input_path: Path,
    chunk_sizes: Sequence[float],
    heightfield_cell: float,
    focus_xy: tuple[float, float],
) -> dict[str, Any]:
    inspection = scan_inspect.inspect_package(input_path, "chunk-probe")
    bounds = inspection["globalBounds"]
    min_x, min_y = bounds["min"][:2]
    max_x, max_y = bounds["max"][:2]
    result: dict[str, Any] = {
        "formatVersion": 1,
        "worldBoundsXY": [min_x, min_y, max_x, max_y],
        "focusXY": list(focus_xy),
        "heightfieldCellMeters": heightfield_cell,
        "totalTriangles": inspection["totals"]["triangleCount"],
        "profiles": {},
    }

    accumulators = {
        float(size): {
            "origin": (
                math.floor(min_x / size) * size,
                math.floor(min_y / size) * size,
            ),
            "counts": defaultdict(int),
            "assignments": 0,
            "crossing": 0,
        }
        for size in chunk_sizes
    }

    processed = 0
    for logical_name, data in scan_inspect.collect(input_path):
        for batch in iter_triangle_batches(logical_name, data):
            processed += batch.count
            for size, accumulator in accumulators.items():
                assignments, crossing = add_batch_counts(
                    accumulator["counts"], batch.triangles,
                    accumulator["origin"][0], accumulator["origin"][1], size,
                )
                accumulator["assignments"] += assignments
                accumulator["crossing"] += crossing
    if processed != result["totalTriangles"]:
        raise RuntimeError(f"processed {processed} triangles, expected {result['totalTriangles']}")

    for size, accumulator in accumulators.items():
        counts = accumulator["counts"]
        values = np.asarray(list(counts.values()), dtype=np.int64)
        origin_x, origin_y = accumulator["origin"]
        grid_x = int(math.ceil((max_x - origin_x) / size))
        grid_y = int(math.ceil((max_y - origin_y) / size))
        points_per_side = int(round(size / heightfield_cell)) + 1
        per_chunk_height_bytes = points_per_side * points_per_side * 2

        radius_profiles = {}
        for radius in (128.0, 256.0, 512.0):
            selected = []
            for (ix, iy), triangle_count in counts.items():
                center_x = origin_x + (ix + 0.5) * size
                center_y = origin_y + (iy + 0.5) * size
                half_diagonal = math.sqrt(2.0) * size * 0.5
                if math.hypot(center_x - focus_xy[0], center_y - focus_xy[1]) <= radius + half_diagonal:
                    selected.append(triangle_count)
            radius_profiles[str(int(radius))] = {
                "chunkCount": len(selected),
                "triangleAssignments": int(sum(selected)),
                "heightfieldBytes": len(selected) * per_chunk_height_bytes,
            }

        result["profiles"][str(int(size))] = {
            "chunkSizeMeters": size,
            "gridOriginXY": [origin_x, origin_y],
            "fullGridDimensions": [grid_x, grid_y],
            "fullGridChunkCount": grid_x * grid_y,
            "nonEmptyChunkCount": len(counts),
            "emptyChunkCount": grid_x * grid_y - len(counts),
            "triangleAssignments": int(accumulator["assignments"]),
            "triangleDuplicationRatio": float(accumulator["assignments"] / processed),
            "boundaryCrossingTriangleCount": int(accumulator["crossing"]),
            "boundaryCrossingFraction": float(accumulator["crossing"] / processed),
            "trianglesPerNonEmptyChunk": {
                "min": int(values.min()),
                "median": float(np.median(values)),
                "p90": float(np.percentile(values, 90)),
                "p95": float(np.percentile(values, 95)),
                "max": int(values.max()),
                "mean": float(values.mean()),
            },
            "heightfield": {
                "cellMeters": heightfield_cell,
                "pointsPerSide": points_per_side,
                "u16BytesPerChunk": per_chunk_height_bytes,
                "u16BytesAllGridChunks": grid_x * grid_y * per_chunk_height_bytes,
                "u16BytesNonEmptyChunks": len(counts) * per_chunk_height_bytes,
            },
            "focusRadiusProfiles": radius_profiles,
            "heaviestChunks": [
                {"index": [ix, iy], "triangleAssignments": count}
                for (ix, iy), count in sorted(counts.items(), key=lambda item: (-item[1], item[0]))[:10]
            ],
        }
    return result


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--chunk-sizes", default="64,128,256")
    parser.add_argument("--heightfield-cell", type=float, default=0.5)
    parser.add_argument("--focus-x", type=float, default=8.0)
    parser.add_argument("--focus-y", type=float, default=8.0)
    arguments = parser.parse_args(argv)
    sizes = [float(value) for value in arguments.chunk_sizes.split(",") if value.strip()]
    report = probe_chunks(
        arguments.input, sizes, arguments.heightfield_cell,
        (arguments.focus_x, arguments.focus_y),
    )
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(stable_json(report))
    print(f"scan_chunk_probe: OK profiles={len(sizes)} report={arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
