# Codex Start Prompt — M3 Stabilization

Use this prompt to start the next Codex pass.

```text
You are Codex taking over Jozz Vehicle Box3D Native on branch `jozz-vehicle-sandbox-m0`.

Your role is post-M3B.2.1 stabilization and M3B.3 visual-attach prep, not a feature sprint.

Read first, in order:
1. README_FOR_AGENTS.md
2. docs/CURRENT_STATE_INDEX_PL.md
3. docs/PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md
4. docs/CODEX_HANDOFF_M3_STABILIZATION_IMPORT_PREP_PL.md
5. docs/M3B_2_RUNTIME_METADATA_VALIDATION_PL.md
6. docs/M3B_2_PREP_RUNTIME_METADATA_REPORT_PL.md
7. docs/M3B_SEMANTIC_PREVIEW_ANCHORING_FIX_PL.md
8. docs/M3B_SEMANTIC_DEBUG_PREVIEW_IMPLEMENTATION_REPORT_PL.md
9. docs/M3A_IMPLEMENTATION_REPORT_PL.md
10. docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md
11. docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md
12. assets/README.md
13. assets/reports/asset_audit_latest.md
14. samples/sample_jozz_vehicle_lab.cpp
15. samples/jozz_vehicle_asset_metadata.h
16. samples/jozz_vehicle_asset_metadata.cpp
17. samples/jozz_vehicle_asset_dimensions.h
18. samples/jozz_vehicle_asset_dimensions.cpp
19. samples/jozz_vehicle_debug_preview.h
20. samples/jozz_vehicle_debug_preview.cpp
21. samples/jozz_vehicle_primitive_corner_lab.h
22. samples/jozz_vehicle_primitive_corner_lab.cpp
23. samples/jozz_vehicle_visual_mesh.h
24. samples/jozz_vehicle_visual_mesh.cpp
25. samples/jozz_vehicle_image_decode.h
26. samples/jozz_vehicle_image_decode.cpp
27. samples/jozz_vehicle_validation.cpp

Current validated state:
- M2.5 one-corner primitive wheel-joint lab works.
- M3A asset-derived primitive defaults work.
- M3B semantic preview anchoring is validated.
- M3B.2-prep runtime metadata path is validated by Jozz screenshot showing `M3B metadata: runtime audit`.
- M3B.2 static visual-only wheel mesh proof exists at a fixed debug origin.
- M3B.2.1 static wheel mesh can load TEXCOORD_0 + baseColor PNG data URI texture.

Your task:
Stabilize, verify, and prepare the current project for the next small gate: `M3B.3 visual-only wheel mesh attached to primitive wheel body`.

Do not start full rendering/rigging. Do not add steering. Do not add full vehicle assembly. Do not use mesh collision. Do not change the wheel-joint rest-anchor physics model.

First perform a critical code/docs audit. Then make small safe commits only. Keep build green after each meaningful change.

Validation commands:
cmd /c "set PATH=& cmake --build --preset windows-debug --target test"
cmd /c "set PATH=& cmake --build --preset windows-debug --target samples"
cmd /c "set PATH=& cmake --build --preset windows-debug --target jozz_vehicle_validation"
cmd /c "set PATH=& build\bin\Debug\test.exe"
cmd /c "set PATH=& build\bin\Debug\jozz_vehicle_validation.exe"

Run asset audits only when intentionally regenerating reports:
py tools\asset_audit.py
py tools\asset_contract_audit.py

Manual validation:
Open `Jozz Vehicle / Lab M2 Primitive Corner` and confirm:
- sample opens;
- W/S, Space, Q/E still work;
- Apply rig rebuild still works;
- M3B semantic preview still draws;
- HUD shows `M3B metadata: runtime audit` or `built-in fallback`;
- M3B.2.1 static textured wheel proof toggles;
- static wheel mesh is visible with texture status and is not attached to physics.

Definition of done:
- build green;
- current behavior preserved;
- stale docs/status corrected;
- metadata boundary clear;
- sample file is thin registration glue and the primitive corner lab lives in `jozz_vehicle_primitive_corner_lab.*`;
- `jozz_vehicle_validation.exe` validates metadata/defaults from CLI;
- next feature gate documented as `M3B.3 visual-only wheel mesh attached to primitive wheel body`, not full rig/import.
```
