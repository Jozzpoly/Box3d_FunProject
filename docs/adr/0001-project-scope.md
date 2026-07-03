# ADR 0001 — Project scope

Status: Accepted  
Date: 2026-07-03

## Decision

This branch starts a distinct vehicle sandbox/game layer on top of Box3D.

It is not VAW, not Coopege, and not a continuation of the old M0-M2.5 prototype as the main runtime.

## Rationale

The new project needs native Windows/Sokol/Box3D foundations, vehicle physics, and Blockbench vehicle-part import. That is different enough from previous projects that sharing the same runtime would add confusion instead of speed.

## Consequences

- Use Box3D as the foundation.
- Keep a separate set of docs/ADRs.
- Keep asset import/contracts local to this project.
- Use previous prototype documentation as research, not as final architecture.
