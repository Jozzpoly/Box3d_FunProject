#!/usr/bin/env python3
"""Read-only, memory-mapped queries over a verified surface-evidence pack.

This interface never interpolates across cells and never returns a collision or
accepted-ground claim. It exposes only the exact observed evidence stored in one
cell, or the explicit states UNKNOWN / OUTSIDE.
"""
from __future__ import annotations

from dataclasses import dataclass
import importlib.util
import math
import mmap
from pathlib import Path
import sys
from typing import Any, Iterator

MODULE_DIR = Path(__file__).resolve().parent


def _load_surface() -> Any:
    path = MODULE_DIR / "scan_surface_evidence.py"
    spec = importlib.util.spec_from_file_location("_jozz_surface_query_contract", path)
    if not spec or not spec.loader:
        raise RuntimeError(f"cannot load {path.name}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


surface = _load_surface()

OUTSIDE = "OUTSIDE"
UNKNOWN = "UNKNOWN"
OBSERVED = "OBSERVED_SURFACE_EVIDENCE"


class SurfaceQueryError(ValueError):
    pass


@dataclass(frozen=True)
class SurfaceEvidenceSample:
    status: str
    cell_x: int | None
    cell_z: int | None
    lowest_height_meters: float | None
    highest_height_meters: float | None
    support_count: int
    source_tile_ids: tuple[int, ...]
    evidence_quality: int
    classification: str

    @property
    def is_observed(self) -> bool:
        return self.status == OBSERVED


class SurfaceEvidenceQuery:
    """Verified read-only query view over one private surface evidence pack."""

    def __init__(self, root: Path):
        self.root = Path(root)
        self.summary = surface.verify_surface_evidence_pack(self.root)
        self.manifest = surface._strict_json(self.root / "COMPLETE.json")
        self._path = self.root / self.manifest["surfacePath"]
        if not self._path.is_file() or self._path.is_symlink():
            raise SurfaceQueryError("surface payload must be a real local file")
        self._handle = self._path.open("rb")
        try:
            self._map = mmap.mmap(self._handle.fileno(), 0, access=mmap.ACCESS_READ)
            header = surface.HEADER.unpack_from(self._map, 0)
            magic, version, width, height, cell_size, origin_x, origin_z = header
            if magic != surface.MAGIC or version != surface.BINARY_VERSION:
                raise SurfaceQueryError("surface payload identity mismatch")
            self.width = int(width)
            self.height = int(height)
            self.cell_size_meters = float(cell_size)
            self.origin_x_meters = float(origin_x)
            self.origin_z_meters = float(origin_z)
            self._tile_ids = tuple(
                int(record["tileId"])
                for record in self.manifest["tileBitOrdinals"]
            )
        except Exception:
            self._handle.close()
            raise

    def close(self) -> None:
        mapping = getattr(self, "_map", None)
        if mapping is not None:
            mapping.close()
            self._map = None
        handle = getattr(self, "_handle", None)
        if handle is not None:
            handle.close()
            self._handle = None

    def __enter__(self) -> "SurfaceEvidenceQuery":
        return self

    def __exit__(self, exc_type: Any, exc: Any, traceback: Any) -> None:
        self.close()

    def _cell_coordinates(self, x_meters: float, z_meters: float) -> tuple[int, int] | None:
        x = float(x_meters)
        z = float(z_meters)
        if not math.isfinite(x) or not math.isfinite(z):
            raise SurfaceQueryError("query coordinates must be finite")
        cell_x = math.floor((x - self.origin_x_meters) / self.cell_size_meters)
        cell_z = math.floor((z - self.origin_z_meters) / self.cell_size_meters)
        if cell_x < 0 or cell_z < 0 or cell_x >= self.width or cell_z >= self.height:
            return None
        return int(cell_x), int(cell_z)

    def sample(self, x_meters: float, z_meters: float) -> SurfaceEvidenceSample:
        coordinates = self._cell_coordinates(x_meters, z_meters)
        if coordinates is None:
            return SurfaceEvidenceSample(
                status=OUTSIDE,
                cell_x=None,
                cell_z=None,
                lowest_height_meters=None,
                highest_height_meters=None,
                support_count=0,
                source_tile_ids=(),
                evidence_quality=0,
                classification=OUTSIDE,
            )
        cell_x, cell_z = coordinates
        index = cell_z * self.width + cell_x
        offset = surface.HEADER.size + index * surface.CELL.size
        low, high, support, mask, quality, classification, reserved = surface.CELL.unpack_from(
            self._map, offset
        )
        if reserved != 0:
            raise SurfaceQueryError("surface cell reserved field is nonzero")
        if classification == surface.UNKNOWN:
            return SurfaceEvidenceSample(
                status=UNKNOWN,
                cell_x=cell_x,
                cell_z=cell_z,
                lowest_height_meters=None,
                highest_height_meters=None,
                support_count=0,
                source_tile_ids=(),
                evidence_quality=0,
                classification=UNKNOWN,
            )
        if classification != surface.OBSERVED_SURFACE_EVIDENCE:
            raise SurfaceQueryError("surface cell classification is unsupported")
        source_ids = tuple(
            tile_id
            for ordinal, tile_id in enumerate(self._tile_ids)
            if mask & (1 << ordinal)
        )
        return SurfaceEvidenceSample(
            status=OBSERVED,
            cell_x=cell_x,
            cell_z=cell_z,
            lowest_height_meters=float(low),
            highest_height_meters=float(high),
            support_count=int(support),
            source_tile_ids=source_ids,
            evidence_quality=int(quality),
            classification=OBSERVED,
        )

    def observed_lowest_height(self, x_meters: float, z_meters: float) -> float | None:
        """Return the cell's observed low point, never an interpolated/fill value."""
        sample = self.sample(x_meters, z_meters)
        return sample.lowest_height_meters if sample.is_observed else None

    def iter_observed(self) -> Iterator[SurfaceEvidenceSample]:
        for cell_z in range(self.height):
            for cell_x in range(self.width):
                x = self.origin_x_meters + (cell_x + 0.5) * self.cell_size_meters
                z = self.origin_z_meters + (cell_z + 0.5) * self.cell_size_meters
                sample = self.sample(x, z)
                if sample.is_observed:
                    yield sample
