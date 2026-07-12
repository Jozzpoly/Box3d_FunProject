# Mapa — Etap 5: spawner i kontrolowany stress-yard

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.
Status: **ZABLOKOWANY; wymaga PropRegistry z Etapu 4.**

## 1. Cel

Dać użytkownikowi szeroki zakres obiektów i powtarzalny stress-test, nie
zamieniając centralnego kampusu w śmietnik. Duże partie powstają na kaflu
`S`, a największe/ciężkie obiekty na `SE`.

## 2. Bezpieczne strefy spawnu

| Punkt | Limit domyślny | Zastosowanie |
|---|---:|---|
| S — stress yard | 250 na klik | siatki, deszcz, stosy, gruz |
| SE — heavy yard | 10 na klik; kontener 1 | głazy, stal, kontenery, duże kolizje |
| Plac fizyki SW | 50 | scenariusze z PropRegistry |
| 10 m przed pojazdem | 10 małych / 1 duży | szybka interakcja |
| Central Core | **0 dużych partii** | tylko jawny tryb debug, nie preset |

Spawner przed pojazdem sprawdza, na jakim kaflu stoi auto, wolny footprint,
podłoże i dystans od bramy/stanowiska. Nie pozwala zespawnować batcha wewnątrz
statycznej przeszkody ani na wybiegu skoczni.

## 3. Model danych

`SpawnDef`:

- shape: box, sphere, capsule, plank, wedge, slab, MIX;
- preset skali: drobnica, gruz, skrzynki, głazy, kontener;
- materiał: lekki, drewno, guma, beton, stal;
- ilość i pattern: single, grid, rain, stack, pyramid, wall;
- target zone/anchor;
- seed;
- safety class: small, heavy, hazardous;
- batch ID i ownership w PropRegistry.

Ten sam seed, definicja i anchor muszą dać identyczny układ.

## 4. UI

Nowa zakładka `Spawner###TabSpawner` po Świat, przed Debug:

1. **Co:** shape, skala, materiał.
2. **Gdzie:** bezpieczna strefa z opisem; „przed pojazdem” pokazuje wyliczony
   limit.
3. **Jak:** pattern, ilość, seed.
4. **Podgląd ryzyka:** przewidywana liczba body, masa największego obiektu,
   informacja czy strefa jest wolna.
5. **Akcja:** Spawnuj.
6. **Zarządzanie:** usuń ostatnią partię, usuń wszystkie zespawnowane, uśpij,
   resetuj seed.
7. **Stress:** scenariusze z ustalonym protokołem, nie przycisk „spam 250”.

Presety pojazdu nie zapisują świata. Restart świata czyści batch registry.

## 5. Zasady fizyczne

- propy: `categoryBits=0x1`;
- sleep domyślnie włączony;
- initial overlap jest błędem walidatora spawn placement;
- rain ma odstęp pionowy minimum 1.2× maksymalnego wymiaru;
- podłoże jest próbkowane z płyty/heightfielda;
- drobnica przy CCD off jest stress-zabawką, nie modelem żwiru pod oponą;
- kontener i stalowe głazy mają twardy limit ilości oraz minimalny dystans od
  chassis;
- usuwanie batcha weryfikuje counters po zniszczeniu body.

## 6. Protokół stress

Scenariusz bazowy: box 0.4 m, drewno, grid na S, partie po 250. Po każdej:
5 s settle, średnia i p95 step time z ostatniej sekundy, liczba awake/sleeping.
Stop przy p95 > 8 ms albo 2500 ciał.

| Ciała zespawnowane | mean ms | p95 ms | awake/sleep | fps | uwagi |
|---:|---:|---:|---:|---:|---|
| 250 | — | — | — | — | — |
| 500 | — | — | — | — | — |
| 1000 | — | — | — | — | — |
| 1500 | — | — | — | — | — |
| 2000 | — | — | — | — | — |
| 2500 | — | — | — | — | — |

Drugi przebieg: auto wjeżdża w stos 1000 obiektów. Trzeci: MIX na SE z limitem
250. Tooltip podaje miękki limit na podstawie p95, nie najlepszego pojedynczego
pomiaru.

## 7. Bramka

- pełny gate M5/M6;
- ten sam seed = identyczne transformy;
- spawn/delete ostatniej partii przywraca dokładne counters;
- „usuń wszystkie” nie narusza stałych propów mapy ani PropRegistry Etapu 4;
- test blokady: próba 250 obiektów w Central Core jest odrzucona;
- test overlap/exclusion przy stanowisku i wybiegu;
- tabela stress wypełniona;
- rendery: S z 500 obiektami, SE z kontenerem/głazami, kadr całej płyty
  potwierdzający, że centrum pozostało czyste;
- ręczny sanity drive przez stress yard.

## 8. Ryzyka

- jeden globalny registry może pomylić stałe scenariusze i batch spawnera —
  ownership/type jest obowiązkowy;
- średnia ukrywa spike solvera — raportujemy p95;
- wygodny „spawn przed autem” może ominąć architekturę stref — twarde limity,
  exclusion zones i komunikat w UI;
- ciężkie obiekty mogą przekroczyć granicę S/SE — containment i limit prędkości
  początkowej.
