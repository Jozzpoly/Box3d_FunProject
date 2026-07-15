#!/usr/bin/env python3
"""Quality probe comparing 1K texture caps against a 2K reference."""
from __future__ import annotations

import argparse
import io
import json
import math
from pathlib import Path
from typing import Any, Sequence

import numpy as np
from PIL import Image, ImageChops, ImageDraw

from scan_geometry import scan_inspect

try:
    from skimage.metrics import structural_similarity
except ImportError:  # pragma: no cover - optional dependency
    structural_similarity = None


def psnr(reference: np.ndarray, candidate: np.ndarray) -> float:
    mse = float(np.mean((reference.astype(np.float32) - candidate.astype(np.float32)) ** 2))
    return 120.0 if mse <= 1e-20 else 20.0 * math.log10(255.0 / math.sqrt(mse))


def edge_energy(image: np.ndarray) -> float:
    gray = image.astype(np.float32).mean(axis=2)
    dx = np.diff(gray, axis=1)
    dy = np.diff(gray, axis=0)
    return float(np.mean(np.abs(dx)) + np.mean(np.abs(dy)))


def compare_image(payload: bytes, source: str, image_index: int) -> tuple[dict[str, Any], dict[str, Image.Image]]:
    with Image.open(io.BytesIO(payload)) as opened:
        opened.load()
        original = opened.convert("RGB")
    reference = original.copy()
    reference.thumbnail((2048, 2048), Image.Resampling.LANCZOS)
    profile_1k = reference.copy()
    profile_1k.thumbnail((1024, 1024), Image.Resampling.LANCZOS)
    upsampled = profile_1k.resize(reference.size, Image.Resampling.LANCZOS)
    metric_reference = reference.copy()
    metric_reference.thumbnail((512, 512), Image.Resampling.LANCZOS)
    metric_candidate = upsampled.resize(metric_reference.size, Image.Resampling.LANCZOS)
    reference_np = np.asarray(metric_reference)
    upsampled_np = np.asarray(metric_candidate)
    score_ssim = None
    if structural_similarity is not None:
        score_ssim = float(structural_similarity(reference_np, upsampled_np, channel_axis=2, data_range=255))
    ref_edge = edge_energy(reference_np)
    candidate_edge = edge_energy(upsampled_np)
    metric_reference.close()
    metric_candidate.close()
    result = {
        "source": source,
        "imageIndex": image_index,
        "originalDimensions": [original.width, original.height],
        "referenceDimensions": [reference.width, reference.height],
        "profile1kDimensions": [profile_1k.width, profile_1k.height],
        "psnrDb": psnr(reference_np, upsampled_np),
        "ssim": score_ssim,
        "referenceEdgeEnergy": ref_edge,
        "profile1kUpsampledEdgeEnergy": candidate_edge,
        "edgeEnergyRatio": candidate_edge / ref_edge if ref_edge > 1e-12 else 1.0,
    }
    difference = ImageChops.difference(reference, upsampled)
    # Amplify only for visualization; numeric metrics use raw data.
    difference = difference.point(lambda value: min(255, value * 5))
    original.close()
    return result, {"reference": reference, "profile1k": upsampled, "difference5x": difference}


def contact_sheet(items: Sequence[tuple[dict[str, Any], dict[str, Image.Image]]], output: Path) -> None:
    tile = 360
    header = 42
    canvas = Image.new("RGB", (tile * 3, (tile + header) * len(items)), (245, 245, 245))
    draw = ImageDraw.Draw(canvas)
    for row, (metrics, images) in enumerate(items):
        y = row * (tile + header)
        label = (
            f"{metrics['source']} img={metrics['imageIndex']} "
            f"PSNR={metrics['psnrDb']:.2f} SSIM={metrics['ssim'] if metrics['ssim'] is not None else 'n/a'}"
        )
        draw.text((8, y + 8), label, fill=(20, 20, 20))
        for column, key in enumerate(("reference", "profile1k", "difference5x")):
            image = images[key].copy()
            image.thumbnail((tile, tile), Image.Resampling.LANCZOS)
            x = column * tile + (tile - image.width) // 2
            iy = y + header + (tile - image.height) // 2
            canvas.paste(image, (x, iy))
            image.close()
    output.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output, optimize=False, compress_level=9)


def run(input_path: Path, output: Path) -> dict[str, Any]:
    results: list[dict[str, Any]] = []
    visuals: list[tuple[dict[str, Any], dict[str, Image.Image]]] = []
    for logical_name, data in scan_inspect.collect(input_path):
        document, binary = scan_inspect.parse_glb(data, logical_name)
        for image_index, image_def in enumerate(document.get("images", [])):
            if "bufferView" not in image_def:
                continue
            payload = scan_inspect.view_bytes(document, binary, int(image_def["bufferView"]))
            metrics, images = compare_image(payload, Path(logical_name).name, image_index)
            results.append(metrics)
            visuals.append((metrics, images))
    finite_ssim = [item["ssim"] for item in results if item["ssim"] is not None]
    report = {
        "formatVersion": 1,
        "comparison": "1K cap upsampled to 2K reference; metrics sampled at max 512px",
        "imageCount": len(results),
        "metrics": results,
        "summary": {
            "psnrDb": _summary([item["psnrDb"] for item in results]),
            "ssim": _summary(finite_ssim) if finite_ssim else None,
            "edgeEnergyRatio": _summary([item["edgeEnergyRatio"] for item in results]),
        },
        "limitations": [
            "2K is treated as the visual reference, not the native 8K image.",
            "Metrics do not replace fixed-camera runtime screenshots.",
            "JPEG recompression and BC7 quality are not evaluated here.",
        ],
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "texture_quality_report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    worst = sorted(visuals, key=lambda item: item[0]["ssim"] if item[0]["ssim"] is not None else item[0]["psnrDb"])[:4]
    contact_sheet(worst, output / "texture_quality_worst4.png")
    for _, images in visuals:
        for image in images.values():
            image.close()
    return report


def _summary(values: Sequence[float]) -> dict[str, float]:
    data = np.asarray(values, dtype=np.float64)
    return {
        "min": float(np.min(data)),
        "median": float(np.median(data)),
        "p10": float(np.percentile(data, 10)),
        "p90": float(np.percentile(data, 90)),
        "max": float(np.max(data)),
    }


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args(argv)
    report = run(arguments.input, arguments.output)
    print(f"scan_texture_quality_probe: OK images={report['imageCount']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
