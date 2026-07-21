# Photogrammetry Import V2 — current status

**Updated:** 2026-07-21  
**Active branch:** `agent/p1b-world-import-contract-staging`  
**Stack base:** `agent/p1-dataset-inspector-staging@dbbd065d9bbf8887b824e351e464dbbe60be1e68`

## Truth table

| Capability | Status | Evidence / blocker |
|---|---|---|
| P0 vehicle baseline | `PASS_REPORTED` | Windows build/validator/test/smoke recorded in P1 checkpoint |
| P1A parser and synthetic contracts | `PASS_CI` | current PR #1 workflow green |
| P1A real 7+7 inspection | `PASS_SESSION_REPORTED` | 7 GLB, 7 PLY, automatic gate pass, deterministic 7/7 artifacts; local receipt still needed |
| Pair bounds compatibility | `PASS_WITH_REVIEW` | five pairs review, two historical strong-match; all remain bounds-only evidence |
| Full source frame | `IN_PROGRESS` | new explicit frame contract and round-trip tests |
| Internal GLB↔PLY correspondence | `NOT_RUN` | occupancy/internal geometry evidence not implemented |
| Diagnostic preview P2 | `NOT_STARTED` | waits for P1B contract foundation; may start before final world acceptance |
| Accepted world patch | `NOT_STARTED` | no Golden Drive Region or authored review document |
| Collision projection | `NOT_STARTED` | no canonical accepted surface/cooker contract |
| Drive test | `NOT_STARTED` | no physics probes on scan terrain |
| JES transfer candidate | `NO` | requires reimport and second real scan |

## NOW

- validate source and lab axis roles;
- reject undeclared mirror;
- prove coordinate round-trip;
- create stable source package identity and content-derived revision;
- create immutable, unreviewed bounds-only world proposal;
- add both suites to the dependency-free runner.

## NEXT

- wire the contracts to inspector outputs;
- emit private and shareable reports separately;
- create a transactional output bundle with completion manifest;
- add internal occupancy correspondence and sabotage fixture;
- capture a reproducible local receipt for the real 7+7 dataset;
- update/close P1A documentation and integration decision.

## PARKED

- renderer and P2 visuals;
- Golden Drive Region;
- ground classifier;
- heightfield cooker;
- Box3D scan physics lab;
- full vehicle drive loop;
- reimport UI;
- JES implementation.

## Current local-only validation for this P1B package

```text
scan_frames: 8 tests PASS
scan_world_contracts: 10 tests PASS
total new tests: 18 PASS
```

These local tests validate only the new dependency-free modules. Full repository contracts and Windows gate must run after the branch is fetched on the owner machine or CI.
