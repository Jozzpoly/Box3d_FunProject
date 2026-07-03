# Box3D Joint Samples Study — Wheel / Driving

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: foundation reference, no implementation changes

## 1. Purpose

Jozz found two useful upstream Box3D samples in the runtime sample picker:

```text
Joints / Wheel
Joints / Driving
```

Both live in:

```text
samples/sample_joint.cpp
```

This document records what we can safely learn from them for Jozz Vehicle Box3D Native, without copying them blindly and without jumping ahead into M3/full vehicle assembly.

## 2. Executive judgement

Both samples are useful references, but neither should be treated as the target architecture for Jozz Vehicle.

The most important finding is positive:

```text
Joints/Wheel and Joints/Driving both support the M2.4/M2.5 rest-anchor model.
```

In both samples, the wheel joint body-A frame is placed at the wheel rest center relative to the chassis/ground body, and body-B frame is placed at the wheel body origin.

That matches our current rule:

```text
Frame A = rest wheel-center anchor on chassis/root
Frame B = wheel center / wheel body origin
b3WheelJoint spring rest = translation 0
```

So the current M2.5 baseline is not a weird custom invention. It is aligned with the direction shown by Box3D's own samples.

## 3. `Joints / Wheel`

### 3.1 What it is

`Joints / Wheel` is the smallest focused `b3WheelJoint` API demo.

It contains one dynamic wheel body connected to a static/ground body. It exposes:

- suspension limit;
- spin motor;
- suspension spring;
- steering;
- steering limit;
- steering angle readout.

It is useful as an API reference, not as a vehicle architecture.

### 3.2 Important setup details

The wheel body is created as a dynamic body at:

```cpp
bodyDef.position = { 0.0f, 2.0f, 0.0f };
bodyDef.rotation = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisY, b3Vec3_axisZ );
```

The shape is a primitive cylinder:

```cpp
b3CreateCylinder( 0.25f, 0.4f, 0.0f, 12 );
```

Important: `b3CreateCylinder(height, radius, yOffset, sides)`.

This reinforces the M2.1 lesson: never guess low-level primitive argument order.

The joint frame setup is the key part:

```cpp
jointDef.base.bodyIdA = groundId;
jointDef.base.bodyIdB = m_bodyId;
jointDef.base.localFrameA.p = { 0.0f, 3.0f, 0.0f };
jointDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
jointDef.base.localFrameB.p = { 0.0f, 0.0f, 0.0f };
jointDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
```

Because the ground body is positioned at y = -1, local frame A at y = 3 corresponds to world y = 2, the same as the wheel body center.

That means the sample starts with wheel-joint translation near zero.

### 3.3 API surface worth remembering

For suspension:

```cpp
b3WheelJoint_EnableSuspensionLimit
b3WheelJoint_SetSuspensionLimits
b3WheelJoint_EnableSuspension
b3WheelJoint_SetSuspensionHertz
b3WheelJoint_SetSuspensionDampingRatio
```

For spin:

```cpp
b3WheelJoint_EnableSpinMotor
b3WheelJoint_SetMaxSpinTorque
b3WheelJoint_SetSpinMotorSpeed
```

For steering:

```cpp
b3WheelJoint_EnableSteering
b3WheelJoint_SetSteeringHertz
b3WheelJoint_SetSteeringDampingRatio
b3WheelJoint_SetTargetSteeringAngle
b3WheelJoint_EnableSteeringLimit
b3WheelJoint_SetSteeringLimits
b3WheelJoint_GetSteeringAngle
```

### 3.4 Suspicious detail / possible sample bug

In `Joints / Wheel`, the steering damping slider appears to call the suspension damping setter instead of the steering damping setter:

```cpp
if ( ImGui::SliderFloat( "Damping##Steering", &m_steeringDampingRatio, 0.0f, 2.0f, "%.1f" ) )
{
    b3WheelJoint_SetSuspensionDampingRatio( m_jointId, m_suspensionDampingRatio );
    b3Joint_WakeBodies( m_jointId );
}
```

This should be treated as suspicious before copying. The `Joints / Driving` sample uses `b3WheelJoint_SetSteeringDampingRatio` for steering damping, which looks more likely to be the intended call.

Action for future work:

```text
Do not copy the Wheel sample steering damping block blindly.
Verify against the Box3D API/source before using steering damping in Jozz Vehicle.
```

## 4. `Joints / Driving`

### 4.1 What it is

`Joints / Driving` is a small four-wheel vehicle demo using `b3WheelJoint`.

It has:

- a dynamic chassis box;
- four wheel bodies;
- four wheel joints;
- front steering;
- rear drive;
- a heightfield/wavy ground;
- optional third-person camera control;
- a `ParallelJoint` that helps keep the vehicle upright.

This is useful for future vehicle architecture experiments, but it is not the target final car model.

### 4.2 Vehicle setup

The chassis is a dynamic body:

```cpp
bodyDef.position = { 0.0f, 2.5f, 0.0f };
bodyDef.type = b3_dynamicBody;
m_chassisId = b3CreateBody( m_worldId, &bodyDef );

shapeDef.density = 0.5f;
b3BoxHull box = b3MakeBoxHull( 2.0f, 0.5f, 1.0f );
b3CreateHullShape( m_chassisId, &shapeDef, &box.base );
```

The sample then adds a `ParallelJoint` between ground and chassis to keep the vehicle upright:

```cpp
parallelJointDef.base.bodyIdA = groundId;
parallelJointDef.base.bodyIdB = m_chassisId;
parallelJointDef.hertz = 0.5f;
parallelJointDef.dampingRatio = 1.0f;
b3CreateParallelJoint( m_worldId, &parallelJointDef );
```

This is probably a demo stabilizer. It may be useful as a temporary debug crutch, but it should not be treated as final vehicle physics.

### 4.3 Wheel placement and joint frames

The shared wheel-joint setup uses:

```cpp
jointDef.base.bodyIdA = m_chassisId;
jointDef.base.localFrameA.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisX, b3Vec3_axisY );
jointDef.base.localFrameB.q = b3ComputeQuatBetweenUnitVectors( b3Vec3_axisZ, b3Vec3_axisY );
jointDef.enableSuspensionLimit = true;
jointDef.enableSuspensionSpring = true;
jointDef.enableSpinMotor = true;
jointDef.enableSteering = true;
jointDef.enableSteeringLimit = true;
```

Then each corner sets `bodyIdB` and a chassis-local frame A position:

```cpp
front-left:  body position {  1.5f, 2.0f,  0.8f }, frameA.p {  1.5f, -0.5f,  0.8f }
front-right: body position {  1.5f, 2.0f, -0.8f }, frameA.p {  1.5f, -0.5f, -0.8f }
rear-left:   body position { -1.5f, 2.0f,  0.8f }, frameA.p { -1.5f, -0.5f,  0.8f }
rear-right:  body position { -1.5f, 2.0f, -0.8f }, frameA.p { -1.5f, -0.5f, -0.8f }
```

Because the chassis body is at y = 2.5, local frame A y = -0.5 resolves to world y = 2.0, matching the wheel body centers.

Again, this supports the M2.5 rest-center-anchor model.

### 4.4 Steering / drive split

The sample uses a simple split:

```text
front wheels: steering enabled, spin motor disabled
rear wheels:  steering disabled, spin motor enabled
```

This is directly useful for a later first full-vehicle prototype.

But it should not be pulled into the current M2.5 lab yet. It belongs after the one-corner baseline and asset-derived dimensions are stable.

### 4.5 Controls

Third-person mode is toggled with `T`.

When third-person mode is active:

```text
W/S -> throttle forward/reverse
A/D -> steering left/right
```

The step function then applies:

```cpp
b3WheelJoint_SetTargetSteeringAngle( m_frontLeftId, maxSteeringAngle * throttle.y );
b3WheelJoint_SetTargetSteeringAngle( m_frontRightId, maxSteeringAngle * throttle.y );

b3WheelJoint_SetSpinMotorSpeed( m_rearLeftId, -m_spinSpeed * throttle.x );
b3WheelJoint_SetSpinMotorSpeed( m_rearRightId, -m_spinSpeed * throttle.x );
```

This is useful future reference for input architecture.

Hotkey warning:

```text
A/D and T must still go through Jozz Vehicle hotkey audit before use.
```

Do not assume the stock sample's key choices are automatically safe for our lab.

### 4.6 Debug readouts worth copying later

`Driving::Render()` prints useful diagnostics:

- forward speed;
- left/right rear wheel spin speed;
- left/right rear spin torque;
- left/right front steering angle;
- left/right front steering torque.

This is a good pattern for future vehicle debugging.

For Jozz Vehicle, similar HUD lines would be valuable later, especially after moving from one-corner to four-corner tests.

## 5. What we should steal later

Good ideas to steal, carefully:

1. **Four wheel-joint layout**  
   The front/rear left/right naming and four joint IDs are a useful future structure.

2. **Front steering + rear drive split**  
   Good first gameplay baseline before differential/advanced tire models.

3. **Steering API usage**  
   `b3WheelJoint_SetTargetSteeringAngle`, steering hertz/damping/torque and steering limits are exactly the APIs we will need.

4. **Per-frame input mapping**  
   Convert input into target steering angle and spin motor speed each step.

5. **Debug readouts**  
   Steering angle/torque and spin speed/torque should become standard vehicle debug data.

6. **Heightfield terrain as a future stress scene**  
   The wavy heightfield is useful later for suspension testing, but not before the one-corner/four-corner foundations are understandable.

## 6. What we should not steal blindly

Do not copy these blindly:

1. **The full `Driving` sample as our vehicle architecture**  
   It is a compact demo, not a modular vehicle assembly system.

2. **The upright `ParallelJoint` as final physics**  
   Useful as a debug stabilizer, suspicious as final vehicle behavior.

3. **Sphere wheels**  
   The driving sample currently uses sphere wheel shapes, which are okay for a demo but not a proper wheel-corner foundation for Jozz's visual wheel rigs.

4. **Immediate full four-corner jump**  
   We should not skip M3A just because `Driving` exists.

5. **Stock hotkeys without audit**  
   `A/D` and `T` are plausible future keys, but they still need explicit audit.

6. **The suspicious steering damping block from `Joints/Wheel`**  
   Verify before copying.

## 7. Relationship to current Jozz M2.5 lab

Current Jozz M2.5 is still the correct active baseline.

Compared with `Joints / Wheel`, our lab adds project-specific discipline:

- asset-like wheel dimensions;
- centered primitive wheel pivot diagnostics;
- rest drop as explicit setup concept;
- pending vs committed structural setup;
- live root stress mover;
- hotkey conflict handling.

Compared with `Joints / Driving`, our lab is intentionally narrower:

- one corner only;
- static root/chassis debug body;
- no steering yet;
- no full vehicle mass/inertia yet;
- no heightfield terrain yet;
- no third-person driving mode yet.

That narrowness is a strength right now. It keeps the hard wheel-joint semantics visible.

## 8. Recommended future use

Recommended sequence remains:

```text
M2.5 validated primitive corner
-> Foundation Grounding
-> M3A asset-derived primitive dimensions
-> M3B visual-only wheel mesh attachment
-> M3C/M4 first four-corner primitive vehicle experiment
```

When the project reaches the four-corner prototype, `Joints / Driving` should be re-read and used as a reference for:

- four joint ownership;
- front/rear behavior split;
- steering target update;
- spin motor target update;
- vehicle debug readouts.

But the implementation should be Jozz Vehicle-specific, not a direct pasted sample.

## 9. Concrete action items for later

Do later, not during the current no-M3 foundation pass:

1. Add an explicit `M3A_ASSET_DERIVED_PRIMITIVE_DIMENSIONS_PL.md` before coding M3A.
2. When steering begins, verify `b3WheelJoint_SetSteeringDampingRatio` against API/source and avoid the suspicious `Joints/Wheel` damping block.
3. Add a future `M4_FOUR_CORNER_PRIMITIVE_VEHICLE_PLAN_PL.md` that cites `Joints / Driving` as reference, not as architecture.
4. Extend `docs/HOTKEY_AUDIT_PL.md` before using `A/D` or `T`.
5. Keep one-corner M2.5 sample available even after four-corner prototypes exist.

## 10. Final judgement

The stock Box3D samples are useful and reassuring.

They confirm that our M2.5 wheel-joint mental model is aligned with Box3D sample practice. The biggest useful future reference is not the whole `Driving` sample, but its clean separation of front steering and rear drive, plus its steering/spin debug readouts.

The correct move is not to rush into a full car now. The correct move is to keep grounding the foundation, then use these samples as references when the project deliberately opens the four-corner vehicle gate.