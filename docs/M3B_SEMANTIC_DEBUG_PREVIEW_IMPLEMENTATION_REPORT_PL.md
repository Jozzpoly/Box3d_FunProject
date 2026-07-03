# M3B Implementation Report — Semantic Debug Preview

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: implemented in code; local build/manual validation pending

## 1. What changed

M3B.1 has been implemented as a debug-only overlay in:

```text
samples/sample_jozz_vehicle_lab.cpp
```

The lab now has an ImGui toggle:

```text
M3B semantic preview
```

When enabled, the sample draws schematic semantic markers from the current audited asset coordinates.

## 2. What the preview draws

### Wheel audit schematic

From `Offroad_Big_Wheels.gltf` audit, the overlay draws:

```text
wheel center/reference
Socket_WheelMount
Marker_TireRadiusOuter
Marker_TireWidthLeft
Marker_TireWidthRight
Axis_WheelSpin_A
Axis_WheelSpin_B
```

It draws:

- a radius line;
- a width line;
- a spin axis line;
- debug crosses at the relevant points.

### Suspension travel schematic

From `One_Sided_wheel_mount.gltf` audit, the overlay draws:

```text
Socket_WheelCenter
Axis_SuspensionTravel_Top
Axis_SuspensionTravel_Bottom
```

It draws:

- wheel-center cross;
- top/bottom travel crosses;
- travel axis line.

## 3. Important limitation

This is not a runtime glTF importer.

This is not the final visual transform.

The code intentionally maps authoring-space coordinates into a nearby in-game schematic preview:

```text
authoring Y -> world Y
authoring X -> world Z
authoring Z -> world X
```

That mapping is only for debug readability. It should not be treated as the final Blockbench-to-game transform.

## 4. What this does not drive

The M3B semantic preview does not drive:

- wheel body transform;
- wheel collision;
- chassis transform;
- `b3WheelJoint` frame A;
- `restDrop`;
- suspension limits;
- visual mesh rendering.

This is deliberate. Visual sockets are still not physics frames.

## 5. UI/debug text

The panel now identifies the lab as:

```text
Jozz Vehicle Lab M2.5 + M3A/M3B debug
```

The HUD prints:

```text
M3B semantic preview: on/off, schematic only, no mesh import
```

## 6. What was not added

M3B.1 did not add:

- full glTF runtime loading;
- material loading;
- texture loading;
- skinning;
- animation;
- mesh rendering;
- mesh collision;
- steering;
- four-corner vehicle;
- new hotkeys;
- CMake/source split.

## 7. Critical self-review

### Good

- The overlay is toggleable.
- It makes wheel radius/width/spin markers visible as relationships.
- It makes suspension travel top/bottom visible as a relationship.
- It does not affect physics.
- It continues the low-risk M3 ladder.

### Risk

The schematic overlay may look visually separate from the real wheel and could confuse users.

Judgement:

This is intentional for now. The current authoring orientation and final visual transform are not locked. Snapping the points onto the physics wheel would create false confidence that the importer transform is solved.

### Risk

The coordinates are still code constants, not runtime-read from glTF or JSON.

Judgement:

Acceptable for M3B.1. The point of this step is in-game semantic visibility without adding runtime import risk. Runtime metadata loading can be a later M3B.2-prep task after this preview is validated.

## 8. Required validation

Run:

```powershell
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Open:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Expected panel:

```text
Jozz Vehicle Lab M2.5 + M3A/M3B debug
```

Check:

- sample opens without crash;
- primitive wheel still works;
- W/S, Space, Q/E still work;
- `M3B semantic preview` toggle exists;
- with toggle enabled, colored schematic marker lines/crosses appear near the wheel;
- disabling the toggle hides only the semantic preview;
- no glTF mesh appears;
- physics behavior does not change when toggling preview.

## 9. Recommended next step after validation

After this preview is validated, the next safe choices are:

```text
M3B.1 polish: labels/legend for semantic preview
M3B.2-prep: runtime metadata loading without mesh rendering
M3B.2: render one static visual wheel mesh at origin
```

Do not jump directly to full suspension/damper/cardan rigging.

## 10. Final judgement

M3B.1 gives the project a safe bridge between raw asset audit data and future visual import. It lets Jozz see semantic relationships in the native lab before any mesh renderer or rig code can hide mistakes behind pretty visuals.