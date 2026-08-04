# TECH_DEBT — Jozz Vehicle

Data odświeżenia: 2026-08-04
Ten plik zawiera **wyłącznie otwarty dług**. Zamknięte i historyczne punkty są w
`archive/ledgers/TECH_DEBT_LEGACY_2026-07_PL.md` oraz w Git.

Skala: **P0** blokuje wiarygodność badań, **P1** blokuje następny etap produktu,
**P2** ważne, ale może poczekać, **P3** dziedzictwo JV bez wpływu na JES.

## P0-2 — Brak czystego eksperymentu podatności

Globalne `contactHertz` zmienia wszystkie kontakty świata. Nie istnieje lokalny
override wheel–ground, więc „miękka opona” miesza się z miękkością całej sceny.

**Spłata:** per-wheel lub per-material normal softness, wybierana w obu ścieżkach
prepare contact; A/B przy identycznym manifoldzie.

## P1-1 — Wheel–hull nie jest pełnym kontaktem convex–convex

Wybór ściany i manifold płaszczyzny nie ograniczają wszystkich punktów do
wielokąta ściany i nie pokrywają kompletu osi rozdzielających. Ryzyko fałszywych
kontaktów przy krawędziach i narożnikach.

**Spłata:** clipping do face polygon + testy edge/axis po ustabilizowaniu mesha.

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

## P2-1 — Pełny UBSan zatrzymuje się w `compound.c`

Pełny zestaw testów pod UBSan zatrzymuje się w `src/compound.c:582` na
misaligned `b3HullData*` store. Ten sam błąd został odtworzony na czystym
checkpointcie `8472512`; `WheelShapeTest` przechodzi pod UBSan.

**Spłata:** osobny minimalny reproduktor i poprawka layoutu/alokacji compound,
bez mieszania jej z geometrią koła.

## P3-1 — Historyczne ograniczenia pojazdu

Sufit droop około 16°, sztywne bump-stopy, delikatny residual pull, niedokończony
rig kierowniczy oraz manualny odbiór mapy pozostają znanymi ograniczeniami JV.
Nie blokują obecnego badania koła ani clean-room transferu do JES.

**Spłata:** tylko gdy wrócą do aktywnego celu produktu; nie refaktoryzować przy
okazji pracy nad oponą.
