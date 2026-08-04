> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# Mapa — Etap 2R: centralny kampus testowy i odzyskanie obstacle kitu

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.
Status: **OTWARTY PO ODRZUCENIU PIERWSZEJ IMPLEMENTACJI**.

## 1. Decyzja

Implementacja `b8afab9` nie jest bazą layoutu do dalszego rozwijania. Jest
bazą techniczną do odzyskania generatorów przeszkód. Sześć lane'ów na
`x=150..195` zostaje zastąpione kampusem na całym środkowym kaflu gridu.

W tym etapie nie robimy mechanicznego revertu. Najpierw zachowujemy dowód
obecnego stanu, klasyfikujemy kod na „odzyskać / poprawić / usunąć”, a dopiero
potem przebudowujemy placement. Etap 3 nie startuje przed ręczną akceptacją
nowego kampusu przez Jozza.

## 2. Co zostaje, co odpada

### Zostaje po audycie

- moduł `jozz_vehicle_obstacle_kit.{h,cpp}` i parametryczne generatory;
- wspólny helper nadający kategorię terenu i materiał;
- deterministyczny rock garden;
- etykiety rysowane per-frame z distance cullingiem;
- arbitralny teleport `JOZZ_M6_TELEPORT_XZ` jako narzędzie testowe;
- współdzielenie course'u przez M5 i M6.

### Do poprawy

- każdy generator dostaje jawny opis anchoru, kierunku, footprintu i
  oczekiwanego wyjścia;
- geometria, która wygląda poprawnie tylko z góry, musi przejść ujęcie z
  wysokości koła oraz przejazd w deklarowanym kierunku;
- kolory całych brył zastępujemy neutralnym materiałem i małym akcentem
  trudności;
- receptury stanowisk przenosimy poza `m5_test_course.cpp`;
- stairs/ruts/berm/gap jump przechodzą osobne sanity checki geometrii,
  kierunku i bezpiecznego wybiegu.

### Odpada

- `kPoligonOriginX=150`, `kPoligonLaneLength=45`, sześć `kLanes` i
  layout `z=-60..60`;
- idea sześciu równoległych pasów jako głównego doświadczenia;
- nasycone zielone/żółte/czerwone całe przeszkody;
- pakowanie stanowisk kursorem `lane.x += ...`;
- przeniesienie wszystkich propów daleko od środka bez zastąpienia ich
  świadomie zaprojektowaną zatoką interakcyjną.

## 3. Geometria środkowego kafla

Nominalne granice kafla `C`:

- `x,z∈[-66.667,66.667]`;
- margines techniczny 6 m od każdej krawędzi;
- używalny layout `x,z∈[-60,60]`;
- cały base shape pozostaje proceduralnym gridem;
- minimum 55% powierzchni kafla pozostaje nieprzykrytym, widocznym gridem.

### 3.1 Rdzeń

`Central Core`: `x,z∈[-12,12]` (24×24 m).

- spawn i restart pojazdu;
- miejsce na obrót, oglądanie rigu i strojenie;
- zero statycznych przeszkód i zero domyślnego scatteru;
- osie X/Z gridu pozostają czytelne;
- cztery korytarze/spokes o szerokości minimum 10 m prowadzą do stanowisk.

### 3.2 Obwodowy korytarz

Między stanowiskami zostaje przejezdna pętla na gridzie. Nie jest osobnym
shape'em ani drogą o innym materiale — to zarezerwowany, pusty footprint.

- orientacyjna oś pętli: prostokąt `x,z≈±48`;
- szerokość korytarza minimum 8 m;
- pozwala objechać kampus i wrócić do rdzenia bez cofania;
- jej cztery bramy staną się później łącznikami do kafli W/N/E/S.

## 4. Cztery podstrefy zamiast sześciu lane'ów

Dokładne współrzędne finalizuje tabela danych po pierwszym renderze footprintów.
Poniższe prostokąty są budżetem, nie magicznymi liczbami w builderze.

| Podstrefa | Budżet footprintu | Kierunek | Receptury | Cel |
|---|---:|---|---|---|
| **N — Komfort i rytm** | `x=-46..46, z=27..50` | W→E, dwukierunkowa po walidacji | speed bump niski, krótki washboard, łagodne whoops | mała amplituda, częstotliwość, tłumienie |
| **W — Artykulacja** | `x=-52..-27, z=-28..26` | S→N | articulation ramps, płytkie ruts, off-camber | skok zawieszenia, kontakt naprzemienny, przechył |
| **E — Teren punktowy** | `x=27..52, z=-26..28` | N→S | rock garden, 1–2 logs, niski berm/wyjście | punktowe uderzenia, trakcja, praca koła |
| **S — Impact i lot** | `x=-46..46, z=-52..-28` | W→E | step 0.1/0.25, kicker łagodny, tabletop kompaktowy | dobicie, oderwanie, kontrolowane lądowanie |

Twardy gap jump i duża skocznia 25° nie mieszczą się odpowiedzialnie w
centralnym kaflu wraz z wymaganym rozbiegiem i wybiegiem. Zostają w obstacle
kit, ale ich docelowa stacja powstanie na kaflach `E/SE` w Etapie 3.
Nie wciskamy generatora na mapę tylko dlatego, że istnieje.

## 5. Zatoka interakcyjna — odzyskanie ducha pierwszej mapy

W narożniku NW centralnego kafla, poza korytarzem pętli, powstaje mała zatoka
z 6–8 lekkimi propami:

- dwie skrzynki, dwie kule, dwa różne rozmiary;
- ustawienie ręczne i czytelne, nie losowy scatter;
- propy nie leżą na osi spawnu, najazdu ani wybiegu;
- wszystkie mają kategorię `0x1` i pełny reset;
- zatoka ma wyglądać jak zaproszenie do interakcji znane z pierwszej mapy,
  ale nie może konkurować z testami zawieszenia.

Pozostałe ciężkie propy czekają na plac fizyki/spawner.

## 6. Model danych stanowiska

Wprowadzić dane niezależne od world-buildingu:

```cpp
struct JozzTestStationSpec
{
    JozzStationId id;
    const char* name;
    JozzStationKind kind;
    JozzTileId tile;
    b3Vec2 centerXZ;
    float yawDegrees;
    b3Vec2 footprintHalfExtents;
    float approachLength;
    float runoffLength;
    float recommendedSpeedMin;
    float recommendedSpeedMax;
    JozzDifficulty difficulty;
    bool bidirectional;
};
```

Nie chodzi o przybicie dokładnie tej reprezentacji, lecz o kontrakt. Builder
nie może ponownie wyliczać rozstawu kursorem bez wiedzy o footprintach.

### Walidator layoutu

Nowa sonda CLI ma failować, gdy:

- footprint wykracza poza `[-60,60]`;
- przecina Central Core;
- przecina inny footprint lub zarezerwowany korytarz;
- brakuje minimalnego najazdu/wybiegu;
- anchor teleportu leży wewnątrz shape'a;
- stanowisko deklarowane jako dwukierunkowe nie ma wybiegu po obu stronach;
- shape jezdny nie ma kategorii terenu;
- prop ma kategorię terenu.

Walidator drukuje tabelę: ID, bounds, approach, runoff, liczba body/shape.

## 7. Podział kodu

- `jozz_vehicle_obstacle_kit.{h,cpp}`: tylko geometria pojedynczej przeszkody;
- `jozz_vehicle_central_test_campus.{h,cpp}`: station specs, receptury,
  budowa kampusu, etykiety i reset zatoki;
- `jozz_vehicle_world_layout.h`: granice kafli, identyfikatory, kotwice i
  stałe globalne; bez 200 linii receptur;
- `jozz_vehicle_m5_test_course.cpp`: cienka orkiestracja modułów świata;
- `validation/jozz_probes_map.cpp` lub równoważny mały plik: czyste sondy
  layoutu i kategorii.

Warunek architektoniczny: po etapie course ma być wyraźnie krótszy niż obecne
313 linii i nie może zawierać osobnej funkcji Build* dla każdego stanowiska.

## 8. Kolejność wykonania

### E2R.0 — baseline odrzuconego stanu

- zachować render całej płyty, środkowego kafla i obecnego poligonu;
- zachować polecenia/kamerę;
- zapisać liczbę body/shape i wynik gate;
- bez zmian geometrii.

### E2R.1 — kontrakt danych i walidator

- station specs, tile bounds, exclusion zones, approach/runoff;
- sonda działa najpierw na danych nowego layoutu;
- nadal bez budowania przeszkód.

### E2R.2 — audyt obstacle kitu

- tabela 15 generatorów: KEEP / FIX / DEFER;
- render każdej używanej receptury z wysokości koła;
- poprawki generatorów osobno od placementu.

### E2R.3 — skeleton kampusu

- tylko neutralne footprint markers/bramki i etykiety;
- top-down środkowego kafla;
- **STOP: akceptacja layoutu przez Jozza przed wstawieniem brył.**

### E2R.4 — stanowiska i zatoka

- wstawienie przeszkód do zaakceptowanych footprintów;
- neutralny język materiałów + małe akcenty trudności;
- integracja M5/M6, reset, teleporty.

### E2R.5 — jazda i sign-off

- przejazdy, rendery, liczby;
- ręczna jazda Jozza;
- dopiero wtedy status Etapu 2 zmienia się na zaakceptowany.

## 9. Bramka techniczna

- build `samples`, `jozz_vehicle_validation`, `test`;
- validator z roota: wszystkie istniejące sondy + nowe sondy mapy;
- `test.exe`;
- boot smoke M5 i M6, 0 błędów sokol;
- liczba shape'ów stabilna po 10 restartach/regeneracjach;
- M5 i M6 budują identyczny kampus;
- R5: każda jezdna powierzchnia = teren, każdy prop = `0x1`;
- brak overlapów, naruszenia core i wyjazdu poza centralny kafel.

## 10. Bramka wizualna i przejazdowa

Obowiązkowe identyczne kamery:

1. `before_4c125ee_center` — stara skupiona mapa;
2. `rejected_b8afab9_center` — pusty środek;
3. `candidate_e2r_center` — nowy kampus;
4. `candidate_e2r_plate` — cała płyta;
5. po jednym ujęciu z wysokości kierowcy dla N/W/E/S.

Kryteria:

- z góry natychmiast widać rdzeń, cztery podstrefy i obwodowy powrót;
- techniczny grid pozostaje dominującą powierzchnią;
- żadne stanowisko nie wygląda jak przypadkowo rozsypane bryły;
- z rdzenia widać co najmniej trzy wejścia do podstref;
- wszystkie wejścia są osiągalne w mniej niż 60 m od spawnu;
- Komfort: 8–12 m/s bez nieoczekiwanego wyrzutu;
- Artykulacja: wolny przejazd, naprzemienny kontakt kół;
- Teren punktowy: brak zakleszczenia na geometrii generatora;
- Impact: kontrolowane oderwanie i bezpieczne lądowanie w footprint;
- pełne okrążenie korytarza bez cofania.

## 11. STOP-gate i definicja „gotowe”

Etap nie jest gotowy, gdy tylko:

- kompiluje się;
- ma wszystkie generatory;
- shape count się zgadza;
- da się przejechać wybrany odcinek.

Etap jest gotowy dopiero, gdy bramka techniczna jest zielona, render skeletonu
został wcześniej zaakceptowany, finalny kampus przeszedł realną jazdę, a Jozz
potwierdził, że środkowy kafel odzyskał fokus i jest lepszy od pierwszej mapy.
