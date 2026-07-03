# README_FOR_AGENTS — Jozz Vehicle Box3D Native

Status: M3A validated; M3B semantic preview validated; M3B.2-prep runtime metadata validated by Jozz screenshot; ready for Codex stabilization handoff  
Date: 2026-07-03  
Owner/creative director: Jozz / Przemek  
Working branch: `jozz-vehicle-sandbox-m0`

## What this branch is

This branch starts **Jozz Vehicle Box3D Native**, a Windows/native vehicle sandbox built on top of Box3D.

The long-term goal is to use Box3D as a proven physics/render-host foundation, then grow a Jozz Vehicle game/lab layer around vehicle assembly, wheel suspension, visual rigs, and Blockbench-authored parts.

## Current reality

Jozz Vehicle currently lives inside the existing Box3D `samples` host.

Current active sample:

```text
Category: Jozz Vehicle
Sample:   Lab M2 Primitive Corner
Source:   samples/sample_jozz_vehicle_lab.cpp
Panel:    Jozz Vehicle Lab M2.5 + M3A/M3B debug
```

The sample host gives windowing, camera, debug draw, ImGui, input, registration and build integration. A separate executable may happen later, but it is not the current blocker.

## Read first

Read in this order before making changes:

1. `docs/CURRENT_STATE_INDEX_PL.md`
2. `docs/PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md`
3. `docs/CODEX_HANDOFF_M3_STABILIZATION_IMPORT_PREP_PL.md`
4. `docs/M3B_2_RUNTIME_METADATA_VALIDATION_PL.md`
5. `docs/M3B_2_PREP_RUNTIME_METADATA_REPORT_PL.md`
6. `docs/M3B_SEMANTIC_PREVIEW_ANCHORING_FIX_PL.md`
7. `docs/M3B_SEMANTIC_DEBUG_PREVIEW_IMPLEMENTATION_REPORT_PL.md`
8. `docs/M3A_IMPLEMENTATION_REPORT_PL.md`
9. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
10. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
11. `docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md`
12. `assets/README.md`
13. `assets/reports/asset_audit_latest.md`
14. `samples/sample_jozz_vehicle_lab.cpp`
15. `samples/jozz_vehicle_asset_metadata.h`
16. `samples/jozz_vehicle_asset_metadata.cpp`
17. `samples/sample_joint.cpp` sections `WheelJoint` and `Driving` only as reference

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

M2.5 is still the current wheel-corner physics baseline. M3A adds asset-derived primitive defaults. M3B adds semantic preview and runtime audit metadata loading.

Do not override this with older M2/M2.1/M2.2/M2.3 assumptions.

Core rules:

```text
b3WheelJoint implicit spring rest = translation 0
Frame A = rest wheel-center anchor on chassis/root
Frame B = wheel center / wheel body origin
Rest drop = explicit chassis-to-rest-wheel-center offset
Visual sockets are not automatically physics frames
```

M3 status:

```text
M3A: wheel radius/width and travel hint derive from asset audit metadata
M3B.1: semantic preview is debug-only and follows correct ownership
M3B.2-prep: runtime metadata loads asset_audit_latest.json if reachable, with fallback
No glTF mesh rendering yet
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
  - no raw glTF mesh loading yet
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

Asset audit tools:

```powershell
py tools\asset_audit.py
py tools\asset_contract_audit.py
```

## Current hotkeys

Current Jozz Vehicle shortcuts:

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

Do not use `[` or `]`; they are global sample switching keys.

Before adding any shortcut, check and update:

1. `docs/HOTKEY_AUDIT_PL.md`
2. `samples/main.cpp`
3. `samples/gfx/keycodes.h`
4. `samples/sample_jozz_vehicle_lab.cpp`

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

The next pass should be Codex stabilization, not a big feature sprint.

Use:

```text
docs/CODEX_HANDOFF_M3_STABILIZATION_IMPORT_PREP_PL.md
docs/CODEX_START_PROMPT_M3_STABILIZATION_PL.md
```

After stabilization, the next small feature gate may be:

```text
M3B.2 static wheel visual mesh at origin
```

Strict scope for that future gate:

```text
one wheel mesh
fixed debug origin
not attached to physics
not animated
not skinned
not collision
not full vehicle
```

## Validation commands

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

Check:

```text
sample opens
W/S, Space, Q/E work
Apply rig rebuild works
M3B semantic preview draws
HUD shows M3B metadata runtime audit or fallback
Reload metadata + reset defaults is safe
no glTF mesh appears yet
```
