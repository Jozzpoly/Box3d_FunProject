# TECH_DEBT — Jozz Vehicle

Data odświeżenia: 2026-08-04
Ten plik zawiera **wyłącznie otwarty dług**. Zamknięte i historyczne punkty są w
`archive/ledgers/TECH_DEBT_LEGACY_2026-07_PL.md` oraz w Git.

Skala: **P0** blokuje wiarygodność badań, **P1** blokuje następny etap produktu,
**P2** ważne, ale może poczekać, **P3** dziedzictwo JV bez wpływu na JES.

## P0-2 — Brak czystego eksperymentu podatności

Globalne `contactHertz` zmienia wszystkie kontakty świata. Warstwa runtime
`WHEEL-SOFT-03A-1` dodaje już lokalny override normalnej softness wyłącznie dla
`b3_wheelShape`, wspólny dla convex i mesh prepare paths; `0/0` zachowuje
precomputed world softness, a non-wheel shapes nie mogą wpływać na wybór.

Otwarta blokada to teraz headless Q2 runner, maszynowy `metrics.json`, powtarzalny
sweep oraz decyzja na podstawie zachowanych wyników — nie sam hook solvera.

**Spłata:** zbudować Q2 i przeprowadzić A/B przy identycznym manifoldzie; nie
zmieniać jeszcze wartości domyślnych pojazdu.

## P1-2 — Niepełne zapytania geometrii `b3Wheel`

Generic proxy jest konserwatywną sferą; shape cast/overlap nie mają wszędzie
pełnej, dokładnej ścieżki wheel. Raycast jest konserwatywnym walcem, nie profilem.

**Spłata:** jawna macierz API: exact / conservative / unsupported, potem testy.

## P1-3 — Model masy jest przybliżony

`b3ComputeWheelMass` używa walca obwiedniowego. M6 zamraża masę do wartości
referencyjnej, co chroni porównania pojazdu, ale API kształtu nie opisuje masy
rzeczywistego profilu.

**Spłata:** albo dokładne całkowanie bryły obrotowej, albo jawny kontrakt
„collision-only; mass supplied by caller”. Nie zmieniać przed testem topologii.

## P2-1 — Pełny UBSan ma niezależny dług wyrównania

Domyślny build SIMD zatrzymuje się przed ścieżką koła na niealigned `_mm_load_sd`
w `src/mesh.c` / `src/simd.h`. Wcześniejszy przebieg bez tego punktu ujawnił też
misaligned `b3HullData*` store w `src/compound.c:582`. Scalar `WheelShapeTest`
przechodzi czysto pod UBSan, więc pakiet hulla nie jest źródłem tych zgłoszeń.

**Spłata:** osobne minimalne reproduktory i jawny kontrakt wyrównania SIMD oraz
compound; nie mieszać z podatnością opony.

## P2-2 — Wheel–hull używa numerycznego searchu normal fan

Face clipping i phantom-corner cases są zamknięte, lecz maksimum na łuku krawędzi
i wewnątrz stożka wierzchołka jest szukane w ograniczonej liczbie iteracji.
Audit 2000 boxów + 60 nieortogonalnych hullów utrzymuje błąd do 3 mm, ale nie jest
formalnym dowodem globalnej optymalności. Ciężka ścieżka małej ściany/narożnika
kosztuje około 12 us/call wobec około 0,7 us dla certyfikowanej szerokiej ściany.

**Spłata:** dopiero po profilu produktu: analityczny/constrained optimizer albo
większy dowód i profiler wielu pojazdów. Nie blokuje `WHEEL-SOFT-03`.

## P2-3 — Headless validator jest splątany z zależnościami GUI

Główny `samples/CMakeLists.txt` inicjalizuje ImGui/NFD/GTK przed utworzeniem
`jozz_vehicle_validation`, mimo że target nie linkuje GUI. Na Linuxie wymusza to
GTK lub sieć nawet dla czystej walidacji headless; obecny pakiet został sprawdzony
świeżym minimalnym targetem z dokładnej listy `JOZZ_VEHICLE_CORE_FILES`.

**Spłata:** wydzielić headless validation przed GUI dependencies albo do osobnego
CMakeLists, z testem konfiguracji offline.

## P3-1 — Historyczne ograniczenia pojazdu

Sufit droop około 16°, sztywne bump-stopy, delikatny residual pull, niedokończony
rig kierowniczy oraz manualny odbiór mapy pozostają znanymi ograniczeniami JV.
Nie blokują obecnego badania koła ani clean-room transferu do JES.

**Spłata:** tylko gdy wrócą do aktywnego celu produktu; nie refaktoryzować przy
okazji pracy nad oponą.
