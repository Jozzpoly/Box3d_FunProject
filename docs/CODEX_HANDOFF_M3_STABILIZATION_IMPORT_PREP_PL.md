# Codex Handoff — M3 Stabilization and Import Prep

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Audience: Codex / stronger implementation agent  
Goal: stabilize, clean, and organize before serious Jozz model import

## 1. Mission

You are taking over a fragile but promising vehicle prototype inside Box3D samples.

Your task is **not** to add a big feature first.

Your task is:

```text
stabilize the current M2.5/M3A/M3B lab, clean the project boundaries, verify build/test workflow, and prepare the next small visual import gate.
```

The project must remain playable and understandable after every commit.

## 2. Mandatory read order

Read first:

1. `README_FOR_AGENTS.md`
2. `docs/CURRENT_STATE_INDEX_PL.md`
3. `docs/PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md`
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

Treat older M2/M2.1/M2.2/M2.3 docs as historical only.

## 3. Current validated state

Jozz manually validated:

```text
M2.5 primitive one-corner wheel-joint lab works
M3A asset-derived primitive defaults work
M3B semantic preview anchoring fix works
M3B.2-prep runtime metadata path works
```

Latest observed runtime metadata state:

```text
M3B metadata: runtime audit
metadata: loaded runtime asset audit report
source: ../../assets/reports/asset_audit_latest.json
```

No glTF mesh is rendered yet.

## 4. Non-negotiable physics model

Do not break this:

```text
Body A = static chassis/root debug rig
Body B = dynamic primitive wheel body
Joint  = b3WheelJoint
Frame A = rest wheel-center anchor on chassis/root
Frame B = wheel body center
Rest drop = explicit/tuned chassis-to-rest-wheel-center offset
Suspension spring rest = wheel-joint translation 0
```

Visual sockets are not automatically physics frames.

## 5. Non-negotiable separation model

Keep these separate:

```text
structural setup -> pending values + Apply rig rebuild
live root stress -> realtime chassis/root motion
semantic preview -> debug drawing only
runtime metadata -> data source only
physics -> primitive wheel joint and primitive collision
visual mesh -> not implemented yet
```

## 6. Your first task: stabilization audit in code

Before changing code, inspect:

```text
samples/sample_jozz_vehicle_lab.cpp
samples/jozz_vehicle_asset_metadata.h
samples/jozz_vehicle_asset_metadata.cpp
samples/CMakeLists.txt
```

Check for:

- compile issues;
- stale UI text;
- hard-coded metadata drift;
- hidden dependency on working directory;
- oversized functions/classes;
- obvious duplication;
- stale comments saying “no runtime metadata” after M3B.2-prep.

Do not rewrite everything at once.

## 7. Expected stabilization work

Good changes:

```text
small doc cleanup
small whitespace/comment cleanup
build workflow notes
safe extraction of helper structs/functions if build remains green
metadata parser robustness improvements
clearer fallback/runtime status handling
small validation helper if practical
```

Acceptable extraction target:

```text
sample_jozz_vehicle_lab.cpp becomes thinner
jozz_vehicle_asset_metadata.* remains metadata-only
new debug-preview helper file only if it reduces risk
```

Keep commits small.

## 8. Do not do yet

Do not add:

- full glTF renderer;
- full vehicle assembly;
- steering;
- mesh collision;
- damper/cardan/suspension rigging;
- skinning;
- animation;
- new hotkeys;
- replacement physics model;
- huge refactor without intermediate green builds.

Do not delete historical docs unless Jozz explicitly asks.

## 9. Build and validation commands

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

Manual checks:

```text
sample opens
W/S motor works
Space brake works
Q/E live root works
Apply rig rebuild still works
M3B semantic preview toggle works
HUD shows M3B metadata runtime audit or fallback
Reload metadata + reset defaults is safe
no glTF mesh appears yet
```

## 10. What “done” means for this Codex pass

This pass is successful when:

```text
build is green
current behavior is preserved
runtime metadata path still works or fallback is clearly reported
sample/code organization is cleaner or at least documented
stale docs/status are corrected
next gate is clearly described
```

## 11. Next feature after this pass

Only after stabilization, prepare:

```text
M3B.2 static wheel visual mesh at origin
```

Strict scope for that future feature:

```text
render one wheel mesh
at fixed debug origin
not attached to physics
not animated
not skinned
not collision
not full vehicle
```

The purpose is to prove the visual import/render path, not to finish vehicle assembly.

## 12. Final warning

This project is easy to damage by making it look more advanced than it is.

The correct next behavior is boring but strong:

```text
stabilize -> verify -> small visual import proof -> attach later -> rig later
```

Do not hide uncertain importer/rig decisions behind pretty rendered models.