# JV — mapa dokumentacji

Status: bieżące źródło prawdy o rolach dokumentów.

Celem tej mapy jest usunięcie klasy błędu „kilka dokumentów naraz udaje aktualny
plan”. Każdy bieżący plik ma jednego właściciela treści. Historia jest oddzielona.

## Wejście

| Plik | Właściciel treści |
|---|---|
| `../README_FOR_AGENTS.md` | reguły pracy agenta |
| `CURRENT_STATE_INDEX_PL.md` | aktualny stan implementacji i następny krok |
| `CHECKPOINTS_PL.md` | najnowszy dziennik zmian, krótki i rotowany |
| `TECH_DEBT_PL.md` | wyłącznie otwarte ryzyka i dług |
| `JV_JES_HERITAGE_PL.md` | co z JV dziedziczy JES i w jakiej formie |
| `MAPA_INDEX_PL.md` | bieżący stan mapy/skanów bez dawnych roadmap |

## Program koła i opony

| Plik | Rola |
|---|---|
| `KOLA_00_INDEX_PL.md` | wejście i twarde reguły programu |
| `KOLA_01_DOWODY_PL.md` | wygenerowane tabele, fakty i wyniki negatywne |
| `KOLA_02_ARCHITEKTURA_PL.md` | hipotezy architektoniczne i kontrakty |
| `KOLA_03_POLITYKA_BOX3D_PL.md` | zasady zmian rdzenia |
| `KOLA_04_PETLA_BADAWCZA_PL.md` | bieżący cykl K0–K7 i kolejka eksperymentów |
| `KOLA_05_PROTOKOL_STENDU_V21_PL.md` | protokół pomiarowy i confoundy |
| `KOLA_FINDINGS.json` | maszynowy status findingów |

Dokumenty `KOLA_*` są obszerne, bo stanowią laboratorium dowodowe. Nie wolno
kopiować ich treści do front doorów. Front door ma wskazywać, nie streszczać
każdą historyczną iterację.

## Bieżące dokumenty subsystemów

- `SUBSYSTEM_UI_PRESETS_PL.md` — UI, sesje i presety;
- `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` — rozdział rigu wizualnego i fizycznego;
- `SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md` — osie i przestrzenie rigu;
- `HOTKEY_AUDIT_PL.md` — skróty klawiszowe;
- `ASSET_CONTRACT_RUNTIME_V1_PL.md` — wdrożony kontrakt runtime;
- `ASSET_CONTRACT_V2_DRAFT_PL.md` — jawnie roboczy draft następnej wersji.

## ADR — trwałe decyzje, nie roadmapa

Katalog `adr/` przechowuje krótkie decyzje architektoniczne. Status w nagłówku
mówi, czy decyzja nadal obowiązuje. `0003` jest superseded jako finalna
architektura zawieszenia, a `0005` to zakończona decyzja o kolejności prac.
Bieżącą zasadą produktową pozostaje m.in. `0006`: realistyczny rdzeń i jawne,
domyślnie wyłączone nakładki `[ARCADE]`.

## Dokumentacja upstream Box3D

Pliki `overview.md`, `simulation.md`, `collision.md`, `recording.md` itd. należą
do upstreamowej dokumentacji silnika i są wejściem do Doxygen. Nie są częścią
narracji JV i nie należy ich przenosić do archiwum projektu.

## Archiwum

`archive/` przechowuje raporty etapów, stare plany, poprzednie ledger-y i pakiety
założycielskie. Pliki archiwalne mogą wyjaśniać **dlaczego** coś zrobiono, ale nie
mówią **co robić teraz**. Pełna mapa: `archive/README_PL.md`.

## Narzędzia higieny

- `python tools/docs_audit.py` — spójność autorytetu, archiwum, findings i kontraktów kod–docs;
- `python tools/repo_hygiene.py` — śledzone artefakty, kolizje nazw Windows, duplikaty, puste i nadmiernie duże pliki;
- `python tools/test_hygiene.py` — izolowane testy negatywne bramek higieny;
- `python tools/export_source.py` — deterministyczna paczka z commitowanego drzewa `HEAD`;
- `python tools/jozz_core_delta.py` — pokrycie i koszt jawnych patchy `src/`/`include/`.

## Reguła tworzenia nowego dokumentu

Nowy plik jest uzasadniony tylko wtedy, gdy jednocześnie:

1. ma trwałego właściciela treści;
2. nie duplikuje istniejącego źródła prawdy;
3. ma określony status: current / draft / evidence / archive;
4. wiadomo, co się z nim stanie po zamknięciu etapu;
5. `python tools/docs_audit.py` oraz `python tools/repo_hygiene.py` przechodzą po jego dodaniu.
