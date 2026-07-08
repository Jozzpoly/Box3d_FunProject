# M3B Plan — Metadata / Semantic Debug Import Preparation

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: planned after Jozz manual validation of M3A panel

## 1. Why this gate exists

Jozz manually confirmed that the M3A lab opens and shows:

```text
Jozz Vehicle Lab M2.5 + M3A defaults
```

That means the next safe step is not full glTF rendering yet. The next safe step is to make asset semantics visible/debuggable in the lab before mesh import and rigging.

M3B exists to answer:

```text
Can we see and reason about wheel/suspension semantic markers in-game without pretending that visual markers are already final physics frames?
```

## 2. Scope

M3B.0/M3B.1 scope:

```text
metadata/debug-first visual import preparation
```

Allowed:

- draw semantic debug points from current audited asset coordinates;
- show wheel radius/width marker relationship;
- show suspension travel marker relationship;
- label the feature clearly as debug/preview;
- keep the current primitive physics unchanged.

Not allowed:

- runtime glTF mesh rendering;
- material loading;
- texture loading;
- skinning;
- animation;
- mesh collision;
- steering;
- full vehicle assembly;
- treating visual sockets as physics frame A.

## 3. Critical design rule

Semantic debug points are **preview/debug overlays**, not authority.

They may help answer:

```text
Are marker distances plausible?
Are axis directions understandable?
Do the values match what M3A uses?
```

They must not silently drive:

```text
restDrop
wheel-joint frame A
chassis body transform
physics collision mesh
```

## 4. Initial implementation strategy

Because runtime JSON/glTF loading is still intentionally deferred, M3B.1 may start with centralized audited constants, just like M3A.

This is okay only if the code says clearly:

```text
these are audited metadata preview constants, not a runtime importer
```

The debug overlay should draw:

### Wheel audit preview

From `Offroad_Big_Wheels.gltf` audit:

```text
Socket_WheelMount       [0.25, 0.5, 0.0]
Axis_WheelSpin_A        [0.4375, 0.5, 0.0]
Axis_WheelSpin_B        [-1.0625, 0.5, 0.0]
Marker_TireRadiusOuter  [-0.125, 1.96875, 0.0]
Marker_TireWidthLeft    [-0.75, 0.5, 0.0]
Marker_TireWidthRight   [0.5, 0.5, 0.0]
```

Draw expected relationships:

- center/reference cross;
- vertical radius line;
- width line;
- spin axis line;
- wheel mount cross.

### Suspension travel preview

From `One_Sided_wheel_mount.gltf` audit:

```text
Socket_WheelCenter            [-1.1875, 0.5, -0.0625]
Axis_SuspensionTravel_Top     [-1.1875, 1.5, 0.0]
Axis_SuspensionTravel_Bottom  [-1.1875, -0.5, 0.0]
```

Draw expected relationships:

- wheel center cross;
- travel top/bottom crosses;
- travel axis line.

## 5. Placement rule

The first preview should be placed as a schematic near the active wheel corner, not snapped onto physics as if it were final visual import.

Reason:

Current model orientation is not final, and the real importer transform is not implemented yet. If we draw markers exactly on top of physics, users may assume the transform is final.

The overlay should therefore be described as:

```text
M3B semantic preview: audited marker schematic, not final visual transform
```

## 6. Validation checklist

After implementation and build:

1. Open `Jozz Vehicle / Lab M2 Primitive Corner`.
2. Confirm panel still says `M2.5 + M3A defaults` or updated M3B debug wording.
3. Confirm primitive wheel physics still works.
4. Confirm semantic debug overlay can be toggled.
5. Confirm the overlay draws a radius/width/travel schematic near the wheel.
6. Confirm no glTF mesh appears.
7. Confirm no new hotkeys were added.
8. Confirm live root and Apply behavior still work.

## 7. Critical review

This is still not real import.

That is good. The point is to gain visibility into asset semantics with minimal risk. If this debug overlay looks confusing or useless, it should be corrected before a mesh renderer exists, because it means the semantic contract is not understandable enough.

## 8. Exit criteria

M3B.0/M3B.1 is complete when:

- M3A remains working;
- semantic debug overlay exists and is clearly labeled;
- overlay is toggleable;
- overlay does not drive physics;
- docs warn that this is a schematic/preview, not final import transform.

## 9. Next after this

Only after this is validated should the project consider:

```text
M3B.2 — render one static visual wheel mesh at origin
M3B.3 — attach visual wheel mesh to primitive wheel body
```

Do not jump straight to damper/cardan/suspension rigging.