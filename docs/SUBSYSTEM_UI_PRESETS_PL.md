# Podsystem UI, sesji, presetów i checkpointów

Status: bieżący kontrakt, zweryfikowany z kodem 2026-08-04.
Historia migracji pól i dawny układ panelu: `archive/vehicle_legacy_2026-07/SUBSYSTEM_UI_PRESETS_SNAPSHOT_2026-07-11_PL.md`.

## 1. Cztery różne klasy stanu

| Stan | Plik | Zakres | Wersjonowany w Git |
|---|---|---|---|
| tuning ostatniej sesji | `build/jozz_vehicle_m6_session.json` | cały `JozzVehicleM6Config` | nie |
| nazwane presety | `assets/vehicle_presets/*.json` | częściowe konfiguracje pojazdu | tak |
| stan lokalnej sesji sampla | `build/jozz_vehicle_m6_debug_session.txt` | widok, preferencje, checkpoint mapy i skan | nie |
| domyślne spawny fragmentów | `assets/vehicle_spawns.txt` | `plate`, `offroad`, `scan.<packId>` | tak |

Te klasy nie mogą zostać scalone w jeden plik:

- preset opisuje **pojazd**, nie lokalny widok ani ścieżkę do skanu;
- debug-session może zawierać lokalną ścieżkę i ma umrzeć razem z `build/`;
- trwały spawn zmienia kuratorowany stan mapy, więc zapisuje się wyłącznie po
  jawnym kliknięciu i jest czytelny w diffie;
- auto-save tuningu chroni pracę przed globalnym `R`, ale nie jest produktem.

## 2. Semantyka wczytywania configu

### Sesja

`LoadJozzVehicleM6Config` modyfikuje przekazany config in-place. Konstruktor
najpierw buduje `m_factoryConfig`, kopiuje go do `m_config`, a potem nakłada
istniejące klucze sesji. Brakujący klucz pozostawia wartość fabryczną.

### Preset

`LoadJozzVehicleM6PresetConfig` zawsze zaczyna od `m_factoryConfig`, a dopiero
potem nakłada klucze presetu. Wynik nie zależy od suwaków ustawionych przed
wczytaniem. To jest obowiązkowa semantyka każdej operacji znaczącej dla
użytkownika „przywróć zapisany setup”. Pilnuje jej
`RunPresetDeterminismProbe`.

### Reset fabryczny

Przycisk resetu przywraca ten sam `m_factoryConfig`. Nie konstruuje drugiej,
równoległej definicji defaultów.

## 3. Co dokładnie przeżywa globalne `R`

`JozzVehicleM6RigLab` zapisuje w destruktorze i odtwarza w konstruktorze:

- tuning `JozzVehicleM6Config`;
- widoczność kół, rigu, brył i diagnostyki;
- `invertSteering` jako preferencję sampla;
- ostatni anchor X/Z oraz seed offroadu;
- informację o załadowanym skanie i lokalną ścieżkę paczki.

Trwałe spawny fragmentów są ładowane osobno z `assets/vehicle_spawns.txt`.
Kolejność wyboru spawnu to:

```text
sesyjny > trwały per fragment/paczka skanu > wbudowany anchor
```

Spawn sesyjny istnieje tylko w pamięci. Trwały jest zapisywany wyłącznie przez
„Zapisz jako domyślny”. Skan ma osobny wpis dla każdego identyfikatora paczki.

## 4. Aktualny układ panelu

Główne zakładki, w kolejności kodu:

```text
Zawieszenie · Nadwozie · Napęd · Kierownica · Świat · Mapa · Debug
```

`JOZZ_M6_TAB=0..6` wymusza zakładkę w przebiegu headless. Zakładka `Mapa` ma
podzakładki `Płyta`, `Offroad` i `Skan (wyspa)`.

Dynamiczne etykiety zakładek muszą mieć stały sufiks ImGui `###...`, np.
`Zawieszenie *###TabSuspension`. Bez niego zmiana widocznej etykiety tworzy nową
tożsamość widgetu i może przestawić aktywną zakładkę.

## 5. Granice konfiguracji

Do `JozzVehicleM6Config` należą parametry wpływające na tożsamość i zachowanie
pojazdu, w tym model nadwozia, wariant wizualny przedniego zawieszenia, fizyczne
pola tarcia zębatki `rackFrictionBase`/`rackFrictionLoadCoeff` oraz jawnie
oznaczone, domyślnie wyłączone nakładki `[ARCADE]`, np. `rackCenteringHertz`.
Do configu nie należą:

- checkboxy widoku;
- lokalne ścieżki do paczek skanu;
- checkpoint mapy;
- preferencja `invertSteering`;
- globalne pokrętła solvera kontaktu w zakładce `Świat`.

Pokrętła kontaktu (`m_contactHertz`, `m_contactDampingRatio`, `m_contactSpeed`)
nie są persystowane. Są ustawieniami laboratorium całego świata, a nie opony ani
presethu pojazdu. Nie przenosić ich do configu przy okazji eksperymentu lokalnej
podatności — przyszła podatność wheel-ground musi mieć własny, izolowany
kontrakt.

## 6. Bieżące ryzyka

- format JSON nie ma jawnej wersji schematu;
- UI nie usuwa nazwanych presetów;
- `scanPackDir` jest celowo lokalny i nie może trafić do pliku commitowanego;
- każda nowa zmienna sampla poza configiem wymaga jawnej decyzji: sesja lokalna,
  stan trwały albo brak persystencji;
- dodanie pola do configu wymaga testu round-trip oraz deterministycznego loadu
  presetu, ale nie wymaga utrzymywania nieograniczonej zgodności historycznej,
  dopóki JV jest laboratorium jednego właściciela.

## 7. Skróty klawiszowe

Host przechwytuje te skróty przed samplem i nie wolno przydzielać ich nowym
funkcjom JV:

| Skrót | Host |
|---|---|
| `Tab` | pokaż/ukryj UI |
| `Esc` | zamknij controls lub wyczyść zaznaczenie |
| `Ctrl+Q` / `Ctrl+O` | wyjście / wyszukiwarka sampli |
| `O` / `Shift+O` / `P` | krok / pięć kroków / pauza |
| `M` / `F` / `?` | metryki / kadrowanie / pomoc |
| `R` | rekonstrukcja bieżącego sampla |
| `[` / `]` | poprzedni / następny sample |

Pojazdy M5/M6: `W/S` jazda, `A/D` skręt, `Space` hamulec, `T` kamera.

Wheel Scope: `1/2` kandydat, `,` / `.` prędkość, `V` kamera, `G` obserwacja,
`S` zapis `.rig`, `W` reset liczników, `C` regulator, `K/J/B` impulsy,
`Ctrl+LPM` chwyt. Ingerencje są wyłączone w trace i unieważniają przebieg
headless jako dowód.

Quarter Car Scope: `1–5` kandydat, `B` droga, `C` napęd, `X` pomiar, `Z` reset
okna, `H` stanowisko/jazda, `J/K` wymuszenia, `,` / `.` prędkość, `V`, `S`, `G`,
`Ctrl+LPM` jak wyżej.

Reguła dodania skrótu: najpierw sprawdź `samples/main.cpp` i bieżący sample,
preferuj UI dla ustawień nietrzymanych, opisz wpływ na ważność eksperymentu i
zachowaj `[`/`]` wyłącznie dla hosta. Historyczny pełny audyt:
`archive/consolidated_2026-08/HOTKEY_AUDIT_PL.md`.

## 8. Właściciele kodu

- `jozz_vehicle_m6_config_io.h/.cpp` — serializacja, load sesji i presetów;
- `jozz_vehicle_m6_rig_lab_internal.h` — ścieżki i klasy stanu;
- `jozz_vehicle_m6_rig_lab_persistence.cpp` — debug-session, presety i spawny;
- `jozz_vehicle_m6_rig_lab_ui_tabs.cpp` — zakładki i akcje użytkownika;
- `validation/jozz_probes_config.cpp` — bramka deterministycznego presetu.

Po zmianie któregokolwiek kontraktu uruchom:

```text
python tools/jv_gate.py quick
```
