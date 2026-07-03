# ADR 0004 — Renderer strategy

Status: Accepted  
Date: 2026-07-03

## Decision

Use the Box3D sample stack for native host, camera, input, ImGui, primitive/debug rendering, and frame loop.

Add a small custom glTF rendering path later for Jozz's Blockbench models.

## Rationale

The sample stack already solves the hard native-app plumbing. However, it should not be treated as a complete game asset renderer.

## Consequences

- Early physics lab can use primitives/debug draw.
- glTF rendering is a separate track.
- Do not rewrite Sokol app glue from scratch unless the sample stack becomes a blocker.
- Do not delay wheel physics until textured models render.
