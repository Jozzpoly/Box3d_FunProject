# README_FOR_AGENTS — Jozz Vehicle Box3D Native

Status: M2.5 + M3A/M3B.3 + M4 Foundation validated; M5/M5.1 played by Jozz; M5.2 Wheel & Steering Foundations landed: steering convention fixed at the sign level (left = -Z; smoke asserts SIGNED direction now), virtual tie rod + Ackermann linkage, sphere wheels by default after probe data showed cylinder facets caused the at-speed hopping (front contact 31% -> 100%), contact tuning / per-axle suspension / CG drop / telemetry plots in the lab UI. Awaiting Jozz's re-test. See docs/M5_2_WHEEL_STEERING_FOUNDATIONS_PL.md
Date: 2026-07-05
Owner/creative director: Jozz / Przemek  
Working branch: `jozz-vehicle-sandbox-m0`

## What this branch is

This branch starts **Jozz Vehicle Box3D Native**, a Windows/native vehicle sandbox built on top of Box3D.

The long-term goal is to use Box3D as a proven physics/render-host foundation, then grow a Jozz Vehicle game/lab layer around vehicle assembly, wheel suspension, visual rigs, and Blockbench-authored parts.

## Current reality

Jozz Vehicle currently lives inside the existing Box3D `samples` host.

Current active samples:

```text
Category: Jozz Vehicle
Sample:   M5 First Drivable        <- drivable 4-corner vehicle (new 2026-07-05)
Source:   samples/jozz_vehicle_m5_drivable_lab.cpp + samples/jozz_vehicle_m5_vehicle.cpp

Sample:   Lab M2 Primitive Corner  <- isolated corner lab, still active
Source:   samples/sample_jozz_vehicle_lab.cpp + samples/jozz_vehicle_primitive_corner_lab.cpp
Panel:    Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 foundation debug
```

The sample host gives windowing, camera, debug draw, ImGui, input, registration and build integration. A separate executable may happen later, but it is not the current blocker.

## Read first

Read in this order before making changes:

0. `docs/M5_2_WHEEL_STEERING_FOUNDATIONS_PL.md` (current vehicle physics state: steering convention, wheel shapes, probe data, soft-tire roadmap) then `docs/M5_1_FEEL_TUNING_IMPLEMENTATION_REPORT_PL.md` + `docs/M5_1_FEEL_TUNING_HANDOFF_2026_07_05_PL.md` + `docs/M5_FIRST_DRIVABLE_PL.md` + `docs/adr/0005-m5-first-drivable-before-m4c.md`
1. `docs/CURRENT_STATE_INDEX_PL.md`
2. `docs/M4_FOUNDATION_SUSPENSION_RIG_PLAN_PL.md`
3. `docs/ASSET_CONTRACT_RUNTIME_V1_PL.md`
4. `docs/SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md`
5. `docs/M4_MANUAL_SMOKE_2026_07_05_PL.md`
6. `docs/CODEX_HANDOFF_M4_FOUNDATION_MAIN_READY_PL.md`
7. `docs/CODEX_START_PROMPT_M4_FOUNDATION_PL.md`
8. `docs/PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md`
9. `docs/CODEX_HANDOFF_M3_STABILIZATION_IMPORT_PREP_PL.md`
10. `docs/M3B_2_RUNTIME_METADATA_VALIDATION_PL.md`
11. `docs/M3B_2_PREP_RUNTIME_METADATA_REPORT_PL.md`
12. `docs/M3B_SEMANTIC_PREVIEW_ANCHORING_FIX_PL.md`
13. `docs/M3B_SEMANTIC_DEBUG_PREVIEW_IMPLEMENTATION_REPORT_PL.md`
14. `docs/M3A_IMPLEMENTATION_REPORT_PL.md`
15. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
16. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
17. `docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md`
18. `assets/README.md`
19. `assets/reports/asset_audit_latest.md`
20. `samples/sample_jozz_vehicle_lab.cpp`
21. `samples/jozz_vehicle_asset_contract.h`
22. `samples/jozz_vehicle_asset_contract.cpp`
23. `samples/jozz_vehicle_corner_rig.h`
24. `samples/jozz_vehicle_corner_rig.cpp`
25. `samples/jozz_vehicle_visual_asset.h`
26. `samples/jozz_vehicle_visual_asset.cpp`
27. `samples/jozz_vehicle_asset_metadata.h`
28. `samples/jozz_vehicle_asset_metadata.cpp`
29. `samples/jozz_vehicle_asset_dimensions.h`
30. `samples/jozz_vehicle_asset_dimensions.cpp`
31. `samples/jozz_vehicle_debug_preview.h`
32. `samples/jozz_vehicle_debug_preview.cpp`
33. `samples/jozz_vehicle_primitive_corner_lab.h`
34. `samples/jozz_vehicle_primitive_corner_lab.cpp`
35. `samples/jozz_vehicle_visual_mesh.h`
36. `samples/jozz_vehicle_visual_mesh.cpp`
37. `samples/jozz_vehicle_image_decode.h`
38. `samples/jozz_vehicle_image_decode.cpp`
39. `samples/jozz_vehicle_validation.cpp`
40. `samples/sample_joint.cpp` sections `WheelJoint` and `Driving` only as reference

Useful background:

- `docs/PROJECT_AUDIT_2026_07_03_PL.md`
- `docs/FOUNDATION_GROUNDING_PHASE_PLAN_PL.md`
- `docs/PRE_RIG_IMPORT_READINESS_AUDIT_PL.md`
- `docs/ASSET_CONTRACT_V2_DRAFT_PL.md`
- `docs/adr/0001-project-scope.md`
- `docs/adr/0002-orientation-policy.md`
- `docs/adr/0003-physics-v0-wheel-joint.md`
- `docs/adr/0004-renderer-strategy.md`

## Authoritative baseline

M2.5 is still the current wheel-corner physics baseline. M3A adds asset-derived primitive defaults. M3B adds semantic preview and runtime audit metadata loading. M3B.2 adds one static visual-only wheel mesh proof at a fixed debug origin. M3B.2.1 adds baseColor texture loading for that same static proof. M3B.3 attaches that same visual-only wheel mesh to the primitive wheel body with an explicit render-only correction transform. M3B.3 hardening centers the primitive cylinder on the wheel body origin and adds a toggle for the orange primitive wheel debug shape; hiding it does not disable the physics wheel body or collision. M4 Foundation adds sidecar asset contract runtime, one-sided suspension visual proof, contract point overlay, and moving endpoint debug preview without changing physics authority. M5 First Drivable (2026-07-05) scales the same rest-anchor corner model to a dynamic four-corner vehicle with engine-native front steering in a render-free physics module shared with the validation CLI; the corner lab remains the isolated tuning environment. The same day, the Jozz layer deduplicated its jsmn helpers into `jozz_vehicle_json`, centralized asset path resolution in `jozz_vehicle_asset_paths`, and made the metadata module's built-in table the only fallback constants source.

Do not override this with older M2/M2.1/M2.2/M2.3 assumptions.

Core rules:

```text
b3WheelJoint implicit spring rest = translation 0
Frame A = rest wheel-center anchor on chassis/root
Frame B = wheel center / wheel body origin
Rest drop = explicit chassis-to-rest-wheel-center offset
Visual sockets are not automatically physics frames
```

M5 vehicle direction convention (settled in M5.2 after two wrong guesses;
do NOT re-derive it casually, the validation smoke asserts it signed):

```text
forward = +X, up = +Y, right = forward x up = +Z, LEFT = -Z
positive steering angle = left turn (rotates +X toward -Z about +Y)
```

M3 status:

```text
M3A: wheel radius/width and travel hint derive from asset audit metadata
M3B.1: semantic preview is debug-only and follows correct ownership
M3B.2-prep: runtime metadata loads asset_audit_latest.json if reachable, with fallback
M3B.2: one Offroad_Big_Wheels glTF mesh primitive renders at a fixed debug origin
M3B.2.1: that static wheel mesh can load TEXCOORD_0 + pbr baseColorTexture PNG data URI
M3B.3: that same visual-only wheel mesh can follow the primitive wheel body
M3B.3 hardening: primitive collision cylinder is centered on body origin; primitive wheel debug shape can be hidden while physics remains active
M4F.1: one-sided suspension contract resolves from sidecar + source glTF, not audit report
M4A: One_Sided_wheel_mount.gltf can render visual-only at the one-corner rig
M4B narrow: damper/cardan endpoint preview is debug-only and follows wheel travel on wheel-side endpoints
M4 manual smoke 2026-07-05: Jozz screenshots confirm suspension model, texture, transparency and helper lines are visible
```

Latest Jozz validation showed:

```text
M3B metadata: runtime audit
metadata: loaded runtime asset audit report
source: ../../assets/reports/asset_audit_latest.json
```

## Runtime separation rule

Keep these separate:

```text
Structural setup
  - rig height, rest drop, wheel radius, wheel width, collision toggle
  - pending values
  - requires Apply rig rebuild
  - primitive wheel cylinder is centered on body origin because Frame B is wheel center

Live root stress test
  - realtime chassis/root movement
  - Q/E and slider
  - must not rebuild bodies/joints

Semantic debug preview
  - debug drawing only
  - wheel schematic follows wheel/body
  - suspension schematic follows chassis/root
  - no physics authority

Runtime metadata
  - reads audit report when reachable
  - falls back safely when not reachable

Asset contract runtime
  - reads assets/contracts/*.asset.json and source glTF directly
  - resolves role/category/node hints into positions after composed node transforms
  - used for M4 suspension visual proof and validator checks
  - audit reports remain diagnostics, not the M4 runtime contract source

Static visual proof
  - loads a narrow subset of Offroad_Big_Wheels.gltf
  - supports one baseColor PNG data URI through WIC on Windows
  - draws one mesh at a fixed debug origin
  - not attached to physics

Attached visual proof
  - reuses the same Offroad_Big_Wheels.gltf mesh
  - draws through JozzVehicleVisualMesh::DrawAtTransform(...)
  - follows the primitive wheel body transform
  - uses a local render-only correction to center and orient the authored wheel
  - centers against the loaded mesh bounds when available, with semantic points as fallback
  - no material/skin/animation/collision/full importer yet

Primitive wheel debug shape
  - orange Box3D debug shape for the primitive collision wheel
  - can be hidden from the Jozz panel through the debug adapter hidden-shape path
  - remains visual/debug-only; physics and collision stay active

Suspension visual foundation
  - loads one_sided_wheel_mount.asset.json through JozzVehicleAssetContract
  - loads One_Sided_wheel_mount.gltf through JozzVehicleVisualAsset
  - draws a visual-only mount proof at the rest wheel-center frame
  - draws contract points and moving endpoint preview for damper/cardan roles
  - does not define wheel-joint frames, restDrop, collision, or constraints
```

## Current assets

Source glTF files live in `assets/source/`.

Current source models:

```text
Asset_Dumper.gltf
Cardan_shaft.gltf
Offroad_Big_Wheels.gltf
One_Sided_wheel_mount.gltf
```

The current audit report intentionally records duplicate roots/nodes and unfinished orientation decisions. Do not trust node names alone.

Runtime metadata code:

```text
samples/jozz_vehicle_asset_metadata.h
samples/jozz_vehicle_asset_metadata.cpp
```

Current visual-only proof code:

```text
samples/jozz_vehicle_primitive_corner_lab.h
samples/jozz_vehicle_primitive_corner_lab.cpp
samples/jozz_vehicle_visual_mesh.h
samples/jozz_vehicle_visual_mesh.cpp
samples/jozz_vehicle_image_decode.h
samples/jozz_vehicle_image_decode.cpp
```

Current M4 foundation code:

```text
samples/jozz_vehicle_asset_contract.h
samples/jozz_vehicle_asset_contract.cpp
samples/jozz_vehicle_corner_rig.h
samples/jozz_vehicle_corner_rig.cpp
samples/jozz_vehicle_visual_asset.h
samples/jozz_vehicle_visual_asset.cpp
```

Current M5 drivable code:

```text
samples/jozz_vehicle_m5_vehicle.h        <- physics prefab module, no gfx deps
samples/jozz_vehicle_m5_vehicle.cpp
samples/jozz_vehicle_m5_drivable_lab.h
samples/jozz_vehicle_m5_drivable_lab.cpp
samples/jozz_vehicle_m5_test_course.h    <- ramps/washboard/rough terrain/props, gfx-free content
samples/jozz_vehicle_m5_test_course.cpp
```

Shared Jozz infrastructure (2026-07-05 dedup):

```text
samples/jozz_vehicle_json.h / .cpp        <- shared jsmn token helpers
samples/jozz_vehicle_asset_paths.h / .cpp <- shared asset path resolver
```

Asset audit tools:

```powershell
py tools\asset_audit.py
py tools\asset_contract_audit.py
```

## Current hotkeys

Lab M2 Primitive Corner:

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

M5 First Drivable:

```text
W/S    drive forward/reverse
A/D    steer left/right
Space  brake
T      third-person camera toggle
```

Do not use `[` or `]`; they are global sample switching keys.

Before adding any shortcut, check and update:

1. `docs/HOTKEY_AUDIT_PL.md`
2. `samples/main.cpp`
3. `samples/gfx/keycodes.h`
4. `samples/sample_jozz_vehicle_lab.cpp`
5. `samples/jozz_vehicle_primitive_corner_lab.cpp`
6. `samples/jozz_vehicle_m5_drivable_lab.cpp`

## Non-negotiables

- Keep commits small and understandable.
- Do not trust node names alone; duplicate node names exist in current glTF exports.
- Always compose parent transforms before reading socket/axis positions.
- Keep visual rig, physics prefab, authoring asset data, and debug metadata separated.
- Do not use glTF mesh collision for wheels/suspension in v0.
- Do not start full vehicle assembly before one corner can load dimensions and show visuals safely.
- Do not mix visual rig markers with physics joint frames without explicit conversion.
- Do not rebuild bodies/joints during slider drag.
- Do not let pending structural setup affect runtime live-root behavior.
- Do not derive rest drop from visual chassis/wheel sockets until a dedicated physics rest anchor contract exists.
- Do not treat M3B semantic preview as final import transform.
- Do not anchor suspension semantic preview to wheel rest drop again.

## Immediate next engineering target

Per ADR-0005, the order is:

```text
M5.1  feel tuning pass driven by Jozz's manual driving feedback
M4C   procedural damper/cardan visual proof on the drivable M5 corners
```

Strict scope for M4C:

```text
use existing contract endpoint data
keep audit reports as diagnostics
do not regenerate reports unless explicitly intended
do not build a full glTF importer
do not add mesh collision or multi-body suspension
```

(steering and the four-corner vehicle now exist through the engine wheel
joint in the M5 module; do not re-implement them elsewhere)

## Validation commands

From repo root:

```powershell
cmake --build --preset windows-debug --target test
cmake --build --preset windows-debug --target samples
cmake --build --preset windows-debug --target jozz_vehicle_validation
build\bin\Debug\test.exe
build\bin\Debug\jozz_vehicle_validation.exe
build\bin\Debug\samples.exe --sample 96 --frames 300
```

Environment warning (2026-07-05): the historical `cmd /c "set PATH=& ..."`
wrapper SILENTLY DOES NOTHING when invoked from Git Bash — cmd starts
interactively, executes nothing, and exits 0, so builds appear green while
binaries stay stale. Call cmake directly and verify binary timestamps or
program output. Use the wrapper only in environments where MSBuild actually
fails on duplicate `Path/PATH` environment keys (the original Codex issue),
and treat that failure as an environment issue, not a Box3D code issue.

`jozz_vehicle_validation.exe` now includes the M5 headless drive smoke
(settle/drive/steer/brake assertions) and must end with
`m5 drive smoke: ok` and `jozz_vehicle_validation: OK`.

Run asset report generators only when you intentionally want to rewrite reports:

```powershell
py tools\asset_audit.py
py tools\asset_contract_audit.py
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Check:

```text
sample opens
W/S, Space, Q/E work
Apply rig rebuild works
M3B semantic preview draws
HUD shows M3B metadata runtime audit or fallback
Reload metadata + reset defaults is safe
M3B.2.1 static textured wheel proof can be toggled
static wheel mesh is visible but not attached to physics
M3B.3 attached textured wheel visual can be toggled
attached wheel mesh follows the primitive wheel body
primitive wheel debug shape can be hidden without disabling physics
hidden primitive wheel debug shape leaves no thin collision mesh/edge overlay
texture status reports loaded baseColor or a solid fallback reason
M4A suspension mount visual toggles independently
M4A contract points show wheel center/chassis mount/travel axis
M4B moving endpoints follow wheel travel only on wheel-side points
2026-07-05 Jozz screenshots show suspension model + texture + transparency + helper lines in the active lab
```
