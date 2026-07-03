# Assets — Jozz Vehicle Box3D Native

Source models are intended for `assets/source/`.

Sidecar contracts are kept in `assets/contracts/`.

## Current source files

- `offroad_big_wheel.gltf`
- `one_sided_wheel_mount.gltf`
- `asset_dumper.gltf`
- `cardan_shaft.gltf`

## Important

These are research/startup assets, not final locked production assets.

Known current condition:

- duplicate root names exist;
- orientation is not final;
- scale is prototype-only;
- marker/socket naming is useful but not strong enough alone;
- importer must compose transforms through parent nodes.

The current uploaded glTF files were audited in `assets/reports/asset_audit_initial_2026-07-03.md`. The model source files themselves still need to be added to the branch through a normal git/file upload path if not already present locally.
