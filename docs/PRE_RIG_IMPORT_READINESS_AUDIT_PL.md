# Pre-Rig / Import Readiness Audit — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: foundation audit before M3A/M3B, no implementation changes

## 1. Purpose

This document answers the question:

```text
What must be prepared before we push Jozz Vehicle into rigging/importing Jozz's glTF models?
```

It is intentionally critical. The project has a validated M2.5 primitive wheel-corner lab, but that does not mean the asset import/visual rig path is ready.

## 2. Current verdict

The project is ready for the next small technical gate:

```text
M3A — asset-derived primitive dimensions
```

The project is **not** ready for a heavy glTF renderer/importer/visual rig push yet.

Reason:

- M2.5 physics baseline is solid enough to feed from asset measurements;
- current asset audit has enough data for wheel radius/width and suspension travel experiments;
- but current asset contracts are still too soft for reliable runtime binding;
- duplicate node names exist in every current glTF;
- importer identity rules are not hardened;
- visual rig roles are not separated enough from physics prefab roles.

So the correct order is:

```text
M2.5 stable primitive corner
-> M3A asset-derived primitive dimensions
-> contract/audit hardening
-> M3B first visual-only wheel attachment
-> visual rig for wheel/suspension/damper/cardan
-> four-corner vehicle later
```

## 3. What is already strong

### 3.1 Physics baseline

M2.5 has the important wheel-joint semantics correct:

```text
Frame A = rest wheel-center anchor on chassis/root
Frame B = wheel body center
spring rest = translation 0
```

This is reinforced by the stock Box3D `Joints / Wheel` and `Joints / Driving` samples, documented in:

```text
docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md
```

### 3.2 Runtime vs structural setup separation

The current sample correctly separates:

```text
pending structural edit values
committed physics values
live root realtime debug controls
```

This must survive every next phase. Import/rig work must not reintroduce direct live mutation of un-applied structural data.

### 3.3 Asset source folder and sidecars exist

Current source assets:

```text
assets/source/Asset_Dumper.gltf
assets/source/Cardan_shaft.gltf
assets/source/Offroad_Big_Wheels.gltf
assets/source/One_Sided_wheel_mount.gltf
```

Current sidecar contracts:

```text
assets/contracts/asset_dumper.asset.json
assets/contracts/cardan_shaft.asset.json
assets/contracts/offroad_big_wheel.asset.json
assets/contracts/one_sided_wheel_mount.asset.json
```

This is already much better than trying to infer everything from raw glTF node names.

## 4. Main blockers before real import/rig

### 4.1 Contract bindings are name-based hints, not stable IDs

Current contracts bind semantics mostly like this:

```json
"wheelMount": "Socket_WheelMount"
```

That is acceptable for a draft, but not enough for runtime import because duplicate node names exist in current glTF files.

Future binding needs at least one stable strategy:

```text
semantic role -> expected node path / index / parent chain / fallback name hint
```

Names may remain useful for human readability, but the importer must not treat names as unique IDs.

### 4.2 Visual sockets are not automatically physics frames

The M2.3 failure proved this hard.

For future contract design, the following must remain separate:

```text
visual chassis mount / damper mount / artistic socket
physics rest wheel-center anchor
actual wheel body center
```

Do not derive the wheel-joint body-A frame directly from `Socket_ChassisMount` or damper sockets.

### 4.3 `asset_audit.py` audits but does not enforce contract correctness

The current tool already:

- loads glTF;
- composes parent transforms;
- reports duplicate node names;
- reports semantic node positions;
- reports mesh counts and bounds.

But it does not yet enforce:

- required semantics by asset type;
- contract references actually resolve to intended nodes;
- duplicate-name-safe binding strategy;
- axis marker length sanity;
- wheel radius/width consistency;
- suspension travel axis consistency;
- cardan missing semantic nodes as a meaningful warning/error;
- scale consistency across all contracts.

That is fine for M3A planning, but not enough for M3B runtime importer confidence.

### 4.4 Cardan shaft is not import-rig-ready

The cardan source has no semantic nodes in the current audit.

The current contract works around this by saying the cardan is placed procedurally between suspension sockets:

```text
Socket_CardanDrive -> Socket_CardanHub
```

That is okay for a later v0 visual placeholder, but not enough for a robust rig system.

### 4.5 Suspension asset has useful visual semantics but not enough physics separation

`One_Sided_wheel_mount.gltf` has many useful sockets:

- chassis mount;
- wheel center;
- damper upper/lower left/right;
- cardan drive/hub;
- suspension travel top/bottom.

But the contract should explicitly mark which ones are:

```text
visual rig endpoint
visual diagnostic point
physics hint
not physics authority
```

Right now those categories are implied, not enforced.

### 4.6 Importer/renderer scope is not sliced yet

The project must not jump from no runtime glTF importer to full visual rig.

The safe import ladder is:

```text
M3B.0 read metadata only / no rendering
M3B.1 draw debug points from semantic positions
M3B.2 render one static visual wheel mesh at origin
M3B.3 attach visual wheel mesh to primitive wheel body
M3B.4 apply explicit visual correction transform
M3B.5 only then rig suspension/damper/cardan visuals
```

If a future agent starts with full material/skin/animation support, they are going too wide.

## 5. Asset-derived measurements: what is usable now

### 5.1 Wheel radius and width

`Offroad_Big_Wheels.gltf` has enough marker data for M3A:

```text
Socket_WheelMount       [0.25, 0.5, 0.0]
Axis_WheelSpin_A        [0.4375, 0.5, 0.0]
Axis_WheelSpin_B        [-1.0625, 0.5, 0.0]
Marker_TireRadiusOuter  [-0.125, 1.96875, 0.0]
Marker_TireWidthLeft    [-0.75, 0.5, 0.0]
Marker_TireWidthRight   [0.5, 0.5, 0.0]
```

At current prototype scale:

```text
1 Blockbench unit = 0.35 m
```

Candidate values:

```text
radius ≈ (1.96875 - 0.5) * 0.35 = 0.5140625 m
width  ≈ abs(0.5 - (-0.75)) * 0.35 = 0.4375 m
```

This matches current M2.5 hard-coded defaults closely:

```text
radius 0.52 m
width  0.44 m
```

So M3A can mostly centralize and trace these values, rather than inventing new ones.

### 5.2 Suspension travel axis

`One_Sided_wheel_mount.gltf` has:

```text
Axis_SuspensionTravel_Top     [-1.1875,  1.5, 0.0]
Axis_SuspensionTravel_Bottom  [-1.1875, -0.5, 0.0]
Socket_WheelCenter            [-1.1875,  0.5, -0.0625]
```

Travel marker length is about:

```text
2.0 Blockbench units * 0.35 = 0.70 m
```

Current M2.5 travel total is:

```text
rebound 0.42 + compression 0.32 = 0.74 m
```

That is close enough to justify using the axis as a future asset-derived travel hint.

But do not blindly map top/bottom to Box3D limits yet. Decide the split explicitly:

```text
example: rebound = 0.39 m, compression = 0.31 m
or keep current 0.42 / 0.32 until better gameplay feel exists
```

### 5.3 Rest drop is not safely derivable yet

Do **not** blindly derive `restDrop` from:

```text
Socket_ChassisMount -> Socket_WheelCenter
```

That pair is visual/authoring data. It may not be the `b3WheelJoint` rest anchor relationship.

M2.3 already failed because visual mount was confused with physics frame A.

For M3A:

```text
wheel radius/width: safe to derive
travel total: safe as a hint
rest drop: keep explicit/tuned unless the contract gains a dedicated physics rest anchor field
```

## 6. Code readiness before M3A

Current source file:

```text
samples/sample_jozz_vehicle_lab.cpp
```

It is still acceptable for M3A, but it should not absorb renderer/importer code.

Safe next code shape for M3A:

```text
sample_jozz_vehicle_lab.cpp
  - sample class and UI remains here for now
  - uses a small data struct for default dimensions

optional later split:
  samples/jozz_vehicle_asset_dimensions.h/.cpp
  samples/jozz_vehicle_corner_lab.h/.cpp
```

Do not split everything before M3A unless it stays very small. The danger is spending a day on architecture while not improving the game.

## 7. Recommended immediate work plan

### Step 1 — Write M3A plan before code

Create:

```text
docs/M3A_ASSET_DERIVED_PRIMITIVE_DIMENSIONS_PLAN_PL.md
```

This should define:

- exact input files;
- exact values derived from audit;
- what is safe to derive;
- what must remain explicit/tuned;
- validation checklist.

### Step 2 — Harden contract draft language

Update:

```text
docs/ASSET_CONTRACT_V2_DRAFT_PL.md
```

Add a section that separates:

```text
semantic name hint
stable node binding
visual rig endpoint
physics authority
physics hint
```

### Step 3 — Add a future audit-tool backlog

Do not necessarily implement it immediately, but document the next validator requirements.

Needed later:

```text
tools/asset_audit.py --validate-contracts
```

### Step 4 — Only then code M3A

M3A should be tiny:

- centralize current wheel radius/width defaults;
- trace them to audit/contract data;
- keep primitive physics;
- keep panel M2.5 or rename carefully only if milestone is actually implemented;
- preserve live root and Apply behavior.

## 8. No-go list before rig/import

Do not do these yet:

- full glTF renderer;
- skinning/animation support;
- material/texture pipeline;
- full vehicle assembly;
- steering implementation;
- mesh collision;
- runtime dependency on node names as unique IDs;
- visual sockets as physics frames;
- automatic restDrop from visual chassis mount;
- new hotkeys without audit.

## 9. Final judgement

The project is in a good position, but only if the next step remains boring and controlled.

M3A is the correct next technical move because it connects Jozz's real models to the physics lab without introducing renderer/importer risk.

The rig/import path should be prepared now, but not executed as one giant leap.