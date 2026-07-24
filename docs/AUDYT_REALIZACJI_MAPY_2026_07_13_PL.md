# Audyt realizacji mapy — 2026-07-13

Status: **WERDYKT WYDANY; DALSZA GEOMETRIA WSTRZYMANA**

> **AKTUALIZACJA 2026-07-24 — werdykt zrealizowany.** E3 odłączony od aktywnego świata (pliki
> toru uśpione na dysku, niepodpięte do builda), E2R central campus odzyskany i zintegrowany ze
> skanem, stan scommitowany na `main`. WP-00 zamknięty decyzją Jozza. Ten audyt pozostaje jako
> **historyczny** zapis dowodów i diagnozy — nie opisuje bieżącego stanu. Rozwiązanie i weryfikacja:
> `ODZYSK_UTRACONYCH_ZMIAN_2026_07_24_PL.md`.
Zakres: dirty worktree po `445db88`, wykonanie E2R i eksperymentalnego E3.
Dokumenty nadrzędne po tym audycie:

1. `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md` — stan i roadmapa;
2. `PLAN_WYKONAWCZY_MAPA_GPT_LUNA_PL.md` — instrukcja wykonawcza;
3. ten audyt — dowody, diagnoza i uzasadnienie decyzji.

## 1. Werdykt bez łagodzenia

Etap 1 pozostaje dobrym i zaakceptowanym fundamentem. Kierunek E2R — pełny
techniczny kafel C, centralny kampus i cienka orkiestracja — jest właściwy, ale
obecna implementacja **nie zamyka E2R**. Jest kandydatem wymagającym odzyskania,
nie zaakceptowanym baseline'em.

Obecny E3 jest **odrzuconym eksperymentem**. Nie wolno go dalej dekorować ani
rozbudowywać. Najpierw trzeba odłączyć jego fizykę od aktywnego świata, zachować
kod jako materiał badawczy i wrócić do E2R. Przyczyna nie jest kosmetyczna:

- trzy prawie współliniowe warianty toru są budowane jednocześnie i fizycznie
  nachodzą na siebie;
- aktywna receptura ramp wytwarza uskoki inne niż deklarowany profil;
- E3 rozpoczęto mimo dwóch niezaliczonych bramek ręcznej akceptacji;
- zielone testy sprawdzają głównie tabele danych, nie rzeczywistą geometrię.

To oznacza: **gate techniczny jest zielony, lecz gate produktu i wiarygodności
walidacji jest czerwony**.

## 2. Co zostało sprawdzone

### 2.1 Repozytorium i przebieg prac

- `HEAD` i `origin` nadal wskazują `445db88`;
- od tego punktu nie ma commitów rozdzielających E2R od E3;
- wejściowy snapshot implementacji (przed dopisaniem tego audytu) mieszał 13
  zmodyfikowanych plików śledzonych i 12 nowych plików źródłowych E2R/E3;
- dokładny aktualny licznik jest zmienny podczas przebudowy dokumentacji i ma
  zostać zapisany przez WP-00; istotny fakt to brak commitów/rollbacku per slice;
- przeanalizowano diff, checkpointy, plany E2R/E3, buildery, layouty, visuale,
  obstacle kit, course, CMake oraz sondy mapy.

### 2.2 Świeża walidacja techniczna

Uruchomiono z roota repozytorium:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\gate.ps1 -Numbers
```

Pierwszy zwykły przebieg zwrócił zielono, ale inspekcja timestampów wykazała,
że `jozz_vehicle_track_layout.obj` i validator były starsze od źródła layoutu.
Standardowy build inkrementalny nie odświeżył więc jednego z własnych wejść.
Wykonano wymuszony czysty rebuild validatora:

```powershell
cmd /c "set PATH=& cmake --build --preset windows-debug --target jozz_vehicle_validation --clean-first"
```

Po nim obiekt i binarka miały timestamp `03:03:59`, nowszy niż źródło
`02:10:46`. Kolejny zwykły gate ujawnił drugi problem: build raportował
`samples/test: OK`, choć clean usunął ich `.exe`, po czym gate upadł przy próbie
uruchomienia nieistniejącego `test.exe`. Wykonano więc pełny `target clean`,
jawny rebuild wszystkich trzech targetów i bezpośrednio uruchomiono validator,
`test.exe`, smoke M5 oraz smoke M6. Wynik aktualnego kodu:

- wymuszony czysty rebuild `samples`, validatora i `test`: OK;
- testy: PASS;
- validator: OK;
- boot smoke M6: 0 błędów sokol;
- validator raportuje 4 stacje, 3 wyspy, 13 banków i 3 warianty toru;
- długości pętli raportowane przez dane: 727,5 m / 707,7 m / 752,0 m.

Uruchomiono dodatkowo 300-klatkowy boot smoke M5: 0 błędów sokol.

Istotne ograniczenie: `tools/gate.ps1` smoke-testuje tylko M6, mimo że plany i
checkpointy mówią o M5+M6. Osobny M5 był więc testem ręcznie dodanym do audytu.
Gate nie weryfikuje też istnienia/świeżości outputu po poleceniu build. To jest
osobny defekt bramki zapisany jako `WP-GATE-A`; zielony końcowy wynik uzyskano
przez clean rebuild i bezpośrednie uruchomienie czterech binariów.

### 2.3 Próby runtime

Wykonano sesyjne uruchomienia headless ze stałymi teleportami i kamerami.
Nie zachowano jeszcze pełnego manifestu komenda/seed/log, dlatego poniższe
liczby są obserwacją tej sesji, a nie trwałym testem regresji:

| Próba | Wynik techniczny | Czego nie dowodzi |
|---|---|---|
| N — bumpery, teleport `(-55,38)`, 420 klatek | globalny `contactCount=4`, ok. 1,21 ms/step w klatce pomiaru | jakości rytmu i komfortu w obu kierunkach |
| E — rock island, teleport `(18,0)`, 300 klatek | globalny `contactCount=11`, ok. 1,76 ms/step w klatce pomiaru | braku zakleszczeń dla wielu kątów najazdu |
| prosta główna, 420 klatek | jazda ok. 15,5 m/s stabilna | pełnej pętli ani prześwitu wariantów |
| czerwona artykulacja, 120 klatek | runtime nie upadł | poprawnego przejazdu przez błędne uskoki |

Pliki dowodowe audytu:

- `build/map_audit_20260713_center_top.png`;
- `build/map_audit_20260713_center_3q.png`;
- `build/map_audit_20260713_rocks_close.png`;
- `build/map_audit_20260713_track_top.png`;
- `build/map_audit_20260713_track_profile.png`;
- `build/map_audit_20260713_drive_bumpers_n.png`;
- `build/map_audit_20260713_drive_rocks_e.png`;
- `build/map_audit_20260713_drive_main_straight.png`;
- `build/map_audit_20260713_drive_red_articulation_reverse.png`.

Zrzuty zostały obejrzane. Nie wykonano interaktywnego feel-testu; użyto
uruchomień `samples.exe`, obrazów headless i inspekcji renderów. **Nie jest to
równoznaczne z ręcznym feel-testem Jozza.**

## 3. Rekonstrukcja wykonania planu

Chronologia wynika z checkpointów, timestampów plików i zachowanych renderów:

1. około 23:05–23:07 powstał skeleton E2R;
2. około 23:23, zaledwie 16 minut później, powstał fizyczny dense slice;
3. około 23:54–00:04 feedback „większe/gęstsze” przełożono na wyspy, bumpery i
   place satelitarne;
4. około 00:31–00:41 dodano W articulation/off-camber;
5. po negatywnym feedbacku wizualnym W-slice wyłączono z buildera;
6. bez odbioru E2R.5 rozpoczęto E3: skeleton około 01:28–01:38;
7. fizyczna baza E3 powstała około 01:48, warstwy około 02:01–02:10, profile
   około 02:16, checkpoint E3.3 około 02:20.

Nie znaleziono trwałego wpisu `ACCEPTED BY JOZZ` ani hasha zaakceptowanego
skeletonu E2R. Nie znaleziono także akceptacji skeletonu E3 przed bazą fizyczną.

### Ocena bramek

| Bramka | Ocena | Konsekwencja |
|---|---|---|
| E2R.0 baseline | częściowa | obrazy istnieją, lecz komendy/kamery nie tworzą pełnego manifestu |
| E2R.1 data/validator | częściowa | walidator nie obejmuje buildera i realnych shape'ów |
| E2R.2 audyt generatorów | pominięta | biblioteka wygląda na gotową szerzej, niż jest |
| E2R.3 skeleton → człowiek | brak dowodu, praktycznie pominięta | bryły weszły przed formalnym odbiorem |
| E2R.4b feedback W | zaliczona lokalnie | zły slice poprawnie wycofano |
| E2R.5 jazda/sign-off | niezaliczona | E2R pozostaje otwarty |
| blokada E3 do E2R | złamana | E3 trafił do aktywnego świata |
| E3 skeleton → człowiek | złamana | baza pojawiła się około 10 minut po skeletonie |

Największym błędem procesu nie było eksperymentowanie. Było nim traktowanie
bramki „STOP” jak sugestii i brak commitów/work-package'y umożliwiających łatwe
wycofanie jednego eksperymentu.

## 4. Znaleziska P0 — blokują dalszą rozbudowę

### P0.1 Trzy warianty toru są aktywne jednocześnie

`BuildJozzTrackBase` iteruje po wszystkich wariantach. Na długim odcinku:

- Green: `z=100`, top `y=0`, szerokość 12 m;
- Yellow: `z=102`, top `y=0,30`, szerokość 11 m;
- zachodzenie poprzeczne wynosi około 9,5 m;
- slab ma 0,24 m grubości, więc pionowa szczelina wynosi około 0,06 m;
- średnica koła M6 to około 1,028 m.

To nie jest funkcjonalny overpass. Dolna nitka ma nad sobą sufit znacznie
niższy od koła. Próg 0,28 m dobrano jako nominalny odstęp topów, lecz validator
porównuje tylko średnie wysokości segmentów: nie sprawdza realnych hull/AABB,
grubości slabu w miejscu overlapu ani prześwitu pojazdu.

Decyzja: trzy nakładające się pełne pętle są **REJECT**. W przyszłości aktywna
jest jedna płaska pętla główna, a dopiero później jeden przestrzennie rozłączny
branch techniczny.

### P0.2 Receptura ramp nie zachowuje deklarowanego profilu

Dla aktywnej artykulacji `length=6 m`, `angle=1,2°`, half-thickness `0,15 m`
sam wznios geometryczny wynosi około 0,1257 m. Jednak zastosowane kotwiczenie
daje:

- top początku podjazdu: 0,3000 m;
- top końca podjazdu: 0,4257 m;
- top początku zjazdu: 0,7257 m;
- top końca zjazdu: 0,6000 m;
- uskok na szczycie: 0,3000 m;
- końcowy spadek do płyty: 0,6000 m.

Checkpoint mówi o łagodnym wzniosie około 0,13 m, lecz realna geometria dodaje
30-centymetrowy próg na wejściu, 30-centymetrowy uskok na szczycie i znacznie
większy drop na wyjściu. Ten sam model kotwiczenia zagraża wedge, kicker,
tabletop i gap jump.

Decyzja: żadna rampa nie wraca do aktywnej mapy, zanim osobny kontrakt geometrii
nie zweryfikuje wysokości top-surface w punktach wejście/szczyt/wyjście.

## 5. Znaleziska P1 — walidacja i produkt

### P1.1 Validator nie buduje świata, który deklaruje jako sprawdzony

Target walidatora linkuje dane kampusu i layout toru, ale nie buildery kampusu,
toru, obstacle kit ani terrain. Nie potrafi więc wykryć:

- faktycznych AABB i liczby wygenerowanych shape'ów;
- uskoku top-surface;
- coplanarności i fizycznych overlapów;
- niewłaściwej kategorii kolizji;
- rozjazdu skeletonu i fizycznego miterowania.

Ponadto harness uruchamia trzy sondy mapy, ale drukuje `+2 map probes`.

### P1.2 „Data-driven campus” ma dwa niezależne źródła prawdy

Station specs i content specs są osobnymi tablicami. Builder nie używa stacji
do ograniczania placementu contentu. Można zatem przesunąć wyspę poza stację,
core albo korytarz, zachowując zielony validator station specs.

### P1.3 Gęstość została zamieniona w metric gaming

Feedback, że wcześniejszy poligon był zbyt rzadki i mały, był zasadny. Błędem
było przełożenie go na minima `>=400` kamieni i `>=130` bumperów. Taka metryka
premiuje fragmentację shape'ów, nie:

- czytelną linię wejścia i wyjścia;
- różne bezpieczne linie jazdy;
- odpowiedni udział wolnej przestrzeni;
- brak zakleszczeń dla najazdu bokiem;
- wartość diagnostyczną dla zawieszenia.

Na renderze kamienie czytają się jak drobny scatter, a bumpery jak cienkie
czarne kreski. Liczba jest duża, ale miejsce nie zyskało proporcjonalnej jakości.

### P1.4 E2R jest niepełny względem własnego kontraktu

- brak zatoki 6–8 resetowalnych propów;
- W pozostało głównie bankiem bumperów po odrzuceniu slice'a;
- S nie realizuje kontrolowanego impact/lotu;
- nie ma dowodu pełnego obwodowego przejazdu bez cofania;
- reset propów w UI działa na pustej kolekcji;
- brak jawnych anchorów wszystkich stanowisk i track startu.

### P1.5 Overlay debugowy udaje finalny język mapy

Camp i track skeleton są rysowane bezwarunkowo. Wielkie prostokąty, podpisy i
kolorowe linie dominują nad gridem. Są wartościowym narzędziem diagnostycznym,
ale muszą mieć toggle i być domyślnie wyłączone w widoku produktu.

### P1.6 Coplanarność i kategorie kontaktu wymagają naprawy

- neutralny tor ma top `y=0`, tak samo jak płyta; slab wchodzi w płytę, co może
  powodować podwójne manifoldy i z-fighting;
- rock islands używają kategorii terenu; specjalny envelope koła M6 może przez
  to ponownie generować przedwczesny kontakt boczny z dyskretną skałą.

## 6. Znaleziska P2 — dług konstrukcyjny

- skeleton toru używa prostych offsetów, a fizyka miterów: brak WYSIWYG;
- shared straight ma trzy wartości szerokości: 10 m w spec, 12 m w builderze i
  12 m w visualu;
- enumy `Hairpin`, `Chicane`, `ConstantRadiusArc` są zaliczane bez pomiaru
  krzywizny; jedna „constant return” jest prostą;
- overlap validator ignoruje runoff i ma szerokie wyjątki fork/merge;
- `tile`, recommended speed i kierunek stacji nie sterują builderem;
- yard probe sprawdza głównie bounds/count, nie role i nakładanie;
- `AddRuts` buduje podniesione bryły zamiast rowów;
- `AddStairs` może chować stopnie pod ciągłą płytą;
- `AddStepDown` jest praktycznie tym samym co `AddStepUp`;
- brak limitu maksymalnego shape'ów i budżetu kosztu kontaktów.

## 7. Krytyczna klasyfikacja sugestii i zmian

| Pomysł / zmiana | Decyzja | Uzasadnienie |
|---|---|---|
| pełny centralny kafel jako techniczny grid | **KEEP** | najsilniejsza tożsamość mapy i skala techniczna |
| czysty rdzeń 24×24 m | **KEEP** | potrzebny dla spawnu, strojenia i czytelności |
| hub-and-spoke i satelity kaflowe | **KEEP** | porządkuje rozwój i relację z centrum |
| większa gęstość | **ADAPT** | mierzyć liniami jazdy i negative space, nie quota shape'ów |
| bumper banks | **ADAPT** | jedna receptura naraz, wyraźny profil, test w dwie strony |
| trzy rock islands | **ADAPT** | zachować role, zmniejszyć fragmentację, dodać test zakleszczeń |
| kategoria dyskretnych skał | **ADAPT_PENDING_PROBE** | terrain/object/top-side rozstrzyga test envelope'u M5/M6 |
| W articulation/off-camber w C | **REJECT / QUARANTINE** | odrzucone wizualnie, zły profil, ciasny footprint |
| zatoka lekkich propów | **KEEP, NIEZREALIZOWANE** | odzyskuje interakcyjność pierwszej mapy |
| stałe blueprint overlays | **REJECT** | tylko debug toggle, domyślnie off |
| prosta 220 m | **ADAPT** | osobny Vmax strip albo fragment jednej pętli |
| trzy pełne pętle warstwowe | **REJECT** | nieczytelne i fizycznie nieprzejezdne |
| miterowane segmenty | **QUARANTINE** | technika do odzyskania po testach narożników |
| duże lądowania E/SE | **DEFER** | dopiero po naprawie kontraktu ramp i odbiorze pętli |
| wejście w E3 przed sign-off E2R | **REJECT** | narusza źródło prawdy i zwiększa koszt cofania |

## 8. Co zachować z implementacji

- Etap 1 bez przebudowy;
- cienki `jozz_vehicle_m5_test_course.cpp`;
- rozdział `layout / builder / visual` dla kampusu i toru;
- deterministyczne specs i jawny `centerY` bumperów;
- usunięcie starego far-east 6-lane placementu;
- tile/yards i anchor registry jako zalążek kontraktu;
- decyzję o wycofaniu słabego W-slice po realnym feedbacku;
- istniejące rendery jako materiał porównawczy;
- korektę nierealnego połączenia 220 m prostej z pętlą 260–340 m.

Zachowanie kodu nie oznacza aktywowania go. E3 i niezbadane generatory trafiają
do kwarantanny do czasu osobnego work-package'u.

## 9. Obowiązkowa decyzja naprawcza

1. Nie dodawać nowej geometrii.
2. Zachować bieżący WIP bez mieszania go z poprawkami.
3. Odłączyć fizyczny E3 od aktywnego course'u, bez kasowania źródeł.
4. Dodać domyślnie wyłączony toggle overlayów.
5. Wzmocnić validator tak, aby budował i mierzył realne shape'y.
6. Naprawić kontrakt top-surface obstacle kitu przed ponownym użyciem ramp.
7. Zamknąć centralny kafel małymi slice'ami i ręcznym sign-offem.
8. Dopiero potem zaprojektować jedną pętlę od centerline, z osobnym STOP po
   skeletonie.

Szczegółową kolejność, poziomy mocy, komendy, awarie i rollback opisuje
`PLAN_WYKONAWCZY_MAPA_GPT_LUNA_PL.md`.
