# Podsystem: UI · sesja · presety — stan aktualny

Krótka mapa panelu strojenia (`jozz_vehicle_m6_rig_lab.cpp`) i systemu
zapisu configu. Stan: 2026-07-08 (M8). Historia decyzji:
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

## 3. Co solidne / do polishu

- **Solidne:** rozdzielenie session/presety/debug-session; stabilne ID zakładek;
  `RefreshPresetList` (naprawiony bug `-1` na starcie).
- **Do polishu (niepilne):** brak walidacji wersji formatu presetu (nowe pole w
  configu → stary preset go po prostu nie ma, brak ostrzeżenia); brak UI do
  usuwania presetu (tylko save/load).

## 3a. Nowe klucze configu (P3/P4/P5, 2026-07-08)

Wszystkie mają kompatybilność wsteczną w `config_io` (stary klucz, jeśli
obecny w JSON, wypełnia oba/domyślne pola nowego formatu) - stare sesje i
presety wczytują się bez błędu.

| Etap | Stary klucz (usunięty) | Nowe klucze | Uwaga migracji |
|---|---|---|---|
| P3 | `suspensionPreload` | `suspensionPreloadFront/Rear` | stary → oba pola (ta sama wartość) |
| P4 | `rackFrictionForce` | `rackStaticFrictionForce/rackKineticFrictionForce` | stary → static=wartość, kinetic=0.5×wartość |
| P4 | — (nowe) | `rackCenteringHertz` | domyślnie 0 (brak w starych plikach = 0 = wyłączone, bezpieczne) |
| P5 | — (nowe) | `frontToeDeg`, `rearToeDeg` | domyślnie 0 w starych plikach = brak toe, bezpieczne |
| P5 | — (już było) | `maxSteeringAngleDegrees` dostał suwak UI | pole istniało w configu od M7, teraz edytowalne (pending-edit, Apply przelicza i w razie potrzeby zaciska - patrz P5 w `CHECKPOINTS_PL.md`) |

3 committed presety (`uliczny`/`drift`/`offroad`) zmigrowane na nowe klucze
P3/P4 bezpośrednio (nie polegają na fallbacku) - patrz `assets/vehicle_presets/*.json`.

## 4. Gdzie szukać w kodzie

`jozz_vehicle_m6_config_io.h/.cpp` (save/load/list), `jozz_vehicle_m6_rig_lab.cpp`
`kSessionFilePath`/`kPresetDirectory` (linie ~49-50), `LoadDebugViewState`/
`SaveDebugViewState` (osobny mechanizm, patrz `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md`
dla kontekstu równoległej zmiany „Zresetuj świat").
