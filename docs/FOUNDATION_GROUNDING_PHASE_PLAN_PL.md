# Foundation Grounding Phase Plan — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Input state: M2.5 primitive corner lab validated by Jozz

## 1. Purpose

The project now has a working primitive one-corner wheel-joint lab. That is a real technical milestone, but it was reached through several iterations that exposed weak points in process, documentation, hotkey ownership and architecture.

The Foundation Grounding Phase exists to make the project harder to break before adding the features Jozz cares about most:

- seeing his real models in the native game/lab;
- wheel visuals moving with physics;
- suspension/damper/cardan visual rigging;
- eventually a modular vehicle assembly sandbox.

This phase is intentionally not glamorous. It is about preventing future chaos.

## 2. Definition of done

Foundation Grounding is done when:

1. the current project state is easy for a future agent to understand;
2. superseded M2 docs cannot mislead future work;
3. M2.5 primitive corner lab remains the authoritative physics baseline;
4. asset contracts and audit reports are tied to the next implementation step;
5. current hotkeys are documented and no new shortcut can be added blindly;
6. the next phase can start with a small, clear task: asset-derived primitive dimensions or first visual-only glTF attachment;
7. there is no unresolved ambiguity around rest drop, live root, wheel center, or suspension limits.

## 3. Non-negotiable phase rules

During this phase:

- do not start full vehicle assembly;
- do not implement a full custom renderer;
- do not turn glTF mesh into physics collision;
- do not rewrite Box3D internals;
- do not delete historical docs unless replacing them with an index/status system;
- do not make large unreviewable commits;
- do not add or change keyboard shortcuts without checking sample-host/global shortcuts first;
- do not hide uncertainty.

## 4. Workstream A — Documentation grounding

### A1. Create a current-state index

Create:

```text
docs/CURRENT_STATE_INDEX_PL.md
```

It should list:

- active sample: `Jozz Vehicle / Lab M2 Primitive Corner`;
- current internal panel: `Jozz Vehicle Lab M2.5`;
- current active physics model;
- active docs;
- superseded docs;
- current assets;
- validation commands;
- next recommended task.

Why:

M2 generated many documents. Without an index, future work may accidentally follow M2.3 or M2.1 instead of M2.5.

### A2. Update README_FOR_AGENTS

`README_FOR_AGENTS.md` currently says the immediate target is a separate executable. That was the original direction, but the practical path used the existing `samples` host first.

Update it to say:

```text
Current reality:
  Jozz Vehicle is currently implemented as a sample-host lab inside `samples`.
  A separate executable may still happen later, but it is not the immediate blocker.
```

Also add:

- read `PROJECT_AUDIT_2026_07_03_PL.md`;
- read `FOUNDATION_GROUNDING_PHASE_PLAN_PL.md`;
- M2.5 is the authoritative wheel-corner baseline.

### A3. Add top-level Jozz project note

The root `README.md` is upstream Box3D. Do not heavily rewrite it yet.

Instead add:

```text
JOZZ_VEHICLE_README_PL.md
```

Recommended content:

- this branch is a Jozz Vehicle experiment on Box3D;
- current branch name;
- how to build;
- how to open the current sample;
- warning that root README is upstream Box3D documentation.

### A4. Mark superseded M2 docs

Do not delete M2.1/M2.2/M2.3 yet. They are useful historical failure records.

But each should be clearly marked:

```text
Superseded by M2.4/M2.5. Do not use this as current architecture.
```

## 5. Workstream B — Hotkey grounding

### B1. Document global sample-host hotkeys

Create:

```text
docs/HOTKEY_AUDIT_PL.md
```

Known global shortcuts from `samples/main.cpp`:

```text
Tab        show/hide UI
Esc        clear selection / close controls
Ctrl+Q     quit
Ctrl+O     open sample picker
O          single step
Shift+O    larger single step
P          pause
M          metrics
R          restart sample
[          previous sample
]          next sample
F          frame/focus
?          controls window
```

Known current Jozz sample shortcuts:

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

### B2. Rule for new hotkeys

Before adding a new hotkey:

1. check `samples/main.cpp` global shortcuts;
2. check `samples/gfx/keycodes.h` aliases;
3. check the current sample file;
4. prefer UI buttons/sliders for debug controls;
5. document the shortcut in `HOTKEY_AUDIT_PL.md` and the relevant milestone doc.

### B3. Why this matters

M2.5 initially used `[` and `]` for live root movement. That conflicted with global sample switching. It was fixed to `Q/E`.

This is now a process lesson: sample-host ownership of keys must be treated as real architecture, not incidental UI trivia.

## 6. Workstream C — Code grounding

### C1. Keep M2.5 stable

Do not rewrite the current lab until documentation is grounded.

Allowed changes:

- obvious compile fixes;
- hotkey conflict fixes;
- comments clarifying current behavior;
- small UI label improvements.

Not allowed yet:

- adding glTF rendering into the same source file;
- turning the one-corner rig into a four-corner vehicle;
- replacing wheel joint with multi-body suspension.

### C2. Plan source split before visual work

`sample_jozz_vehicle_lab.cpp` is now large enough that glTF rendering should not be dumped into it blindly.

Recommended next split before/while adding visuals:

```text
samples/sample_jozz_vehicle_lab.cpp          sample registration / high-level sample classes
samples/jozz_vehicle_corner_lab.h/.cpp       primitive corner rig state and UI
samples/jozz_vehicle_asset_dimensions.h/.cpp asset-derived primitive dimensions
```

This split should be done only if it helps clarity. Do not do a giant architecture rewrite.

### C3. Restore CMake clarity later

`samples/CMakeLists.txt` was functionally updated but earlier comments were reduced during a connector-safety workaround.

Later cleanup:

- restore or improve comments;
- keep `sample_jozz_vehicle_lab.cpp` registration obvious;
- avoid touching unrelated sample files.

## 7. Workstream D — Asset/contract grounding

### D1. Promote asset-derived dimensions as next small technical step

Before visual rendering, use existing audit/contract data to set primitive values:

- wheel radius;
- wheel width;
- suspension rest drop;
- rebound/compression defaults.

This should still use primitive physics. The point is to connect asset data to physics without introducing renderer risk.

### D2. Strengthen asset audit tool

Extend `tools/asset_audit.py` later to validate:

- duplicate names as warning/error level;
- required semantic nodes by asset type;
- axis length sanity;
- tire radius/width marker consistency;
- suspension travel top/bottom consistency;
- missing cardan semantic nodes.

### D3. Do not trust names alone

Current glTF exports have duplicate node names. Importer must use stable node index/path/parent chain and composed transforms.

This stays non-negotiable.

## 8. Workstream E — Next feature gate

After Foundation Grounding, choose exactly one next implementation gate:

### Option 1 — M3A Asset-derived primitive dimensions

Recommended first.

Goal:

```text
M2.5 primitive corner still uses primitive physics, but default wheel radius/width/rest-drop/travel values are derived from current asset audit/contracts.
```

Why:

This gives Jozz a stronger connection between his models and physics without the risk of renderer/importer bugs.

### Option 2 — M3B First visual-only wheel mesh attachment

Only after M3A or if Jozz strongly wants visuals next.

Goal:

```text
Render Offroad_Big_Wheels.gltf as visual-only mesh following the primitive wheel body.
```

Rules:

- no mesh collision;
- no model-driven physics yet;
- visual correction transform must be explicit;
- duplicate node names must not break loading.

## 9. Manual validation checklist for the foundation phase

Before leaving Foundation Grounding:

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Check:

- panel says M2.5;
- live root slider works realtime;
- Q lowers root;
- E raises root;
- [ and ] still switch samples globally, not root movement;
- W/S motor still works;
- Space brake still works;
- structural pending values do not affect live root until Apply;
- Apply commits structural setup;
- wheel axis remains centered;
- no weird chassis collision with collision OFF.

## 10. Recommended next concrete task list

1. Create `docs/CURRENT_STATE_INDEX_PL.md`.
2. Create `docs/HOTKEY_AUDIT_PL.md`.
3. Update `README_FOR_AGENTS.md` to M2.5 reality.
4. Add `JOZZ_VEHICLE_README_PL.md`.
5. Mark M2.1/M2.2/M2.3 as superseded.
6. Re-run local build validation.
7. Only then start M3A asset-derived primitive dimensions.

## 11. Final recommendation

Do not rush into glTF visuals immediately after M2.5.

The correct next move is a short but serious grounding pass. It will save time later because the project already showed how easy it is to regress when physics semantics, UI state, hotkeys and documentation are not grounded together.
