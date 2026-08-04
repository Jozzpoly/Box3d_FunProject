> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# Mapa — Etap 4: satelitarny plac fizyki box3d

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.
Status: **ZABLOKOWANY do akceptacji Etapu 2R.**

## 1. Cel

Kafel `SW` staje się placem narzędzi fizycznych i kontrolowanej destrukcji.
Nie jest drugim centrum mapy: ma jedną aleję wjazdową skierowaną do kampusu,
wyraźne stanowiska oraz granicę zatrzymującą dynamiczne obiekty przed
rozsypaniem się na pozostałe kafle.

## 2. Zakres kafla

- nominalny tile SW:
  `x∈[-200,-66.667], z∈[-200,-66.667]`;
- margines 8 m od granic;
- brama w narożniku skierowanym ku C;
- główna aleja 10 m szerokości;
- stanowiska ustawione frontem do alei, nie losowo co 25 m;
- niski obwodowy containment dla propów, ale bez twardej bariery na osi wjazdu.

## 3. Podetap A — narzędzia zawieszenia

Najpierw powstają stanowiska, które realnie pomagają stroić auto:

| Stanowisko | Realizacja | Wymagany kontrakt |
|---|---|---|
| Shaker 4-post | cztery niezależne kinematyczne płyty lub jawnie uproszczona wspólna płyta; `SetTargetTransform` | tryb sinus/sweep, bez teleportowania w kroku |
| Rolling road | statyczna taśma z `tangentVelocity` | bezpieczne boczne ograniczniki i wyłącznik |
| Most z desek | deski na revolute, wjazd/wyjazd | udźwig auta z zapasem, reset |
| See-saw | deska na revolute z limitami | brak zakleszczenia pod podwoziem |
| Obrotnica | jedno ciało kinematyczne, wiele shape'ów | prędkość ograniczona, łagodna rampa |

Każde stanowisko ma station spec analogiczny do Etapu 2R: footprint, anchor,
zalecana prędkość, exclusion zone i reset.

**STOP po Podetapie A:** jazda i ocena użyteczności przez Jozza. Nie dokładamy
zabawek, jeśli narzędzia nie są dobrym placem testowym.

## 4. Podetap B — interakcja i destrukcja

- wrecking ball;
- piramida skrzynek;
- ściana pustaków;
- domino;
- kręgle;
- eksplozja `b3World_Explode`.

Zasady:

- wszystkie dynamiczne obiekty są w wspólnym `PropRegistry`;
- każdy scenariusz ma reset do identycznego seeda/transformu;
- eksplozja ma osobny uzbrojony stan i nie działa w promieniu bezpieczeństwa
  od chassis ani w stronę centralnego kampusu;
- containment jest częścią testu: po scenariuszu żaden prop nie powinien
  wjechać samoczynnie na C/W/S;
- obiekty ciężkie mają osobną zatokę od delikatnych stanowisk.

## 5. Architektura

- `jozz_vehicle_physics_playground.{h,cpp}`: budowa, update, reset i specs;
- `jozz_vehicle_prop_registry.{h,cpp}`: wspólny rejestr bodyId, spawn
  transform, batch/scenario i bezpieczne niszczenie;
- UI w zakładce Świat: najpierw wybór stanowiska/teleport, potem sterowanie
  tylko aktywnym stanowiskiem;
- brak globalnych suwaków działających na kilka stanowisk naraz;
- kinematic update przez prędkość/target, nie `SetTransform` w pętli;
- powierzchnie stanowisk = kategoria terenu; propy = `0x1`.

## 6. Bramka

### Techniczna

- pełny gate M5/M6;
- shaker przy 0.5, 2 i 8 Hz; liczby kontaktu i travel są wiarygodne;
- rolling road 15 m/s działa przy aucie na hamulcu;
- most przejezdny w obie strony i resetowalny;
- reset registry przywraca dokładną liczbę body/shape;
- eksplozja respektuje bezpiecznik;
- walidator footprintów i containment przechodzi.

### Wizualna/produktowa

- widok z Central Core pokazuje tylko czytelną bramę SW, nie ścianę propów;
- top-down tile'a pokazuje aleję i oddzielne zatoki;
- ujęcia: shaker w dwóch fazach, auto na moście, rolling road, destrukcja
  przed/po;
- ręczna ocena Jozza po Podetapie A i przed zamknięciem całego etapu.

## 7. Ryzyka

- „plac zabaw” może wygrać z narzędziem testowym — dlatego twarda kolejność
  A→sign-off→B;
- obiekty mogą wyciekać poza SW — containment i scenariusz eksplozji są
  walidowane;
- most może być efektowny, ale bezużyteczny — bramka wymaga dwóch kierunków i
  powtarzalnego resetu;
- rozrost UI — sterowanie kontekstowe tylko po wyborze stanowiska.
