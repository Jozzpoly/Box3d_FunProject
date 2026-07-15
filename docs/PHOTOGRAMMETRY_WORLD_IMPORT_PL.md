# Jozz Vehicle — Photogrammetry World Import

## Status i źródło prawdy

- Branch eksperymentu: `Photogrametry_Import_experiment`.
- `main` pozostaje nietykalny.
- Właściciel kierunku i ręcznej akceptacji: Jozz.
- Ten dokument jest operacyjnym frontem eksperymentu; kod i najnowsze testy są ważniejsze niż starsze plany.
- Aktualny etap: **P1 — dependency-free Scan Inspector**.
- Nie przechodzić do P2/P3 bez akceptacji Jozza.

## Cel produktu

Zbudować bezpieczny pipeline, który przekształca jeden lub kilka skanów GLB w świat, po którym Jozz Vehicle może stabilnie jeździć. Surowy mesh fotogrametryczny jest reprezentacją wizualną, a nie automatycznie poprawną kolizją.

Docelowo świat może posiadać trzy rozdzielone warstwy:

```text
visual photogrammetry mesh
continuous drivable ground
simplified obstacle collision
```

Nie projektujemy teraz kompletnego silnika otwartego świata. Najpierw kolejno udowadniamy:

```text
1. Rozumiemy dane.
2. Widzimy dane poprawnie.
3. Rozumiemy szwy.
4. Potrafimy wydobyć wiarygodny grunt na małym obszarze.
5. Samochód przejeżdża przez najtrudniejszy szew.
6. Dopiero wtedy skalujemy rozwiązanie.
```

## Potwierdzone fakty dla pierwszej paczki

Paczka zawiera cztery GLB:

```text
MipTile_0.glb
MipTile_1.glb
MipTile_2.glb
MipTile_3.glb
```

Łącznie:

```text
1 012 211 wierzchołków
3 626 238 indeksów
1 208 746 trójkątów
18 meshów / primitive'ów
18 materiałów
18 osadzonych tekstur JPEG
brak NORMAL
brak TANGENT
obecne POSITION + TEXCOORD_0
```

Globalne bounds:

```text
min:    [-572.745483398, -444.656463623, 291.493927002]
max:    [ 580.308593750,  560.300842285, 410.869445801]
extent: [1153.054077148, 1004.957305908, 119.375518799]
```

Hipoteza osi dla P1:

```text
source X/Y = płaszczyzna pozioma
source Z   = wysokość
```

To nadal hipoteza. Skala i osie muszą zostać potwierdzone znanym realnym odcinkiem przed runtime importem.

Rozkład tekstur:

```text
8192x8192: 7
4096x4096: 4
2048x2048: 4
1024x1024: 3
```

`MipTile_1` jest wąskim pasem łączeniowym, nie regularną ćwiartką mapy. Nie wolno zakładać układu 2×2.

## Golden Seam Region

Pierwszy obowiązkowy region pilotażowy:

```text
source X: -32 .. 48
source Y: -32 .. 48
```

Obejmuje styk wszystkich czterech źródeł. Będzie używany kolejno do:

- wizualizacji overlapów;
- pomiaru residuali;
- pierwszego wspólnego DEM;
- porównania siatki 0,25 m i 0,50 m;
- pierwszego przejazdu przez najtrudniejszy szew.

## Prywatność i duże pliki

Surowe skany przedstawiają realne domy, ogrody i teren. Dlatego:

- raw GLB/ZIP nie trafiają automatycznie do publicznego Git;
- lokalne źródła żyją pod `local_assets/scans/`;
- wyniki robocze żyją pod `build/scan_pipeline/`;
- `local_assets/scans/` jest gitignored;
- repo przechowuje kod, małe syntetyczne fixtures, manifesty i bezpieczne raporty;
- Git LFS lub zewnętrzny storage wymagają osobnej decyzji Jozza.

## Nienegocjowalne zasady

1. Nie dotykać `src/` ani `include/` Box3D.
2. Nie zmieniać accepted vehicle behavior ani defaultów M6/M7/M8.
3. Nie rozszerzać `jozz_vehicle_visual_mesh` do roli world importera.
4. Nie tworzyć pustych placeholderów.
5. Najpierw reprodukcja i pomiar, potem architektura.
6. Render jest bramką dla pracy wizualnej.
7. Zielony validator nie zastępuje czytania liczb ani ręcznego testu Jozza.
8. Nie luzować progów testów, aby etap przeszedł.
9. Nie force-pushować i nie przepisywać historii.
10. Nie przechodzić do kolejnego etapu przez obejście kryterium akceptacji.

## Roadmapa

### L0 — Governance i Ground Truth

- potwierdzenie repo, brancha, remote i czystego drzewa;
- standardowy baseline projektu;
- hashe źródeł;
- polityka prywatności;
- jawne osie, skala i origin;
- zapis Golden Seam Region.

### P1 — Dependency-free Scan Inspector

Aktualny etap.

Implementacja:

```text
tools/scan_pipeline/scan_inspect.py
tests/scan_pipeline/test_scan_inspect.py
```

Inspector korzysta wyłącznie z Python standard library i obsługuje potrzebny podzbiór GLB 2.0:

- GLB header;
- JSON/BIN chunks;
- scenes i nodes;
- matrix/TRS;
- meshes i primitives;
- accessors i bufferViews;
- POSITION, TEXCOORD_0 i indeksy;
- materiały, tekstury i embedded JPEG/PNG;
- world bounds;
- overlapy XY;
- wykrywanie seam-strip candidate;
- deterministyczny JSON/Markdown;
- dwa deterministyczne PNG diagnostyczne.

Uruchomienie z repo root:

```powershell
python .\tools\scan_pipeline\scan_inspect.py `
  --input .\local_assets\scans\home_large `
  --output .\build\scan_pipeline\home_large `
  --name home-large
```

Wejście może być:

- pojedynczym `.glb`;
- katalogiem zawierającym GLB;
- archiwum `.zip` zawierającym GLB.

Wyjście:

```text
inspection.json
inspection.md
source_layout.png
texture_inventory.png
```

Testy:

```powershell
python -m unittest discover -s tests\scan_pipeline -p "test_*.py" -v
```

Bramka P1:

- testy syntetycznego GLB przechodzą;
- pięć uruchomień daje identyczne SHA raportu i PNG;
- counts odpowiadają realnej paczce;
- `MipTile_1` zostaje wykryty jako seam-strip candidate;
- Golden Seam Region jest zapisany;
- raw scan nie znajduje się w Git;
- standardowy `tools/gate.ps1` zostaje uruchomiony lokalnie na Windows;
- obrazy zostają faktycznie otwarte i ocenione.

### P2 — Parser Decision Spike

Po akceptacji P1 porównać:

```text
cgltf jako autorytatywny parser GLB/glTF
Python/Open3D jako warstwa eksperymentów geometrycznych
```

Nie integrować dwóch pełnych importerów.

### P3 — Izolowany Scan Preview Lab

- nowy sample scan-only;
- wszystkie źródła i materiały;
- jawny source→Box3D transform;
- origin rebase;
- unlit baseColor;
- maks. 1024² w preview;
- bez fizyki, LOD, spatial chunkingu i DDS jako blockera;
- fixed camera screenshots;
- ręczny sign-off Jozza.

**STOP: bez akceptacji orientacji, skali i renderu nie przechodzić do DEM.**

### P4 — Seam Measurement

- overlap AABB;
- liczba próbek;
- mediana, P90 i P95 residuali;
- różnica wysokości i normalnych;
- heatmapy;
- brak automatycznego stosowania ICP.

### P5 — Ground Candidate Extraction

Tylko Golden Seam Region i drugi Golden Drive Region:

- wszystkie przecięcia promieni;
- slope/normal/component metadata;
- ground seeds;
- flood fill po ciągłości;
- exclusion/forced-ground masks;
- confidence map;
- profile 0,25 m i 0,50 m.

Dach ani korona drzewa nie mogą zostać zaakceptowane jako grunt bez jawnego override'u.

### P6 — Heightfield Seam Experiment

- minimalny zestaw sąsiednich heightfieldów;
- jeden globalny min/max;
- identyczne próbki graniczne;
- debug granic;
- headless raycast/sweep/contact tests.

Różnica próbek na wspólnej krawędzi musi wynosić dokładnie zero.

### P7 — First Drive MVP

- nowy Scan Terrain Drive Lab;
- istniejący moduł pojazdu, bez kopii solvera;
- visual/collision/contact debug;
- spoczynek, wolny i szybki przejazd, hamowanie, skręt i lądowanie na szwie;
- porównanie 0,25 m vs 0,50 m;
- ręczny sign-off Jozza.

### P8 — Pełny cook świata

Odblokowany wyłącznie po P7. Najpierw statyczny load całego świata, nadal bez streamingu.

Rozmiar chunka wybiera benchmark:

```text
64 m
128 m
256 m
```

### P9–P11 — Optymalizacja według profilu

- 1024/2048 RGBA8;
- BC7 DDS dopiero po proofie;
- meshoptimizer i attribute-aware LOD;
- renderer refactor tylko po udowodnieniu realnego problemu.

Brak problemu oznacza brak refaktoru.

### P12 — Przeszkody

Najpierw ręczne i półautomatyczne proxy:

- budynki;
- mury;
- ważne krawężniki;
- duże skały i uskoki.

Bez kolizji dla liści, trawy, cienkich gałęzi i fotogrametrycznych floaterów.

### P13–P15 — Streaming, texture rebake i kolejne skany

Streaming odblokowuje profil, nie ambicja. System staje się produktem dopiero wtedy, gdy drugi niezależny skan przechodzi pipeline bez zmian C++.

## Aktualny wynik P1

Lokalna weryfikacja pierwszej paczki:

```text
4/4 testy syntetyczne: PASS
5/5 pełnych uruchomień: identyczne artefakty
inspection.json SHA-256:
3cda0a7c3b04b362c9c52a53ab9812b7c3232813fcba962b90c71df6684185bb
```

Potwierdzone przez inspector:

```text
files=4
vertices=1012211
triangles=1208746
materials=18
images=18
seam_candidates=MipTile_1.glb
```

Oba PNG zostały otwarte i ocenione:

- `source_layout.png` pokazuje trzy duże źródła, wąski pas `MipTile_1` i Golden Seam Region;
- `texture_inventory.png` pokazuje poprawną hierarchię 7×8K, 4×4K, 4×2K i 3×1K.

## Niewykonana bramka środowiskowa

W tej sesji nie było checkoutu Windows ani dostępu sieciowego z kontenera, dlatego nie wykonano uczciwie:

```powershell
.\tools\gate.ps1 -SaveBaseline
.\tools\gate.ps1 -DiffBaseline
```

To nie jest zaliczone domyślnie. P1 pozostaje technicznie zaimplementowany i przetestowany na danych, ale standardowy gate projektu musi zostać uruchomiony w lokalnym checkoutcie Windows przed oznaczeniem etapu jako w pełni zamkniętego.

## Protokół STOP

Agent zatrzymuje się, gdy:

- nie może potwierdzić brancha;
- źródła są niekompletne;
- skala lub osie wymagają decyzji Jozza;
- trzeba zmienić accepted vehicle behavior;
- trzeba dotknąć Box3D core;
- kryterium etapu jest nieosiągalne;
- test przechodzi tylko po poluzowaniu progu;
- następna bramka wymaga ręcznego render/feel sign-offu.

W STOP należy podać dokładny stan, pytanie i nie rozpoczynać kolejnego etapu.
