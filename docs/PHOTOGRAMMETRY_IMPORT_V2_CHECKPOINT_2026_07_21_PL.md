# Photogrammetry Import V2 — checkpoint implementacyjny P0

**Data:** 2026-07-21  
**Branch roboczy:** `photogrammetry/import-v2-foundation`  
**Branch bazowy:** `Photogrametry_Import_experiment`  
**Bazowy commit:** `f1c4919e501721749084210aea9b571e96b69bed`  
**Aktualna bramka:** P0 — oczekuje na wykonanie lokalnego Windows gate

## Co zostało zaimplementowane

Dodano hermetyczny runner P0:

```text
tools/scan_pipeline/scan_p0_baseline.py
tools/scan_pipeline/scan_p0_baseline.ps1
tests/scan_pipeline/test_scan_p0_baseline.py
```

Runner wykonuje przed uruchomieniem `tools/gate.ps1 -SaveBaseline` twarde kontrole:

- bieżący branch nie jest `main`;
- repozytorium nie jest w detached HEAD;
- bazowy commit `f1c4919e501721749084210aea9b571e96b69bed` istnieje lokalnie;
- bazowy commit jest przodkiem bieżącego HEAD;
- working tree jest czyste;
- RAR znajduje się pod `local_assets/scans/`;
- plik RAR jest ignorowany przez Git;
- żadne `.rar`, `.ply`, `.las` ani `.laz` nie są śledzone przez Git;
- opcjonalny oczekiwany SHA-256 zgadza się z lokalnym plikiem;
- katalog raportu pozostaje wewnątrz gitignored `build/`.

Po kontroli runner:

1. uruchamia istniejący Windows gate z `-SaveBaseline`;
2. zapisuje pełny output gate;
3. kopiuje świeży validator baseline;
4. kopiuje świeży quad screenshot M6;
5. mierzy czas gate;
6. zapisuje wersję Git, CMake, Python i PowerShell;
7. zapisuje stan czystości repo przed i po gate;
8. generuje lokalny raport JSON i Markdown bez absolutnych ścieżek, nazw użytkownika i georeferencji;
9. nie kopiuje starych artefaktów baseline, jeżeli bieżący gate nie utworzył lub nie nadpisał plików;
10. uznaje P0 za PASS tylko wtedy, gdy gate zwrócił kod 0, podsumowanie zawiera `build 3/3 OK`, `walidator OK`, `test PASS` i `smoke 0 err`, oba świeże artefakty istnieją, a repo po gate pozostaje czyste.

Wyniki są zapisywane wyłącznie pod:

```text
build/scan_pipeline/p0_baseline/<commit>_<UTC>/
```

## Testy wykonane w tej sesji

```text
9/9 PASS
python -m py_compile: PASS
CLI --help smoke: PASS
```

Testy pokrywają:

- stabilny SHA-256;
- izolację raw skanu do `local_assets/scans/`;
- ochronę prywatności raportu;
- wybór końcowej linii gate;
- kompletność sukcesu gate;
- izolację outputu do `build/`;
- blokadę kopiowania starego artefaktu;
- kopiowanie rzeczywiście zaktualizowanego artefaktu;
- brak absolutnego repo root w finalnym JSON.

## Ochrona raw scanów

Istniejąca reguła:

```gitignore
local_assets/scans/
```

obejmuje cały lokalny katalog wejściowy, niezależnie od rozszerzenia. Nie dodano globalnego `*.ply`, ponieważ późniejsze testy P1 mogą potrzebować małych, jawnie bezpiecznych fixtures PLY. Runner P0 osobno blokuje śledzone raw `.rar/.ply/.las/.laz`.

## Zidentyfikowany plik źródłowy

Dla archiwum przekazanego w rozmowie:

```text
nazwa:   Model_skanu.rar
rozmiar: 415133488 bytes
SHA-256: 575d170b9643af82f79a8e3ecd6deedb2f13f075d7dbedd41d6f758269d1e38d
```

Nagłówek RAR zawiera 7 GLB i 7 PLY. Raw dane nie zostały dodane do repozytorium.

## Wykonanie P0 na Windows

Z repo root, po umieszczeniu identycznego archiwum w `local_assets/scans/`:

```powershell
.\tools\scan_pipeline\scan_p0_baseline.ps1 `
  -ScanArchive .\local_assets\scans\Model_skanu.rar `
  -ExpectedSha256 575d170b9643af82f79a8e3ecd6deedb2f13f075d7dbedd41d6f758269d1e38d
```

Sukces kończy się komunikatem:

```text
P0: PASS — baseline captured; P1 may start
```

Każdy błąd kończy się kodem różnym od zera, zapisuje powody w lokalnym raporcie, jeśli gate został uruchomiony, i pozostawia P1 zablokowane.

## Uczciwy stan bramki

W obecnym środowisku nie ma Windows, MSVC, D3D11 ani lokalnego checkoutu zdolnego uruchomić `windows-debug`, `samples.exe` i screenshot gate. Dlatego nie oznaczono P0 jako zaliczonego.

Nie rozpoczęto P1. To celowe zastosowanie kryterium STOP.
