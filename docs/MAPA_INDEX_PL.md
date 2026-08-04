# Mapa i skany — bieżący indeks JV

Status: subsystem działa, lecz nie jest aktualnym głównym kierunkiem badań.
Historyczne roadmapy i raporty: `archive/map_scan_2026-07/`.

## W kodzie istnieje

- wspólna płyta i proceduralny teren offroad;
- centralny kampus testowy z odzyskanym obstacle kitem;
- import paczki skanu, teksturowany mesh i cache ugotowanego BVH;
- zakładka Mapa;
- checkpoint po `R`;
- spawny session/persistent per fragment;
- szew pod wiele paczek skanu.

## Status produktu

Implementacja jest zachowana jako działający subsystem i źródło doświadczeń.
Manualny odbiór wizualny/jezdny przez Jozza nie został domknięty, więc mapa nie
jest „ukończonym produktem”. Nie należy rozpoczynać dawnych etapów 3–6 tylko
dlatego, że istnieją ich plany w archiwum.

## Najważniejsze pliki kodu

- `samples/jozz_vehicle_world_layout.h`;
- `samples/jozz_vehicle_world_terrain.{h,cpp}`;
- `samples/jozz_vehicle_m6_rig_lab.cpp` i pliki `_persistence`/`_ui_tabs`;
- reader i renderer skanu w odpowiadających plikach `jozz_scan_*`;
- `assets/vehicle_spawns.txt`.

## Wartość dla JES

Promować: rozdział fragmentów, kontrakt paczki skanu, cache gotowanej kolizji,
dwa poziomy spawnu i zasadę, że restart nie usuwa aktywnego świata. Nie promować
automatycznie konkretnego układu płyta/offroad/kampus ani starej roadmapy E1–E6.
