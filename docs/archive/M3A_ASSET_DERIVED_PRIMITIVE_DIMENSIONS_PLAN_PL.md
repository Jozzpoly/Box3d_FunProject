# M3A Plan — Asset-Derived Primitive Dimensions

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: planned next technical gate after foundation grounding

## 1. Purpose

M3A is the next small implementation gate.

Goal:

```text
Keep the M2.5 primitive one-corner wheel-joint lab, but make its default primitive dimensions traceable to current asset audit/contract data.
```

M3A is **not** a glTF renderer.  
M3A is **not** visual rigging.  
M3A is **not** a full vehicle.

It is the bridge between:

```text
hand-tuned primitive values
```

and:

```text
asset-informed primitive physics defaults
```

## 2. Why M3A comes before visual import

Jozz wants real models visible and moving in-game. That is the right direction.

But if we attach visuals before the physics dimensions are traceable, future bugs become harder to diagnose:

```text
Is the wheel wrong because the renderer transform is wrong?
Is the glTF orientation wrong?
Is the wheel joint frame wrong?
Is the primitive collider too small?
Is rest drop wrong?
```

M3A keeps the renderer out of the problem. It proves that current asset measurements can safely influence primitive physics without changing the M2.5 behavior model.

## 3. Input files

Primary inputs:

```text
assets/reports/asset_audit_latest.json
assets/reports/asset_audit_latest.md
assets/contracts/offroad_big_wheel.asset.json
assets/contracts/one_sided_wheel_mount.asset.json
samples/sample_jozz_vehicle_lab.cpp
```

Reference docs:

```text
docs/PRE_RIG_IMPORT_READINESS_AUDIT_PL.md
docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md
docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md
docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md
```

## 4. Current hard-coded M2.5 defaults

Current M2.5 code uses:

```cpp
m_wheelRadius = 0.52f;
m_wheelWidth = 0.44f;
m_chassisHalfHeight = 0.16f;
m_rigHeight = 1.70f;
m_restDrop = 0.82f;
m_reboundTravel = 0.42f;
m_compressionTravel = 0.32f;
```

These are not random. Radius and width already roughly match the current audited wheel asset at:

```text
1 Blockbench unit = 0.35 m
```

M3A should make this relationship explicit and harder to lose.

## 5. Safe asset-derived candidates

### 5.1 Wheel radius

From `Offroad_Big_Wheels.gltf` audit:

```text
Axis/Wheel center Y ≈ 0.5 BU
Marker_TireRadiusOuter Y = 1.96875 BU
```

Candidate derivation:

```text
radiusBU = 1.96875 - 0.5 = 1.46875 BU
radiusM  = 1.46875 * 0.35 = 0.5140625 m
```

Recommended M3A default:

```text
wheelRadius = 0.514 m, rounded in UI to 0.51 or 0.52
```

### 5.2 Wheel width

From `Offroad_Big_Wheels.gltf` audit:

```text
Marker_TireWidthLeft  X = -0.75 BU
Marker_TireWidthRight X =  0.5 BU
```

Candidate derivation:

```text
widthBU = abs(0.5 - (-0.75)) = 1.25 BU
widthM  = 1.25 * 0.35 = 0.4375 m
```

Recommended M3A default:

```text
wheelWidth = 0.438 m, rounded in UI to 0.44
```

### 5.3 Suspension total travel

From `One_Sided_wheel_mount.gltf` audit:

```text
Axis_SuspensionTravel_Top Y    =  1.5 BU
Axis_SuspensionTravel_Bottom Y = -0.5 BU
```

Candidate derivation:

```text
travelBU = 1.5 - (-0.5) = 2.0 BU
travelM  = 2.0 * 0.35 = 0.70 m
```

Current M2.5 default total:

```text
0.42 + 0.32 = 0.74 m
```

Recommended M3A behavior:

```text
Keep current rebound/compression defaults for feel unless a deliberate test switches to 0.70 m total.
Record the asset-derived total as a hint, not as a forced replacement.
```

Possible future candidate:

```text
reboundTravel     = 0.39 m
compressionTravel = 0.31 m
```

But do not silently change feel unless Jozz validates it.

## 6. Unsafe / not-yet-safe derivations

### 6.1 Rest drop

Do not derive `restDrop` directly from:

```text
Socket_ChassisMount -> Socket_WheelCenter
```

Why:

- `Socket_ChassisMount` is visual/authoring data;
- M2.3 failed because visual chassis mount was treated as physics frame A;
- `b3WheelJoint` needs frame A at rest wheel-center anchor, not visual mount.

Current M2.5 value:

```text
restDrop = 0.82 m
```

Recommended M3A behavior:

```text
Keep restDrop explicit/tuned.
Add documentation explaining why it is not asset-derived yet.
```

Future contract requirement:

```text
physics.restWheelCenterAnchor or physics.restDropHint must be explicit if we want asset-derived rest drop.
```

### 6.2 Chassis physics half-height

Current value:

```text
m_chassisHalfHeight = 0.16f
```

This is a debug block dimension, not a real chassis asset dimension.

Do not derive it from suspension art yet.

## 7. Recommended implementation shape

M3A should be tiny.

Minimum acceptable implementation:

1. Add a small data struct near the M2 class:

```cpp
struct JozzVehiclePrimitiveDefaults
{
    float metersPerBlockbenchUnit;
    float wheelRadius;
    float wheelWidth;
    float reboundTravel;
    float compressionTravel;
    float restDrop;
};
```

2. Initialize it from named constants with comments pointing to the audit markers.

3. Use those constants in `SetDefaults()`.

4. Do **not** read JSON at runtime yet unless the scope stays tiny and safe.

Why not runtime JSON in M3A?

Because runtime importer/JSON path handling is a separate risk. M3A is about centralizing and tracing the data first. A later M3A.1 can load the same values from contracts/audit if needed.

## 8. UI/HUD requirements

After M3A, the panel/HUD should make traceability visible, but not noisy.

Possible line:

```text
asset scale 0.35 m/BU, wheel radius 0.51 m, width 0.44 m
```

Or a small text block under structural setup:

```text
Defaults are derived from Offroad_Big_Wheels audit markers. Rest drop remains explicit/tuned.
```

Do not rename the panel to M3A unless the implementation is actually done and validated.

## 9. Validation checklist

Build:

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

1. Panel still opens and does not crash.
2. Wheel radius/width defaults are effectively unchanged from M2.5.
3. Wheel pivot remains centered.
4. W/S drive still works.
5. Space brake still works.
6. Q/E live root still works.
7. Live root slider still works without Apply.
8. Structural sliders remain pending until Apply.
9. Apply rebuild still works once.
10. No glTF visuals are rendered yet.

## 10. Hard no-go during M3A

Do not do these in M3A:

- runtime glTF mesh rendering;
- material loading;
- skinning/animation;
- full four-corner vehicle;
- steering;
- mesh collision;
- automatic restDrop from visual mount;
- importer relying on unique node names;
- new hotkeys.

## 11. Exit criteria

M3A is complete when:

- current defaults are traceable to audit/contract data;
- code still behaves like M2.5;
- docs explain what is and is not asset-derived;
- Jozz can validate the lab without seeing any new visual renderer complexity.

## 12. Final judgement

M3A should feel almost boring when implemented.

That is good. The point is to make the next visual/import phase safer by removing uncertainty around primitive wheel size and travel hints first.