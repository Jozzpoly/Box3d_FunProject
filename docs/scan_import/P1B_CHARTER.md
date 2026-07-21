# P1B charter — World Import Contract Foundation

**Maturity:** `DECISION_CANDIDATE`  
**Implementation authority:** bounded branch work only  
**Named consumer:** P2 Diagnostic Preview and later Golden Drive Region review

## Owner problem

The first scan inspector can prove file integrity and bounds compatibility, but the project still lacks an explicit boundary between private source evidence, generated import proposal and future authored world decisions. Without that boundary, later preview/cooking work could accidentally turn a GLB, PLY or Box3D heightfield into world truth and make safe reimport impossible.

## Research question

Can the current P1 inspection result be converted into a backend-neutral, deterministic source revision and an explicitly unreviewed world proposal while preserving a complete, testable source→lab frame contract?

## Inputs

- P1A inspection schema v3;
- canonical `MipTile_N` source labels and hashes;
- one local source-frame contract;
- no raw scan files in Git.

## Outputs

- normalized source-frame contract and SHA-256;
- stable `ScanSourcePackage` with content-derived revision;
- immutable `WorldImportProposal` with bounds-only pair evidence;
- dependency-free tests;
- no renderer, collision or authored acceptance output.

## Positive cases

- right-handed source Z-up maps to right-handed lab Y-up without mirror;
- nonzero local origin and units-per-meter round-trip exactly in test tolerance;
- source file content change keeps `packageId` but changes `revisionId`;
- unordered source arrays produce sorted stable tile records;
- historical `strong-match` becomes `bounds-strong-match` in proposal.

## Negative/intervention cases

- axis-role matrix mismatch;
- duplicate semantic axis;
- handedness mismatch;
- nonfinite/negative units or origin;
- mirror without approval;
- duplicate or unpaired tiles;
- invalid stable ID;
- native/GPU/UI handle field;
- proposal mutation after hash;
- manual decisions embedded in proposal.

## Falsifier

Reject or redesign P1B if the clean contract cannot express the real scan frame without renderer-specific assumptions, if stable identity depends on content hashes or filenames, or if a future review/reimport would need to mutate the proposal instead of producing a separate authored document.

## Non-goals

- full GLB↔PLY geometry correspondence;
- visual preview;
- ground extraction;
- world editing UI;
- accepted world patch;
- collision cooking;
- Box3D integration;
- final JES schema.

## Promotion gate

```text
NEW_DEPENDENCY_FREE_TESTS = PASS
FULL_P1_CONTRACT_RUNNER = PASS
WINDOWS_VEHICLE_GATE = PASS
NATIVE_HANDLE_FIREWALL = PASS
SOURCE_FRAME_ROUND_TRIP = PASS
MIRROR_REQUIRES_EXPLICIT_APPROVAL = PASS
PROPOSAL_STATUS = UNREVIEWED
PAIRING_EVIDENCE = BOUNDS_ONLY
```
