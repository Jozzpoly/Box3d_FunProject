# README_FOR_AGENTS — Jozz Vehicle Box3D Native

Status: project seed on branch `jozz-vehicle-sandbox-m0`  
Date: 2026-07-03  
Owner/creative director: Jozz / Przemek

## What this branch is

This branch starts **Jozz Vehicle Box3D Native**, a Windows/native vehicle sandbox built on top of Box3D.

The goal is not to keep modifying random Box3D samples forever. The goal is to use the Box3D repo as a proven physics/render-host foundation, then grow a separate game/lab layer around vehicle assembly, wheel suspension, visual rigs, and Blockbench-authored parts.

## Current project stance

- Box3D is the physics core.
- Sokol + ImGui from the Box3D sample stack are the preferred native host/UI foundation.
- The uploaded Blockbench/glTF models are accepted as **research-grade v1 assets**, not final contract assets.
- Jozz will adjust model orientation later while seeing the models in-game. Do **not** block early project work on perfecting authoring orientation now.
- For now, every asset may carry temporary orientation/scale correction metadata in its sidecar `.asset.json`.
- The first drivable prototype should use a **single Box3D wheel joint per suspension corner**.
- Wahacze, damper body, and cardan shaft are **visual-only in v0** unless future tests prove that physical multi-body suspension is worth the added complexity.

## Read first

1. `docs/PROJECT_DIRECTION_PL.md`
2. `docs/M0_RENDERER_RECON_REPORT_PL.md`
3. `docs/ASSET_CONTRACT_V2_DRAFT_PL.md`
4. `docs/IMPLEMENTATION_START_PLAN_PL.md`
5. `docs/adr/0001-project-scope.md`
6. `docs/adr/0002-orientation-policy.md`
7. `docs/adr/0003-physics-v0-wheel-joint.md`
8. `docs/adr/0004-renderer-strategy.md`
9. `assets/reports/asset_audit_initial_2026-07-03.md`

## Non-negotiables

- Keep commits small and understandable.
- Do not silently treat draft/open decisions as accepted.
- Do not trust node names alone. Duplicate node names exist in current glTF exports.
- Always compose parent transforms before reading socket/axis positions.
- Keep visual rig, physics prefab, and authoring asset data separated.
- Do not begin full vehicle builder UI before one physical suspension corner feels acceptable.
- Do not start full custom renderer architecture before proving the primitive physics lab works.

## Current assets

Source glTF files live in `assets/source/`.

Sidecar contracts live in `assets/contracts/`.

The initial audit report was generated from the current uploaded files and intentionally records duplicate roots and unfinished orientation decisions.

## Immediate next engineering target

Create a separate executable/target named `jozz_vehicle_lab`, reusing the Box3D sample host/renderer/UI stack where practical.

Minimum first visible result:

- window opens;
- camera works;
- ground/grid is visible;
- ImGui panel says `Jozz Vehicle Lab`;
- one primitive dynamic body falls onto ground;
- no gameplay yet.

After that, build a primitive single-corner suspension lab before rendering the glTF models.
