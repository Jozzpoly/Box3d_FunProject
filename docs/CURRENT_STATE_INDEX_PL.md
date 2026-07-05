# Current State Index — Jozz Vehicle Box3D Native

Date: 2026-07-05
Branch: `jozz-vehicle-sandbox-m0`  
Status: M2.5/M3A/M3B.3 baseline validated; M4 Foundation validated by Jozz screenshots; M5 First Drivable implemented and played by Jozz (~20 min); M5.1 feel-tuning pass done (stationary steering torque fix, chassis width, chase camera, wider test playground) per docs/M5_1_FEEL_TUNING_IMPLEMENTATION_REPORT_PL.md, awaiting Jozz's re-test

## 1. Current active samples

```text
Category: Jozz Vehicle
Sample:   M5 First Drivable          <- pierwszy jeżdżący pojazd (nowe)
Source:   samples/jozz_vehicle_m5_drivable_lab.cpp + samples/jozz_vehicle_m5_vehicle.cpp

Sample:   Lab M2 Primitive Corner    <- izolowany narożnik, nadal aktywny
Source:   samples/sample_jozz_vehicle_lab.cpp + samples/jozz_vehicle_primitive_corner_lab.cpp
Panel:    Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 foundation debug
```

The corner lab remains the isolated tuning environment; M5 is the drivable
four-corner vehicle built on the same rest-anchor model. See
`docs/M5_FIRST_DRIVABLE_PL.md` and `docs/adr/0005-m5-first-drivable-before-m4c.md`.

The older smoke test still exists:

```text
Jozz Vehicle / Lab M1 Smoke
```

## 2. Current validated state

Validated by Jozz:

```text
M2.5 primitive one-corner wheel-joint lab works
M3A asset-derived primitive defaults work
M3B semantic preview anchoring fix works
M3B.2-prep runtime audit metadata path works
M3B.2 static visual-only wheel mesh proof exists
M3B.2.1 baseColor texture proof exists for the same static wheel mesh
M3B.3 visual-only wheel mesh attach follows the primitive wheel body
M3B.3 hardening centers the primitive collision wheel and can hide its debug shape
M4F.1 asset contract runtime resolves sidecar bindings from source glTF
M4A one-sided suspension mount visual proof exists
M4B narrow moving endpoint debug preview exists
2026-07-05 Jozz screenshots confirm suspension model, texture, transparency and helper-line visibility in the active lab
```

Implemented 2026-07-05, machine-validated, awaiting Jozz manual feel check:

```text
Cleanup: shared jozz_vehicle_json helpers, shared asset path resolver,
         single built-in fallback table, upstream CMakeLists layout restored
M5 vehicle physics module (jozz_vehicle_m5_vehicle.*): dynamic chassis,
         four b3WheelJoints on the M2.4 rest-anchor model, front steering,
         AWD/RWD drive, brake, optional upright assist
M5 headless drive smoke in jozz_vehicle_validation.exe: settle/drive/steer/
         brake with assertions (caught collapsed suspension and inverted
         steering sign during development)
M5 sample "Jozz Vehicle / M5 First Drivable": W/S/A/D/Space/T input, ramp +
         washboard course, live tuning, glTF wheel visuals on all corners
```

M5.1 feel tuning (2026-07-05, after Jozz's ~20 min playtest), see
`docs/M5_1_FEEL_TUNING_HANDOFF_2026_07_05_PL.md` (analysis/plan) and
`docs/M5_1_FEEL_TUNING_IMPLEMENTATION_REPORT_PL.md` (what shipped):

```text
Reproduced the reported broken stationary steering in the headless smoke
  FIRST (0.0 deg of 32 deg target at the old 80 N*m default) before fixing,
  per the project's evidence-before-fix discipline
Fixed: maxSteeringTorque 80->700, steeringHertz 8->14 (stationary tire
  parking-torque was undertorqued by an order of magnitude; scrub radius in
  this rig is actually zero, so that was not the mechanism)
Narrowed chassis half-width 0.80->0.55 (was clipping into the tire sidewalls)
Replaced the default camera with a proper rear chase view and reset
  m_thirdPerson explicitly; added an "Invert steering" checkbox as a
  safety net since screen orientation could not be verified visually
  this session; the steering sign math itself was NOT changed (verified
  correct against this codebase's own right = up x forward convention)
Chassis/track/wheelbase/mass geometry moved to a pending/Apply pattern
  (mirrors the corner lab), and all previously-live sliders widened
  substantially for stress testing per Jozz's request
Extracted jozz_vehicle_m5_test_course.h/.cpp: 2x ground, 4 ramps, 2
  washboard lanes, a rough-terrain heightfield zone, 14 scattered props
  with a "Reset props" button
NOT touched: the speed-dependent wheel "teleporting" instability - needs
  Jozz's eyes on the corrected build; first diagnostic is the existing
  Solver panel's Sub-steps slider (already on by default, no code needed)
```

Latest observed runtime metadata state:

```text
M3B metadata: runtime audit
metadata: loaded runtime asset audit report
source: ../../assets/reports/asset_audit_latest.json
```

M3B.2 renders one static `Offroad_Big_Wheels.gltf` mesh primitive at a fixed debug origin. M3B.2.1 adds the narrow baseColor texture path for that same proof. M3B.3 reuses the same mesh and draws it through `DrawAtTransform(...)` so it follows the primitive wheel body. M3B.3 hardening centers the primitive cylinder on the wheel body origin and adds a panel toggle for the orange primitive wheel debug shape; hiding it does not disable the physics body, primitive collision, or wheel joint. The mesh remains visual-only and is not a full glTF/material/skin/animation/collision importer.

## 3. Authoritative physics baseline

Current model:

```text
Body A: static chassis/root debug rig
Body B: dynamic primitive wheel body
Joint:  b3WheelJoint
```

Rules:

1. `b3WheelJoint` has implicit spring rest at `translation = 0`.
2. Frame A is the rest wheel-center anchor on chassis/root.
3. Frame B is the wheel center/body origin.
4. `Rest drop` is explicit/tuned.
5. Visual sockets are not automatically physics frames.
6. Primitive wheel collision remains centered cylinder/hull, not glTF mesh.
7. M3B semantic preview is debug-only and does not drive physics.
8. Runtime metadata is a data source only, not a renderer.

Do not return to the historical M2.3 visual-mount-as-frame-A model.

## 4. Current separation model

Keep these separate:

```text
Structural setup
  - rig height
  - rest drop
  - wheel radius
  - wheel width
  - collision toggle
  - pending values + Apply rig rebuild
  - primitive wheel cylinder is centered on body origin because Frame B is wheel center

Live root stress test
  - realtime chassis/root movement
  - slider + Q/E
  - no body/joint rebuild

Semantic preview
  - debug overlay only
  - wheel schematic follows wheel/body
  - suspension schematic follows chassis/root
  - no physics authority

Runtime metadata
  - reads assets/reports/asset_audit_latest.json when reachable
  - falls back safely when not reachable

Static visual proof
  - loads a narrow subset of Offroad_Big_Wheels.gltf
  - reads TEXCOORD_0 and one pbr baseColorTexture PNG data URI
  - decodes PNG to RGBA8 through isolated Windows/WIC helper
  - draws one mesh at a fixed debug origin
  - remains available as debug/comparison and is not attached to physics

Attached visual proof
  - reuses the same Offroad_Big_Wheels.gltf mesh
  - draws through JozzVehicleVisualMesh::DrawAtTransform(...)
  - follows the primitive wheel body transform
  - applies a local render-only correction for the current authored wheel orientation and center
  - centers against loaded mesh bounds when available, with semantic points as fallback
  - no multi-material/normal-map/metallic-roughness/skin/animation/collision/full importer
  - no physics authority or mesh collision

Primitive wheel debug shape
  - orange Box3D debug shape for the primitive collision wheel
  - can be hidden independently from the attached visual mesh through the debug adapter hidden-shape path
  - hiding it does not change physics/collision
  - hidden mode leaves no collision mesh/edge overlay

Asset contract runtime
  - reads assets/contracts/*.asset.json, not assets/reports/*latest*, for M4 runtime binding data
  - resolves nodeIndexHint/nodePathHint/nameHint against the source glTF
  - composes node parent transforms before resolving positions
  - keeps duplicate node-name warnings visible

Suspension visual foundation
  - loads one_sided_wheel_mount.asset.json and One_Sided_wheel_mount.gltf
  - draws a visual-only one-sided suspension mount proof
  - overlays contract wheel center, chassis mount, travel top and travel bottom
  - draws debug-only moving damper/cardan endpoints
  - does not change b3WheelJoint, Frame A/Frame B, restDrop, or primitive collision
```

Known visual debt after M3B.2.1:

```text
alpha-masked tire tread affects the lit pass but not the shadow caster yet
wheel shadow therefore does not show tread/cutout structure
inner rim/felga shows visible banded/striped shading in close-up screenshots
```

These are not blockers for M3B.3 visual-only attach. Revisit them during a focused render/material polish pass. For the rim banding, first isolate whether the source is shadow acne/self-shadowing, imported vertex normals, material roughness/specular mismatch, or low-resolution nearest-filter texture detail.

## 5. Read order for next agent

Read first:

1. `README_FOR_AGENTS.md`
2. `docs/CURRENT_STATE_INDEX_PL.md`
3. `docs/M4_FOUNDATION_SUSPENSION_RIG_PLAN_PL.md`
4. `docs/ASSET_CONTRACT_RUNTIME_V1_PL.md`
5. `docs/SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md`
6. `docs/M4_MANUAL_SMOKE_2026_07_05_PL.md`
7. `docs/CODEX_HANDOFF_M4_FOUNDATION_MAIN_READY_PL.md`
8. `docs/CODEX_START_PROMPT_M4_FOUNDATION_PL.md`
9. `docs/PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md`
10. `docs/CODEX_HANDOFF_M3_STABILIZATION_IMPORT_PREP_PL.md`
11. `docs/CODEX_START_PROMPT_M3_STABILIZATION_PL.md`
12. `docs/M3B_2_RUNTIME_METADATA_VALIDATION_PL.md`
13. `docs/M3B_2_PREP_RUNTIME_METADATA_REPORT_PL.md`
14. `docs/M3B_SEMANTIC_PREVIEW_ANCHORING_FIX_PL.md`
15. `docs/M3B_SEMANTIC_DEBUG_PREVIEW_IMPLEMENTATION_REPORT_PL.md`
16. `docs/M3A_IMPLEMENTATION_REPORT_PL.md`
17. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
18. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
19. `docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md`
20. `assets/README.md`
21. `assets/reports/asset_audit_latest.md`
22. `samples/sample_jozz_vehicle_lab.cpp`
23. `samples/jozz_vehicle_asset_contract.h`
24. `samples/jozz_vehicle_asset_contract.cpp`
25. `samples/jozz_vehicle_corner_rig.h`
26. `samples/jozz_vehicle_corner_rig.cpp`
27. `samples/jozz_vehicle_visual_asset.h`
28. `samples/jozz_vehicle_visual_asset.cpp`
29. `samples/jozz_vehicle_asset_metadata.h`
30. `samples/jozz_vehicle_asset_metadata.cpp`
31. `samples/jozz_vehicle_asset_dimensions.h`
32. `samples/jozz_vehicle_asset_dimensions.cpp`
33. `samples/jozz_vehicle_debug_preview.h`
34. `samples/jozz_vehicle_debug_preview.cpp`
35. `samples/jozz_vehicle_primitive_corner_lab.h`
36. `samples/jozz_vehicle_primitive_corner_lab.cpp`
37. `samples/jozz_vehicle_visual_mesh.h`
38. `samples/jozz_vehicle_visual_mesh.cpp`
39. `samples/jozz_vehicle_image_decode.h`
40. `samples/jozz_vehicle_image_decode.cpp`
41. `samples/jozz_vehicle_validation.cpp`
42. `samples/sample_joint.cpp` sections `WheelJoint` and `Driving` only as reference

Useful background:

- `docs/PROJECT_AUDIT_2026_07_03_PL.md`
- `docs/FOUNDATION_GROUNDING_PHASE_PLAN_PL.md`
- `docs/PRE_RIG_IMPORT_READINESS_AUDIT_PL.md`
- `docs/ASSET_CONTRACT_V2_DRAFT_PL.md`
- `docs/adr/0001-project-scope.md`
- `docs/adr/0002-orientation-policy.md`
- `docs/adr/0003-physics-v0-wheel-joint.md`
- `docs/adr/0004-renderer-strategy.md`

## 6. Historical docs

These are history, not active architecture:

```text
docs/M2_PRIMITIVE_CORNER_LAB_PL.md
docs/M2_1_PRIMITIVE_CORNER_AXIS_FIX_PL.md
docs/M2_2_CENTERED_WHEEL_PIVOT_AND_RIG_CONTROLS_PL.md
docs/M2_3_SUSPENSION_MOUNT_MODEL_PL.md
```

Current authority superseding them:

```text
M2.4 — correct rest-anchor model
M2.5 — live root + pending/committed separation
M3A — asset-derived primitive defaults
M3B.1 — semantic preview overlay + ownership fix
M3B.2-prep — runtime audit metadata without mesh rendering
M3B.2 — static visual-only wheel mesh proof at fixed debug origin
M3B.2.1 — baseColor texture proof on the same static wheel mesh
M3B.3 — visual-only wheel mesh attached to the primitive wheel body
```

## 7. Current assets

Source models:

```text
assets/source/Asset_Dumper.gltf
assets/source/Cardan_shaft.gltf
assets/source/Offroad_Big_Wheels.gltf
assets/source/One_Sided_wheel_mount.gltf
```

Current status:

- research/startup assets, not final production contracts;
- duplicate node names exist;
- orientation is not final;
- scale is prototype-only;
- marker/socket naming is useful but not enough alone;
- importer must use stable node identity/path/parent chain and composed transforms.

Runtime metadata currently reads when reachable:

```text
assets/reports/asset_audit_latest.json
```

Metadata code:

```text
samples/jozz_vehicle_asset_metadata.h
samples/jozz_vehicle_asset_metadata.cpp
```

Visual proof code:

```text
samples/jozz_vehicle_primitive_corner_lab.h
samples/jozz_vehicle_primitive_corner_lab.cpp
samples/jozz_vehicle_visual_mesh.h
samples/jozz_vehicle_visual_mesh.cpp
samples/jozz_vehicle_image_decode.h
samples/jozz_vehicle_image_decode.cpp
samples/jozz_vehicle_validation.cpp
```

## 8. Current hotkeys

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

Do not use `[` or `]`; they are global sample-switching keys. Details in
`docs/HOTKEY_AUDIT_PL.md` (updated 2026-07-05 for A/D/T).

## 9. Validation commands

From repo root:

```powershell
cmake --build --preset windows-debug --target test
cmake --build --preset windows-debug --target samples
cmake --build --preset windows-debug --target jozz_vehicle_validation
build\bin\Debug\test.exe
build\bin\Debug\jozz_vehicle_validation.exe
build\bin\Debug\samples.exe --sample 96 --frames 300
```

Environment note (2026-07-05): the historical `cmd /c "set PATH=& ..."`
wrapper silently does nothing when invoked from Git Bash (cmd starts
interactively and exits 0 without running the command). Call cmake directly;
use the wrapper only in environments where MSBuild actually fails on
duplicate `Path/PATH` keys, and verify output/binary timestamps either way.

Run `py tools\asset_audit.py` and `py tools\asset_contract_audit.py` only when intentionally regenerating repo reports.

Manual sample check:

```text
Open:  Jozz Vehicle / Lab M2 Primitive Corner
Panel: Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 foundation debug
```

Regression check:

```text
sample opens
panel shows metadata status
HUD shows M3B metadata runtime audit or fallback
Reload metadata + reset defaults is safe
W/S motor works
Space brake works
Q/E live root works
Apply rig rebuild works
M3B semantic preview draws
semantic preview does not change physics
M3B.2.1 static textured wheel proof toggles
static wheel mesh is visible but not attached to physics
M3B.3 attached textured wheel visual toggles
attached wheel mesh follows primitive wheel body transform
primitive wheel debug shape toggle hides only the orange collision debug visual
hidden primitive wheel debug shape leaves no thin collision mesh/edge overlay
UI texture status shows loaded baseColor or solid fallback reason
M4 contract runtime reports sidecar + glTF status
M4A suspension mount visual toggles independently
M4A contract points show wheel center/chassis mount/travel axis
M4B moving endpoint preview follows wheel travel only on wheel-side points
```

## 10. Current implementation status

M3A does:

```text
wheel radius/width from asset audit metadata
suspension total travel as asset hint
rest drop explicit/tuned
```

M3B.1 does:

```text
semantic marker preview overlay
wheel preview anchored to wheel/body
suspension preview anchored to chassis/root
```

M3B.2-prep does:

```text
runtime load attempt for assets/reports/asset_audit_latest.json
fallback to built-in audited metadata
metadata status shown in UI/HUD
M3A defaults and M3B preview read through metadata source
```

M3B.2 does:

```text
static Offroad_Big_Wheels glTF mesh proof
fixed debug origin
visual-only draw path through the sample renderer geometry registry
no physics attachment
```

M3B.2.1 does:

```text
extends MeshVertex with UV
adds optional baseColor texture binding to geom shader path
loads TEXCOORD_0 and one pbr baseColorTexture PNG data URI
decodes PNG to RGBA8 through Windows/WIC helper
keeps solid-color fallback if texture decode/upload fails
no physics attachment
```

M3B.3 does:

```text
attached Offroad_Big_Wheels glTF mesh proof
visual-only draw path through JozzVehicleVisualMesh::DrawAtTransform(...)
uses primitive wheel body transform as the base
uses a render-only correction to center/orient the current authored wheel
centers primitive wheel cylinder on body origin / frame B
allows hiding the primitive wheel debug shape while physics stays active
keeps the M3B.2.1 fixed static proof as comparison/debug
no mesh collision
no full rig/importer
```

Foundation Grounding V2 does:

```text
extracts the primitive corner lab from the sample registration file
adds DrawAtTransform for future visual-only wheel attach
adds jozz_vehicle_validation CLI metadata/defaults check
hardens MeshVertex layout offsets with offsetof
```

M4 Foundation does:

```text
loads one_sided_wheel_mount.asset.json as a runtime sidecar contract
resolves contract bindings against One_Sided_wheel_mount.gltf node transforms
validates required suspension roles in jozz_vehicle_validation.exe
draws One_Sided_wheel_mount.gltf as visual-only M4A suspension mount proof
draws contract point diagnostics for wheel center, chassis mount, and travel axis
draws M4B narrow moving endpoint preview for damper/cardan roles
keeps audit reports as diagnostics, not M4 runtime contract source
does not change b3WheelJoint, restDrop, primitive collision, or physics authority
```

Still not implemented:

```text
mesh collision
steering
four-corner vehicle
final visual rig
skinning/animation
full glTF renderer/material importer
procedural damper/cardan/chassis visual parts
```

## 11. Next pass

```text
M5.1 re-test  Jozz re-plays the corrected build: A/D direction, stationary
              steering, chassis proportions, and the Sub-steps diagnostic
              for the still-open speed instability
M4C           procedural damper/cardan visual proof using validated contract
              endpoints - now targeting the drivable M5 vehicle corners
```

Strict scope for M4C stays as documented:

```text
use existing sidecar contract endpoint data
keep audit reports as diagnostics
do not regenerate reports automatically
not full importer
not mesh collision
not multi-body suspension
```

(the former "not steering / not full vehicle" items are satisfied by M5
through the engine wheel joint and are no longer prohibitions)

## 12. No-go list for Codex

Do not:

- start full glTF renderer;
- build full vehicle assembly;
- replace primitive collision with mesh collision;
- rewrite Box3D internals;
- add new hotkeys without audit;
- merge visual rig marker positions directly into physics joint frames;
- mix pending structural setup with runtime live root controls;
- treat M2.1/M2.2/M2.3 as current architecture;
- derive `restDrop` directly from visual chassis/wheel sockets;
- treat M3B schematic preview as final import transform;
- let suspension semantic preview become wheel-owned again;
- hide uncertain importer/rig decisions behind pretty visuals.
