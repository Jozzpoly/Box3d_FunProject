# P2A Source Visual Preview

**Status:** implementation present on draft PR #5; final execution proof pending.  
**Purpose:** show verified private source geometry in the native sample host without turning scan evidence into authored-world or collision truth.

## Why P2A exists

The first visible terrain result must arrive early enough to give useful feedback, but it must not bypass the import truth boundary.

P2A therefore answers only:

> Can the exact GLB geometry from one verified source revision be transformed into lab-space metres and displayed consistently?

P2A does **not** answer:

- whether GLB and PLY interiors correspond;
- which surfaces are ground;
- whether tile seams are physically traversable;
- whether any geometry should become an accepted world patch;
- whether any triangle should become collision.

## Trust chain

```text
real GLB/PLY inspection
        ↓
confirmed source-frame contract
        ↓
verified P1B evidence bundle
        ↓
manual shareable privacy review
        ↓
bundle-bound P1B_BUNDLE_PASS receipt v2
        ↓
private render-only preview pack
        ↓
independent preview verifier
        ↓
native P2A Source Visual Preview Lab
```

No later stage is allowed to manufacture an earlier PASS.

## Private preview pack

The generator consumes:

- one independently verifiable P1B bundle;
- the exact private owner-gate receipt for that bundle and source revision;
- a private directory containing the original GLB files named by the immutable source package;
- an ignored local output directory.

It then:

1. verifies the bundle and receipt binding;
2. verifies each source GLB byte length and SHA-256;
3. parses all triangle primitives and node transforms;
4. applies source origin, units and source-to-lab axes;
5. reverses triangle winding only for an explicitly approved mirror;
6. rebuilds deterministic normals from transformed triangles;
7. serializes one render tile at a time;
8. calculates manifest bounds from the final float32 payload;
9. publishes transactionally under a content-addressed directory;
10. independently verifies the completed pack.

### Binary tile format v1

Header, little-endian:

```text
8 bytes   magic: JSPREV1\0
uint32    version = 1
uint32    tileId
uint32    vertexCount
uint32    indexCount
```

Then:

```text
vertexCount × (float32 position.xyz + float32 normal.xyz)
indexCount  × uint32
```

Indices are a triangle list. Textures, UVs, materials and collision data are deliberately absent from v1.

## Closed capability boundary

A valid manifest must state exactly:

```text
purpose = SOURCE_VISUAL_PREVIEW_ONLY
privacyClass = PRIVATE_LOCAL_ONLY
sourceGeometryVisible = true
texturesIncluded = false
internalGeometryCorrespondencePassed = false
acceptedWorld = false
collisionReady = false
```

Unknown fields, changed layouts, extra files, symlinks, stale receipt bindings and recomputed overclaims are rejected.

## Independent verifier

The Python verifier is the cryptographic trust boundary. It checks:

- exact closed manifest fields;
- canonical lower-case SHA-256 syntax;
- content-addressed manifest hash;
- exact file set;
- no symlinks;
- byte lengths and SHA-256 for every tile;
- binary magic/version/counts;
- finite float32 vertices;
- unit normals;
- in-range indices;
- per-tile bounds derived from final bytes;
- global bounds derived from tile bounds.

The native C++ reader repeats structural validation for defense in depth. It intentionally does not claim to recompute SHA-256.

## Native lab behavior

The sample is registered as:

```text
Jozz Vehicle → P2A Source Visual Preview
```

It supports:

- automatic selection only when exactly one complete local pack exists;
- explicit selection through `JOZZ_SCAN_PREVIEW_PACK`;
- whole-preview camera framing;
- per-tile visibility toggles;
- per-tile bounds;
- metre grid;
- lab axes;
- geometry counts;
- persistent UI warnings that the content is evidence-only.

It does not expose the private absolute path in the UI.

## Physics exclusion

P2A source files are covered by a static architecture test that rejects body, shape, mesh-shape, heightfield and terrain-creation APIs.

The lab may use renderer geometry registry types, camera types and world-coordinate math types. It must not create or mutate physical world content.

## Known limitations of the first proof

- geometry-only; no original textures;
- no PLY rendering;
- no GLB↔PLY interior correspondence;
- no seam quality classification;
- no frustum/streaming system;
- every tile is uploaded as one GPU mesh;
- renderer registry identity is currently based on a 32-bit hash, so collision-avoidance hardening should be reviewed before treating the loader as hostile-input infrastructure;
- source coordinates that are finite in Python but outside float32 range stop serialization, but the final clean error classification still needs an explicit regression test;
- final hosted/local execution on the exact branch head is not yet proven.

These limitations do not authorize collision or accepted-world use.

## Required local proof

Before `TERRAIN_VISIBLE_PASS`:

1. run the canonical scan suite and record the exact test count;
2. run the corrected Windows gate;
3. create the real confirmed frame;
4. produce the real P1B bundle and acknowledged receipt;
5. build the real preview pack;
6. run the independent preview verifier;
7. build `samples`;
8. launch the P2A lab;
9. inspect scale, up axis, mirror state, tile count, bounds and seams;
10. restart and confirm deterministic selection/layout.

## Promotion result

Success produces only:

```text
TERRAIN_VISIBLE_PASS
```

It does not produce:

```text
PAIRING_SEMANTICS_PASS
ACCEPTED_WORLD_PATCH_READY
COLLISION_PROJECTION_READY
DRIVE_TEST_READY
```

The next package after visual proof is internal GLB↔PLY correspondence plus adjacency/seam evidence.
