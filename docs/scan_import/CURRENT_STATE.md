# Scan import — current state

**Updated:** 2026-07-22  
**Authoritative branch:** `agent/project-refoundation-audit-v1`  
**Active draft PR:** #17  
**Exact current head:** read from GitHub Control Issue #11  
**Integration base:** `jozz-vehicle-sandbox-m0`

## Highest honest capability

```text
TERRAIN_VISIBLE_PASS
```

Owner-local evidence proved that one real private seven-tile source set can be:

```text
resolved exactly
→ packaged deterministically
→ independently verified
→ loaded in the native host
→ recognized by the owner
→ restarted on the same revision
```

Safe public runtime facts:

```text
tiles:      7
vertices:   1 409 687
triangles:  1 775 775
restart:    949 frames / 0 Sokol errors
```

Redacted milestone:

```text
docs/scan_import/TERRAIN_VISIBLE_PASS_2026_07_22_PL.md
```

## Capability boundary

```text
WORLD_SCALE_VALIDATED = false
TEXTURED_SOURCE_PREVIEW_READY = false
COLLISION_PROJECTION_READY = false
DRIVE_TEST_READY = false
```

The metre grid and source transform support a plausible scale. Final scale remains an
owner visual gate until the accepted vehicle is rendered on a recognizable road or
beside a known house/object.

## Mandatory product order

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
→ OWNER_FUN_VERDICT
```

Textures precede scale and collision because they provide the context required to
recognize road, shoulder, grass, buildings and the first useful drive region.

## Current integration topology

The remote now contains exactly three branches:

```text
main
jozz-vehicle-sandbox-m0
agent/project-refoundation-audit-v1
```

PR #17 is the only current integration/review surface. PR #13 and PR #16 were closed
without merge during branch cleanup. Their content was not lost: the current branch
is linearly ahead of the vehicle baseline and contains their integrated history.

```text
branch cleanup:       COMPLETE_TO_3
further deletion:     FORBIDDEN
retention tag debt:   3 missing refs
```

Exact branch record:

```text
docs/BRANCH_RETENTION_PLAN_2026_07_22.json
```

The three missing retention tags must be created from a capable git environment, but
this debt does not authorize recreating old branches or blocking current documentation
and texture planning work.

## Evidence table

| Capability | Status | Authority |
|---|---|---|
| inspection/source contracts | `PASS_CODE_AND_CI` | repository contracts |
| real 7 GLB + 7 PLY identity | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local evidence |
| source frame and privacy receipt | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local contracts |
| exact source resolution | `PASS_OWNER_PRIVATE_RUNTIME` | resolver receipt |
| geometry preview pack | `PASS_OWNER_PRIVATE_RUNTIME` | independent verifier |
| native geometry load | `PASS_OWNER_VISUAL_EVIDENCE` | real render |
| same-revision restart | `PASS_OWNER_RUNTIME_EVIDENCE` | repeated launch |
| terrain visible | `TERRAIN_VISIBLE_PASS` | owner acceptance |
| textured runtime | `NOT_IMPLEMENTED` | next campaign |
| final scale | `NOT_VALIDATED` | car/known-object scene |
| Golden Drive Region | `NOT_SELECTED` | owner visual gate |
| accepted surface | `NOT_STARTED` | separate authority |
| collision representation | `NOT_SELECTED` | comparative research |
| drive readiness | `NOT_READY` | contact/drop/drive proof |
| fun verdict | `NOT_RUN` | owner drive |

## Accepted source limitations

For the current proof the owner knowingly accepts:

- peripheral floating geometry and reconstruction walls;
- stretched/disconnected edge fragments;
- imperfect global framing;
- no textures in preview v1;
- no collision or traversability claim.

Classification:

```text
KNOWN_SOURCE_LIMITATIONS
NON_BLOCKING_FOR_TERRAIN_VISIBLE_PASS
```

The project will not clean the entire scan before proving one useful drive region.

## Textured Source Preview — next campaign

Historical experiments suggest:

```text
baseline:  max 1K
quality AB: optional max 2K
GPU compression: after working native render
```

The next brief must still verify the current seven-tile set and define:

- exact material/primitive/UV/image identity;
- deterministic pack/version boundary;
- embedded image and colour-space validation;
- 1K memory budget and optional 2K A/B;
- independent verifier;
- no-texture fallback and clear errors;
- fixed-camera screenshot matrix;
- same-revision restart;
- owner visual acceptance;
- non-colliding accepted vehicle as the later scale reference.

Preview v1 remains closed and geometry-only. Textures require a new adjacent capability
or explicit format version; they must not be smuggled into v1.

## Surface/collision parking

The former PR #7 branch was deleted during cleanup, but its exact commit remains
recoverable. The work is evidence, not accepted surface authority.

Collision remains blocked until:

1. textured preview acceptance;
2. vehicle scale-reference acceptance;
3. owner-selected Golden Drive Region;
4. comparison of triangle mesh, heightfield, parked surface evidence and Blender proxy.

## Current gates

```text
private gate:  CLEAR_FOR_EXISTING_SOURCE_SET
visual gate:   TEXTURED_SOURCE_PREVIEW_REQUIRED
owner gate:    TEXTURE_AND_SCALE_REFERENCE_REVIEW_REQUIRED
physics gate:  COLLISION_BLOCKED_UNTIL_TEXTURE_SCALE_AND_ROI
merge gate:    OWNER_REVIEW_REQUIRED
```

## Privacy

Never publish private paths, coordinates, source hashes, receipts, raw GLB/PLY,
original textures or credentials. Use logical IDs, redacted counts and bounded
capability claims.
