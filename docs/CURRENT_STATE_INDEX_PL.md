# Current State Index — Jozz Vehicle Box3D Native

Date: 2026-07-08
Branch: `jozz-vehicle-sandbox-m0`

> **Jak używać tego pliku.** Front dla agentów to `README_FOR_AGENTS.md` (krótki,
> aktualny, jedno wejście). Ten plik to **ledger kamieni + zwalidowany stan**.
> Sekcje 2–4, 6, 7, 10 opisują szczegółowo **fundament M3B/M4/M5** — to HISTORIA
> baseline'u, nie aktualna architektura pojazdu. Aktualna fizyka = M7
> (`docs/M7_REAL_FORCES_FOUNDATION_PL.md`), aktualny rig+poza = M8
> (`docs/M8_SUSPENSION_RIG_REPAIR_PLAN_PL.md`). Dług/ryzyka: `docs/TECH_DEBT_PL.md`.

Status (2026-07-08): M2.5/M3A/M3B.3 + M4 + M5.2 zwalidowane; fizyka przebudowana
przez **M7 Real Forces Foundation** (wahacze jako CIAŁA z limitami zawiasów —
koniec rozpadu na skoczni; back-drivable rack — kontra z castera/sił kontaktu,
programowy self-align USUNIĘTY; napęd momentem; ARB per oś zamiast upright assist
default OFF; opór aero; trailing arm z kontraktu One_Sided_wheel_mount).
**M8 (2026-07-07/08):** rig modelu na żywych ciałach + wahacze wpięte w
authored-sockety; **poza domyślna jako świadome ustawienie** (`restArmDroopDeg` +
`suspensionPreload`, opadające wahacze, kompensacja bump-steer, sufit droop 15°);
system zrzutów D3D11→PNG + `tools/quad_shot.ps1`; **przebudowa UI (polski, 6
zakładek) + system presetów pojazdu** (`jozz_vehicle_m6_config_io`,
`assets/vehicle_presets/`, auto-zapis sesji naprawiający „R" kasujące strojenie).
Walidacja headless + test.exe + boot smoke zielone; czeka na ręczny test feelu
Jozza. Soft-tire nadal świadomie odłożona.

## 1. Current active samples

```text
Category: Jozz Vehicle
Sample:   M6 Suspension Rig Lab      <- multi-body zawieszenie na fundamencie M7
                                        (UI w zakładkach; tył = trailing arm
                                        z modelu Jozza, przód = wishbone)
Source:   samples/jozz_vehicle_m6_rig_lab.cpp + samples/jozz_vehicle_m6_suspension_rig.cpp
          + samples/jozz_vehicle_m7_suspension_import.cpp

Sample:   M5 First Drivable          <- pierwszy jeżdżący pojazd, baseline strut
Source:   samples/jozz_vehicle_m5_drivable_lab.cpp + samples/jozz_vehicle_m5_vehicle.cpp

Sample:   Lab M2 Primitive Corner    <- izolowany narożnik, nadal aktywny
Source:   samples/sample_jozz_vehicle_lab.cpp + samples/jozz_vehicle_primitive_corner_lab.cpp
Panel:    Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 foundation debug
```

The corner lab remains the isolated tuning environment; M5 is the drivable
four-corner vehicle built on the same rest-anchor model. See
`docs/archive/M5_FIRST_DRIVABLE_PL.md` and `docs/adr/0005-m5-first-drivable-before-m4c.md`.

The older smoke test still exists:

```text
Jozz Vehicle / Lab M1 Smoke
```

## 2. Current validated state

Validated by Jozz:

```text
M2.5 primitive one-corner wheel-joint lab works
M3A asset-derived primitive defaults work
M3B semantic preview anchoring fix works
M3B.2-prep runtime audit metadata path works
M3B.2 static visual-only wheel mesh proof exists
M3B.2.1 baseColor texture proof exists for the same static wheel mesh
M3B.3 visual-only wheel mesh attach follows the primitive wheel body
M3B.3 hardening centers the primitive collision wheel and can hide its debug shape
M4F.1 asset contract runtime resolves sidecar bindings from source glTF
M4A one-sided suspension mount visual proof exists
M4B narrow moving endpoint debug preview exists
2026-07-05 Jozz screenshots confirm suspension model, texture, transparency and helper-line visibility in the active lab
```

Implemented 2026-07-05, machine-validated, awaiting Jozz manual feel check:

```text
Cleanup: shared jozz_vehicle_json helpers, shared asset path resolver,
         single built-in fallback table, upstream CMakeLists layout restored
M5 vehicle physics module (jozz_vehicle_m5_vehicle.*): dynamic chassis,
         four b3WheelJoints on the M2.4 rest-anchor model, front steering,
         AWD/RWD drive, brake, optional upright assist
M5 headless drive smoke in jozz_vehicle_validation.exe: settle/drive/steer/
         brake with assertions (caught collapsed suspension and inverted
         steering sign during development)
M5 sample "Jozz Vehicle / M5 First Drivable": W/S/A/D/Space/T input, ramp +
         washboard course, live tuning, glTF wheel visuals on all corners
```

M5.1 feel tuning (2026-07-05, after Jozz's ~20 min playtest), see
`docs/archive/M5_1_FEEL_TUNING_HANDOFF_2026_07_05_PL.md` (analysis/plan) and
`docs/archive/M5_1_FEEL_TUNING_IMPLEMENTATION_REPORT_PL.md` (what shipped):

```text
Reproduced the reported broken stationary steering in the headless smoke
  FIRST (0.0 deg of 32 deg target at the old 80 N*m default) before fixing,
  per the project's evidence-before-fix discipline
Fixed: maxSteeringTorque 80->700, steeringHertz 8->14 (stationary tire
  parking-torque was undertorqued by an order of magnitude; scrub radius in
  this rig is actually zero, so that was not the mechanism)
Narrowed chassis half-width 0.80->0.55 (was clipping into the tire sidewalls)
Replaced the default camera with a proper rear chase view and reset
  m_thirdPerson explicitly; added an "Invert steering" checkbox as a
  safety net since screen orientation could not be verified visually
  this session; the steering sign math itself was NOT changed (verified
  correct against this codebase's own right = up x forward convention)
Chassis/track/wheelbase/mass geometry moved to a pending/Apply pattern
  (mirrors the corner lab), and all previously-live sliders widened
  substantially for stress testing per Jozz's request
Extracted jozz_vehicle_m5_test_course.h/.cpp: 2x ground, 4 ramps, 2
  washboard lanes, a rough-terrain heightfield zone, 14 scattered props
  with a "Reset props" button
NOT touched: the speed-dependent wheel "teleporting" instability - needs
  Jozz's eyes on the corrected build; first diagnostic is the existing
  Solver panel's Sub-steps slider (already on by default, no code needed)
```

Latest observed runtime metadata state:

```text
M3B metadata: runtime audit
metadata: loaded runtime asset audit report
source: ../../assets/reports/asset_audit_latest.json
```

M3B.2 renders one static `Offroad_Big_Wheels.gltf` mesh primitive at a fixed debug origin. M3B.2.1 adds the narrow baseColor texture path for that same proof. M3B.3 reuses the same mesh and draws it through `DrawAtTransform(...)` so it follows the primitive wheel body. M3B.3 hardening centers the primitive cylinder on the wheel body origin and adds a panel toggle for the orange primitive wheel debug shape; hiding it does not disable the physics body, primitive collision, or wheel joint. The mesh remains visual-only and is not a full glTF/material/skin/animation/collision importer.

## 3. Authoritative physics baseline

Current model:

```text
Body A: static chassis/root debug rig
Body B: dynamic primitive wheel body
Joint:  b3WheelJoint
```

Rules:

1. `b3WheelJoint` has implicit spring rest at `translation = 0`.
2. Frame A is the rest wheel-center anchor on chassis/root.
3. Frame B is the wheel center/body origin.
4. `Rest drop` is explicit/tuned.
5. Visual sockets are not automatically physics frames.
6. Primitive wheel collision remains centered cylinder/hull, not glTF mesh.
7. M3B semantic preview is debug-only and does not drive physics.
8. Runtime metadata is a data source only, not a renderer.

Do not return to the historical M2.3 visual-mount-as-frame-A model.

## 4. Current separation model

Keep these separate:

```text
Structural setup
  - rig height
  - rest drop
  - wheel radius
  - wheel width
  - collision toggle
  - pending values + Apply rig rebuild
  - primitive wheel cylinder is centered on body origin because Frame B is wheel center

Live root stress test
  - realtime chassis/root movement
  - slider + Q/E
  - no body/joint rebuild

Semantic preview
  - debug overlay only
  - wheel schematic follows wheel/body
  - suspension schematic follows chassis/root
  - no physics authority

Runtime metadata
  - reads assets/reports/asset_audit_latest.json when reachable
  - falls back safely when not reachable

Static visual proof
  - loads a narrow subset of Offroad_Big_Wheels.gltf
  - reads TEXCOORD_0 and one pbr baseColorTexture PNG data URI
  - decodes PNG to RGBA8 through isolated Windows/WIC helper
  - draws one mesh at a fixed debug origin
  - remains available as debug/comparison and is not attached to physics

Attached visual proof
  - reuses the same Offroad_Big_Wheels.gltf mesh
  - draws through JozzVehicleVisualMesh::DrawAtTransform(...)
  - follows the primitive wheel body transform
  - applies a local render-only correction for the current authored wheel orientation and center
  - centers against loaded mesh bounds when available, with semantic points as fallback
  - no multi-material/normal-map/metallic-roughness/skin/animation/collision/full importer
  - no physics authority or mesh collision

Primitive wheel debug shape
  - orange Box3D debug shape for the primitive collision wheel
  - can be hidden independently from the attached visual mesh through the debug adapter hidden-shape path
  - hiding it does not change physics/collision
  - hidden mode leaves no collision mesh/edge overlay

Asset contract runtime
  - reads assets/contracts/*.asset.json, not assets/reports/*latest*, for M4 runtime binding data
  - resolves nodeIndexHint/nodePathHint/nameHint against the source glTF
  - composes node parent transforms before resolving positions
  - keeps duplicate node-name warnings visible

Suspension visual foundation
  - loads one_sided_wheel_mount.asset.json and One_Sided_wheel_mount.gltf
  - draws a visual-only one-sided suspension mount proof
  - overlays contract wheel center, chassis mount, travel top and travel bottom
  - draws debug-only moving damper/cardan endpoints
  - does not change b3WheelJoint, Frame A/Frame B, restDrop, or primitive collision
```

Known visual debt after M3B.2.1:

```text
alpha-masked tire tread affects the lit pass but not the shadow caster yet
wheel shadow therefore does not show tread/cutout structure
inner rim/felga shows visible banded/striped shading in close-up screenshots
```

These are not blockers for M3B.3 visual-only attach. Revisit them during a focused render/material polish pass. For the rim banding, first isolate whether the source is shadow acne/self-shadowing, imported vertex normals, material roughness/specular mismatch, or low-resolution nearest-filter texture detail.

## 5. Kolejność czytania

Zastąpione. Mapa dokumentacji (co jest aktualne, a co historią) i minimalna
lista wejściowa są teraz w **`README_FOR_AGENTS.md` §8**. Ta 40-punktowa lista
istniała tu i w README jednocześnie, spóźniała się o kamień i miała błąd
numeracji — utrzymywanie jej w dwóch miejscach było źródłem rozjazdu. Aktualny
kod pojazdu = M7 (fizyka) + M8 (rig/poza/UI/presety); reszta plików w `docs/` to
historia (patrz sekcja 6 i README §8).

## 6. Historical docs

These are history, not active architecture:

```text
docs/archive/M2_PRIMITIVE_CORNER_LAB_PL.md
docs/archive/M2_1_PRIMITIVE_CORNER_AXIS_FIX_PL.md
docs/archive/M2_2_CENTERED_WHEEL_PIVOT_AND_RIG_CONTROLS_PL.md
docs/archive/M2_3_SUSPENSION_MOUNT_MODEL_PL.md
```

Current authority superseding them:

```text
M2.4 — correct rest-anchor model
M2.5 — live root + pending/committed separation
M3A — asset-derived primitive defaults
M3B.1 — semantic preview overlay + ownership fix
M3B.2-prep — runtime audit metadata without mesh rendering
M3B.2 — static visual-only wheel mesh proof at fixed debug origin
M3B.2.1 — baseColor texture proof on the same static wheel mesh
M3B.3 — visual-only wheel mesh attached to the primitive wheel body
```

## 7. Current assets

Source models:

```text
assets/source/Asset_Dumper.gltf
assets/source/Cardan_shaft.gltf
assets/source/Offroad_Big_Wheels.gltf
assets/source/One_Sided_wheel_mount.gltf
```

Current status:

- research/startup assets, not final production contracts;
- duplicate node names exist;
- orientation is not final;
- scale is prototype-only;
- marker/socket naming is useful but not enough alone;
- importer must use stable node identity/path/parent chain and composed transforms.

Runtime metadata currently reads when reachable:

```text
assets/reports/asset_audit_latest.json
```

Metadata code:

```text
samples/jozz_vehicle_asset_metadata.h
samples/jozz_vehicle_asset_metadata.cpp
```

Visual proof code:

```text
samples/jozz_vehicle_primitive_corner_lab.h
samples/jozz_vehicle_primitive_corner_lab.cpp
samples/jozz_vehicle_visual_mesh.h
samples/jozz_vehicle_visual_mesh.cpp
samples/jozz_vehicle_image_decode.h
samples/jozz_vehicle_image_decode.cpp
samples/jozz_vehicle_validation.cpp
```

## 8. Current hotkeys

Lab M2 Primitive Corner:

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

M5 First Drivable:

```text
W/S    drive forward/reverse
A/D    steer left/right
Space  brake
T      third-person camera toggle
```

Do not use `[` or `]`; they are global sample-switching keys. Details in
`docs/HOTKEY_AUDIT_PL.md` (updated 2026-07-05 for A/D/T).

## 9. Validation commands

From repo root:

```powershell
cmake --build --preset windows-debug --target test
cmake --build --preset windows-debug --target samples
cmake --build --preset windows-debug --target jozz_vehicle_validation
build\bin\Debug\test.exe
build\bin\Debug\jozz_vehicle_validation.exe
build\bin\Debug\samples.exe --sample 96 --frames 300
```

Environment note (2026-07-05): the historical `cmd /c "set PATH=& ..."`
wrapper silently does nothing when invoked from Git Bash (cmd starts
interactively and exits 0 without running the command). Call cmake directly;
use the wrapper only in environments where MSBuild actually fails on
duplicate `Path/PATH` keys, and verify output/binary timestamps either way.

Run `py tools\asset_audit.py` and `py tools\asset_contract_audit.py` only when intentionally regenerating repo reports.

Manual sample check:

```text
Open:  Jozz Vehicle / Lab M2 Primitive Corner
Panel: Jozz Vehicle Lab M2.5 + M3A/M3B.3 + M4 foundation debug
```

Regression check:

```text
sample opens
panel shows metadata status
HUD shows M3B metadata runtime audit or fallback
Reload metadata + reset defaults is safe
W/S motor works
Space brake works
Q/E live root works
Apply rig rebuild works
M3B semantic preview draws
semantic preview does not change physics
M3B.2.1 static textured wheel proof toggles
static wheel mesh is visible but not attached to physics
M3B.3 attached textured wheel visual toggles
attached wheel mesh follows primitive wheel body transform
primitive wheel debug shape toggle hides only the orange collision debug visual
hidden primitive wheel debug shape leaves no thin collision mesh/edge overlay
UI texture status shows loaded baseColor or solid fallback reason
M4 contract runtime reports sidecar + glTF status
M4A suspension mount visual toggles independently
M4A contract points show wheel center/chassis mount/travel axis
M4B moving endpoint preview follows wheel travel only on wheel-side points
```

## 10. Current implementation status

M3A does:

```text
wheel radius/width from asset audit metadata
suspension total travel as asset hint
rest drop explicit/tuned
```

M3B.1 does:

```text
semantic marker preview overlay
wheel preview anchored to wheel/body
suspension preview anchored to chassis/root
```

M3B.2-prep does:

```text
runtime load attempt for assets/reports/asset_audit_latest.json
fallback to built-in audited metadata
metadata status shown in UI/HUD
M3A defaults and M3B preview read through metadata source
```

M3B.2 does:

```text
static Offroad_Big_Wheels glTF mesh proof
fixed debug origin
visual-only draw path through the sample renderer geometry registry
no physics attachment
```

M3B.2.1 does:

```text
extends MeshVertex with UV
adds optional baseColor texture binding to geom shader path
loads TEXCOORD_0 and one pbr baseColorTexture PNG data URI
decodes PNG to RGBA8 through Windows/WIC helper
keeps solid-color fallback if texture decode/upload fails
no physics attachment
```

M3B.3 does:

```text
attached Offroad_Big_Wheels glTF mesh proof
visual-only draw path through JozzVehicleVisualMesh::DrawAtTransform(...)
uses primitive wheel body transform as the base
uses a render-only correction to center/orient the current authored wheel
centers primitive wheel cylinder on body origin / frame B
allows hiding the primitive wheel debug shape while physics stays active
keeps the M3B.2.1 fixed static proof as comparison/debug
no mesh collision
no full rig/importer
```

Foundation Grounding V2 does:

```text
extracts the primitive corner lab from the sample registration file
adds DrawAtTransform for future visual-only wheel attach
adds jozz_vehicle_validation CLI metadata/defaults check
hardens MeshVertex layout offsets with offsetof
```

M4 Foundation does:

```text
loads one_sided_wheel_mount.asset.json as a runtime sidecar contract
resolves contract bindings against One_Sided_wheel_mount.gltf node transforms
validates required suspension roles in jozz_vehicle_validation.exe
draws One_Sided_wheel_mount.gltf as visual-only M4A suspension mount proof
draws contract point diagnostics for wheel center, chassis mount, and travel axis
draws M4B narrow moving endpoint preview for damper/cardan roles
keeps audit reports as diagnostics, not M4 runtime contract source
does not change b3WheelJoint, restDrop, primitive collision, or physics authority
```

Still not implemented:

```text
mesh collision
steering
four-corner vehicle
final visual rig
skinning/animation
full glTF renderer/material importer
procedural damper/cardan/chassis visual parts
```

## 11. Next pass

```text
2026-07-06 (later): M7 Real Forces Foundation implemented after Jozz's M6
  drive feedback ("suspension breaks on the jump", "slide self-align feels
  scripted"). Arms are BODIES with hinge stops now (landing probes at 2.0 m
  and 3.5 m drops pass with 0.6 deg worst camber), the rack is back-drivable
  (counter-steer measured -11 deg from contact forces with a FREE rack,
  -3 deg with a frozen one - the proof it is mechanical), drive is torque-
  based (wheelspin exists above grip torque), ARB + aero replace the upright
  crutch, and the rear axle runs Jozz's One_Sided_wheel_mount as a trailing
  arm imported from the sidecar contract with the model riding the live arm
  body. Machine-validated; awaiting Jozz's manual drive (checklist:
  docs/M7_REAL_FORCES_FOUNDATION_PL.md section 9).
Next gates (szczegóły w raporcie M7 sekcja 10):
  M7.1  ZROBIONE w M8 (2026-07-07/08): per-part rig na żywych ciałach +
        lustro prawej strony + wpięcie wahaczy w authored-sockety.
  M7.2  wishbone hardpoints z markerów assetu (import wypełnia struct) — TODO
  M7.3  drivetrain: dyfry, split momentu, krzywa engine-brake — TODO
  M7.4  model opony (krzywa poślizgu, wrażliwość na obciążenie) — TODO (soft-tire)
  M7.5  analogowe wejście kierownicy + miękkie przejście hands-on/off — TODO
```

Zrobione w M8 (poza M7.1 wyżej), czeka na ręczny test feelu Jozza:
```text
- Poza domyślna jako świadome ustawienie: restArmDroopDeg (geometria) +
  suspensionPreload (docisk sprężyny). Opadające wahacze zamiast wyginania w
  górę. Kompensacja bump-steer drążka. Sufit droop 15° (over-center > 16°).
- System zrzutów D3D11->PNG (--screenshot) + tools/quad_shot.ps1 (moje oczy).
- Przebudowa UI: polski, 6 zakładek (Zawieszenie/Nadwozie/Napęd/Kierownica/
  Świat/Debug), poprawka fontu (Segoe UI + /utf-8).
- System presetów pojazdu: jozz_vehicle_m6_config_io (JSON save/load całego
  configu), assets/vehicle_presets/{uliczny,drift,offroad}.json, auto-zapis
  sesji (build/) naprawiający "R" które kasowało strojenie.
Odłożone świadomie: dwa boczne dampery; agresywna poza >16° (przeprojektowanie
  Ackermanna) — patrz docs/TECH_DEBT_PL.md.
```

Ważne lekcje inżynierskie z M6+M7 (pełne opisy w raportach M6 sekcje 2/4/5
i M7 sekcje 1/5):

```text
- b3DefaultShapeDef() ma categoryBits = WSZYSTKIE bity (nie 0x1 jak Box2D);
  wąskie maski wymagają tagowania OBU stron pary (teren 0x2 / obiekty 0x1)
- małe ciała strukturalne (rack, zwrotnica, wahacze) BEZ shape'ów +
  b3Body_SetMassData, inaczej solver flaguje je jako "fast" i CCD vs grunt
  potrafi ubić TOI assert
- M7: światy pojazdów jeżdżą z b3World_EnableContinuous(false) - powyżej
  ~15 m/s KOŁA same są "fast", a sweep toczącego się koła startuje w
  kontakcie i głodzi walidację debug TOI (distance.c:1798); świat nie ma
  cienkiej geometrii, więc CCD nic nie kupuje (silnik NIETKNIĘTY)
- M7: wahacze z prętów distance-joint mają GAŁĄŹ LUSTRZANĄ - twarde
  lądowanie potrafi przerzucić narożnik w odbite rozwiązanie ("złamane
  zawieszenie"); ciała wahaczy na zawiasach z limitami mają jedną gałąź
- M7: hertz sprężyny distance jointa na smukłym OBRACAJĄCYM SIĘ ramieniu
  ma masę efektywną kilka kg (człon rotacyjny (r×û)ᵀI⁻¹(r×û) dominuje) -
  trailing arm liczy kompensację z docelowej sztywności NA KOLE
- M7: wyzerowanie castera NIE gasi samo-prostowania kół w ślizgu - scrub
  radius i offset masy od osi zwrotu też prostują (falsyfikacja = zamrożony
  rack, nie zerowy caster)
- pełny Ackermann wpycha trapez w martwy punkt (over-center) przy pełnym
  skoku racka -> ackermannFraction (default 0.6)
- phased-union kół (nakładane obrócone cylindry) OBALONE pomiarem: skoki
  kontaktu między hullami gubią warm-start, toczy się gorzej niż cylinder
- rack potrzebuje motor-servo z limitem siły (parking torque, lekcja M5.1);
  M7: i musi umieć PUŚCIĆ (spring off + tarcie), żeby caster mógł pracować
```

## 12. No-go list for Codex

Do not:

- start full glTF renderer;
- build full vehicle assembly;
- replace primitive collision with mesh collision;
- rewrite Box3D internals;
- add new hotkeys without audit;
- merge visual rig marker positions directly into physics joint frames;
- mix pending structural setup with runtime live root controls;
- treat M2.1/M2.2/M2.3 as current architecture;
- derive `restDrop` directly from visual chassis/wheel sockets;
- treat M3B schematic preview as final import transform;
- let suspension semantic preview become wheel-owned again;
- hide uncertain importer/rig decisions behind pretty visuals.
