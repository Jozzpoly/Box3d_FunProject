# M2 — Primitive Corner Lab

> **STATUS: SUPERSEDED / HISTORICAL RECORD**  
> Superseded by `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md` and `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`.  
> Do not use this document as current architecture. Use it only to understand the first M2 baseline and the path that led to M2.5.

Status: superseded historical record  
Date: 2026-07-03

## What changed

A second Jozz Vehicle sample was added:

```text
Category: Jozz Vehicle
Name: Lab M2 Primitive Corner
Source: samples/sample_jozz_vehicle_lab.cpp
```

This is the first actual vehicle-physics step.

## Scene

The scene intentionally uses primitive collision/debug shapes only:

- static chassis test rig;
- dynamic primitive cylinder wheel;
- `b3WheelJoint` connecting chassis to wheel;
- ground box;
- ImGui tuning panel;
- W/S/Space input for motor/brake.

## Why the chassis is static in M2

This is deliberate. A full dynamic vehicle body would add mass distribution, roll, pitch, torque, contact, and balance problems at the same time.

M2 isolates the most important question first:

```text
Does a Box3D wheel joint give us a controllable suspension corner foundation?
```

If this feels bad even in isolation, full vehicle assembly would only hide the real problem.

## Controls

```text
W      drive forward
S      reverse
Space  brake
R      restart sample
Tab    show/hide UI, inherited from Box3D samples
F      frame view, inherited from Box3D samples
```

## ImGui tuning

The right-side controls expose:

- suspension spring on/off;
- spring hertz;
- damping ratio;
- suspension limit on/off;
- lower travel;
- upper travel;
- spin motor on/off;
- drive speed;
- drive torque;
- brake torque.

## Local validation commands

From repo root:

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Then run `samples` and pick:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Expected result:

- a chassis-ish static block sits above the ground;
- a primitive wheel is connected below it;
- the wheel is constrained by a visible joint;
- W/S changes wheel spin/motion;
- Space brakes;
- sliders visibly change suspension/motor behavior;
- no glTF models are shown yet.

## What to observe critically

Do not only check that it compiles. Actually play with it:

1. Does the wheel settle without violent shaking?
2. Does low damping bounce too much and high damping calm it down?
3. Does higher hertz feel stiffer?
4. Do travel limits visibly constrain suspension movement?
5. Does W/S motor input feel understandable?
6. Does braking stop the wheel reasonably?
7. Does anything explode numerically after 30–60 seconds?

## Current limitations

- The chassis is static, not a real vehicle body.
- The wheel is a primitive cylinder, not Jozz's mesh.
- Axis orientation currently follows the known working Box3D wheel sample pattern, not final Blockbench asset orientation.
- No tire model yet.
- No steering yet.
- No visual damper/cardans/wahacze yet.

## Next decision after local validation

Historical note: this section is no longer current. The current baseline is M2.5 and the recommended next gate is documented in `docs/CURRENT_STATE_INDEX_PL.md`.

If M2 is stable enough, the next step should be M3A:

```text
primitive corner + asset-derived dimensions
```

Meaning: still use primitive physics, but derive initial radius/width/travel values from `assets/contracts` and audit data.

Only after that should we attach glTF visual meshes as visual-only objects.