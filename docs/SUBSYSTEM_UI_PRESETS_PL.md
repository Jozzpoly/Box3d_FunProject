# Podsystem: UI · sesja · presety — stan aktualny

Krótka mapa panelu strojenia (`jozz_vehicle_m6_rig_lab_ui_tabs.cpp` — zakładki,
po podziale R3) i systemu zapisu configu. Stan: 2026-07-08 (M8, split R3
2026-07-11). Historia decyzji:
`M8_SUSPENSION_RIG_REPAIR_PLAN_PL.md` (fizyka), `CHECKPOINTS_PL.md` (skrót UI).

## 1. Trzy niezależne pliki zapisu — i dlaczego

| Plik | Format | Co trzyma | Kasowany przez reset? |
|---|---|---|---|
| `build/jozz_vehicle_m6_session.json` | JSON (`config_io`) | cały `JozzVehicleM6Config` (strojenie) | NIE — to jest ochrona przed „R" |
| `assets/vehicle_presets/*.json` | JSON (`config_io`) | nazwane presety (uliczny/drift/offroad), commitowane | tylko ręcznie przez usera |
| `build/jozz_vehicle_m6_debug_session.txt` | `key=value` | toggle zakładki Debug (linie/wizual/tint) | NIE — osobno od configu celowo |

**Dlaczego trzy, nie jeden:** toggle Debug to widok, nie strojenie — nie mogą
wyciekać do nazwanego presetu ani zniknąć pod „Przywróć domyślne". Rozdzielenie
jest świadome, nie przypadkowe zaśmiecenie.

## 1b. MAPA PERSYSTENCJI — gdzie żyje każde pole (2026-07-09)

Autorytatywna odpowiedź na „czy to przeżyje R / trafi do presetu". Kategoria,
nie lista 50 pól (lista sama by dryfowała — patrz zasada niżej).

| Kategoria pól | Gdzie | Przeżywa „R"? | W presecie? |
|---|---|---|---|
| Wszystko w `JozzVehicleM6Config` (zawieszenie, napęd, kierownica, nadwozie, koło...) | sesja JSON + presety | TAK (sesja) | TAK |
| Toggle widoku Debug (linie diagnostyczne, wizual koła/mocowania, tint) | debug-session txt | TAK | NIE (to widok) |
| `invertSteering` (preferencja sterowania) | debug-session txt (od 2026-07-09) | TAK | NIE (preferencja) |
| Solver kontaktu (`m_contactHertz/DampingRatio/Speed`, zakładka Świat) | **nigdzie** — patrz #12 poniżej | NIE (jeszcze) | NIE |
| `m_editX` (bufory pending-edit) | pochodne `m_config` (SyncEditFromConfig) | n/d | n/d |

**Reguła utrzymania (żeby ta mapa nie zdradzała):** to jest mapa KATEGORII, nie
pól. Dodając pole do `JozzVehicleM6Config` — automatycznie łapie je config_io
(sesja+preset), nic nie trzeba dopisywać do tej tabeli. Dodając pole SAMPLA
poza configiem (jak invert) — zdecyduj kategorię i dopisz do debug-session +
tu. Strażnik: `RunPresetDeterminismProbe` pilnuje że pola configu wracają do
fabryki przy wczytaniu presetu (klasa buga z 2026-07-09).

**#12 — solver kontaktu, świadomie ODŁOŻONE:** `m_contactHertz/DampingRatio/
Speed` nie persystują, bo do poprawnej persystencji trzeba by je APLIKOWAĆ przy
starcie (`ApplyContactTuning()` jest dziś wołane TYLKO przy ruchu suwaka —
świat startuje na domyślnych silnika, nie na 30/10/3 z UI). Dodanie aplikacji
startowej to zmiana fizyki-startu — poza zakresem porządków strukturalnych.
Kandydat na wielki refactor albo osobną, skupioną zmianę za zgodą Jozza.
`invertSteering` NIE ma tego problemu (czytane przy inpucie, nie aplikowane do
świata) — dlatego zrobione teraz.

## 2. Zakładki (kolejność = flow, ustalone z Jozzem)

`Zawieszenie · Nadwozie · Napęd · Kierownica · Świat · Debug`
(`JOZZ_M6_TAB` env = 0–5 wymusza jedną na klatkę, do zrzutów headless).

**Bug historyczny (naprawiony):** ID zakładek w ImGui hashują się z tekstu —
`"Zawieszenie"` vs `"Zawieszenie *"` (gwiazdka = niezapisane zmiany) to dwa
różne ID, więc zamknięta-zakładka fallback ImGui przełączał na inną zakładkę
przy każdym Apply. Fix: `"Zawieszenie *###TabSuspension"` — sufiks `###`
przypina identity do stałego stringa niezależnie od widocznego tekstu.
**Wzorzec do powielania:** każda zakładka z dynamicznym tytułem (gwiazdka,
licznik itp.) MUSI mieć `###StabilnySufiks`.

## 2a. SEMANTYKA WCZYTYWANIA — reguła obowiązkowa (2026-07-09)

Dwie ścieżki, dwa RÓŻNE kontrakty — nie wolno ich mylić:

| Ścieżka | Semantyka | Dlaczego |
|---|---|---|
| **Sesja** (`build/..._session.json`) | in-place (`LoadJozzVehicleM6Config`) — brakujący klucz zachowuje bieżącą wartość | plik pisany ZAWSZE w całości przez destruktor na świeżo-domyślny config; in-place daje darmową kompatybilność wsteczną (stary plik bez nowego klucza → default) |
| **Preset** (`assets/vehicle_presets/*.json`) | **deterministyczny** (`LoadJozzVehicleM6PresetConfig`) — wynik = `m_factoryConfig` + klucze pliku, NIEZALEŻNIE od stanu przed wczytaniem | presety są CZĘŚCIOWE („tylko to, co odróżnia setup"); in-place po cichu dziedziczył każdy eksperymentalny suwak, którego preset nie wymieniał |

**Incydent, który to wymusił (2026-07-09, złapany przez Jozza w grze):**
pokręcił suwakami „na brudno", wczytał preset licząc na pełny powrót — preset
ustawił tylko swoje ~15-19 kluczy, reszta eksperymentów ZOSTAŁA, a auto-sesja
utrwaliła je na dysku. Z fotela użytkownika: „preset zapisał moje zmiany bez
pozwolenia". Pliki presetów na dysku były nietknięte — wadliwa była semantyka
wczytania, nie zapis.

**Reguły na przyszłość:**
1. Każda nowa ścieżka load, która dla użytkownika znaczy „przywróć zapisany
   stan", MUSI iść przez `LoadJozzVehicleM6PresetConfig` (defaulty+nadpisania).
2. Baza fabryczna = `m_factoryConfig` (komponowana RAZ w konstruktorze:
   defaulty silnika + geometria z kontraktu + typy osi) — presety nakładają
   się na NIĄ, przycisk „Przywróć domyślne" przywraca JĄ. Jedno źródło prawdy,
   trzy miejsca użycia.
3. Strażnik maszynowy: sonda `RunPresetDeterminismProbe` w walidatorze
   (sabotuje pola spoza presetu → muszą wrócić do fabrycznych po load).

## 3. Co solidne / do polishu

- **Solidne:** rozdzielenie session/presety/debug-session; stabilne ID zakładek;
  `RefreshPresetList` (naprawiony bug `-1` na starcie).
- **Do polishu (niepilne):** brak walidacji wersji formatu presetu (nowe pole w
  configu → stary preset go po prostu nie ma → po fixie 2026-07-09 dostaje
  wartość FABRYCZNĄ, bezpiecznie); brak UI do usuwania presetu (tylko
  save/load); pola POZA configiem nie przeżywają R (solver kontaktu w Świat,
  „Odwróć kierowanie") — TECH_DEBT #12.

## 3a. Nowe klucze configu (P3/P4/P5, 2026-07-08)

Wszystkie mają kompatybilność wsteczną w `config_io` (stary klucz, jeśli
obecny w JSON, wypełnia oba/domyślne pola nowego formatu) - stare sesje i
presety wczytują się bez błędu.

| Etap | Stary klucz (usunięty) | Nowe klucze | Uwaga migracji |
|---|---|---|---|
| P3 | `suspensionPreload` | `suspensionPreloadFront/Rear` | stary → oba pola (ta sama wartość) |
| P4 | `rackFrictionForce` | `rackStaticFrictionForce/rackKineticFrictionForce` | (zastąpione ponownie w P4b, patrz niżej) |
| **P4b** | `rackStaticFrictionForce`, `rackKineticFrictionForce`, `rackFrictionForce` | `rackFrictionBase`, `rackFrictionLoadCoeff` | **INNY MODEL** (tarcie zależne od obciążenia drążków) — brak uczciwej konwersji liczbowej; klucze legacy są IGNOROWANE z wypisaną notką, plik dostaje defaulty nowego modelu. Presety zmigrowane ręcznie. |
| P4 | — (nowe) | `rackCenteringHertz` | domyślnie 0 (brak w starych plikach = 0 = wyłączone, bezpieczne); w UI jako `[ARCADE]` (ADR-0006) |
| P5 | — (nowe) | `frontToeDeg`, `rearToeDeg` | domyślnie 0 w starych plikach = brak toe, bezpieczne |
| P5 | — (już było) | `maxSteeringAngleDegrees` dostał suwak UI | pole istniało w configu od M7, teraz edytowalne (pending-edit, Apply przelicza i w razie potrzeby zaciska - patrz P5 w `CHECKPOINTS_PL.md`) |

3 committed presety (`uliczny`/`drift`/`offroad`) zmigrowane na nowe klucze
P3/P4 bezpośrednio (nie polegają na fallbacku) - patrz `assets/vehicle_presets/*.json`.

## 4. Gdzie szukać w kodzie

`jozz_vehicle_m6_config_io.h/.cpp` (save/load/list); po podziale R3:
`kSessionFilePath`/`kPresetDirectory`/`kDebugSessionFilePath` w
`jozz_vehicle_m6_rig_lab_internal.h`, presety + `LoadDebugViewState`/
`SaveDebugViewState` w `jozz_vehicle_m6_rig_lab_persistence.cpp` (osobny
mechanizm, patrz `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md`
dla kontekstu równoległej zmiany „Zresetuj świat").
