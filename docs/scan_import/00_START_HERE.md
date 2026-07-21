# Photogrammetry Import V2 — start here

**Status:** `P1A_SOURCE_INSPECTION_PASS / P1B_CONTRACT_FOUNDATION_IN_PROGRESS`  
**Branch roboczy:** `agent/p1b-world-import-contract-staging`  
**Baza:** head draft PR #1 (`dbbd065d9bbf8887b824e351e464dbbe60be1e68`)  
**Zakres:** offline contracts, tests and documentation only; no runtime C++, renderer, vehicle or Box3D behavior changes.

## Co jest prawdziwe

- P0 Windows build/validator/test/smoke został zaliczony.
- Aktualny P1A inspector ma zielone dependency-free CI na Windows/Linux, Python 3.11/3.13, stdlib/NumPy.
- W aktywnej sesji właściciela wykonano realny przebieg 7 GLB + 7 PLY: 7/7 źródeł wykryto, automatic evidence gate przeszedł i dwa przebiegi dały identyczne siedem artefaktów.
- Ten wynik sesji nadal wymaga zapisania jako odtwarzalny lokalny receipt na dokładnym headzie; prywatne outputy nie trafiają do Git.
- P2 nie jest odblokowane semantycznie: bounds nie dowodzą zgodności wnętrza geometrii, a dotychczasowy kontrakt osi nie opisuje znaków osi, mirror, local origin ani pełnej transformacji source→lab.

## Co robi P1B

P1B wprowadza najmniejszą granicę potrzebną przed diagnostic preview:

```text
inspection evidence
→ ScanSourcePackage (stable identity + content revision)
→ WorldImportProposal (generated, UNREVIEWED)
→ future WorldPatchReview
→ future AcceptedWorldPatch
```

Pierwszy pakiet P1B zawiera:

- `scan_frames.py` — pełny, jawny kontrakt jednostek i source→lab;
- `scan_world_contracts.py` — backend-neutralne source package i import proposal;
- dependency-free tests;
- kanoniczną architekturę i aktualny status.

## Kanoniczne polecenie testowe

```powershell
python .\tools\scan_pipeline\run_p1_contracts.py
```

Po pobraniu brancha na Windows należy dodatkowo uruchomić:

```powershell
.\tools\gate.ps1
```

## Najbliższa bramka

`P1B_CONTRACT_FOUNDATION_PASS` wymaga łącznie:

1. wszystkich kontraktów dependency-free PASS;
2. round-trip source→lab→source;
3. jawnego wykrycia mirror i odrzucenia niezatwierdzonego mirror;
4. stable package ID niezależnego od rewizji plików;
5. content-derived revision ID;
6. proposal pozostającego `UNREVIEWED` i `BOUNDS_ONLY`;
7. braku native/GPU/UI handles w trwałych kontraktach;
8. lokalnego P0/vehicle gate bez regresji.

## Następne po tym pakiecie

- podłączyć frame contract i source package do inspectora jako osobne outputy prywatne;
- rozdzielić private evidence od shareable evidence;
- dodać transactional output bundle;
- wprowadzić occupancy/internal-geometry pair evidence;
- dopiero potem otworzyć P2 Diagnostic Preview.

## Stop conditions

Zatrzymaj implementację, jeżeli:

- raw GLB/PLY lub prywatne współrzędne mają trafić do Git;
- proposal zaczyna zawierać ręczne decyzje autora;
- Box3D/GPU/UI handle trafia do kontraktu;
- source mesh lub heightfield zostaje nazwany authored world truth;
- branch zaczyna zmieniać pojazd, renderer albo budować pełny Workbench JES.
