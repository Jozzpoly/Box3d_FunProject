# Mapa — Etap 2R: odzyskanie centralnego kampusu

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.
Status: **RECOVERY_REQUIRED — NIEZAAKCEPTOWANY**.
Następny dozwolony strumień: R0/R1, potem dokończenie R2.

## 1. Decyzja po audycie 2026-07-13

Kierunek centralnego kampusu jest dobry, lecz obecna implementacja nie spełnia
własnej definicji ukończenia. Nie istnieje udokumentowana akceptacja skeletonu
E2R.3 ani finalny odbiór E2R.5. Rozpoczęcie E3 nie zmieniło tego statusu.

Do aktywnego baseline'u można odzyskać:

- pełny techniczny grid kafla C;
- czysty Central Core 24×24 m;
- modularny podział data/builder/visual;
- trzy role terenowych wysp E jako kandydaty;
- niskie bumper banks jako kandydaty;
- deterministyczne specs i cienki course;
- tile/yard registry.

Nie są zaakceptowane:

- quota 401 kamieni i 147 bumperów jako miara jakości;
- dawna W-articulation/off-camber — została odrzucona wizualnie i jest
  nieaktywna;
- niezbadane rampy/ruts/stairs/berm z obstacle kitu;
- stały blueprint overlay;
- brakująca zatoka propów, niepełne S/W i nieudowodniona pętla obwodowa.

Szczegóły problemów i dowody: `AUDYT_REALIZACJI_MAPY_2026_07_13_PL.md`.

## 2. Cel produktu

Zbudować na całym kaflu C skupiony techniczny kampus, który:

- zachowuje grid jako dominującą powierzchnię;
- przypomina energią pierwszą, skupioną mapę, ale ma profesjonalny layout;
- pozwala w kilka sekund wybrać krótką próbę i wrócić do core;
- nie wygląda jak katalog drobnych brył;
- jest użyteczny jednocześnie dla prostego auta M5 i rigu M6;
- oddziela debug layoutu od finalnego obrazu mapy.

## 3. Kontrakt przestrzeni

### 3.1 Kafel i rdzeń

- nominalny C: `x,z∈[-66,667;66,667]`;
- używalny layout: `x,z∈[-60;60]`;
- Central Core: `x,z∈[-12;12]`;
- zero statycznych przeszkód i domyślnego scatteru w core;
- cztery spokes od core: minimum 10 m rzeczywistej wolnej szerokości;
- obwodowy pusty korytarz: minimum 8 m rzeczywistej wolnej szerokości;
- minimum 55% używalnego rzutu `120×120 m` pozostaje widocznym,
  nieprzykrytym gridem;
- żadna bryła E2R nie wykracza poza margines techniczny C.

Metryka 55% używa pola sumy mnogościowej (bez podwójnego liczenia overlapów)
rzutów realnych AABB shape'ów, przyciętych do `[-60;60]²`; jest konserwatywnym
przybliżeniem widocznego gridu. Mianownik to 14 400 m². Core, spokes i loop są
dodatkowymi warunkami ciągłości wolnej przestrzeni, nie powierzchnią dodawaną
drugi raz do wyniku.

### 3.2 Role, nie obowiązkowe wypełnienie

| Rejon | Pierwsza rola | Status bieżący | Decyzja |
|---|---|---|---|
| N | komfort i rytm | wiele bumper banks | uprościć i odebrać jedną recepturę |
| E | teren punktowy/trakcja | 3 gęste wyspy | zachować role, przeprojektować footprint i linie |
| S | lekki impact/lot | brak pełnej funkcji | po naprawie kontraktu ramp |
| W | wolny test/artykulacja | zły slice wycofany | nie reaktywować; nowa koncepcja albo wolny korytarz |
| NW pocket | interakcja | brak propów | dodać 6–8 lekkich, resetowalnych propów |

Podstrefa może pozostać pusta, jeśli lepsza geometria nie przeszła odbioru.
Symetria czterech „wypełnionych ćwiartek” nie jest celem.

## 4. Model danych i jedno źródło prawdy

Każda stacja ma stabilne ID i co najmniej:

```cpp
struct JozzTestStationSpec
{
    JozzStationId id;
    JozzTileId tile;
    b3Vec2 centerXZ;
    float yawDegrees;
    b3Vec2 footprintHalfExtents;
    float approachLength;
    float runoffLength;
    float recommendedSpeedMin;
    float recommendedSpeedMax;
    bool bidirectional;
};
```

Dokładny typ może się zmienić, ale kontrakt nie. Receptury contentu muszą
odwoływać się do ID stacji, a validator ma porównać faktycznie zbudowane AABB z
jej footprintem. Osobna tablica, której builder nie czyta, nie jest źródłem
prawdy.

## 5. Walidator E2R

Walidator failuje, gdy:

- builder tworzy shape poza przypisaną stacją albo poza C;
- realny shape przecina core, spoke lub obwodowy loop;
- anchor leży w shape'ie albo nie ma bezpiecznego spawn footprintu auta;
- approach/runoff jest za krótki dla deklarowanej prędkości;
- dwukierunkowa stacja nie ma dwóch bezpiecznych wyjść;
- rzeczywista kategoria shape'a jest niezgodna z rolą;
- liczba body/shape przekracza jawny limit;
- obstacle top-surface ma nieplanowany lip/step;
- M5 i M6 dostają różne aktywne elementy kampusu;
- debug overlay wpływa na fizykę lub jest domyślnie włączony w product view.

Raport drukuje per stacja: ID, real bounds, approach, runoff, bodies, shapes,
occupied ratio, najwęższą linię i maksymalny step.

Kategoria dyskretnej skały nie jest jeszcze rozstrzygnięta. Przed pierwszą
wyspą osobny probe porównuje co najmniej: całość jako terrain, całość jako
object oraz rozdzielony top/side. Mierzy kontakty rolling sphere i sidewall M6,
najazd bokiem oraz zachowanie M5. Do wyniku tego probe'a validator nie może
uznać żadnej z tych kategorii za „zgodną z rolą”. Status: **ADAPT_PENDING_PROBE**.

## 6. Obstacle kit — ponowny audyt

Żaden generator nie jest `KEEP` tylko dlatego, że kompiluje się. Tabela musi
mieć `KEEP / FIX / QUARANTINE / DEFER` i dowód profilu.

Minimalne próby generatora:

1. top-surface w punktach entry/25%/50%/75%/exit;
2. zgodność deklarowanego rise/drop z wynikiem;
3. widok z wysokości koła;
4. najazd prosto i pod kątem;
5. przejazd przy dolnej i górnej deklarowanej prędkości;
6. kategoria kontaktu w M5 i M6;
7. brak ukrycia pod ciągłą płytą;
8. brak dodatkowego progu wynikającego z grubości slabu.

Do chwili przejścia tych prób **wszystkie generatory są QUARANTINE**, poza
jawnie wymienionymi kandydatami `AddBumperBank` i `AddRockIsland`. Te dwa mają
status **FIX/CANDIDATE**, nie `KEEP` ani `ACCEPTED`. Obejmuje to także pomijane
wcześniej `AddStepUp`, `AddStepDown`, `AddWhoops`, `AddSpeedBump`,
`AddWashboard`, `AddRockGarden` i `AddLogs`; brak nazwy na starej liście nie
jest zgodą na użycie.

## 7. Kolejność recovery i ukończenia

### E2R-R0 — zachowanie bieżącego WIP (`WP-00`, `WP-01`)

- pełny inventory i diff;
- snapshot/gałąź wybrana przez Jozza;
- manifest obecnych renderów i wyników;
- zero zmian geometrii.

### E2R-R1 — baseline aktywnego świata (`WP-02`, `WP-03`)

- E3 fizycznie wyłączony;
- overlay pod toggle, domyślnie off;
- E1 niezmieniony;
- bieżący E2R widoczny z czterech stałych kamer;
- checkpoint `BASELINE_RECOVERED`.

### E2R-R2 — validator realnego buildera (`WP-GATE-A`, `WP-GATE-B`)

- shape manifest;
- core/spokes/loop/anchor/category checks;
- maksymalny budżet shape'ów;
- test M5+M6 w gate.

### E2R-C1 — N: jedna receptura komfortu (`WP-C2-N-COMFORT`)

- jedna krótka sekcja, nie 13 banków naraz;
- jawna wysokość, odstęp i prędkość;
- test obu kierunków;
- obraz top/driver i odbiór przed kolejną recepturą.

### E2R-C2 — E: jedna wyspa terenowa (`WP-C3-E-CATEGORY`, `WP-C3-E-ISLAND`)

- 2–3 czytelne linie jazdy; każda ma minimalną wolną szerokość równą większej
  z obwiedni M5/M6 przy skręcie ±10° plus 0,5 m marginesu z każdej strony;
- wolny bypass;
- brak pojedynczych kamieni niewidocznych z fotela;
- próby prosto, bokiem i po skosie;
- brak zakleszczenia envelope'u M6.

### E2R-C3 — zatoka interakcyjna (`WP-C4-PROPS`)

- 6–8 propów kategorii `0x1`;
- poza core, spokes i loop;
- pełny reset i stały body count po 10 cyklach;
- nie rozsypywać propów na osie jazdy.

### E2R-C4 — kolejne małe stanowisko (`WP-C5-CHOICE` + wybrany WP)

Wybór po ocenie C1–C3. Nie zakładać z góry, że musi to być W albo rampa. Każdy
nowy typ kontaktu ma osobny WP i STOP.

### E2R-C5 — integracja kampusu (`WP-C6-INTEGRATION`)

- pełny obwodowy przejazd bez cofania;
- z core widoczne co najmniej trzy czytelne wejścia;
- wszystkie aktywne stacje osiągalne w mniej niż 60 m;
- brak regresji gridu i centralnego fokusu;
- ręczna jazda Jozza.

## 8. Stałe dowody

Każdy kandydat zachowuje:

- `plate_top` — cała płyta;
- `center_top` — cały C;
- `center_3q` — sylweta przeszkód;
- `core_driver` — czytelność wejść z core;
- `station_<id>_driver`;
- obraz lub log przejazdu referencyjnego;
- manifest komendy, kamera, seed, frame count, hash.

Porównanie obowiązkowe:

1. skupiona pierwsza mapa;
2. odrzucony far-east 6-lane;
3. odzyskany baseline E2R;
4. finalny kandydat E2R.

## 9. Definition of Done

E2R jest gotowy wyłącznie, gdy:

- pełny gate i osobny smoke M5+M6 są zielone;
- validator mierzy realny builder;
- nie ma P0/P1 z audytu przypisanego do centralnego kafla;
- przynajmniej N, E i zatoka propów mają osobne dowody;
- core, spokes i obwodowy loop są przejezdne;
- produktowy overlay jest domyślnie wyłączony;
- Jozz przejechał kampus i wpisał `E2R ACCEPTED BY JOZZ` wraz z hashem.

Build, quota shape'ów, 300 klatek na anchorze albo ładny top-down nie zamykają
etapu osobno ani łącznie bez ręcznego sign-offu.
