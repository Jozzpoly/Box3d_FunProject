#!/usr/bin/env python3
"""Adaptive quadtree load-balance probe using full-world triangle centroids.

This does not split or duplicate geometry. It tests whether a uniform world grid
is a sensible load-balancing model before committing to runtime chunk formats.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import math
from pathlib import Path
from typing import Any, Sequence

import numpy as np
from PIL import Image, ImageDraw

from scan_geometry import iter_triangle_batches, scan_inspect


@dataclass
class Leaf:
    bounds: tuple[float, float, float, float]
    count: int
    depth: int


def collect_centroids(input_path: Path) -> np.ndarray:
    chunks = []
    for logical_name, data in scan_inspect.collect(input_path):
        for batch in iter_triangle_batches(logical_name, data):
            chunks.append(batch.triangles.mean(axis=1)[:, :2].astype(np.float32))
    return np.concatenate(chunks, axis=0)


def build_quadtree(
    points: np.ndarray,
    bounds: tuple[float, float, float, float],
    target_triangles: int,
    min_size: float,
    depth: int = 0,
) -> list[Leaf]:
    min_x, min_y, max_x, max_y = bounds
    count = int(len(points))
    if count <= target_triangles or max(max_x - min_x, max_y - min_y) <= min_size:
        return [Leaf(bounds, count, depth)]
    mid_x = (min_x + max_x) * 0.5
    mid_y = (min_y + max_y) * 0.5
    right = points[:, 0] >= mid_x
    top = points[:, 1] >= mid_y
    quadrants = [
        ((min_x, min_y, mid_x, mid_y), ~(right | top)),
        ((mid_x, min_y, max_x, mid_y), right & ~top),
        ((min_x, mid_y, mid_x, max_y), ~right & top),
        ((mid_x, mid_y, max_x, max_y), right & top),
    ]
    leaves: list[Leaf] = []
    for child_bounds, mask in quadrants:
        child_points = points[mask]
        if len(child_points):
            leaves.extend(build_quadtree(
                child_points, child_bounds, target_triangles, min_size, depth + 1
            ))
    return leaves


def profile(leaves: Sequence[Leaf], focus_xy: Sequence[float]) -> dict[str, Any]:
    counts = np.asarray([leaf.count for leaf in leaves], dtype=np.int64)
    sizes = np.asarray([max(leaf.bounds[2] - leaf.bounds[0], leaf.bounds[3] - leaf.bounds[1]) for leaf in leaves])
    depths = np.asarray([leaf.depth for leaf in leaves], dtype=np.int64)
    focus = {}
    for radius in (128.0, 256.0, 512.0):
        selected = []
        for leaf in leaves:
            min_x, min_y, max_x, max_y = leaf.bounds
            nearest_x = min(max(float(focus_xy[0]), min_x), max_x)
            nearest_y = min(max(float(focus_xy[1]), min_y), max_y)
            if math.hypot(nearest_x - focus_xy[0], nearest_y - focus_xy[1]) <= radius:
                selected.append(leaf.count)
        focus[str(int(radius))] = {
            "leafCount": len(selected),
            "triangleCountByCentroid": int(sum(selected)),
        }
    return {
        "leafCount": len(leaves),
        "trianglesPerLeaf": {
            "min": int(counts.min()),
            "median": float(np.median(counts)),
            "p90": float(np.percentile(counts, 90)),
            "p95": float(np.percentile(counts, 95)),
            "max": int(counts.max()),
            "mean": float(counts.mean()),
        },
        "leafSizeMeters": {
            "min": float(sizes.min()),
            "median": float(np.median(sizes)),
            "max": float(sizes.max()),
        },
        "depth": {
            "min": int(depths.min()),
            "median": float(np.median(depths)),
            "max": int(depths.max()),
        },
        "sizeHistogram": {
            f"{size:g}": int(np.count_nonzero(np.isclose(sizes, size)))
            for size in sorted(np.unique(sizes))
        },
        "focusRadiusProfiles": focus,
    }


def draw_quadtree(
    leaves: Sequence[Leaf],
    world_bounds: Sequence[float],
    output: Path,
    target: int,
    size: int = 1000,
) -> None:
    min_x, min_y, max_x, max_y = map(float, world_bounds)
    image = Image.new("RGB", (size, size), (245, 245, 245))
    draw = ImageDraw.Draw(image)

    def px(x: float) -> int:
        return int(round((x - min_x) / (max_x - min_x) * (size - 1)))

    def py(y: float) -> int:
        return int(round((max_y - y) / (max_y - min_y) * (size - 1)))

    for leaf in leaves:
        x0, y0, x1, y1 = leaf.bounds
        intensity = min(1.0, leaf.count / max(target, 1))
        color = (
            int(50 + 205 * intensity),
            int(190 - 130 * intensity),
            int(210 - 160 * intensity),
        )
        draw.rectangle((px(x0), py(y1), px(x1), py(y0)), fill=color, outline=(30, 30, 30), width=1)
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output, optimize=False, compress_level=9)


def run(input_path: Path, output_dir: Path, targets: Sequence[int], min_size: float) -> dict[str, Any]:
    inspection = scan_inspect.inspect_package(input_path, "adaptive-chunk-probe")
    points = collect_centroids(input_path)
    if len(points) != inspection["totals"]["triangleCount"]:
        raise RuntimeError("centroid count mismatch")
    bounds = inspection["globalBounds"]
    world = (bounds["min"][0], bounds["min"][1], bounds["max"][0], bounds["max"][1])
    report = {
        "formatVersion": 1,
        "triangleCount": int(len(points)),
        "worldBoundsXY": list(world),
        "minLeafSizeMeters": min_size,
        "profiles": {},
        "limitations": [
            "Triangles are assigned by centroid; boundary duplication is not estimated here.",
            "This is a load-balance experiment, not a runtime chunk format.",
        ],
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    for target in targets:
        leaves = build_quadtree(points, world, target, min_size)
        item = profile(leaves, (8.0, 8.0))
        item["targetTrianglesPerLeaf"] = target
        report["profiles"][str(target)] = item
        draw_quadtree(leaves, world, output_dir / f"quadtree_{target}.png", target)
    (output_dir / "adaptive_chunk_report.json").write_bytes(
        (json.dumps(report, indent=2, sort_keys=True) + "\n").encode("utf-8")
    )
    return report


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--targets", default="25000,50000,100000")
    parser.add_argument("--min-size", type=float, default=32.0)
    arguments = parser.parse_args(argv)
    targets = [int(item) for item in arguments.targets.split(",") if item.strip()]
    report = run(arguments.input, arguments.output, targets, arguments.min_size)
    print(f"scan_adaptive_chunk_probe: OK profiles={len(report['profiles'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
