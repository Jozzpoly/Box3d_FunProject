# M2.3 — Suspension Mount Model

Status: ready for local validation  
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

M2.3 changes the primitive corner model to use explicit geometry:

```text
rig height          = chassis center Y
chassis mount Y     = rig height - chassis half height
wheel center Y      = chassis mount Y - rest drop
wheel bottom Y      = wheel center Y - wheel radius
```

The wheel joint now uses:

```cpp
jointDef.base.localFrameA.p = b3Body_GetLocalPoint(m_chassisId, chassisMount);
jointDef.base.localFrameB.p = b3Vec3_zero;
```

So body A is a real chassis mount and body B is the true wheel center.

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

## Diagnostics

Axis diagnostics now show:

- wheel body axes;
- yellow cross at wheel center;
- cyan cross at chassis mount;
- cyan line from chassis mount to wheel center;
- yellow axle line through wheel center.

This should make it obvious whether the joint is using the correct points.

## Local validation

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Panel should say:

```text
Jozz Vehicle Lab M2.3
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

The next step is not glTF.

The next step would be M2.4: a split visualization of three wheel-joint frame variants side-by-side so we can prove which frame convention is physically correct before any real model is attached.

## Current judgement

M2.3 is the first version where the lab model matches the mental model needed for real vehicle parts:

```text
chassis mount != wheel center
wheel center == body-B pivot
rest drop == real initial suspension extension
structural setup rebuilds are explicit and clean
```
