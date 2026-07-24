# Śledztwo i odzysk utraconych zmian lokalnych — 2026-07-24

Kontekst: Jozz zgłosił, że przy imporcie skanu terenu „nowa mapa" została zastąpiona starą
z obcego brancha, a presety oraz ustawienia sterowania/pojazdu zniknęły. Zmiany te **nie
były w repozytorium** — były lokalne w worktree. Poniżej pełna forensyka (źródła: `git
reflog`, `git fsck`, porównania commitów, pliki lokalne) i macierz odzysku.

## TL;DR

**Nic nie jest bezpowrotnie stracone.** Cały stan sprzed straty jest zachowany w commicie
snapshotu `56c04c1` (na `origin/jozz-map-wip-snapshot-2026-07-24`) ORAZ fizycznie w worktree
(pliki untracked). Import skanu był **czysto addytywny** (7 plików, wyłącznie skan + Mapa UI +
spawny) i **nie tknął** kodu pojazdu/sterowania/zawieszenia ani generatora terenu. Utrata
„nowej mapy" nastąpiła **wcześniej** — przez `git reset` o 01:41, jeszcze przed powstaniem
gałęzi skanu.

## Oś czasu (z reflogu)

| Czas (2026) | Zdarzenie | Skutek |
|---|---|---|
| 07-11..07-12 | ETAP 1/2 mapy (masyw `8ab1635`, obstacle kit `b8afab9`) + finalizacja rigu (`4222f71/0454fc1/1e9a3fb`) | **zacommitowane, są w HEAD** |
| 07-12 17:34 | `445db88` „rebase map plan around central test campus" | ostatni **zacommitowany** stan mapy |
| 07-12 → 07-24 | E2R central test campus + E3 tor + pakiet JES + spike rozwijane **lokalnie (bez commita)** na bazie `445db88` | tylko worktree |
| **07-24 01:41:17** | `56c04c1` „WIP snapshot" — dirty worktree zacommitowany | **pełny zapis utraconego stanu** |
| **07-24 01:41:20** | `reset: moving to HEAD~1` → powrót do `445db88` | **zdjął wiring nowej mapy z aktywnej gałęzi**; pliki untracked przetrwały na dysku |
| 07-24 02:14–02:31 | commity docs (`706f67c`,`718914f`,`36f3e79`) | — |
| 07-24 09:54 | gałąź `jozz-scan-terrain-f0` z `36f3e79` | — |
| 07-24 09:57–17:08 | import skanu M0–M2 + spawny (`53a629e`..`64a8bf2`) | **addytywny, 7 plików** |

Mechanizm straty: snapshot → `reset` cofnął modyfikacje plików **śledzonych** (wiring mapy)
do `445db88`. Nowe pliki **untracked** (campus, tor, spike) nie są ruszane przez reset i
zostały na dysku, ale **bez podpięcia do budowania** — dlatego aktywna mapa wróciła do
zacommitowanego masywu (`8ab1635`), co Jozz widzi jako „starą mapę".

## 1. Co zostało nadpisane / utracone (z aktywnej gałęzi)

- **Wiring „nowej mapy" (pliki śledzone, cofnięte resetem):** `samples/CMakeLists.txt` (+16),
  `samples/jozz_vehicle_world_layout.h` (zmiana 114 linii), `samples/jozz_vehicle_obstacle_kit.{cpp,h}`
  (refaktor), `samples/jozz_vehicle_m5_test_course.{cpp,h}` (refaktor), hooki w
  `jozz_vehicle_m6_rig_lab.cpp`, `jozz_vehicle_m5_drivable_lab.cpp`, `jozz_vehicle_validation.cpp`.
- **NIE utracone (mylne wrażenie):**
  - *Presety* — 3 pliki (`drift/offroad/uliczny`) są na dysku i **żaden nigdy nie był usunięty
    z gita**; brak śladu, by istniało więcej. „Zniknięcie" to najpewniej problem ścieżki
    `kPresetDirectory` **względnej do CWD** (uruchomienie exe z innego katalogu → pusty combo).
  - *Sterowanie / system pojazdu* — pełny strojony config jest w `build/jozz_vehicle_m6_session.json`
    (gitignored, przetrwał operacje gita). Kod pojazdu/sterowania/zawieszenia **nie był tknięty**
    przez import skanu (potwierdzone `git diff`).
  - *Generator terenu (masyw)* — `jozz_vehicle_world_terrain.cpp` **bajt-w-bajt identyczny** z `8ab1635`.

## 2. Co udało się odzyskać / zabezpieczyć

- **Kopia archiwalna całego stanu** worktree (samples/assets/docs/spike/tools + pliki sesji build)
  do scratchpada — punkt zerowy odzysku, niezależny od gita.
- **Pliki untracked (campus, tor, probes, spike)** — obecne w worktree i **identyczne** ze
  snapshotem `56c04c1` (0 różnic treści). Autentyczna zachowana praca.
- **Wiring nowej mapy** — w całości w `56c04c1` (odzyskiwalny cherry-pickiem/patchem).
- **Presety + tuning pojazdu** — na dysku, nietknięte.

## 3. Czego nie udało się odzyskać

- **Brak twardych strat.** Jedyne teoretyczne luki: (a) presety/tuning zapisane w runtime PO
  snapshocie `56c04c1` (01:41) — brak jakiegokolwiek śladu, że takie istniały; (b) dwa
  **osierocone stashe** (`121081f` z 07-12, `2ad2dd9` z 07-08) dotykające `world_terrain.cpp`
  i `suspension_rig.cpp` — to stare WIP-y **zastąpione** przez późniejsze commity, wartość
  odzysku znikoma (do decyzji). Rezerwowo dostępna też **historia lokalna VS Code**.

## 4. Konflikty / regresje

- **Import skanu jest czysty** — zmienił od bazy mapy dokładnie 7 plików, wszystkie addytywne
  (skan/Mapa/spawny). Zero zmian w kodzie pojazdu/sterowania/terenu. Walidator: **18 sond OK**.
- **Re-integracja mapy = konflikt 3-way:** `world_layout.h` (mapa 114 linii vs skan +43) i
  `CMakeLists.txt` (mapa +16 vs skan +6) zmienione po obu stronach → wymaga ręcznego scalenia.
  `obstacle_kit`/`test_course` refaktor mapy nakłada się czysto (skan ich nie tykał).
- **Znane wady zawartości mapy (audyt 2026-07-13):** E3 tor = „3 nakładające się pętle bez
  prześwitu auta" (odrzucony); E2R campus = „recovery required". Snapshot `56c04c1` odtwarza
  ten stan **z tymi wadami** — dlatego re-integracja powinna być selektywna, nie ślepa.
- **Regresje skanu naprawione w tej sesji (osobno):** „R" gubił wyspę skanu i teksturę wracała
  „0/25" (wyciek obrazów sokol w destruktorze). Poprawki gotowe i **scommitowane** (2026-07-24, na `main`).

## 5. Wykonane zmiany

- Kopia bezpieczeństwa całego worktree; pełna forensyka read-only (bez operacji niszczącej gita).
- Ten raport.
- **Poprawki regresji skanu #1/#4** — teardown skanu w destruktorze (koniec wycieku obrazów
  sokol → koniec „tekstury 0/25") + reload wyspy po „R"/reopen (checkpoint parity).
- **Odzysk „nowej mapy": E2R central test campus, BEZ toru E3 (decyzja Jozza).** Merge 3-way na
  obecny HEAD ze skanem:
  - pobrane z `56c04c1` (skan ich nie tykał): `obstacle_kit.{cpp,h}`, `m5_test_course.{cpp,h}`,
    `m5_drivable_lab.cpp`, `world_layout.h`;
  - `world_layout.h` scalony: masterplan/kampus + kotwice (z `56c04c1`) **+ ponownie dołożone**
    stałe skanu (`kScanSouthEdgeZ`, `kScanGroundY`) i klasyfikator fragmentu;
  - `test_course.cpp`: budowany tylko `BuildCentralTestCampus`; wywołania toru
    (`BuildJozzTrackBase/Profiles`) i include toru **usunięte**;
  - `rig_lab.cpp` / `drivable_lab.cpp`: rysują `DrawCentralCampusSkeleton()`, **bez** toru;
  - `validation.cpp` + `probes_map.cpp`: sondy campus + masterplan; tor-probe **usunięty**
    (walidacja nie ciągnie `track_layout`);
  - `CMakeLists.txt`: dodane pliki campusu + `probes_map`; pliki toru **niepodpięte**.
  - Pliki toru E3 (`jozz_vehicle_track_*`) zostają na dysku, **uśpione**, na przyszłość.

## 6. Aktualny, zweryfikowany stan

- Gałąź robocza `jozz-scan-terrain-f0`; **stan scommitowany i wyniesiony na `main`**
  (fast-forward) 2026-07-24 — decyzja Jozza „zapisujemy aktualny stan na maina" przed skokiem w bok.
- **Bramka ZIELONA:** build 3/3 OK, walidator **18 sond + 2 sondy mapy OK** (campus: 4 stacje,
  3 wyspy skalne, 13 banków garbów), test PASS, boot-smoke 0 błędów.
- **Render potwierdzony:** central campus + 4 stacje + wyspy skalne + banki garbów w świecie,
  BEZ toru E3.
- Import skanu (M0–M2 + spawny) + poprawki #1/#4 działają obok campusu (osobna wyspa na północy).
- Presety (3) + tuning pojazdu/sterowania (`build/session.json`) nietknięte.
- **Rozstrzygnięte (2026-07-24):** WP-00 zamknięty decyzją Jozza. Snapshot mieszanego WIP
  `56c04c1` istnieje na `origin/jozz-map-wip-snapshot-2026-07-24`; odzyskany, sklasyfikowany,
  zielony stan scommitowany na `main` (fast-forward z `jozz-scan-terrain-f0`). Kruchość „pracy
  tylko lokalnej" — źródło całego incydentu — usunięta. Następny krok: skok w bok do nowego
  repo (projekt „ULTIMATE"/JES) — w osobnej rozmowie.
