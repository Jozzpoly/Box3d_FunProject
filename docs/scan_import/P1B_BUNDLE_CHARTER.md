# P1B — inspector bridge and evidence bundle charter

**Branch:** `agent/p1b-inspector-bundle-staging`  
**Base:** `agent/p1b-world-import-contract-staging@eac2327589ad799e270ed760cf7288696f4f50c3`  
**Scope:** offline Python contracts, persistence and privacy only.

## Question

Can one deterministic P1 inspection be converted into a source package and an explicitly unreviewed world proposal, while keeping private evidence local and producing a small shareable projection that cannot silently leak source coordinates, hashes, free-form warnings or semantic place names?

## Intended flow

```text
inspection.json + source-frame.json
→ validate exact source revision
→ ScanSourcePackage
→ WorldImportProposal (UNREVIEWED / BOUNDS_ONLY)
→ private/shareable document split
→ content-addressed staging directory
→ COMPLETE.json written last
→ atomic publication of one immutable bundle
```

The bundle is a persistence envelope. It is not an authored world document, terrain format, accepted patch or collision cache.

## Files in one complete bundle

```text
private/inspection.private.json
private/source_frame.json
private/source_package.json
private/world_import_proposal.json
shareable/inspection.shareable.json
COMPLETE.json
```

`COMPLETE.json` contains the exact sorted file set, byte lengths, SHA-256 values, privacy class and cross-document IDs/hashes.

## Required invariants

1. The private inspection remains byte-independent canonical JSON but semantically unchanged.
2. The source package revision is derived from exact GLB/PLY hashes and normalized frame contract.
3. The proposal references the exact package revision and private inspection content hash.
4. The proposal remains `UNREVIEWED`, `BOUNDS_ONLY`, with no manual decisions.
5. The shareable projection is built from an explicit allow-list.
6. Shareable output omits source bounds, center deltas, file hashes, file labels, free-form warnings and the source-provided package name.
7. Shareable privacy says `georeferencingStatus = NOT_ASSERTED`; it does not claim knowledge it cannot prove.
8. Non-finite JSON, duplicate object keys, symlinks, extra files and incomplete directories are rejected.
9. A bundle with inconsistent but individually re-hashed documents is rejected before publication.
10. Re-running identical input returns the same content-addressed directory.
11. Existing corrupted content is never overwritten as if it were valid.
12. `COMPLETE.json` is written only after every payload file has been flushed.

## Positive evidence

- confirmed source frame + passing inspection produces a verifiable bundle;
- historical `strong-match` becomes `bounds-strong-match` in shareable/proposal evidence;
- identical documents are idempotent;
- verifier reconstructs the canonical shareable projection from the private inspection.

## Adversarial evidence

Tests must reject:

- private package/place names in shareable output;
- source coordinates, source hashes or free-form warning text in shareable output;
- one modified payload file;
- an unexpected extra file;
- a directory without `COMPLETE.json`;
- duplicate JSON keys;
- `NaN`/`Inf` before hashing;
- unsafe output labels/path traversal;
- proposal/source revision mismatch even after recomputing proposal hash;
- noncanonical shareable content even if all file hashes would be recomputed;
- an existing corrupted content-addressed bundle.

## Acceptance gate

`P1B_BUNDLE_PASS` requires:

- new bundle tests PASS in the repository runner;
- Python 3.11 and 3.13 on Windows and Linux;
- stdlib and NumPy matrix remains green;
- real 7+7 `inspection.json` can create and verify a local bundle;
- shareable output from the real scan passes a manual privacy review;
- `tools/gate.ps1` still passes on the owner Windows checkout.

## Stop conditions

Stop instead of extending this package if:

- raw GLB/PLY are about to be copied into the bundle;
- shareable output needs a deny-list instead of an allow-list;
- a bundle is treated as an accepted world patch;
- renderer, Box3D, GPU or UI types enter the schema;
- occupancy correspondence, ground extraction or P2 rendering starts inside this branch.

## Next consumer

After this gate passes, the next separate package may add GLB↔PLY internal occupancy correspondence and the wrong-interior/same-bounds sabotage fixture. It must consume the immutable source package/proposal boundary rather than bypassing it.
