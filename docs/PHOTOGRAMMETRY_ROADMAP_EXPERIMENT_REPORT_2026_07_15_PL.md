# Photogrammetry World Import — eksperymentalna walidacja roadmapy

**Data:** 2026-07-15  
**Branch:** `Photogrametry_Import_experiment`  
**Status:** eksperymenty offline zakończone; runtime C++/Box3D nadal niezaimplementowany  
**Cel:** dotknąć realnej implementacji wielu etapów roadmapy, wykryć błędne założenia przed budową właściwego importera i zapisać mierzalny feedback dla kolejnego agenta.

---

## 1. Werdykt

Roadmapa została **częściowo potwierdzona**, ale kilka jej ważnych założeń wymaga korekty.

Potwierdzone:

- struktura GLB i transformacje mogą być wiarygodnie odtwarzane;
- osobna warstwa wizualna i osobny ground collision są konieczne;
- wspólny globalny DEM przed podziałem na heightfieldy jest właściwym kierunkiem;
- identyczne graniczne próbki oraz jeden globalny zakres kwantyzacji rzeczywiście eliminują matematyczny szew heightfieldów;
- tekstury są większym problemem pamięciowym niż geometria;
- 1K jest bardzo rozsądnym profilem preview;
- pipeline musi być offline, etapowy i cache’owalny.

Podważone lub zmienione:

- sam seed-driven continuity flood **nie odróżnia gruntu od dachów**;
- 0,25 m nie jest automatycznie lepsze od 0,50 m — zachowuje więcej szumu;
- uniform chunk 128 m nie jest dobrym budżetem renderowym przy tak nierównej gęstości;
- `Golden Seam Region` nadaje się do stress-testu szwów i klasyfikacji, ale nie powinien automatycznie stać się pierwszym obszarem jazdy;
- globalne ICP byłoby niebezpieczne — residuale są lokalne i semantyczne;
- jedna monolityczna komenda cookera jest błędem operacyjnym;
- przejście kinematycznego probe’u nie oznacza semantycznie poprawnego gruntu.

Najważniejszy wniosek:

> Pierwszy runtime MVP powinien użyć ręcznie zatwierdzonego, małego Golden Drive Region, profilu 0,50 m i jawnych masek ground/exclusion. Automatyczna klasyfikacja całego kilometra przed pierwszą jazdą byłaby przedwczesna.

---

## 2. Zaimplementowane eksperymenty

Dodane narzędzia:

```text
tools/scan_pipeline/scan_geometry.py
tools/scan_pipeline/scan_plan_experiments.py
tools/scan_pipeline/scan_ground_filters.py
tools/scan_pipeline/scan_drive_probe.py
tools/scan_pipeline/scan_chunk_probe.py
tools/scan_pipeline/scan_adaptive_chunk_probe.py
tools/scan_pipeline/scan_texture_quality_probe.py

tests/scan_pipeline/test_scan_plan_experiments.py
```

Zakres realnej implementacji:

1. niezależna, NumPy-owa ekstrakcja transformowanych trójkątów z GLB;
2. cross-check wyników z `trimesh`;
3. rzeczywisty pomiar corridorów overlapu;
4. rasteryzacja trójkątów do globalnego DEM;
5. top/bottom/continuity hypotheses;
6. trzy profile progressive morphological filtering;
7. generacja 2×2 heightfieldów z identycznymi krawędziami;
8. wspólna kwantyzacja `uint16`;
9. eksport debug OBJ po transformacji source Z-up → Box3D Y-up;
10. kinematyczny probe czterech kół według footprintu M6;
11. pełnoświatowy test chunków 64/128/256 m;
12. adaptacyjny quadtree load-balance probe;
13. prawdziwe resize’y wszystkich 18 tekstur do 1K i 2K;
14. PSNR/SSIM 1K względem referencji 2K;
15. etapowe uruchamianie `core` i `textures` z checkpointem raportu.

Testy syntetyczne:

```text
12/12 PASS
```

Obejmują:

- uszkodzony GLB i accessor;
- counts, bounds i embedded image;
- deterministyczne artefakty inspectora;
- rasteryzację znanej płaszczyzny;
- odzyskanie znanego offsetu pionowego szwu;
- identyczność wspólnych krawędzi heightfieldów;
- usunięcie syntetycznego budynku filtrem morfologicznym;
- transformację osi i upward winding OBJ;
- płaski przejazd czterech kół;
- wykrycie poprzecznego stopnia pod kołami;
- dokładne assignmenty trójkątów przecinających granicę chunka.

---

## 3. Parser i geometria

Nasz parser/adapter został porównany z niezależnym `trimesh` dla wszystkich czterech GLB.

Wynik:

```text
4/4 źródła: counts identyczne
4/4 źródła: bounds identyczne
maksymalny błąd bounds: < 5e-10 jednostki źródłowej
```

Pełna paczka:

```text
vertices:  1 012 211
triangles: 1 208 746
materials: 18
images:    18
```

Trójkąty przecinające Golden Seam Region:

```text
MipTile_0:  56 131
MipTile_1: 137 628
MipTile_2:  59 940
MipTile_3:  27 164
```

### Wniosek

P1 parser jest wystarczająco wiarygodny jako narzędzie evidence/inspection. Produkcyjny cooker nadal powinien rozważyć `cgltf`, ale nie ma już potrzeby traktować parsera jako głównego ryzyka projektu.

Nie należy integrować `trimesh` ani NumPy do runtime. To narzędzia offline.

---

## 4. Pomiar szwów — poprawiona interpretacja

Pierwsza implementacja nearest-neighbour porównywała także punkty leżące w różnych częściach regionu i dawała mylące residuale rzędu dziesiątek metrów.

Poprawka:

- każda para jest ograniczana do faktycznego overlap/contact corridor;
- dopasowania muszą znajdować się w promieniu 1 m w płaszczyźnie XY;
- osobno raportowana jest różnica pionowa i pełny dystans 3D;
- para bez wspólnego corridoru nie dostaje fałszywej metryki.

Wyniki pionowego residualu:

| Para | Mediana | P95 | Interpretacja |
|---|---:|---:|---|
| Tile 0 ↔ 1 | 0,175 m | 4,115 m | dobry rdzeń, silne lokalne outliery |
| Tile 0 ↔ 2 | 0,057 m | 3,762 m | dobry rdzeń, lokalne obiekty/artefakty |
| Tile 0 ↔ 3 | — | — | źródła rozłączne w regionie |
| Tile 1 ↔ 2 | 0,116 m | 5,154 m | najtrudniejszy corridor |
| Tile 1 ↔ 3 | 0,036 m | 1,772 m | ogólnie lepszy, nadal outliery |
| Tile 2 ↔ 3 | 0,021 m | 0,238 m | bardzo dobra zgodność |

### Wniosek

Niskie mediany i wysokie P95 oznaczają, że problem nie wygląda jak jeden globalny rigid offset. Bardziej prawdopodobne są:

- różne rekonstrukcje roślinności;
- dachy i krawędzie budynków;
- lokalne floatery;
- różnice coverage;
- pojedyncze błędne powierzchnie.

**Globalne ICP pozostaje zabronione.** Ewentualna korekta może dotyczyć wyłącznie starannie zamaskowanego gruntu i musi mieć raport before/after.

---

## 5. Ground extraction

### 5.1. Surowy continuity DEM

Coverage:

```text
0,50 m: 98,93%
0,25 m: 96,78%
```

Jednak obrazy wykazały, że continuity surface nadal zawiera:

- dachy;
- podniesione obiekty;
- fragmenty roślinności;
- ostre przejścia między warstwami.

P95 nachylenia wynosi około 59°, a maksymalne lokalne nachylenia przekraczają 85°.

Kinematyczny probe M6:

```text
P95 artykulacji: 2,93–4,21 m
budżet travel:    0,70 m
P95 pitch/roll:   około 56–67°
```

To jednoznaczny FAIL.

### 5.2. Progressive morphological filtering

Przetestowano profile:

```text
gentle
balanced
aggressive
```

Dla 0,50 m:

| Profil | Cells usunięte >1 m | P95 slope po filtrze | P95 artykulacji w drive probe |
|---|---:|---:|---:|
| gentle | 11,1% | 34,1° | 0,368 m |
| balanced | 12,9% | 28,5° | 0,334 m |
| aggressive | 15,1% | 29,9° | 0,270 m |

Dla 0,25 m:

| Profil | Cells usunięte >1 m | P95 slope po filtrze | P95 artykulacji w drive probe |
|---|---:|---:|---:|
| gentle | 9,0% | 43,8° | 0,339 m |
| balanced | 10,8% | 35,9° | 0,335 m |
| aggressive | 13,1% | 37,6° | 0,375 m |

### Krytyczna obserwacja

Profil `gentle` przechodzi provisional kinematic gate dla obu rozdzielczości, ale wizualnie nadal zachowuje fragmenty podniesionych struktur. Profil `aggressive` usuwa więcej dachów, lecz może naruszać rzeczywiste skarpy i krawędzie terenu.

Zatem:

```text
kinematic PASS != semantic ground truth
```

### Nowa decyzja

MVP ground extraction musi być półautomatyczny:

1. automatyczny candidate DEM;
2. object-height map;
3. ręczny `groundSeeds`;
4. ręczne `excludedPolygons`;
5. ręczne `forcedGroundPolygons`;
6. fixed-camera/overlay review;
7. kinematic four-wheel probe;
8. dopiero potem Box3D runtime.

---

## 6. 0,25 m kontra 0,50 m

0,25 m:

- zachowuje więcej lokalnego detalu;
- ma cztery razy więcej punktów;
- ma więcej brakujących komórek;
- zachowuje więcej wysokoczęstotliwościowego szumu;
- nie daje konsekwentnie niższych wymagań dla zawieszenia.

0,50 m:

- ma lepszą coverage;
- jest około cztery razy mniejsze;
- szybciej się rasteryzuję;
- w drive probe jest przynajmniej równie stabilne;
- daje 161×161 punktów dla regionu 80×80 m.

### Korekta roadmapy

```text
MVP default: 0,50 m
0,25 m: tylko ręcznie wybrane high-detail zones po runtime sign-offie
```

Nie należy generować 0,25 m dla całego świata jako pierwszego kroku.

---

## 7. Heightfield seam i format danych

Dla obu rozdzielczości wygenerowano 2×2 chunki:

```text
0,50 m: global 161×161, chunk 81×81
0,25 m: global 321×321, chunk 161×161
```

Każdy wariant używa:

- jednego globalnego minimum;
- jednego globalnego maksimum;
- wspólnej kwantyzacji `uint16`;
- skopiowanych granicznych rzędów i kolumn.

Wynik:

```text
south vertical edge: exact
north vertical edge: exact
west horizontal edge: exact
east horizontal edge: exact
```

Dla wszystkich surowych i morfologicznych wariantów:

```text
allSharedEdgesExact = true
```

### Wniosek

Fundamentalna strategia heightfieldów jest poprawna. Ryzykiem nie jest matematyczny szew po podziale, lecz jakość powierzchni wejściowej.

---

## 8. Source → Box3D i eksport debug mesh

Zaimplementowana transformacja:

```text
boxX = sourceX - originX
boxY = sourceZ - originZ
boxZ = -(sourceY - originY)
```

Pierwszy eksport OBJ miał poprawne osie, ale błędny winding:

```text
100% face normals skierowane w dół
```

Po naprawie i dodaniu testu:

```text
6/6 eksportów: 100% face normals skierowane w +Y
```

Rozmiary:

```text
0,50 m: 25 921 vertices, 51 200 triangles
0,25 m: 103 041 vertices, 204 800 triangles
```

### Wniosek

Transformację osi i winding trzeba traktować jako osobny kontrakt z testem numerycznym. Sam poprawny obraz bounds nie wystarcza.

---

## 9. Kinematic four-wheel drive probe

Probe wykorzystuje defaulty M6:

```text
wheelbase: 2,50 m
track:     2,10 m
total suspension travel hint: 0,70 m
```

Dla każdej powierzchni i rozdzielczości analizuje:

- automatycznie wykryte source-owner seams;
- przejazd E–W;
- przejazd N–S;
- diagonalę;
- cztery wysokości kół;
- pitch;
- roll;
- cross-axle articulation;
- height step;
- aproksymowaną vertical acceleration dla 5/15/30 m/s.

### Wynik

Surowy continuity:

```text
FAIL krytyczny
P95 articulation: do 4,21 m
travel exceed na części tras: 20–30%
```

Morfologia 0,50 m:

```text
gentle:     provisional PASS
balanced:   near-pass; pojedyncze przekroczenia travel
aggressive: provisional PASS
```

Morfologia 0,25 m:

```text
gentle:     provisional PASS
balanced:   near-pass
aggressive: near-pass
```

### Ograniczenie

To nie jest solver Box3D. Probe nie modeluje:

- opony;
- bezwładności;
- tłumienia;
- prędkościowego odrywania koła;
- kolizji bocznych;
- rzeczywistej dynamiki chassis.

Jest jednak skutecznym pre-gate’em: powierzchnia wymagająca 3–4 m artykulacji nie powinna trafiać do runtime.

---

## 10. Fixed chunk probe 64/128/256 m

Pełny świat został rozdzielony offline na trzy uniform profile.

| Chunk | Grid | Non-empty | Duplication ratio | Najcięższy chunk |
|---:|---:|---:|---:|---:|
| 64 m | 19×16 | 209 | 1,0172 | 250 127 tri |
| 128 m | 10×9 | 66 | 1,0085 | 371 501 tri |
| 256 m | 6×5 | 22 | 1,0052 | 395 013 tri |

Dla 128 m:

```text
median: 488 tri/chunk
P95:    123 477
max:    371 501
```

To skrajnie nierówny rozkład.

### Wniosek

Uniform grid jest nadal dobry jako:

- adresowanie świata;
- heightfield contract;
- streaming ownership;
- manifest i sąsiedztwo.

Nie powinien jednak być jedyną jednostką renderowego budżetu. Ciężkie chunki potrzebują wewnętrznego podziału na sections/clusters/meshlets.

---

## 11. Adaptive quadtree probe

To wyłącznie test load-balancingu po centroidach, bez boundary duplication.

| Target | Leaves | Median tri | P95 tri | Max tri |
|---:|---:|---:|---:|---:|
| 25k | 111 | 9 340 | 25 376 | 46 329 |
| 50k | 87 | 11 284 | 35 539 | 47 558 |
| 100k | 36 | 24 218 | 87 674 | 97 642 |

### Wniosek

Adaptive partition znacznie lepiej kontroluje maksymalny ciężar niż uniform grid. Nie oznacza to automatycznego wyboru quadtree runtime.

Najbardziej obiecujący model:

```text
uniform world chunks
    + adaptive render sections wewnątrz chunka
    + osobne heightfield chunks
```

Należy odłożyć decyzję o finalnym formacie do P3/P8, po pomiarze prawdziwego renderera.

---

## 12. Tekstury

Prawdziwy resize wszystkich 18 obrazów:

| Profil | Encoded JPEG | Szacowany RGBA8 + mip |
|---|---:|---:|
| max 1K | 4,74 MB | 100,66 MB (~96 MiB) |
| max 2K | 13,68 MB | 352,32 MB (~336 MiB) |

Native decoded RGBA bez pełnego runtime cookingu przekracza 2,2 GB.

### Quality probe 1K vs 2K

1K zostało upsample’owane do referencji 2K; metryki liczone na proxy maks. 512 px.

```text
SSIM min:    0,99843
SSIM median: 0,99880
PSNR min:    49,10 dB
PSNR median: 50,42 dB
```

Wizualne worst-case sheets pokazują różnice głównie na drobnych krawędziach i wysokiej częstotliwości, przy czym źródło samo jest mocno rozmyte i zawiera padding.

### Korekta roadmapy

```text
P3 preview default: 1K
2K: opcjonalny profil quality A/B
BC7: dopiero po działającym renderze i fixed-camera comparison
```

Metryki atlasu nie zastępują runtime screenshotów z bliskiej kamery.

---

## 13. Pipeline musi być etapowy

Pierwszy monolityczny run zakończył core, lecz późny etap tekstur przekroczył limit i nie zapisał finalnego raportu.

Po przebudowie:

```text
core:     24,55 s
textures: 13,20 s
```

Każdy etap zapisuje checkpoint `experiment_report.json`.

Drugi niezależny core run:

```text
95/95 artefaktów byte-identical
raport semantycznie identyczny po usunięciu timingów
```

### Docelowy model cookera

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

Każdy etap:

- osobny hash wejść;
- osobny katalog wyniku;
- możliwość resume;
- jawna wersja algorytmu;
- brak utraty wcześniejszych wyników po błędzie późnej fazy.

---

## 14. Czego nie zwalidowano

Nie wykonano:

- C++ `cgltf` spike;
- Scan Preview Lab w rendererze;
- prawdziwej integracji z Box3D;
- rzeczywistego `b3HeightFieldShape`;
- kontaktów kół i solvera;
- meshoptimizer/LOD;
- DDS/BC7;
- runtime culling;
- streamingu;
- ręcznej kalibracji skali znanym odcinkiem;
- ręcznego wyboru Golden Drive Region;
- standardowego Windows `gate.ps1`.

Brak tych elementów nie jest zaliczony domyślnie.

---

## 15. Zmieniona kolejność następnych prac

### Następny etap A — ręczna walidacja danych

1. uruchomić Windows `gate.ps1`;
2. potwierdzić skalę znanym realnym odcinkiem;
3. obejrzeć mapy:
   - continuity;
   - object height;
   - balanced/aggressive ground;
   - seam heatmap;
4. wybrać Golden Drive Region z realną drogą;
5. zapisać pierwsze `groundSeeds` i `excludedPolygons`.

### Następny etap B — minimalny C++ preview

1. `cgltf` read-only spike;
2. scan-only sample;
3. unlit baseColor max 1K;
4. source→Box3D transform;
5. fixed cameras;
6. bez fizyki i bez refaktoru wspólnego renderera;
7. ręczny sign-off Jozza.

### Następny etap C — pierwszy Box3D heightfield

1. tylko Golden Drive Region;
2. 0,50 m;
3. ręcznie zatwierdzony ground;
4. jeden lub cztery heightfieldy;
5. debug visual/collision difference;
6. spokojny spawn;
7. powolny przejazd;
8. dopiero potem seam stress-test.

### Nadal zablokowane

- pełny kilometr runtime;
- streaming;
- BC7 jako blocker;
- automatyczne obstacle compounds;
- globalne ICP;
- finalny `.jmesh` format;
- zmiany Box3D core.

---

## 16. Ostateczny werdykt dla roadmapy

Najważniejsze założenie roadmapy pozostaje poprawne:

```text
visual mesh != drivable ground != obstacle collision
```

Jednak kolejność musi być jeszcze bardziej konserwatywna:

```text
inspect
→ real seam metrics
→ provisional candidate DEM
→ morphology/object-height diagnostics
→ manual semantic correction
→ four-wheel kinematic gate
→ small C++ preview
→ small Box3D drive region
→ dopiero full-world cook
```

Automatyzacja gruntu nie może wyprzedzać narzędzi ręcznej kontroli. Uniform world chunks mogą zostać kontraktem przestrzennym, ale nie powinny same definiować render budgetu. Profil 0,50 m + 1K jest obecnie najlepszym startowym kompromisem MVP.
