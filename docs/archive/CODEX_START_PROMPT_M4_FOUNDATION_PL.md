# Codex Start Prompt - M4 Foundation Continuation

Use this prompt to start the next Codex pass.

```text
You are Codex taking over Jozz Vehicle Box3D Native on branch `jozz-vehicle-sandbox-m0`.

Current active sample:
Jozz Vehicle / Lab M2 Primitive Corner

Current status:
M2.5 + M3A/M3B.3 + M4 Foundation is active.

Read first:
1. README_FOR_AGENTS.md
2. docs/CURRENT_STATE_INDEX_PL.md
3. docs/M4_FOUNDATION_SUSPENSION_RIG_PLAN_PL.md
4. docs/ASSET_CONTRACT_RUNTIME_V1_PL.md
5. docs/SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md
6. docs/M4_MANUAL_SMOKE_2026_07_05_PL.md
7. docs/CODEX_HANDOFF_M4_FOUNDATION_MAIN_READY_PL.md
8. docs/CODEX_HANDOFF_M3_STABILIZATION_IMPORT_PREP_PL.md
9. docs/PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md
10. docs/ASSET_CONTRACT_V2_DRAFT_PL.md
11. samples/jozz_vehicle_asset_contract.h/.cpp
12. samples/jozz_vehicle_corner_rig.h/.cpp
13. samples/jozz_vehicle_visual_asset.h/.cpp
14. samples/jozz_vehicle_primitive_corner_lab.cpp
15. samples/jozz_vehicle_visual_mesh.h/.cpp
16. samples/jozz_vehicle_validation.cpp

Important current behavior:
- M3B.3 attached textured wheel follows primitive wheel body.
- Primitive wheel debug shape can be hidden without leaving a thin collision overlay.
- M4 contract runtime loads `one_sided_wheel_mount.asset.json` and resolves points from `One_Sided_wheel_mount.gltf`.
- M4A draws the one-sided suspension mount visual-only.
- M4B draws debug-only moving damper/cardan endpoints.
- 2026-07-05 Jozz screenshots confirm the suspension model, texture, semi-transparent debug rendering and helper lines are visible in the active lab.
- The semi-transparent proof and rough debug overlap are not a request to polish renderer/materials in this gate.

Do not change:
- Box3D core.
- b3WheelJoint model.
- Frame A/Frame B semantics.
- explicit restDrop.
- primitive wheel collision.
- asset reports unless intentionally running audit tools.

Do not add yet:
- mesh collision;
- full vehicle;
- steering;
- multi-body suspension;
- final glTF importer;
- skinning/animation;
- torque transfer through cardan.

Validation:
cmd /c "set PATH=& cmake --build --preset windows-debug --target test"
cmd /c "set PATH=& cmake --build --preset windows-debug --target samples"
cmd /c "set PATH=& cmake --build --preset windows-debug --target jozz_vehicle_validation"
cmd /c "set PATH=& build\bin\Debug\test.exe"
cmd /c "set PATH=& build\bin\Debug\jozz_vehicle_validation.exe"
cmd /c "set PATH=& build\bin\Debug\samples.exe --sample 95 --frames 120"

Manual smoke:
- W/S drive, Space brake, Q/E live root.
- Apply rig rebuild.
- attached wheel visual follows body and spin.
- primitive debug shape hides fully.
- M4A suspension mount visual toggles independently.
- M4A contract points show wheel center/chassis mount/travel axis.
- M4B moving endpoints follow wheel travel only on wheel-side points.
- no asset reports changed unless explicitly intended.

Recommended next gate:
M4C procedural damper/cardan visual proof using contract endpoints.
```
