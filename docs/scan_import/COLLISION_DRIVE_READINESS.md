# Collision / drive-test readiness

> **STATUS 2026-07-23: DELIVERED — the drive test now exists and passes.**
> The readiness audit below was correct; the build it predicted is the
> `"P2B Scan Drive (M6)"` sample. See **[Result](#result-drive-test-passed)** at the
> bottom and the runbook [DRIVE_THE_SCAN.md](DRIVE_THE_SCAN.md). The audit text is kept
> as-authored, since it is what the implementation was planned against.

**Verdict: fundamentally ready.** The static-mesh physics API exists and fits the pack
geometry directly, the geometry is now leveled + metric + static, and the integration
seam in the **current M6 car** is identified. The drive test is a well-scoped build, not
a research problem. No fundamental blocker.

This document is a readiness audit only. It does **not** create collision (P2A stays
render-only) and does not grant `DRIVE_TEST_READY`.

## What is already in place

### 1. The static-mesh API fits the pack byte-for-byte

- `b3CreateMesh(const b3MeshDef* def, int* degenerateTriangleIndices, int degenerateCapacity)`
  → BVH-baked `b3MeshData*` (`include/box3d/collision.h`). Static only.
- `b3CreateMeshShape(b3BodyId, const b3ShapeDef*, const b3MeshData* mesh, b3Vec3 scale)`
  → attaches the mesh to a (static) body (`include/box3d/box3d.h`). Holds a reference to
  the mesh — it must outlive the shape.
- `b3MeshDef` (`include/box3d/types.h`) wants exactly:
  - `b3Vec3* vertices` — positions only;
  - `int32_t* indices` — 3 per triangle;
  - `uint8_t* materialIndices` — optional (may be null for a single material);
  - `float weldTolerance`.

The preview pack's tile `.bin` already stores lab-metre `position.xyz` per vertex and a
`uint32` group-local triangle list. Mapping pack → `b3MeshDef` is a straight copy of the
positions plus an index re-base (below).

### 2. The geometry is physics-suitable

- **Leveled** (owner-confirmed): the ground is horizontal, so a car will not creep
  downhill from an export-tilt artifact. See [[project-scan-leveling-repro]] equivalent.
- **Metric**: lab metres, unit scale (`b3CreateMeshShape` scale = `{1,1,1}`).
- **Static + triangulated**: 7 tiles, ~1.41M vertices / ~1.78M triangles.

### 3. The integration seam (current M6 car)

`JozzVehicleM6RigLab` builds its world ground in its constructor:

```
jozz_vehicle_m6_rig_lab.cpp:37
    m_worldGround = CreateJozzWorldGround( m_worldId, JOZZ_M6_TERRAIN_CATEGORY );
```

`CreateJozzWorldGround` (`jozz_vehicle_world_terrain.cpp`) currently builds a procedural
plate (hull) + an offroad **height field**. The drive test swaps in a
**scan-mesh ground**: a static body carrying `b3CreateMeshShape` built from the pack,
tagged with `JOZZ_M6_TERRAIN_CATEGORY` so the wheels collide, and registered with
`SetGroundShape(...)` for the debug renderer.

> Note: some shared helpers keep legacy `M5` names (e.g. `CreateJozzVehicleM5TestCourse`
> is still called by the M6 rig). The **vehicle/rig to build on is M6**
> (`samples/jozz_vehicle_m6_*`); the archival **M5 car** (`jozz_vehicle_m5_drivable_lab.*`)
> is not to be used.

## Drive-phase work (scoped tasks, not blockers)

1. **Index re-base.** Pack indices are group-local `uint32`; `b3MeshDef` wants `int32`
   into one contiguous vertex array. Merge groups/tiles with a running vertex-base offset
   (the renderer already applies this exact pattern), or build one mesh per group.
2. **A collision-side geometry reader.** The pack → geometry parse currently lives in
   `jozz_scan_preview_pack.cpp`, which is **physics-excluded by a static architecture
   test** (it rejects body/shape/mesh/heightfield/terrain APIs). So the drive test must be
   a **separate M6-based sample**. Factor a pure `read pack .bin → positions[] + indices[]`
   helper that the collision sample calls; P2A keeps render-only.
3. **Degenerate / sliver triangles.** Photogrammetry meshes contain them.
   `b3CreateMesh` reports them via `degenerateTriangleIndices` + capacity; set a
   `weldTolerance`. Decide the tolerance and how many degenerates to tolerate/log.
4. **BVH bake of ~1.78M static triangles.** One-time and static, but sanity-check bake
   time, BVH height (`b3GetHeight`), and step cost. Consider per-tile meshes for locality.
5. **Winding.** Confirm the pack's triangle winding matches the mesh collider's expected
   convention (the pack already reverses winding for an approved mirror).

## Suggested first drive milestone

A new M6 sample that: reads the active pack geometry (helper from task 2), builds one (or
per-tile) static `b3CreateMesh` ground with `JOZZ_M6_TERRAIN_CATEGORY`, spawns the M6 car
over the central village tile, and lets it drive. Primary target: the village core;
stress target: the imperfect tile edges/seams.

## Result: drive test passed

Built exactly as scoped, as `"P2B Scan Drive (M6)"`
(`samples/jozz_vehicle_scan_drive_lab.*` + the pure reader
`samples/jozz_vehicle_scan_geometry.*`). The M6 rig lab was left untouched.

Measured on the leveled pack `source-preview-aee5242a20848294`:

| Check | Result |
|---|---|
| Collision baked | **7 tile meshes, 1 775 775 triangles** (all of them) |
| Degenerate triangles | **5 824** reported by `b3CreateMesh`, tolerated, no weld needed |
| Winding (hazard 1) | **default correct** — no flip required; the toggle stays for future scans |
| Car at rest | settles at `(46.77, 277.720, -60.50)`, **identical at step 300 and 600** — stable, no sinking, no fall-through |
| Car under power | autodrive moved it to `(55.82, 276.789, -59.72)` ≈ **9 m**, descending 1 m with the terrain |
| Wheel contact | 2/4 at rest on uneven ground, **4/4 while driving** |
| Stopped by | a scanned vertical object (pole) — real collision against scan geometry, not just the ground plane |

The five drive-phase tasks above resolved as: (1) index re-base implemented in the reader;
(2) the collision-side reader is a separate pure file, so P2A stayed render-only;
(3) degenerates only needed reporting; (4) per-tile BVH bake is fast enough even in a Debug
build; (5) winding needed no change.

Textured visuals were then layered on by consuming the render-only `JozzScanPreviewPack`
from the drive sample, so the texture and the collider are the *same* pack bytes rather
than two things kept in sync.
