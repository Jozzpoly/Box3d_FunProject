# Asset Contract Audit Report

| Contract | Asset type | Source | Status | Issues | Bindings |
|---|---|---|---|---:|---:|
| `asset_dumper.asset.json` | `damper_visual` | `../source/Asset_Dumper.gltf` | **WARN** | 1 | 3 |
| `cardan_shaft.asset.json` | `cardan_shaft_visual` | `../source/Cardan_shaft.gltf` | **WARN** | 1 | 3 |
| `offroad_big_wheel.asset.json` | `wheel` | `../source/Offroad_Big_Wheels.gltf` | **WARN** | 1 | 6 |
| `one_sided_wheel_mount.asset.json` | `suspension_corner_visual` | `../source/One_Sided_wheel_mount.gltf` | **WARN** | 1 | 12 |

## `asset_dumper.asset.json`

Asset ID: `jozz.asset_dumper.v0`  
Asset type: `damper_visual`  
Source: `../source/Asset_Dumper.gltf`  
Status: **WARN**

### Issues

- **WARN**: Duplicate node names exist in source glTF: Dumper_rig_root x2. Name-only binding is unsafe.

### Bindings

| Path | Role | Category | Name | Node index hint | Matching node indices |
|---|---|---|---|---:|---|
| `semantics.visualParts.upper` | `damper.visual.upper` | `visual_part` | `Part_Upper` | 0 | `0` |
| `semantics.visualParts.stretch` | `damper.visual.stretch` | `visual_part` | `Part_Stretch` | 1 | `1` |
| `semantics.visualParts.lower` | `damper.visual.lower` | `visual_part` | `Part_Lower` | 2 | `2` |

## `cardan_shaft.asset.json`

Asset ID: `jozz.cardan_shaft.v0`  
Asset type: `cardan_shaft_visual`  
Source: `../source/Cardan_shaft.gltf`  
Status: **WARN**

### Issues

- **WARN**: Duplicate node names exist in source glTF: Cardan_shaft x2. Name-only binding is unsafe.

### Bindings

| Path | Role | Category | Name | Node index hint | Matching node indices |
|---|---|---|---|---:|---|
| `semantics.visualParts.firstPart` | `cardan.visual.first_part` | `visual_part` | `First_Part` | 0 | `0` |
| `semantics.visualParts.midPart` | `cardan.visual.mid_part` | `visual_part` | `Mid_Part` | 2 | `2` |
| `semantics.visualParts.lastPart` | `cardan.visual.last_part` | `visual_part` | `Last_Part` | 1 | `1` |

## `offroad_big_wheel.asset.json`

Asset ID: `jozz.offroad_big_wheel.v0`  
Asset type: `wheel`  
Source: `../source/Offroad_Big_Wheels.gltf`  
Status: **WARN**

### Issues

- **WARN**: Duplicate node names exist in source glTF: Big_Wheel x2. Name-only binding is unsafe.

### Bindings

| Path | Role | Category | Name | Node index hint | Matching node indices |
|---|---|---|---|---:|---|
| `semantics.sockets.wheelMount` | `wheel.visual.wheel_mount` | `visual_endpoint` | `Socket_WheelMount` | 5 | `5` |
| `semantics.axes.wheelSpin[0]` | `wheel.spin_axis.a` | `physics_hint` | `Axis_WheelSpin_A` | 6 | `6` |
| `semantics.axes.wheelSpin[1]` | `wheel.spin_axis.b` | `physics_hint` | `Axis_WheelSpin_B` | 7 | `7` |
| `semantics.markers.tireRadiusOuter` | `wheel.tire_radius_outer` | `physics_hint` | `Marker_TireRadiusOuter` | 8 | `8` |
| `semantics.markers.tireWidthLeft` | `wheel.tire_width_left` | `physics_hint` | `Marker_TireWidthLeft` | 9 | `9` |
| `semantics.markers.tireWidthRight` | `wheel.tire_width_right` | `physics_hint` | `Marker_TireWidthRight` | 10 | `10` |

## `one_sided_wheel_mount.asset.json`

Asset ID: `jozz.one_sided_wheel_mount.v0`  
Asset type: `suspension_corner_visual`  
Source: `../source/One_Sided_wheel_mount.gltf`  
Status: **WARN**

### Issues

- **WARN**: Duplicate node names exist in source glTF: OneSidedSuspension_Rig x2. Name-only binding is unsafe.

### Bindings

| Path | Role | Category | Name | Node index hint | Matching node indices |
|---|---|---|---|---:|---|
| `semantics.sockets.chassisMount` | `suspension.visual.chassis_mount` | `visual_endpoint` | `Socket_ChassisMount` | 0 | `0` |
| `semantics.sockets.wheelCenter` | `suspension.visual.wheel_center` | `visual_endpoint` | `Socket_WheelCenter` | 1 | `1` |
| `semantics.sockets.damperUpperR` | `suspension.visual.damper_upper_r` | `visual_endpoint` | `Socket_DamperUpper_R` | 4 | `4` |
| `semantics.sockets.damperUpperL` | `suspension.visual.damper_upper_l` | `visual_endpoint` | `Socket_DamperUpper_L` | 5 | `5` |
| `semantics.sockets.damperLowerR` | `suspension.visual.damper_lower_r` | `visual_endpoint` | `Socket_DamperLower_R` | 6 | `6` |
| `semantics.sockets.damperLowerL` | `suspension.visual.damper_lower_l` | `visual_endpoint` | `Socket_DamperLower_L` | 7 | `7` |
| `semantics.sockets.cardanDrive` | `suspension.visual.cardan_drive` | `visual_endpoint` | `Socket_CardanDrive` | 8 | `8` |
| `semantics.sockets.cardanHub` | `suspension.visual.cardan_hub` | `visual_endpoint` | `Socket_CardanHub` | 9 | `9` |
| `semantics.axes.suspensionTravel[0]` | `suspension.travel_axis.top` | `physics_hint` | `Axis_SuspensionTravel_Top` | 10 | `10` |
| `semantics.axes.suspensionTravel[1]` | `suspension.travel_axis.bottom` | `physics_hint` | `Axis_SuspensionTravel_Bottom` | 11 | `11` |
| `semantics.visualParts.chassisTop` | `suspension.visual.chassis_top` | `visual_part` | `Chassis_Top` | 2 | `2` |
| `semantics.visualParts.chassisBottom` | `suspension.visual.chassis_bottom` | `visual_part` | `Chassis_Bottom` | 3 | `3` |
