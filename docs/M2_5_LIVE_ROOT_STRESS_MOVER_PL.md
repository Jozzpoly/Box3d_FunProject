# M2.5 — Live Root Stress Mover

Status: ready for local validation  
Date: 2026-07-03

## Why this patch exists

After M2.4, Jozz confirmed that the wheel-joint rest anchor model is now behaving correctly:

- the wheel is not glued to the chassis;
- rest drop moves the rest center cleanly;
- rebound and compression travel work;
- chassis collision off/on behaves as expected.

Before moving to model visuals, Jozz requested one more lab feature: a way to move the whole suspension root up and down in realtime to stress-test the wheel, spring, travel limits, and ground contact under fast height changes.

## Design requirement

This must not behave like the older setup sliders.

There are now two different concepts:

```text
Structural setup
  changes rig height, rest drop, wheel radius, wheel width, chassis collision
  requires Apply rig rebuild

Live root stress test
  moves the chassis/root body in realtime
  no Apply button
  does not rebuild the wheel joint
  does not teleport the wheel
```

## Implementation model

M2.5 adds:

```cpp
m_liveRootOffset
m_liveRootSpeed
```

The live root offset moves only body A, the static chassis/root body:

```cpp
b3Body_SetTransform(m_chassisId, {0, GetLiveRigHeight(), 0}, b3Quat_identity);
```

The wheel body is not teleported. Because the wheel joint frame A is local to the chassis, the rest wheel center moves with the root automatically. The wheel then has to follow through the wheel joint spring/limit/contact simulation.

This is the intended stress behavior.

## Controls

UI:

```text
Live root offset      realtime slider
Live root key speed   speed for keyboard movement
Reset live root       returns offset to 0
```

Keyboard:

```text
[  lower live root
]  raise live root
```

The existing controls remain:

```text
W      drive forward
S      reverse
Space  brake
R      restart sample
```

## Preventing setup/live-root interference

A bug was caught during self-review before handoff: if structural sliders directly modified committed setup values, live root could accidentally read un-applied pending values.

M2.5 fixes that by separating:

```text
committed setup values   used by physics and live root
pending edit values      used only by structural UI sliders
```

`Apply rig rebuild` copies pending values into committed values and rebuilds bodies/joint once.

Until Apply is pressed, live root continues to use the committed setup.

## What to validate

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
Jozz Vehicle Lab M2.5
```

Test plan:

1. Move `Live root offset` slider quickly up/down. It should move immediately without Apply.
2. Use `[` and `]` to move root down/up continuously.
3. The chassis/root should move, but the wheel should not teleport with it.
4. The suspension should stretch/compress and settle naturally.
5. Fast downward movement should create stronger ground/contact reactions.
6. Changing pending structural sliders without Apply should not affect live root behavior.
7. Press Apply after structural changes and confirm live root offset is preserved.
8. Reset live root should return only the realtime root offset to zero, not reset the structural setup.

## Important limitation

The live root mover currently teleports the static chassis transform each frame. That is acceptable for this debug/stress lab, but it is not the final vehicle-body model. A real vehicle body later should be dynamic or kinematic according to gameplay needs.

This is a testing rig, not final suspension architecture.
