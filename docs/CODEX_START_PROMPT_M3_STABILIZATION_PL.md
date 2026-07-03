# Codex Start Prompt — M3 Stabilization

Use this prompt to start the next Codex pass.

```text
You are Codex taking over Jozz Vehicle Box3D Native on branch `jozz-vehicle-sandbox-m0`.

Your role is stabilization and import-prep, not a feature sprint.

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

Current validated state:
- M2.5 one-corner primitive wheel-joint lab works.
- M3A asset-derived primitive defaults work.
- M3B semantic preview anchoring is validated.
- M3B.2-prep runtime metadata path is validated by Jozz screenshot showing `M3B metadata: runtime audit`.

Your task:
Stabilize, clean, and organize the current project so it is ready for the next small visual import gate.

Do not start full rendering/rigging. Do not add steering. Do not add full vehicle assembly. Do not use mesh collision. Do not change the wheel-joint rest-anchor physics model.

First perform a critical code/docs audit. Then make small safe commits only. Keep build green after each meaningful change.

Validation commands:
py tools\asset_audit.py
py tools\asset_contract_audit.py
cmake --preset windows
cmake --build --preset windows-debug --target samples

Manual validation:
Open `Jozz Vehicle / Lab M2 Primitive Corner` and confirm:
- sample opens;
- W/S, Space, Q/E still work;
- Apply rig rebuild still works;
- M3B semantic preview still draws;
- HUD shows `M3B metadata: runtime audit` or `built-in fallback`;
- no glTF mesh appears yet.

Definition of done:
- build green;
- current behavior preserved;
- stale docs/status corrected;
- metadata boundary clear;
- sample file less risky or extraction plan clearly documented;
- next feature gate documented as `M3B.2 static wheel visual mesh at origin`, not full rig/import.
```
