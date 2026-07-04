# Project Stabilization Audit — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Purpose: record stabilization risks and the implemented M3B.2 visual-only import proof before broader vehicle work

## 1. Current verified state

The active lab is:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

The panel/HUD identifies the current state as:

```text
Jozz Vehicle Lab M2.5 + M3A/M3B.2.1 debug
```

Validated by Jozz:

```text
M2.5 one-corner wheel-joint lab works
M3A asset-derived primitive defaults work
M3B.1 semantic preview ownership fix works
M3B.2-prep runtime audit metadata path works
M3B.2 static visual-only wheel mesh proof is implemented
M3B.2.1 static textured visual-only wheel mesh proof is implemented
```

The latest Jozz screenshot shows:

```text
M3B metadata: runtime audit
metadata: loaded runtime asset audit report
source: ../../assets/reports/asset_audit_latest.json
```

This means the runtime metadata path is reachable in Jozz's current local setup.

## 2. What is stable enough to keep

### Physics baseline

Keep the M2.5 wheel-joint model:

```text
Frame A = rest wheel-center anchor on chassis/root
Frame B = wheel body center
Rest drop = explicit/tuned value
spring rest = wheel-joint translation 0
```

Do not return to the older visual-mount-as-frame-A idea.

### Debug interaction model

Keep the separation:

```text
structural setup -> pending values + Apply rig rebuild
live root stress test -> realtime root/chassis movement
semantic preview -> debug only, no physics authority
runtime metadata -> data source only
static visual proof -> one wheel mesh at fixed debug origin, no physics authority
```

### Runtime metadata direction

The metadata loader is a useful bridge. It reads the existing audit report first and falls back safely. This is a good stepping stone toward later asset contract/runtime import.

## 3. Main problems now

### Problem A — sample file was becoming too dense

`samples/sample_jozz_vehicle_lab.cpp` was owning too much:

```text
sample registration
physics body/joint setup
M2.5 controls
M3A defaults
M3B semantic preview mapping
runtime metadata usage
HUD/panel text
```

This is partially addressed. Asset-derived primitive dimensions, semantic debug preview, and the narrow static visual mesh proof now live in helper modules. The corner lab class still owns physics setup/UI and should not become the place for full vehicle assembly.

Current direction:

```text
jozz_vehicle_asset_metadata.*     already exists
jozz_vehicle_asset_dimensions.*   asset-derived primitive defaults
jozz_vehicle_debug_preview.*      semantic preview helpers
jozz_vehicle_visual_mesh.*        narrow static visual-only glTF proof
jozz_vehicle_primitive_corner_lab.* owns M2.5/M3A/M3B.2.1 corner lab logic
sample_jozz_vehicle_lab.cpp       thin sample registration/M1 smoke glue
jozz_vehicle_validation.cpp       CLI metadata/defaults guard
```

Do not do a giant rewrite. Extract in small safe steps.

### Problem B — metadata loader reads audit report, not final asset contract

The loader currently reads:

```text
assets/reports/asset_audit_latest.json
```

This is acceptable for M3B.2-prep, but not final import architecture.

Future importer should likely use a dedicated asset contract/runtime metadata format instead of depending forever on an audit output file.

### Problem C — fallback data is duplicated

Fallback semantic points are intentionally duplicated inside `jozz_vehicle_asset_metadata.cpp`.

This keeps the lab robust today, but creates future drift risk.

Codex should not remove fallback immediately. It should document and later replace it with a generated/staged asset metadata file or reliable runtime path staging.

### Problem D — metadata parser guard is intentionally small

`jozz_vehicle_validation.exe` now provides a narrow CLI guard for the runtime/fallback metadata path.

It checks at least:

```text
runtime report can parse known semantic nodes
wheel radius derives to about 0.51 m
wheel width derives to about 0.44 m
travel hint derives to about 0.70 m
```

### Problem E — visual asset renderer boundary is only a thin proof

There is now a minimal interface for:

```text
load mesh
hold visual asset handle
draw visual asset at debug transform
attach visual to primitive wheel body later
```

It is intentionally narrow: first mesh primitive, embedded buffer path, fixed debug transform, no materials, no animation, no skinning, no collision and no full importer contract yet. M3B.3 should attach this visual-only wheel mesh to the primitive wheel body before any full rig/import work.

## 4. What should not be changed during stabilization

Do not change:

- Box3D internals;
- wheel-joint rest-anchor physics model;
- keyboard hotkeys;
- primitive wheel physics shape;
- `restDrop` semantics;
- M3B semantic preview ownership model;
- asset source files;
- generated audit report unless explicitly regenerating with the existing tool.

Do not start:

- full vehicle assembly;
- steering;
- mesh collision;
- full glTF renderer;
- skinning/animation;
- suspension/damper/cardan procedural rig.

## 5. Cleanup done before handoff

Completed before this handoff:

```text
M3B.2-prep runtime metadata path validated by Jozz screenshot
metadata loader fallback whitespace cleaned
separate M3B.2 runtime metadata validation doc created
asset-derived dimensions moved out of the sample file
semantic debug preview moved out of the sample file
M3B.2 static visual-only wheel mesh proof added
asset contracts hardened with nodeIndexHint/nodePathHint/role metadata
```

## 6. Cleanup still worth doing

Codex should do these in small commits:

1. Re-run/verify build from clean checkout.
2. Manually inspect the M3B.2 static visual proof in the sample host.
3. Split the remaining corner lab physics/UI only if the build can be kept green after each extraction.
4. Add a tiny metadata parser validation path if practical.
5. Ensure docs do not contain stale pending claims for M3A/M3B/M3B.2 validation.
6. Keep historical docs but do not treat old M2/M2.1/M2.2/M2.3 as active architecture.

## 7. Recommended Codex mission

Codex should continue in small gates, not add a big feature immediately.

Primary mission:

```text
Turn the current one-corner lab into a clean foundation ready for visual-only wheel attachment.
```

Success means:

```text
build green
manual lab still works
runtime metadata still loads
fallback still safe
sample file less overloaded or clearly prepared for the next extraction
next feature gate documented and small
```

## 8. Next feature gate after M3B.2

After validating the static proof, the next visual-only gate was:

```text
M3B.3 — visual-only wheel mesh attached to primitive wheel body
```

Rules for that gate:

```text
attached visually to the primitive wheel body transform
not skinned
not animated
not collision
not full rig
not full vehicle
```

The goal is only to prove that a Jozz wheel mesh can follow the primitive wheel body visually. It must not become mesh collision, full suspension rigging, or vehicle assembly.

Post-audit update:

```text
M3B.3 is implemented as an attached visual-only wheel mesh proof.
The next small gate is M3C runtime asset contract readiness, not full rig/import.
```
