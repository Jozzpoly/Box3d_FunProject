# Plan fundamentu: teren ze skanu jako pierwszorzędna warstwa świata

> ⚠️ **ZASTĄPIONY jako plan wykonawczy przez `PLAN_FUNDAMENT_SKANU_v3_KONTRAKT_PL.md`**
> (2026-07-24, pivot Jozza: auto‑klasyfikator = „automat, którego nie użyjemy";
> separację robi Jozz zewnętrznie, silnik konsumuje role). Ten dokument żyje już
> **tylko jako rejestr pomiarów** (§2: heightfield 0,5 m vs mesh — nadal prawdziwe).
> Sekcje §4.1/§4.2 i decyzje D7–D9 są **martwe** — nie realizować.

**Data:** 2026-07-24
**Autor:** Luna (agent), na zlecenie Jozza
**Status:** `WERSJA 2 — po feedbacku Jozza o źródle gruntu; decyzje D1–D6 podjęte, D7–D9 otwarte`
**Historia:** wersja 1 (heightfield 0,5 m z chmury PLY) **odrzucona przez pomiar** — patrz §2.

**Teza wersji 2:** grunt ze skanu ma być **dokładny co do trójkąta źródła**, a wszystko, co
nie jest gruntem — domy, drzewa, słupy, płoty, artefakty — ma zostać **odseparowane i
zastąpione uproszczonymi bryłami**. To nie jest kompromis, tylko jednoczesna wygrana:
koła czują prawdziwą fakturę terenu, a koszt kolizji spada, bo ⅔ geometrii przestaje być
zupą trójkątów.

Snapshot stanu przed startem: `origin/jozz-map-wip-snapshot-2026-07-24` (`8dbe3b3`).

---

## 1. Streszczenie

1. Feedback Jozza („PLY jako źródło gruntu — nie jestem pewny; teren musi być dokładny")
   jest **słuszny**, ale z innego powodu, niż sam podał — i ten powód jest mocniejszy (§2.5).
2. Zmierzyłem paczkę. **Heightfield 0,5 m niszczy dokładnie tę informację, po którą tu
   jesteśmy**: fakturę terenu w skali 3–6 cm (§2.3).
3. Ta sama miara pokazuje, że **tylko ~⅓ geometrii skanu to w ogóle grunt**, a ~⅕ to
   powierzchnie niemal pionowe (§2.2). Intuicja Jozza o segmentacji jest liczbowo
   potwierdzona.
4. Fundament zmienia się na **sześć klas semantycznych** (grunt / struktury / roślinność /
   detale / odrzucone / render), każda z własną reprezentacją fizyczną dobraną do tego,
   czym naprawdę jest (§4).
5. PLY nie znika — dostaje lepszą rolę: **klasyfikacja, weryfikacja i naprawa**, a nie
   źródło powierzchni (§5).

---

## 2. Dowód pomiarowy (wykonany 2026-07-24 na lokalnej paczce)

Paczka `source-preview-aee5242a20848294`, 7 kafli, 1 770 391 trójkątów z policzalną normalną.
Skrypty pomiarowe w scratchpadzie sesji; wyniki poniżej są jedynym, co z nich zostaje.

### 2.1 Rozdzielczość źródła jest nierówna i miejscami bardzo wysoka

Mediana długości krawędzi trójkąta:

| kafel | 2 | 4 | 1 | 3 | 5 | 0 | 6 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| mediana krawędzi | **8,7 cm** | 13,3 cm | 21,2 cm | 29,1 cm | 42,0 cm | 45,2 cm | 47,9 cm |

Percentyl 10 schodzi do **3,6 cm**. Źródło niesie realny detal centymetrowy w rdzeniu i
kilkudziesięciocentymetrowy na obrzeżach — to typowy rozkład nalotu skupionego na centrum.

### 2.2 Tylko jedna trzecia tego jest kandydatem na grunt

Rozkład nachylenia trójkątów (kąt normalnej do pionu), całość:

```text
< 10 stopni   10,9%     <- plaski grunt
< 20 stopni   24,3%
< 30 stopni   35,2%     <- gorna granica "to moze byc teren"
< 45 stopni   50,7%
> 75 stopni   19,6%     <- sciany, pnie, sciany rekonstrukcji
```

**~65% geometrii skanu to nie jest teren.** Dokładnie to, co Jozz zapowiedział, że będzie
oddzielał — z liczbą.

Budżet gruntu po segmentacji: `1,77 mln × 35% ≈ 620 tys. trójkątów` na całe 1,32 km².
Przy 66 niepustych chunkach 128 m to **~9,4 tys. trójkątów na chunk** — dla statycznego
mesha z BVH to nic.

### 2.3 Faktura w skali koła — i dlaczego heightfield ją zabija

Komórki 0,30 m (rząd wielkości plamki kontaktu opony), tylko trójkąty o nachyleniu < 25°,
rozrzut pionowy w komórce:

| kafel | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| rozrzut Y, mediana | **3,1 cm** | 4,4 cm | 5,3 cm | 5,0 cm | 5,9 cm | 53,4 cm | 27,8 cm |

W rdzeniu: **3–6 cm zmienności wysokości na 30 cm powierzchni**. To jest dokładnie ten
sygnał, który zawieszenie ma przetwarzać.

> Uczciwie: te 3–6 cm to mieszanka prawdziwej faktury i szumu rekonstrukcji. Rozdzielenie
> ich to praca nad jakością skanu. Ale **heightfield 0,5 m usuwa jedno i drugie naraz** —
> uśrednia 30-centymetrową fakturę do jednej próbki na pół metra.

Percentyl 90 tego rozrzutu sięga metrów, a p99 dziesiątek metrów. To komórki, w których
nad gruntem stoi ściana albo korona drzewa. **Heightfield ma z definicji jedną wysokość na
(x,z)** — takich miejsc nie umie wyrazić i musiałby jedno z dwóch skasować.

### 2.4 Chmura punktów jest 17× gęstsza od meshu

```text
PLY:  23 937 988 punktow  (7 plikow, 343 MB)
mesh:  1 409 687 wierzcholkow
```

To obala moje wcześniejsze uzasadnienie „PLY bo surowsze" **w drugą stronę niż myślałem**:
PLY nie jest gorszy, jest znacznie bogatszy. Problemem nigdy nie był wybór PLY — problemem
było **przepuszczenie czegokolwiek przez siatkę 0,5 m**.

### 2.5 Werdykt: Jozz ma rację, ale z mocniejszego powodu

| Teza | Ocena |
| --- | --- |
| „PLY będzie znacznie różnić się od modelu terenu" | **Częściowo.** Nikt tego nie udowodnił w żadną stronę — ich własny manifest ma `GLB_PLY_INTERIOR_CORRESPONDENCE = false`, a dowód par był `BOUNDS_ONLY`. Budowanie fizyki na niezweryfikowanej zgodności było błędem planu v1 |
| „Potrzebuję realnie kół reagujących na teren" | **Tak, i to przesądza sprawę.** §2.3: faktura 3–6 cm vs komórka 50 cm |
| „Teren musi być dokładny" | **Tak** — z jednym doprecyzowaniem: dokładny **względem powierzchni, którą widzisz**, czyli GLB. Fizyka z innego źródła niż render to systematyczna rozbieżność „auto jedzie tam, gdzie nic nie widać" |
| „Oddzielę teren od dróg, domów, lasów — to ułatwi i zoptymalizuje kolizję" | **Tak, i to jest największa wygrana w całym projekcie.** §2.2: 65% geometrii przestaje być zupą trójkątów |

### 2.6 Przy okazji: kafle to nie LOD-y, ale mają pasy podwójnej geometrii

Nakładanie footprintów kafli: maks. **19,2%** wąskiego kafla, w pasach 3–12 m szerokości.
Czyli kafle są przestrzenne (nie ma stosu LOD-ów = nie ma podwójnej powierzchni w całym
świecie), **ale w pasach styku geometria się dubluje**. Cook musi deduplikować szwy —
inaczej auto jeździ po dwóch powierzchniach naraz w tych pasach.

---

## 3. Co PoC robi źle (bez zmian z wersji 1, uzupełnione o pomiar)

Gałąź `agent/project-refoundation-audit-v1` jest **potomkiem** `jozz-vehicle-sandbox-m0`
(445db88): `git diff samples/` = **14 plików, +3555, −0**. To nie jest starsza wersja
projektu — uboższe UI to `P2B Scan Drive`, 1025-liniowy **klon** laboratorium M6.

| # | Defekt | Dowód | Konsekwencja |
| --- | --- | --- | --- |
| **D1** | cały skan otagowany `JOZZ_M6_TERRAIN_CATEGORY` | `jozz_vehicle_scan_drive_lab.cpp` | **Realny błąd fizyki.** Kategoria terenu obsługuje *toczącą się kulę* split wheel envelope. §2.2: 65% tej geometrii to ściany i pnie — koło „toczy się" po elewacji budynku |
| **D2** | `identifyEdges = false` | tamże; wszystkie buildery box3d dają `true` | brak flag krawędzi wklęsłych ⇒ duchy pod kołami |
| **D2b** | **mesh dzielony per kafel × per materiał** (25 grup) | `COMPLETE.json`, 7 kafli / 2–6 grup | sąsiedztwo trójkątów działa tylko **wewnątrz** jednego mesha ⇒ nawet z `identifyEdges=true` duchy zostają **na każdej granicy droga/trawa**. To gorszy wariant D2 i nikt go nie zauważył |
| **D3** | jedno tarcie 0.9 na asfalt, trawę, błoto i dach | `baseMaterial.friction = 0.9f` | zero `userMaterialId` |
| **D4** | wypiek BVH 1,78 mln trójkątów przy każdym starcie (40,4 s Debug) | `DRIVE_THE_SCAN.md` | zbędny — §6.1 |
| **D5** | „dokładnie jedna paczka albo nic" | `FindJozzActiveScanPreviewPack()` | wyklucza łączenie skanów |
| **D6** | zero segmentacji | cały pipeline | §2.2 |
| **D7** | wizual i kolizja to ten sam mesh | `m_visualPack.Load(packDir)` | LOD wizualny zmieniałby fizykę |
| **D8** | pasy styku kafli nieodeduplikowane | §2.6 | podwójna powierzchnia w pasach 3–12 m |

---

## 4. Fundament: sześć klas semantycznych

To jest model Jozza, doprowadzony do konkretu i przypięty do API box3d.

```text
KLASA          CO TO JEST                REPREZENTACJA FIZYCZNA           KATEGORIA
-------------  ------------------------  -------------------------------  ---------
G  GRUNT       teren, drogi, pobocza,    mesh 1:1 z GLB, spawany w jeden  0x2 TERRAIN
               place, sciezki            spojny bufor, identifyEdges,     (toczaca sie kula)
                                         material per trojkat, BVH
                                         upieczony offline
S  STRUKTURY   domy, budynki, mury,      b3CreateHull per bryla           0x1 OBJECT
               ogrodzenia murowane       (maxVertexCount ~16-32),         (pelny walec)
                                         grupowane w b3CreateCompound
V  ROSLINNOSC  drzewa, krzewy, zywoploty kapsula pnia albo brak kolizji   0x1 OBJECT
                                         (decyzja D7)
D  DETALE      slupy, znaki, ploty,      kapsula / box                    0x1 OBJECT
               latarnie, smietniki
X  ODRZUCONE   artefakty peryferyjne,    BRAK kolizji                     -
               sciany rekonstrukcji,     (opcjonalnie tez brak renderu)
               rozciagniete fragmenty
R  RENDER      pelny mesh teksturowany   BRAK fizyki                      -
```

### Reguły, które to trzymają w kupie

1. **Każdy trójkąt źródła należy do dokładnie jednej klasy.** Test w cookerze, nie dobre chęci.
2. **Tylko G jest dokładne.** Reszta to bryły. To jest sedno optymalizacji, którą Jozz
   przewidział: 620 tys. trójkątów gruntu zamiast 1,77 mln zupy, plus kilkadziesiąt hulli
   na chunk zamiast setek tysięcy trójkątów ścian.
3. **G jest jednym spójnym meshem na chunk**, nie zbiorem meshy per materiał. Materiały
   wchodzą jako `materialIndices` (uint8 na trójkąt), **nie** jako osobne kształty.
   To likwiduje D2b — duchy na granicy droga/trawa.
4. **Drogi nie są osobną warstwą geometryczną** — są trójkątami klasy G z materiałem
   „asfalt". Rozdzielenie, którego chce Jozz, dzieje się przez `userMaterialId`, i dzięki
   temu przejście asfalt→pobocze→trawa jest ciągłą powierzchnią o zmiennym tarciu, a nie
   szwem między dwoma kolizjami.
5. **X jest pełnoprawną klasą, nie śmietnikiem.** Peryferyjne artefakty dostają jawny
   status „nie ma kolizji", a nie są cicho zostawiane.

### 4.1 Gdzie dokładnie kończy się G, a zaczyna S (rozstrzygnięcie D8)

Jozz oddał tę decyzję („zdecyduj sam krytycznie"). Odrzucam własną wcześniejszą propozycję
progu „wszystko poniżej 50 cm to G" — 50 cm było liczbą wziętą z powietrza.

**Właściwe kryterium wynika z geometrii auta, nie z arbitralnej wysokości.** Powód jest
konkretny: klasa G ma kategorię `0x2`, którą split wheel envelope obsługuje **toczącą się
kulą**, a klasa S kategorię `0x1`, obsługiwaną **pełnym walcem**. Czyli:

```text
na co auto NAJEZDZA (koło się toczy)  -> musi byc w G, dokladnym meshem
w co auto UDERZA (koło nie wjedzie)   -> S, bryla
```

Fizyczną stałą, która to rozdziela, jest **promień koła** (M6 bierze go z kontraktu assetu,
~0,3 m): uskok niższy niż promień koła da się najechać, wyższy zatrzymuje koło na ścianie.

**Reguła G (do implementacji w etapie `classify`):**

```text
G = spojna skladowa NAJAZDOWA, wypelniana od ziaren gruntu (flood fill), gdzie
    krawedz miedzy trojkatami jest przechodnia, jesli:
        nachylenie trojkata     <  ~40 stopni
        uskok na krawedzi       <= promien kola (~0,30 m)
Wszystko poza ta skladowa -> S / V / D / X.
Pionowe sciski NIZSZE niz prog uskoku (krawezniki, progi, wjazdy) ZOSTAJA w G
i sa dokladne - to sa wlasnie miejsca, gdzie zawieszenie pracuje.
```

Dlaczego to jest lepsze niż próg wysokości:

- **dach sam się wyklucza** — jest powierzchnią o małym nachyleniu, ale nie jest połączony
  z gruntem żadną przechodnią krawędzią, więc flood fill tam nie wejdzie;
- **rampa, podjazd, taras najazdowy same się włączają** — są połączone, więc wejdą, i to
  niezależnie od tego, na jakiej są wysokości;
- **góra muru oporowego** trafia do G tylko wtedy, gdy da się tam wjechać — czyli
  dokładnie wtedy, gdy to ma znaczenie dla gry;
- kryterium jest **wyprowadzone z auta**, więc zmiana promienia koła przelicza je automatycznie,
  zamiast unieważniać ręcznie dobraną stałą.

Falsyfikator: jeśli flood fill „przecieknie" na dach przez rampę albo przewrócone drzewo,
zobaczysz to na mapie klas w F4 i utniesz wielokątem wykluczającym (D9). To jest tańsze niż
zgadywanie progu z góry.

Koszt: potrzebny graf sąsiedztwa trójkątów dla 1,77 mln trójkątów — offline, numpy/scipy,
jednorazowo w cooku.

### 4.2 Roślinność: kolizja prawdziwa, z jawną granicą zasięgu auta (rozstrzygnięcie D7)

Jozz wybrał kolizję **prawdziwą**. Interpretuję to jako: *auto ma realnie zderzać się ze
wszystkim, czego fizycznie może dotknąć* — i tak to implementuję, z jednym jawnym
ograniczeniem, które jest wierniejsze rzeczywistości, a nie tańsze:

```text
pien i galezie PONIZEJ ~4 m       -> kolizja z rzeczywistej geometrii (hull/kapsula
                                     dopasowana do skanu, nie nominalny walec)
korona POWYZEJ ~4 m               -> brak kolizji: auto tam nie siega, a bryla korony
                                     w fotogrametrii jest nieostra i staje sie
                                     NIEWIDZIALNA SCIANA W POWIETRZU - to byloby
                                     mniej prawdziwe, nie bardziej
krzewy i zywoploty do ~1,5 m      -> kolizja WLACZONA (auto nie przenika przez zywoplot)
trawa i niska ziemia              -> czesc G, bez osobnej obslugi
```

Czyli: prawdziwa kolizja wszędzie tam, gdzie auto może dosięgnąć, i żadnej zmyślonej
geometrii tam, gdzie nie może. Próg 4 m jest parametrem, nie dogmatem.

Falsyfikator: jeśli przy przejeździe pod drzewami auto zaczepia o coś niewidzialnego,
próg jest za wysoki i schodzimy niżej. Jeśli przejeżdżasz przez pień — hull jest za mały.
Jedno i drugie widać na pierwszym przejeździe.

### Dlaczego to jest właściwa odpowiedź na „duszę projektu"

- Koła dostają **prawdziwą powierzchnię źródła**, nie jej resampling — to jest warunek
  „realnie reagujących kół".
- Zachowanie na drodze vs na trawie wynika z **materiału powierzchni**, nie ze skryptu
  (README §1: zachowanie ma wynikać z konstrukcji).
- Domy i drzewa trafiają w istniejący split wheel envelope po **właściwej** stronie
  (pełny walec vs obiekt), zamiast udawać jezdnię.
- Klasy S/V/D są tanie, więc budżet idzie tam, gdzie ma sens: w grunt.

---

## 5. Nowa rola PLY: instrument jakości, nie źródło powierzchni

PLY (23,9 mln punktów, 17× gęstsze od meshu) jest zbyt cenne, żeby je wyrzucić, i zbyt
surowe, żeby robić z niego powierzchnię — rekonstrukcja powierzchni z chmury to dokładnie
to, co ContextCapture już zrobił, i zrobił lepiej, niż zrobilibyśmy my.

| Zastosowanie PLY | Po co |
| --- | --- |
| **Klasyfikacja G/S/V/D** | filtry morfologiczne (`scan_ground_filters.py` już istnieje i nigdy nie był podłączony) działają natywnie na chmurze; wynik mapuje się na trójkąty GLB |
| **Mapa residuów GLB↔PLY** | gdzie mesh odbiega od surowych punktów = gdzie został wygładzony, załatany albo zmyślony. **To jest przyrząd, którego Jozz potrzebuje do „polepszania jakości skanów"** |
| **Łatanie dziur** | GLB ma braki tam, gdzie chmura ma punkty |
| **Weryfikacja skali i poziomowania** | niezależne źródło do sprawdzenia ramy współrzędnych |
| **Wykrywanie roślinności** | rozkład punktów w pionie (wielokrotne zwroty przez koronę) odróżnia drzewo od dachu lepiej niż sam mesh |

Przy okazji **domyka to** ich otwarte `GLB_PLY_INTERIOR_CORRESPONDENCE = false`: mapa
residuów jest właśnie tym dowodem, którego nigdy nie zrobiono.

---

## 6. Zdolności box3d, na których to stoi

Wszystko zweryfikowane w nagłówkach **tego** repo. Bez zmian w `src/`/`include/`.

### 6.1 `b3MeshData` to samowystarczalny blob z BVH i hashem treści

```c
typedef struct b3MeshData {
    uint64_t version;   // B3_MESH_VERSION - "Useful for validating serialized data"
    int      byteCount; // caly rozmiar
    uint32_t hash;      // hash tresci
    int nodeOffset, nodeCount, treeHeight;      // <-- BVH W SRODKU
    int vertexOffset, triangleOffset, materialOffset, flagsOffset;  // offsety OD ADRESU
} b3MeshData;
```

`b3DestroyMesh` to `b3Free(mesh, mesh->byteCount)`; alokator używa wyrównania **64 B**
(`src/core.c:141`). Brak nienazwanego paddingu jest jawnym wymogiem (komentarz w
`height_field.c:471`).

⇒ **BVH gruntu piecze się raz, offline, i wczytuje jako bajty.** To jest zdolność, która
w ogóle umożliwia „grunt dokładny co do trójkąta": bez niej 620 tys. trójkątów to ~14 s
zamrożenia w Debug przy każdym starcie. Wymaga spike'u z falsyfikatorem (§9, F2).

### 6.2 Materiały per trójkąt i `userMaterialId`

`b3MeshDef.materialIndices` (uint8/trójkąt) → `b3ShapeDef.materials[]` (do 256).
`b3SurfaceMaterial` niesie `friction`, `restitution`, `rollingResistance`, `tangentVelocity`
i **`userMaterialId` (uint64)**, który propaguje się do wyników raycastu (`b3CastResultFcn`),
danych kontaktu (`userMaterialIdA/B`) i **callbacków mieszania tarcia/restytucji**
(`b3FrictionCallback`, `b3RestitutionCallback` w `b3WorldDef`).

`b3Shape_SetMeshMaterial(shapeId, material, index)` pozwala stroić je **suwakiem na żywo**,
bez przebudowy geometrii.

### 6.3 `identifyEdges` — sąsiedztwo trójkątów

Liczy flagi krawędzi wklęsłych (`b3_concaveEdge1/2/3`). Działa **tylko wewnątrz jednego
mesha** — stąd wymóg z §4.3 (jeden spójny bufor na chunk, materiały jako indeks).

### 6.4 Bryły uproszczone dla S/V/D

```c
b3HullData*     b3CreateHull( const b3Vec3* points, int pointCount, int maxVertexCount );
b3CompoundData* b3CreateCompound( const b3CompoundDef* def );
b3ShapeId       b3CreateHullShape( bodyId, shapeDef, hull );
b3ShapeId       b3CreateCompoundShape( bodyId, shapeDef, compound );
```

`maxVertexCount` w `b3CreateHull` to wprost pokrętło „jak uproszczona ma być bryła".
Compound pozwala spakować wszystkie proxy jednego chunka w jeden kształt.

### 6.5 Heightfield — zdegradowany, ale nie skreślony

Zdolności realne i sprawdzone: `uint16` skompresowane wysokości, materiał per komórka,
**dziury `0xFF`**, kontrakt dokładnych szwów (`globalMinimumHeight/MaximumHeight`:
*„important if you want height fields to be placed next to each other and line up exactly"*),
brak aktualizacji w miejscu (nie ma `b3Shape_SetHeightField`).

**Nie jest reprezentacją gruntu ze skanu** (§2.3). Zostaje jako:
- reprezentacja terenu **proceduralnego** (już jest, Etap 1 mapy — bez zmian),
- kandydat na **strefy odkształcalne** w przyszłości (JES „żywy grunt"): chunk 64 m to
  ~81 KB, więc przebudowa jest darmowa,
- ewentualny **most** przy fizycznym zszyciu skanu z terenem proceduralnym.

### 6.6 Duże światy

`BOX3D_DOUBLE_PRECISION` (styl Jolta, zakres ±1e7 m, koszt „kilka procent, nie 2×").
**Nie włączamy** — przy osadzeniu z §7.1 najdalszy punkt to ~1,47 km, gdzie `float` ma
0,1 mm rozdzielczości. Piszemy jednak tak, żeby migracja była mechaniczna: żadna funkcja
terenu nie zakłada, że `(0,0,0)` coś znaczy; każda pozycja to `origin chunka + offset`.

---

## 7. Kontrakt przestrzenny i osadzenie

### 7.1 Osadzenie na mapie (D2 = cały skan, D4 = północ, D5 = wyspa)

```text
skan:             1204 x 1099 m, relief 161 m
luka od plyty:    50 m (urwisko, brak polaczenia fizycznego)
srodek kafla:     x = 0,  z = 799,5
obszar:           x in [-602, +602],  z in [+250, +1349]
najdalszy punkt:  ~1,47 km od poczatku swiata
datum Y:          mediana gruntu w rdzeniu wsi -> y ~ 0
```

Kontrola kolizji planistycznych: płyta `z ∈ [−200,200]`, offroad `x ∈ [198,598]` przy
`z ∈ [−200,200]`, place N/NW/NE kończą się na `z = 160`. **Kafel na `z ≥ 250` nie nachodzi
na nic.**

### 7.2 Chunki (D3 = 128 m, ziarno 64 m)

Chunk 128 m jest jednostką **adresowania, streamingu i własności geometrii**:
10×9 = 90 chunków, ~66 niepustych, ~9,4 tys. trójkątów gruntu na chunk.

Uzasadnienie 128 m po zmianie fundamentu: pierwotnie 128 m wynikało z pomiarów rozkładu
trójkątów w raporcie 2026-07-15 (mediana 488 / P95 123 tys. / max 371 tys. — czyli uniform
grid jest dobry do adresowania, zły jako budżet renderu). To ustalenie **przeżywa** zmianę
reprezentacji. Ziarno 64 m zostaje dla stref gęstych i przyszłych stref odkształcalnych.

### 7.3 Klasyfikacja musi przeżyć reimport — to jest warunek „powtarzalnej czynności"

Jozz będzie poprawiał jakość skanów i importował je ponownie. Gdyby decyzje klasyfikacyjne
były zapisane jako indeksy trójkątów, **każdy reimport kasowałby całą pracę ręczną**.

Dlatego (zgodnie z ich własnym `ARCHITECTURE.md`, który mówił „decyzje world-space, nie
indeksy konkretnego gridu"):

```text
decyzje zapisujemy jako:   polygony XZ + zakresy wysokosci + regula klasy
                           + wersja algorytmu + referencja do rewizji zrodla
NIGDY jako:                indeksy trojkatow, node index GLB, handle Box3D
```

Reimport ulepszonego skanu = ponowne zastosowanie tych samych decyzji world-space do nowej
geometrii + raport „co się zmieniło i gdzie decyzja przestała pasować".

---

## 8. Cooker offline

```text
 1 inspect             integralnosc, bounds, pary GLB/PLY, prywatnosc
 2 frame_contract      source -> lab: jednostki, osie, handedness, mirror, poziomowanie
 3 geometry_extract    GLB -> trojkaty; PLY -> chmura
 4 seam_dedup          usuniecie zdublowanej geometrii w pasach styku kafli (par. 2.6)
 5 classify            G/S/V/D/X: geometria (nachylenie, wysokosc nad gruntem, spojne
                       skladowe, pionowosc) + kolor z GLB + rozklad pionowy PLY
 6 review              >>> BRAMKA JOZZA <<< mapy klas do obejrzenia i recznej korekty;
                       zapis decyzji world-space (par. 7.3)
 7 ground_cook         G: spawanie w jeden bufor na chunk, materialy per trojkat,
                       identifyEdges, b3CreateMesh, serializacja bloba
 8 proxy_cook          S/V/D: skladowe spojne -> b3CreateHull / kapsuly -> compound
 9 residual_map        GLB vs PLY: gdzie mesh odbiega od chmury (przyrzad jakosci)
10 texture_cook        1K baseColor (zmierzone: SSIM 1K vs 2K = 0,9988)
11 render_lod_cook     sekcje adaptacyjne + LOD dla R
12 package             manifest, hashe, wersje algorytmow
```

Każdy etap: własny hash wejść, własny katalog, wznawialność, jawna wersja algorytmu.
Model sprawdzony — raport 2026-07-15 osiągnął **95/95 artefaktów byte-identical** przy
niezależnym powtórzeniu.

Powtarzalność jest własnością tego modelu, nie osobną funkcją:
```powershell
python tools\scan_pipeline\scan_cook.py --dataset <katalog> --decisions <plik>
```

### 8.1 Uczciwie o klasyfikacji

Pełna automatyczna segmentacja semantyczna fotogrametrii to problem badawczy. Ten plan
**nie obiecuje** automatu. Obiecuje:

1. heurystyki geometryczne (nachylenie, wysokość nad lokalnym gruntem, spójne składowe,
   pionowość) — wyciągają większość,
2. kolor z GLB i rozkład pionowy PLY jako drugi i trzeci sygnał,
3. **ręczną korektę Jozza jako prawdę**, trwale zapisaną w world-space,
4. raport pokrycia: ile procent trójkątów sklasyfikowano automatycznie z wysoką pewnością,
   a ile czeka na rękę.

To jest ta sama zasada, którą ma ich `ARCHITECTURE.md`: dowód → propozycja → prawda
autorska. Zostaje bez zmian.

---

## 9. Program F0–F9

Gałąź `jozz-scan-terrain-f0` od zacommitowanego **`445db88`** (czysty E1), nie od brudnego
drzewa E2R/E3. Bramka `.\tools\gate.ps1` zielona na każdym etapie; wpis w `CHECKPOINTS_PL.md`;
dla etapów wizualnych **przeczytany screenshot**; bez `ACCEPTED BY JOZZ` + hash nie ruszam dalej.

| # | Etap | Dostarcza | Dowód / falsyfikator |
| --- | --- | --- | --- |
| **F0** | Kontrakty | schematy JSON (chunk, klasy, decyzje world-space, manifest), testy kontraktowe. Zero geometrii | testy schematów; bramka i perf mapy bez zmian |
| **F1** | Port + inspekcja | `tools/scan_pipeline/**`, `tests/**`, czytniki, sample P2A jako narzędzie oglądania. Mapa nietknięta | `run_p1_contracts.py` zielone; P2A renderuje skan; screenshot |
| **F2** | **Spike serializacji bloba** | dowód, że `b3MeshData` da się zapisać i wczytać | upiecz → zapisz → wczytaj → `hash`, `bounds`, `treeHeight`, `nodeCount` identyczne; ten sam wynik drive-probe. **Falsyfikator:** jeśli nie — wypiek w locie, budżet trójkątów na chunk i mówimy to wprost |
| **F3** | Klasyfikacja v1 | etapy 3–5 cooker: dedup szwów, mapy klas G/S/V/D/X, mapa residuów GLB↔PLY | mapy do obejrzenia; raport pokrycia; determinizm (dwa runy byte-identical) |
| **F4** | **Bramka Jozza: klasy** | oglądasz mapy, poprawiasz klasy, zatwierdzasz progi; zapis decyzji world-space | Twoja akceptacja — maszyna tu nie decyduje |
| **F5** | **Grunt w mapie + teleport** ← *pierwotna prośba, na docelowym fundamencie* | `ground_cook` → chunki G → kafel skanu w świecie M6 (§7.1), rejestr kotwic w obrębie skanu, montaż leniwy + culling per chunk, tymczasowy wizual = paczka PoC (§9.1) | `wheel_contacts ≥ 1`; `car_y` identyczne w kroku 300 i 600; **ms/step i FPS przed/po**; zero regresji płyty i offroadu; **przejazd bez zaczepień na granicy droga/trawa** (test D2b); Twoja jazda |
| **F6** | Materiały powierzchni | klasy materiału per trójkąt + `userMaterialId` + suwaki tarcia + debug-widok kolorów | przejazd droga↔trawa: mierzalna różnica poślizgu; widok kolorów zgodny z teksturą |
| **F7** | Struktury i detale (S, D) | `proxy_cook` → hulle i kapsuły w compoundach, kategoria `0x1` | test rozłączności klas; auto nie „wjeżdża" na ścianę toczącą się kulą; liczba kształtów na chunk w budżecie |
| **F8** | Roślinność (V) + warstwa R | drzewa wg decyzji D7; docelowy wizual z sekcjami i LOD | budżet tekstur; FPS; zrzuty z ustalonych kamer |
| **F9** | Wiele skanów + streaming | rejestr paczek, N regionów, montaż po odległości | dwa regiony naraz; montaż chunka ≤5 ms; brak zacięć |

Pierwszy obraz: **F1**. Pierwsza jazda na docelowym fundamencie: **F5**.

### 9.1 Dlaczego F5 pożycza wizual z PoC

F5 dostarcza klasę G. Bez warstwy R byłby to szary mesh z siatką debugową — nie da się
rozpoznać wsi ani ocenić, czy jazda odbywa się tam, gdzie powinna.

Warstwy są rozdzielone, więc R wolno wziąć skądinąd: fizyka z chunków G, obraz z gotowej,
przetestowanej paczki `JSPREV2`, z tym samym transformem i cullingiem per chunk. Zysk jest
podwójny — PoC zostaje użyty do tego, w czym jest naprawdę dobry (render), a **rozbieżność
między obrazem a fizyką staje się widoczna gołym okiem**: jeśli auto jedzie po niewidzialnej
półce albo wpada tam, gdzie widać drogę, od razu wiadomo, że klasyfikacja jest zła. To
lepszy test poprawności G niż jakikolwiek zrzut liczb.

---

## 10. Budżety (bramka, nie życzenie)

```text
krok fizyki:            <= 1,50 ms/step     (dzis mapa: 1,15-1,21 ms/step)
trojkaty gruntu/chunk:  <= 25 tys.          (zmierzona srednia ~9,4 tys.)
proxy S/V/D na chunk:   <= 200 ksztaltow
grunt w pamieci:        <= 64 MB
tekstury:               <= 96 MiB RGBA+mip  (profil 1K, zmierzony)
montaz chunka:          <= 5 ms
```

---

## 11. Decyzje

| # | Decyzja | Stan |
| --- | --- | --- |
| **D1** | fundament vs szybka ścieżka | **program F0–F9** |
| **D2** | zasięg | **cały skan 1204×1099 m** |
| **D3** | chunk | **128 m, ziarno 64 m w strefach** |
| **D4** | miejsce na mapie | **północ, `z > 200`** |
| **D5** | styk z płytą | **wyspa + teleport, urwisko** |
| **D6** | klon `P2B Scan Drive` | **nie portujemy** |
| **D7** | kolizja roślinności | **prawdziwa** — pień i gałęzie poniżej ~4 m z rzeczywistej geometrii, korona wyżej bez kolizji, krzewy/żywopłoty kolidują (§4.2) |
| **D8** | granica dokładności G | **spójna składowa najazdowa** od ziaren gruntu: nachylenie < ~40°, uskok ≤ promień koła; krawężniki i progi zostają w G (§4.1). *Decyzja oddana agentowi przez Jozza; odrzucono wcześniejszy arbitralny próg 50 cm* |
| **D9** | ile ręcznej korekty klas w v1 | **progi globalne + wykluczanie/wymuszanie regionów** wielokątami w world-space (§7.3) |

---

## 12. Ryzyka i falsyfikatory

| # | Ryzyko | Falsyfikator / mitygacja |
| --- | --- | --- |
| R1 | serializacja bloba `b3MeshData` niestabilna między buildami | **F2 jest spikiem właśnie po to**; falsyfikator jawny, fallback opisany |
| R2 | klasyfikacja myli dach z gruntem albo drogę z obiektem | mapy klas do obejrzenia w F3/F4 **przed** jakąkolwiek fizyką; ręczna korekta jest prawdą |
| R3 | koła czują szum rekonstrukcji, nie teren | §2.3 — to realne; osobna praca nad jakością (filtrowanie, residua GLB↔PLY); **decyduje przejazd, nie liczba** |
| R4 | klasy G i S nachodzą ⇒ auto klinuje | test rozłączności w cookerze (F7) przed wpuszczeniem do świata |
| R5 | duchy na granicach materiałów mimo `identifyEdges` | wymóg §4.3 (jeden bufor na chunk); test „przejazd przez granicę droga/trawa" jest bramką F5 |
| R6 | `b3World_EnableContinuous(false)` + mesh + prędkość ⇒ tunelowanie | test pełnej prędkości w F5; ewentualnie CCD tylko dla nadwozia |
| R7 | budżet ms/step przekroczony | §10 jest bramką; regresja = STOP |
| R8 | peryferyjne artefakty w świecie (skutek D2) | klasa X z jawnym „brak kolizji"; progi zatwierdzasz w F4 patrząc na mapy |
| R9 | 1,78 mln trójkątów wizualu bez cullingu ⇒ spadek FPS | culling per chunk już w F5 |
| R10 | 1,2 km terenu — gubisz się | rejestr kotwic w obrębie skanu w F5 |
| R11 | reimport ulepszonego skanu kasuje pracę ręczną | §7.3 — decyzje world-space, nigdy indeksy |
| **R15** | flood fill klasy G „przecieka" na dach albo na koronę drzewa (§4.1) | widać na mapie klas w F4; cięcie wielokątem wykluczającym; próg uskoku i nachylenia są parametrami, nie stałymi |
| **R16** | próg 4 m dla korony (§4.2) daje niewidzialne ściany albo przenikanie pnia | pierwszy przejazd pod drzewami jest testem; oba objawy są jednoznaczne i prowadzą do przeciwnych korekt progu |
| **R17** | graf sąsiedztwa 1,77 mln trójkątów jest wolny albo pamięciożerny w cooku | offline, jednorazowo, numpy/scipy; jeśli nie wyrobi — dzielimy na chunki z marginesem zakładki i sklejamy składowe na granicach |
| R12 | program długi, energia wyparuje | F1 daje obraz, F5 daje jazdę; każdy etap ma osobną wartość i sign-off |
| R13 | prywatne dane w repo | `.gitignore` + manifest bez ścieżek/hashy źródła + rejestr lokalny |
| R14 | recovery mapy wejdzie w drogę | gałąź od `445db88`, rozłączne pliki, kafel przestrzennie rozłączny |

---

## 13. Anty-zakres

- nie mergujemy `agent/project-refoundation-audit-v1` (+49 975 linii wobec `main`, w tym
  równoległy system zarządzania: `tools/automation/**`, `tools/project/**`, `AGENTS.md`,
  `PROJECT_*` — konkuruje z `README_FOR_AGENTS.md`);
- nie bierzemy ich `tools/gate.ps1` (rozjechany +63/−20);
- nie ruszamy `src/`, `include/` — **cały ten plan mieści się w publicznym API box3d**;
- nie ruszamy zaakceptowanej fizyki M7/M8 ani układu UI;
- nie włączamy `BOX3D_DOUBLE_PRECISION` w tym programie;
- nie robimy heightfielda z gruntu skanu (§2.3);
- nie rekonstruujemy powierzchni z chmury punktów (§5);
- nie commitujemy paczek ani datasetu (107 MB / 509 MB, prywatne);
- nie naprawiamy przy okazji E2R/E3;
- nie obiecujemy automatycznej segmentacji semantycznej (§8.1).

---

## 14. Czego potrzebuję, żeby ruszyć

Wszystkie decyzje D1–D9 są podjęte. Zostaje:

1. Potwierdzenie gałęzi `jozz-scan-terrain-f0` od `445db88` — i ruszam z F0.
2. Do F4: Twojego oka na mapach klas — tam decydujesz, co jest terenem, a co domem.

Plików źródłowych nie potrzebuję — dataset (509 MB) i paczka PoC (107 MB) są lokalnie
w `JS_Photogrametry/`, i to wystarcza aż do F9.
