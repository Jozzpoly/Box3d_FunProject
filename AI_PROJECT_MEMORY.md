# AI Project Memory — Box3d_FunProject scan terrain

## Active objective

Show the real seven photogrammetry GLB tiles in the native render-only preview,
then obtain an honest owner visual review of orientation, scale, up axis, mirror
state, coverage and seams.

## Active integration branch

`agent/r1b-source-resolution-owner-integration`

Base: exact PR #5 head `f20357ba10618ddecfdd2e274e93917fe508a983`.
The branch intentionally does not contain PR #7 surface-evidence or derivative
catalog work.

## Current private evidence reported by the owner

- real 7 GLB + 7 PLY inspection: passed;
- real owner-confirmed source frame: passed;
- real P1B bundle and privacy acknowledgement: passed;
- real preview pack: not yet produced;
- native load and visual proof: not yet performed.

Private paths, coordinates, source file hashes and raw scan data must remain
outside GitHub and public logs.

## Current blocker and solution

The inspector accepts nested sources, while the original preview builder expects
one flat source directory. R1B introduces a shared source resolver which finds
exact assets recursively by kind, tile ID, byte length and SHA-256, rejects
missing/ambiguous/linked matches, and publishes a private content-addressed
canonical source view under ignored `build/`.

## Owner workflow rule

The owner is not a manual pipeline orchestrator. Technical state, artifact
binding, hashes and paths are persisted by `scan_real_terrain_flow.py`.
The intended owner interaction is one resumable command plus the genuine visual
decision.

## Hard capability boundary

Never claim `TERRAIN_VISIBLE_PASS` from tests, CI, a bundle, a preview pack or a
launched executable. It requires all seven real tiles loaded, manual visual
review and a same-revision restart. Accepted surface, collision and drive
readiness remain blocked and are not part of R1B.

## Branch rules

- never merge or close PRs without owner approval;
- never force-push or rewrite existing branch history;
- PR #7 remains frozen until exact visual proof;
- never use Git_Diff_Patcher_Bridge;
- work only through safe GitHub branch operations.
