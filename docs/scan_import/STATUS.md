# Photogrammetry Import V2 — current status

**Updated:** 2026-07-21  
**Active branch:** `agent/p2a-scan-derivatives-foundation`  
**Stack base:** `agent/p2a-source-visual-preview@f20357ba10618ddecfdd2e274e93917fe508a983`  
**Exact-preview draft PR:** #5  
**Derivative-foundation draft PR:** #7

## Core rule

> A scan is evidence about the world. It is not automatically authored world,
> accepted ground, or collision.

## Truth table

| Capability | Status | Evidence / blocker |
|---|---|---|
| P0 vehicle baseline | `PASS_LOCAL_EXACT_HEAD` | Windows build, validator, Box3D tests and 300-frame smoke passed |
| P1A parser and synthetic contracts | `PASS_CI` | hosted parser/inspection matrix passed |
| P1A real 7+7 inspection | `PASS_LOCAL_DETERMINISTIC` | two byte-identical reports; each 7 GLB, 7 PLY and 7 pairs |
| Pair bounds compatibility | `PASS_WITH_REVIEW` | current pair evidence remains `BOUNDS_ONLY`; interior correspondence is not asserted |
| Source-frame contract machinery | `PASS_HOSTED_AND_LOCAL` | units, signed axes, handedness, origin, mirror approval and round-trip checks implemented |
| Real owner-confirmed source frame | `NOT_CREATED` | MipMap report supports metric `LOCAL_ENU`, but real local origin and explicit owner confirmation are still required |
| Private/shareable evidence boundary | `PASS_CI_SYNTHETIC` | shareable allow-list excludes names, paths, bounds, source hashes and free-form private metadata |
| Transactional P1B bundle and verifier | `PASS_CI_SYNTHETIC` | content-addressed publication, exact file set and independent tamper verification implemented |
| Owner-gate receipt v2 | `IMPLEMENTED_NOT_REAL_RUN` | receipt is bound to exact bundle SHA-256 and source revision |
| Real 7+7 P1B bundle | `NOT_RUN` | requires owner-confirmed source frame |
| Manual shareable privacy review | `NOT_RUN` | waits for real bundle and exact printed review target |
| P2A exact preview code gate | `PASS_LOCAL_EXACT_HEAD` | head `f20357ba...`: 108/108 tests, CMake, native samples build, vehicle gate and smoke passed |
| Real exact preview pack | `NOT_RUN` | requires real `P1B_BUNDLE_PASS` receipt and private GLB root |
| P2A native terrain review | `NOT_RUN` | no real pack has yet been loaded in the P2A lab |
| Conservative surface-evidence pack | `IMPLEMENTED_NOT_EXACT_HEAD_EXECUTED` | streamed PLY evidence, canonical UNKNOWN cells, content addressing and independent verifier implemented on PR #7 |
| Read-only surface query API | `IMPLEMENTED_NOT_EXACT_HEAD_EXECUTED` | exact observed cell / UNKNOWN / OUTSIDE semantics; no interpolation or ground claim |
| Derivative graph and resource catalog | `IMPLEMENTED_NOT_EXACT_HEAD_EXECUTED` | exact preview and surface evidence may be READY; optimized visual, accepted surface and collision remain blocked |
| Internal GLB↔PLY correspondence | `BLOCKED_REAL_BUNDLE` | no occupancy/surface-support proof yet |
| Tile adjacency and seam evidence | `NOT_STARTED` | starts after real source geometry is visible and measured |
| Golden Drive Region | `NOT_SELECTED` | requires visual/seam evidence and explicit owner choice |
| Accepted surface | `NOT_STARTED` | surface evidence is not authored truth |
| Collision projection | `NOT_STARTED` | must derive only from accepted surface |
| Scan-terrain physics probes | `NOT_STARTED` | waits for collision projection |
| First vehicle drive | `NOT_STARTED` | waits for probe PASS |
| JES transfer candidate | `NO` | requires reimport and a second real scan |

## Exact P2A proof already completed

```text
HEAD: f20357ba10618ddecfdd2e274e93917fe508a983
Canonical contracts: 108/108 PASS
CMake configure: PASS
Native samples target: PASS
Existing vehicle/build/test/smoke gate: PASS
Result: P2A_LOCAL_CODE_GATE_PASS
```

This proves the exact-preview code and existing vehicle stack. It does not prove
real private asset transformation or `TERRAIN_VISIBLE_PASS`.

## Derivative foundation implemented on PR #7

The derivative branch adds only Python tooling, tests and documentation. It does
not change C++, the renderer, Box3D physics or the verified P2A sample.

### Closed derivative graph

```text
SOURCE_REVISION
├── EXACT_VISUAL_PREVIEW
│   └── OPTIMIZED_VISUAL
└── SURFACE_EVIDENCE
    └── ACCEPTED_SURFACE
        └── COLLISION_PROJECTION
```

Version 1 permits `READY` only for exact preview and conservative surface
evidence. A rehashed mutation cannot bypass owner review or make collision READY.

### Surface evidence semantics

Each raster cell may contain:

```text
lowest observed Y
highest observed Y
support count
source-tile mask
evidence quality
classification = OBSERVED_SURFACE_EVIDENCE or UNKNOWN
```

`UNKNOWN` is stored canonically and is never filled, blurred, averaged or made
collidable. Evidence quality is not ground confidence.

### Runtime policy recorded by the catalog

```text
render interest center  = CAMERA
physics interest center = VEHICLE
unknown surface         = NOT_COLLIDABLE_UNTIL_REVIEWED
source GLB/PLY parsing   = OFFLINE ONLY
visual LOD decision     = MEASURE_EXACT_PREVIEW_FIRST
geometry/texture budget = SEPARATE
```

## Current derivative contract scope

```text
previous exact P2A suite                  108
surface evidence contracts                 9
surface evidence verifier CLI              3
surface query contracts                    5
derivative catalog contracts               7
derivative catalog verifier CLI            3
---------------------------------------------
expected canonical total                 135
```

`135` is an expected count, not a PASS. The current PR #7 head must be executed
locally before this branch is described as green.

## Current blockers before the first terrain image

The derivative work does **not** add a new blocker to `TERRAIN_VISIBLE_PASS`.
The shortest honest path remains:

1. create the real owner-confirmed source-frame contract;
2. finalize the real 7+7 P1B bundle without privacy acknowledgement;
3. manually inspect only the printed shareable JSON;
4. rerun finalize with privacy acknowledgement and retain the private receipt v2;
5. build and independently verify the exact private preview pack;
6. launch `Jozz Vehicle → P2A Source Visual Preview`;
7. inspect all seven tiles, orientation, scale, mirror state, coverage and seams;
8. restart and confirm the same content-addressed result.

Success produces only:

```text
TERRAIN_VISIBLE_PASS
```

It does not produce pairing, accepted-world, collision or drive readiness.

## Correct path after `TERRAIN_VISIBLE_PASS`

```text
real PLY surface evidence
→ surface query/debug overlay
→ internal GLB↔PLY correspondence
→ sabotage fixture with equal bounds but wrong interior
→ adjacency and seam evidence
→ owner-selected Golden Drive Region
→ reviewed accepted surface
→ disposable collision projection
→ physics probes
→ first vehicle drive
```

## Parked until measured or required

- optimized visual LOD cooking;
- texture transcoding;
- native tile streaming and eviction;
- automatic ground classification;
- gap filling and DTM smoothing;
- accepted-surface editor;
- heightfield/collision cooker;
- full scan-terrain drive loop;
- reimport UI and JES implementation.

No source coordinates, original source names, absolute paths, direct source file
hashes or private scan payloads belong in Git.
