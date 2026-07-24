# Plan fundamentu: teren ze skanu jako pierwszorzędna warstwa świata

**Data:** 2026-07-24
**Autor:** Luna (agent), na zlecenie Jozza
**Status:** `DECYZJE D1–D6 PODJĘTE PRZEZ JOZZA 2026-07-24 — plan gotowy do F0`
**Zastępuje:** szkic „import skanu jako kafel" z tego samego dnia (był planem portu PoC).

**Teza:** skanowany teren nie może być doklejonym kaflem obok proceduralnej mapy. Ma być
**tym samym rodzajem obiektu co teren proceduralny** — heightfield z materiałami per
komórka, adresowany chunkami, z osobną warstwą przeszkód i osobną warstwą wizualną.
Wtedy skan dziedziczy wszystko, co projekt już umie (split wheel envelope, kategorie
kolizji, teleport, seed/regen, budżet ms/step), a nie buduje obok drugiego, równoległego
świata.

Snapshot stanu przed startem: gałąź `jozz-map-wip-snapshot-2026-07-24` (commit `8dbe3b3`).

---

## 0. Streszczenie w pięciu zdaniach

1. PoC na gałęzi `agent/project-refoundation-audit-v1` **działa** i jest cenny jako dowód,
   że źródło jest dobre, host to udźwignie, a auto po tym jeździ.
2. Architektonicznie PoC jest jednak **jedną wielką zupą trójkątów**, i to niesie siedem
   konkretnych defektów (§1) — z których dwa są realnymi błędami fizyki w tym projekcie.
3. Właściwa architektura **została już opisana w tym repo 2026-07-15** (raport eksperymentów),
   zwalidowana liczbowo, po czym porzucona na rzecz szybkiego PoC (§2).
4. box3d ma **dziewięć zdolności**, których PoC nie użył, a które są dokładnie pod ten
   problem zaprojektowane — z serializowalnym blobem BVH i heightfieldami o dokładnych
   szwach na czele (§3).
5. Proponuję program F0–F9 z pierwszą grywalną dostawą (kafel skanu + teleport) w F5,
   zbudowaną już na docelowym fundamencie, a nie na PoC do późniejszego wyrzucenia.

---

## 1. Werdykt o PoC: co udowodnił, a gdzie kłamie architektonicznie

### 1.1 Co PoC realnie udowodnił (i to zostaje)

```text
7 kafli, 1 409 687 wierzchołków, 1 775 775 trójkątów
auto stoi stabilnie: identyczna pozycja w kroku 300 i 600
4/4 koła w kontakcie podczas jazdy, 2/4 w spoczynku na nierówności
zderzenie ze zeskanowanym słupem = realna kolizja, nie płaszczyzna
restart tej samej rewizji: 949 klatek, 0 błędów sokol
winding domyślnie poprawny, 5 824 zdegenerowanych trójkątów tolerowanych
```

To jest solidny dowód **wykonalności**. Pipeline Python (28 narzędzi, 16 plików testów),
kontrakt ramy współrzędnych, poziomowanie per-skan, gate prywatności — to wszystko jest
dobrze zrobione i **przechodzi do fundamentu bez zmian**.

Do tego fakt, który koryguje Twoje założenie: gałąź skanu **nie jest starszą wersją
projektu**. `jozz-vehicle-sandbox-m0` (445db88) jest jej przodkiem, a `git diff` po
`samples/` to **14 plików, +3555, −0** — same dodatki, zero modyfikacji. Wrażenie „uboższego
UI i rigu" bierze się stąd, że `P2B Scan Drive (M6)` to **1025-liniowy klon laboratorium**
z własną, okrojoną kopią panelu — a nie samo laboratorium. Rig fizyczny pod spodem jest
identyczny.

### 1.2 Siedem defektów architektonicznych

| # | Defekt | Dowód | Konsekwencja |
| --- | --- | --- | --- |
| **D1** | **Cały skan otagowany jako powierzchnia jezdna.** Dachy, ściany, słupy, drzewa dostają `JOZZ_M6_TERRAIN_CATEGORY` | `jozz_vehicle_scan_drive_lab.cpp`: `shapeDef.filter.categoryBits = JOZZ_M6_TERRAIN_CATEGORY` dla wszystkich 7 meshy | **Realny błąd fizyki w TYM projekcie.** README §4: kategoria terenu (0x2) obsługuje *toczącą się kulę* split wheel envelope, kategoria obiektów (0x1) — *pełny walec*. PoC każe kołu „toczyć się" po ścianie budynku |
| **D2** | `identifyEdges = false` | tamże, `def.identifyEdges = false`; wszystkie buildery box3d (`src/mesh.c:1382`, `:1442`) dają `true` | Brak flag krawędzi wklęsłych ⇒ koła zaczepiają się o wewnętrzne krawędzie sąsiednich trójkątów. Duchy i szarpnięcia przy prędkości |
| **D3** | Jedno tarcie 0.9 na cały świat | `shapeDef.baseMaterial.friction = 0.9f` | Asfalt = trawa = błoto = dach. Zero `userMaterialId`. Sprzeczne z kierunkiem BeamNG: zachowanie ma **wynikać** z powierzchni |
| **D4** | Wypiek BVH 1.78 mln trójkątów przy **każdym** starcie | tabela z `DRIVE_THE_SCAN.md`: Debug 40.4 s, Release 0.96 s | 40 s zamrożonego okna w Debug. A `b3MeshData` jest **blobem serializowalnym** (§3.1) — ten koszt jest do wyzerowania |
| **D5** | „Dokładnie jedna paczka" | `FindJozzActiveScanPreviewPack()`: `candidates.size() != 1 ⇒ {}` | Architektonicznie wyklucza Twój cel „łączyć różne skany" |
| **D6** | Brak warstwy gruntu (zero heightfieldów) | w całym pipeline słowo „heightfield" pada tylko w narzędziach *probe*, nigdy w runtime | Traci: dziury (`0xFF`), materiały per komórka, dokładne szwy, tanie samplowanie wysokości, tanią deformację, streaming |
| **D7** | Wizual i kolizja to ten sam mesh | `m_visualPack.Load(packDir)` na tych samych bajtach co kolizja | Nie da się mieć ładnego renderu i taniej fizyki naraz; LOD wizualny zmieniałby kolizję |

D1 i D2 to nie estetyka — to powody, dla których jazda po skanie **nigdy nie będzie
dobra**, choćby geometria była idealna.

---

## 2. Odkrycie: właściwa architektura już tu była i została porzucona

`docs/PHOTOGRAMMETRY_ROADMAP_EXPERIMENT_REPORT_2026_07_15_PL.md` (645 linii, 2026-07-15)
zawiera zmierzoną, poprawną roadmapę. Jej werdykt końcowy:

> ```text
> visual mesh != drivable ground != obstacle collision
> ```

Co ten raport **już zmierzył** (i czego nie trzeba powtarzać):

| Ustalenie | Liczba |
| --- | --- |
| Rozdzielczość gruntu MVP | **0,50 m** — lepsza coverage, ~4× mniejsze, szybsza rasteryzacja, w drive probe równie stabilne co 0,25 m |
| Szwy heightfieldów po podziale 2×2 | `allSharedEdgesExact = true` dla wszystkich wariantów — jedno globalne min/max + skopiowane rzędy graniczne ⇒ **szew matematycznie dokładny** |
| Rozkład trójkątów w chunkach 128 m | mediana 488, P95 123 477, max 371 501 — skrajnie nierówny |
| Adaptive quadtree (cel 25k) | 111 liści, mediana 9 340, max 46 329 — dużo lepsza kontrola maksimum |
| Model docelowy | uniform world chunks (adresowanie + heightfield + streaming) **+** adaptive render sections wewnątrz chunka |
| Tekstury | 1K = 4,74 MB encoded / ~96 MiB RGBA+mip; 2K = 352 MB; SSIM 1K↔2K = 0,9988 ⇒ **1K wystarcza** |
| Etapowy cooker | inspect → geometry_extract → seam_measure → pilot_dem → manual_review → heightfield_cook → texture_cook → render_lod_cook → package |
| Powtarzalność | drugi niezależny run: **95/95 artefaktów byte-identical** |

Raport wprost planował „**Następny etap C — pierwszy Box3D heightfield**":
Golden Drive Region, 0,50 m, ręcznie zatwierdzony grunt, jeden lub cztery heightfieldy,
różnica visual/collision na debugu, spokojny spawn, powolny przejazd.

**Etap C nigdy nie powstał.** Zamiast niego poszła zupa trójkątów, bo dawała obrazek
szybciej. To była rozsądna decyzja *dla dowodu widoczności* i zła decyzja *dla fundamentu*.

Ten plan = dokończenie etapu C, wzmocnione o to, czego raport nie wiedział (§3), i wpięte
w istniejącą mapę zamiast w osobny sample.

---

## 3. Dziewięć zdolności box3d, których PoC nie użył

Wszystko zweryfikowane w nagłówkach tego repo — bez zmian w `src/`/`include/`.

### 3.1 `b3MeshData` i `b3HeightFieldData` to **samowystarczalne, przenoszalne blob-y z hashem treści**

```c
typedef struct b3MeshData {
    uint64_t version;   // B3_MESH_VERSION 0xABD11AB62A6E886D — "Useful for validating serialized data"
    int      byteCount; // CAŁY rozmiar
    uint32_t hash;      // hash treści (pole zerowane przy liczeniu)
    ...
    int nodeOffset; int nodeCount; int treeHeight;   // <-- BVH JEST W ŚRODKU
    int vertexOffset, triangleOffset, materialOffset, flagsOffset; // offsety OD ADRESU STRUKTURY
} b3MeshData;
```

Wszystkie offsety są względem adresu struktury, nie ma nienazwanego paddingu (komentarz
w `height_field.c:471`: „Content hash over the whole blob with the hash field zeroed, like
b3HullData/b3MeshData"), a `b3DestroyMesh` to po prostu `b3Free(mesh, mesh->byteCount)`.
Alokator używa **64-bajtowego wyrównania** (`src/core.c:141`).

**Wniosek: BVH da się upiec RAZ, offline, i wczytać jako bajty.** Ładowanie = `_aligned_malloc(byteCount, 64)`
+ `memcpy` + walidacja `version`/`byteCount`/`hash`. **Defekt D4 (40 s w Debug) znika do zera**,
i to bez dotykania silnika. To jest do potwierdzenia spikiem z falsyfikatorem (§10, F2), nie
do przyjęcia na wiarę.

### 3.2 Kontrakt dokładnych szwów heightfieldów — wprost w API

```c
/// Global minimum and maximum heights used for quantization. This is important
/// if you want height fields to be placed next to each other and line up exactly.
/// In that case, both height fields should use the same minimum and maximum heights.
float globalMinimumHeight, globalMaximumHeight;
```

To jest dokładnie to, co raport 2026-07-15 zmierzył jako `allSharedEdgesExact = true`.
Silnik został zaprojektowany pod kafelkowany teren — PoC tego nie tknął.

### 3.3 Dziury w gruncie (`0xFF`)

```c
/// Grid cell material. A value of 0xFF is reserved for holes
uint8_t* materialIndices;   // count = (countX-1) * (countZ-1)
```

Rekonstrukcja fotogrametryczna ma braki danych, tunele, prześwity pod obiektami.
Zamiast rozciągniętego trójkąta-śmiecia: **jawna dziura**. To narzędzie jakości, którego
zupa trójkątów nie ma.

### 3.4 Materiały per komórka / per trójkąt + `userMaterialId`

```c
typedef struct b3SurfaceMaterial {
    float friction, restitution, rollingResistance;
    b3Vec3 tangentVelocity;      // taśmociąg
    uint64_t userMaterialId;     // "the shape or triangle surface type"
    uint32_t customColor;
} b3SurfaceMaterial;
```

`userMaterialId` **propaguje się** do: wyników raycastu (`b3CastResultFcn`), danych
kontaktu (`userMaterialIdA/B`) oraz **callbacków mieszania tarcia i restytucji**
(`b3FrictionCallback`, `b3RestitutionCallback` w `b3WorldDef`).

Czyli: asfalt, żwir, trawa, błoto i beton mogą mieć własne tarcie i własną tożsamość
widoczną dla pojazdu — i to jest **fizyka wynikowa**, nie skrypt. Dokładnie kierunek
BeamNG z README §1. Mesh: `uint8` na trójkąt → do 256 materiałów. Heightfield: `uint8`
na komórkę.

Bonus: `b3Shape_SetMeshMaterial(shapeId, material, index)` pozwala **stroić te materiały
na żywo suwakiem**, bez przebudowy geometrii — czyli tak, jak stroi się resztę tego projektu.

### 3.5 Pamięć heightfielda: `uint16` skompresowanych wysokości

| Zasięg | Punkty @0,5 m | Wysokości | Materiały | Flagi | **Razem** |
| --- | ---: | ---: | ---: | ---: | ---: |
| chunk 64 m | 129×129 | 33 KB | 16 KB | 32 KB | **~81 KB** |
| chunk 128 m | 257×257 | 132 KB | 64 KB | 128 KB | **~324 KB** |
| region 300×300 m | 601×601 | 722 KB | 360 KB | 720 KB | **~1,8 MB** |
| **cały skan 1204×1099 m** | 2409×2199 | 10,6 MB | 5,3 MB | 10,6 MB | **~26,5 MB** |

Dla porównania paczka PoC: **107 MB** (64 MB geometrii + 43 MB tekstur) — i to bez węzłów
BVH, które dochodzą przy wypieku. Cały jeżdżalny grunt całego skanu w heightfieldach kosztuje
mniej niż połowę samej geometrii jednej zupy trójkątów.

Do tego: heightfield **nie ma BVH** — jest regularną siatką, więc koszt zapytania zależy od
obszaru zapytania, nie od globalnej liczby trójkątów, i nie ma czego piec.

### 3.6 Brak aktualizacji „w miejscu" — i co z tego wynika

Zweryfikowane: istnieje `b3Shape_SetHull`, `b3Shape_SetSphere`, `b3Shape_SetCapsule`, ale
**nie ma** `b3Shape_SetHeightField` ani `SetMesh`. Zmiana terenu = zniszcz + zbuduj chunk.

To nie jest blocker — to **wymóg projektowy na rozmiar chunka**. Chunk 64 m @0,5 m to 81 KB
i tworzenie w ułamku milisekundy. Czyli: **rozmiar chunka jest decyzją o przyszłej
odkształcalności gruntu**, nie tylko o streamingu. To jest bezpośredni most do wizji JES
(„teren z bilansem materii", eksperyment X1 „Żywy grunt") — i powód, żeby nie wybierać
chunków 256 m tylko dlatego, że jest ich mniej.

### 3.7 Duży świat: podwójna precyzja w stylu Jolta

`BOX3D_DOUBLE_PRECISION` (patrz `docs/large_worlds.md`): granica precyzji tylko na pozycji
świata (`b3Pos`, `b3WorldTransform`), reszta zostaje `float`. Koszt „kilka procent, nie 2×".
Zakres pracy ±1e7…1e8 m. Domyślnie **wyłączone**; włączenie to świadoma migracja źródeł
(kompilator wskaże każde miejsce).

To jest realna odpowiedź na „ambicje na coś dużo większego". **Nie włączamy tego teraz** —
ale piszemy kod tak, żeby migracja była mechaniczna: §5 (zasada „żaden kod nie zakłada, że
początek świata coś znaczy").

### 3.8 Split wheel envelope — projekt już ma właściwy podział, PoC go zignorował

`JOZZ_M6_TERRAIN_CATEGORY` (0x2) = powierzchnie jezdne, kontakt toczącej się kuły.
`JOZZ_M6_OBJECT_CATEGORY` (0x1) = obiekty, kontakt pełnego walca koła.
Trójwarstwowy model (§4) trafia w ten podział **z definicji**: grunt → 0x2, przeszkody → 0x1.
Zupa trójkątów nie potrafi ich rozróżnić, bo nie wie, co jest czym.

### 3.9 Serializacja heightfielda w silniku istnieje, ale jest debugowa

`b3DumpHeightData`/`b3LoadHeightField` zapisują **tekstem** (`fprintf("%.9f")` na każdą
wysokość). Dla 361 tys. punktów to kilkumegabajtowy plik tekstowy i wolny parse.
Wniosek: używamy własnego formatu binarnego chunka (i tak potrzebnego dla materiałów,
metadanych i wersji algorytmu), a te dwie funkcje traktujemy jako narzędzie diagnostyczne.

---

## 4. Fundament: trzy warstwy terenu

```text
                     ŹRÓDŁO (prywatne, lokalne)
        GLB (mesh teksturowany)          PLY (chmura punktów)
                 │                                │
                 │            COOKER OFFLINE      │
                 ▼                                ▼
        ┌────────────────┐            ┌──────────────────────┐
        │  L3 WIZUAL     │            │  L1 GRUNT JEZDNY     │
        │  mesh + 1K tex │            │  b3HeightField       │
        │  sekcje + LOD  │            │  0,5 m, uint16       │
        │  BEZ fizyki    │            │  materiał/komórkę    │
        └────────────────┘            │  dziury 0xFF         │
                 │                    │  kategoria 0x2       │
                 │                    └──────────────────────┘
                 │                                │
                 │      ┌──────────────────────┐  │
                 └──────│  L2 PRZESZKODY       │──┘
                        │  b3Mesh / compound   │
                        │  ściany, słupy,      │
                        │  budynki, krawężniki │
                        │  kategoria 0x1       │
                        └──────────────────────┘
```

### Kontrakt rozdziału (to jest sedno, nie diagram)

1. **Każdy trójkąt źródła należy do dokładnie jednej warstwy fizycznej.** L1 i L2 nie mogą
   opisywać tej samej powierzchni — inaczej auto zderza się dwa razy albo klinuje w szczelinie
   między reprezentacjami. To jest **testowalne** i musi mieć test w cookerze.
2. **Separator to maska obiektów z filtra gruntu**, który już istnieje
   (`scan_ground_filters.py`: progresywne otwarcie morfologiczne, `object_height`,
   profile gentle/balanced/aggressive). Był napisany i nigdy nie podłączony.
3. **L3 nie ma prawa wpływać na fizykę.** Utrzymujemy istniejący test architektury, który
   wyklucza API fizyki z czytnika wizualnego.
4. **Granica rozdzielczości jest jawna.** Przy 0,5 m grunt nie odwzoruje krawężnika (15 cm
   na 50 cm komórki to 17° zbocza zamiast pionowej ścianki). Detale poniżej ~1 m albo idą do
   L2, albo są świadomie tracone i **zapisane jako utracone**. Dla wybranych stref
   dopuszczamy 0,25 m (raport: „tylko ręcznie wybrane high-detail zones").

### Dlaczego to jest „dusza projektu", a nie moja fantazja

- Grunt skanu staje się **tym samym typem obiektu** co offroad z Etapu 1 mapy
  (`b3CreateHeightFieldShape`, ten sam tag kategorii, ten sam sposób samplowania wysokości).
  Teleport, spawn po footprincie 4 kół, „Zresetuj świat", budżet ms/step — wszystko działa
  bez wyjątków „a jeśli to skan, to…".
- Zachowanie auta na drodze vs na trawie wynika z **materiału powierzchni**, nie ze skryptu
  (README §1: „behaviour must emerge from the construction").
- Przeszkody trafiają w istniejący split wheel envelope zamiast go oszukiwać.
- Odkształcalny grunt (JES) staje się kwestią rozmiaru chunka, a nie przepisywania świata.

---

## 5. Kontrakt przestrzenny świata

### 5.1 Chunk jako jednostka adresowania

```text
chunkSize        128 m   (adresowanie, własność heightfielda, streaming)
groundCell       0,5 m   (257 x 257 punktów na chunk)
detailCell       0,25 m  (opcjonalnie, strefy wskazane ręcznie)
renderSection    adaptacyjnie wewnątrz chunka (cel ~25k trójkątów/sekcję)
```

Podstawa liczbowa: raport 2026-07-15 (rozkład trójkątów 128 m: mediana 488 / P95 123k /
max 371k ⇒ uniform grid dobry do adresowania, zły jako budżet renderu; quadtree z celem 25k
daje max 46k).

### 5.2 Kwantyzacja i szwy

Reguła: **wszystkie chunki jednej spójnej powierzchni gruntu dzielą jeden zakres
`globalMinimumHeight`/`globalMaximumHeight` i kopiują graniczne rzędy/kolumny.**
Wtedy szew jest dokładny co do bitu (zmierzone: `allSharedEdgesExact = true`).

Zakres dobierany per region, zapisany w manifeście. Przy `uint16`:
zakres 800 m ⇒ krok 12,2 mm; zakres 200 m ⇒ krok 3,1 mm. Dla regionu wielkości wsi
zakres jest ciasny i kwantyzacja schodzi poniżej własnego szumu skanu.

*Migracja na później:* jeżeli kiedyś skan ma **fizycznie stykać się** z terenem proceduralnym,
oba muszą przyjąć wspólny zakres świata. Dlatego zakres jest polem manifestu, a nie stałą.

### 5.3 Zasada „początek świata nic nie znaczy"

Każda pozycja w kodzie terenu wyraża się jako `origin chunka + offset lokalny`.
Żadna funkcja nie zakłada, że `(0,0,0)` to cokolwiek szczególnego. Dzięki temu włączenie
`BOX3D_DOUBLE_PRECISION` (§3.7) będzie migracją typów, a nie przeprojektowaniem świata.
To kosztuje dziś zero i kupuje opcję na świat kilometrowy.

### 5.4 Konkretne osadzenie (D2 = cały skan, D4 = północ, D5 = wyspa)

```text
skan:            1204 x 1099 m  (X x Z), relief 161 m
luka od plyty:   50 m  (urwisko, brak polaczenia fizycznego - D5)
srodek kafla:    x = 0,  z = 200 + 50 + 1099/2  =  799,5
zajmowany obszar: x in [-602, +602],  z in [+250, +1349]
najdalszy punkt: ~1,47 km od poczatku swiata
```

Kontrola kolizji planistycznych: płyta zajmuje `z ∈ [−200, 200]`, offroad `x ∈ [198, 598]`
przy `z ∈ [−200, 200]`, a masterplanowe place N/NW/NE kończą się na `z = 160`.
**Kafel skanu na `z ≥ 250` nie nachodzi na nic.**

Precyzja: przy 1,5 km od początku świata `float` ma rozdzielczość rzędu 0,1 mm.
`BOX3D_DOUBLE_PRECISION` **nie jest potrzebne** — potwierdza to wybór osadzenia
i odkłada tę migrację na czas, gdy świat naprawdę pójdzie w dziesiątki kilometrów.

**Datum wysokości:** zero mapy przypinamy do mediany gruntu w rdzeniu wsi (tam, gdzie
będzie spawn), więc jeżdżalny rdzeń leży przy `y ≈ 0`, a wzniesienia rosną w górę.
Zakres kwantyzacji regionu ≈ `[−40, +180]` m ⇒ krok `uint16` **3,4 mm** — poniżej
własnego szumu skanu.

---

## 6. Cooker offline — etapowy, hashowany, wznawialny

Model przejęty z raportu 2026-07-15 (przetestowany: 95/95 artefaktów byte-identical).

```text
1 inspect            integralność, bounds, pary GLB/PLY, prywatność
2 frame_contract     source -> lab: jednostki, osie, handedness, mirror, poziomowanie
3 geometry_extract   GLB -> geometria; PLY -> chmura punktów
4 pilot_dem          rasteryzacja gruntu z PLY (0,5 m) + mapy diagnostyczne
5 manual_review      >>> BRAMKA JOZZA <<< region, groundSeeds, excludedPolygons
6 ground_cook        heightfield chunks: uint16 + materiały + dziury + szwy
7 obstacle_cook      L2: mesh/compound z maski obiektów, BVH upieczony i zserializowany
8 texture_cook       1K baseColor, kolor-space, budżet pamięci
9 render_lod_cook    sekcje adaptacyjne + LOD
10 package           manifest, hashe, wersje algorytmów
```

Każdy etap: własny hash wejść, własny katalog wyjścia, możliwość wznowienia, jawna wersja
algorytmu, brak utraty wcześniejszych wyników przy błędzie późnej fazy.

**Dlaczego PLY, a nie GLB, jest źródłem gruntu:** chmura punktów jest surową prawdą
geometryczną i rasteryzuje się do siatki wysokości w sposób naturalny (percentyl/mediana na
komórkę), a filtry gruntu (morfologia progresywna) to standardowa operacja na chmurach.
GLB to mesh renderowy — dobry na L3 i na klasyfikację materiału przez teksturę, zły jako
źródło DEM. PoC używał GLB do wszystkiego, a 343 MB PLY leżało odłogiem jako „dowód par".

**Powtarzalność, o którą prosisz**, jest własnością tego modelu, nie osobną funkcją:
```powershell
python tools\scan_pipeline\scan_cook.py --dataset <katalog> --region <nazwa>
```
ten sam wejściowy dataset + ten sam plik decyzji = ten sam hash paczki. Zmiana jednego
etapu przelicza tylko ten etap i te po nim.

---

## 7. Runtime

### 7.1 Ładowanie

- L1: chunk = bajty `b3HeightFieldData` z pliku → `_aligned_malloc(byteCount, 64)` → walidacja
  `version`/`byteCount`/`hash` → `b3CreateHeightFieldShape`. Zero wypieku.
- L2: to samo z `b3MeshData` (BVH w środku, §3.1). Fallback: `b3CreateMesh` w locie, jeśli
  wersja bloba nie pasuje do zlinkowanego box3d.
- L3: mesh + tekstury do rejestru geometrii hosta, jak dziś.

**Bramka wersji:** blob niesie `B3_MESH_VERSION` / `B3_HEIGHT_FIELD_VERSION`. Niezgodność
⇒ komunikat + przepiek, nigdy cicha awaria.

### 7.2 Streaming i budżety

Chunki montowane po odległości od auta, z histerezą. Budżety do zadeklarowania **przed**
implementacją i pilnowane w bramce:

```text
krok fizyki:        <= 1,50 ms/step  (dziś mapa: 1,15-1,21 ms/step)
grunt w pamięci:    <= 32 MB
tekstury:           <= 96 MiB RGBA+mip (profil 1K, zmierzony)
czas montażu chunka: <= 5 ms  (żeby streaming nie robił zacięć)
```

### 7.3 Wysokość terenu

`SampleJozzWorldGroundHeight` dostaje gałąź „chunk skanu" = odczyt z siatki + interpolacja
(O(1)), zamiast raycastu z PoC. Zachowanie na płycie i offroadzie bit w bit bez zmian —
to jest wymóg bramki, nie życzenie.

---

## 8. Materiały powierzchni: fizyka z charakterem

Taksonomia startowa (`userMaterialId`, rozszerzalna):

| id | powierzchnia | tarcie (start) | źródło klasyfikacji |
| --- | --- | ---: | --- |
| 1 | asfalt / beton | 1,00 | jasność + niskie nachylenie + tekstura z GLB |
| 2 | żwir / pobocze | 0,85 | tekstura + szorstkość DEM |
| 3 | trawa / ziemia | 0,70 | tekstura (zieleń) + szorstkość |
| 4 | błoto / piach | 0,55 | ręczne wskazanie w `manual_review` |
| 255 | dziura (brak danych) | — | maska braków |

Zasady:
- klasyfikacja jest **propozycją cookera**, ręczna korekta jest prawdą (ta sama zasada
  evidence → proposal → authored truth, którą ma ich `ARCHITECTURE.md` — i ona zostaje);
- wartości tarcia są **strojone suwakiem na żywo** (`b3Shape_SetMeshMaterial`) i zapisywane
  jak każdy inny tuning;
- `customColor` na materiale daje debug-widok „co silnik uważa za jaką powierzchnię" —
  i to jest właściwy dowód dla „render is the gate", a nie ładny screenshot.

To jest miejsce, w którym skanowany świat przestaje być tapetą i zaczyna być fizyką.

---

## 9. Ambicje: co ten fundament otwiera

| Ambicja | Co dziś robimy, żeby była możliwa | Co odkładamy |
| --- | --- | --- |
| **Świat kilometrowy** | zasada §5.3 (pozycje względem chunka), chunki jako własność streamingu | włączenie `BOX3D_DOUBLE_PRECISION` — świadoma migracja, osobna decyzja |
| **Żywy grunt (JES X1)** | chunk 64–128 m ⇒ przebudowa chunka rzędu 81–324 KB; brak update-in-place jest wtedy nieistotny | sam model bilansu materii |
| **Wiele skanów, wspólny świat** | rejestr paczek + manifest osadzenia zamiast „dokładnie jedna paczka"; wspólna kwantyzacja przy styku | globalne dopasowanie ICP między skanami |
| **Most do JES** | kontrakt chunka, etapy cookera, taksonomia powierzchni, rozdział L1/L2/L3 to **wiedza przenośna** (zgodnie z ich `JES_04` clean-room) | kod hosta i Box3D-specific cooker nie przechodzą |
| **Mieszanie skanu z terenem proceduralnym** | wspólny format chunka dla obu ⇒ proceduralny offroad może kiedyś zostać przeliczony na ten sam kontrakt | fizyczne zszycie skan↔proceduralne |

---

## 10. Program F0–F9

Zasady: jeden etap na raz; bramka `.\tools\gate.ps1` zielona; wpis ≤5 linii w
`CHECKPOINTS_PL.md`; dla etapów wizualnych **przeczytany screenshot**; bez
`ACCEPTED BY JOZZ` + hash nie ruszam dalej. Gałąź: `jozz-scan-terrain-f0` odbita od
**zacommitowanego `445db88`** (czysty E1), nie od brudnego drzewa E2R/E3.

| # | Etap | Dostarcza | Dowód / falsyfikator |
| --- | --- | --- | --- |
| **F0** | Kontrakt i decyzje | ten dokument + schematy JSON (chunk, manifest, slot) + testy kontraktowe. Zero geometrii | testy schematów przechodzą; bramka bez zmian; perf mapy bez zmian |
| **F1** | Port pipeline + inspekcja | `tools/scan_pipeline/**`, `tests/scan_pipeline/**`, czytniki, sample P2A jako narzędzie oglądania. **Mapa nietknięta** | `run_p1_contracts.py` zielone; P2A renderuje istniejącą paczkę; screenshot przeczytany |
| **F2** | **Spike serializacji blobów** | dowód, że `b3MeshData`/`b3HeightFieldData` da się zapisać i wczytać | upiecz → zapisz → wczytaj → `hash`, `bounds`, `treeHeight` identyczne; ten sam wynik drive-probe. **Falsyfikator:** jeśli nie — wracamy do wypieku w locie z limitem trójkątów i mówimy to wprost |
| **F3** | DEM i diagnostyka | etapy cookera 1–4: DEM 0,5 m z PLY, mapy continuity / object-height / slope / seam | mapy do obejrzenia; metryki pokrycia; determinizm (dwa runy byte-identical) |
| **F4** | **Bramka Jozza: maski i wykluczenia** | Ty oglądasz mapy diagnostyczne i zatwierdzasz progi dziur/wykluczeń oraz kotwice startowe; zapis `groundSeeds` + `excludedPolygons` | Twoja akceptacja — maszyna tu nie decyduje |
| **F5** | **Grunt w mapie + teleport** ← *Twoja pierwotna prośba, na docelowym fundamencie* | `ground_cook` → 90 chunków L1 → kafel skanu w świecie M6 (§5.4), **rejestr kotwic w obrębie skanu**, montaż leniwy + **culling per chunk**, tymczasowy wizual = paczka PoC (§10.1), stan w debug-session | szwy dokładne; `wheel_contacts ≥ 1`; `car_y` identyczne w kroku 300 i 600; **ms/step i FPS przed i po**; zero regresji płyty i offroadu; Twoja jazda |
| **F6** | Materiały powierzchni | klasyfikacja per komórka + `userMaterialId` + suwaki tarcia + debug-widok kolorów | przejazd droga↔trawa: mierzalna różnica poślizgu; widok kolorów zgodny z teksturą |
| **F7** | Warstwa wizualna | L3: sekcje adaptacyjne, 1K, culling per chunk | budżet tekstur dotrzymany; FPS w regionie; zrzuty z ustalonych kamer |
| **F8** | Przeszkody | L2: ściany/słupy/budynki jako `0x1`, `identifyEdges = true` | test rozłączności warstw (żaden trójkąt w obu); auto nie „wjeżdża" na ścianę toczącą się kulą |
| **F9** | Wiele skanów + streaming | rejestr paczek, N regionów, montaż po odległości | dwa regiony naraz; montaż chunka ≤5 ms; brak zacięć |

Pierwsza rzecz, którą **zobaczysz**, jest w F1 (dni, nie tygodnie). Pierwsza rzecz, po której
**pojeździsz na docelowym fundamencie**, jest w F5.

### 10.1 Dlaczego F5 pożycza wizual z PoC — i dlaczego to nie jest hack

F5 dostarcza L1 (grunt jezdny). Bez warstwy wizualnej byłby to szary heightfield z siatką
debugową — czyli dokładnie to, jak dziś wygląda offroad z Etapu 1. Technicznie poprawne,
ale nie da się wtedy rozpoznać wsi ani ocenić, czy jazda odbywa się tam, gdzie powinna.

Rozwiązanie wynika z samej architektury: **warstwy są rozdzielone, więc L3 wolno wziąć
skądinąd.** W F5 fizyka pochodzi z chunków L1, a obraz — z gotowej, przetestowanej,
teksturowanej paczki PoC (`JSPREV2`), narysowanej z tym samym transformem osadzenia i
cullingiem per chunk.

Zysk jest podwójny: PoC zostaje użyty do tego, w czym jest naprawdę dobry (render),
a **rozbieżność między obrazem a fizyką staje się widoczna gołym okiem** — jeżeli auto
jedzie po niewidzialnej półce albo wpada w miejsce, gdzie widać drogę, to od razu widać,
że maska gruntu jest zła. To jest lepszy test poprawności L1 niż jakikolwiek zrzut liczb.
W F7 tymczasowy wizual zastępuje docelowy z sekcjami i LOD.

---

## 11. Decyzje dla Ciebie

| # | Decyzja | **Wybór Jozza (2026-07-24)** | Skutek dla planu |
| --- | --- | --- | --- |
| **D1** | Fundament vs. szybka ścieżka | **program F0–F9** | plan jak niżej |
| **D2** | Zasięg pierwszego regionu | **CAŁY SKAN 1204×1099 m** *(odrzucona rekomendacja 300×300)* | §11.1 — istotne skutki, plan zaktualizowany |
| **D3** | Rozmiar chunka | **128 m, ziarno 64 m w strefach** | 10×9 = 90 chunków, ~66 niepustych |
| **D4** | Gdzie na mapie | **PÓŁNOC, z > 200** | §5.4 — konkretne osadzenie |
| **D5** | Styk z płytą | **wyspa + teleport, urwisko na krawędzi** | apron dopiero po ocenie Jozza |
| **D6** | Los sampla `P2B Scan Drive` | **nie portujemy** | jego funkcja przechodzi do mapy w F5 |

Do potwierdzenia bez blokowania: gałąź `jozz-scan-terrain-f0` od `445db88`.

### 11.1 Skutki wyboru „cały skan" — uczciwie

Rekomendowałem region 300×300 m; Jozz wybrał cały skan. Decyzja jest jego i plan ją realizuje
w pełni. Trzy rzeczy trzeba jednak nazwać, bo zmieniają wykonanie:

**(a) Peryferyjne artefakty WCHODZĄ do świata.** Ich własna dokumentacja opisuje wiszącą
geometrię, ściany rekonstrukcji i rozciągnięte fragmenty krawędzi jako świadomie zaakceptowane
dla dowodu widoczności. W zupie trójkątów byłyby to twarde kolizyjne śmieci. **W architekturze
heightfielda są obsługiwalne** — i to jest argument, który wybór Jozza wzmacnia, a nie osłabia:

```text
komórka z za małą liczbą punktów PLY           -> 0xFF (dziura)
komórka o nachyleniu / szorstkości ponad próg   -> 0xFF (dziura)
pionowa ściana rekonstrukcji                    -> L2 albo wykluczenie, nigdy grunt
```

Maska pokrycia i pewności staje się **obowiązkowym** etapem cookera, nie opcją. Auto po prostu
nie ma gdzie wjechać w śmieci.

**(b) Warstwa wizualna przestaje być darmowa.** 1,78 mln trójkątów rysowanych bez cullingu co
klatkę to problem GPU niezależny od fizyki. Dlatego **culling per chunk wchodzi już do F5**,
a nie dopiero do F7.

**(c) Jedna kotwica teleportu nie wystarczy na 1,2 km.** Rejestr kotwic w obrębie skanu
(rdzeń wsi, droga, wzniesienie, obrzeże) jest częścią F5, a nie późniejszym udogodnieniem.

**Czego to NIE zmienia:** budżetu pamięci gruntu (26,5 MB to nadal nic), kolejności warstw,
ani bramek. Cook potrwa dłużej i wygeneruje więcej artefaktów do obejrzenia — to wszystko.

---

## 12. Ryzyka i falsyfikatory

| # | Ryzyko | Falsyfikator / mitygacja |
| --- | --- | --- |
| R1 | Serializacja blobów okaże się niestabilna między buildami | **F2 jest spikiem właśnie po to.** Jeśli padnie — wypiek w locie + limit trójkątów, i mówimy to wprost zamiast udawać |
| R2 | 0,5 m gubi krawężniki i wąskie rowy — droga „traci charakter" | strefy 0,25 m + L2 na krawężniki; **decyduje przejazd, nie liczba** |
| R3 | Filtr gruntu źle sklasyfikuje (dach jako grunt / droga jako obiekt) | maski do obejrzenia w F3 przed jakąkolwiek fizyką; ręczna korekta jest prawdą |
| R4 | Warstwy L1/L2 nachodzą ⇒ auto klinuje | test rozłączności w cookerze (F8), zanim cokolwiek trafi do świata |
| R5 | Świat pojazdu ma `b3World_EnableContinuous(false)`; mesh + prędkość ⇒ tunelowanie | L1 to heightfield (odporniejszy); test pełnej prędkości w F5; CCD ewentualnie tylko dla nadwozia |
| R6 | Budżet ms/step przekroczony | budżety w §7.2 są **bramką**, nie życzeniem; regresja = STOP |
| R7 | Program jest długi, energia wyparuje | F1 daje obraz od razu, F5 daje jazdę; każdy etap ma osobną wartość i osobny sign-off |
| R8 | Prywatne dane wyciekną do repo | `.gitignore` + manifest bez ścieżek/hashy źródła + rejestr wyłącznie lokalny; ta dyscyplina PoC jest dobra i zostaje |
| R9 | Recovery mapy wejdzie w drogę | gałąź od `445db88`, rozłączne pliki, kafel skanu przestrzennie rozłączny z kampusem i torem |
| **R10** | **(nowe, z D2)** peryferyjne artefakty w świecie: auto jedzie po „ścianie rekonstrukcji" albo po rozciągniętym trójkącie | maska pokrycia/pewności ⇒ `0xFF` (dziura) jest **obowiązkowym** etapem cookera; progi zatwierdzasz w F4 patrząc na mapy, nie na liczby |
| **R11** | **(nowe, z D2)** 1,78 mln trójkątów wizualu bez cullingu ⇒ spadek FPS | culling per chunk przesunięty z F7 do **F5**; budżet FPS jest bramką F5 |
| **R12** | **(nowe, z D2)** 1,2 km terenu, gubisz się / nie wiesz gdzie jechać | rejestr kotwic w obrębie skanu już w F5; mapa poglądowa regionu z F3 jako materiał do wyboru kotwic |

---

## 13. Anty-zakres

- nie mergujemy `agent/project-refoundation-audit-v1` (+49 975 linii wobec `main`, w tym
  równoległy system zarządzania: `tools/automation/**`, `tools/project/**`, `AGENTS.md`,
  `PROJECT_*`, protokół „Control Issue" — konkuruje z `README_FOR_AGENTS.md`);
- nie bierzemy ich `tools/gate.ps1` (rozjechany +63/−20) — dokładamy tylko wywołanie testów skanu;
- nie ruszamy `src/`, `include/` (rdzeń box3d) — **cały ten plan mieści się w publicznym API**;
- nie ruszamy zaakceptowanej fizyki M7/M8 ani układu UI;
- nie włączamy `BOX3D_DOUBLE_PRECISION` w tym programie;
- nie commitujemy paczek ani datasetu (107 MB / 509 MB, prywatne);
- nie naprawiamy przy okazji E2R/E3 — to osobny track;
- nie twierdzimy, że skan jest „authored world" — pozostaje dowodem źródłowym osadzonym w mapie.

---

## 14. Czego potrzebuję, żeby ruszyć

1. **D1–D6** z §11.
2. Do F4: Twojego oka na mapach diagnostycznych — wskażesz region z prawdziwą drogą.
3. Potwierdzenie gałęzi `jozz-scan-terrain-f0` od `445db88`.

Plików źródłowych **nie potrzebuję** — dataset (509 MB: 167 MB GLB + 343 MB PLY) i gotowa
paczka PoC (107 MB) są lokalnie w `JS_Photogrametry/`, i to wystarcza aż do F9. Nowe skany
będą potrzebne dopiero przy sprawdzeniu łączenia — wtedy wystarczy wrzucić katalog i
uruchomić cooker.
