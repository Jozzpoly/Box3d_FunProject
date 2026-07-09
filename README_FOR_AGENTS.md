# README_FOR_AGENTS — Jozz Vehicle Box3D Native

**This is the single front door. Read this fully before touching anything.**
It is kept short and current on purpose. Everything else in `docs/` is either a
milestone report (history) or a deep-dive reference — see §9 for which is which.

- **Date of this handoff:** 2026-07-08 (state: M8)
- **Owner / creative director:** Jozz (respond to Jozz in **Polish**)
- **Working branch:** `jozz-vehicle-sandbox-m0` (`main` = upstream box3d)
- **Detailed milestone ledger:** `docs/CURRENT_STATE_INDEX_PL.md`
- **Known debt & risks:** `docs/TECH_DEBT_PL.md` ← read before "cleaning up"

---

## 1. What this project is

**Jozz Vehicle Box3D Native** — a Windows/native sandbox+game about *building
cars from user-authored Blockbench (glTF) parts*, grown as a **non-invasive
overlay on the box3d physics engine**.

Two rules define the whole thing:

- **Direction = BeamNG.drive.** Vehicle behaviour must **emerge from the
  construction** (springs, arms, geometry, torque vs grip), never be scripted or
  animated. If a behaviour is faked, it will be rejected — this has already
  happened (the old software "self-align assist" was removed in M7 for reading
  as scripted).
- **The engine core is untouched.** `src/` and `include/` are box3d and stay
  box3d. All Jozz work lives in `samples/jozz_vehicle_*`, `tools/`, `assets/`,
  `docs/`. If you think you need to change engine internals, you are almost
  certainly wrong — stop and ask Jozz.
  - Narrow exception, used sparingly: the shared sample host
    (`samples/sample.h/.cpp`, `samples/host/gui.cpp`, `samples/CMakeLists.txt`)
    may get small, **purely additive, opt-in-by-default-false** hooks when a
    Jozz lab genuinely needs host behavior no per-sample override already
    covers — e.g. the Polish-font fix in `gui.cpp` (M8.x), `/utf-8` in
    `CMakeLists.txt`, and `Sample::CondenseDebugOverlay()` (2026-07-08: lets a
    tuning-dense lab fold the info panel's frame-time/camera block behind a
    closed header; every other sample's default is unchanged — verified by
    screenshotting a stock sample before shipping it). Any such change must be
    verified to leave every other sample's behavior byte-for-byte identical.

---

## 2. Current state (M8, accepted vs experimental)

The vehicle runs inside the box3d `samples` host. **Active samples** (open by
name, indices shift — see §6):

```text
Jozz Vehicle / M6 Suspension Rig Lab   <- THE main drivable car. Multi-body
                                          double-wishbone corners on the M7 real-
                                          forces foundation, full Polish tuning UI,
                                          Jozz's One_Sided_wheel_mount model rigged
                                          onto the live bodies, telescoping damper.
Jozz Vehicle / M8 Suspension Rig Bench <- isolated 1-corner spring bench (posable)
Jozz Vehicle / M9 Steering Rig Bench   <- isolated 2-corner (L/R) bench for the NEW
                                          OneSided_Steering_Suspension_Rig model
                                          (steer + travel DOF, no vehicle yet - see
                                          §7's "not yet integrated" note and
                                          docs/CHECKPOINTS_PL.md 2026-07-09).
Jozz Vehicle / M5 First Drivable       <- strut baseline (kept as reference)
Jozz Vehicle / Lab M2 Primitive Corner <- isolated corner lab (kept)
Jozz Vehicle / Lab M1 Smoke            <- oldest smoke sample (kept)
```

**Accepted / stable foundation** (do not casually rework):
- **M7 real-forces physics** (`jozz_vehicle_m6_suspension_rig.cpp/.h`): arms are
  BODIES on hinges with angle limits (not distance-joint rods — those had a
  mirrored solution branch that snapped on hard landings); back-drivable steering
  rack (caster/contact forces do the counter-steer, no script); torque-based
  drive; anti-roll bars; aero drag; split wheel collision envelope. The rack is
  hands-off spring-free by design: the wheels self-center **only while rolling**
  (caster trail), and stay put at a standstill — this is correct (a stopped car
  doesn't self-center), not a bug. An **opt-in** `rackCenteringHertz` slider
  (default 0 = off, "Wspomaganie powrotu (arcade)") adds a rest-centering spring
  for players who want it; it's a sibling of `uprightAssist`, not the default.
  (History note: the "steering jam" once tracked in TECH_DEBT #9 was proven to
  be this rest-state no-centering, misdiagnosed by a probe that demanded
  self-centering on a stationary car — now closed.)
- **M8 rig + pose foundation**: the mount model is rigged per-bone onto the live
  bodies; suspension **default pose is a deliberate setting** (`restArmDroopDeg`
  geometry + `suspensionPreload` spring preload) so arms droop to the wheels
  instead of folding up; bump-steer compensation keeps the steering geometry
  correct through droop. ⚠ The **visual dampers are decorative and decoupled
  from the physics spring** — before any rig/damper/mount work read
  `docs/SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md`.
- **Preset + session system** (`jozz_vehicle_m6_config_io.cpp/.h`): whole-vehicle
  configs save/load as JSON. `assets/vehicle_presets/*.json` (committed:
  `uliczny`/`drift`/`offroad`); `build/jozz_vehicle_m6_session.json` (gitignored
  auto-save) means restarting the sample resumes tuning instead of wiping it.
  Debug-tab view toggles (rig diagnostic lines, wheel/mount visuals, arm tint)
  are a SEPARATE auto-save, `build/jozz_vehicle_m6_debug_session.txt` — they are
  view state, not vehicle tuning, so they must never leak into a preset or get
  wiped by "restore defaults", but they still need to survive the "R" restart
  the same way tuning does. A "Zresetuj świat" button (Świat tab) does a full
  in-place simulation reset (vehicle respawn + props + telemetry) without going
  through the engine's global restart at all, for anyone who wants a clean
  world without touching the keyboard.
- **Screenshot tooling** (`samples/host/screenshot.cpp`, `--screenshot`): D3D11
  backbuffer → PNG. This is how you SEE your own visual work headless.
- **Dual damper visual, socket-driven** (2026-07-08): the telescoping shock
  mesh (`Asset_Dumper.gltf`) is drawn TWICE per corner, pinned to the model's
  own `Socket_DamperUpper_L/R` and `Socket_DamperLower_L/R` markers from the
  `one_sided_wheel_mount.asset.json` contract - not hand-guessed offsets. `_L`/
  `_R` differ only in Z (two shocks straddling the arm), so both get the same
  X-mirror treatment as `Socket_WheelCenter` for right-side corners. Upper
  rides `bracketWorld` (chassis-relative), lower rides `hubWorld` (knuckle-
  relative) - the exact same per-corner transforms that already pin the
  Chassis_Top/Chassis_Bottom arms, so the shocks stay glued to the live rig.
  Visual-only (`physicsAuthority: false` in the contract); the real coilover
  spring/damper stays the existing distance-joint physics, untouched.

**Experimental / in-flight / not yet done:**
- Aggressive droop **> ~16°** is unstable (Ackermann over-centre) — 15° is the
  measured safe ceiling. Full screen-2-level droop needs a steering-geometry
  redesign (deferred, see TECH_DEBT).
- Soft-tire deformation, differentials/drivetrain, tire slip-curve model,
  markers→hardpoints import — all still deferred (roadmap in §8).

**Awaiting Jozz's manual drive test** — the M7/M8 physics passes machine
validation but Jozz's feel check on several items is still pending.

---

## 3. How to build, test, and SEE it

Environment: **Windows, PowerShell** (the Bash tool here sometimes has a broken
PATH; prefer PowerShell for cmake). Run from the **repo root**. Note: the
PowerShell CWD can drift if a Bash `cd` ran earlier — `Set-Location` to the repo
root explicitly if a build complains it can't read presets.

```powershell
# Kill a running sample first (it locks samples.exe and the build fails on LNK1168)
Get-Process samples -ErrorAction SilentlyContinue | Stop-Process -Force

cmake --build --preset windows-debug --target samples
cmake --build --preset windows-debug --target jozz_vehicle_validation
cmake --build --preset windows-debug --target test

.\build\bin\Debug\jozz_vehicle_validation.exe   # MUST end "jozz_vehicle_validation: OK"
.\build\bin\Debug\test.exe                       # engine suite, ~11 s, "All Box3D tests passed!"
.\build\bin\Debug\samples.exe --sample-name "Suspension Rig" --frames 300   # boot smoke, 0 sokol errors
```

**Seeing visual work (mandatory for any visual change — "render is the gate"):**
```powershell
# One framed screenshot of the running lab (last frame, incl. the ImGui panel):
.\build\bin\Debug\samples.exe --sample-name "Suspension Rig" --frames 150 --screenshot <path>.png
# Then READ the PNG. Four sides stitched into one image:
.\tools\quad_shot.ps1 -Out <path>.png
```
The lab reads env vars to pose state headless without UI clicks: `JOZZ_M6_CAM`
("yaw,pitch,radius,px,py,pz"), `JOZZ_M6_DIAG`, `JOZZ_M6_WHEEL`, `JOZZ_M6_DUMPER`,
`JOZZ_M6_MOUNT`, `JOZZ_M6_TAB` (0-5 forces a UI tab open), `JOZZ_M6_PRESET`,
`JOZZ_M6_HERTZ`/`DAMP`/`PRELOAD`/`DROOP`, `JOZZ_M6_DUMP` (prints corner geometry
numbers). *(Full/authoritative list of env hooks + the throwaway ones: TECH_DEBT.)*

**⚠ The validator asserts loosely.** It PRINTS diagnostic numbers (e.g. steering
angle, camber) but many asserts only check "is finite" or a wide threshold. A
badly broken geometry can print a clearly wrong number and still say `OK`. On any
geometry change, **read the printed numbers**, don't trust the final `OK` alone.
(Found exactly this way: droop 20° printed 69° steering vs a 32° limit and still
passed.)

---

## 4. Non-negotiable rules (physics + workflow)

**Physics / architecture (timeless, verified the hard way):**
- Direction convention: `forward=+X, up=+Y, right=+Z, LEFT=-Z`; **positive
  steering angle = LEFT turn**. Asserted signed in the validator. Do not re-derive
  casually (two wrong guesses already happened).
- `b3WheelJoint` spring rest = translation 0; Frame A = rest wheel-center anchor;
  restDrop explicit. **Visual sockets are NOT physics frames** without explicit
  conversion.
- `b3DefaultShapeDef()` sets `categoryBits` = **ALL bits** (unlike Box2D's single
  bit). The split wheel envelope needs both sides tagged: drivable surfaces
  `JOZZ_M6_TERRAIN_CATEGORY` (0x2), props `JOZZ_M6_OBJECT_CATEGORY` (0x1).
- Structural bodies (knuckle, rack, arms) are **shapeless** with explicit
  `b3Body_SetMassData`. A small shape at driving speed makes the body "fast" →
  continuous collision vs the ground → debug TOI assert. Vehicle worlds also run
  `b3World_EnableContinuous(false)` for the same reason.
- No glTF **mesh collision** for wheels/suspension. Wheels = primitive envelope.
- Keep separated: physics rig · visual/rigged mesh · authoring asset data · debug
  overlays. Do not merge visual marker positions into joint frames.

**Workflow (this is what keeps multi-agent work sane):**
- **Render is the gate.** Never report visual work "done" without reading a
  screenshot of it. Green tests + "0 sokol errors" say nothing about whether the
  image is correct.
- **Evidence before fix.** Reproduce a reported bug (headless probe / dump /
  screenshot) *before* changing code, so you fix the real cause.
- **Prefer numbers over pixels for geometry.** `JOZZ_M6_DUMP=1` prints exact
  corner coordinates; that is how the L/R asymmetry was proven to be a render bug,
  not physics.
- **Commit discipline (2026-07-08, Jozz's standing rule):** agents commit
  and push **autonomously** to `jozz-vehicle-sandbox-m0` whenever the quality
  gate (build + validator + `test.exe`) is green — do not wait to be asked per
  commit. Keep commits small and self-describing; group a logical unit of
  work into one commit, not one commit per file. **`main` is Jozz-only** — he
  updates it himself at real milestones. Agents never push to `main`, never
  force-push, never rewrite history. See §5 for keeping this cheap in tokens.
- **Doc discipline:** after a real change, add a ≤5-line entry to
  `docs/CHECKPOINTS_PL.md` (co/czemu/efekt/dalej — the standing handoff
  mechanism) and update this file's §2 if the state moved. Do NOT add a new
  `docs/*.md` per tiny change — the doc pile is already too big (§9); a new
  file is only for a genuinely new subsystem or a full milestone report.

---

## 5. Token economy — keep chat lean, repo authoritative

Rules Jozz set 2026-07-08 to cut token spend without cutting rigor. The
quality gate (build + validator + `test.exe`) and doc/commit discipline stay
**mandatory** — this section changes *how much of the process gets narrated
in chat*, not what gets done.

- **Never paste raw tool output.** Summarize builds/tests/`git status`/diffs
  in one line ("Build: 3/3 targets OK", "Testy: 11/11 PASS (11.2s)"). Paste
  the actual failing fragment only when something failed and you need it for
  diagnosis. Still scan full output for `warning` even on success — a silent
  new compiler warning is exactly the kind of thing this project has been
  bitten by before (§3's "validator asserts loosely" is the same failure
  mode: don't let a green summary hide a bad detail). Screenshots are
  evidence, not log spam — "render is the gate" (§3) is untouched, always
  show and read the PNG.
- **Quiet flags where they exist:** `git push -q`, `git fetch -q`,
  `git diff --stat` instead of a full diff, `git log --oneline -5` instead of
  full history. Full `git diff` only when actually reviewing a specific hunk.
  Caveat: `-q` suppresses *progress* output, not error text — a failed command
  still prints its error and still needs full, un-quieted output for
  diagnosis.
- **Grep/offset before full Read.** The three largest files
  (`jozz_vehicle_m6_rig_lab.cpp` ~1600L, `jozz_vehicle_visual_mesh.cpp`
  ~1900L, `jozz_vehicle_m6_suspension_rig.cpp` ~1500L — see TECH_DEBT #7) are
  exactly where a targeted Grep beats reading the whole file. Don't re-read a
  file already seen this session unless something could plausibly have
  changed it (your own edit already invalidates the "unread" assumption
  automatically; a fresh session, a compaction, or work landing on the shared
  branch from another agent does not — `git fetch -q && git status` before
  trusting a stale read of shared-branch state).
- **Batch independent steps into one call.** Build + validator + test + status
  is one chained command, not four narrated ones. This never overrides
  safety: `git status` still runs before any destructive git command, and the
  full gate still runs before every commit — batching cuts round-trips and
  narration, not verification.
- **Gate reporting is one line**, e.g. "Bramka: build 3/3 OK, walidator OK,
  testy 11/11 PASS (11.2s) — commituję." Full transcript only on failure.
- **One milestone per session where practical.** Detailed step-by-step
  narrative belongs in the repo's milestone `docs/*.md`, not the chat log —
  chat gets a 2-3 sentence close-out + the commit list. An agent can't force a
  new session, but should say so and suggest `/compact` or a fresh session
  once a milestone's gate is green and pushed, or once a session is carrying
  a long tool-call history.
- **What this does NOT shrink:** the explanation Jozz actually asked for in
  §10 ("what changed, why, consequences — in Polish, plainly") stays full.
  Token economy trims raw tool transcripts, not reasoning or communication.

---

## 6. Sample indices are fragile — use names

`--sample <N>` numbers = registration order and **shift when samples are added**.
Jozz samples register last (after all box3d demos). Prefer `--sample-name
"<substr>"` (e.g. `"Suspension Rig"`, `"Rig Bench"`) which is index-independent.
Registration order today: Lab M1 Smoke · Lab M2 Primitive Corner · M5 First
Drivable · M6 Suspension Rig Lab · M8 Suspension Rig Bench · M9 Steering Rig
Bench.

Do not use `[` / `]` (global sample-switch keys). Adding a hotkey → update
`docs/HOTKEY_AUDIT_PL.md`, `main.cpp`, `gfx/keycodes.h`, and each lab.

---

## 7. Do NOT do without Jozz's explicit go-ahead

- Change box3d engine internals (`src/`, `include/`).
- Rework the M7 real-forces model or the M8 pose foundation (they are accepted).
- Redesign the steering geometry / Ackermann to push droop past 16°
  (big physics change; Jozz chose this direction but it's a careful multi-step job).
- Add a full glTF renderer / material / skin / animation importer.
- Add mesh collision for wheels or suspension.
- Regenerate `assets/reports/*` (`py tools\asset_audit.py`) unless intentional.
- Push to `main`, force-push, or rewrite history anywhere. (Routine commits +
  pushes to `jozz-vehicle-sandbox-m0` behind a green gate do NOT need
  go-ahead — see §4/§5.)
- Rename/move the active samples or the tuning UI layout (Jozz just approved it).

---

## 8. Priorities / roadmap (next gates)

Nothing here is started; confirm with Jozz before picking one up:

```text
M7.2  wishbone hardpoints filled from asset markers (import fills the struct)
M7.3  drivetrain: differentials, torque split, engine-braking curve
M7.4  tire model (slip curve, load sensitivity) — the soft-tire roadmap
M7.5  analog steering input + soft hands-on/off transition
--    two side dampers off to the side (Jozz asked, deferred)
--    steering-geometry redesign to allow aggressive droop >16° (deferred)
```

---

## 9. Documentation map (what to trust)

The `docs/` folder has ~40 files. Most are **historical milestone reports** —
useful as history, **not** as current architecture. Trust this order:

**Current / authoritative:**
- `README_FOR_AGENTS.md` (this file) — front door.
- `docs/CHECKPOINTS_PL.md` — handoff ledger (co/czemu/efekt/dalej, newest first).
  **Read this first for "what happened recently".**
- `docs/CURRENT_STATE_INDEX_PL.md` — milestone ledger + validated state.
- `docs/TECH_DEBT_PL.md` — known debt, risks, deferred work.
- `docs/SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` — **read before touching the visual
  rig / dampers / mounts.** Records that the visual dampers are decorative and
  decoupled from the physics spring, what's solid vs temporary, and a staged
  polish plan.
- `docs/SUBSYSTEM_UI_PRESETS_PL.md` — the tuning UI/session/preset system: the
  three separate save files and why, tab order, the `###stable-suffix` tab-ID
  pattern (copy it for any future dynamic tab title).
- `docs/PLAN_STABILNOSC_PROWADZENIE_PL.md` — **the active work plan (execute
  this).** Step-by-step P1–P6 stages written for cheap/weak agents: exact
  files, functions, expected numbers, gate checklists, STOP conditions.
  Order: P2 → P1 → P3 → P4 → P5 → P6, one stage per session.
- `docs/AUDIT_PHYSICS_STEERING_2026_07_08_PL.md` — the findings behind that
  plan: broken-steering mechanism (tie-rod over-center vs the ±70° twist
  fence), missing RecomputeRackTravel on Apply, preload/stiffness coupling,
  full slider audit. Read for the WHY; the plan doc has the HOW.
- `docs/M7_REAL_FORCES_FOUNDATION_PL.md` — the current physics model.
- `docs/M8_SUSPENSION_RIG_REPAIR_PLAN_PL.md` — rig/pose/droop work (physics side,
  history through 2026-07-07; damper/mount current state now in the subsystem doc).
- `docs/SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md`, `docs/adr/000{1,2,3}` — conventions.

**History only, moved to `docs/archive/`** (2026-07-08, `git mv` — history
preserved): all `M0_`…`M5_*`, `CODEX_HANDOFF_*`, `CODEX_START_*`,
`PROJECT_AUDIT_*`, `PROJECT_STABILIZATION_*`, `FOUNDATION_GROUNDING_*`,
`IMPLEMENTATION_START_*`, `PRE_RIG_IMPORT_*` files. They describe superseded
states (e.g. the primitive-corner-lab-as-main-thing era). `docs/
M6_SUSPENSION_RIG_FOUNDATION_PL.md` is also history (superseded by M7/M8) but
stays in `docs/` root for now — it is still occasionally cross-referenced.

Also at repo root: `README.md` (upstream box3d), `CONTRIBUTING.md` (upstream),
`JOZZ_VEHICLE_README_PL.md` (older Polish overview — verify before trusting).

---

## 10. Where a new agent should start

1. Read this file + `docs/TECH_DEBT_PL.md` end to end.
2. Build the three targets and run the validator + `test.exe` (§3). If they are
   not green on a clean checkout, that is your first task — nothing else.
3. Take one screenshot of the M6 lab (§3) so you have working eyes.
4. Only then pick up work — from Jozz's request, or §8, confirmed with Jozz.
5. When you touch code: reproduce → change → re-validate → **screenshot if
   visual** → update §2 / the ledger. Report to Jozz *what* changed, *why*, and
   the *consequences* — in Polish, plainly, no hedging.
