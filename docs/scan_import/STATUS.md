# Photogrammetry Import V2 — current status

**Updated:** 2026-07-21  
**Active branch:** `agent/p2a-source-visual-preview`  
**Stack base:** `agent/p1b-owner-gate-hardening@2d91d17428292234c9b560a2ef855761f445f54f`  
**Product draft PR:** #5

## Truth table

| Capability | Status | Evidence / blocker |
|---|---|---|
| P0 vehicle baseline | `PASS_LOCAL_EXACT_HEAD` | Windows build/validator/test/smoke passed locally on `2d91d174...` |
| P1A parser and synthetic contracts | `PASS_CI` | PR #1 workflow green |
| P1A real 7+7 inspection | `PASS_LOCAL_DETERMINISTIC` | two real reports; each 7 GLB, 7 PLY, 7 pairs, automatic gate pass; reports byte-identical |
| Pair bounds compatibility | `PASS_WITH_REVIEW` | all pair evidence remains `BOUNDS_ONLY`; interior correspondence is not asserted |
| Full source-frame contract machinery | `PASS_HOSTED_AND_LOCAL` | signed axes, handedness, mirror approval, local origin and round-trip validation implemented |
| Real owner-confirmed source frame | `NOT_CREATED` | MipMap report supports LOCAL_ENU/metric assumptions, but a real local origin and explicit owner confirmation are still required |
| Source package / proposal boundary | `PASS_CI` | exact source revision binding and adversarial validation covered by hosted matrix |
| Private/shareable evidence split | `PASS_CI_SYNTHETIC` | explicit allow-list; no source names, paths, bounds, hashes or free-form warnings |
| Transactional evidence bundle | `PASS_CI_SYNTHETIC` | content-addressed directory, cross-document checks, `COMPLETE.json`, tamper verifier |
| Independent bundle verifier | `PASS_CI_SYNTHETIC` | valid, tampered and missing bundle CLI cases pass on Windows/Linux |
| Owner-gate orchestrator | `PASS_HOSTED_AND_LOCAL` | 89/89 tests and corrected Windows gate passed locally on exact hardening head |
| Owner-gate receipt v2 | `IMPLEMENTED_NOT_REAL_RUN` | private receipt is bound to exact bundle SHA-256 and source revision; P2A rejects pending, stale or mismatched receipts |
| Real 7+7 bundle | `NOT_RUN` | requires owner-confirmed source frame |
| Manual shareable privacy review | `NOT_RUN` | waits for real bundle; exactly one review target will be printed |
| P2A private preview-pack generator | `IMPLEMENTED_NOT_FINAL_EXECUTION_PROVEN` | streams verified GLB tiles into content-addressed render-only binary tiles; requires exact P1B_BUNDLE_PASS receipt |
| P2A independent preview verifier | `IMPLEMENTED_NOT_FINAL_EXECUTION_PROVEN` | checks closed manifest schema, exact file set, symlinks, byte lengths, SHA-256, binary structure, bounds, normals and indices |
| P2A native runtime reader | `IMPLEMENTED_NOT_COMPILE_PROVEN_ON_FINAL_HEAD` | structural defense in C++; intentionally not a cryptographic verifier and cannot create Box3D bodies/shapes |
| P2A Source Visual Preview Lab | `IMPLEMENTED_NOT_REAL_ASSET_RUN` | geometry-only tile rendering, metre grid, axes, tile bounds/toggles and evidence-only warnings; no textures or collision |
| Internal GLB↔PLY correspondence | `BLOCKED` | starts only after real `P1B_BUNDLE_PASS`; occupancy/internal geometry evidence not implemented |
| P2B correspondence/seam preview | `NOT_STARTED` | requires internal correspondence and adjacency evidence |
| Accepted world patch | `NOT_STARTED` | no Golden Drive Region or authored surface review |
| Collision projection | `NOT_STARTED` | no accepted surface/cooker contract |
| Drive test | `NOT_STARTED` | no scan-terrain physics probes |
| JES transfer candidate | `NO` | requires reimport and a second real scan |

## Locally proven on the exact P1B hardening head

```text
HEAD: 2d91d17428292234c9b560a2ef855761f445f54f
Canonical scan contracts: 89/89 PASS
Windows gate: build 3/3, validator PASS, test PASS, smoke 0 errors
Real inspection discovery: 2 byte-identical reports
Each report: 7 GLB / 7 PLY / 7 pairs / compatible-review
```

The local proof above validates the base of PR #5. It does not validate later P2A commits.

## Implemented in P2A

### Private projection boundary

The P2A generator:

- consumes one independently verified P1B bundle;
- requires a private owner-gate receipt with `status = P1B_BUNDLE_PASS`;
- binds the receipt to the exact `bundleContentSha256` and `sourceRevisionId`;
- verifies every private source GLB against immutable byte-length and SHA-256 records;
- parses triangle primitives and node transforms;
- applies the confirmed source-frame origin, units and axis matrix;
- corrects winding for explicitly approved mirrored transforms;
- rebuilds deterministic normals from transformed triangle geometry;
- writes one tile at a time so the whole scan is not retained in Python memory;
- publishes a content-addressed directory transactionally;
- never writes collision data or an accepted-world claim.

Every preview manifest is fixed to:

```text
purpose = SOURCE_VISUAL_PREVIEW_ONLY
privacyClass = PRIVATE_LOCAL_ONLY
texturesIncluded = false
internalGeometryCorrespondencePassed = false
acceptedWorld = false
collisionReady = false
```

### Native render-only lab

The native sample:

- reads only the prepared preview format, not GLB or PLY;
- validates the closed structural contract before GPU registration;
- shows per-tile geometry, bounds, metre grid and lab axes;
- allows individual tile visibility toggles;
- clearly labels the result as source evidence only;
- does not display the private absolute pack path;
- contains no Box3D body, shape, mesh-shape, heightfield or terrain-creation API.

The Python verifier remains the cryptographic trust boundary. The C++ reader performs defense-in-depth structural validation and does not claim to replace SHA-256 verification.

## Current contract-test scope

The canonical runner now includes:

```text
P1/P1B tests                       89
P2A generator/verifier tests       11
P2A runtime architecture tests      3
-------------------------------------
Canonical intended total          103
```

`103` is the expected suite size from the registered test modules. It must not be recorded as PASS until the final branch head is actually executed.

## Current blockers before first terrain image

1. create the real owner-confirmed source-frame contract;
2. run `scan_owner_gate.py finalize` against the real 7+7 inspection;
3. manually review only the printed `shareable/inspection.shareable.json`;
4. rerun finalize with privacy acknowledgement and retain the private v2 receipt;
5. execute all 103 contracts on the exact P2A head;
6. configure and build the `samples` target on Windows;
7. build and independently verify the real private preview pack;
8. launch `P2A Source Visual Preview` and review orientation, scale, tile coverage and seams.

## Promotion criteria: `TERRAIN_VISIBLE_PASS`

The first terrain image is accepted only when:

- the exact preview pack passes the independent verifier;
- all seven expected tiles load;
- source-to-lab orientation is visually correct;
- metre scale is plausible against known scene dimensions;
- the result is not mirrored;
- tile bounds and seams can be inspected;
- restart produces the same content-addressed pack and layout;
- UI continues to state `NO COLLISION` and `NOT ACCEPTED WORLD`.

## NEXT after `TERRAIN_VISIBLE_PASS`

- internal occupancy/surface-support correspondence GLB↔PLY;
- sabotage fixture with identical bounds and mismatched interior geometry;
- tile adjacency and seam evidence;
- P2B diagnostic overlay;
- selection of a small Golden Drive Region.

## PARKED

- automatic ground classifier;
- accepted-world editor;
- heightfield/collision cooker;
- scan-terrain physics probes;
- full vehicle drive loop;
- reimport UI;
- JES implementation.

No source coordinates, private source names, absolute paths or source file hashes belong in this status document.
