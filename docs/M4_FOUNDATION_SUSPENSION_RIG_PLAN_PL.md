# M4 Foundation Suspension Rig - Implementation Status

Date: 2026-07-05
Status: M3C/M4F.1 + M4A + narrow M4B debug preview implemented; 2026-07-05 Jozz screenshot smoke confirmed

## Summary

The project now has a first solid foundation for mounting Jozz's suspension model without giving the visual asset physics authority.

Implemented:

- runtime asset contract loader based on `assets/contracts/*.asset.json`;
- source glTF node-transform resolution by `nodeIndexHint`;
- contract validation in `jozz_vehicle_validation.exe`;
- one-sided suspension mount visual proof in the active lab;
- contract point overlay;
- moving endpoint debug preview for wheel-side damper/cardan endpoints.
- documented manual visual smoke from Jozz screenshots.

Not implemented:

- final glTF importer;
- skinning;
- animation;
- mesh collision;
- multi-body suspension;
- steering;
- full vehicle assembly.

## Active Sample

```text
Jozz Vehicle / Lab M2 Primitive Corner
Panel: Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 foundation debug
```

New toggles:

```text
M4A suspension mount visual
M4A suspension contract points
M4B moving endpoint preview
```

The suspension mount is visual-only and currently uses:

```text
assets/contracts/one_sided_wheel_mount.asset.json
assets/source/One_Sided_wheel_mount.gltf
```

## Code Boundaries

Runtime contract:

```text
samples/jozz_vehicle_asset_contract.*
```

Rig space helpers:

```text
samples/jozz_vehicle_corner_rig.*
```

Visual asset wrapper:

```text
samples/jozz_vehicle_visual_asset.*
```

Active lab integration:

```text
samples/jozz_vehicle_primitive_corner_lab.cpp
```

The lab still owns the one-corner experiment. The new helpers keep contract parsing, rig-space math, and visual loading from becoming hidden ad hoc code inside the sample class.

## Acceptance Criteria

The M4 foundation is healthy when:

- existing wheel drive/brake/live-root behavior still works;
- attached wheel visual stays centered and follows spin;
- primitive wheel debug shape can be hidden fully;
- contract runtime reports sidecar + glTF load status;
- all suspension contract points resolve in CLI validation;
- duplicate node-name warnings stay visible;
- suspension visual can be toggled independently;
- contract points show the difference between physics frames and visual sockets;
- Jozz screenshots show the suspension model, texture, transparency and helper-line labels in the active lab;
- asset reports do not change unless audit tools were intentionally run.

## Manual Smoke 2026-07-05

Jozz screenshots confirm that `One_Sided_wheel_mount.gltf` loads in the active lab with visible texture/material, semi-transparent proof rendering, contract labels, helper lines, the M3B.3 attached wheel, and hidden primitive wheel debug shape.

This confirms the M4 foundation path is alive, but it is not final suspension animation, mesh collision, steering, or production placement.

## Next Responsible Step

The next feature should stay narrow:

```text
M4C - procedural damper/cardan visual proof using contract endpoints
```

That means drawing or loading visual-only damper/cardan pieces between already validated endpoints. It still should not introduce mesh collision, steering, multi-body suspension, or final skinning.
