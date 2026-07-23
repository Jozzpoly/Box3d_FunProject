# Drive the scan (P2B Scan Drive, M6 car)

Drive the **current M6 car** on the imported photogrammetry scan, with the scan itself as
static collision geometry and its own textures as the visuals.

**No code changes are needed per scan.** The sample renders and collides with whatever
pack `JOZZ_SCAN_PREVIEW_PACK` points at, so importing the next scan is the whole job.

## 1. Import a scan (once per scan)

Follow [IMPORT_A_NEW_SCAN.md](IMPORT_A_NEW_SCAN.md). The one-shot importer pins
`JOZZ_SCAN_PREVIEW_PACK` to the pack it just built, which is what this sample reads.

Per-scan leveling (`--level-x-deg` / `--level-z-deg`) matters here: the drive test is the
gravity check of that correction. If the car creeps downhill on flat-looking ground, the
scan is not level — fix it at import, not in the physics.

## 2. Drive it

**Build Release to drive.** Baking the scan's BVH is the one step that collapses under
an unoptimized build — measured on this 1.78M-triangle pack, same machine, same pack:

| Build | read | BVH bake | textures | **total load** |
|---|---|---|---|---|
| Debug | 0.53 s | **40.4 s** | 1.1 s | **42.0 s** |
| Debug + fast bake | 0.53 s | 12.6 s | 1.2 s | 14.4 s |
| **Release** | 0.09 s | **0.96 s** | 0.6 s | **1.7 s** |
| Release + fast bake | 0.09 s | 0.29 s | 0.6 s | 1.0 s |

The bake is **~42x slower in Debug**. Nothing else in the sample is meaningfully
config-sensitive, and the load runs on the main thread, so a Debug launch is a frozen
window for ~40 s that looks exactly like a hang.

```bash
cmake --build build --config Release --target samples
```

Launch `build/bin/Release/samples.exe`, then in the sample browser:
**Jozz Vehicle → "P2B Scan Drive (M6)"**. Debug still works and is still the right build
for stepping through physics — just expect the wait, and use the fast-bake toggle.

| Key | Action |
|---|---|
| `W` / `S` | drive forward / reverse |
| `A` / `D` | steer left / right |
| `Space` | brake |
| `T` | chase camera |
| `R` | restart the sample |
| `F` | snap the camera back to the car |

The camera opens on the car. `F` recovers it if you orbit away — deliberately *not* the
whole scan, which is a 1.2 km box the car is invisible in; that view is the **Kamera na
caly skan** button.

Panel controls: **Kamera na auto** / **Kamera na caly skan**, **Tekstury skanu** (textured
scan visuals), **Siatka kolizji** (show the raw collision mesh instead — use this to
inspect what the car is actually hitting), **Odwroc nawijanie trojkatow** (winding flip,
see troubleshooting), **Szybkie pieczenie BVH** (median-split bake, see the table above),
and **Upusc auto na srodku skanu** (re-drop the car at the scan centre).

## Car setup, and what survives a restart

The panel's **Ustawienia auta (M6)** section carries the workshop's own tabs
(Zawieszenie / Nadwozie / Naped / Kierownica). Live dials bite immediately; geometry
changes wait for **Zastosuj (przebuduj auto)** — only the car is rebuilt, the scan's BVH
is untouched.

Two files back it, split for the same reason the workshop splits its own:

| File | Holds | Written |
|---|---|---|
| `build/jozz_scan_drive_session.json` | vehicle tuning | on exit (and on `R`) |
| `build/jozz_scan_drive_view.txt` | view toggles + the drop point | on exit (and on `R`) |

`R` reconstructs the sample from scratch, so **without a save on the way out it would
throw away every dial** — the same failure the workshop's session file was invented to
stop. Tuning and view state stay in separate files so loading a preset can replace the
whole car without also flipping your texture toggles or teleporting you off your test
spot.

The scan session is deliberately **not** the workshop's. The first run seeds itself from
`build/jozz_vehicle_m6_session.json`, and after that the two move only on demand:
**Wczytaj sesje warsztatu** pulls, **Zapisz do warsztatu** pushes. Driving the scan can
never quietly rewrite the setup you built in the workshop.

## 3. Verify headlessly (no human at the keyboard)

Every hook mirrors the M6 lab's env registry.

| Variable | Effect |
|---|---|
| `JOZZ_SCANDRIVE_SETTLE_DUMP` | at step *N*, print one machine-checkable line and continue |
| `JOZZ_SCANDRIVE_AUTODRIVE` | `1` = full throttle straight ahead, no input needed |
| `JOZZ_SCANDRIVE_SPAWN_XZ` | `"x,z"` — drop the car somewhere specific |
| `JOZZ_SCANDRIVE_CAM` | `"yaw,pitch,radius,px,py,pz"` — aim the camera |
| `JOZZ_SCANDRIVE_FLIP_WINDING` | `1` = flip triangle winding at load |
| `JOZZ_SCANDRIVE_FAST_BAKE` | `1` = median-split BVH instead of binned SAH (faster bake, looser tree) |

The sample always prints its own load cost, so a new scan reports what it costs before
anyone has to guess:

```
JOZZ_SCANDRIVE_LOAD read=0.09s bake=0.96s textures=0.62s total=1.66s tris=1775775 build=optimized
```

Launch from a terminal and that line is also the answer to "is it hung, or just baking?".

Run a fixed number of frames and capture a screenshot with the host's own flags:

```bash
samples.exe --sample-name P2B --frames 340 --screenshot proof.png
```

`--sample-name` takes a **substring with no spaces** (`P2B`); a value containing a space
gets split by most shells and will silently select the wrong sample.

The dump line looks like this:

```
JOZZ_SCANDRIVE_SETTLE step=300 car=(46.77, 277.720, -60.50) ground_y=277.951 above=-0.231 wheel_contacts=2/4 tris=1775775 degenerate=5824
```

Read it as a pass/fail gate for a newly imported scan:

- `tris` — the whole scan is in collision (compare against the pack's triangle count).
- `wheel_contacts` — **1 or more means the car is on the surface.** `0/4` with a falling
  `car_y` means it fell through.
- `car_y` across two different steps — if it is unchanged, the car has settled; if it
  keeps dropping, collision is not holding.
- `degenerate` — informational; photogrammetry always has some (a few thousand is normal).

Note `above` (car minus ray-cast surface) can be slightly negative: the ray reports the
first surface under the sky at the chassis XZ, which may be a canopy, wall or grass tuft
*above* the car. Trust `wheel_contacts` and a stable `car_y` over `above`.

## Troubleshooting

**It takes forever to load / the window goes white and stops responding.** You are on a
Debug build; see the table in step 2. Build Release, or tick **Szybkie pieczenie BVH**.
Nothing is broken — the BVH is baking.

**The sample opens on an empty world, no car and no terrain.** Fixed 2026-07-23; if it
ever comes back, it is the camera, not the load. A scan lives wherever the survey put it —
this one floats 250-400 m up the Y axis and spans ~1.2 km — so a camera framed on the
world origin (the convention every procedural-plate sample uses) points at empty sky a
quarter kilometre below the terrain. The sample now frames the car once it exists. Check
the info panel first: if it reports tile meshes and a triangle count, everything loaded
and you are simply looking the wrong way — press `F`.

**The car falls through the ground.** The pack's triangle winding is inverted for the
single-sided mesh collider. Tick *Odwroc nawijanie trojkatow*, or set
`JOZZ_SCANDRIVE_FLIP_WINDING=1`. The current scan needs no flip; a future export might.

**"no pack selected".** Build exactly one pack, or point `JOZZ_SCAN_PREVIEW_PACK` at the
one you want. The sample uses the same discovery helper as the P2A preview, so if P2A
shows the scan, this will too.

**The car stops for no reason.** Look with *Siatka kolizji* on — scans contain poles,
walls and fences, and the car really does collide with them.

## Boundaries

The render-only **P2A** preview stays physics-free; it is guarded by
`tests/scan_pipeline/test_scan_preview_runtime_contract.py`. All collision lives in the
P2B sample, guarded by `tests/scan_pipeline/test_scan_drive_runtime_contract.py`. Both run
in the canonical suite:

```bash
python tools/scan_pipeline/run_p1_contracts.py
```
