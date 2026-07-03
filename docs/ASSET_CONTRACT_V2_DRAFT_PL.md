# Asset Contract v2 Draft — Jozz Vehicle

Status: draft  
Date: 2026-07-03

## Purpose

The old `Socket_`, `Axis_`, `Marker_`, `Part_`, and `Chassis_` node naming system is useful, but not strong enough as the only source of truth.

Asset Contract v2 adds a sidecar `.asset.json` beside each glTF file.

## Core principles

1. glTF contains geometry, skin/node hierarchy, and visible authoring markers.
2. Sidecar JSON contains stable gameplay/importer meaning.
3. Importer must validate both together.
4. Duplicate node names are allowed in raw glTF, but importer must not use names as unique IDs.
5. Model orientation may be corrected later by Jozz; until then, sidecar stores temporary correction metadata.

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
