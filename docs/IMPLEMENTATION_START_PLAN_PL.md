# Implementation Start Plan — Jozz Vehicle Box3D Native

Status: active starting plan  
Date: 2026-07-03

## Phase 0 — repository seed

Done in this seed:

- project direction docs;
- ADRs;
- source glTF assets planned for `assets/source/`;
- sidecar `.asset.json` contracts;
- initial audit report;
- `tools/asset_audit.py`.

## Phase 1 — `jozz_vehicle_lab` executable

Goal: create a separate target rather than turning the upstream `samples` target into the game.

Minimum output:

```text
jozz_vehicle_lab.exe
window
camera
grid / primitive renderer
ImGui panel
one dynamic box falling onto ground
```

Rules:

- keep upstream Box3D source easy to diff against;
- reuse sample host/renderer/UI where practical;
- avoid gameplay systems in `samples/main.cpp`;
- commit when the target configures;
- commit again when it runs.

## Phase 2 — primitive suspension corner

Goal: one physical wheel corner before full vehicle assembly.

V0 model:

```text
Body A: chassis test body
Body B: wheel test body
Joint: Box3D wheel joint
Visual: primitive cube/sphere/capsule/debug axes
Input: throttle/brake through motor
Tuning: ImGui sliders for spring/damping/travel/motor
```

Do not use final glTF visual rig in this phase.

## Phase 3 — asset audit loop

Goal: every glTF can be checked before runtime.

`tools/asset_audit.py` should remain simple but strict:

- list duplicate node names;
- list root nodes;
- list skins/meshes/animations;
- compute world-space semantic node positions;
- compute mesh bounds;
- warn when expected sockets/axes are missing for a declared asset type.

## Phase 4 — minimal glTF rendering

Goal: show one static model in the native app.

Minimum:

- `cgltf` parser;
- vertex/index buffer;
- POSITION/NORMAL/TEXCOORD_0;
- basic material/texture;
- one simple shader;
- no skinning required yet.

## Phase 5 — visual rig

Goal: connect visuals to the primitive physics result.

V0:

- wheel mesh follows wheel body and spin;
- suspension mesh follows chassis/wheel;
- damper stretches between sockets;
- cardan shaft visually connects drive/hub sockets;
- cardan does not transmit physics torque yet.

## Phase 6 — full vehicle prototype

Goal: chassis + 4 corners + throttle/steering/brake, still rough.

Only after one corner feels acceptable.
