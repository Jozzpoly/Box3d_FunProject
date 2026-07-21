# Photogrammetry Import V2 — checkpoint P1

**Data:** 2026-07-21  
**Docelowy branch:** `photogrammetry/import-v2-foundation`  
**Branch stagingowy:** `agent/p1-dataset-inspector-staging`  
**Baza stagingu:** `36fcc91969c5d18d01abf3187d2c0ae7f49812bb`  
**Aktualna bramka:** automatyczna implementacja P1 gotowa do lokalnej reprodukcji; P2 pozostaje zablokowane

## P0

P0 zostało wykonane przez Jozza na Windows po poprawnej konfiguracji CMake.

Potwierdzony wynik:

```text
build samples: OK
build jozz_vehicle_validation: OK
build test: OK
validator: OK
test.exe: PASS
boot smoke: 0 sokol errors
BRAMKA: build 3/3 OK - walidator OK - test PASS - smoke 0 err
P0: PASS — baseline captured; P1 may start
```

Lokalny raport P0:

```text
build/scan_pipeline/p0_baseline/36fcc91969c5_20260721T002323Z/
```

Pierwszy nieudany przebieg ujawnił, że świeży clone wymaga wcześniejszego:

```powershell
cmake --preset windows
```

## Zakres implementacji P1

Dodano wyłącznie offline tooling i testy:

```text
tools/scan_pipeline/scan_ply.py
tools/scan_pipeline/scan_dataset_inspect.py
tests/scan_pipeline/test_scan_ply.py
tests/scan_pipeline/test_scan_dataset_inspect.py
```

Nie zmieniono:

- `src/` ani `include/` Box3D;
- kodu pojazdu M6/M7/M8;
- renderera;
- runtime C++;
- surowych danych skanu;
- istniejącego `scan_inspect.py`.

## `scan_ply.py`

Minimalny parser PLY obsługuje wymagany kontrakt point cloud:

- `binary_little_endian` i `binary_big_endian` PLY 1.0;
- ograniczony rozmiar nagłówka;
- scalar vertex properties;
- obowiązkowe `x`, `y`, `z`;
- opcjonalne `red`, `green`, `blue`;
- bounds, center, extent, count, stride i SHA-256;
- odczyt blokowy bez materializowania całych 24 mln punktów;
- opcjonalny chunkowany fast path NumPy;
- standard-library fallback bez obowiązkowej zależności NumPy.

Parser celowo odrzuca:

- ASCII PLY dla dużych danych;
- list properties w rekordzie vertex;
- variable-length element przed vertex;
- nieznane typy scalar;
- brak XYZ;
- ucięty payload;
- wartości XYZ `NaN`/`Inf`.

Komentarze i `obj_info` są czytane wyłącznie w celu przejścia nagłówka i nigdy nie trafiają do raportu. Chroni to przed przypadkowym ujawnieniem georeferencji lub lokalnych ścieżek.

## `scan_dataset_inspect.py`

Inspektor zestawu:

1. znajduje rekurencyjnie GLB i PLY;
2. wymusza unikalne Tile ID;
3. paruje `MipTile_N.glb` z `MipTile_N.ply`;
4. korzysta z istniejącego inspectora GLB;
5. przed jego uruchomieniem sprawdza, czy każdy accessor pozostaje wewnątrz własnego `bufferView`;
6. porównuje bounds, centroidy, extenty i overlap XY;
7. klasyfikuje pary jako:
   - `strong-match`,
   - `review`,
   - `incompatible`;
8. buduje rzeczywisty, streamowany grid evidence z punktów PLY;
9. zapisuje abstrakcyjne mapy diagnostyczne;
10. nigdy nie renderuje PLY RGB ani tekstur GLB.

`compatible` lub `compatible-review` nie oznacza automatycznej zgody na P2. Raport zawsze zachowuje:

```json
{
  "p2Unblocked": false,
  "scaleConfirmed": false,
  "axesConfirmed": false
}
```

Dopiero znany odcinek oraz ręczna ocena orientacji mogą zmienić ten stan w późniejszym, jawnym kroku.

## Artefakty P1

Każdy przebieg tworzy lokalnie:

```text
inspection.json
inspection.md
source_layout.png
point_density.png
vertical_spread.png
source_support.png
glb_ply_alignment.png
```

Mapy są abstrakcyjne:

- `source_layout.png` — bounds GLB i PLY;
- `point_density.png` — liczba punktów w komórce;
- `vertical_spread.png` — `zMax-zMin` w komórce;
- `source_support.png` — liczba źródeł wspierających komórkę;
- `glb_ply_alignment.png` — znormalizowane metryki par.

Nie są kopiowane do Git wyniki dla prywatnego skanu.

## Testy kontraktowe

`test_scan_ply.py` pokrywa:

- little endian;
- big endian;
- bounds, count, RGB i SHA-256;
- deterministyczne chunkowanie;
- brak wycieku `comment` i `obj_info`;
- odrzucenie ASCII;
- odrzucenie list property;
- odrzucenie braku XYZ;
- odrzucenie uciętego payloadu;
- limit nagłówka.

`test_scan_dataset_inspect.py` pokrywa:

- poprawne parowanie wielu kafli;
- klasyfikację `strong-match`;
- deterministyczność wszystkich siedmiu artefaktów;
- brak absolutnych ścieżek i prywatnych komentarzy;
- odrzucenie niesparowanych Tile ID;
- blokadę przestrzennie niezgodnego PLY;
- twardy expected-count gate;
- accessor wychodzący poza własny `bufferView`, nawet jeśli nadal mieści się w całym BIN.

## Lokalna reprodukcja po pobraniu stagingu

```powershell
git fetch origin
git switch agent/p1-dataset-inspector-staging

python -m unittest discover `
  -s .\tests\scan_pipeline `
  -p "test_*.py" `
  -v

python -m py_compile `
  .\tools\scan_pipeline\scan_ply.py `
  .\tools\scan_pipeline\scan_dataset_inspect.py
```

Po rozpakowaniu RAR do jednego lokalnego katalogu zawierającego oba poddrzewa `model-glb` i `model-ply`:

```powershell
python .\tools\scan_pipeline\scan_dataset_inspect.py `
  --input .\local_assets\scans\Model_skanu_extracted `
  --output .\build\scan_pipeline\p1_dataset_a `
  --name model-skanu `
  --expected-glb 7 `
  --expected-ply 7

python .\tools\scan_pipeline\scan_dataset_inspect.py `
  --input .\local_assets\scans\Model_skanu_extracted `
  --output .\build\scan_pipeline\p1_dataset_b `
  --name model-skanu `
  --expected-glb 7 `
  --expected-ply 7
```

Porównanie deterministyczności:

```powershell
$files = @(
  "inspection.json",
  "inspection.md",
  "source_layout.png",
  "point_density.png",
  "vertical_spread.png",
  "source_support.png",
  "glb_ply_alignment.png"
)

foreach ($file in $files) {
  $a = (Get-FileHash ".\build\scan_pipeline\p1_dataset_a\$file" -Algorithm SHA256).Hash
  $b = (Get-FileHash ".\build\scan_pipeline\p1_dataset_b\$file" -Algorithm SHA256).Hash
  if ($a -ne $b) { throw "P1 nondeterministic artifact: $file" }
  Write-Host "P1 deterministic: $file"
}
```

Na końcu ponownie uruchomić standardową bramkę projektu:

```powershell
.\tools\gate.ps1
```

## Kryteria promocji stagingu na branch roboczy

Branch `photogrammetry/import-v2-foundation` może zostać przesunięty na staging dopiero po potwierdzeniu:

- pełne test discovery PASS;
- `py_compile` PASS;
- 7/7 GLB rozpoznanych;
- 7/7 PLY rozpoznanych;
- dwa przebiegi dają 7/7 identycznych artefaktów;
- żadna para nie ma statusu `incompatible`;
- raport nie zawiera absolutnej ścieżki ani georeferencji;
- `tools/gate.ps1` pozostaje zielone.

Nawet po promocji P2 pozostaje zablokowane, dopóki Jozz nie zatwierdzi:

- skali znanym odcinkiem;
- osi i orientacji;
- par oznaczonych `review`;
- późniejszego teksturowanego renderu.

## Uczciwy stan

Wcześniejszy lokalny prototyp tej fazy przeszedł testy syntetyczne i dwa pełne przebiegi na 7 GLB + 7 PLY. Po ograniczeniu pracy wyłącznie do GitHub kod został odtworzony i ponownie poddany statycznemu audytowi na branchu stagingowym, ale ta dokładna zdalna wersja wymaga jeszcze uruchomienia powyższych komend na lokalnym Windows checkoutcie.

Nie promowano stagingu na `photogrammetry/import-v2-foundation`. Nie rozpoczęto P2.
