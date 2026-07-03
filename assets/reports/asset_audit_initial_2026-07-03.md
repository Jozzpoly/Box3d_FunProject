# Initial Asset Audit — 2026-07-03

Generated from the current startup glTF files provided by Jozz.

## Summary

| File | Verts | Tris | Nodes | Meshes | Skins | Duplicate names | Semantic nodes |
|---|---:|---:|---:|---:|---:|---|---:|
| `asset_dumper.gltf` | 240 | 120 | 5 | 1 | 1 | `Dumper_rig_root x2` | 3 |
| `cardan_shaft.gltf` | 456 | 228 | 5 | 1 | 1 | `Cardan_shaft x2` | 0 |
| `offroad_big_wheel.gltf` | 1224 | 612 | 13 | 1 | 1 | `Big_Wheel x2` | 6 |
| `one_sided_wheel_mount.gltf` | 456 | 228 | 14 | 1 | 1 | `OneSidedSuspension_Rig x2` | 12 |

## Key findings

- All four current glTF files are usable as startup assets.
- All four files contain duplicate root-style names; importer must not assume unique node names.
- `cardan_shaft.gltf` has no `Socket_`, `Axis_`, or `Marker_` semantic nodes yet. This is acceptable for v0 because it can be placed between cardan sockets from the suspension asset.
- Model orientation is intentionally not final. Sidecar contracts keep this explicit.
- Scale is prototype-only: `0.35 meter / Blockbench unit`.

## Cardan shaft notes

`cardan_shaft.gltf` appears directionally compatible with the suspension cardan sockets:

```text
mesh world bounds X: about -0.9375 to 0.5
suspension Socket_CardanHub:  [-0.9375, 0.5, 0.0]
suspension Socket_CardanDrive: [0.5, 0.5, 0.0]
```

This is a strong sign that the shaft can be visual-fitted between those two sockets in v0.

## Semantic nodes

### `asset_dumper.gltf`

| Node | Name | World position |
|---:|---|---|
| 0 | `Part_Upper` | `[0.0, 1.5, 0.0]` |
| 1 | `Part_Stretch` | `[0.0, 0.492188, 0.0]` |
| 2 | `Part_Lower` | `[0.0, -0.46875, 0.0]` |

### `cardan_shaft.gltf`

No semantic `Socket_`, `Axis_`, `Marker_`, `Part_`, or `Chassis_` nodes detected by prefix scan. Visual bones/nodes detected separately in the contract: `First_Part`, `Mid_Part`, `Last_Part`.

### `offroad_big_wheel.gltf`

| Node | Name | World position |
|---:|---|---|
| 5 | `Socket_WheelMount` | `[0.25, 0.5, 0.0]` |
| 6 | `Axis_WheelSpin_A` | `[0.4375, 0.5, 0.0]` |
| 7 | `Axis_WheelSpin_B` | `[-1.0625, 0.5, 0.0]` |
| 8 | `Marker_TireRadiusOuter` | `[-0.125, 1.96875, 0.0]` |
| 9 | `Marker_TireWidthLeft` | `[-0.75, 0.5, 0.0]` |
| 10 | `Marker_TireWidthRight` | `[0.5, 0.5, 0.0]` |

### `one_sided_wheel_mount.gltf`

| Node | Name | World position |
|---:|---|---|
| 0 | `Socket_ChassisMount` | `[0.015625, 1.625, -0.453125]` |
| 1 | `Socket_WheelCenter` | `[-1.1875, 0.5, -0.0625]` |
| 2 | `Chassis_Top` | `[0.40625, 0.96875, 0.0]` |
| 3 | `Chassis_Bottom` | `[0.40625, 0.03125, 0.0]` |
| 4 | `Socket_DamperUpper_R` | `[0.046875, 1.84375, -0.8125]` |
| 5 | `Socket_DamperUpper_L` | `[0.046875, 1.84375, 0.8125]` |
| 6 | `Socket_DamperLower_R` | `[-0.71875, 0.03125, -0.8125]` |
| 7 | `Socket_DamperLower_L` | `[-0.71875, 0.03125, 0.8125]` |
| 8 | `Socket_CardanDrive` | `[0.5, 0.5, 0.0]` |
| 9 | `Socket_CardanHub` | `[-0.9375, 0.5, 0.0]` |
| 10 | `Axis_SuspensionTravel_Top` | `[-1.1875, 1.5, 0.0]` |
| 11 | `Axis_SuspensionTravel_Bottom` | `[-1.1875, -0.5, 0.0]` |

## Importer warning policy

Warnings for current assets should not block M0/M1:

- duplicate names: warning;
- authoring orientation not final: warning;
- no cardan semantic socket/axis nodes: warning.

Future blocking errors:

- missing required wheel spin markers for a wheel asset;
- missing required suspension travel axis for a suspension asset;
- unreadable glTF buffers;
- invalid JSON sidecar;
- scale missing from sidecar.
