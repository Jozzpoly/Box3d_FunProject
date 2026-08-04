> **ARCHIWUM — nie jest bieżącą instrukcją.** 2026-08-04 treść została scalona lub zastąpiona przez `docs/ASSET_CONTRACT_PL.md`. Plik pozostaje jako zapis historii.

# Asset Contract v2 Draft — Jozz Vehicle

Status: draft, first sidecars hardened with stable binding hints
Date: 2026-07-03

## Purpose

The old `Socket_`, `Axis_`, `Marker_`, `Part_`, and `Chassis_` node naming system is useful, but not strong enough as the only source of truth.

Asset Contract v2 adds a sidecar `.asset.json` beside each glTF file.

The contract must prevent a future importer from confusing:

```text
human-readable marker names
stable node identity
visual rig endpoints
physics joint frames
physics tuning hints
```

## Core principles

1. glTF contains geometry, skin/node hierarchy, and visible authoring markers.
2. Sidecar JSON contains stable gameplay/importer meaning.
3. Importer must validate both together.
4. Duplicate node names are allowed in raw glTF, but importer must not use names as unique IDs.
5. Model orientation may be corrected later by Jozz; until then, sidecar stores temporary correction metadata.
6. Visual rig markers are not automatically physics frames.
7. Physics prefabs must remain separate from visual assets.

## Minimal schema draft

```json
{
  "assetId": "jozz.offroad_big_wheel.v0",
  "assetType": "wheel",
  "source": {
    "gltf": "../source/offroad_big_wheel.gltf",
    "authoringTool": "Blockbench",
    "contractVersion": 2
  },
  "units": {
    "metersPerBlockbenchUnit": 0.35,
    "status": "prototype_scale"
  },
  "orientation": {
    "status": "authoring_orientation_not_final",
    "authoringUp": "+Y",
    "gameUp": "+Y",
    "temporaryImporterCorrection": {
      "enabled": true,
      "note": "Jozz will adjust final orientation after in-game visual tests."
    }
  },
  "semantics": {
    "sockets": {},
    "axes": {},
    "markers": {}
  },
  "physics": {
    "role": "visual_or_prefab_hint",
    "v0PhysicalTreatment": "defined_by_prefab_not_raw_mesh"
  }
}
```

## Runtime convention

Recommended game-space convention:

```text
+X = vehicle forward
+Y = up
+Z = vehicle left/right lateral axis, exact side handled by mounting side
```

Current authoring assets are not guaranteed to already match this. Importer must convert or annotate; gameplay physics must not contain ad-hoc per-model rotations.

## Current prototype scale

Temporary recommendation:

```text
1 Blockbench unit = 0.35 meter
```

Why:

- offroad wheel radius marker is about `1.46875` Blockbench units;
- at `0.35 m/unit`, wheel radius becomes about `0.514 m`;
- that gives about `1.03 m` tire diameter, plausible for a large offroad vehicle.

This value is **not final**. It is a prototype constant, but it must be centralized.

## Binding rules before runtime import

The sidecar must eventually distinguish between these concepts:

```text
nameHint        human-readable expected node name, not unique identity
nodeIndexHint   audited glTF node index hint, validated against name/path
nodePathHint    stable parent-chain path when available
role            semantic gameplay/import role
roleCategory    visual_endpoint / physics_hint / physics_authority / etc.
physicsAuthority whether the binding may define physics behavior
space           authoring space / game space / local node space
required        whether missing binding is error or warning
```

Draft binding object:

```json
{
  "role": "wheel.spinAxis.a",
  "roleCategory": "visual_endpoint",
  "nameHint": "Axis_WheelSpin_A",
  "nodeIndexHint": 6,
  "nodePathHint": "12:Big_Wheel/11:Big_Wheel/6:Axis_WheelSpin_A",
  "physicsAuthority": false,
  "required": true,
  "space": "authoring_world_after_composed_transforms",
  "notes": "Name is not unique identity. Importer must resolve and validate path/index."
}
```

Why this matters:

Current glTF exports have duplicate root/node names. A future importer that does `findNodeByName()` and assumes uniqueness will be broken by design.

Current sidecars already use this object shape for sockets, axes, markers and visual parts. The current audit tool still accepts legacy string bindings, but reports richer metadata when the object shape is present.

## Role categories

Every semantic marker should eventually declare one of these role categories:

```text
visual_endpoint       endpoint for procedural visual rigging
visual_part           named mesh/part to pose or stretch
physics_hint          may inform generated primitive physics dimensions
physics_authority     allowed to define physics prefab data
diagnostic_marker     draw/debug only
```

Default rule:

```text
visual_endpoint != physics_authority
```

A marker cannot become physics authority unless the contract explicitly says so.

## Wheel asset required semantics

For a wheel asset, v2 should require:

```text
wheel center hint or mount socket
spin axis A/B
outer radius marker
width left/right markers
scale metadata
orientation status
```

Safe M3A derivations:

```text
radius from wheel center Y to outer radius marker Y, after scale
width from width marker separation, after scale
```

Unsafe until stronger contract:

```text
physics rest drop
steering pivot
suspension limits
```

## Suspension corner visual required semantics

For a suspension-corner visual asset, v2 should require:

```text
visual chassis mount
visual wheel center
suspension travel top/bottom axis markers
damper upper/lower endpoints
cardan drive/hub endpoints
visual moving parts if present
```

Safe M3A use:

```text
suspension travel top/bottom as total-travel hint
```

Unsafe until stronger contract:

```text
body-A wheel-joint frame
restDrop directly from Socket_ChassisMount -> Socket_WheelCenter
full multi-body suspension
```

Reason:

M2.3 proved that treating the visual chassis/damper mount as the wheel-joint body-A frame creates wrong spring behavior. `b3WheelJoint` needs frame A at the rest wheel-center anchor, not the visible mount.

## Damper visual required semantics

For a damper visual asset, v2 should require:

```text
upper visual part
stretch visual part
lower visual part
stretch axis
```

The damper is visual-only in v0.

Future rigging should procedurally place/stretch it between damper sockets from the suspension asset. It should not drive physics.

## Cardan visual required semantics

For a cardan shaft visual asset, v2 should eventually require:

```text
shaft start socket or local start endpoint
shaft end socket or local end endpoint
length axis
optional rotating visual part
```

Current cardan source has no semantic nodes. For v0, it may be procedurally placed between:

```text
Socket_CardanDrive
Socket_CardanHub
```

from the suspension asset.

That workaround is acceptable for a first visual placeholder but should not be considered final rig contract quality.

## Contract validation backlog

Future `tools/asset_audit.py --validate-contracts` should check:

1. every contract source file exists;
2. every required semantic name resolves to at least one node;
3. every `nodeIndexHint` is in range and points to the expected node name;
4. every runtime-bound semantic has `nodePathHint`, `role`, and `roleCategory`;
5. duplicate names are detected and contract does not rely on ambiguous name-only binding;
6. required wheel markers exist;
7. wheel radius and width marker distances are plausible;
8. suspension travel axis exists and has plausible length;
9. damper visual parts exist;
10. cardan missing semantic nodes are reported as expected warning/error based on contract status;
11. all contracts share the same prototype scale or explicitly explain why not;
12. generated markdown report includes warnings severe enough that future agents cannot ignore them.

## M3A contract stance

M3A may use current contracts and audit data as **offline/reference input**.

M3A should not require raw glTF mesh import or physics authority from visual markers.

The safest M3A path is:

```text
centralize asset-derived constants in code with comments pointing to audited markers
```

A later M3A.1 can load the same constants from JSON once contract validation is stronger.

Current M3B/M3B.2 state still uses the audit metadata bridge for semantic positions and a narrow static visual proof for one wheel mesh. Final runtime import should move toward the sidecar contract rather than depending forever on `asset_audit_latest.json`.

## Final warning

The contract exists to stop the project from lying to itself.

If a value is only a visual hint, call it a visual hint. If it drives physics, it must be explicit, validated, and documented.
