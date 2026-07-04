# README_FOR_AGENTS — Jozz Vehicle Box3D Native

Status: M3A/M3B validated; M3B.2.1 static textured wheel proof implemented; M3B.3 visual attach not started
Date: 2026-07-04
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
Source:   samples/sample_jozz_vehicle_lab.cpp + samples/jozz_vehicle_primitive_corner_lab.cpp
Panel:    Jozz Vehicle Lab M2.5 + M3A/M3B.2.1 debug
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
17. `samples/jozz_vehicle_asset_dimensions.h`
18. `samples/jozz_vehicle_asset_dimensions.cpp`
19. `samples/jozz_vehicle_debug_preview.h`
20. `samples/jozz_vehicle_debug_preview.cpp`
21. `samples/jozz_vehicle_primitive_corner_lab.h`
22. `samples/jozz_vehicle_primitive_corner_lab.cpp`
23. `samples/jozz_vehicle_visual_mesh.h`
24. `samples/jozz_vehicle_visual_mesh.cpp`
25. `samples/jozz_vehicle_image_decode.h`
26. `samples/jozz_vehicle_image_decode.cpp`
27. `samples/jozz_vehicle_validation.cpp`
28. `samples/sample_joint.cpp` sections `WheelJoint` and `Driving` only as reference

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

M2.5 is still the current wheel-corner physics baseline. M3A adds asset-derived primitive defaults. M3B adds semantic preview and runtime audit metadata loading. M3B.2 adds one static visual-only wheel mesh proof at a fixed debug origin. M3B.2.1 adds baseColor texture loading for that same static proof.

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
M3B.2: one Offroad_Big_Wheels glTF mesh primitive renders at a fixed debug origin
M3B.2.1: that static wheel mesh can load TEXCOORD_0 + pbr baseColorTexture PNG data URI
M3B.3: visual-only wheel mesh attach to the primitive wheel body is not started
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

Static visual proof
  - loads a narrow subset of Offroad_Big_Wheels.gltf
  - supports one baseColor PNG data URI through WIC on Windows
  - draws one mesh at a fixed debug origin
  - not attached to physics
  - no material/skin/animation/collision/full importer yet
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
5. `samples/jozz_vehicle_primitive_corner_lab.cpp`

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

The next small feature gate is:

```text
M3B.3 visual-only wheel mesh attached to primitive wheel body
```

Strict scope for that gate:

```text
one wheel mesh
attached visually to the primitive wheel body transform
not animated
not skinned
not collision
not full vehicle
```

## Validation commands

From repo root:

```powershell
cmd /c "set PATH=& cmake --build --preset windows-debug --target test"
cmd /c "set PATH=& cmake --build --preset windows-debug --target samples"
cmd /c "set PATH=& cmake --build --preset windows-debug --target jozz_vehicle_validation"
cmd /c "set PATH=& build\bin\Debug\test.exe"
cmd /c "set PATH=& build\bin\Debug\jozz_vehicle_validation.exe"
```

In Codex/PowerShell on Windows, use the `cmd /c "set PATH=& ..."` wrapper if MSBuild fails with duplicate `Path/PATH` environment keys. Treat that as an environment issue, not a Box3D code issue.

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
texture status reports loaded baseColor or a solid fallback reason
```
