# Photogrammetry Import V2 — current status

**Updated:** 2026-07-21  
**Active branch:** `agent/p1b-owner-gate-hardening`  
**Stack base:** `agent/p1b-inspector-bundle-staging@a7459be8ffad14a6bfaea04696750b1e18bd0b43`

## Truth table

| Capability | Status | Evidence / blocker |
|---|---|---|
| P0 vehicle baseline | `PASS_LOCAL_EXACT_HEAD` | Windows build/validator/test/smoke passed locally on `a7459be...` |
| P1A parser and synthetic contracts | `PASS_CI` | PR #1 workflow green |
| P1A real 7+7 inspection | `PASS_LOCAL_DETERMINISTIC` | two real reports; each 7 GLB, 7 PLY, 7 pairs, automatic gate pass; reports byte-identical |
| Pair bounds compatibility | `PASS_WITH_REVIEW` | five pairs review, two historical strong-match; all remain `BOUNDS_ONLY` |
| Full source frame contract | `IMPLEMENTED_NOT_OWNER_CONFIRMED` | signs, handedness, mirror, local origin and round-trip implemented; real owner values absent |
| Source-frame creation workflow | `IMPLEMENTED_TESTED_ISOLATED` | CLI derives matrix from explicit roles, blocks implicit mirror and never confirms implicitly |
| Source package / proposal boundary | `PASS_CI` | exact source revision binding and adversarial validation covered by hosted matrix |
| Private/shareable evidence split | `PASS_CI_SYNTHETIC` | explicit allow-list; no source names, paths, bounds, hashes or free-form warnings |
| Transactional evidence bundle | `PASS_CI_SYNTHETIC` | content-addressed directory, cross-document checks, `COMPLETE.json`, tamper verifier |
| Independent read-only verifier | `PASS_CI_SYNTHETIC` | valid, tampered and missing bundle CLI cases pass on Windows/Linux |
| Hosted P1B bundle CI | `PASS_8_OF_8` | final PR #3 head `a7459be8ffad14a6bfaea04696750b1e18bd0b43` |
| Owner-gate orchestrator | `IMPLEMENTED_AWAITING_HOSTED_CI` | deterministic report discovery, confirmed-frame requirement, bundle + independent verify, privacy-safe receipt |
| Windows gate reliability | `FIX_IMPLEMENTED_AWAITING_LOCAL_RETEST` | fresh-worktree configure, CMake exit codes and expected executable existence are now hard gates |
| Real 7+7 bundle | `NOT_RUN` | requires owner-confirmed source frame |
| Manual shareable privacy review | `NOT_RUN` | waits for real bundle; exactly one review target will be printed |
| Internal GLB↔PLY correspondence | `BLOCKED` | waits for `P1B_BUNDLE_PASS`; occupancy/internal geometry evidence not implemented |
| Diagnostic preview P2 | `NOT_STARTED` | waits for real P1B bundle and correspondence gates |
| Accepted world patch | `NOT_STARTED` | no Golden Drive Region or authored review |
| Collision projection | `NOT_STARTED` | no accepted surface/cooker contract |
| Drive test | `NOT_STARTED` | no scan-terrain physics probes |
| JES transfer candidate | `NO` | requires reimport and second real scan |

## Completed in the owner-gate hardening package

- removed dependence on placeholder inspection paths;
- added deterministic discovery for repeated real `inspection.json` outputs;
- auto-selection is allowed only for byte-identical passing 7+7 reports;
- added explicit source-frame generator/validator with derived matrix and mirror block;
- added one-command bundle publication plus separate-process verification;
- added a receipt that omits paths, source hashes, names, bounds and coordinates;
- separated technical verification from manual shareable privacy acknowledgement;
- repaired `tools/gate.ps1` so a missing CMake configure/build cannot be reported as green;
- added 12 focused hardening tests and connected them to the canonical runner and matrix.

## Evidence already obtained locally

On exact P1B bundle head `a7459be8ffad14a6bfaea04696750b1e18bd0b43`:

```text
P1/P1B dependency-free tests: 77/77 PASS
Windows gate: build 3/3, validator PASS, test PASS, smoke 0 errors
Real inspection A: 7 GLB / 7 PLY / 7 pairs / automatic gate PASS
Real inspection B: 7 GLB / 7 PLY / 7 pairs / automatic gate PASS
Inspection A and B: byte-identical
```

No source coordinates, source names or source hashes are recorded in this status file.

## Remaining owner-local gate

Do not start occupancy correspondence or P2 until all are complete:

1. pull or check out the hardening branch after hosted CI is green;
2. rerun the canonical Python suite and corrected `tools/gate.ps1`;
3. create a real local source-frame contract with owner-confirmed axes, units, mirror state and origin;
4. run `scan_owner_gate.py finalize` against the real 7+7 output root;
5. open and manually inspect only the printed `shareable/inspection.shareable.json`;
6. rerun with `--acknowledge-shareable-privacy-review`;
7. retain all private outputs under ignored `build/` only;
8. record only the privacy-safe receipt status.

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

Existing P1/P1B suite: 77 tests on the prior head.

Owner-gate hardening adds:

```text
scan_source_frame_contract: 4 tests
scan_owner_gate: 5 tests
scan_gate_ps1: 3 regression-contract tests
```

Expected canonical total after integration: 89 tests.

These tests prove workflow contracts and fail-fast boundaries. They do not prove the correctness of the private real frame or semantic correspondence of GLB and PLY interiors.
