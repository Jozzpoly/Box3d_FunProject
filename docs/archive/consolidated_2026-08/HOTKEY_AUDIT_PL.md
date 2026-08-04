> **ARCHIWUM — nie jest bieżącą instrukcją.** 2026-08-04 treść została scalona lub zastąpiona przez sekcję „Skróty klawiszowe” w `docs/SUBSYSTEM_UI_PRESETS_PL.md`. Plik pozostaje jako zapis historii.

# Skróty klawiszowe — Jozz Vehicle

Status: bieżące źródło prawdy.
Sprawdzone 2026-08-04 wobec `samples/main.cpp`, `samples/gfx/keycodes.h`, M5,
M6, Wheel Scope, Quarter Car Scope i historycznego Primitive Corner Lab.

## 1. Skróty hosta — zarezerwowane globalnie

Host obsługuje je przed kodem sampla:

| Skrót | Działanie |
|---|---|
| `Tab` | pokaż/ukryj UI |
| `Esc` | zamknij controls albo wyczyść zaznaczenie |
| `Ctrl+Q` | wyjście |
| `Ctrl+O` | wyszukiwarka sampli |
| `O` | jeden krok |
| `Shift+O` | pięć kroków |
| `P` | pauza |
| `M` | metryki |
| `R` | rekonstrukcja bieżącego sampla |
| `[` / `]` | poprzedni / następny sample |
| `F` | kadruj zaznaczenie albo scenę |
| `?` | okno pomocy sterowania |

**Nie przydzielaj tych klawiszy do nowej funkcji JV.** `Space` celowo nie jest
skrótem hosta, ponieważ sample pojazdu używają go jako hamulca, a character
sample jako skoku.

## 2. Pojazdy M5 i M6

| Skrót | Działanie |
|---|---|
| `W` / `S` | jazda przód / tył |
| `A` / `D` | skręt w lewo / prawo |
| `Space` | hamulec |
| `T` | kamera third-person |

M5 i M6 mają ten sam układ celowo. Strojenie, mapa, spawny i debug pozostają w
UI. W M6 globalne `R` rekonstruuje sample, ale tuning, ustawienia Debug oraz
checkpoint/spawn są odtwarzane z właściwych plików sesji. Przycisk „Zresetuj
świat” w UI jest lepszy, gdy potrzebny jest reset przebiegu bez rekonstrukcji
całego sampla.

## 3. Wheel Scope

| Skrót | Działanie |
|---|---|
| `1` / `2` | sphere / prism-max |
| `,` / `.` | wolniej / szybciej |
| `V` | kamera follow |
| `G` | zapisz obserwację |
| `S` | zapisz konstrukcję `.rig` |
| `W` | wyzeruj liczniki pracy; nie zmienia fizyki |
| `C` | włącz/wyłącz regulator |
| `K` | impuls boczny |
| `J` | impuls w dół |
| `B` | impuls hamujący |
| `Ctrl+LPM` | chwyt koła; ingerencja w przebieg |

`C/K/J/B` i chwyt są wyłączone w trybie trace. Sesja po takiej ingerencji jest
eksploracją, nie dowodem headless.

## 4. Quarter Car Scope (Q3)

| Skrót | Działanie |
|---|---|
| `1`–`5` | wybór dostępnego kandydata |
| `B` | następny typ drogi |
| `C` | następny tryb napędu |
| `X` | pomiar teraz |
| `Z` | wyzeruj okna pomiarowe |
| `H` | stanowisko / jazda |
| `J` | podnieś narożnik |
| `K` | uderz nadwozie |
| `,` / `.` | wolniej / szybciej |
| `V` | kamera follow |
| `S` | zapisz konstrukcję `.qc` |
| `G` | zapisz obserwację |
| `Ctrl+LPM` | chwyt; ingerencja w przebieg |

`P`, `O`, `Shift+O` i `R` zachowują znaczenie globalne. Q3 przenosi ustawienia
warsztatu przez rekonstrukcję `R`, więc resetuje przebieg, nie świadomie
wybrane strojenie.

## 5. Primitive Corner Lab — sample historyczny, nadal kompilowany

| Skrót | Działanie |
|---|---|
| `W` / `S` | silnik koła przód / tył |
| `Space` | hamulec |
| `Q` / `E` | live root w dół / górę |

Ten sample wyjaśnia historyczny konflikt `[`/`]`, ale nie jest bieżącym
baseline’em JV. Dawna wersja audytu: `archive/vehicle_legacy_2026-07/HOTKEY_AUDIT_M2_5_2026-07-05_PL.md`.

## 6. Reguła dodawania nowego skrótu

1. Najpierw sprawdź `samples/main.cpp` i bieżący sample.
2. Preferuj UI dla ustawień, które nie wymagają trzymania klawisza.
3. Skrót badawczy musi mówić, czy zmienia fizykę i unieważnia przebieg dowodowy.
4. Dodaj alias do `samples/gfx/keycodes.h` tylko wtedy, gdy jest potrzebny w
   wielu plikach; lokalny `SAPP_KEYCODE_*` jest wystarczający dla jednego sampla.
5. Zaktualizuj ten dokument w tym samym commicie.
6. Ponownie sprawdź, że `[` i `]` pozostają wyłącznie nawigacją hosta.
