# M2.2 — Centered Wheel Pivot and Rig Controls

Status: ready for local validation  
Date: 2026-07-03

## Why this patch exists

Jozz reported that M2.1 was closer to the intended wheel dimensions, but the wheel still did not rotate around its true center. It behaved like an eccentric mass/vibration mechanism instead of a centered wheel.

That diagnosis was correct.

## Critical finding

In M2/M2.1 the wheel joint body-B anchor was computed from a world point above the wheel center:

```cpp
b3Pos anchor = { 0.0f, 1.32f, 0.0f };
jointDef.base.localFrameB.p = b3Body_GetLocalPoint( m_wheelId, anchor );
```

But the wheel body center was lower than that. This made the joint spin/motor act through an offset local pivot.

A real wheel primitive must have the joint body-B frame at the wheel body origin/center of mass:

```cpp
jointDef.base.localFrameB.p = b3Vec3_zero;
```

The chassis/body-A frame can still reference that same world wheel-center point, transformed into chassis local space. This matches the known Box3D wheel-joint sample pattern: body-B local frame is at the wheel center.

## What changed

`Jozz Vehicle / Lab M2 Primitive Corner` is now M2.2 internally.

Changes:

- body-B joint pivot is centered at the wheel body origin;
- chassis/body-A joint frame uses the wheel center transformed into chassis local space;
- corner creation was moved into `CreateCorner()` so setup changes can rebuild cleanly;
- added `Rig height` slider;
- added `Chassis-wheel rest drop` slider;
- added `Wheel radius` and `Wheel width` sliders;
- added optional `Wheel collides with chassis` diagnostic toggle;
- added `Axis diagnostics` toggle;
- axis diagnostics draw:
  - wheel body axes;
  - cross at true wheel center;
  - yellow axle line through the wheel center.

## Local validation commands

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Right-side panel should say:

```text
Jozz Vehicle Lab M2.2
```

## What to validate now

Primary checks:

1. Does the wheel now spin around its visible center?
2. Does the yellow axle line pass through the true wheel center?
3. Does the center cross stay in the middle of the wheel while spinning?
4. Are the huge eccentric-mass vibrations gone?
5. Does W/S still drive clearly?
6. Does Space still brake clearly?
7. Do spring hertz/damping/travel limits still work?

Rig controls:

1. Move `Rig height` up/down. The whole corner should rebuild higher/lower.
2. Move `Chassis-wheel rest drop`. The relationship between chassis block and wheel should change.
3. Toggle `Wheel collides with chassis` and lower the rest drop to test wheel/chassis clearance.
4. Change radius/width carefully and watch whether the axis remains centered.

## Important judgement

If M2.2 still spins off-center, do not continue toward glTF.

The next step would be a deeper axis-frame diagnostic with multiple axis variants. But the main known eccentric-pivot bug is now fixed, so M2.2 should be a much cleaner baseline.

## Current limitation

This is still a static chassis test rig, not a full car. That is intentional. The goal is to prove the corner before adding full vehicle mass, steering, tire model, and real visual meshes.
