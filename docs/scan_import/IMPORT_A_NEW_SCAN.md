# Import a new scan (one command)

This runbook takes a **raw photogrammetry dataset** (GLB + PLY trees, e.g. Bentley
ContextCapture / mip-tile output) all the way to the **textured source preview**
rendering in the native Box3D host — using the existing, unchanged pipeline. No new
importer.

Everything lives under the git-ignored `build/` tree. Nothing is uploaded anywhere
(`PRIVATE_LOCAL_ONLY`). Source GPS/ENU coordinates never leave the local machine.

## Prerequisites

- `samples.exe` built (`build/bin/**/samples.exe`).
- A local dataset directory containing the scan's GLB and PLY files (searched
  recursively). Keep it **outside** the repo (e.g. a sibling `_private_scan_local/`).
- Python on PATH. An image backend (OpenCV **or** Pillow) is required to *build* the
  textured pack; import/verify themselves stay dependency-free.

## The command (two phases)

The importer stops once for a **human privacy review**, then finishes on a second run.

**Phase 1 — build + stop at the privacy gate:**

```bash
python tools/scan_pipeline/scan_oneshot.py --dataset ..\_private_scan_local
```

This inspects the dataset, derives + confirms the source→lab frame, builds and
independently verifies one evidence bundle, and prints a single **shareable review
file** path (`REVIEW_ONLY_THIS_FILE`). It then STOPS. Open that one file and confirm
it contains no private coordinates, absolute paths, hashes, RGB or textures.

**Phase 2 — after eyeballing that file, launch:**

```bash
python tools/scan_pipeline/scan_oneshot.py --dataset ..\_private_scan_local --reviewed-privacy
```

`--reviewed-privacy` is **your** human decision. It unlocks the `P1B_BUNDLE_PASS`
receipt, builds the exact preview pack, launches the native window, and pins the
persistent `JOZZ_SCAN_PREVIEW_PACK` pointer at the new scan so a **double-clicked**
`samples.exe` (any CWD, later session) shows it too.

Re-running is safe/idempotent: the frame contract is overwritten, the bundle is
content-addressed (same input → same bundle), the receipt is rewritten.

## Viewing

In Box3D: sample browser → **Jozz Vehicle → P2A Source Visual Preview**. Use
"Frame whole preview" to fit the camera.

## Per-scan options (defaults suit a 7-tile ENU / real-metres / Z-up scan)

| Option | Default | When to change |
| --- | --- | --- |
| `--expected-glb` / `--expected-ply` | `7` / `7` | Different tile count. Import STOPS unless it finds exactly this many complete GLB+PLY pairs. |
| `--source-units-per-meter` | `1.0` | Source not already in metres. |
| `--source-right/-forward/-up` | `+X` / `+Y` / `+Z` | Non-ENU axis convention. |
| `--local-origin-source X Y Z` | `0 0 0` | Recenter about a specific source point (kept private). |
| `--no-launch` | off | Build the pack without opening the window (and without pinning the env). |

> **Leveling / orientation calibration is per-scan** — mip-tile exports are often
> tilted a few degrees. Each scan can have a *different* tilt, so it is never a fixed
> constant. See the leveling section below once you have measured this scan's values.

## What this does NOT grant

The preview is evidence-only. It does not assert accepted-world, collision, or
GLB↔PLY correspondence. It never grants `TERRAIN_VISIBLE_PASS` automatically — that
remains a human visual review (orientation, scale, coverage, seams).
