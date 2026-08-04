# ADR 0003 — Physics v0 uses Box3D wheel joint

Status: Superseded as the final suspension architecture; retained as the v0 decision
Date: 2026-07-03
Superseded: M6 introduced the multi-body wishbone/trailing-arm rig, while each wheel still uses a wheel joint for suspension/spin semantics.

## Decision

The first suspension prototype uses one Box3D wheel joint per corner.

## Rationale

A multi-body suspension matching every visual wahacz/damper part is attractive, but too expensive and unstable for the first playable test. A wheel joint gives a fast path to suspension travel, spring/damping, motorized wheel spin, and live tuning.

## Consequences

- Chassis and wheel are physical bodies.
- Wahacze/damper/cardan are visual-only in v0.
- The first success criterion is feel and controllability, not mechanical completeness.
- Multi-body suspension remains an option after the primitive corner proves what is missing.
