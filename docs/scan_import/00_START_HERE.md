# Photogrammetry Import V2 — start here

**Status:** `P1A_SOURCE_INSPECTION_PASS / P1B_BUNDLE_HOSTED_CI_PASS / OWNER_LOCAL_GATE_REQUIRED`  
**Branch roboczy:** `agent/p1b-inspector-bundle-staging`  
**Baza:** `agent/p1b-world-import-contract-staging@eac2327589ad799e270ed760cf7288696f4f50c3`  
**Zakres:** offline contracts, privacy, persistence, tests and documentation only; no runtime C++, renderer, vehicle or Box3D behavior changes.

## Co jest prawdziwe

- P0 Windows build/validator/test/smoke został zaliczony.
- P1A inspector ma zielone dependency-free CI na Windows/Linux, Python 3.11/3.13, stdlib/NumPy.
- W sesji właściciela wykonano realny przebieg 7 GLB + 7 PLY: automatic evidence gate przeszedł i dwa przebiegi dały identyczne siedem artefaktów.
- Bounds nie dowodzą zgodności wnętrza geometrii. P2 nie jest jeszcze semantycznie odblokowane.
- `ScanSourcePackage` i `WorldImportProposal` istnieją jako neutralne kontrakty, ale nie są accepted world truth.
- Bundle oraz niezależny verifier przeszły pełne hosted CI 8/8 na Windows i Linux dla code headu `9718b89a46834a5e102ea3342fb54ab5d044c501`.

## Aktualny przepływ

```text
inspection.json + source-frame.json
→ ScanSourcePackage
→ WorldImportProposal (UNREVIEWED / BOUNDS_ONLY)
→ private/shareable projection
→ content-addressed staging
→ COMPLETE.json
→ immutable verified bundle
```

Bundle jest kopertą dowodową. Nie jest mapą, accepted patch, heightfieldem ani runtime cache.

## Kanoniczne testy

```powershell
python .\tools\scan_pipeline\run_p1_contracts.py
```

Po pobraniu brancha na Windows:

```powershell
.\tools\gate.ps1
```

## Lokalny bundle z istniejącego inspectora

Najpierw przygotuj prywatny `source-frame.json`. Następnie:

```powershell
python .\tools\scan_pipeline\scan_import_bundle.py `
  --inspection .\build\scan_pipeline\p1_dataset_approved\inspection.json `
  --frame-contract .\build\scan_pipeline\p1_source_frame.json `
  --output-root .\build\scan_pipeline\bundles `
  --package-id scan/model-skanu `
  --proposal-id proposal/model-skanu/revision-1 `
  --bundle-label model-skanu `
  --require-inspection-pass `
  --require-frame-confirmed
```

Wynik pozostaje pod `build/`. Nie commituj katalogu bundle ani prywatnego frame contract.

## Niezależna kontrola gotowego bundle’a

Po utworzeniu uruchom osobne, read-only sprawdzenie:

```powershell
python .\tools\scan_pipeline\scan_import_bundle_verify.py `
  .\build\scan_pipeline\bundles\<dokładna-nazwa-katalogu-bundle>
```

Kod wyjścia:

```text
0 = bundle kompletny i spójny
2 = brak, uszkodzenie, dodatkowy plik, zły hash lub niespójny kontrakt
```

Verifier nie poprawia i nie nadpisuje danych.

## Bramka `P1B_BUNDLE_PASS`

Hosted Python gate jest zielony. Pełna bramka wymaga jeszcze:

1. bundle’a z realnego 7+7 inspection;
2. niezależnego verify tego bundle’a;
3. realnego, potwierdzonego przez Jozza source frame;
4. ręcznego privacy review realnego shareable JSON;
5. lokalnego `tools/gate.ps1` bez regresji;
6. bezpiecznego receipt bez prywatnych współrzędnych i nazw.

## Następne po tej bramce

- internal occupancy correspondence GLB↔PLY;
- fixture: identyczne bounds, błędne wnętrze;
- adjacency/seam evidence;
- dopiero później P2 Diagnostic Preview.

## Stop conditions

Zatrzymaj implementację, jeżeli:

- raw GLB/PLY mają zostać skopiowane do bundle;
- shareable output zaczyna kopiować nieznane pola lub free-form warnings;
- bundle jest przedstawiany jako accepted world patch;
- Box3D/GPU/UI handle trafia do kontraktu;
- branch zaczyna ground extraction, cooker, renderer albo pełny Workbench JES.
