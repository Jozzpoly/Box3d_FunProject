# M0 Renderer Recon Report

Status: initial decision note  
Date: 2026-07-03

## Question

Can the current Box3D sample renderer directly render Jozz's textured glTF/Blockbench models?

## Current finding

The Box3D sample stack is valuable, but it should be treated as a host/debug/render foundation, not a finished game renderer for custom glTF assets.

Useful existing pieces:

- `samples/main.cpp` already wires Sokol app lifecycle, renderer init/shutdown, UI, camera, input, and frame pacing.
- `samples/gfx/renderer.*` exposes primitive instance paths such as cube, sphere, and capsule.
- `samples/gfx/draw.*` exposes debug/world drawing: lines, points, axes, hull wireframes, grids, primitive solids.
- ImGui is already part of the sample stack.

Missing or not yet accepted as usable for our goal:

- no obvious public high-level API for loading/rendering arbitrary textured glTF meshes;
- no asset/material system for Blockbench parts;
- no vehicle-specific visual rig path.

## Decision for M0

Use the sample stack as the shortest path to a native Windows host:

```text
Sokol window
camera/input
ImGui
debug draw
primitive rendering
frame pacing
```

Do **not** assume it can directly display the Blockbench models.

## Consequence

The project should grow two tracks:

```text
Track A — Primitive Physics Lab
  box/capsule/sphere/hull debug visuals
  wheel joint
  suspension tuning
  no glTF required

Track B — Visual Asset Pipeline
  cgltf
  mesh buffers
  texture upload
  simple shader
  sidecar contract
  visual rig
```

Track A should unblock first gameplay feel. Track B should make Jozz's models visible as soon as practical, but should not block physics experimentation.
