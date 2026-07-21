# Photogrammetry Import V2 — checkpoint P1 po audycie krytycznym

**Data:** 2026-07-21  
**Docelowy branch:** `photogrammetry/import-v2-foundation`  
**Branch stagingowy:** `agent/p1-dataset-inspector-staging`  
**Baza stagingu:** `36fcc91969c5d18d01abf3187d2c0ae7f49812bb`  
**Draft PR:** `#1 — P1: add streaming GLB/PLY dataset inspector`  
**Aktualna bramka:** implementacja i syntetyczne kontrakty P1 są zielone; rzeczywisty przebieg 7+7, ręczny kontrakt skali/osi i lokalny Windows gate nadal blokują promocję oraz P2.

## 1. Potwierdzony baseline P0

P0 zostało wykonane przez Jozza na Windows po `cmake --preset windows`.

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

Lokalny raport:

```text
build/scan_pipeline/p0_baseline/36fcc91969c5_20260721T002323Z/
```

## 2. Zakres P1 po ponownym audycie

Dodano wyłącznie offline tooling, testy, dokumentację oraz ograniczony workflow Python:

```text
tools/scan_pipeline/scan_ply.py
tools/scan_pipeline/scan_glb_quality.py
tools/scan_pipeline/scan_dataset_inspect.py
tools/scan_pipeline/run_p1_contracts.py

tests/scan_pipeline/test_scan_ply.py
tests/scan_pipeline/test_scan_glb_quality.py
tests/scan_pipeline/test_scan_dataset_inspect.py

.github/workflows/p1-scan-inspector.yml
docs/PHOTOGRAMMETRY_IMPORT_V2_P1_CHECKPOINT_2026_07_21_PL.md
```

Nie zmieniono:

- `src/` ani `include/` Box3D;
- zachowania pojazdu M6/M7/M8;
- renderera;
- runtime C++;
- istniejącego `scan_inspect.py`;
- surowych GLB, PLY ani RAR;
- `main`;
- docelowego brancha `photogrammetry/import-v2-foundation`.

## 3. Iteracje krytyczne i znalezione problemy

### Iteracja A — kontrakt danych i prywatność

Pierwsza wersja raportowała zbyt szerokie struktury legacy GLB i oryginalne nazwy logiczne. Zostało to zastąpione jawnie dozwolonym zestawem pól i kanonicznymi etykietami:

```text
MipTile_0.glb
MipTile_0.ply
...
```

Raport nie kopiuje:

- generatora GLB;
- komentarzy ani `obj_info` PLY;
- absolutnych ścieżek;
- nazw katalogów źródłowych;
- georeferencji;
- source RGB;
- tekstur ani miniaturek prywatnego miejsca.

### Iteracja B — integralność GLB

Legacy inspector był dobrym inwentaryzatorem, ale nie był wystarczająco twardym walidatorem geometrii. Dodano `scan_glb_quality.py`, który:

- waliduje wszystkie `bufferView` względem deklarowanego `buffer.byteLength`, nie tylko fizycznego paddingu BIN;
- wymusza accessor wewnątrz własnego `bufferView`;
- waliduje offset, stride, typ i count;
- wymusza `POSITION = FLOAT VEC3`, bez `normalized`;
- wymusza unsigned scalar indices;
- odrzuca indeks poza zakresem `POSITION`;
- waliduje zgodność count pozostałych vertex attributes;
- porównuje deklarowane `POSITION min/max` z danymi rzeczywistymi;
- oblicza world bounds z realnych pozycji, nie z deklaracji;
- raportuje sceny, orphan nodes, meshe poza sceną domyślną i non-identity transforms;
- analizuje wszystkie triangle primitive’y;
- liczy trójkąty degeneracyjne;
- tworzy histogram długich krawędzi.

Próg długiej krawędzi jest zapisany w **source units**, nie w metrach. P1 dopiero potwierdza skalę, więc wcześniejsze nazywanie takiego progu metrycznym byłoby logicznie błędne.

Raportuje progi:

```text
>1, >5, >10, >25, >50, >100 source units
```

`>10` jest wyłącznie provisional review threshold. Nie oznacza automatycznie błędu geometrii.

### Iteracja C — integralność PLY

`scan_ply.py` został zaostrzony o:

- unikalne elementy i properties;
- pełną składnię list property;
- dokładnie jeden element `vertex`;
- odrzucenie variable-length element przed `vertex`;
- odrzucenie niepustego elementu o zerowym stride przed `vertex`;
- poprawne pominięcie fixed-size elementu przed `vertex`;
- obowiązkowe, skończone XYZ;
- RGB8 tylko wtedy, gdy `red/green/blue` są `uchar/uint8`;
- kontrolę rozmiaru i `mtime_ns` przed i po inspekcji;
- SHA-256 pliku;
- ponowne potwierdzenie SHA-256 podczas budowania evidence grid.

PLY nie może więc zmienić się pomiędzy raportem bounds a rasteryzacją density/spread/support.

### Iteracja D — klasyfikacja par GLB ↔ PLY

Wykryto błąd, w którym lepsza permutacja extentów mogła obniżyć odległy kafel z `incompatible` do `review`.

Po poprawce:

- podejrzenie permutacji osi jest tylko dodatkowym dowodem;
- para musi najpierw być przestrzennie wiarygodna;
- duży centroid delta lub brak overlapu pozostaje bezwzględnie `incompatible`;
- ręczny kontrakt nie może nadpisać `incompatible`.

### Iteracja E — pamięć i backendy

Evidence grid ma dwa jawne profile:

```text
NumPy backend:  maksymalnie 2048 × 2048
stdlib backend: maksymalnie  512 × 512
```

Standard-library fallback pozostaje poprawny i dependency-free, ale dla realnych 24 mln punktów będzie znacznie wolniejszy. NumPy jest opcjonalnym fast pathem, a nie obowiązkową zależnością formatu P1.

### Iteracja F — bramka ręczna

Samo `compatible-review` nie może odblokować P2. Dodano lokalny, hashowany review contract:

```text
jozz.scan-p1-review / schemaVersion 1
```

Kontrakt zapisuje decyzję właściciela o:

- znanym realnym odcinku;
- source-units-per-meter;
- orientacji osi;
- zatwierdzeniu par `review`.

Do raportu trafia hash i bezpieczne metryki pochodne, nie punkty odcinka ani prywatny opis miejsca.

### Iteracja G — prawdziwe CI dokładnej wersji z GitHuba

Dodano workflow P1-only. Nie uruchamia ciężkich roadmap experiments wymagających NumPy/SciPy/Pillow/trimesh/scikit-image.

Jawny runner:

```text
tools/scan_pipeline/run_p1_contracts.py
```

obejmuje dokładnie:

```text
test_scan_inspect.py
test_scan_ply.py
test_scan_glb_quality.py
test_scan_dataset_inspect.py
```

Pełna macierz dla commita `621e682e7cba453de4417bf8fb548d8d1b1bc263` przeszła:

```text
Ubuntu / Python 3.11 / stdlib   PASS
Ubuntu / Python 3.11 / NumPy    PASS
Ubuntu / Python 3.13 / stdlib   PASS
Ubuntu / Python 3.13 / NumPy    PASS
Windows / Python 3.11 / stdlib  PASS
Windows / Python 3.11 / NumPy   PASS
Windows / Python 3.13 / stdlib  PASS
Windows / Python 3.13 / NumPy   PASS
```

Łącznie: **8/8 jobów PASS**.

CI nie zastępuje realnego skanu ani lokalnego D3D11/C++ gate.

## 4. Automatyczna bramka evidence

`automaticEvidenceGate.passed` wymaga:

- wszystkich primitive’ów przeanalizowanych jako wspierane triangle primitive’y;
- pełnej zgodności liczby przeanalizowanych trójkątów;
- braku nieobsługiwanych `extensionsRequired`;
- braku mesh nodes poza sceną domyślną;
- stabilności wszystkich PLY podczas inspekcji;
- ponownej zgodności hashów PLY podczas evidence rasterization.

Ręczny review contract nie może przegłosować błędu strukturalnego.

Orphan node bez mesha jest raportowany jako warning. Różnica deklarowanych bounds jest raportowana, ale actual accessor data pozostaje autorytetem.

## 5. Klasyfikacja par

Każda para otrzymuje:

```text
strong-match
review
incompatible
```

P2 może zostać odblokowane tylko, gdy:

```text
datasetStatus != incompatible
automaticEvidenceGate.passed == true
scaleConfirmed == true
axesConfirmed == true
reviewPairsApproved == true
```

Nie ma już błędnego wymagania „najpierw teksturowany render”. P2 jest właśnie etapem visual preview, więc użycie jego wyniku do odblokowania P2 byłoby kołem logicznym.

## 6. Artefakty lokalne P1

Każdy przebieg tworzy:

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
- `vertical_spread.png` — `zMax-zMin` w source units;
- `source_support.png` — liczba źródeł wspierających komórkę;
- `glb_ply_alignment.png` — centroid, extent i overlap par.

Wyniki prywatnego skanu pozostają pod `build/` i nie trafiają do Git.

## 7. Testy kontraktowe

Testy adversarialne obejmują między innymi:

- little i big endian PLY;
- fixed-size element przed vertex;
- zero-stride element;
- duplikaty elementów i properties;
- złą list property;
- zły typ RGB;
- brak XYZ, truncation, `NaN/Inf` i header limit;
- modyfikację PLY podczas inspekcji;
- accessor poza własnym `bufferView`;
- float indices i index poza zakresem;
- fałszywe deklarowane bounds;
- degeneracyjne i długokrawędziowe trójkąty;
- orphan/default-unreachable mesh nodes;
- zgodność NumPy i stdlib;
- deterministyczność wszystkich siedmiu artefaktów;
- brak wycieku metadanych i nazw folderów;
- duplicate/unpaired Tile ID;
- odległą parę z pozornie lepszą permutacją osi;
- brak możliwości przegłosowania `incompatible`;
- manual review contract;
- limit pamięci stdlib grid.

## 8. Lokalna reprodukcja stagingu

```powershell
git fetch origin
git switch agent/p1-dataset-inspector-staging
git pull --ff-only

python .\tools\scan_pipeline\run_p1_contracts.py

python -m py_compile `
  .\tools\scan_pipeline\scan_inspect.py `
  .\tools\scan_pipeline\scan_ply.py `
  .\tools\scan_pipeline\scan_glb_quality.py `
  .\tools\scan_pipeline\scan_dataset_inspect.py `
  .\tools\scan_pipeline\run_p1_contracts.py
```

## 9. Rzeczywisty przebieg 7+7

Po rozpakowaniu RAR do jednego katalogu zawierającego oba poddrzewa GLB i PLY:

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

Bez review contract polecenie ma wygenerować evidence, ale `p2Unblocked` musi pozostać `false`.

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

## 10. Lokalny review contract

Kontrakt powinien pozostać pod ignorowanym `build/`, ponieważ zawiera source-space punkty znanego odcinka.

Przykład struktury — wartości są wyłącznie przykładowe:

```json
{
  "schema": "jozz.scan-p1-review",
  "schemaVersion": 1,
  "ownerApproved": true,
  "knownDistance": {
    "pointA": [0.0, 0.0, 0.0],
    "pointB": [10.0, 0.0, 0.0],
    "meters": 10.0,
    "expectedSourceUnitsPerMeter": 1.0,
    "maxRelativeError": 0.02
  },
  "axes": {
    "horizontal": ["X", "Y"],
    "up": "Z",
    "confirmed": true
  },
  "approvedReviewTileIds": [1, 4, 6]
}
```

Po ręcznej ocenie i wpisaniu prawdziwych danych:

```powershell
python .\tools\scan_pipeline\scan_dataset_inspect.py `
  --input .\local_assets\scans\Model_skanu_extracted `
  --output .\build\scan_pipeline\p1_dataset_approved `
  --name model-skanu `
  --expected-glb 7 `
  --expected-ply 7 `
  --review-contract .\build\scan_pipeline\p1_review.json `
  --require-p2-ready
```

Exit code `0` oznacza, że wszystkie automatyczne i ręczne bramki P1 są spełnione. Exit code `4` oznacza, że raport powstał, ale P2 pozostaje zablokowane.

`ownerApproved` jest deterministycznym zapisem decyzji właściciela, nie systemem kryptograficznego uwierzytelniania.

## 11. Lokalny Windows gate

Po testach P1:

```powershell
.\tools\gate.ps1
```

Wynik musi pozostać:

```text
BRAMKA: build 3/3 OK - walidator OK - test PASS - smoke 0 err
```

## 12. Kryteria promocji stagingu

Branch `photogrammetry/import-v2-foundation` może zostać przesunięty na staging dopiero po potwierdzeniu:

- GitHub Actions P1: PASS;
- lokalny `run_p1_contracts.py`: PASS;
- `py_compile`: PASS;
- 7/7 GLB rozpoznanych;
- 7/7 PLY rozpoznanych;
- wszystkie realne hash checks: PASS;
- dwa przebiegi dają 7/7 identycznych artefaktów;
- żadna para nie ma statusu `incompatible`;
- `automaticEvidenceGate.passed == true`;
- rzeczywista skala potwierdzona znanym odcinkiem;
- osie X/Y/Z potwierdzone przez Jozza;
- wszystkie realne pary `review` jawnie zatwierdzone;
- `--require-p2-ready` kończy się kodem `0`;
- raport nie zawiera ścieżek, nazw prywatnych ani georeferencji;
- `tools/gate.ps1` pozostaje zielone.

## 13. Uczciwy stan końcowy tej iteracji

Dokładna wersja Pythonowego P1 zapisana na GitHubie przeszła pełną syntetyczną macierz Windows/Linux, Python 3.11/3.13, NumPy/stdlib.

Nie zostały jeszcze wykonane na tej wersji:

- rzeczywisty przebieg 7 GLB + 7 PLY;
- pomiar znanego odcinka;
- ręczny review contract Jozza;
- ponowny lokalny Windows `tools/gate.ps1` po pobraniu stagingu.

Staging nie został promowany na `photogrammetry/import-v2-foundation`. Nie rozpoczęto P2.
