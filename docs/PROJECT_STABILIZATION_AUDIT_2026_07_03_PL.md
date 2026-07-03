# Project Stabilization Audit — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Purpose: prepare the project for Codex stabilization before serious visual asset import

## 1. Current verified state

The active lab is:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

The panel/HUD identifies the current state as:

```text
Jozz Vehicle Lab M2.5 + M3A/M3B debug
```

Validated by Jozz:

```text
M2.5 one-corner wheel-joint lab works
M3A asset-derived primitive defaults work
M3B.1 semantic preview ownership fix works
M3B.2-prep runtime audit metadata path works
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
runtime metadata -> data source only, no mesh renderer yet
```

### Runtime metadata direction

The metadata loader is a useful bridge. It reads the existing audit report first and falls back safely. This is a good stepping stone toward later asset contract/runtime import.

## 3. Main problems now

### Problem A — sample file is becoming too dense

`samples/sample_jozz_vehicle_lab.cpp` now owns too much:

```text
sample registration
physics body/joint setup
M2.5 controls
M3A defaults
M3B semantic preview mapping
runtime metadata usage
HUD/panel text
```

This is still workable, but it is no longer a good place to keep adding rendering/import complexity.

Codex should split by responsibility before adding visual mesh work.

Recommended direction:

```text
jozz_vehicle_asset_metadata.*     already exists
jozz_vehicle_primitive_corner.*   future extracted M2.5/M3A corner lab logic
jozz_vehicle_debug_draw.*         future semantic preview helpers
sample_jozz_vehicle_lab.cpp       should become thin sample registration/glue
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

### Problem D — no automated test for metadata parser

`jozz_vehicle_asset_metadata.cpp` has no unit/smoke test. If the audit report shape changes, the loader could silently fall back.

A future lightweight test or validation sample should check at least:

```text
runtime report can parse known semantic nodes
wheel radius derives to about 0.51 m
wheel width derives to about 0.44 m
travel hint derives to about 0.70 m
```

### Problem E — no visual asset renderer boundary yet

There is not yet a clean interface for:

```text
load mesh
hold visual asset handle
draw visual asset at debug transform
attach visual to primitive wheel body later
```

Codex should prepare this boundary before rendering real models.

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
```

## 6. Cleanup still worth doing

Codex should do these in small commits:

1. Re-run/verify build from clean checkout.
2. Make `README_FOR_AGENTS.md` and `CURRENT_STATE_INDEX_PL.md` fully agree with the latest M3B.2 validation state.
3. Split the giant sample file only if the build can be kept green after each extraction.
4. Add a tiny metadata parser validation path if practical.
5. Ensure docs do not contain stale “pending” claims for M3A/M3B validation.
6. Keep historical docs but do not treat old M2/M2.1/M2.2/M2.3 as active architecture.

## 7. Recommended Codex mission

Codex should **stabilize and organize**, not add a big feature immediately.

Primary mission:

```text
Turn the current one-file lab into a clean foundation ready for a first visual wheel mesh import gate.
```

Success means:

```text
build green
manual lab still works
runtime metadata still loads
fallback still safe
sample file less overloaded or clearly prepared for extraction
next feature gate documented and small
```

## 8. Next feature gate after stabilization

Only after stabilization:

```text
M3B.2 — render one static visual wheel mesh at origin
```

Rules for that gate:

```text
not attached to physics yet
not skinned
not animated
not collision
not full rig
not full vehicle
```

The first visual mesh goal is simply to prove that the rendering/import path can display Jozz's wheel asset safely in the sample host.