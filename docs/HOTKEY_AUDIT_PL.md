# Hotkey Audit — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: active reference for sample-host and Jozz Vehicle lab hotkeys

## Why this exists

M2.5 initially used `[` and `]` for live root movement. Jozz correctly noticed that these keys are already used by the Box3D samples program.

This document prevents that mistake from returning.

Rule:

```text
No new keyboard shortcut may be added before checking this file and `samples/main.cpp`.
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
- `E` was added as an explicit key alias in `samples/gfx/keycodes.h`.
- Live root can also be controlled by the `Live root offset` ImGui slider, so keyboard control is not mandatory.

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

## Reserved / avoid list

Avoid for Jozz Vehicle sample controls unless there is a very strong reason:

```text
Tab
Esc
Ctrl+Q
Ctrl+O
O
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
A/D      steering left/right, if not reserved by camera/sample mode
Q/E      vertical debug/root controls, currently used by M2.5
T/Y      debug toggles only if documented
V/B/N    visual/debug toggles only if documented
```

Do not treat this candidate list as permission. It is only a shortlist for review.
