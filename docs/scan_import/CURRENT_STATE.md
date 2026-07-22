# Scan import — current state

**Updated:** 2026-07-22  
**Authoritative branch:** `agent/scan-terrain-r1b-consolidated-integration`  
**Active draft PR:** #13  
**Exact current head:** GitHub Control Issue #11  
**Integration base:** `jozz-vehicle-sandbox-m0`  
**Milestone proof revision:** recorded in `TERRAIN_VISIBLE_PASS_2026_07_22_PL.md`

## 1. Highest honest capability

```text
TERRAIN_VISIBLE_PASS
```

A real private seven-tile source set was resolved against verified evidence,
converted into an exact render-only pack, independently verified, displayed in the
native host, accepted by the owner as recognizable geometry and loaded again on the
same code revision.

Redacted proof:

```text
docs/scan_import/TERRAIN_VISIBLE_PASS_2026_07_22_PL.md
```

## 2. Critical boundary after owner feedback

```text
WORLD_SCALE_VALIDATED = false
TEXTURED_SOURCE_PREVIEW = required next
COLLISION_PROJECTION_READY = false
DRIVE_TEST_READY = false
```

The metre grid and source transform support a plausible scale, but final scale
validation is intentionally deferred until the accepted vehicle is visible on the
road or beside a known house/object.

Textures must precede that scene. They provide the perceptual context required to
recognize road, shoulder, grass, buildings and the first drive region.

Mandatory order:

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
```

## 3. Product integration state

PR #13 remains the consolidated P0–R1B product integration surface. It preserves the
existing ancestry without rebase, squash, force-push or history rewrite.

Included lineage:

```text
photogrammetry/import-v2-foundation
→ P1 inspection
→ P1B contracts and evidence
→ owner-local privacy gate
→ P2A exact source geometry preview
→ R1B source resolution and owner flow
→ automation safety foundation
→ repository readiness integration
```

PR #15 was owner-reviewed and merged into the authoritative PR #13 branch. It is no
longer a separate pending governance layer.

Historical PRs #1–#5, #8 and #9 remain preserved as superseded lineage. Divergent
surface-evidence work is parked in Issue #14 and excluded from the current source
preview path.

PR #13 must not become an unbounded branch for textures, collision and world
production. Its integration strategy is part of the project re-foundation review.

## 4. Evidence table

| Capability | Status | Evidence authority | Consequence |
|---|---|---|---|
| Inspection machinery | `PASS_CODE_AND_CI` | repository contracts | preserve |
| Real 7 GLB + 7 PLY identity | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local bundle/source | preserve privately |
| Source-frame contract | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-confirmed local contract | preserve |
| P1B bundle/privacy receipt | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local artifacts | preserve privately |
| Source resolution | `PASS_OWNER_PRIVATE_RUNTIME` | exact physical matches | preserve |
| Geometry preview pack | `PASS_OWNER_PRIVATE_RUNTIME` | independent verifier | preserve |
| Native geometry load | `PASS_OWNER_VISUAL_EVIDENCE` | real native render | complete |
| Same-revision restart | `PASS_OWNER_RUNTIME_EVIDENCE` | repeated native launch | complete |
| Terrain visible | `TERRAIN_VISIBLE_PASS` | owner visual acceptance | milestone closed |
| Textures in runtime | `NOT_IMPLEMENTED_IN_PREVIEW_V1` | v1 closed contract | next campaign |
| Final world scale | `NOT_VALIDATED` | needs car/known-object scene | blocked on textures |
| Golden Drive Region | `NOT_SELECTED` | owner visual decision | blocked on textured scene |
| GLB↔PLY interior correspondence | `NOT_PROMOTED` | separate evidence work | later decision |
| Accepted surface | `NOT_STARTED` | separate owner authority | blocked |
| Collision representation | `NOT_SELECTED` | comparative research required | blocked |
| Drive readiness | `NOT_READY` | Box3D contact/vehicle proof | blocked |
| Fun drive verdict | `NOT_RUN` | owner manual drive | final gate of next phase |

## 5. What the completed milestone proved

```text
real private source set can be located safely
→ exact assets can be authenticated
→ geometry can be transformed into lab metres
→ seven tiles can be packaged deterministically
→ independent verification can catch corruption/overclaim
→ native loader can display the selected pack
→ owner can recognize the real place
→ the same revision can restart repeatably
```

Runtime facts safe to publish:

```text
tiles:      7
vertices:   1 409 687
triangles:  1 775 775
restart:    949 frames, 0 Sokol errors
```

## 6. Accepted limitations of the first source set

Owner knowingly accepts for the current proof:

- peripheral floating shelves and islands;
- vertical reconstruction walls;
- stretched or disconnected edge artifacts;
- incomplete global framing caused by extreme bounds;
- no texture/material rendering;
- no collision and no traversability claim.

Classification:

```text
KNOWN_SOURCE_LIMITATIONS
NON_BLOCKING_FOR_TERRAIN_VISIBLE_PASS
MAY_REQUIRE_MASKING_OR_AUTHORING_BEFORE_DRIVE_REGION_PROMOTION
```

The project will not clean the entire scan before one useful drive proof. The first
physics experiment will eventually target a manually selected, high-quality central
region.

## 7. Texture evidence already available

Historical offline experiments established useful starting facts:

```text
first preview profile: max 1K
quality comparison:    optional max 2K A/B
GPU compression:       not a blocker before working runtime render
pipeline:               staged, cacheable and resumable
validation:             fixed-camera runtime screenshots required
```

For the earlier source set, max-1K textures were estimated at roughly 96 MiB decoded
RGBA8 plus mipmaps and showed very small measured loss against 2K. These numbers are
historical evidence, not a final budget for the current seven-tile set.

P2A preview format v1 intentionally contains positions, normals and indices only.
Textures must use a new explicit capability/format version or an adjacent pack. They
must not be smuggled into v1 while keeping `texturesIncluded=false`.

## 8. Next required campaign brief

The next implementation package must define **Textured Source Preview**, including:

- exact material/primitive/UV bindings;
- embedded image verification;
- color-space and baseColor behavior;
- texture cook profile 1K;
- optional 2K A/B;
- mip and memory budget;
- deterministic manifest and independent verifier;
- native no-texture fallback;
- fixed-camera screenshot matrix;
- same-revision restart;
- owner visual acceptance;
- vehicle model rendered as a non-colliding scale reference after textures pass.

The brief must be prepared after project re-foundation inventory and before product
implementation.

## 9. Current gates

```text
private gate:  CLEAR_FOR_COMPLETED_GEOMETRY_MILESTONE
visual gate:   TEXTURED_SOURCE_PREVIEW_REQUIRED
owner gate:    TEXTURE_AND_SCALE_REFERENCE_REVIEW
physics gate:  COLLISION_BLOCKED_UNTIL_TEXTURE_SCALE_AND_ROI
```

Issue #11 remains the mutable authority and must be updated only by an explicit owner
action. Versioned documents do not move the authoritative head themselves.

## 10. Surface/collision parking

Issue #14 preserves the exact historical surface-evidence branch. Its previous gate
`TERRAIN_VISIBLE_PASS` has been satisfied, so the work may now be **re-evaluated**.
That does not mean it is accepted or merge-ready.

Required before reactivation:

1. complete texture and scale-reference planning;
2. select a real drive region visually;
3. compare parked surface evidence with triangle mesh, heightfield and manual Blender
   collision proxy approaches;
4. re-evaluate exact base/dependencies;
5. preserve accepted-surface and collision promotion as separate owner gates.

## 11. Privacy boundary

Never publish:

- private paths;
- coordinates or identifiable location metadata;
- raw GLB/PLY or original texture payloads;
- source hashes/receipts/private state;
- credentials.

Use logical IDs, redacted counts and capability boundaries.

## 12. Project-wide re-foundation

Current manual audit:

```text
docs/PROJECT_REFOUNDATION_AUDIT_2026_07_22_PL.md
```

Durable project intent:

```text
docs/PROJECT_CHARTER_PL.md
```

The re-foundation is docs/governance/inventory work. It does not authorize collision,
accepted-world promotion or a merge of PR #13.
