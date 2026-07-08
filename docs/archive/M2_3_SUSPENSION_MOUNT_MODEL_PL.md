# M2.3 — Suspension Mount Model

> **STATUS: SUPERSEDED / HISTORICAL RECORD — CONTAINS WRONG CURRENT MODEL**  
> Superseded by `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md` and `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`.  
> Do not use this document as current architecture. It is useful because it records a real mistake: treating the visual chassis/damper mount as body-A frame for `b3WheelJoint`.

Status: superseded historical record  
Date: 2026-07-03

## Why this patch exists

Jozz validated M2.2 and confirmed that the wheel pivot/spin issue was fixed:

- yellow axle passed through the wheel center;
- wheel rotated around that axis;
- eccentric mass vibrations disappeared;
- rig height worked.

But `Chassis-wheel rest drop` and `Wheel radius` still produced strange behavior, as if old positions or offsets were being remembered.

## Critical analysis

M2.2 still had a flawed conceptual model.

It centered the wheel pivot correctly, but the chassis-side joint frame was still placed at the wheel center. That meant the scene did not have a real suspension relationship:

```text
chassis mount
   |
   | rest drop / suspension travel
   |
wheel center
```

Instead, the chassis frame and wheel frame were effectively initialized at the same wheel-center point. This made `rest drop` a misleading setup variable.

M2.2 also rebuilt bodies/joints immediately during slider drag. That is a bad lab behavior because dragging a structural slider can trigger many destroy/create cycles during a single UI interaction. Even if Box3D handles it, the test feels dirty and can look like stale contacts or offset memory.

## What changed

M2.3 changed the primitive corner model to use explicit geometry:

```text
rig height          = chassis center Y
chassis mount Y     = rig height - chassis half height
wheel center Y      = chassis mount Y - rest drop
wheel bottom Y      = wheel center Y - wheel radius
```

The wheel joint used:

```cpp
jointDef.base.localFrameA.p = b3Body_GetLocalPoint(m_chassisId, chassisMount);
jointDef.base.localFrameB.p = b3Vec3_zero;
```

So body A was treated as a real chassis mount and body B as the true wheel center.

## Why this is superseded

This model looked intuitive but was wrong for the current `b3WheelJoint` baseline.

M2.4 proved the important solver/API lesson:

```text
b3WheelJoint implicit spring rest = translation 0
Frame A = rest wheel-center anchor on chassis
Frame B = wheel center
```

If frame A is placed at the visual chassis/damper mount, the spring tries to pull the wheel toward that mount because the initial joint translation is not zero.

So this document must not guide future implementation except as a warning.

## UI change

Structural sliders no longer rebuild instantly while dragging.

These controls mark the setup as dirty:

- Rig height;
- Chassis-wheel rest drop;
- Wheel radius;
- Wheel width;
- Wheel collides with chassis.

After changing them, press:

```text
Apply rig rebuild
```

This destroys/recreates the corner once, cleanly.

This Apply-based idea survived into M2.5, but the frame-A model did not.

## Diagnostics

Axis diagnostics in this version showed:

- wheel body axes;
- yellow cross at wheel center;
- cyan cross at chassis mount;
- cyan line from chassis mount to wheel center;
- yellow axle line through wheel center.

This helped expose the mismatch that M2.4 later fixed.

## Local validation

Historical note: this validation section is no longer the current project gate. Use `docs/CURRENT_STATE_INDEX_PL.md` and M2.5 validation instead.

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Panel historically said:

```text
Jozz Vehicle Lab M2.3
```

The current panel should say:

```text
Jozz Vehicle Lab M2.5
```

## What to test carefully

1. Move `Rig height`, press `Apply rig rebuild`. The entire corner should move cleanly.
2. Move `Chassis-wheel rest drop`, press `Apply rig rebuild`. The cyan mount-to-wheel line should change length.
3. Move `Wheel radius`, press `Apply rig rebuild`. The wheel size should change, and `wheel bottom y` should update logically.
4. With `Wheel collides with chassis` off, the wheel should not collide with the chassis block.
5. With `Wheel collides with chassis` on, lowering rest drop / increasing radius should create real contact only when the visible wheel reaches the chassis.
6. The yellow axle should stay centered through all rebuilds.
7. Suspension hertz/damping/travel should still behave logically after rebuild.

## If this still feels wrong

The correct next step became M2.4: prove the actual wheel-joint rest-anchor model instead of tuning around symptoms.

## Current judgement

M2.3 is valuable as a failure record, not as current architecture.

What survived:

```text
structural setup rebuilds require Apply
wheel center == body-B pivot
```

What did not survive:

```text
chassis visual mount == body-A frame
rest drop == direct spring rest distance
```

The current authority is M2.5, with M2.4 as the core solver/model explanation.