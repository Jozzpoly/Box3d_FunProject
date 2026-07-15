# Jozz Vehicle — Photogrammetry World Import

## Status i źródło prawdy

- Branch eksperymentu: `Photogrametry_Import_experiment`.
- `main` pozostaje nietykalny.
- Właściciel kierunku i ręcznej akceptacji: Jozz.
- Kod, testy i ten dokument są ważniejsze niż wcześniejsze rozmowy.
- P1 inspector jest zaimplementowany.
- Wieloetapowa roadmapa została dotknięta eksperymentalnie offline.
- Pełne wyniki: `docs/PHOTOGRAMMETRY_ROADMAP_EXPERIMENT_REPORT_2026_07_15_PL.md`.
- Runtime C++/Box3D nadal nie jest zaimplementowany.

## Cel produktu

Pipeline ma przekształcać skany GLB w świat, po którym Jozz Vehicle może stabilnie jeździć.

Trzy warstwy pozostają rozdzielone:

```text
visual photogrammetry mesh
continuous drivable ground
simplified obstacle collision
```

Surowy mesh fotogrametryczny nie jest automatycznie prawidłową kolizją.

## Potwierdzone dane pierwszej paczki

```text
GLB:        4
vertices:   1 012 211
triangles:  1 208 746
materials:  18
images:     18 JPEG
NORMAL:     brak
TANGENT:    brak
```

Globalne bounds:

```text
min:    [-572.745483398, -444.656463623, 291.493927002]
max:    [ 580.308593750,  560.300842285, 410.869445801]
extent: [1153.054077148, 1004.957305908, 119.375518799]
```

Hipoteza osi:

```text
source X/Y = płaszczyzna pozioma
source Z   = wysokość
```

Skala i osie nadal wymagają ręcznego potwierdzenia znanym realnym odcinkiem.

`MipTile_1` jest wąskim pasem łączeniowym, nie regularną ćwiartką mapy.

## Golden Seam Region

```text
source X: -32 .. 48
source Y: -32 .. 48
```

Region obejmuje styk wszystkich czterech źródeł. Jest stress-testem szwów i klasyfikacji, ale nie jest automatycznie pierwszym obszarem jazdy.

Pierwszy runtime drive region ma zostać ręcznie wybrany po renderze jako **Golden Drive Region**.

## Prywatność i duże pliki

- Raw GLB/ZIP nie trafiają automatycznie do publicznego Git.
- Lokalne źródła: `local_assets/scans/`.
- Wyniki robocze: `build/scan_pipeline/`.
- `local_assets/scans/` jest gitignored.
- Repo przechowuje kod, małe fixtures, manifesty i bezpieczne raporty.
- Git LFS lub zewnętrzny storage wymagają osobnej decyzji Jozza.

## Zaimplementowane narzędzia

```text
tools/scan_pipeline/scan_inspect.py
tools/scan_pipeline/scan_geometry.py
tools/scan_pipeline/scan_plan_experiments.py
tools/scan_pipeline/scan_ground_filters.py
tools/scan_pipeline/scan_drive_probe.py
tools/scan_pipeline/scan_chunk_probe.py
tools/scan_pipeline/scan_adaptive_chunk_probe.py
tools/scan_pipeline/scan_texture_quality_probe.py
tools/scan_pipeline/requirements-experiments.txt

tests/scan_pipeline/test_scan_inspect.py
tests/scan_pipeline/test_scan_plan_experiments.py
```

Są to narzędzia offline. NumPy, SciPy, Pillow, trimesh i scikit-image nie są zależnościami runtime gry.

## Uruchomienie inspectora

Z repo root:

```powershell
python .\tools\scan_pipeline\scan_inspect.py `
  --input .\local_assets\scans\home_large `
  --output .\build\scan_pipeline\home_large `
  --name home-large
```

Wejście może być pojedynczym GLB, katalogiem lub ZIP-em.

## Uruchomienie eksperymentów

Instalacja środowiska badawczego:

```powershell
python -m pip install -r .\tools\scan_pipeline\requirements-experiments.txt
```

Core bez tekstur:

```powershell
python .\tools\scan_pipeline\scan_plan_experiments.py `
  --input .\local_assets\scans\home_large `
  --output .\build\scan_pipeline\roadmap_core `
  --name home-large `
  --only core
```

Tekstury jako osobny, wznawialny etap:

```powershell
python .\tools\scan_pipeline\scan_plan_experiments.py `
  --input .\local_assets\scans\home_large `
  --output .\build\scan_pipeline\roadmap_textures `
  --name home-large `
  --only textures
```

Kinematiczny four-wheel probe:

```powershell
python .\tools\scan_pipeline\scan_drive_probe.py `
  --core-output .\build\scan_pipeline\roadmap_core `
  --output .\build\scan_pipeline\drive_probe
```

Uniform chunk probe:

```powershell
python .\tools\scan_pipeline\scan_chunk_probe.py `
  --input .\local_assets\scans\home_large `
  --output .\build\scan_pipeline\chunk_probe.json
```

Adaptive load-balance probe:

```powershell
python .\tools\scan_pipeline\scan_adaptive_chunk_probe.py `
  --input .\local_assets\scans\home_large `
  --output .\build\scan_pipeline\adaptive_chunks
```

Texture quality probe:

```powershell
python .\tools\scan_pipeline\scan_texture_quality_probe.py `
  --input .\local_assets\scans\home_large `
  --output .\build\scan_pipeline\texture_quality
```

## Testy

```powershell
python -m unittest discover -s tests\scan_pipeline -p "test_*.py" -v
```

Aktualny wynik eksperymentów:

```text
12/12 PASS
```

Dwa pełne przebiegi `core` wygenerowały 95/95 identycznych plików. Raporty są semantycznie identyczne po pominięciu timingów.

## Najważniejsze zwalidowane wnioski

### Parser

- Nasz parser i trimesh zgadzają się dla 4/4 GLB.
- Counts są identyczne.
- Maksymalny błąd bounds jest mniejszy niż 5e-10.
- Parser nie jest już głównym ryzykiem projektu.

### Szwy

Niskie mediany i wysokie P95 wskazują lokalne problemy semantyczne, nie jeden globalny rigid offset.

```text
Tile 0–1: median 0,175 m, P95 4,115 m
Tile 0–2: median 0,057 m, P95 3,762 m
Tile 1–2: median 0,116 m, P95 5,154 m
Tile 1–3: median 0,036 m, P95 1,772 m
Tile 2–3: median 0,021 m, P95 0,238 m
```

**Globalne ICP pozostaje zabronione.**

### Ground extraction

Surowy continuity flood nie odróżnia gruntu od dachów.

```text
raw P95 articulation: 2,93–4,21 m
M6 travel hint:       0,70 m
```

Morfologia obniża wymagania do około 0,27–0,38 m P95, ale nie tworzy semantycznej prawdy. Ręczne maski i ground seeds są obowiązkowe.

### Rozdzielczość

```text
MVP default: 0,50 m
0,25 m: tylko ręcznie wybrane high-detail zones
```

0,25 m zachowuje więcej szumu i nie jest automatycznie lepsze dla pojazdu.

### Heightfieldy

- 0,50 m: global 161×161, chunk 81×81.
- 0,25 m: global 321×321, chunk 161×161.
- Jeden globalny min/max i wspólna kwantyzacja uint16.
- Wszystkie cztery wspólne krawędzie są matematycznie identyczne.

Strategia podziału heightfieldów jest potwierdzona. Ryzykiem pozostaje jakość wejściowego gruntu.

### Source → Box3D

```text
boxX = sourceX - originX
boxY = sourceZ - originZ
boxZ = -(sourceY - originY)
```

Winding eksportu jest testowany numerycznie; finalne OBJ mają normalne skierowane w +Y.

### Chunki

Uniform 128 m ma niski koszt duplikacji, ale skrajnie nierówny load:

```text
median: 488 tri
P95:    123 477 tri
max:    371 501 tri
```

Najbardziej prawdopodobna architektura:

```text
uniform world/physics chunks
+ adaptive render sections wewnątrz ciężkich chunków
```

Uniform grid nie może sam definiować render budgetu.

### Tekstury

```text
1K: około 96 MiB RGBA8 + mip
2K: około 336 MiB RGBA8 + mip
native decoded: ponad 2,2 GB
```

Quality probe 1K względem 2K:

```text
SSIM median: 0,99880
PSNR median: 50,42 dB
```

P3 preview zaczyna od 1K. 2K jest profilem A/B. BC7 nie jest blockerem pierwszego renderu.

### Pipeline

Monolityczne `all` jest niebezpieczne operacyjnie. Cooker ma być etapowy, checkpointowany i wznawialny:

```text
inspect
geometry_extract
seam_measure
pilot_dem
manual_review
heightfield_cook
texture_cook
render_lod_cook
package
```

## Zmieniona kolejność prac

### A. Ręczna walidacja danych

1. Uruchomić Windows `gate.ps1`.
2. Potwierdzić skalę znanym odcinkiem.
3. Obejrzeć continuity, object-height, balanced/aggressive ground i seam heatmap.
4. Wybrać Golden Drive Region z realną drogą.
5. Zapisać pierwsze `groundSeeds`, `excludedPolygons` i `forcedGroundPolygons`.

### B. Minimalny C++ preview

1. Read-only `cgltf` spike.
2. Osobny scan-only sample.
3. Unlit baseColor max 1K.
4. Jawny source→Box3D transform.
5. Fixed cameras i screenshoty.
6. Bez fizyki i bez refaktoru wspólnego renderera.
7. Ręczny sign-off Jozza.

### C. Pierwszy Box3D heightfield

1. Tylko Golden Drive Region.
2. Profil 0,50 m.
3. Ręcznie zatwierdzony ground.
4. Jeden lub cztery heightfieldy.
5. Visual/collision difference overlay.
6. Spokojny spawn i powolny przejazd.
7. Dopiero potem stress-test Golden Seam Region.

## Nadal zablokowane

- pełny kilometr runtime;
- streaming;
- BC7 jako blocker;
- automatyczne obstacle compounds;
- globalne ICP;
- finalny `.jmesh` format;
- zmiany Box3D core.

## Nienegocjowalne zasady

1. Nie dotykać `src/` ani `include/` Box3D.
2. Nie zmieniać accepted vehicle behavior M6/M7/M8.
3. Nie rozszerzać `jozz_vehicle_visual_mesh` do roli world importera.
4. Nie commitować raw scanów.
5. Nie tworzyć pustych placeholderów.
6. Najpierw pomiar i render, potem architektura.
7. Kinematic PASS nie oznacza semantic PASS.
8. Nie luzować progów testów.
9. Nie force-pushować.
10. Ręczny sign-off Jozza jest realną bramką.

## Niewykonane bramki

Nie wykonano jeszcze:

- Windows `tools/gate.ps1`;
- C++ build i testów;
- prawdziwego renderu skanu;
- `b3HeightFieldShape`;
- kontaktów kół w solverze;
- kalibracji skali;
- ręcznego wyboru Golden Drive Region.

Nie wolno oznaczać tych elementów jako zaliczone domyślnie.
