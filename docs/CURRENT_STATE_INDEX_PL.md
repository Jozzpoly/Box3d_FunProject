# Current State Index — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: active handoff/index after Jozz-validated M2.5 primitive corner lab

## 1. Purpose

This document is the first file a future agent should use to orient itself after M2.5.

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
Panel:    Jozz Vehicle Lab M2.5
```

The older M1 smoke sample still exists as a basic host sanity check:

```text
Jozz Vehicle / Lab M1 Smoke
```

## 4. Authoritative physics baseline

The authoritative baseline is **M2.5 primitive one-corner wheel-joint lab**.

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

Do not return to the M2.3 model where frame A was treated as a visual chassis mount.

## 5. Runtime vs structural setup separation

M2.5 intentionally separates two control categories:

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
```

Future UI/debug work should preserve this pattern:

```text
pending edit values != committed physics values
```

Live root must continue reading committed setup values until Apply is pressed.

## 6. Current hotkeys

Sample-host/global keys are owned by the Box3D samples app. Do not add new shortcuts without checking `docs/HOTKEY_AUDIT_PL.md`, `samples/main.cpp`, `samples/gfx/keycodes.h`, and the current Jozz sample code.

Current Jozz Vehicle M2.5 sample keys:

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

## 7. Active documentation

Read these first:

1. `README_FOR_AGENTS.md`
2. `docs/CURRENT_STATE_INDEX_PL.md`
3. `docs/PROJECT_AUDIT_2026_07_03_PL.md`
4. `docs/FOUNDATION_GROUNDING_PHASE_PLAN_PL.md`
5. `docs/HOTKEY_AUDIT_PL.md`
6. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
7. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
8. `docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md`
9. `docs/PROJECT_DIRECTION_PL.md`
10. `assets/README.md`
11. `assets/reports/asset_audit_latest.md`
12. `samples/sample_jozz_vehicle_lab.cpp`
13. `samples/sample_joint.cpp` sections `WheelJoint` and `Driving` only as reference

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

Current audit report:

```text
assets/reports/asset_audit_latest.md
assets/reports/asset_audit_latest.json
```

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
Panel: Jozz Vehicle Lab M2.5
```

Check:

- live root slider moves root realtime without Apply;
- Q lowers root;
- E raises root;
- `[ ]` still switch samples globally and are not Jozz controls;
- W/S motor works;
- Space brake works;
- structural slider edits do not affect live root until Apply;
- Apply commits structural setup and rebuilds once;
- wheel pivot remains centered;
- collision OFF prevents wheel/chassis collision ambiguity.

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

Use them later as references for steering API, four-corner ownership, front steering/rear drive split and debug readouts. Do not copy them blindly and do not use their existence as a reason to skip M3A.

## 12. Recommended next implementation gate

After Foundation Grounding is complete, the recommended next gate is:

```text
M3A — Asset-derived primitive dimensions
```

Goal:

```text
Keep primitive physics and M2.5 behavior, but derive the default wheel radius, wheel width, rest drop and travel defaults from current asset audit/contracts.
```

Why this is preferred:

- it connects Jozz's real models to the physics lab;
- it does not introduce glTF renderer risk yet;
- it keeps the M2.5 wheel-joint baseline testable.

Only after that should the project move to a visual-only glTF wheel attachment.

## 13. Explicit no-go list for the next agent

Do not do these during foundation grounding:

- do not start M3/full glTF renderer;
- do not build full vehicle assembly;
- do not replace the primitive collision with mesh collision;
- do not rewrite Box3D internals;
- do not add new hotkeys without audit;
- do not merge visual rig marker positions directly into physics joint frames;
- do not mix pending structural setup with runtime live root controls;
- do not treat M2.1/M2.2/M2.3 as current architecture.

## 14. Current critical judgement

The project is in a good but fragile place.

M2.5 is a real foundation: centered wheel pivot, usable rest drop, rebound/compression travel, collision toggle, and live root stress movement are now understandable. The danger is rushing into models/rendering before the documented mental model becomes impossible to misread.

The correct next move is still organization first, then one small technical gate.