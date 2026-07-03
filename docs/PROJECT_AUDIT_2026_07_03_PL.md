# Project Audit — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: post-M2.5 primitive wheel-corner lab validated by Jozz

## 1. Purpose of this audit

This document captures the real state of the project after the first successful physics-foundation loop.

The goal is not to pretend the project is already clean. The goal is to prevent repeating the same mistakes while moving toward the next major features:

- asset-derived primitive dimensions;
- glTF visual mesh rendering;
- visual rig attachment for wheel/suspension/damper/cardan;
- eventually full vehicle assembly.

This audit should be read before continuing work.

## 2. Current high-level project state

The branch is no longer just upstream Box3D. It is now a seeded Jozz Vehicle branch with:

- project direction documents;
- ADRs;
- startup asset folders;
- glTF source models;
- sidecar asset contract drafts;
- an asset audit tool and report;
- a primitive Jozz Vehicle sample inside the Box3D samples host;
- a validated primitive one-corner wheel-joint lab.

The branch is intentionally still using the Box3D `samples` executable instead of a separate standalone game executable. This was a pragmatic choice: the existing sample host already gives us windowing, camera, debug draw, ImGui, input and build integration.

The long-term project goal is still not to permanently live as a random Box3D sample. The current sample is a foundation lab.

## 3. Current branch diff scope

Compared with `main`, the branch adds/changes a focused set of project files:

- `README_FOR_AGENTS.md`;
- `assets/README.md`;
- `assets/source/*.gltf`;
- `assets/contracts/*.asset.json`;
- `assets/reports/*`;
- `tools/asset_audit.py`;
- project direction / ADR / milestone docs;
- `samples/sample_jozz_vehicle_lab.cpp`;
- `samples/CMakeLists.txt`.

The most important source file right now is:

```text
samples/sample_jozz_vehicle_lab.cpp
```

It contains:

- `JozzVehicleLabM1` — smoke test with a falling cube;
- `JozzVehiclePrimitiveCornerM2` — current M2.5 primitive wheel-corner lab.

## 4. Confirmed working milestones

### M1 — Native smoke sample

Jozz confirmed that `Jozz Vehicle / Lab M1 Smoke` works:

- cube spawns in the air;
- cube falls onto ground;
- camera works;
- Box3D sample controls work;
- ImGui/sample host path is healthy.

This proved the native host path and justified building on the samples app for early foundation work.

### M2 — Primitive wheel-corner lab progression

The M2 line went through several painful but valuable iterations:

- M2: basic wheel joint, spring, damping, limits, W/S/Space;
- M2.1: cylinder dimension/order correction;
- M2.2: wheel pivot centered at body-B origin;
- M2.3: bad attempt to model chassis mount as frame A;
- M2.4: correct rest-anchor model for `b3WheelJoint`;
- M2.5: realtime live root stress mover with pending/committed setup separation.

Jozz validated M2.5 behavior:

- live root offset slider works realtime without Apply;
- wheel reacts smoothly through spring, not by teleporting;
- suspension stretches/compresses;
- fast downward root movement creates stronger contact reactions;
- pending structural values do not affect live root until Apply;
- Apply commits structural setup.

This is a meaningful foundation checkpoint.

## 5. Current M2.5 model

### 5.1 Core idea

There are now two separate categories of controls.

```text
Structural setup
  - rig height
  - rest drop
  - wheel radius
  - wheel width
  - wheel/chassis collision toggle
  - requires Apply rig rebuild

Live root stress test
  - live root offset
  - live root key speed
  - reset live root
  - realtime, no Apply
  - moves chassis/root only
```

This separation is important and should not be undone.

### 5.2 Why pending/committed setup exists

A bug was caught before M2.5 handoff: if structural sliders directly modified the physics values, live root could accidentally use un-applied `Rest drop`, `Rig height`, or wheel dimensions.

M2.5 fixes this by having:

```text
committed setup values   used by physics and live root
pending edit values      used only by structural UI
```

Only `Apply rig rebuild` copies pending values into committed values and rebuilds the body/joint setup.

### 5.3 Wheel-joint rest-anchor model

The core `b3WheelJoint` lesson:

```text
spring rest state is translation = 0
```

So frame A should not be the visible damper/chassis mount when using the built-in wheel-joint spring. Frame A is the **rest wheel-center anchor** on the chassis. Body B frame is the wheel center.

Current model:

```text
visual chassis mount       diagnostic only
rest wheel center          body-A frame position in chassis local space
actual wheel center        body-B dynamic body origin
```

This fixed the M2.3 regression where the wheel was being pulled toward the chassis.

### 5.4 Live root mover

The live root mover teleports the static chassis/root body in realtime using `b3Body_SetTransform`.

This is acceptable for a debug stress lab. It is not the final vehicle-body architecture.

The wheel body is not teleported, so the suspension and contact solver are actually stressed.

## 6. Major problems we hit and lessons learned

### Problem 1 — Moving too fast after partial success

The early M2 patches were too reactive. They fixed visible symptoms without first proving the wheel-joint model from source/API behavior.

Lesson:

```text
When behavior is physically strange, read the joint implementation/API first.
Do not tune around a misunderstood constraint.
```

### Problem 2 — Cylinder dimensions were wrong

`b3CreateCylinder` takes arguments as:

```text
height, radius, yOffset, sides
```

The early code treated it like radius/width in the wrong order. This caused a primitive that looked and behaved like a bad roller instead of a wheel.

Lesson:

```text
Never rely on intuitive parameter order for low-level shape builders.
Always verify the signature before using it in physics tests.
```

### Problem 3 — Eccentric wheel pivot

Early M2 placed the wheel joint body-B anchor above the wheel center. That made the wheel spin like an unbalanced eccentric mass.

Correct rule:

```text
For a primitive wheel, body-B frame position should be the wheel body origin / center of mass.
```

Current code uses `localFrameB.p = b3Vec3_zero`.

### Problem 4 — Misunderstanding rest drop

M2.3 tried to use the visible chassis mount as the joint frame A. That made the starting suspension translation roughly equal to `-restDrop`, while the spring wanted `translation = 0`.

Result:

- wheel appeared attached/pulled to chassis;
- limits fought the setup;
- rest distance seemed useless.

Correct model:

```text
Rest drop positions the rest wheel-center anchor.
It is not a separate spring rest length field inside b3WheelJoint.
```

### Problem 5 — Rebuilding physics during slider drag

M2.2 rebuilt bodies/joints immediately while dragging sliders. This made the lab feel dirty and could look like stale contacts or offset memory.

Correct rule:

```text
Structural changes require explicit Apply.
Live runtime controls are separate and never rebuild structural physics.
```

### Problem 6 — Pending values leaking into live behavior

A subtle M2.5 risk was caught before handoff: if the structural UI directly edited committed variables, live root would read un-applied values.

Fix:

```text
pending edit values != committed physics values
```

This pattern should be kept for future editor/debug UI.

### Problem 7 — Keyboard hint confusion

The test instruction `[/]` was misunderstood as the letter `i`. On Polish keyboards, square brackets may be inconvenient.

Action:

```text
Replace or supplement [/] controls with clearer keys or UI buttons before relying on keyboard stress tests.
```

The slider works, so this is not a physics blocker.

## 7. Current assets audit

Current source files:

```text
Asset_Dumper.gltf
Cardan_shaft.gltf
Offroad_Big_Wheels.gltf
One_Sided_wheel_mount.gltf
```

Known current asset state:

- research/startup assets, not final production contracts;
- duplicate root names exist;
- orientation is not final;
- scale is prototype-only;
- marker/socket naming is useful but not enough alone;
- importer must compose parent transforms.

Important audit facts:

- all four current glTF files have duplicate node names;
- wheel has useful `Socket_WheelMount`, `Axis_WheelSpin_A/B`, tire radius/width markers;
- one-sided suspension has chassis/wheel/damper/cardan/travel semantic nodes;
- cardan shaft currently has no semantic nodes;
- dumper has upper/stretch/lower parts.

The asset audit tool is useful but still basic.

## 8. Current code structure audit

### 8.1 Good

- The project uses Box3D's stable existing samples host.
- M1/M2 are easy to launch through the existing sample picker.
- Primitive wheel-corner lab is now aligned with `b3WheelJoint` semantics.
- Debug UI is rich enough to tune and stress the corner.
- The code keeps physics primitive and avoids premature visual complexity.

### 8.2 Weak

- `samples/sample_jozz_vehicle_lab.cpp` is becoming a large monolithic experiment file.
- M2 iteration docs are useful but now noisy and partially superseded.
- `README_FOR_AGENTS.md` still says the immediate target is a separate executable, while actual progress used the sample host first.
- The root `README.md` is still upstream Box3D and does not explain the Jozz branch.
- `samples/CMakeLists.txt` lost upstream comments during a safety-blocked earlier update and should be cleaned up later.
- No automated smoke test exists for the Jozz sample registration/build path.
- No runtime glTF importer exists yet.
- No asset-contract validator exists beyond the basic audit tool.

### 8.3 Acceptable-for-now compromises

- Keeping the lab inside `samples` is acceptable until primitive physics and first visual attach are proven.
- Using a static chassis/root body is acceptable for one-corner lab isolation.
- Teleporting the static chassis for live root stress testing is acceptable as a debug rig, not as final vehicle behavior.
- Primitive cylinder wheel collision is acceptable before visual mesh rendering.

## 9. Documentation structure audit

Current docs are valuable but fragmented.

Useful permanent docs:

- `README_FOR_AGENTS.md`;
- `docs/PROJECT_DIRECTION_PL.md`;
- ADRs;
- asset contract draft;
- implementation plan;
- M2.4/M2.5 docs;
- this audit.

Useful historical docs, but not primary handoff docs anymore:

- M2 initial;
- M2.1;
- M2.2;
- M2.3.

These should not be deleted yet because they document mistakes and lessons, but future agents should not treat them as current architecture. A docs index should mark them as superseded by M2.4/M2.5.

## 10. Things that currently clutter or confuse the project

### 10.1 Upstream root README

`README.md` still describes Box3D generally. It is not wrong, but it is not enough for this branch. Add a short top-level Jozz branch note or a new `JOZZ_VEHICLE_README.md` and link it.

### 10.2 Milestone docs without current/superseded status

M2.1/M2.2/M2.3 are easy to misread as active guidance. Add an index file or status headers:

```text
Superseded by M2.4/M2.5
```

### 10.3 Monolithic lab source

`sample_jozz_vehicle_lab.cpp` is fine for fast iteration, but before adding glTF rendering it should be split or at least organized with clear sections.

Possible future split:

```text
sample_jozz_vehicle_lab.cpp          registration / high-level samples
jozz_vehicle_corner_lab.h/.cpp       primitive corner rig
jozz_vehicle_asset_dimensions.h/.cpp asset-derived measurements
```

Do not split too early if it slows feature work, but do not add importer/rendering into the same monolith.

### 10.4 CMake comments regression

Earlier CMake update reduced comments in `samples/CMakeLists.txt`. Functional behavior is more important now, but this should be cleaned once the foundation phase pauses.

### 10.5 Ambiguous keyboard controls

`[` and `]` work technically but are not friendly. Add UI buttons or switch to a clearer input scheme before relying on keyboard testing.

## 11. Current non-negotiables going forward

1. Do not attach glTF visuals until primitive corner remains clean after stress tests.
2. Do not use glTF mesh as physics collision for wheels/suspension in v0.
3. Do not trust node names alone.
4. Do not mix visual rig markers with physics joint frames without explicit conversion.
5. Do not rebuild bodies/joints during slider drag.
6. Do not let pending UI values affect live physics.
7. Do not let historical M2 docs override M2.4/M2.5 model.
8. Do not begin full vehicle assembly before one corner can load dimensions and show visuals safely.

## 12. Known gaps and unfinished work

### Foundation gaps

- no top-level Jozz-specific readme;
- no docs index / current-state map;
- README_FOR_AGENTS not updated to M2.5 reality;
- CMake comments cleanup pending;
- Jozz sample source is monolithic;
- keyboard root controls are unclear on Polish keyboard.

### Physics gaps

- static chassis only;
- one corner only;
- no full vehicle body mass/inertia;
- no tire model beyond primitive friction;
- no steering yet;
- no left/right/front/back corner convention;
- no asset-derived physics tuning yet;
- no automated regression scenario for wheel-joint setup.

### Asset/import gaps

- no runtime glTF importer;
- no visual mesh attachment;
- no material/texture handling;
- no transform-path-safe node lookup system;
- duplicate node names unresolved in source assets;
- no stable node path IDs;
- sidecar contracts are drafts, not enforced;
- cardan shaft lacks semantic nodes.

### Tooling gaps

- asset audit tool does not validate contracts deeply;
- no CI/build workflow for the branch-specific sample;
- no automated script to verify `Jozz Vehicle` sample registration;
- no documented manual test checklist index.

## 13. Recommended immediate direction

Before adding models, run a **Foundation Grounding Phase**.

The purpose is to freeze the correct lessons from M2, clean documentation, prevent old mistakes from returning, and prepare a clean path for glTF visual work.

The next document defines that phase:

```text
docs/FOUNDATION_GROUNDING_PHASE_PLAN_PL.md
```

## 14. Final audit judgement

The project is finally past the first dangerous point: the primitive one-corner wheel-joint lab is no longer nonsense. M2.5 gives a real debug foundation for spring, limit, contact and stress testing.

But the project is not ready for full vehicle assembly yet.

It is ready for foundation grounding and then a carefully scoped visual phase:

```text
M2.5 validated primitive physics foundation
-> foundation grounding cleanup
-> asset-derived primitive dimensions
-> first visual-only glTF mesh attachment
-> visual rig for wheel/suspension/damper/cardan
-> only then full vehicle assembly
```
