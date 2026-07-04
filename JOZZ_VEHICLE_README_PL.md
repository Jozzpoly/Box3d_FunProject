# Jozz Vehicle Box3D Native — README PL

Date: 2026-07-04
Branch: `jozz-vehicle-sandbox-m0`  
Status: dokument wejściowy dla gałęzi Jozz Vehicle po M2.5 + M3A + M3B.2.1 static textured visual proof

## Co to jest

To repozytorium jest forkiem/gałęzią eksperymentalną na fundamencie Box3D.

Cel projektu: zbudować natywny sandbox pojazdów Jozza, zaczynając od małych, sprawdzalnych laboratoriów fizyki zawieszenia, kół i późniejszych wizualnych rigów modeli z Blockbencha/glTF.

Ważne: główny `README.md` nadal opisuje upstream Box3D. To nie jest błąd. Ten plik opisuje dodatkową warstwę projektu Jozz Vehicle na aktualnej gałęzi.

## Aktualny stan

Aktualnie projekt nie ma jeszcze osobnego executable dla Jozz Vehicle.

Praktyczna rzeczywistość po M2.5/M3A/M3B.2.1:

```text
Jozz Vehicle działa jako lab w istniejącym Box3D samples host.
```

Aktywny sample:

```text
Category: Jozz Vehicle
Sample:   Lab M2 Primitive Corner
Panel:    Jozz Vehicle Lab M2.5 + M3A/M3B.2.1 debug
Source:   samples/sample_jozz_vehicle_lab.cpp
```

To jest świadomy wybór na ten etap. Sample host daje już okno, kamerę, ImGui, debug draw, input i integrację z buildem, więc nie tracimy czasu na przepisywanie fundamentu zanim fizyka narożnika jest dobrze ugruntowana.

## Co działa w M2.5/M3A/M3B.2.1

Ręcznie zwalidowany baseline M2.5:

- primitive one-corner wheel-joint lab;
- wycentrowany pivot koła;
- poprawny rest-anchor model dla `b3WheelJoint`;
- rest drop;
- rebound/compression travel;
- collision ON/OFF między kołem i chassis;
- live root stress mover przez slider;
- live root przez klawisze `Q/E`;
- oddzielenie pending structural setup od runtime live root controls.

M3A dodaje:

- scentralizowane primitive defaults;
- wheel radius/width wyciągnięte z aktualnych markerów audytu assetu koła;
- suspension travel z assetu jako hint;
- widoczny opis w panelu, że rest drop nadal jest explicit/tuned.

M3B.1 dodaje:

- checkbox `M3B semantic preview`;
- debugowy schemat markerów koła: radius, width, spin axis, wheel mount;
- debugowy schemat osi travel zawieszenia;
- brak mesh renderingu i brak wpływu na fizykę.

M3B.2 dodaje:

- statyczny visual-only proof dla `Offroad_Big_Wheels.gltf`;
- jeden mesh rysowany przy stałym debug originie;
- checkbox `M3B.2.1 static textured wheel proof`;
- brak attachu do fizyki, brak animacji, brak skinningu, brak kolizji mesh i brak pełnego importera glTF.

M3B.2.1 dodaje:

- minimalny textured mesh path w rendererze samples;
- `TEXCOORD_0` + `pbrMetallicRoughness.baseColorTexture`;
- PNG data URI decode przez izolowany helper Windows/WIC;
- status UI typu `texture: loaded baseColor 256x256`;
- fallback solid-color z czytelnym powodem, jeśli tekstura nie przejdzie.

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
cmd /c "set PATH=& cmake --build --preset windows-debug --target test"
cmd /c "set PATH=& cmake --build --preset windows-debug --target samples"
cmd /c "set PATH=& build\bin\Debug\test.exe"
```

W Codex/PowerShell na Windows zwykły build może potknąć się o zduplikowane klucze środowiska `Path/PATH` w MSBuild. Wtedy używaj wrappera `cmd /c "set PATH=& ..."` i traktuj ten błąd jako problem środowiska, nie kodu.

Audyt assetów uruchamiaj świadomie, bo zapisuje raporty w repo:

```powershell
py tools\asset_audit.py
py tools\asset_contract_audit.py
```

Po buildzie uruchom `samples.exe` z wygenerowanego katalogu build dla konfiguracji debug/release i wybierz sample z listy:

```text
Jozz Vehicle / Lab M2 Primitive Corner
```

Panel po prawej powinien pokazywać:

```text
Jozz Vehicle Lab M2.5 + M3A/M3B.2.1 debug
```

Sprawdź też checkbox:

```text
M3B semantic preview
```

Powinien pokazywać/ukrywać debugowy schemat markerów, ale nie zmieniać fizyki.

## Aktualne sterowanie w labie

```text
W      wheel motor forward
S      wheel motor reverse
Space  brake
Q      live root down
E      live root up
```

Nie używać `[ ]` dla kontroli Jozz Vehicle. Box3D samples host używa ich globalnie do przełączania sample'i.

M3B nie dodało nowych hotkeyów.

## Gdzie zacząć czytanie

Najważniejsze pliki:

1. `README_FOR_AGENTS.md`
2. `docs/CURRENT_STATE_INDEX_PL.md`
3. `docs/PROJECT_AUDIT_2026_07_03_PL.md`
4. `docs/FOUNDATION_GROUNDING_PHASE_PLAN_PL.md`
5. `docs/PRE_RIG_IMPORT_READINESS_AUDIT_PL.md`
6. `docs/M3A_IMPLEMENTATION_REPORT_PL.md`
7. `docs/M3B_SEMANTIC_DEBUG_PREVIEW_IMPLEMENTATION_REPORT_PL.md`
8. `docs/HOTKEY_AUDIT_PL.md`
9. `docs/M2_5_LIVE_ROOT_STRESS_MOVER_PL.md`
10. `docs/M2_4_WHEEL_JOINT_REST_ANCHOR_MODEL_PL.md`
11. `docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md`
12. `assets/README.md`
13. `assets/reports/asset_audit_latest.md`
14. `samples/sample_jozz_vehicle_lab.cpp`
15. `samples/jozz_vehicle_asset_metadata.h`
16. `samples/jozz_vehicle_asset_metadata.cpp`
17. `samples/jozz_vehicle_asset_dimensions.h`
18. `samples/jozz_vehicle_asset_dimensions.cpp`
19. `samples/jozz_vehicle_debug_preview.h`
20. `samples/jozz_vehicle_debug_preview.cpp`
21. `samples/jozz_vehicle_visual_mesh.h`
22. `samples/jozz_vehicle_visual_mesh.cpp`

## Aktualne assety

Modele źródłowe są w:

```text
assets/source/
```

Kontrakty/sidecary są w:

```text
assets/contracts/
```

Raporty/narzędzia audytu:

```text
assets/reports/asset_audit_latest.md
py tools\asset_audit.py
py tools\asset_contract_audit.py
```

Aktualne glTF-y są traktowane jako research/startup assets, nie finalne produkcyjne kontrakty. Mają duplikaty nazw node'ów i nie wolno ufać samym nazwom bez indeksu/ścieżki/parent chain i złożonych transformów.

Kontrakty assetów używają teraz bindingów z `nameHint`, `nodeIndexHint`, `nodePathHint`, `role`, `roleCategory` i `physicsAuthority`. Nazwa node'a jest tylko hintem dla człowieka i walidacji, nie unikalnym ID runtime.

## Czego teraz nie robić

Na aktualnym etapie nie zaczynać:

- pełnego vehicle assembly;
- pełnego renderera glTF;
- mesh collision z modeli glTF;
- skomplikowanego multi-body suspension;
- nowych hotkeyów bez audytu;
- mieszania markerów wizualnych z frame'ami jointów fizyki;
- zmian, które cofają M2.4/M2.5 rest-anchor model;
- automatycznego `restDrop` z `Socket_ChassisMount -> Socket_WheelCenter`;
- traktowania M3B semantic preview jako finalnego import transformu.

## Najbliższy zalecany krok

Nie skakać od razu w pełny rig/import ani builder. Najbezpieczniejszy następny techniczny gate:

```text
M3B.3 — visual-only wheel mesh attached to primitive wheel body
```

Zakres M3B.3: jeden mesh koła, tylko wizualny attach do transformu primitive wheel body, bez animacji, skinningu, mesh collision, pełnego auta i buildera.
