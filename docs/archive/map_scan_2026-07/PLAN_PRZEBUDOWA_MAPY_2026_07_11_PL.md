> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# Plan: Mapa 2.0 — centralny kampus testowy i świat kaflowy

Data pierwotna: 2026-07-11. Fundamentalny rebase planu: 2026-07-12.
Właściciel kierunku: Jozz.

## 0. Status i źródło prawdy

- **Etap 1 jest zaakceptowany i zostaje.** Płyta 400×400 m, teren offroad
  400×400 m, szew pod płytą, seed/regeneracja, góra i węzły górskie są
  fundamentem, którego ten plan nie cofa.
- Implementacja Etapu 2 z commita `b8afab9` jest technicznie działająca, ale
  **odrzucona produktowo przez Jozza**. Jej kod pozostaje chwilowo w drzewie,
  żeby można było odzyskać wartościowy obstacle kit bez chaotycznego revertu.
- **Etap 2 jest ponownie otwarty. Etapy 3–6 są zablokowane do jego ręcznej
  akceptacji.**
- Ten dokument ustala architekturę całej mapy. Szczegóły wykonawcze są w
  `MAPA_ETAP_1..6_*.md`. Gdy dokumenty się różnią, wygrywa ten plan, a dla
  Etapu 1 — dodatkowo jego zapis wykonanej walidacji.

## 1. Dlaczego poprzedni plan Etapu 2 zawiódł

### 1.1 Fakty z aktualnego kodu i renderu

- Środkowy kafel ma granice około `x,z∈[-66.67,66.67]` i jako jedyny ma
  proceduralny techniczny grid. Po Etapie 2 pozostał prawie pusty.
- Sześć lane'ów umieszczono w `x∈[150,195], z∈[-60,60]`, czyli na skraju
  wschodniego kafla, tuż przed szwem offroadu — poza technicznym gridem i daleko
  od spawnu.
- Stara mapa miała skocznie, tarki i propy skupione w promieniu kilkudziesięciu
  metrów od `(0,0)`. Była prymitywna, ale miała czytelny środek ciężkości.
- Obecny poligon z lotu ptaka czyta się jak luźny katalog kolorowych brył, nie
  jak sześć przejezdnych lane'ów ani warsztat zawieszenia.
- Całe przeszkody pomalowano według trudności. Zielony/żółty/czerwony dominuje
  nad techniczną estetyką gridu i daje efekt zabawkowy.
- Plan obiecywał, że `jozz_vehicle_m5_test_course.cpp` zmaleje. W commicie
  Etapu 2 urósł o 196 linii netto, bo layout i receptury lane'ów trafiły do
  course'u zamiast do osobnego modułu danych.
- Walidacja udowodniła, że shape'y istnieją, kolidują i nie psują rigu. Nie
  sprawdziła najważniejszego: czy mapa ma dobry fokus, czy stanowiska tworzą
  spójne miejsce i czy Jozz chce po nim jeździć.

### 1.2 Błędy planistyczne

1. **Layout został zaprojektowany od listy ficzerów, nie od doświadczenia
   gracza.** Najpierw powstało „15 generatorów i 6 lane'ów”, dopiero potem
   próbowano znaleźć dla nich miejsce.
2. **Kafle nie były źródłem prawdy.** Roadmapa deklarowała 3×3 kafle jako
   przyszłe strefy, ale później użyła arbitralnych prostokątów przecinających
   ich granice.
3. **„Centrum ma być czyste” zinterpretowano jako „centrum ma być puste”.**
   Potrzebny jest czytelny rdzeń do spawnu i strojenia, otoczony aktywnością —
   nie martwa płyta.
4. **Progresja lane'ów zastąpiła projekt przestrzeni.** Długa tabela przeszkód
   nie daje tras wjazdu, zawracania, bezpiecznego wybiegu ani powodu, by wracać
   do środka.
5. **Bramka była za słaba.** Kolorowy top-down i dwa przejazdy nie są
   akceptacją layoutu. Brakowało porównania „przed / odrzucony / nowy” z tej
   samej kamery oraz obowiązkowego sign-offu Jozza.

## 2. Nowa doktryna mapy

### 2.1 Centralny kafel jest sercem produktu

Cały środkowy kafel `C` pozostaje jedną, nieprzerwaną powierzchnią
technicznego gridu. Nie nakładamy na niego asfaltowego „dywanu” ani kolorowych
podkładów stref. Grid ma być widoczny między stanowiskami i służyć jednocześnie
jako:

- skala metryczna do oceny auta i przeszkód;
- plac spawnu, strojenia i oglądania zawieszenia;
- centralny kampus krótkich, powtarzalnych prób;
- węzeł, z którego widać i wybiera się dalsze strefy mapy.

### 2.2 Hub-and-spoke, nie osiedle oddalonych prostokątów

Każda strefa satelitarna ma wjazd skierowany ku centrum, kotwicę teleportu przy
wejściu i czytelny powrót. Użytkownik zaczyna w kampusie, wybiera test,
wyjeżdża do satelity i wraca. Żadna strefa nie może wyglądać jak doklejona
osobna mapa.

### 2.3 Kafle są kontraktem layoutu

Strefy respektują granice dziewięciu kafli płyty. Wyjątkiem może być wyłącznie
trasa łącząca dwa kafle, jawnie opisana jako połączenie. Nie projektujemy już
stref typu `x=-190..140`, które przecinają kilka kafli bez semantyki.

### 2.4 Przeszkoda nie jest stanowiskiem

Generator bryły to tylko narzędzie. Pełne stanowisko musi mieć:

- stabilne ID i nazwę;
- cel testu oraz mierzalny sygnał;
- footprint/AABB, anchor wjazdu, kierunek i dozwolony kierunek przejazdu;
- zalecaną prędkość;
- minimalny najazd i wybieg;
- poziom trudności;
- materiał/kategorię kolizji;
- recepturę resetu i dowód wizualny.

### 2.5 Język wizualny

- powierzchnia środka: neutralny techniczny grid;
- przeszkody: stal/szarość/ziemia zależnie od funkcji;
- kolor trudności: mały akcent na bramce, krawędzi lub tabliczce — nie pełne
  nasycone bryły;
- tekst tylko blisko stanowiska; z daleka czytelność ma wynikać z sylwety,
  ustawienia i kierunku wjazdu;
- brak dekoracji, która utrudnia ocenę kontaktu koła z podłożem.

## 3. Docelowy układ 3×3

Granice kafli wynikają z `kPlateTileSize = 133.333...`. Poniższe role są
architekturą, nie poleceniem wypełnienia każdego metra.

```text
z+
+----------------------+----------------------+----------------------+
| NW: tor techniczny   | N: prosta / szybkie  | NE: łuki + łącznik   |
| i zakręty            | próby torowe         | do offroadu          |
+----------------------+----------------------+----------------------+
| W: drift / skid pad  | C: TECHNICAL GRID    | E: brama offroadu,   |
| wejście do C         | CENTRAL TEST CAMPUS  | rozbieg i wybieg      |
+----------------------+----------------------+----------------------+
| SW: plac fizyki      | S: spawner / stress  | SE: ciężkie lądowania|
| box3d                | w kontrolowanej niecce| i duże obiekty        |
+----------------------+----------------------+----------------------+
                                                      -> OFFROAD 400×400
```

Reguły:

- `C` jest gotowe w Etapie 2 i zawsze pozostaje dominującym wizualnie
  punktem płyty.
- `W/N/NW/NE` tworzą później układ torowo-driftowy otaczający kampus od
  zachodu i północy, zamiast odcinać go wielką ścianą toru.
- `E` pozostaje czytelną bramą do offroadu i strefą rozbiegu/wybiegu. Nie
  upychamy tam ponownie katalogu przeszkód.
- `SW/S/SE` mieszczą cięższe stanowiska i stress, ale wszystkie wjazdy są
  skierowane ku `C`.

## 4. Etap 1 — fundament zaakceptowany

Etap 1 pozostaje zamknięty. Wolno go tylko doszlifować, gdy nowy kampus ujawni
konkretny problem integracyjny:

- grid nie obejmuje całego shape'a środkowego kafla;
- szew kafli daje fizyczny lub widoczny próg;
- neutralne kafle konkurują wizualnie z centrum;
- teleport/spawn nie potrafi bezpiecznie osadzić auta na gridzie.

Każda taka poprawka jest mała, osobno walidowana i nie zmienia zaakceptowanego
generatora offroadu ani charakteru góry. Pełny kontrakt: Etap 1 §13.

## 5. Etap 2R — odzyskanie obstacle kitu i centralny kampus

Etap 2 nie buduje sześciu równoległych lane'ów. Buduje **jeden centralny
kampus z czterema stanowiskami/podstrefami wokół czystego rdzenia**:

- północ: komfort i rytm;
- zachód: artykulacja i przechył;
- wschód: teren punktowy i trakcja;
- południe: uderzenie i kontrolowany lot;
- rdzeń `24×24 m` przy `(0,0)`: spawn, strojenie, obrót auta, zero
  przeszkód;
- mała zatoka interakcyjna inspirowana pierwszą mapą: kilka świadomie
  ustawionych lekkich propów, bez losowego scatteru na osi jazdy.

Wartościowy `jozz_vehicle_obstacle_kit` z odrzuconej implementacji zostaje
zaudytowany i ponownie użyty. Usuwamy layout 6-lane, jego stałe przy
`x=150..195` i nasycone malowanie całych przeszkód. Szczegóły i bramki:
`MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md`.

## 6. Etap 3 — pierścień prowadzenia, tor i drift

Etap 3 najpierw buduje sam szkielet dróg na kaflach `W/NW/N/NE`, pokazany
Jozzowi przed dodaniem barier. Tor ma być osiągalny z centrum w kilkanaście
sekund, a jego początek i koniec czytelnie wracają do kampusu. Drift zajmuje
`W`, a nie arbitralny prostokąt przecinający SW.

Tor, stoper, nawierzchnie i bezpieczne bariery powstają dopiero po akceptacji
topologii. Duże skocznie wymagające długiego wybiegu trafiają do `E/SE`, nie
do ciasnego centrum. Szczegóły: `MAPA_ETAP_3_TOR_I_DRIFT_PL.md`.

## 7. Etap 4 — plac fizyki jako satelita

Plac box3d zajmuje `SW`. Stanowiska mają wspólną aleję wjazdową od centrum,
jasne strefy bezpieczeństwa i nie wylewają dynamicznych obiektów na kampus.
Najpierw powstają narzędzia: shaker, rolling road, most i see-saw. Zabawki
destrukcyjne są drugim podetapem i wymagają kompletnego resetu. Szczegóły:
`MAPA_ETAP_4_PLAC_FIZYKI_PL.md`.

## 8. Etap 5 — spawner i stress poza centrum

Domyślny punkt dużych spawnów to kafel `S`, a ciężkich obiektów `SE`.
Przycisk „przed pojazdem” ma twardy limit małej partii; nie wolno przypadkowo
zasypać centralnego kampusu 250 obiektami. Szczegóły:
`MAPA_ETAP_5_SPAWNER_I_STRESS_PL.md`.

## 9. Etap 6 — nawigacja, telemetria i finalny sign-off

Nawigacja opisuje relacje względem centrum („północny tor”, „zachodni drift”),
nie listę odległych punktów. Teleporty lądują na bramach skierowanych do
stanowiska. Finalna suita zawsze zawiera ten sam kadr całej płyty i ten sam kadr
centralnego kafla, żeby regresja fokusu była widoczna. Szczegóły:
`MAPA_ETAP_6_NAWIGACJA_POMIAR_POLISH_PL.md`.

## 10. Kolejność i bramki decyzji

| Etap | Stan | Warunek wejścia | Warunek zamknięcia |
|---|---|---|---|
| 1 | **zaakceptowany** | — | utrzymujemy istniejące dowody |
| 2R | **otwarty** | aktualny kod + render „odrzucony” | testy + nowy kampus + ręczny sign-off Jozza |
| 3 | zablokowany | 2R zaakceptowany | akceptacja szkieletu, potem pełna jazda |
| 4 | zablokowany | 2R zaakceptowany | narzędzia, reset, strefy bezpieczeństwa |
| 5 | zablokowany | 4 ma PropRegistry | tabela stress i kontrola sprzątania |
| 6 | zablokowany | 2R–5 zamknięte | finalna suita + sign-off „Mapa 2.0” |

Każdy etap ma dwa rodzaje bramki:

1. **techniczna:** build, walidator, testy, boot M5/M6, kategorie kolizji,
   brak wycieków i liczby;
2. **produktowa:** render z ustalonej kamery, realny przejazd i — gdy plan tak
   mówi — akceptacja Jozza.

Zielona bramka techniczna nie może zastąpić odrzuconej bramki produktowej.

## 11. Kontrakty architektoniczne

- box3d core `src/` i `include/` bez zmian;
- `world_layout.h`: granice kafli, kotwice, identyfikatory i dane layoutu,
  bez budowania świata;
- `obstacle_kit`: geometria pojedynczych przeszkód, bez wiedzy o kampusie;
- osobny moduł `jozz_vehicle_central_test_campus.{h,cpp}`: receptury,
  placement i etykiety Etapu 2R;
- `m5_test_course`: orkiestracja course'u i legacy props; ma być cienki;
- każdy surface jezdny ma kategorię terenu; propy pozostają `0x1`;
- layout ma walidator footprintów: granice kafla, overlap, spawn exclusion,
  najazd i wybieg;
- identyczne moduły świata są używane przez M5 i M6.

## 12. Minimalny zestaw dowodów dla zmian mapy

- zrzut z góry całej płyty z ustalonej kamery;
- zrzut z góry środkowego kafla z ustalonej kamery;
- ujęcie z wysokości kierowcy przy każdym nowym stanowisku;
- porównanie tego samego kadru przed zmianą i po zmianie;
- mapa footprintów/liczby z walidatora;
- przynajmniej jeden przejazd w obie strony tam, gdzie stanowisko jest
  dwukierunkowe;
- CHECKPOINT rozdzielający wynik techniczny od ręcznej akceptacji.

Rendery scratchpad bez zachowanego polecenia/kamery nie wystarczają do
zamknięcia etapu.

## 13. Świadomie poza tym trackiem

- streaming/LOD terenu;
- minimapa 2D;
- pogoda i dzień/noc;
- import heightmapy PNG;
- AI, duchy i przeciwnicy;
- zapis dynamicznego stanu świata w presetach pojazdu;
- zmiany solvera lub core box3d.
