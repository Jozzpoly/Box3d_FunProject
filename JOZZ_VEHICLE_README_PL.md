# Jozz Vehicle Box3D Native — README PL

Data: 2026-07-05  
Branch: `jozz-vehicle-sandbox-m0`

## Co to jest

To repozytorium jest forkiem/gałęzią eksperymentalną na fundamencie Box3D
(https://github.com/erincatto/box3d).

Cel projektu: natywny sandbox pojazdów Jozza — docelowo gra o budowaniu
samochodów z komponentów projektowanych w Blockbenchu — rozwijany małymi,
sprawdzalnymi bramkami (gates) na stabilnym baseline fizyki.

Ważne: główny `README.md` nadal opisuje upstream Box3D. To nie jest błąd.
Rdzeń silnika (`src/`, `include/`) pozostaje nietknięty; cała warstwa Jozz
żyje w `samples/jozz_vehicle_*`, `assets/`, `tools/` i `docs/`.

## Gdzie jest aktualny stan projektu

Ten plik celowo NIE utrzymuje kopii statusu. Jedno źródło prawdy:

```text
docs/CURRENT_STATE_INDEX_PL.md   <- aktualny stan, baseline, hotkeys, komendy
README_FOR_AGENTS.md             <- wejście dla agentów AI, no-go listy
```

Historia decyzji: `docs/adr/0001..0005`. Raporty milestone'ów: `docs/M*_PL.md`.

## Szybki start

```powershell
cmake --build --preset windows-debug --target samples
cmake --build --preset windows-debug --target jozz_vehicle_validation
build\bin\Debug\jozz_vehicle_validation.exe   # musi kończyć się: OK
build\bin\Debug\samples.exe                   # uruchamiać z katalogu repo
```

W pickerze sampli (kategoria `Jozz Vehicle`):

```text
M5 First Drivable         <- jeżdżący pojazd: W/S/A/D, Space, T
Lab M2 Primitive Corner   <- izolowany narożnik zawieszenia (M2.5+M3+M4)
Lab M1 Smoke              <- historyczny smoke test
```

Uwaga środowiskowa: wrapper `cmd /c "set PATH=& ..."` z wcześniejszych wersji
tego README nie działa w Git Bash (cicho nic nie robi). Wołaj cmake
bezpośrednio; szczegóły w `docs/CURRENT_STATE_INDEX_PL.md` sekcja 9.

## Najważniejsza lekcja fizyki (niezmienna)

```text
b3WheelJoint ma implicit rest translation = 0.
Frame A = rest wheel-center anchor na chassis.
Frame B = centrum koła.
Rest drop jest jawny; wizualne sockety nie są frame'ami fizyki.
```

Nie wracamy do modelu, w którym Frame A jest wizualnym mountem
amortyzatora/chassis (błąd M2.3).

## Assety

```text
assets/source/     modele źródłowe glTF (research/startup, nie finalne)
assets/contracts/  sidecar kontrakty *.asset.json (runtime binding source)
assets/reports/    raporty audytu (tylko diagnostyka)
```

Duplikaty nazw node'ów w glTF są faktem — nie ufać nazwom bez
indeksu/ścieżki/parent chain. Audyt: `py tools\asset_audit.py` (świadomie —
nadpisuje raporty w repo).
