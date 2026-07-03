# M3A Implementation Report — Asset-Derived Primitive Defaults

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: implemented in code and manually validated by Jozz; local agent build not run

## 1. What changed

M3A has been implemented as a small, controlled code step in:

```text
samples/sample_jozz_vehicle_lab.cpp
```

The implementation adds a centralized primitive-defaults struct and a single defaults function:

```cpp
struct JozzVehiclePrimitiveDefaults
static JozzVehiclePrimitiveDefaults GetJozzVehicleM3ADefaults()
```

These defaults are traced to current asset audit/contract data, but are kept as code constants for now. Runtime JSON loading is deliberately not introduced in M3A.

## 2. Asset-derived defaults now centralized

Current values:

```text
metersPerBlockbenchUnit = 0.35
wheelRadius             = 1.46875 * 0.35 = 0.5140625 m
wheelWidth              = 1.25 * 0.35 = 0.4375 m
assetSuspensionTravelHint = 2.0 * 0.35 = 0.70 m
```

The wheel values come from `Offroad_Big_Wheels.gltf` audit markers:

```text
Marker_TireRadiusOuter
Marker_TireWidthLeft
Marker_TireWidthRight
```

The travel hint comes from `One_Sided_wheel_mount.gltf` travel markers:

```text
Axis_SuspensionTravel_Top
Axis_SuspensionTravel_Bottom
```

## 3. What intentionally did not change

The following stay intentionally close to Jozz-validated M2.5 behavior:

```text
reboundTravel     = 0.42 m
compressionTravel = 0.32 m
chassisHalfHeight = 0.16 m
rigHeight         = 1.70 m
restDrop          = 0.82 m
```

Important:

```text
restDrop remains explicit/tuned.
```

It is not derived from:

```text
Socket_ChassisMount -> Socket_WheelCenter
```

because visual sockets are not physics frame A.

## 4. UI/debug changes

M3A introduced the panel identity:

```text
Jozz Vehicle Lab M2.5 + M3A defaults
```

It displayed:

```text
asset defaults: scale 0.35 m/BU, wheel r 0.51, width 0.44
asset travel hint 0.70 m; rest drop 0.82 m is explicit/tuned
```

The debug text also printed the M3A asset defaults.

Note: after M3B.1, the panel text may include M3B debug wording too, but the M3A default values remain the same.

## 5. What was not added

M3A did not add:

- glTF runtime rendering;
- material loading;
- skinning;
- animation;
- runtime JSON parsing;
- full importer;
- mesh collision;
- steering;
- four-corner vehicle;
- new hotkeys;
- CMake/source split.

This is intentional.

## 6. Jozz manual validation

Jozz reported that the sample opens, runs, and shows:

```text
Jozz Vehicle Lab M2.5 + M3A defaults
```

Jozz also confirmed:

```text
for me everything works
```

Manual validation status:

```text
accepted by Jozz for continuing into M3B planning/implementation
```

The agent did not run a local compiler/build. The manual validation came from Jozz's local build/run.

## 7. Critical self-review

### Good

- The change is small.
- The M2.5 rest-anchor model is untouched.
- Radius/width are now traceable to real asset audit values.
- Travel from the suspension asset is recorded as a hint, not forced into physics.
- The UI tells Jozz what is asset-derived and what is still tuned.
- Jozz locally confirmed that the lab opens and works.

### Risk

- The default radius changed from `0.52` to `0.5140625` and width from `0.44` to `0.4375`.

Judgement:

This is acceptable because it is a tiny numerical change and moves the values from rounded hand-tuned approximations to exact audit-derived constants. Jozz's manual run did not report a visible problem.

## 8. Required local validation for future agents

Future agents should still run from repo root:

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

Expected panel after M3B.1 may now be:

```text
Jozz Vehicle Lab M2.5 + M3A/M3B debug
```

Manual checks:

- no crash on sample open;
- wheel radius/width look effectively unchanged;
- W/S drive works;
- Space brake works;
- Q/E live root works;
- live root slider works realtime;
- structural sliders remain pending until Apply;
- Apply rebuild works once;
- reset restores M3A defaults;
- no glTF mesh is rendered.

## 9. Recommended next step

M3A is now good enough to move into M3B.0/M3B.1:

```text
metadata/debug-first visual import preparation
```

Specifically:

```text
M3B.0 read/validate metadata only, no rendering
M3B.1 draw semantic debug points from audited positions, no mesh rendering
```

Only after that should visual wheel mesh attachment begin.

## 10. Final judgement

M3A now does what it should: it connects Jozz's asset measurements to primitive physics defaults without opening the renderer/importer problem yet.

The project is better prepared for rig/import work, but still should move through the M3B ladder carefully.