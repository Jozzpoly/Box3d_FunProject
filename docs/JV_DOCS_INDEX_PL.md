# JV — mapa dokumentacji

Status: bieżące źródło prawdy o roli dokumentów. Celem jest jeden szlak dla
agenta i brak równoległych „aktualnych planów”.

## 1. Wejście

| Plik | Jedyny właściciel treści |
|---|---|
| `../README_FOR_AGENTS.md` | reguły pracy i bramki |
| `CURRENT_STATE_INDEX_PL.md` | aktualny stan kodu i najbliższy krok |
| `CHECKPOINTS_PL.md` | krótki, rotowany handoff |
| `TECH_DEBT_PL.md` | wyłącznie otwarty dług |
| `MAPA_INDEX_PL.md` | stan mapy i skanów |
| `JV_JES_HERITAGE_PL.md` | granica transferu JV → JES |

Czytanie zaczyna się od `README_FOR_AGENTS.md`, nie od wyszukiwarki po `docs/`.

## 2. Koło i opona

| Plik | Rola |
|---|---|
| `KOLA_00_INDEX_PL.md` | krótki front door i granice programu |
| `KOLA_01_DOWODY_PL.md` | ledger pomiarów, faktów i wyników negatywnych |
| `KOLA_02_ARCHITEKTURA_PL.md` | bieżące warstwy i kontrakty `b3Wheel` |
| `KOLA_03_POLITYKA_BOX3D_PL.md` | prawo zmian rdzenia |
| `KOLA_04_PETLA_BADAWCZA_PL.md` | cykl K0–K7 i kolejka rekurencji |
| `KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md` | manifest, metryki i bramki dowodu |
| `KOLA_FINDINGS.json` | jedyny maszynowy status findingów |
| `JOZZ_CORE_PATCHES.json` | własność i zakres delty Box3D |

Duży ledger dowodowy nie jest roadmapą. Plan bieżący żyje w `CURRENT_STATE` i
kolejce `KOLA_04`.

## 3. Subsystemy

| Plik | Zakres |
|---|---|
| `ASSET_CONTRACT_PL.md` | glTF + sidecar + granica physics authority |
| `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` | przestrzenie, hardpointy i wizual rigu |
| `SUBSYSTEM_UI_PRESETS_PL.md` | UI, stan sesji, presety, checkpointy i hotkeys |

Nie twórz osobnego dokumentu dla fragmentu, który ma już właściciela w tej
tabeli. Rozbuduj istniejący kontrakt albo dodaj krótkie ADR dla jednej trwałej
decyzji.

## 4. ADR

`adr/` przechowuje jedną decyzję na plik. ADR nie jest roadmapą. Status w
nagłówku mówi, czy decyzja obowiązuje, została wykonana czy superseded.

## 5. Upstream Box3D

`overview.md`, `simulation.md`, `collision.md`, `recording.md` i pozostałe pliki
manuala należą do upstreamu i Doxygen. Nie są narracją JV i nie podlegają
scalaniu z dokumentami projektu.

## 6. Archiwum

`archive/` zachowuje dawne plany, raporty i poprzedników scalonych dokumentów.
Plik archiwalny może wyjaśniać **dlaczego**, ale nie mówi **co robić teraz**.
Mapa: `archive/README_PL.md`.

## 7. Narzędzia jakości

- `python tools/jv_gate.py quick` — normalny checkpoint;
- `python tools/jv_gate.py deep` — dokumentacja/infrastruktura i testy regresji;
- `python tools/jv_gate.py wheel` — dodatkowo lokalne bramki Wheel Scope;
- `python tools/docs_audit.py` — autorytet, routing i code–docs drift;
- `python tools/repo_hygiene.py` — przenośność i czystość commitowanego drzewa;
- `python tools/export_source.py` — deterministyczny ZIP z `HEAD`.

## 8. Reguła nowego dokumentu

Nowy aktywny dokument powstaje tylko wtedy, gdy:

1. ma jednego trwałego właściciela treści;
2. jego cykl życia różni się od istniejących dokumentów;
3. nie jest jednorazowym raportem;
4. zostaje podłączony do tej mapy;
5. `docs_audit` potrafi wykryć jego osierocenie.

Raport zakończonego etapu trafia do `archive/`. Realna zmiana stanu to kilka
linii w `CHECKPOINTS`, nie kolejny plan o nazwie „final”.
