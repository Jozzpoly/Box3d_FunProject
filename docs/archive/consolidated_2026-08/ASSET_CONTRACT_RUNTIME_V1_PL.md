> **ARCHIWUM — nie jest bieżącą instrukcją.** 2026-08-04 treść została scalona lub zastąpiona przez `docs/ASSET_CONTRACT_PL.md`. Plik pozostaje jako zapis historii.

# Asset Contract Runtime V1 - Jozz Vehicle

Date: 2026-07-04
Status: implemented for the one-sided suspension mount proof

## Purpose

Asset Contract Runtime V1 moves the next rig-facing data source from local audit reports to stable sidecar contracts.

The separation is now:

```text
assets/contracts/*.asset.json
  Runtime binding source for roles, categories, node hints, scale and source glTF path.

assets/source/*.gltf
  Runtime source for node transforms and visual mesh proof loading.

assets/reports/*latest*
  Local diagnostics only. Do not treat them as the runtime contract source.
```

The existing M3 audit metadata loader still exists for M3A primitive defaults and the old semantic preview. M4 contract runtime does not depend on that report path.

## Implemented Code

```text
samples/jozz_vehicle_asset_contract.h
samples/jozz_vehicle_asset_contract.cpp
```

The loader:

1. Finds a sidecar under `assets/contracts`.
2. Reads `assetId`, `assetType`, `contractVersion`, source glTF path, scale and orientation status.
3. Recursively collects binding objects under `semantics`.
4. Requires runtime-bound bindings to provide `role`, `roleCategory`, `nameHint`, `nodeIndexHint`, `nodePathHint`, `space`, `required`, and `physicsAuthority`.
5. Reads the source glTF directly.
6. Composes node parent transforms before resolving node positions.
7. Resolves each binding by `nodeIndexHint`, with `nameHint` as a sanity check.
8. Preserves duplicate node-name warnings instead of hiding them.

Current validation target:

```text
assets/contracts/one_sided_wheel_mount.asset.json
```

## Runtime Rules

- `nodeIndexHint` and `nodePathHint` are the stable binding hints. Node names alone are unsafe because current glTF exports contain duplicate root names.
- `physicsAuthority` must remain `false` for current visual suspension bindings.
- Contract positions may guide visual placement and diagnostics only.
- Contract positions must not rewrite `b3WheelJoint` frames, primitive collision, or `restDrop`.
- Validator CLI must not open a GUI and must not regenerate reports.

## Validation

`jozz_vehicle_validation.exe` now checks:

- legacy M3 audit metadata/defaults still load;
- the one-sided suspension contract loads from sidecar + source glTF;
- all required suspension roles resolve;
- categories match expected `visual_endpoint`, `physics_hint`, or `visual_part`;
- `nodeIndexHint`, `nodePathHint`, `nameHint` exist;
- `physicsAuthority` is false;
- suspension travel axis length stays in the expected prototype range.

Expected warning:

```text
duplicate node name in source glTF: OneSidedSuspension_Rig x2
```

That warning is intentional and should stay visible.
