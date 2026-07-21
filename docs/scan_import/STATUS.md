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
| Source package / proposal boundary | `IMPLEMENTED_LOCAL_PASS` | exact source revision binding and adversarial validation implemented in stacked PR #2 |
| Private/shareable evidence split | `IMPLEMENTED_LOCAL_SYNTHETIC_PASS` | explicit allow-list; no source names, paths, bounds, hashes or free-form warnings |
| Transactional evidence bundle | `IMPLEMENTED_LOCAL_SYNTHETIC_PASS` | content-addressed directory, cross-document checks, `COMPLETE.json`, tamper verifier |
| Hosted P1B bundle CI | `PENDING` | workflow updated for stacked P1B PR targets and `agent/p1b-*` pushes |
| Real 7+7 bundle | `NOT_RUN` | requires owner-local `inspection.json` and confirmed frame contract |
| Internal GLB↔PLY correspondence | `NOT_RUN` | occupancy/internal geometry evidence not implemented |
| Diagnostic preview P2 | `NOT_STARTED` | waits for P1B bundle and correspondence gates |
| Accepted world patch | `NOT_STARTED` | no Golden Drive Region or authored review |
| Collision projection | `NOT_STARTED` | no accepted surface/cooker contract |
| Drive test | `NOT_STARTED` | no scan-terrain physics probes |
| JES transfer candidate | `NO` | requires reimport and second real scan |

## NOW

- convert one private inspection into `ScanSourcePackage` and `WorldImportProposal`;
- preserve exact cross-document source revision links;
- emit one canonical shareable projection through a field allow-list;
- publish immutable content-addressed bundles only after `COMPLETE.json`;
- reject tampering, extra files, incomplete writes, duplicate JSON keys and non-finite values;
- run the new test in the dependency-free repository runner and CI matrix.

## NEXT

- execute the complete runner on hosted Windows/Linux CI;
- create a real local bundle from the 7+7 inspection;
- manually inspect the real shareable JSON for privacy and semantic overclaims;
- record the real P1A/P1B receipt without committing private output;
- add internal occupancy correspondence and the same-bounds/wrong-interior sabotage fixture;
- only then start P2 Diagnostic Preview.

## PARKED

- Golden Drive Region;
- ground classifier;
- heightfield cooker;
- Box3D scan physics lab;
- full vehicle drive loop;
- reimport UI;
- JES implementation.

## Validation evidence

Previously completed for the contract foundation:

```text
scan_frames: 8 tests PASS
scan_world_contracts: 13 tests PASS
```

Completed for the new bundle package in an isolated filesystem harness:

```text
scan_import_bundle: 13 tests PASS
```

The bundle tests cover:

- private/shareable separation;
- source-name, coordinate, hash and warning omission;
- content-addressed idempotence;
- payload tamper detection;
- refusal to overwrite corrupt existing content;
- extra-file and incomplete-directory rejection;
- duplicate JSON key rejection;
- `NaN` rejection before hashing;
- optional inspection/frame promotion gates;
- unsafe output-label rejection;
- cross-document revision mismatch after recomputing proposal hash;
- noncanonical shareable document rejection before publication.

This does **not** yet prove the full repository runner against the exact real sibling modules on hosted CI. The Windows vehicle gate and real 7+7 bundle remain owner-local gates.
