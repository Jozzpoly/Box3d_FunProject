# README_FOR_AGENTS — vehicle-domain manual

> **This is not the global project front door.**
>
> Start with `AGENTS.md`, GitHub Control Issue #11, `AI_PROJECT_MEMORY.md` and the
> matching domain `CURRENT_STATE.md`. This file documents the accepted vehicle
> sandbox foundation and the rules for touching that domain.

**Owner / creative director:** Jozz — communicate with the owner in Polish.  
**Vehicle baseline branch:** `jozz-vehicle-sandbox-m0`  
**Current project campaign:** selected elsewhere; do not infer it from this file.  
**Detailed vehicle ledger:** `docs/CURRENT_STATE_INDEX_PL.md`  
**Known debt and deferred work:** `docs/TECH_DEBT_PL.md`

## 1. What the vehicle domain is

Jozz Vehicle Box3D Native is a Windows/native sandbox about building and testing
cars from user-authored Blockbench/glTF parts on honest Box3D physics.

Two principles define the domain:

1. Vehicle behaviour must emerge from construction, geometry, joints, torque,
   contact and grip. Do not fake accepted dynamics with animation or scripted
   self-alignment.
2. Box3D engine core remains upstream. `src/` and `include/` are not a normal
   project work area. Jozz-specific work belongs in samples, tools, assets and
   documentation.

Small shared sample-host hooks are acceptable only when they are additive,
opt-in and proven not to alter unrelated samples.

## 2. Accepted vehicle foundation

The current accepted foundation is the M7/M8 vehicle architecture:

- double-wishbone/trailing-arm suspension represented by physical bodies and
  joints rather than decorative rods;
- back-drivable steering rack with caster/contact-driven return;
- torque-based drive and braking;
- anti-roll bars and aerodynamic drag;
- split primitive wheel collision envelope;
- authored visual rig attached to live physical bodies;
- default suspension pose controlled by geometry and preload;
- visual dampers kept separate from the physical spring/damper authority;
- deterministic preset, session and debug-view persistence;
- native screenshot tooling and headless diagnostic hooks;
- shared world/map foundation used by vehicle labs.

Do not casually rebuild this foundation because an older milestone document
shows a simpler model.

## 3. Current vehicle samples

Use sample names, never numeric indices:

```text
Jozz Vehicle / M6 Suspension Rig Lab
Jozz Vehicle / M8 Suspension Rig Bench
Jozz Vehicle / M9 Steering Rig Bench
Jozz Vehicle / M5 First Drivable
Jozz Vehicle / Lab M2 Primitive Corner
Jozz Vehicle / Lab M1 Smoke
```

Numeric indices change when samples are added.

## 4. Non-negotiable physics rules

- Coordinate convention: `forward=+X`, `up=+Y`, `right=+Z`, left is `-Z`.
- Positive steering angle means a left turn.
- A `b3WheelJoint` spring rests at translation zero; Frame A is the rest
  wheel-centre anchor and Frame B is the wheel-centre/body origin.
- Visual sockets are not physics frames unless an explicit conversion contract
  says so.
- Structural bodies may be shapeless with explicit mass data; do not add tiny
  collision shapes merely to make them visible.
- Wheels and suspension use primitive collision, not glTF mesh collision.
- Keep physics rig, visual mesh, authored asset metadata and debug overlays
  separate.
- A stopped realistic car is not required to self-centre. The default rack has
  no artificial centring spring; caster return appears while rolling.
- Aggressive droop above the current safe range requires a steering-geometry
  redesign and owner approval.

Before rig, mount or damper work, read:

```text
docs/SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md
```

Before UI, presets or persistence work, read:

```text
docs/SUBSYSTEM_UI_PRESETS_PL.md
```

## 5. Evidence rules

### Evidence before fix

Reproduce a reported failure with a probe, dump, screenshot or controlled run
before modifying code. A plausible explanation is not evidence.

### Render is the gate

A successful build, validator and clean process exit do not prove visual work.
Any visual change requires a screenshot or equivalent render evidence that is
actually inspected.

### Read diagnostic numbers

The vehicle validator prints useful geometry and physics numbers, but some
assertions are intentionally broad. A final `OK` can coexist with obviously bad
angles. Inspect the numbers relevant to the changed mechanism.

### Preserve accepted behaviour

A refactor that changes validator output, screenshots, defaults or feel is not a
move-only refactor. Stop and classify the behaviour change.

## 6. Build and validation

Environment: Windows and PowerShell, from the repository root.

Primary gate:

```powershell
.\tools\gate.ps1
.\tools\gate.ps1 -Numbers
```

For move-only refactors with an established baseline:

```powershell
.\tools\gate.ps1 -SaveBaseline
.\tools\gate.ps1 -DiffBaseline
.\tools\gate.ps1 -DiffBaseline -Shots
```

Manual components when diagnosis requires them:

```powershell
cmake --build --preset windows-debug --target samples
cmake --build --preset windows-debug --target jozz_vehicle_validation
cmake --build --preset windows-debug --target test

.\build\bin\Debug\jozz_vehicle_validation.exe
.\build\bin\Debug\test.exe
.\build\bin\Debug\samples.exe --sample-name "Suspension Rig" --frames 300
```

Visual evidence:

```powershell
.\build\bin\Debug\samples.exe --sample-name "Suspension Rig" --frames 150 --screenshot <path>.png
.\tools\quad_shot.ps1 -Out <path>.png
```

Do not regenerate committed asset reports unless regeneration is the explicit
scope.

## 7. Branch and PR workflow

The global rules in `AGENTS.md` override older vehicle-era commit habits.

For new work:

1. identify the current authoritative branch and exact remote SHA;
2. create a new isolated branch;
3. keep one coherent scope;
4. run the relevant gate;
5. update current docs only when state moved;
6. commit in small logical units;
7. push and open a draft PR;
8. stop before merge.

Never:

- push directly to `main`;
- push directly to `jozz-vehicle-sandbox-m0` or the active campaign branch;
- force-push or rewrite branch history;
- retarget or close another PR without owner approval;
- merge without explicit owner approval;
- change `src/` or `include/` as an incidental fix;
- widen a validator threshold merely to obtain green CI.

## 8. STOP gates

Stop and ask the owner when:

- an acceptance criterion is unreachable without changing the plan;
- a physics, feel, realism-vs-arcade, UX or default-value decision appears;
- accepted code must change outside the declared scope;
- the real acceptance is an owner drive test or visual review;
- a Box3D core change appears necessary;
- a private-data boundary is involved;
- the authoritative base moved;
- CI is pending or contradictory;
- the task has grown beyond a small reviewable unit.

Do not invent substitute work after a STOP gate.

## 9. Vehicle and map status

### Vehicle

The M7/M8 foundation is stable. Known future candidates include:

- authored marker-to-hardpoint import;
- drivetrain and differential modelling;
- tire slip/load-sensitivity model;
- analog steering input;
- steering-geometry redesign for aggressive droop;
- future rig/editor tooling.

These are candidates, not automatically active tasks.

### Map

The technically working six-lane Etap 2 layout was rejected as a product layout.
The central test-campus redesign remains paused. Do not begin later map stages
from old code or old checkpoint entries without an explicit campaign decision.

Current map planning references:

```text
docs/PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md
docs/MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md
```

## 10. Documentation map

Use this order inside the vehicle domain:

1. `AI_PROJECT_MEMORY.md` — confirms whether vehicle work is active at all;
2. `docs/PROJECT_OPERATING_PLAN_PL.md` — project-wide critical path;
3. this file — accepted vehicle-domain rules;
4. `docs/CHECKPOINTS_PL.md` — recent historical handoff ledger;
5. `docs/CURRENT_STATE_INDEX_PL.md` — detailed vehicle milestone ledger;
6. `docs/TECH_DEBT_PL.md` — risks and deferred work;
7. matching `docs/SUBSYSTEM_*` documents;
8. code, tests and current render evidence.

Older milestone reports are historical context, not current task authority.

## 11. Documentation discipline

After a proven state change:

- update the matching current-state document;
- update project memory only when campaign/authority/gates moved;
- update this manual only when accepted vehicle rules changed;
- use the checkpoint ledger for a concise handoff;
- create a new long-form document only for a real milestone or architectural
  decision.

Do not commit periodic reports or unchanged gate summaries.

## 12. Current project handoff

The project-wide active campaign, exact branch/head, automation mode and next
critical gate are maintained in:

```text
AI_PROJECT_MEMORY.md
docs/PROJECT_OPERATING_PLAN_PL.md
docs/scan_import/CURRENT_STATE.md
```

Read those files before choosing any vehicle work.
