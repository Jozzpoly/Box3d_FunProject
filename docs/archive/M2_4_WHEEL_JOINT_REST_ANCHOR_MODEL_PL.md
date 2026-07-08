# M2.4 — Wheel Joint Rest Anchor Model

Status: ready for local validation  
Date: 2026-07-03

## Why this patch exists

Jozz validated M2.3 and found another regression:

- offsets were displayed more logically;
- but the wheel appeared attached to the chassis block;
- `wheel rest distance` did not produce real clean suspension behavior.

That feedback exposed a deeper misunderstanding of `b3WheelJoint`.

## Solver-level finding

The Box3D wheel joint does not have a separate `restLength` or `restDistance` field.

In `src/wheel_joint.c`, the suspension spring solves:

```c
float c = translation;
float bias = joint->suspensionSoftness.biasRate * c;
```

So the spring's implicit rest state is:

```text
translation = 0
```

This means the joint frame on body A must represent the **rest wheel-center anchor on the chassis**, not the visible damper/chassis mount above the wheel.

The public API also states:

```text
Body A is the chassis and body B is the wheel.
The wheel rotates around local Z in frame B.
The wheel translates along local X in frame A.
```

## What was wrong in M2.3

M2.3 set frame A at the visual chassis mount and frame B at the wheel center. That made the initial translation roughly `-restDrop`.

Because the spring tries to drive translation back to zero, it pulled the wheel toward the chassis mount. Also, the old absolute travel range could make the starting state immediately outside the limit.

## Correct M2.4 model

M2.4 separates visual mount from physics rest anchor:

```text
chassis mount          visual diagnostic point near chassis bottom
rest wheel center      physics joint frame A position on chassis
actual wheel center    dynamic wheel body position
```

The wheel joint uses:

```cpp
jointDef.base.localFrameA.p = chassis-local(restWheelCenter);
jointDef.base.localFrameB.p = b3Vec3_zero;
```

So at spawn:

```text
translation = 0
spring rest = clean
```

`Rest drop` now moves the rest wheel-center anchor relative to the chassis. It is not used as an active spring length inside Box3D.

## Relative travel limits

M2.4 replaces absolute lower/upper translation UI with relative travel:

```text
Rebound travel down      => lower limit = -reboundTravel
Compression travel up    => upper limit = +compressionTravel
```

Defaults:

```text
reboundTravel     = 0.42 m
compressionTravel = 0.32 m
travel limits     = -0.42 .. +0.32
```

This keeps the initial state inside the range because initial translation is zero.

## Collision clarity

When `Wheel collides with chassis` is disabled, M2.4 now uses both:

```text
jointDef.base.collideConnected = false
negative shared filter groupIndex
```

This makes the lab more explicit and avoids ambiguous chassis/wheel contact while testing setup offsets.

## UI behavior

Structural values still require explicit rebuild:

- Rig height;
- Rest drop;
- Wheel radius;
- Wheel width;
- Wheel collides with chassis.

After changing these, press:

```text
Apply rig rebuild
```

Suspension tuning values apply live:

- hertz;
- damping;
- rebound travel;
- compression travel;
- motor/brake torque.

## Diagnostics

Axis diagnostics now draw:

- yellow cross at actual wheel center;
- cyan cross at visual chassis mount;
- purple cross at physics rest wheel center;
- cyan line mount -> rest center;
- purple line rest center -> actual wheel center;
- yellow axle line through actual wheel center.

The HUD also reports actual current translation relative to rest center.

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
Jozz Vehicle Lab M2.4
```

## What to test

1. At default settings, the wheel should not be stuck to the chassis block.
2. The HUD translation should start near `0.00` around rest.
3. Changing `Rest drop` then pressing `Apply rig rebuild` should move the whole rest wheel-center lower/higher relative to chassis without creating instant spring pull to the chassis mount.
4. `Rebound travel down` should control how far below rest the wheel can go.
5. `Compression travel up` should control how far above rest the wheel can go.
6. With chassis collision off, the wheel should ignore chassis block contact.
7. With chassis collision on, contact should happen only when visible geometry actually overlaps.
8. W/S and Space should still behave as before.
9. Axis line should remain centered.

## Quality gate

Do not move to glTF visuals until M2.4 feels clean. If M2.4 still fails, the next step should be a side-by-side axis/frame lab, not another blind patch.
