# Photogrammetry Import V2 — start here

**Status:** `P1A_REAL_INSPECTION_LOCAL_PASS / P1B_OWNER_GATE_HARDENING`  
**Branch roboczy:** `agent/p1b-owner-gate-hardening`  
**Baza:** `agent/p1b-inspector-bundle-staging@a7459be8ffad14a6bfaea04696750b1e18bd0b43`  
**Zakres:** owner-local contracts, privacy, persistence, tests and gate reliability only; no occupancy, runtime C++, renderer, vehicle or Box3D behavior changes.

## Co jest prawdziwe

- P0 Windows build/validator/test/smoke przeszedł lokalnie na dokładnym headzie `a7459be...`.
- Pełny dependency-free runner P1/P1B przeszedł lokalnie: 77/77 na tym samym headzie.
- Dwa realne przebiegi 7 GLB + 7 PLY wygenerowały byte-identyczne `inspection.json`.
- Oba raporty mają 7 GLB, 7 PLY, 7 par, automatic evidence gate `PASS` i status `compatible-review`.
- Bounds nadal nie dowodzą zgodności wnętrza geometrii. P1C/occupancy i P2 pozostają zablokowane.
- Bundle, independent verifier i private/shareable projection mają zielone synthetic CI.
- Brakuje realnego owner-confirmed source frame, realnego bundle'a i ręcznego privacy review.

## Przepływ prawdy

```text
private GLB/PLY
→ inspection.json
→ explicit owner-confirmed source-frame.json
→ ScanSourcePackage
→ WorldImportProposal (UNREVIEWED / BOUNDS_ONLY)
→ private/shareable bundle
→ independent verifier
→ manual review of one shareable JSON
→ P1B_BUNDLE_PASS
```

Bundle jest kopertą dowodową. Nie jest mapą, accepted patch, heightfieldem ani runtime cache.

## Kanoniczne testy

```powershell
python .\tools\scan_pipeline\run_p1_contracts.py
.\tools\gate.ps1
```

`gate.ps1` konfiguruje świeży Windows worktree automatycznie i zatrzymuje się po każdym realnym błędzie CMake lub brakującym executable.

## Krok 1 — sprawdź realne raporty

Podaj katalog zawierający prywatne outputy inspectora:

```powershell
python .\tools\scan_pipeline\scan_owner_gate.py inspect `
  --inspection-root <PRIVATE_SCAN_PIPELINE_OUTPUT_ROOT>
```

Runner wybierze raport automatycznie tylko wtedy, gdy wszystkie pasujące raporty 7+7 są byte-identyczne. Różne raporty są stop condition.

## Krok 2 — przygotuj source frame

Najpierw ustal faktyczne:

- source units per meter;
- signed source axes `right`, `forward`, `up`;
- local source origin;
- czy transformacja zachowuje orientację, czy wymaga mirroru.

Zobacz wymagane argumenty:

```powershell
python .\tools\scan_pipeline\scan_source_frame_contract.py create --help
```

Generator sam wylicza handedness i `axisMatrix`. Bez jawnego `--confirmed` zapisuje kontrakt niepotwierdzony. Mirror wymaga osobnego `--mirror-approved`.

Walidacja gotowego kontraktu:

```powershell
python .\tools\scan_pipeline\scan_source_frame_contract.py validate `
  .\build\scan_pipeline\p1_source_frame.json `
  --require-confirmed
```

Nie commituj source-frame contractu. Pozostaje pod ignorowanym `build/`.

## Krok 3 — owner gate

Po potwierdzeniu frame contractu:

```powershell
python .\tools\scan_pipeline\scan_owner_gate.py finalize `
  --inspection-root <PRIVATE_SCAN_PIPELINE_OUTPUT_ROOT> `
  --frame-contract .\build\scan_pipeline\p1_source_frame.json
```

Runner:

1. wybierze byte-identyczny raport 7+7;
2. zbuduje content-addressed bundle;
3. uruchomi osobny read-only verifier;
4. zapisze privacy-safe local receipt;
5. wskaże dokładnie jeden plik `shareable/inspection.shareable.json` do ręcznej kontroli.

Pierwszy przebieg kończy się statusem:

```text
TECHNICAL_PASS / PRIVACY_REVIEW_REQUIRED
```

Po faktycznym otwarciu i sprawdzeniu wskazanego shareable JSON uruchom tę samą komendę z:

```text
--acknowledge-shareable-privacy-review
```

Dopiero wtedy runner może zgłosić:

```text
P1B_BUNDLE_PASS
```

## Następne po tej bramce

Osobny branch i osobny PR:

- internal occupancy correspondence GLB↔PLY;
- sabotage fixture: identyczne bounds, błędne wnętrze;
- adjacency/seam evidence;
- dopiero później P2 Diagnostic Preview.

## Stop conditions

Zatrzymaj implementację, jeżeli:

- source axis, scale lub origin są zgadywane;
- mirror ma przejść bez jawnej akceptacji;
- różne raporty 7+7 są automatycznie scalane lub wybierane;
- raw GLB/PLY mają zostać skopiowane do bundle;
- shareable output zaczyna kopiować nieznane pola lub free-form warnings;
- bundle jest przedstawiany jako accepted world patch;
- Box3D/GPU/UI handle trafia do kontraktu;
- branch zaczyna occupancy, ground extraction, cooker, renderer albo pełny Workbench JES przed `P1B_BUNDLE_PASS`.
