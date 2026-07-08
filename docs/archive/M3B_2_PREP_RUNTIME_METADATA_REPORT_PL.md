# M3B.2-prep Report — Runtime Asset Audit Metadata, No Mesh Rendering

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: implemented in code; local build/manual validation pending

## 1. Purpose

This step moves the project closer to the real goal: importing and rigging Jozz's models.

It does **not** render glTF meshes yet. It introduces a runtime metadata path first.

Goal:

```text
Use runtime-loaded asset audit metadata to drive M3A primitive defaults and M3B semantic preview, while keeping a safe built-in fallback.
```

This is a concrete bridge between:

```text
hard-coded audited constants
```

and:

```text
future runtime glTF/asset contract import
```

## 2. What changed

Added files:

```text
samples/jozz_vehicle_asset_metadata.h
samples/jozz_vehicle_asset_metadata.cpp
```

Updated build list:

```text
samples/CMakeLists.txt
```

Updated lab:

```text
samples/sample_jozz_vehicle_lab.cpp
```

## 3. Runtime metadata loader

The new loader tries to read:

```text
assets/reports/asset_audit_latest.json
```

from several relative paths, because `samples.exe` may be launched from different build working directories.

If the report is found and parsed, metadata status becomes:

```text
loaded runtime asset audit report
```

If the report is not found or parsing fails, the loader falls back to built-in audited metadata matching the current report values.

Fallback status:

```text
using built-in audited fallback; runtime asset audit report not found
```

This is important: the lab must not crash or become unusable just because runtime asset files are not found.

## 4. What is parsed

The loader intentionally parses only the small piece needed for M3B.2-prep:

```text
file
semantic_nodes[].name
semantic_nodes[].world_position
```

It does not parse:

- meshes;
- materials;
- skins;
- animations;
- glTF buffers;
- node hierarchy;
- transforms beyond the already-composed audit report positions.

## 5. What uses runtime metadata now

### M3A defaults

`sample_jozz_vehicle_lab.cpp` now derives:

```text
wheel radius
wheel width
asset suspension travel hint
```

from `JozzVehicleAuditMetadata`.

If runtime report loading succeeds, those values come from the report. If not, they come from fallback audited constants.

### M3B semantic preview

The wheel and suspension debug preview now uses semantic points from `JozzVehicleAuditMetadata`:

```text
Offroad_Big_Wheels.gltf / Socket_WheelMount
Offroad_Big_Wheels.gltf / Marker_TireRadiusOuter
Offroad_Big_Wheels.gltf / Marker_TireWidthLeft
Offroad_Big_Wheels.gltf / Marker_TireWidthRight
Offroad_Big_Wheels.gltf / Axis_WheelSpin_A/B
One_Sided_wheel_mount.gltf / Socket_WheelCenter
One_Sided_wheel_mount.gltf / Axis_SuspensionTravel_Top/Bottom
```

## 6. UI/HUD changes

The panel now reports metadata status, for example:

```text
metadata: loaded runtime asset audit report
source: ../../assets/reports/asset_audit_latest.json
```

or:

```text
metadata: using built-in audited fallback; runtime asset audit report not found
```

The HUD also reports:

```text
M3B metadata: runtime audit
```

or:

```text
M3B metadata: built-in fallback
```

The old `Reset M3A defaults` button is now:

```text
Reload metadata + reset defaults
```

## 7. What was not added

This step did not add:

- glTF mesh rendering;
- material/texture loading;
- skinning;
- animation;
- mesh collision;
- steering;
- full vehicle assembly;
- new hotkeys.

## 8. Critical self-review

### Good

- This is a real step toward import without jumping to mesh rendering.
- The lab can now prove whether runtime asset metadata paths are reachable.
- The fallback keeps the lab robust if the executable is launched from an unexpected working directory.
- M3A primitive dimensions and M3B preview now share one metadata source.

### Risk

The loader currently reads the audit report, not the raw glTF or contract JSON.

Judgement:

Acceptable for this step. The audit report already contains composed semantic positions. Raw glTF import would bring much more risk and should come later.

### Risk

The relative-path search may not find the report depending on launch location.

Judgement:

Acceptable because the UI explicitly reports runtime vs fallback. This is exactly what this gate is meant to discover.

### Risk

The JSON parser is intentionally tiny and only supports the known audit-report shape.

Judgement:

Acceptable. This is not a general JSON system. It is a focused bridge toward the future importer.

## 9. Required validation

Run:

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

1. Build succeeds.
2. Sample opens.
3. Panel still shows `Jozz Vehicle Lab M2.5 + M3A/M3B debug`.
4. Panel shows a metadata status line.
5. HUD shows `M3B metadata: runtime audit` or `built-in fallback`.
6. Wheel radius remains about `0.51`.
7. Wheel width remains about `0.44`.
8. Semantic preview still draws.
9. Toggling semantic preview still does not affect physics.
10. No glTF mesh appears.
11. `Reload metadata + reset defaults` does not crash.

## 10. What result is acceptable?

Both outcomes are useful:

```text
runtime audit
```

means the executable found and parsed the report.

```text
built-in fallback
```

means the launch working directory did not find the report, but the lab remained safe. In that case, the next step should improve path handling or copy/report staging, not start mesh rendering.

## 11. Recommended next step

After validation, choose based on result:

### If runtime audit loads

Proceed to:

```text
M3B.2 — static wheel visual mesh at origin, still not attached to physics
```

### If fallback is used

Do not start mesh rendering yet. First fix/report asset path discovery:

```text
M3B.2-prep-path — reliable runtime asset/report path resolution
```

## 12. Final judgement

This is a concrete step toward the goal. It starts the runtime asset-data path while preserving the project's rule: metadata and visuals may inform/debug, but they must not silently become physics authority.