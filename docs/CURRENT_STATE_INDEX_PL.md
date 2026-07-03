# Current State Index — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: M2.5/M3A/M3B baseline validated; M3B.2-prep runtime metadata validated; ready for Codex stabilization handoff

## 1. Current active sample

```text
Category: Jozz Vehicle
Sample:   Lab M2 Primitive Corner
Source:   samples/sample_jozz_vehicle_lab.cpp
Panel:    Jozz Vehicle Lab M2.5 + M3A/M3B debug
```

The sample picker name remains `Lab M2 Primitive Corner` because the scene is still the same one-corner primitive lab.

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
```

Latest observed runtime metadata state:

```text
M3B metadata: runtime audit
metadata: loaded runtime asset audit report
source: ../../assets/reports/asset_audit_latest.json
```

No glTF mesh rendering exists yet.

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
6. Primitive wheel collision remains cylinder/hull, not glTF mesh.
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
  - no mesh/material/skin/animation loading
```

## 5. Read order for next agent

Read first:

1. `README_FOR_AGENTS.md`
2. `docs/CURRENT_STATE_INDEX_PL.md`
3. `docs/PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md`
4. `docs/CODEX_HANDOFF_M3_STABILIZATION_IMPORT_PREP_PL.md`
5. `docs/CODEX_START_PROMPT_M3_STABILIZATION_PL.md`
6. `docs/M3B_2_RUNTIME_METADATA_VALIDATION_PL.md`
7. `docs/M3B_2_PREP_RUNTIME_METADATA_REPORT_PL.md`
8. `docs/M3B_SEMANTIC_PREVIEW_ANCHORING_FIX_PL.md`
9. `docs/M3B_SEMANTIC_DEBUG_PREVIEW_IMPLEMENTATION_REPORT_PL.md`
10. `docs/M3A_IMPLEMENTATION_REPORT_PL.md`
11. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
12. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
13. `docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md`
14. `assets/README.md`
15. `assets/reports/asset_audit_latest.md`
16. `samples/sample_jozz_vehicle_lab.cpp`
17. `samples/jozz_vehicle_asset_metadata.h`
18. `samples/jozz_vehicle_asset_metadata.cpp`
19. `samples/sample_joint.cpp` sections `WheelJoint` and `Driving` only as reference

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

## 8. Current hotkeys

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

Do not use `[` or `]`; they are global sample-switching keys.

M3A/M3B added no new hotkeys.

## 9. Validation commands

From repo root:

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
py tools\asset_audit.py
py tools\asset_contract_audit.py
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Manual sample check:

```text
Open:  Jozz Vehicle / Lab M2 Primitive Corner
Panel: Jozz Vehicle Lab M2.5 + M3A/M3B debug
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
no glTF mesh is rendered yet
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

Still not implemented:

```text
glTF mesh rendering
mesh collision
steering
four-corner vehicle
visual rig
skinning/animation
```

## 11. Next pass

The next pass should be Codex stabilization:

```text
stabilize -> clean -> organize -> verify -> prepare small visual import gate
```

Do not start full rig/import yet.

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
