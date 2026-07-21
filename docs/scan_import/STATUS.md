# Photogrammetry Import V2 — current status

**Updated:** 2026-07-21  
**Active branch:** `agent/p1b-inspector-bundle-staging`  
**Stack base:** `agent/p1b-world-import-contract-staging@eac2327589ad799e270ed760cf7288696f4f50c3`

## Truth table

| Capability | Status | Evidence / blocker |
|---|---|---|
| P0 vehicle baseline | `PASS_REPORTED` | Windows build/validator/test/smoke recorded in P1 checkpoint |
| P1A parser and synthetic contracts | `PASS_CI` | PR #1 workflow green |
| P1A real 7+7 inspection | `PASS_SESSION_REPORTED` | 7 GLB, 7 PLY, automatic gate pass, deterministic 7/7 artifacts; reproducible local receipt still needed |
| Pair bounds compatibility | `PASS_WITH_REVIEW` | five pairs review, two historical strong-match; all remain `BOUNDS_ONLY` |
| Full source frame contract | `IMPLEMENTED_NOT_OWNER_CONFIRMED` | signs, handedness, mirror, local origin and round-trip implemented; real owner frame values absent |
| Source package / proposal boundary | `PASS_CI` | exact source revision binding and adversarial validation covered by hosted matrix |
| Private/shareable evidence split | `PASS_CI_SYNTHETIC` | explicit allow-list; no source names, paths, bounds, hashes or free-form warnings |
| Transactional evidence bundle | `PASS_CI_SYNTHETIC` | content-addressed directory, cross-document checks, `COMPLETE.json`, tamper verifier |
| Independent read-only verifier | `PASS_CI_SYNTHETIC` | valid, tampered and missing bundle CLI cases pass on Windows/Linux |
| Hosted P1B bundle CI | `PASS_8_OF_8` | run `29834444976`, code head `9718b89a46834a5e102ea3342fb54ab5d044c501` |
| Real 7+7 bundle | `NOT_RUN` | requires owner-local `inspection.json` and confirmed frame contract |
| Internal GLB↔PLY correspondence | `NOT_RUN` | occupancy/internal geometry evidence not implemented |
| Diagnostic preview P2 | `NOT_STARTED` | waits for real P1B bundle and correspondence gates |
| Accepted world patch | `NOT_STARTED` | no Golden Drive Region or authored review |
| Collision projection | `NOT_STARTED` | no accepted surface/cooker contract |
| Drive test | `NOT_STARTED` | no scan-terrain physics probes |
| JES transfer candidate | `NO` | requires reimport and second real scan |

## Completed in the current package

- converted one private inspection into `ScanSourcePackage` and `WorldImportProposal`;
- preserved exact cross-document source revision links;
- emitted one canonical shareable projection through a field allow-list;
- published immutable content-addressed bundles only after `COMPLETE.json`;
- rejected tampering, extra files, incomplete writes, duplicate JSON keys and non-finite values;
- added a separate read-only verifier command;
- ran the exact repository modules through the complete hosted matrix.

## Hosted validation

The final code head passed:

```text
Canonical Ubuntu / Python 3.11 / stdlib  PASS
Ubuntu / Python 3.11 / NumPy            PASS
Ubuntu / Python 3.13 / stdlib           PASS
Ubuntu / Python 3.13 / NumPy            PASS
Windows / Python 3.11 / stdlib          PASS
Windows / Python 3.11 / NumPy           PASS
Windows / Python 3.13 / stdlib          PASS
Windows / Python 3.13 / NumPy           PASS
```

Run: `29834444976` — **8/8 PASS**.

## Remaining owner-local gate

Do not start occupancy correspondence or P2 until all are complete:

1. create a real local source-frame contract with owner-confirmed axes, units, mirror state and origin;
2. create a bundle from the real 7+7 `inspection.json`;
3. run the independent verifier against that bundle;
4. manually inspect only `shareable/inspection.shareable.json` for privacy and semantic overclaims;
5. run `tools/gate.ps1` on the owner Windows checkout;
6. record a non-private receipt containing only hashes, statuses and tool versions.

## NEXT after that gate

- internal occupancy correspondence GLB↔PLY;
- sabotage fixture: identical bounds, mismatched interior geometry;
- adjacency/seam evidence;
- then P2 Diagnostic Preview.

## PARKED

- Golden Drive Region;
- ground classifier;
- heightfield cooker;
- Box3D scan physics lab;
- full vehicle drive loop;
- reimport UI;
- JES implementation.

## Current test scope

Bundle persistence/privacy:

```text
scan_import_bundle: 13 adversarial tests
scan_import_bundle_verify: 3 CLI tests
```

These run inside the full dependency-free P1/P1B suite. They prove the synthetic contract and filesystem boundary, not correctness of the private real frame or semantic correspondence of GLB and PLY interiors.
