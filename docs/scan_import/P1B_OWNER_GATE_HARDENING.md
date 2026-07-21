# P1B owner-gate hardening

**Branch:** `agent/p1b-owner-gate-hardening`  
**Base:** `agent/p1b-inspector-bundle-staging@a7459be8ffad14a6bfaea04696750b1e18bd0b43`  
**Scope:** owner-local evidence workflow only; no occupancy, renderer, cooker, terrain or runtime changes.

## Question

Can the real P1B gate be completed without relying on fragile placeholder paths, manual bundle-directory discovery, or a falsely green Windows gate?

## Implemented boundary

### Windows gate

`tools/gate.ps1` now:

- configures a fresh Windows worktree when `build/CMakeCache.txt` is absent;
- checks the real CMake exit code for every target;
- requires each expected executable to exist;
- invokes executables through paths rooted at the repository;
- checks `test.exe` and smoke-process exit codes;
- cannot report `build ...: OK` after a failed or missing build.

### Source-frame CLI

`scan_source_frame_contract.py`:

- requires explicit source units, signed source axes and local origin;
- uses the fixed lab convention `right=+X`, `forward=-Z`, `up=+Y` unless overridden;
- derives handedness and the signed-permutation matrix;
- refuses an orientation mirror unless `--mirror-approved` is explicit;
- writes `confirmed=false` unless the owner explicitly passes `--confirmed`;
- validates the result through the canonical `scan_frames.py` contract.

It does not infer axes or scale from bounds.

### Owner gate runner

`scan_owner_gate.py`:

- discovers `inspection.json` files under an explicit local root;
- auto-selects a report only when every passing 7+7 candidate is byte-identical;
- stops when passing reports differ;
- requires a confirmed frame contract;
- publishes the existing immutable private/shareable bundle;
- invokes the independent verifier as a separate Python process;
- identifies exactly one shareable JSON for manual privacy review;
- writes a local receipt that omits paths, source hashes, names, bounds and coordinates;
- reports `P1B_BUNDLE_PASS` only after explicit acknowledgement of manual privacy review.

## Falsifiers

This package fails if any of the following is possible:

- a fresh worktree receives a green build result without configuration;
- a failed CMake process is reported as `OK`;
- two different passing inspection reports are selected automatically;
- a frame becomes confirmed without explicit owner action;
- a mirror transform passes without explicit approval;
- bundle verification is skipped;
- the receipt exposes source paths, coordinates, bounds, names or source hashes;
- occupancy or P2 starts before the real owner gate passes.

## Promotion gate

Before starting internal GLB↔PLY correspondence:

1. canonical dependency-free tests pass;
2. hosted Windows/Linux matrix passes;
3. local `tools/gate.ps1` passes in a clean worktree;
4. the real source-frame contract is owner-confirmed;
5. the real 7+7 bundle is published and independently verified;
6. `shareable/inspection.shareable.json` is manually reviewed;
7. the owner runner reports `P1B_BUNDLE_PASS`.
