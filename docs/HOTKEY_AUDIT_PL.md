# Hotkey Audit — Jozz Vehicle Box3D Native

Date: 2026-07-05  
Branch: `jozz-vehicle-sandbox-m0`  
Status: active reference for sample-host and Jozz Vehicle lab hotkeys  
Last checked against: `samples/main.cpp`, `samples/gfx/keycodes.h`, `samples/sample_jozz_vehicle_lab.cpp`, `samples/jozz_vehicle_primitive_corner_lab.cpp`, `samples/jozz_vehicle_m5_drivable_lab.cpp`

## Why this exists

M2.5 initially used `[` and `]` for live root movement. Jozz correctly noticed that these keys are already used by the Box3D samples program.

This document prevents that mistake from returning.

Rule:

```text
No new keyboard shortcut may be added before checking this file, samples/main.cpp, samples/gfx/keycodes.h, and the current Jozz sample source.
```

## Current global sample-host hotkeys

These are handled by the sample host before sample-specific controls.

```text
Tab        show/hide UI
Esc        clear selection / close controls window
Ctrl+Q     quit
Ctrl+O     open fuzzy sample picker
O          single step
Shift+O    larger single step
P          pause
M          metrics drawer
R          restart current sample
[          previous sample
]          next sample
F          frame/focus view
?          controls help window
```

Important consequence:

```text
Do not use [ or ] for Jozz Vehicle controls.
```

## Current Jozz Vehicle Lab M2.5 hotkeys

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

Notes:

- `Q` without Ctrl is usable by the sample.
- `Ctrl+Q` remains the global quit shortcut.
- `E` is present as a key alias in `samples/gfx/keycodes.h`.
- Live root can also be controlled by the `Live root offset` ImGui slider, so keyboard control is not mandatory.
- `R` is still global restart and should not be described as a Jozz-specific action even if it is useful during testing.

## Current Jozz Vehicle M5 First Drivable hotkeys

Added 2026-07-05 with the M5 sample, taken from the pre-approved candidate
list below after re-checking `samples/main.cpp` and `samples/gfx/keycodes.h`:

```text
W      drive forward
S      drive reverse
A      steer left
D      steer right
Space  brake
T      toggle third-person camera (same convention as the stock Driving sample)
```

Notes:

- `A/D/T` were on the candidate list and are not consumed by the sample host.
- The stock third-person `CharacterMover` also reads `W/A/S/D`, but only in
  character samples; it does not run in the Jozz samples.
- In third-person mode the camera follows the chassis pivot; mouse owns yaw.
- All tuning (torque, suspension, steering limits) stays in ImGui sliders per
  the control policy below.

## Current Jozz Vehicle M6 Suspension Rig Lab hotkeys (2026-07-06)

Identical to M5 First Drivable on purpose — no new keys were added:

```text
W/S    drive forward/reverse
A/D    steer left/right
Space  brake
T      toggle third-person camera
```

Everything M6-specific (rig type per axle, wishbone geometry, self-align
assist, wheel envelope, rack servo force) is ImGui-only, per the control
policy below.

### `R` restart and Debug-tab view toggles (2026-07-08 fix)

`R` is still the global sample-host restart (destroy + reconstruct the whole
sample). The constructor already restores saved vehicle tuning from
`build/jozz_vehicle_m6_session.json` (see `jozz_vehicle_m6_config_io`), but the
Debug tab's view checkboxes (rig diagnostic lines, wheel/mount 3D models,
wheel-collision-shape overlay, arm tint) were hardcoded back to fixed defaults
on every construction, independent of that config file. Effect: pressing `R`
silently re-enabled "Linie geometrii zawieszenia" (and any other Debug
checkbox) even after Jozz had turned it off — reported as "R kasuje ustawienia
debug".

Fix: those toggles now round-trip through their own auto-save,
`build/jozz_vehicle_m6_debug_session.txt` (plain `key=value` lines, not JSON —
deliberately not folded into `JozzVehicleM6Config` so they can never leak into
a named preset or get wiped by "Przywróć wszystkie ustawienia domyślne").
Loaded in the constructor right after the hardcoded defaults, saved in the
destructor alongside the vehicle config.

Also added: a "Zresetuj świat" button (Świat tab) that does a full
in-place reset — vehicle respawn, test-course props, telemetry buffers —
without going through the sample-host's destroy/reconstruct path at all. It is
the recommended way to get a clean world state; `R` remains available for
cases that need the full engine-level restart.

## Current code ownership summary

`Q/E` live root control is implemented inside `JozzVehiclePrimitiveCornerM2::Step()`.

`W/S/Space` motor/brake control is also handled by the Jozz sample step path.

The sample host still owns `[`, `]`, `Tab`, `Esc`, `Ctrl+Q`, `Ctrl+O`, `O`, `Shift+O`, `P`, `M`, `R`, `F`, and `?`.

This means future Jozz sample controls should prefer ImGui first unless the control genuinely needs a held key for realtime stress testing.

## Preferred future control policy

For debug lab features:

1. Prefer ImGui sliders/buttons for controls that may conflict with global host keys.
2. Use keyboard only for realtime stress actions where holding a key matters.
3. Before adding a key, check:
   - `samples/main.cpp` global handler;
   - `samples/gfx/keycodes.h` aliases;
   - current Jozz sample code;
   - this document.
4. Update this document in the same commit as any hotkey change.
5. When possible, document the control in the relevant milestone doc and in `docs/CURRENT_STATE_INDEX_PL.md`.

## Reserved / avoid list

Avoid for Jozz Vehicle sample controls unless there is a very strong reason:

```text
Tab
Esc
Ctrl+Q
Ctrl+O
O
Shift+O
P
M
R
[
]
F
?
```

Also avoid overloading `W/S/Space` unless it is clearly vehicle driving/braking behavior.

## Candidate future keys

Possible future keys after checking conflicts:

```text
A/D      steering left/right — TAKEN by M5 First Drivable (2026-07-05)
Q/E      vertical debug/root controls, currently used by M2.5
T        third-person toggle — TAKEN by M5 First Drivable (2026-07-05)
Y        debug toggles only if documented
V/B/N    visual/debug toggles only if documented
```

Do not treat this candidate list as permission. It is only a shortlist for review.

## Foundation grounding verdict

The current audit is consistent with M2.5:

- live root uses `Q/E`, not `[ ]`;
- `[ ]` remain reserved for global sample switching;
- the panel/HUD should describe Q/E root movement;
- new hotkeys are blocked until this audit is updated again.