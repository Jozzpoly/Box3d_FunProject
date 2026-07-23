# Merge notes — scan import, P2B Scan Drive, M6 setup UI

Written when this work was committed, for whoever merges it later (including
future me). It records what landed, where a merge can actually collide, and the
invariants that must still hold on the other side.

## Branch topology as committed

Measured, not assumed:

```
origin/main                  is an ANCESTOR of this branch  (321 ahead, 0 behind)
origin/jozz-vehicle-sandbox-m0   is an ANCESTOR of this branch  (248 ahead, 0 behind)
```

So at the time of writing there is **nothing to merge in** — both other branches
are fully contained here, and merging this branch into either is a
fast-forward. If the other branch has since advanced, the rest of this file is
the map.

Re-check before merging:

```bash
git fetch origin '+refs/heads/*:refs/remotes/origin/*'
git rev-list --left-right --count HEAD...origin/main
```

## What landed

| Commit | Subject |
|---|---|
| `b212289` | chore: never commit Python bytecode from the scan pipeline |
| `4a74960` | feat(scan): textured P2A preview pack (JSPREV2) with per-scan leveling |
| `8cedb83` | feat(scan): one-command scan import (scan_oneshot) |
| `0082e03` | feat(scan): P2B Scan Drive — drive the M6 car on the real scan |

They are split by topic on purpose: each one cherry-picks independently if the
whole branch is not wanted.

## Conflict surface

**New files — zero conflict risk.** They do not exist on any other branch:

```
samples/jozz_vehicle_scan_geometry.{h,cpp}     pure pack -> geometry reader
samples/jozz_vehicle_scan_drive_lab.{h,cpp}    the P2B sample
samples/jozz_vehicle_m6_visual_skin.{h,cpp}    real car model on a live M6
samples/jozz_vehicle_m6_setup_ui.{h,cpp}       workshop tabs, reusable
tests/scan_pipeline/test_scan_drive_runtime_contract.py
tools/scan_pipeline/scan_oneshot.py
tests/scan_pipeline/test_scan_oneshot.py
docs/scan_import/{DRIVE_THE_SCAN,COLLISION_DRIVE_READINESS,IMPORT_A_NEW_SCAN}.md
```

**Shared files — the only three places a merge can collide.** All three changes
are pure additions, so a conflict here means "keep both", never "pick one":

| File | Change | If it conflicts |
|---|---|---|
| `samples/CMakeLists.txt` | +8 lines: four new file pairs in `SAMPLE_FILES` | Keep both lists. The new files belong in `SAMPLE_FILES` **only** — not `JOZZ_VEHICLE_CORE_FILES`, not the `jozz_vehicle_validation` target. A contract test enforces this. |
| `samples/sample_jozz_vehicle_lab.cpp` | +4 lines: one `#include` + one `RegisterSample("Jozz Vehicle", "P2B Scan Drive (M6)", ...)` | Keep both registrations. |
| `tools/scan_pipeline/run_p1_contracts.py` | +8 lines: two test files added to `TEST_FILES` | Keep both entries; the list is order-insensitive. |

`samples/jozz_scan_preview_pack.{h,cpp}` and `tools/scan_pipeline/scan_preview_pack.py`
were substantially rewritten for the textured v2 pack. If another branch also
touched them, prefer this branch's version and re-apply the other change on top:
the JSPREV2 format, the leveling correction and the reader in
`jozz_vehicle_scan_geometry.cpp` all have to agree byte-for-byte.

## Invariants a merge must not break

Each one is held by a test, so a bad merge fails loudly rather than silently.

1. **P2A stays render-only.** `jozz_scan_preview_{pack,lab}.{h,cpp}` must contain
   no body/shape/mesh creation. Guarded by
   `tests/scan_pipeline/test_scan_preview_runtime_contract.py`.
   Consuming P2A *from* P2B is fine — the test scans only P2A's own files.
2. **The geometry reader stays pure.** No physics, renderer or ImGui in
   `jozz_vehicle_scan_geometry.*`. That purity is the whole reason P2B can share
   P2A's bytes.
3. **The car is M6, never the archival M5.**
4. **New sample files are `samples` target only** — not the headless validator.
5. **Naming: the reader keeps the `jozz_vehicle_` prefix.** `jozz_scan_pack_*`
   is unclassified by `tools/project/repository_forensic_snapshot.py::_classify_path`
   and would break `test_current_tracked_tree_has_complete_coverage`. Any new
   file must classify — check before committing, including new docs (a
   repo-root `*.md` does **not** classify).
6. **Tuning survives "R".** `SelectSample` deletes a sample before constructing
   the replacement, so a ctor load needs a matching dtor save. P2B writes its
   own session file; it must never auto-save over the workshop's
   `build/jozz_vehicle_m6_session.json`.
7. **No scan data, source coordinates or textures in git.** Everything lives
   under the git-ignored `build/` tree.

Items 1–4 and 6 are covered by `test_scan_drive_runtime_contract.py`.

## Verify after merging

```bash
python tools/scan_pipeline/run_p1_contracts.py
```

Then the other two suites and a real build:

```bash
python -m unittest discover -s tests/project
```

```bash
cmake --build build --config Release --target samples
```

Baseline at commit time: **159 scan + 39 project + 25 automation** tests green.

Finally, prove the scan still drives — this needs no human at the keyboard:

```bash
samples.exe --sample-name P2B --frames 320 --screenshot proof.png
```

with `JOZZ_SCANDRIVE_SETTLE_DUMP=300 JOZZ_SCANDRIVE_AUTODRIVE=1`. Read the
`JOZZ_SCANDRIVE_SETTLE` line as described in
[scan_import/DRIVE_THE_SCAN.md](scan_import/DRIVE_THE_SCAN.md): a held wheel
contact and a stable `car_y` mean the merge kept collision intact.

Note `--sample-name` takes a **space-free** substring (`P2B`); a value with a
space gets split by the shell and silently selects the wrong sample.
