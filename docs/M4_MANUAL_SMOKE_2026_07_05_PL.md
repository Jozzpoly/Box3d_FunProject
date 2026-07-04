# M4 Manual Smoke - Jozz Screenshots

Date: 2026-07-05
Status: manual visual confirmation from Jozz screenshots

## Scope

This document records the first visual smoke of the M4 suspension foundation after the one-sided suspension model was loaded in the active lab.

It is documentation only. No geometry, renderer, physics, contract, or asset-report change is implied by this note.

## Observed Screen State

Confirmed from the provided screenshots:

- `One_Sided_wheel_mount.gltf` is visible in the active `Jozz Vehicle / Lab M2 Primitive Corner` sample.
- The suspension mount texture/material is visible.
- The suspension proof is intentionally semi-transparent, so the wheel mesh and helper lines remain readable through it.
- The M3B.3 attached textured wheel remains visible at the primitive wheel body.
- The M3B.2.1 static textured wheel proof remains visible separately as a fixed comparison object.
- The primitive wheel debug shape is hidden in the shown state, while the wheel physics and attached wheel visual remain active.
- The M4 contract labels and helper lines are visible, including:
  - `M4 contract chassis visual socket`;
  - `M4 contract wheel center at rest`.
- The helper lines appear broadly coherent in the submitted views: chassis-side diagnostics stay near the chassis/root side, and wheel/rest diagnostics are drawn around the current wheel-corner rig.

## Critical Interpretation

The screenshots are a positive M4A/M4B smoke signal, not proof of a final suspension rig.

They confirm that the current foundation can:

- load the suspension visual asset;
- render it in the one-corner rig;
- keep it visual-only;
- show contract point diagnostics in the same scene as the attached wheel;
- keep M3B.3 wheel attachment usable while M4 diagnostics are enabled.

They do not yet prove:

- final suspension orientation;
- production-quality placement;
- animated arms;
- skinning;
- steering;
- cardan torque transfer;
- mesh collision;
- multi-corner behavior.

## Known Visual/Authoring Debt

Do not treat these as blockers for the current checkpoint:

- the M4 suspension proof is semi-transparent debug rendering, not final material presentation;
- some camera angles show close overlap between suspension proof, wheel, and chassis/root debug block;
- the static wheel proof is still visible as a comparison/debug object when enabled;
- the helper labels are debug labels and may visually overlap dense close-up views;
- final mount orientation and authoring-space polish remain future work.

## Stop Conditions For Next Agent

Stop and reassess before implementing further features if:

- contract runtime stops reporting sidecar + source glTF load status;
- primitive wheel debug hiding again leaves a thin collision overlay;
- attached wheel no longer stays centered on the primitive wheel body;
- M4 visual sockets are used to define `b3WheelJoint` frames or `restDrop`;
- asset reports change without explicitly running audit tools.

## Next Responsible Gate

The next reasonable gate is:

```text
M4C procedural damper/cardan visual proof using validated contract endpoints
```

That gate should use the existing sidecar contract endpoint data and remain visual-only.
