# Current State Index — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: active handoff/index after M3A validation, M3B semantic preview validation, and M3B.2-prep runtime metadata implementation

## 1. Purpose

This document is the first file a future agent should use to orient itself after M2.5/M3A/M3B.

The project now has enough milestone documents that reading them in the wrong order can easily reintroduce old mistakes. This index tells the next agent what is current, what is historical, and what must not be treated as active architecture.

## 2. Current reality

Jozz Vehicle Box3D Native is currently implemented as a lab inside the existing Box3D `samples` host.

This is intentional for the current phase. The sample host already provides:

- native windowing;
- camera;
- debug draw;
- ImGui;
- input handling;
- sample registration;
- CMake/build integration.

A separate executable may still happen later, but it is not the immediate blocker. Do not restart the project around a custom executable before the foundation and next small vehicle-lab gates are clean.

## 3. Active sample

Current active lab:

```text
Category: Jozz Vehicle
Sample:   Lab M2 Primitive Corner
Source:   samples/sample_jozz_vehicle_lab.cpp
Panel:    Jozz Vehicle Lab M2.5 + M3A/M3B debug
```

The sample picker name remains `Lab M2 Primitive Corner` because the scene is still the same one-corner primitive lab. M3A changed the source of primitive defaults. M3B adds semantic/debug metadata plumbing, not a new sample architecture.

The older M1 smoke sample still exists as a basic host sanity check:

```text
Jozz Vehicle / Lab M1 Smoke
```

## 4. Authoritative physics baseline

The authoritative physics baseline is still **M2.5 primitive one-corner wheel-joint lab**, now with **M3A asset-derived primitive defaults**, **M3B semantic debug preview**, and **M3B.2-prep runtime asset audit metadata**.

Current model:

```text
Body A: static chassis/root debug rig
Body B: dynamic primitive wheel body
Joint:  b3WheelJoint
```

Important physics rules:

1. `b3WheelJoint` has implicit spring rest at `translation = 0`.
2. Body-A frame is the **rest wheel-center anchor on the chassis**, not the visual damper/chassis mount.
3. Body-B frame is the **wheel center / body origin**.
4. `Rest drop` positions the rest wheel-center anchor relative to the chassis.
5. Rebound/compression limits are relative to that rest position.
6. Primitive wheel collision remains a cylinder/hull, not the glTF mesh.
7. M3A radius/width defaults are traced to asset audit markers.
8. M3A suspension travel from the asset is a hint only.
9. M3A `restDrop` remains explicit/tuned, not derived from visual sockets.
10. M3B semantic preview is a debug schematic only and does not drive physics.
11. M3B wheel preview follows the actual primitive wheel body.
12. M3B suspension preview follows chassis/root, not wheel rest drop.
13. M3B.2-prep can load semantic positions from `assets/reports/asset_audit_latest.json` at runtime, with a built-in fallback.

Do not return to the M2.3 model where frame A was treated as a visual chassis mount.

## 5. Runtime vs structural setup separation

M2.5/M3A/M3B intentionally separates four control categories:

```text
Structural setup
  - rig height
  - rest drop
  - wheel radius
  - wheel width
  - wheel/chassis collision toggle
  - uses pending edit values
  - requires Apply rig rebuild

Live root stress test
  - live root offset slider
  - Q/E keyboard root movement
  - reset live root
  - realtime
  - moves chassis/root only
  - must not rebuild bodies/joints

Semantic preview
  - M3B semantic preview checkbox
  - debug overlay only
  - wheel schematic follows wheel/body
  - suspension schematic follows chassis/root
  - no physics authority
  - no mesh rendering

Runtime metadata
  - reads asset audit report when reachable
  - falls back to built-in audited metadata when not reachable
  - must not crash the lab if files are missing
  - does not load meshes/materials/skins/animations
```

Future UI/debug work should preserve this pattern:

```text
pending edit values != committed physics values
semantic debug overlay != physics authority
suspension semantic preview != wheel-owned marker group
runtime metadata != mesh renderer
```

Live root must continue reading committed setup values until Apply is pressed.

## 6. Current hotkeys

Sample-host/global keys are owned by the Box3D samples app. Do not add new shortcuts without checking `docs/HOTKEY_AUDIT_PL.md`, `samples/main.cpp`, `samples/gfx/keycodes.h`, and the current Jozz sample code.

Current Jozz Vehicle M2.5/M3A/M3B sample keys:

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

Important:

```text
[ and ] are global sample-switching keys. Do not use them for Jozz Vehicle controls.
```

M3A/M3B added no new hotkeys.

## 7. Active documentation

Read these first:

1. `README_FOR_AGENTS.md`
2. `docs/CURRENT_STATE_INDEX_PL.md`
3. `docs/PROJECT_AUDIT_2026_07_03_PL.md`
4. `docs/FOUNDATION_GROUNDING_PHASE_PLAN_PL.md`
5. `docs/PRE_RIG_IMPORT_READINESS_AUDIT_PL.md`
6. `docs/M3A_ASSET_DERIVED_PRIMITIVE_DIMENSIONS_PLAN_PL.md`
7. `docs/M3A_EXECUTION_PLAN_AND_CRITICAL_REVIEW_PL.md`
8. `docs/M3A_IMPLEMENTATION_REPORT_PL.md`
9. `docs/M3B_METADATA_DEBUG_IMPORT_PLAN_PL.md`
10. `docs/M3B_SEMANTIC_DEBUG_PREVIEW_IMPLEMENTATION_REPORT_PL.md`
11. `docs/M3B_SEMANTIC_PREVIEW_ANCHORING_FIX_PL.md`
12. `docs/M3B_2_PREP_RUNTIME_METADATA_REPORT_PL.md`
13. `docs/HOTKEY_AUDIT_PL.md`
14. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
15. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
16. `docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md`
17. `docs/PROJECT_DIRECTION_PL.md`
18. `assets/README.md`
19. `assets/reports/asset_audit_latest.md`
20. `samples/sample_jozz_vehicle_lab.cpp`
21. `samples/jozz_vehicle_asset_metadata.h`
22. `samples/jozz_vehicle_asset_metadata.cpp`
23. `samples/sample_joint.cpp` sections `WheelJoint` and `Driving` only as reference

Also useful as policy background:

- `docs/adr/0001-project-scope.md`
- `docs/adr/0002-orientation-policy.md`
- `docs/adr/0003-physics-v0-wheel-joint.md`
- `docs/adr/0004-renderer-strategy.md`
- `docs/ASSET_CONTRACT_V2_DRAFT_PL.md`

## 8. Superseded / historical documentation

These documents are useful history, but they are not current architecture:

```text
docs/M2_PRIMITIVE_CORNER_LAB_PL.md
docs/M2_1_PRIMITIVE_CORNER_AXIS_FIX_PL.md
docs/M2_2_CENTERED_WHEEL_PIVOT_AND_RIG_CONTROLS_PL.md
docs/M2_3_SUSPENSION_MOUNT_MODEL_PL.md
```

How to use them:

- M2 records the first primitive corner attempt.
- M2.1 records the cylinder dimension/order mistake.
- M2.2 records the centered wheel-pivot fix.
- M2.3 records the structural setup/Apply direction, but also contains the incorrect visual-mount-as-frame-A model.

Current authority superseding them:

```text
M2.4 — correct wheel-joint rest-anchor model
M2.5 — live root stress mover + pending/committed setup separation
M3A — asset-derived primitive radius/width defaults, travel hint only
M3B.1 — semantic debug preview overlay, no mesh import
M3B.1 anchoring fix — wheel preview follows wheel/body; suspension preview follows chassis/root
M3B.2-prep — runtime asset audit metadata without mesh rendering
```

## 9. Current assets

Current source models:

```text
assets/source/Asset_Dumper.gltf
assets/source/Cardan_shaft.gltf
assets/source/Offroad_Big_Wheels.gltf
assets/source/One_Sided_wheel_mount.gltf
```

Current asset status:

- research/startup assets, not final production contracts;
- all four current glTF files have duplicate node names;
- orientation is not final;
- scale is prototype-only;
- marker/socket naming is useful but cannot be trusted alone;
- importer must use stable node index/path/parent chain and composed transforms.

Current audit report used by runtime metadata when reachable:

```text
assets/reports/asset_audit_latest.md
assets/reports/asset_audit_latest.json
```

New metadata loader:

```text
samples/jozz_vehicle_asset_metadata.h
samples/jozz_vehicle_asset_metadata.cpp
```

New contract audit helper:

```text
tools/asset_contract_audit.py
```

It validates sidecar contracts against referenced glTF files and reports missing/ambiguous/name-only bindings. It is not a runtime importer.

## 10. Validation commands for Jozz

From repo root:

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Manual sample check:

```text
Open:  Jozz Vehicle / Lab M2 Primitive Corner
Panel: Jozz Vehicle Lab M2.5 + M3A/M3B debug
```

Regression check:

- build succeeds;
- sample opens;
- panel shows a `metadata:` status line;
- HUD shows `M3B metadata: runtime audit` or `M3B metadata: built-in fallback`;
- `Reload metadata + reset defaults` does not crash;
- live root slider moves root realtime without Apply;
- Q lowers root;
- E raises root;
- `[ ]` still switch samples globally and are not Jozz controls;
- W/S motor works;
- Space brake works;
- structural slider edits do not affect live root until Apply;
- Apply commits structural setup and rebuilds once;
- `M3B semantic preview` toggle exists;
- semantic preview draws schematic marker crosses/lines near the wheel;
- wheel preview follows the primitive wheel/body;
- suspension preview follows chassis/root;
- changing `Rest drop` and pressing Apply does not drag the whole suspension schematic as if it were wheel-owned;
- live root movement may move suspension preview because live root moves chassis/root;
- semantic preview toggle does not change physics;
- wheel pivot remains centered;
- collision OFF prevents wheel/chassis collision ambiguity;
- no glTF mesh is rendered yet.

Optional asset/tool checks:

```powershell
py tools\asset_audit.py
py tools\asset_contract_audit.py
```

## 11. Box3D sample references

`docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md` analyzes upstream/reference samples:

```text
Joints / Wheel
Joints / Driving
```

Main takeaway:

```text
Both support the M2.4/M2.5 rest-anchor model.
```

Use them later as references for steering API, four-corner ownership, front steering/rear drive split and debug readouts. Do not copy them blindly.

## 12. Current implementation status and next gate

M3A was manually validated by Jozz. M3B.1 semantic preview is implemented. The first M3B.1 anchoring model was criticized by Jozz, fixed, and then manually validated by Jozz screenshots. M3B.2-prep runtime asset audit metadata is implemented but needs build/manual validation.

M3A does:

```text
wheel radius/width: centralized and asset-derived from audit markers
suspension total travel: stored as asset hint
rest drop: remains explicit/tuned
```

M3B.1 does:

```text
semantic marker preview overlay
wheel radius/width/spin schematic
suspension travel schematic
toggleable debug-only visualization
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

M3A/M3B does not do:

```text
runtime glTF mesh rendering
mesh collision
steering
four-corner vehicle
new hotkeys
```

After M3B.2-prep validation, choose based on metadata result:

```text
If runtime audit loads: M3B.2 static wheel visual mesh at origin
If fallback is used: fix runtime asset/report path discovery first
```

## 13. Explicit no-go list for the next agent

Do not do these before validating M3B.2-prep:

- do not start full glTF renderer;
- do not build full vehicle assembly;
- do not replace the primitive collision with mesh collision;
- do not rewrite Box3D internals;
- do not add new hotkeys without audit;
- do not merge visual rig marker positions directly into physics joint frames;
- do not mix pending structural setup with runtime live root controls;
- do not treat M2.1/M2.2/M2.3 as current architecture;
- do not derive `restDrop` directly from visual chassis/wheel sockets;
- do not treat M3B schematic preview as final import transform;
- do not let suspension semantic preview become wheel-owned again;
- do not proceed to mesh rendering if runtime metadata path discovery is broken and only fallback works.

## 14. Current critical judgement

The project is in a good but fragile place.

M2.5 gave the correct wheel-joint behavior. M3A connected primitive wheel radius/width to Jozz's real asset measurements. M3B.1 exposed semantic marker relationships in-game as a safe debug overlay, and Jozz's critique improved the ownership model. M3B.2-prep now begins the runtime asset-data path.

The next move is build/manual validation. If runtime metadata loads, the project can attempt one static wheel visual mesh at origin. If not, fix asset path staging first.