# M1 — Jozz Vehicle Lab Smoke Test

Status: ready for local validation  
Date: 2026-07-03

## What changed

A new Box3D sample was added:

```text
Category: Jozz Vehicle
Name: Lab M1 Smoke
Source: samples/sample_jozz_vehicle_lab.cpp
```

It is intentionally tiny:

- native sample host only;
- no glTF rendering yet;
- one ground box;
- one dynamic cube falling under gravity;
- ImGui control panel with project status;
- HUD text explaining the next step.

## Why this is the right first code step

The original plan wanted a separate `jozz_vehicle_lab.exe`, but that is not the lowest-risk first code change. The Box3D sample host already has window setup, camera, input, ImGui, debug draw, renderer frame loop, and sample registration. Reusing it first avoids duplicating host code before we know what we need to split out.

A separate executable can still happen later, after the lab proves its shape.

## Local validation commands

From repo root:

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Then run the generated `samples` executable from the build output or through Visual Studio.

In the sample picker, search for:

```text
Jozz Vehicle / Lab M1 Smoke
```

Expected result:

- sample opens;
- camera works;
- ground is visible;
- one cube falls and settles;
- right-side controls show `Jozz Vehicle Lab M1`;
- HUD says `Jozz Vehicle Lab M1 Smoke`.

## If it fails

Copy the first real compiler error, not the whole terminal wall.

Likely failure areas:

1. The new sample file was not included by CMake.
2. MSVC/CMake cache needs reconfigure after pulling.
3. A Box3D API name changed in the local checkout.

Try this before deeper debugging:

```powershell
cmake --preset windows
cmake --build --preset windows-debug --target samples --verbose
```

## Next milestone after this passes

M2 should add a primitive one-corner wheel-joint lab:

```text
chassis primitive body
wheel primitive body
b3WheelJoint
ImGui sliders for spring/damping/limits/motor
keyboard throttle/brake
no glTF visuals yet
```

Only after M2 feels controllable should we start the minimal glTF renderer path.
