# README_FOR_AGENTS — Jozz Vehicle Box3D Native

Status: M3A asset-derived primitive defaults implemented; local validation pending  
Date: 2026-07-03  
Owner/creative director: Jozz / Przemek  
Working branch: `jozz-vehicle-sandbox-m0`

## What this branch is

This branch starts **Jozz Vehicle Box3D Native**, a Windows/native vehicle sandbox built on top of Box3D.

The long-term goal is not to keep modifying random Box3D samples forever. The goal is to use the Box3D repo as a proven physics/render-host foundation, then grow a separate Jozz Vehicle game/lab layer around vehicle assembly, wheel suspension, visual rigs, and Blockbench-authored parts.

## Current reality after M2.5/M3A

Jozz Vehicle is currently implemented as a lab inside the existing Box3D `samples` host.

Current active sample:

```text
Category: Jozz Vehicle
Sample:   Lab M2 Primitive Corner
Source:   samples/sample_jozz_vehicle_lab.cpp
Panel:    Jozz Vehicle Lab M2.5 + M3A defaults
```

This is intentional for the current phase. The existing sample host already provides windowing, camera, debug draw, ImGui, input, sample registration and build integration.

A separate executable may still happen later, but it is **not** the immediate blocker. Do not restart the project around a standalone executable before the Foundation Grounding phase and the next small vehicle-lab gates are clean.

## Read first

Read in this order before making changes:

1. `docs/CURRENT_STATE_INDEX_PL.md`
2. `docs/PROJECT_AUDIT_2026_07_03_PL.md`
3. `docs/FOUNDATION_GROUNDING_PHASE_PLAN_PL.md`
4. `docs/PRE_RIG_IMPORT_READINESS_AUDIT_PL.md`
5. `docs/M3A_ASSET_DERIVED_PRIMITIVE_DIMENSIONS_PLAN_PL.md`
6. `docs/M3A_EXECUTION_PLAN_AND_CRITICAL_REVIEW_PL.md`
7. `docs/M3A_IMPLEMENTATION_REPORT_PL.md`
8. `docs/HOTKEY_AUDIT_PL.md`
9. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
10. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
11. `docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md`
12. `docs/PROJECT_DIRECTION_PL.md`
13. `assets/README.md`
14. `assets/reports/asset_audit_latest.md`
15. `samples/sample_jozz_vehicle_lab.cpp`
16. `samples/sample_joint.cpp` sections `WheelJoint` and `Driving` only as reference

Also useful as background:

- `docs/ASSET_CONTRACT_V2_DRAFT_PL.md`
- `docs/IMPLEMENTATION_START_PLAN_PL.md`
- `docs/adr/0001-project-scope.md`
- `docs/adr/0002-orientation-policy.md`
- `docs/adr/0003-physics-v0-wheel-joint.md`
- `docs/adr/0004-renderer-strategy.md`

## Authoritative baseline

M2.5 is still the current wheel-corner physics baseline. M3A adds asset-derived primitive defaults on top of it.

Do not override it with older M2/M2.1/M2.2/M2.3 assumptions.

Core M2.5 rules:

```text
b3WheelJoint implicit spring rest = translation 0
Frame A = rest wheel-center anchor on chassis
Frame B = wheel center / wheel body origin
Rest drop = where the rest wheel-center anchor is placed relative to chassis
```

M3A rules:

```text
wheel radius/width are centralized from current asset audit markers
suspension total travel from the asset is a hint only
rest drop remains explicit/tuned
no glTF rendering/importing was added
```

The visual chassis/damper mount is diagnostic/visual information. It is not automatically the physics joint frame A.

## Runtime vs structural setup rule

M2.5/M3A intentionally separates structural setup from runtime debug controls.

```text
Structural setup
  - rig height
  - rest drop
  - wheel radius
  - wheel width
  - wheel/chassis collision toggle
  - edited as pending values
  - requires Apply rig rebuild

Live root stress test
  - live root offset
  - live root key speed
  - reset live root
  - realtime
  - moves chassis/root only
  - must not rebuild the wheel joint
```

Do not let pending structural UI values affect live physics until Apply is pressed.

## Current project stance

- Box3D is the physics core.
- Sokol + ImGui from the Box3D sample stack are the preferred native host/UI foundation for early work.
- The current Jozz Vehicle lab lives inside the `samples` executable.
- The uploaded Blockbench/glTF models are accepted as **research-grade v1 assets**, not final contract assets.
- Jozz will adjust model orientation later while seeing the models in-game. Do **not** block early project work on perfecting authoring orientation now.
- For now, every asset may carry temporary orientation/scale correction metadata in its sidecar `.asset.json`.
- The current physical baseline uses a **single Box3D wheel joint per suspension corner**.
- Wahacze, damper body, and cardan shaft are **visual-only in v0** unless future tests prove that physical multi-body suspension is worth the added complexity.

## Pre-rig / import stance

Before visual rig/import work, read:

```text
docs/PRE_RIG_IMPORT_READINESS_AUDIT_PL.md
docs/M3A_IMPLEMENTATION_REPORT_PL.md
docs/ASSET_CONTRACT_V2_DRAFT_PL.md
```

Current judgement:

```text
M3A code is implemented; local build/manual validation is pending.
Heavy glTF import/rigging is not ready yet.
```

Safe after M3A validation:

```text
M3B.0 read/validate metadata only, no rendering
M3B.1 draw semantic debug points from audited positions, no mesh rendering
```

Not safe yet:

```text
restDrop derived directly from visual Socket_ChassisMount -> Socket_WheelCenter
runtime importer relying on unique node names
visual sockets treated as physics frames
full visual rig leap
```

## Box3D sample references

`docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md` analyzes:

```text
Joints / Wheel
Joints / Driving
```

Main result:

```text
Both support the M2.4/M2.5 rest-anchor model.
```

Use those stock samples later as references for steering API, four-corner ownership and debug readouts. Do not copy them blindly.

## Current assets

Source glTF files live in `assets/source/`.

Sidecar contracts live in `assets/contracts/`.

Current source models:

```text
Asset_Dumper.gltf
Cardan_shaft.gltf
Offroad_Big_Wheels.gltf
One_Sided_wheel_mount.gltf
```

The current audit report intentionally records duplicate roots/nodes and unfinished orientation decisions. Do not trust node names alone.

Asset audit tools:

```powershell
py tools\asset_audit.py
py tools\asset_contract_audit.py
```

`asset_contract_audit.py` is a pre-import validator helper, not a runtime importer.

## Current hotkey rule

The Box3D samples host owns global shortcuts. Jozz Vehicle sample shortcuts must not conflict with them.

Current Jozz Vehicle M2.5/M3A shortcuts:

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

Do not use `[` or `]` for Jozz Vehicle controls. They are global sample switching keys.

Before adding any new shortcut, check and update:

1. `docs/HOTKEY_AUDIT_PL.md`
2. `samples/main.cpp`
3. `samples/gfx/keycodes.h`
4. `samples/sample_jozz_vehicle_lab.cpp`

Prefer ImGui buttons/sliders for debug controls unless holding a key is genuinely useful.

## Non-negotiables

- Keep commits small and understandable.
- Do not silently treat draft/open decisions as accepted.
- Do not trust node names alone; duplicate node names exist in current glTF exports.
- Always compose parent transforms before reading socket/axis positions.
- Keep visual rig, physics prefab, and authoring asset data separated.
- Do not attach glTF visuals until the primitive physics baseline remains clean.
- Do not use glTF mesh collision for wheels/suspension in v0.
- Do not start full vehicle assembly before one corner can load dimensions and show visuals safely.
- Do not mix visual rig markers with physics joint frames without explicit conversion.
- Do not rebuild bodies/joints during slider drag.
- Do not let pending structural setup values affect runtime live-root debug behavior.
- Do not add hotkeys without checking global sample-host conflicts.
- Do not derive rest drop from visual chassis/wheel sockets until a dedicated physics rest anchor contract exists.

## Immediate next engineering target

Validate M3A locally before starting the next feature gate.

After validation, the next recommended gate is:

```text
M3B.0 / M3B.1 — metadata/debug-first visual import preparation
```

Do not start full glTF rendering, visual rigging, steering or full vehicle assembly before M3A is validated.

## Validation commands for Jozz

From repo root:

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
py tools\asset_audit.py
py tools\asset_contract_audit.py
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Expected panel:

```text
Jozz Vehicle Lab M2.5 + M3A defaults
```