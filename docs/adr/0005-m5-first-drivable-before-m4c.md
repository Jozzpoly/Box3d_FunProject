# ADR 0005 — M5 First Drivable precedes M4C visual polish

Status: Completed historical sequencing decision; not a current roadmap
Date: 2026-07-05

## Decision

Implement M5 First Drivable (dynamic chassis + four wheel joints + engine
steering + drive input) before the previously queued M4C procedural
damper/cardan visual gate. M4C is deferred, not cancelled.

## Rationale

ADR-0003 defines the first success criterion as feel and controllability.
Between M3B.2 and M4B every gate was a visual-only proof on a static rig, so
the project's riskiest open question — does `b3WheelJoint` hold up under a
real dynamic chassis — stayed untested while visual scope grew. The upstream
`Driving` sample demonstrates the engine natively supports the missing pieces
(steering springs, limits, per-joint motors), so the cost of answering the
question is one small gate that reuses the validated M2.4/M2.5 corner model.

A 2026-07-05 external audit of the project recommended this reprioritization.

## Consequences

- The vehicle lives in a render-free module (`jozz_vehicle_m5_vehicle.*`),
  which starts the physics-prefab layer from PROJECT_DIRECTION.
- `jozz_vehicle_validation.exe` gains a headless drive smoke, so vehicle
  physics regressions fail validation without a GUI.
- M4C runs after M5 and can target the drivable vehicle instead of the
  static one-corner rig.
- The corner lab (M2.5) remains the isolated tuning environment; M5 does not
  replace it.
- Feel tuning (M5.1) becomes the next Jozz-driven validation loop.
