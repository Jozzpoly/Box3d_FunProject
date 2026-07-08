# Codex Handoff - M4 Foundation Main-Ready State

Date: 2026-07-05
Branch: `jozz-vehicle-sandbox-m0`
Status: prepared for owner review and push/merge decision; not pushed or merged by this pass

## Current State

The active lab is:

```text
Jozz Vehicle / Lab M2 Primitive Corner
Panel: Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 foundation debug
```

The checkpoint contains:

- M2.5 one-corner primitive wheel-joint baseline;
- M3A asset-derived primitive defaults;
- M3B runtime metadata and semantic preview;
- M3B.2.1 static textured wheel proof;
- M3B.3 attached textured wheel proof, centered on the primitive wheel body;
- hidden primitive wheel debug shape path without residual thin collision overlay;
- M4F.1 sidecar asset contract runtime;
- M4A `One_Sided_wheel_mount.gltf` visual-only suspension mount proof;
- M4B narrow moving endpoint diagnostics for damper/cardan-style endpoints.

Box3D core physics remains unchanged.

## Manual Smoke Evidence

Jozz provided screenshots on 2026-07-05 showing:

- the suspension model loaded in the lab;
- suspension texture/material visible;
- suspension proof rendered semi-transparent;
- contract helper lines and labels visible;
- attached textured wheel still active;
- primitive wheel debug shape hidden while physics remains active.

See:

```text
docs/M4_MANUAL_SMOKE_2026_07_05_PL.md
```

## Validation State

The last code validation before this documentation handoff passed:

```powershell
cmd /c "set PATH=& cmake --build --preset windows-debug --target test"
cmd /c "set PATH=& cmake --build --preset windows-debug --target samples"
cmd /c "set PATH=& cmake --build --preset windows-debug --target jozz_vehicle_validation"
cmd /c "set PATH=& build\bin\Debug\test.exe"
cmd /c "set PATH=& build\bin\Debug\jozz_vehicle_validation.exe"
cmd /c "set PATH=& build\bin\Debug\samples.exe --sample 95 --frames 120"
```

Important validator behavior:

- suspension contract resolves from sidecar + source glTF;
- required suspension roles are checked;
- duplicate source node names remain warnings;
- validator does not open GUI;
- validator does not regenerate reports.

Before pushing or merging, rerun the validation commands from repo root.

## Changed Areas

Code:

```text
samples/CMakeLists.txt
samples/gfx/debug_adapter.*
samples/jozz_vehicle_asset_contract.*
samples/jozz_vehicle_corner_rig.*
samples/jozz_vehicle_visual_asset.*
samples/jozz_vehicle_primitive_corner_lab.cpp
samples/jozz_vehicle_validation.cpp
```

Docs:

```text
README_FOR_AGENTS.md
docs/CURRENT_STATE_INDEX_PL.md
docs/ASSET_CONTRACT_RUNTIME_V1_PL.md
docs/SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md
docs/M4_FOUNDATION_SUSPENSION_RIG_PLAN_PL.md
docs/M4_MANUAL_SMOKE_2026_07_05_PL.md
docs/CODEX_START_PROMPT_M4_FOUNDATION_PL.md
docs/CODEX_HANDOFF_M4_FOUNDATION_MAIN_READY_PL.md
```

## Report Boundary

Do not delete or clean `assets/reports/*latest*`.

Do not run these unless the next gate intentionally regenerates diagnostics:

```powershell
py tools\asset_audit.py
py tools\asset_contract_audit.py
```

At this handoff point, asset reports are expected to remain untouched.

## Non-Goals

Do not add yet:

- mesh collision;
- steering;
- full vehicle assembly;
- multi-body suspension;
- skinning or skeletal animation;
- final glTF importer;
- torque transfer through cardan;
- renderer polish for tire alpha shadowing or rim banding.

Do not derive physics joint frames or `restDrop` from suspension visual sockets.

## Next Gate

Recommended next gate:

```text
M4C procedural damper/cardan visual proof using validated contract endpoints
```

Minimum responsible scope:

- use existing `JozzVehicleAssetContract` resolved endpoints;
- draw procedural visual-only damper/cardan proof lines or simple cylinders;
- keep chassis-side endpoints in chassis/root space;
- keep wheel-side endpoints following the live primitive wheel body;
- document what is debug-only versus future authored part binding;
- rerun CLI and headless sample smoke.

## Push/Merge Checklist

Before pushing to `main`:

1. Confirm `git status --short`.
2. Confirm no unintended `assets/reports/*latest*` changes.
3. Run the validation commands.
4. Review the full diff by category: code, docs, generated shader/debug plumbing, local reports.
5. Commit from `jozz-vehicle-sandbox-m0`.
6. Push or merge to `main` only after owner approval.
