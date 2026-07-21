# Scan derivatives foundation

**Branch:** `agent/p2a-scan-derivatives-foundation`  
**Stack base:** `agent/p2a-source-visual-preview@f20357ba10618ddecfdd2e274e93917fe508a983`  
**Status:** implementation present; exact-head local/hosted execution still required.

## Purpose

The web HomeScan project demonstrated a useful performance architecture:
expensive scan preparation belongs offline, while runtime consumes small,
purpose-built products. This package transfers that idea without transferring
the web project's unsafe terrain semantics or Three.js-specific machinery.

The governing rule remains:

> A scan is evidence about the world. It is not automatically authored world,
> accepted ground, or collision.

## Closed derivative graph

```text
SOURCE_REVISION
├── EXACT_VISUAL_PREVIEW
│   └── OPTIMIZED_VISUAL
└── SURFACE_EVIDENCE
    └── ACCEPTED_SURFACE
        └── COLLISION_PROJECTION
```

The graph is directional. A later derivative cannot manufacture or bypass an
earlier PASS.

Version 1 permits `READY` only for:

- exact visual preview;
- conservative surface evidence.

The following remain deliberately blocked:

- optimized visual LOD;
- accepted surface;
- collision projection.

They require separate versioned formats, independent verification and promotion
gates. Rehashing a catalog after changing an edge, status or capability does not
make the mutation legal.

## What was adopted from the web project

### Offline scan compiler

Source GLB/PLY files are parsed and transformed by tools under
`tools/scan_pipeline`. Native runtime must not parse the original photogrammetry
formats or decide source semantics during a frame.

### Separate products for separate consumers

- exact preview answers whether source geometry is transformed and displayed
  correctly;
- optimized visual will later answer rendering cost and LOD questions;
- surface evidence answers where point observations exist and how strong they
  are;
- accepted surface will contain explicit owner/authored decisions;
- collision projection will be a disposable cooker output from accepted data.

### Resource accounting

The derivative catalog records exact serialized byte cost and relevant geometry
or grid counts. Geometry and texture budgets remain separate by policy. No LOD
cooker should be designed from assumptions before the real exact preview is
measured.

### Different interest centers

```text
render interest center  = CAMERA
physics interest center = VEHICLE
```

A camera may need distant coarse context while physics needs detailed accepted
surface near the vehicle. One residency policy must not silently control both.

### Lightweight surface queries

`scan_surface_query.py` memory-maps a verified surface evidence payload and
returns one exact cell observation. It returns explicit `UNKNOWN` or `OUTSIDE`
when no observation exists.

It never:

- fills a gap;
- interpolates through unknown cells;
- calls an observed low point accepted ground;
- creates collision.

## What was explicitly rejected from the web project

The web DTM v0 used operations appropriate for camera walking but unsafe for a
vehicle physics truth boundary:

- propagation from neighbours;
- global-average fallback;
- morphological opening as automatic truth;
- repeated blur;
- silent conversion of missing data into traversable terrain.

None of those operations are present in the surface evidence pack.

Unknown remains unknown.

## Conservative surface evidence pack v1

Builder:

```text
tools/scan_pipeline/scan_surface_evidence.py
```

Independent verifier:

```text
tools/scan_pipeline/scan_surface_evidence_verify.py
```

The builder requires:

- one independently verifiable P1B bundle;
- the exact acknowledged, bundle-bound P1B owner receipt;
- an owner-confirmed source frame;
- the private original PLY root;
- immutable size and SHA-256 matches for every PLY.

The point cloud is read in bounded chunks. Each point is transformed into lab
space before rasterization.

### Binary payload

Header, little-endian:

```text
8 bytes  magic = JSSURF1\0
uint32   version = 1
uint32   width
uint32   height
float32  cell size in metres
float32  origin X in lab metres
float32  origin Z in lab metres
```

Each cell:

```text
float32  lowest observed Y
float32  highest observed Y
uint32   support count
uint64   source-tile mask
uint8    evidence quality
uint8    classification
uint16   reserved = 0
```

An unknown cell is canonical:

```text
lowest/highest = canonical quiet NaN
support        = 0
source mask    = 0
quality        = 0
classification = UNKNOWN
```

An observed cell is classified only as:

```text
OBSERVED_SURFACE_EVIDENCE
```

This is not a ground classification.

### Evidence quality

The quality byte is a closed deterministic summary of:

- support count;
- number of contributing source tiles;
- vertical spread relative to cell size.

It is evidence quality, not probability that a cell is drivable ground.

### Publication and verification

The pack is:

- private-local-only;
- content-addressed;
- transactionally published;
- closed-schema;
- exact-file-set verified;
- SHA-256 verified;
- canonical-NaN verified;
- source-mask verified;
- statistics re-derived from payload cells;
- incapable of claiming accepted-world or collision readiness.

## Surface query API

```python
from pathlib import Path
from scan_surface_query import SurfaceEvidenceQuery

with SurfaceEvidenceQuery(Path("build/scan_pipeline/surface-evidence/...")) as view:
    sample = view.sample(x_meters=12.5, z_meters=-3.0)
    if sample.is_observed:
        print(sample.lowest_height_meters)
    else:
        print(sample.status)  # UNKNOWN or OUTSIDE
```

`observed_lowest_height()` returns `None` for both unknown and outside cells.
It performs no interpolation.

## Derivative catalog

Builder/verifier:

```text
tools/scan_pipeline/scan_derivative_catalog.py
tools/scan_pipeline/scan_derivative_catalog_verify.py
```

The catalog binds all present derivatives to the same:

- source bundle hash;
- package ID;
- source revision ID;
- source-frame contract hash.

It also records:

- serialized bytes;
- estimated v1 resident bytes;
- tile, vertex and triangle counts for exact visual;
- cell, observed and unknown counts for surface evidence;
- immutable runtime policies;
- blockers for every unavailable derivative.

## Runtime policy boundary

The catalog fixes these policies in schema v1:

```text
coarse-first visual loading may exist later
exact evidence remains separately addressable
unknown surface is not collidable until reviewed
runtime does not parse source GLB or PLY
offline cooking is required
measure exact preview before designing visual LOD
geometry and texture budgets are separate
```

Changing these values and recomputing the document hash is rejected.

## Current test scope

This branch adds:

```text
surface evidence contracts                 9
surface evidence verifier CLI              3
surface query contracts                    5
derivative catalog contracts               7
derivative catalog verifier CLI            3
---------------------------------------------
new tests                                  27
previous exact P2A suite                  108
expected canonical total                 135
```

The exact total must not be recorded as PASS until the branch head is executed.

## Correct promotion sequence

The nearest user-visible path is unchanged:

```text
owner-confirmed source frame
→ real P1B bundle and acknowledged receipt
→ real exact preview pack
→ TERRAIN_VISIBLE_PASS
```

After the exact terrain is visible and measured:

```text
real PLY surface evidence
→ surface query/debug overlay
→ internal GLB↔PLY correspondence
→ seam and adjacency evidence
→ owner-selected Golden Drive Region
→ reviewed accepted surface
→ collision projection
→ physics probes
→ first vehicle drive
```

## Deliberately parked

- automatic ground classification;
- automatic gap filling;
- DTM blur or smoothing;
- visual LOD generation before real measurements;
- texture transcoding;
- native tile streaming/eviction manager;
- accepted-surface editor;
- heightfield collision cooker;
- vehicle drive on scan terrain.

Parking these items is not abandonment. It prevents optimization or physical
authority from being built on unmeasured or unreviewed data.

## Privacy

No source coordinates, original source names, absolute paths, direct source file
hashes or private scan payloads belong in Git. Real derivative packs stay under
ignored local build output. Only closed, explicitly shareable metadata may cross
the privacy boundary.
