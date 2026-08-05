# TECH_DEBT — Jozz Vehicle

Data odświeżenia: 2026-08-06
Ten plik zawiera **wyłącznie otwarty dług**. Zamknięte i historyczne punkty są w
`archive/ledgers/TECH_DEBT_LEGACY_2026-07_PL.md` oraz w Git.

Skala: **P0** blokuje wiarygodność badań, **P1** blokuje następny etap produktu,
**P2** ważne, ale może poczekać, **P3** dziedzictwo JV bez wpływu na JES.

## P0-3 — Nieudowodniony fizyczny powrót kierownicy z pełnego skrętu

Ręczny przypadek jednoczesnego puszczenia gazu i kierownicy nie był objęty
istniejącą sondą P4. Reprodukcja i source localization są już zamknięte. W tym
rigu `b3Wheel` ma osobne przedziały przejścia: `+0,44/+0,45` oraz
`-0,41/-0,42`. Limit twist włącza się po rozpoczęciu trwałej wady. Podpis
momentu przechodzi przez dominująco obciążone koło i spin-joint, podczas gdy
rack i drążek przeciwdziałają dalszemu skrętowi. Normalno-impulsowy środek
kontaktu przesuwa się bardziej przed oś. To nadal lokalizacja, nie przyczyna ani
zaakceptowany baseline produktu.

**Spłata:** `B3WHEEL-STEER-01C`: probe-only fork at release i pojedyncze
interwencje wykluczające — limity twist, coast, load-friction racka, friction
kontaktu oraz no-contact. Dopiero po rozdzieleniu normalnego/stycznego kontaktu
i constraintów wolno projektować minimalną zmianę modelu. Zakazane są ukryte
sprężyny, serva do zera, moment zależny od puszczenia wejścia i globalne
obniżenie gripu tylko po to, by zazielenić test.

## P0-2 — Brak uczciwego bodźca drogowego dla podatności

Globalne `contactHertz` zmienia wszystkie kontakty świata. Warstwa runtime
`WHEEL-SOFT-03A-1` dodaje już lokalny override normalnej softness wyłącznie dla
`b3_wheelShape`, wspólny dla convex i mesh prepare paths; `0/0` zachowuje
precomputed world softness, a non-wheel shapes nie mogą wpływać na wybór.

Headless Q2 runner, immutable sweep i publikacja evidence są zamknięte. Wynik potwierdza monotoniczną compliance, ale decyzja jest `INCONCLUSIVE`, ponieważ bezpośredni load pulse nie reprezentuje przejazdu po drodze.

**Spłata:** zbudować `WHEEL-SOFT-03R` z nieruchomym statycznym bump mesh, hashowanym input trace i powtórzeniami w świeżych światach; kinematyczne podłoże jest zakazanym confoundem. Nie zmieniać jeszcze wartości domyślnych pojazdu.

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

## P2-1 — Pełny UBSan ma niezależne długi wyrównania i zero-size API

Domyślny build SIMD zatrzymuje się przed ścieżką koła na niealigned `_mm_load_sd`
w `src/mesh.c` / `src/simd.h`. Wcześniejszy przebieg bez tego punktu ujawnił też
misaligned `b3HullData*` store w `src/compound.c:582`. Pełny scalar suite zgłasza
również przekazanie null pointera do operacji o rozmiarze zero w ścieżce
`src/core.c:238` / `src/compound.c:273`. Ten sam problem odtwarza się już na
`241fe10`, więc nie jest regresją odzyskanej softness. Scalar `WheelShapeTest` i
headless Q2 przechodzą czysto pod UBSan.

**Spłata:** osobne minimalne reproduktory i jawny kontrakt wyrównania SIMD,
compound oraz operacji zero-size; nie mieszać z podatnością opony.

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

## P2-4 — `b3Wheel` jest niewidoczny w debug draw

Koło uczestniczy w fizyce, lecz bieżący Windows runtime nie pokazuje oczekiwanej
geometrii prymitywu po włączeniu debug shapes. Utrudnia to walidację aktywnej
reprezentacji i manifoldów; pochodzenie starej binarki nie było osadzone w UI.

**Spłata:** odtworzyć na buildzie z jawnym commit/tree/build type, prześledzić
adapter debug draw i dodać mały test bez zmiany zachowania kolizji.

## P3-1 — Historyczne ograniczenia pojazdu

Sufit droop około 16°, sztywne bump-stopy, delikatny residual pull, niedokończony
rig kierowniczy oraz manualny odbiór mapy pozostają znanymi ograniczeniami JV.
Nie blokują obecnego badania koła ani clean-room transferu do JES.

**Spłata:** tylko gdy wrócą do aktywnego celu produktu; nie refaktoryzować przy
okazji pracy nad oponą.
