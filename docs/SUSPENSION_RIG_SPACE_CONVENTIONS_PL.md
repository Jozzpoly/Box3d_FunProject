# Suspension Rig Space Conventions - Jozz Vehicle

Date: 2026-07-04
Status: M4 foundation convention, visual-only

## Spaces

Current one-corner rig uses these spaces:

```text
Physics world
  Box3D body transforms and wheel-joint motion.

Chassis/root frame
  Static debug chassis body. Q/E live root moves this body in realtime.

Rest wheel-center frame
  Frame A concept: the intended wheel center at wheel-joint translation 0.

Live wheel body frame
  Frame B concept: dynamic primitive wheel body center and spin transform.

Authoring asset space
  Blockbench/glTF node space after composed node transforms.

Game visual correction frame
  Temporary render-only correction that places current authored visuals in the lab.
```

## Current Physics Authority

The only physics authority remains:

```text
Body A: static chassis/root debug rig
Body B: dynamic primitive wheel body
Joint:  b3WheelJoint
Frame A: rest wheel-center anchor on chassis/root
Frame B: wheel body center
restDrop: explicit/tuned value
```

Suspension sockets are not physics frames.

## Current M4 Visual Placement

`One_Sided_wheel_mount.gltf` is loaded as a single static visual proof through the visual asset wrapper. The mount transform is built from the contract runtime:

```text
contract suspension.visual.wheel_center -> rest wheel-center frame
authoring +Y -> game vertical +Y
authoring +X -> game wheel-side axis
```

This is a temporary visual correction for proving placement and diagnostics. It is not a final importer orientation policy.

## Diagnostics

The sample can show:

- physics live wheel center;
- rest wheel center;
- contract wheel center at rest;
- contract chassis mount;
- contract travel top/bottom;
- moving debug endpoints for damper and cardan roles.

If those points do not line up, fix the visual correction or contract data first. Do not hide the mismatch by changing wheel-joint frames.

## Explicit Non-Goals

- No mesh collision.
- No full vehicle.
- No steering.
- No multi-body suspension.
- No skinning or bone animation.
- No torque transfer through cardan.
- No derivation of `restDrop` from visual sockets.
