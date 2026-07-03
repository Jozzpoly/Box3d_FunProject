# Jozz Vehicle Box3D Native — README PL

Date: 2026-07-03  
Branch: `jozz-vehicle-sandbox-m0`  
Status: dokument wejściowy dla gałęzi Jozz Vehicle po M2.5

## Co to jest

To repozytorium jest forkiem/gałęzią eksperymentalną na fundamencie Box3D.

Cel projektu: zbudować natywny sandbox pojazdów Jozza, zaczynając od małych, sprawdzalnych laboratoriów fizyki zawieszenia, kół i późniejszych wizualnych rigów modeli z Blockbencha/glTF.

Ważne: główny `README.md` nadal opisuje upstream Box3D. To nie jest błąd. Ten plik opisuje dodatkową warstwę projektu Jozz Vehicle na aktualnej gałęzi.

## Aktualny stan

Aktualnie projekt nie ma jeszcze osobnego executable dla Jozz Vehicle.

Praktyczna rzeczywistość po M2.5:

```text
Jozz Vehicle działa jako lab w istniejącym Box3D samples host.
```

Aktywny sample:

```text
Category: Jozz Vehicle
Sample:   Lab M2 Primitive Corner
Panel:    Jozz Vehicle Lab M2.5
Source:   samples/sample_jozz_vehicle_lab.cpp
```

To jest świadomy wybór na ten etap. Sample host daje już okno, kamerę, ImGui, debug draw, input i integrację z buildem, więc nie tracimy czasu na przepisywanie fundamentu zanim fizyka narożnika jest dobrze ugruntowana.

## Co działa w M2.5

Ręcznie zwalidowany baseline:

- primitive one-corner wheel-joint lab;
- wycentrowany pivot koła;
- poprawny rest-anchor model dla `b3WheelJoint`;
- rest drop;
- rebound/compression travel;
- collision ON/OFF między kołem i chassis;
- live root stress mover przez slider;
- live root przez klawisze `Q/E`;
- oddzielenie pending structural setup od runtime live root controls.

Najważniejsza lekcja fizyki:

```text
b3WheelJoint ma implicit rest translation = 0.
Frame A = rest wheel-center anchor na chassis.
Frame B = centrum koła.
```

Nie wracamy do modelu, w którym Frame A jest wizualnym mountem amortyzatora/chassis.

## Jak zbudować

Z katalogu głównego repo:

```powershell
git pull --ff-only origin jozz-vehicle-sandbox-m0
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Po buildzie uruchom `samples.exe` z wygenerowanego katalogu build dla konfiguracji debug/release i wybierz sample z listy:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Panel po prawej powinien pokazywać:

```text
Jozz Vehicle Lab M2.5
```

## Aktualne sterowanie w labie

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

Nie używać `[ ]` dla kontroli Jozz Vehicle. Box3D samples host używa ich globalnie do przełączania sample'i.

## Gdzie zacząć czytanie

Najważniejsze pliki:

1. `README_FOR_AGENTS.md`
2. `docs/CURRENT_STATE_INDEX_PL.md`
3. `docs/PROJECT_AUDIT_2026_07_03_PL.md`
4. `docs/FOUNDATION_GROUNDING_PHASE_PLAN_PL.md`
5. `docs/HOTKEY_AUDIT_PL.md`
6. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
7. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
8. `assets/README.md`
9. `assets/reports/asset_audit_latest.md`
10. `samples/sample_jozz_vehicle_lab.cpp`

## Aktualne assety

Modele źródłowe są w:

```text
assets/source/
```

Kontrakty/sidecary są w:

```text
assets/contracts/
```

Raport audytu:

```text
assets/reports/asset_audit_latest.md
```

Aktualne glTF-y są traktowane jako research/startup assets, nie finalne produkcyjne kontrakty. Mają duplikaty nazw node'ów i nie wolno ufać samym nazwom bez indeksu/ścieżki/parent chain i złożonych transformów.

## Czego teraz nie robić

Na aktualnym etapie nie zaczynać:

- pełnego vehicle assembly;
- pełnego renderera glTF;
- mesh collision z modeli glTF;
- skomplikowanego multi-body suspension;
- nowych hotkeyów bez audytu;
- mieszania markerów wizualnych z frame'ami jointów fizyki;
- zmian, które cofają M2.4/M2.5 rest-anchor model.

## Najbliższy zalecany krok po grounding phase

Po uporządkowaniu dokumentacji i stanu projektu najbezpieczniejszy następny techniczny gate to:

```text
M3A — asset-derived primitive dimensions
```

Czyli: nadal używamy primitive physics z M2.5, ale domyślne wartości koła/rest drop/travel zaczynają pochodzić z audytu/kontraktów assetów.

Dopiero potem warto przejść do pierwszego visual-only glTF mesh attachment.