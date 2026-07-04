# Codex Handoff — M3 Stabilization and Import Prep

Date: 2026-07-04
Branch: `jozz-vehicle-sandbox-m0`  
Audience: Codex / stronger implementation agent  
Goal: continue after M3B.2.1 static textured visual proof toward visual-only wheel attachment

## 1. Mission

You are taking over a fragile but promising vehicle prototype inside Box3D samples.

Your task is **not** to add a big feature or full importer.

Your task is:

```text
stabilize the current M2.5/M3A/M3B.2.1 lab, keep the build/test workflow green, and prepare the next small visual-only attachment gate.
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

Treat older M2/M2.1/M2.2/M2.3 docs as historical only.

## 3. Current validated state

Jozz manually validated:

```text
M2.5 primitive one-corner wheel-joint lab works
M3A asset-derived primitive defaults work
M3B semantic preview anchoring fix works
M3B.2-prep runtime metadata path works
M3B.2 static visual-only wheel mesh proof exists
M3B.2.1 baseColor texture proof exists for that same static mesh
```

Latest observed runtime metadata state:

```text
M3B metadata: runtime audit
metadata: loaded runtime asset audit report
source: ../../assets/reports/asset_audit_latest.json
```

One textured `Offroad_Big_Wheels.gltf` mesh primitive is rendered at a fixed debug origin. It is visual-only, not attached to physics, and not a full material/skin/animation/collision importer.

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
visual mesh -> M3B.2.1 static textured proof only, no physics authority
```

## 6. Your first task: stabilization audit in code

Before changing code, inspect:

```text
samples/sample_jozz_vehicle_lab.cpp
samples/jozz_vehicle_primitive_corner_lab.h
samples/jozz_vehicle_primitive_corner_lab.cpp
samples/jozz_vehicle_asset_metadata.h
samples/jozz_vehicle_asset_metadata.cpp
samples/CMakeLists.txt
samples/jozz_vehicle_visual_mesh.h
samples/jozz_vehicle_visual_mesh.cpp
samples/jozz_vehicle_image_decode.h
samples/jozz_vehicle_image_decode.cpp
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
jozz_vehicle_primitive_corner_lab.* owns the current M2.5/M3A/M3B.2.1 corner lab
jozz_vehicle_asset_metadata.* remains metadata-only
jozz_vehicle_validation.exe validates metadata/defaults from CLI
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
cmd /c "set PATH=& cmake --build --preset windows-debug --target test"
cmd /c "set PATH=& cmake --build --preset windows-debug --target samples"
cmd /c "set PATH=& cmake --build --preset windows-debug --target jozz_vehicle_validation"
cmd /c "set PATH=& build\bin\Debug\test.exe"
cmd /c "set PATH=& build\bin\Debug\jozz_vehicle_validation.exe"
```

Run `py tools\asset_audit.py` and `py tools\asset_contract_audit.py` only when intentionally regenerating repo reports.

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
M3B.2.1 static textured wheel proof toggles
static wheel mesh is visible with texture status but not attached to physics
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

Only after validating M3B.2.1, prepare:

```text
M3B.3 visual-only wheel mesh attached to primitive wheel body
```

Strict scope for that future feature:

```text
render one wheel mesh
follow the primitive wheel body transform visually
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
