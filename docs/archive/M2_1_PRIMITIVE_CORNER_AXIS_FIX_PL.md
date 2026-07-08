# M2.1 — Primitive Corner Axis / Dimension Fix

> **STATUS: SUPERSEDED / HISTORICAL RECORD**  
> Superseded by `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md` and `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`.  
> Do not use this document as current architecture. It remains useful only for the cylinder dimension/order lesson.

Status: superseded historical record  
Date: 2026-07-03

## Why this patch exists

Jozz validated M2 and reported that the wheel settled, suspension tuning behaved correctly, braking worked, and the sample stayed stable for several minutes.

But the wheel did not spin cleanly. The visible behavior suggested that the center axis / primitive wheel dimensions were wrong.

## Root cause found

The public Box3D API declares:

```c
b3HullData* b3CreateCylinder(float height, float radius, float yOffset, int sides);
```

M2 used the function as if the first argument were the radius. That produced a primitive wheel with bad radius/width proportions, making it look like a strange roller instead of a wheel.

## What changed

M2.1 updates `samples/sample_jozz_vehicle_lab.cpp`:

- wheel radius is now `0.52 m`;
- wheel width is now `0.44 m`;
- these values intentionally approximate the audited `Offroad_Big_Wheels.gltf` asset at `0.35 m / Blockbench unit`;
- `b3CreateCylinder` is now called as `height = wheel width`, `radius = wheel radius`;
- cylinder sides increased to `24` for a more readable primitive wheel;
- chassis/anchor/wheel spawn positions were adjusted for the larger asset-like radius;
- spin motor now uses torque only while W/S is held or Space brakes, instead of behaving like a constant idle brake.

## What to validate

Historical note: this validation section is no longer the current project gate. Use `docs/CURRENT_STATE_INDEX_PL.md` and M2.5 validation instead.

Pull and rebuild:

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Check specifically:

1. Does the primitive wheel now look like a wheel instead of a thick/wrong roller?
2. Is the spin visibly more centered and even?
3. Does idle feel less like permanent braking?
4. Do W/S still drive clearly?
5. Does Space still brake clearly?
6. Are suspension hertz/damping/travel still behaving as before?
7. Is there any new jitter from the larger radius?

## If the spin is still wrong

Do not jump to glTF yet.

The next debugging step should add an explicit axis diagnostic mode:

- draw/update a colored local axle line through the wheel center;
- compare wheel body transform, cylinder local axis, and wheel joint spin axis;
- optionally create three test variants: cylinder axis X, Y, Z;
- keep the best one as the canonical primitive reference before attaching real visual meshes.

## Current design judgement

M2.1 was promising but not finished. The cylinder argument-order lesson remains important, but the current architecture is M2.5.