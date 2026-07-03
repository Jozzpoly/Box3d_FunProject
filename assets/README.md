# Assets — Jozz Vehicle Box3D Native

Source models are kept in `assets/source/`.

Sidecar contracts are kept in `assets/contracts/`.

## Current source files

- `Asset_Dumper.gltf`
- `Cardan_shaft.gltf`
- `Offroad_Big_Wheels.gltf`
- `One_Sided_wheel_mount.gltf`

These names intentionally match Jozz's cleaned local filenames without chat-upload suffix numbers.

## Important

These are research/startup assets, not final locked production assets.

Known current condition:

- duplicate root names exist;
- orientation is not final;
- scale is prototype-only;
- marker/socket naming is useful but not strong enough alone;
- importer must compose transforms through parent nodes.

Run the audit after changing source models:

```powershell
py tools\asset_audit.py
```

The current generated audit lives in `assets/reports/asset_audit_latest.md` and `assets/reports/asset_audit_latest.json`.
