# M3A Execution Plan and Critical Review — Asset-Derived Primitive Dimensions

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: execution plan before code changes

## 1. Goal

Implement the smallest useful M3A step:

```text
Keep the M2.5 primitive one-corner wheel-joint lab, but centralize its default primitive dimensions as asset-derived constants traceable to current asset audit markers.
```

This is a preparation step for future rig/import work. It must not become renderer/importer work.

## 2. Initial plan

1. Read the current M2.5 sample code.
2. Add a small primitive-defaults data structure near the Jozz sample class.
3. Move wheel radius/width defaults into named constants derived from the current asset audit.
4. Keep rest drop explicit/tuned, not auto-derived from visual sockets.
5. Keep rebound/compression values unchanged for feel, but record the asset-derived suspension total travel as a hint.
6. Add concise UI/HUD text explaining that radius/width are asset-derived and rest drop is explicit.
7. Update docs/index/agent README so future agents know M3A has started or is complete.
8. Avoid runtime JSON, renderer, glTF visual attachment, steering, four-corner vehicle, and hotkeys.

## 3. Critical review of the initial plan

### Risk 1 — Fake progress

Because current M2.5 values already match the wheel asset approximately, the code change could look cosmetic.

Countermeasure:

```text
Make traceability explicit in code and UI, not just comments.
```

M3A is valuable only if future agents can see where values came from and why rest drop is not derived.

### Risk 2 — Accidentally changing validated feel

Changing radius, width, rebound, compression, rest drop, or rig height too much could break the Jozz-validated M2.5 behavior.

Countermeasure:

```text
Keep effective defaults nearly identical:
wheel radius 0.5140625 instead of 0.52
wheel width 0.4375 instead of 0.44
travel/restdrop unchanged
```

The small radius/width difference is intentional traceability, but should be visually/physically very close.

### Risk 3 — Runtime JSON too early

Reading `assets/contracts/*.asset.json` at runtime sounds attractive, but it brings path, packaging, parsing, error and platform concerns before we have a visual importer.

Countermeasure:

```text
M3A uses centralized code constants with source comments. Runtime JSON is deferred to M3A.1 or importer work after contract validation is stronger.
```

### Risk 4 — Reintroducing M2.3 mistake

If M3A derives rest drop from `Socket_ChassisMount -> Socket_WheelCenter`, it repeats the old error: visual mount pretending to be physics frame A.

Countermeasure:

```text
Rest drop remains explicit/tuned. The code and docs must say this.
```

### Risk 5 — Version naming confusion

Renaming the whole panel to M3A before validation may confuse Jozz if the build is not yet checked locally.

Countermeasure:

```text
Use a modest panel line like “M3A prep: asset-derived primitive defaults”. Keep sample picker name unchanged. Full milestone naming can be updated after validation.
```

## 4. Final corrected plan

Implement this exact small change:

1. Add `JozzVehiclePrimitiveDefaults` near the M2 class.
2. Add a function returning M3A defaults:

```text
metersPerBlockbenchUnit = 0.35
wheelRadius = 1.46875 * 0.35 = 0.5140625
wheelWidth = 1.25 * 0.35 = 0.4375
assetTravelHint = 2.0 * 0.35 = 0.70
reboundTravel = 0.42
compressionTravel = 0.32
restDrop = 0.82
```

3. Use those defaults in `SetDefaults()`.
4. Add short HUD/panel text:

```text
asset-derived defaults: scale 0.35 m/BU, wheel r 0.51, width 0.44
rest drop remains explicit/tuned; visual sockets are not physics frame A
```

5. Do not change input/hotkeys.
6. Do not add files to CMake unless necessary.
7. Update docs to mark M3A as implemented if the code change is committed.

## 5. Manual validation checklist

After pulling locally:

```powershell
py tools\asset_audit.py
py tools\asset_contract_audit.py
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Check:

- panel still opens;
- wheel radius is about 0.51/0.52 m;
- wheel width is about 0.44 m;
- live root slider works realtime;
- Q/E live root works;
- W/S drive works;
- Space brake works;
- structural setup remains pending until Apply;
- Apply rebuild still works;
- no glTF visuals appear;
- no new hotkeys exist.

## 6. What this does not solve

M3A still does not solve:

- runtime glTF import;
- stable node path binding;
- visual mesh rendering;
- damper/cardan rigging;
- full vehicle assembly;
- steering;
- mesh collision.

That is intentional.

## 7. Final judgement

The correct M3A implementation is small, traceable and slightly boring.

If it feels like a whole new renderer/importer task, it has already gone wrong.