# Mapa — Etap 3: pierścień prowadzenia, tor, drift i duże lądowania

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.
Status: **ZABLOKOWANY do akceptacji Etapu 2R.**

## 1. Cel

Rozwinąć centralny kampus w większy system jazdy bez utraty fokusu. Tor i drift
mają otaczać centrum od zachodu i północy, mieć wejścia skierowane do kampusu i
wracać do niego. Etap przejmuje też duże przeszkody powietrzne, które nie
mieszczą się bezpiecznie na środkowym kaflu.

## 2. Kafle i role

| Kafel | Rola | Relacja z centrum |
|---|---|---|
| W | drift, skid pad, ósemka | brama zachodnia z kampusu |
| NW | wolne zakręty techniczne, szykana | łączy W z N |
| N | prosta główna, start/meta, Vmax | widoczna z bramy północnej |
| NE | hairpin/banked corner, łącznik E | zamyka pętlę |
| E | brama offroadu, rozbieg i neutralny wybieg | przedłużenie osi wschodniej |
| SE | duży tabletop/gap jump i strefa lądowania | wjazd z E, powrót do S/C |

Żadna z tych stref nie wchodzi na centralny tile poza płaską, niekolizyjną
bramą/oznaczeniem na jego krawędzi.

## 3. Kolejność — najpierw topologia

### E3.0 — plan linii i footprintów

- dane centerline toru, promienie, szerokość i run-off;
- footprint driftu i strefy dużych lądowań;
- walidator granic kafli oraz przecięć z kampusem;
- zero barier i dekoracji.

### E3.1 — skeleton

- neutralne znaczniki krawędzi i linia centerline;
- przejezdna pętla o szerokości 10–12 m;
- bramy W/N/E widoczne z centrum;
- render top-down całej płyty i widok z Central Core.

**STOP: Jozz akceptuje topologię przed krawężnikami, oponami i sensorami.**

### E3.2 — nawierzchnie i bezpieczeństwo

- krawężniki tylko tam, gdzie mają funkcję;
- zewnętrzne bariery z bezpiecznym offsetem;
- strefy tarcia drift/lód;
- run-off bez twardej ściany na osi możliwego wypadnięcia.

### E3.3 — pomiar i duże przeszkody

- lap timer na sensorach;
- splity;
- duży tabletop i gap jump w E/SE z realnym rozbiegiem i lądowaniem;
- telemetria airtime może być przygotowana, ale finalnie domyka ją Etap 6.

## 4. Kontrakt toru

- pętla zamknięta, ale nie musi być symetryczna;
- szerokość 10–12 m;
- prosta N minimum 220 m używalnego rozbiegu;
- co najmniej: hairpin, łuk szybki, szykana i łuk o stałym promieniu;
- wejście startowe z bramy N nie przecina linii pomiarowej w złym kierunku;
- powrót do centrum nie wymaga jazdy pod prąd;
- centerline i bariery są danymi osobnymi: zmiana linii nie wymaga edycji
  dziesiątek wywołań `Add*`;
- minimalny run-off na zewnętrznych szybkich łukach: 12 m lub jawnie
  uzasadniona bariera energochłonna.

## 5. Drift na kaflu W

- skid pad Ø40–50 m;
- ósemka z dwóch stycznych okręgów;
- płaska strefa o obniżonym tarciu, bez fizycznego progu na granicy materiału;
- wejście od wschodu, czyli od kampusu;
- strefa nie zajmuje SW — ten kafel pozostaje dla placu fizyki;
- pachołki dynamiczne i resetowalne, ale nie tworzą ściany śmieci widocznej z C.

## 6. Duże lądowania E/SE

Z obstacle kitu można użyć `AddTabletop`, `AddGapJump`, `AddKicker` i
`AddWedgeRamp`, ale pełna stacja ma osobny spec:

- prosty rozbieg minimum 45 m;
- marker zalecanej prędkości;
- żadnej przeszkody w strefie awaryjnego hamowania;
- lądowanie o długości minimum 35 m;
- boczny powrót do centrum/offroadu;
- wariant łatwy tabletop przed twardym gap jump;
- oba kierunki jazdy tylko jeśli geometria i run-off to wspierają.

## 7. Elementy techniczne

- `jozz_vehicle_track_layout.{h,cpp}`: centerline, footprinty, generowanie
  krawędzi i bram;
- `jozz_vehicle_lap_timer.{h,cpp}`: sensory i stan stopera;
- obstacle kit pozostaje biblioteką geometrii;
- materiały per sub-shape, bez dzielenia całego bazowego kafla na nachodzące
  płyty; jeśli potrzebna jest strefa tarcia, podział tile'a zachowuje top y=0;
- sensor liczy chassis, ignoruje koła i propy;
- teleport/reset zeruje bieżące okrążenie, nie fałszuje best lap.

## 8. Bramka

### Techniczna

- pełny gate M5/M6;
- walidator centerline, granic tile'ów, szerokości i run-off;
- kategorie: nawierzchnia/krawężnik/banka = teren, propy/bariera = `0x1`;
- przejazd po wszystkich szwach bez uskoku;
- dwa pełne okrążenia, splity i debounce działają;
- duży skok nie kończy się poza płytą ani rozpadem rigu.

### Produktowa

- ten sam top-down całej płyty przed i po skeletonie;
- kadr z Central Core pokazuje czytelne bramy W/N/E, ale centrum nadal
  dominuje;
- osobne widoki z maski drogi: wejście na tor, szykana, hairpin, powrót;
- drift czyta się jako jeden plac, nie zbiór pachołków;
- **akceptacja skeletonu przez Jozza przed E3.2** i akceptacja finalnej jazdy
  przed zamknięciem Etapu 3.

## 9. Ryzyka

- tor może wizualnie otoczyć i „udusić” centrum — bramy oraz bariery zaczynają
  się dopiero za granicą C, a widok z rdzenia jest bramką;
- długi prosty shape może przeciąć granice tile'ów — centerline może je
  przekraczać, lecz materiały i shape'y muszą mieć jawny podział na szwie;
- gap jump kusi do ustawienia na osi offroadu — zachować boczny, bezkolizyjny
  wjazd na heightfield;
- stoper przy driftowaniu przez bramkę — kierunek i filtr chassis są
  obowiązkowe.
