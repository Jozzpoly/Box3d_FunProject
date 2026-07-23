# Plan: Mapa 2.0 — centralny kampus i kontrolowany świat kaflowy

Data pierwotna: 2026-07-11. Rebase v2: 2026-07-12. Rebase v3 po audycie:
2026-07-13. Właściciel kierunku i odbiorów produktowych: Jozz.

## 0. To jest jedyne źródło prawdy statusu mapy

Gdy dokumenty mapy różnią się statusem, wygrywa poniższa tabela. Dokumenty
etapowe opisują zakres, a `PLAN_WYKONAWCZY_MAPA_GPT_LUNA_PL.md` mówi wykonawcy,
jak bezpiecznie realizować jeden work-package.

| Strumień | Stan | Decyzja |
|---|---|---|
| E1 — teren i płyta | **ACCEPTED** | zachować; wyłącznie małe poprawki integracyjne |
| E2R — centralny kampus | **RECOVERY_REQUIRED** | kierunek dobry, kandydat nieodebrany |
| bieżący E3 — trzy pętle | **REJECTED_EXPERIMENT** | odłączyć od runtime, zachować kod do audytu |
| nowy E3 — jedna pętla | **LOCKED** | otworzyć dopiero po `E2R ACCEPTED BY JOZZ` |
| E4 — plac fizyki | **LOCKED** | po E2R i po kontrakcie PropRegistry |
| E5 — spawner/stress | **LOCKED** | po bezpiecznym resecie i ownership dynamicznych body |
| E6 — nawigacja/telemetria | **LOCKED** | po ustabilizowaniu stref, nie przed nimi |

`REJECTED_EXPERIMENT` zapisuje połączony werdykt: Jozz w bieżącym zadaniu
odrzucił rezultat tego kierunku jako wyraźnie gorszy, a audyt wykazał dodatkowo
błędy fizyczne P0. Nie jest to samodzielna decyzja modelu. Kod pozostaje w
kwarantannie, aby decyzja była odwracalna i udokumentowana.

**Aktualny dozwolony krok:** R0 — Truth and Recovery. Nie wolno ulepszać
bieżącego E3, dopóki nie zostanie zachowany WIP, odłączony fizyczny tor i
przywrócony czytelny baseline E1+E2R.

Audyt uzasadniający status: `AUDYT_REALIZACJI_MAPY_2026_07_13_PL.md`.

## 1. Nienegocjowalna tożsamość mapy

### 1.1 Kafel C jest sercem produktu

Cały środkowy kafel `C`, nominalnie `x,z∈[-66,667;66,667]`, pozostaje jedną
nieprzerwaną powierzchnią proceduralnego technicznego gridu. Nie pokrywamy go
asfaltowym dywanem ani kolorowymi płytami stref.

Grid pełni cztery role:

- metryczna skala do oceny pojazdu i geometrii;
- spawn, strojenie i obserwacja rigu;
- kampus krótkich, powtarzalnych prób;
- centralny węzeł do stref satelitarnych.

### 1.2 Środek jest czysty, ale nie pusty

Central Core `24×24 m` przy `(0,0)` ma zero przeszkód i scatteru. Otaczają go
krótkie stanowiska, cztery szerokie spokes i pusty obwodowy korytarz. Aktywność
skupia się na całym kaflu C, podobnie jak w pierwszej mapie, ale jest
zaprojektowana, mierzalna i resetowalna.

### 1.3 Hub-and-spoke

Każda strefa satelitarna:

- respektuje przypisany kafel;
- ma wjazd skierowany ku C;
- ma jawny gate/anchor przed pierwszą przeszkodą;
- oferuje bezpieczny powrót;
- nie udaje osobnej mapy doklejonej na obrzeżu.

### 1.4 Przeszkoda nie jest stanowiskiem

Generator bryły jest biblioteką. Stanowisko musi mieć ID, cel testu, footprint,
anchor, kierunek, prędkość, approach, runoff, kategorię, reset, mierzalny sygnał
i dowód przejazdu. Sam shape count nie jest kryterium jakości.

### 1.5 Język wizualny

- grid i ziemia są powierzchnią dominującą;
- stal/szarość/ziemia wynikają z funkcji;
- kolor trudności jest małym akcentem, nie nasyconą całą bryłą;
- overlay footprintów, yardów i centerline jest debugiem, domyślnie wyłączonym;
- tekst pojawia się blisko stanowiska i nie zasłania kontaktu koła;
- sylweta oraz kierunek wjazdu muszą być czytelne bez debug overlay.

## 2. Czego nauczyło wykonanie E2R/E3

### Zachować

- pełny grid C i czysty core;
- cienki course oraz rozdział data/builder/visual;
- deterministyczne specs;
- trzy role wysp skał i niskie bumper banks jako materiał do dopracowania;
- place satelitarne i wspólny world layout;
- gotowość do wycofania słabego slice'a po feedbacku;
- zaakceptowany E1, w tym offroad, góra, szew i regeneracja.

### Dostosować

- „gęściej” oznacza więcej wartościowych linii jazdy, nie setki shape'ów;
- jedna receptura kontaktu jest odbierana przed następną;
- station specs muszą sterować builderem albo walidować jego realny wynik;
- miterowanie można odzyskać tylko dla jednej zaakceptowanej trasy;
- 220 m prosta może być Vmax stripem albo fragmentem jednej pętli, nie wymusza
  trzech wersji toru.

### Odrzucić

- trzy nałożone pełne pętle budowane jednocześnie;
- warstwę drogi z prześwitem mniejszym od envelope'u auta;
- rampy bez kontraktu top-surface;
- stałe blueprint overlays;
- quota `>=400 rocks`, `>=130 bumpers` jako definicję jakości;
- automatyczne przechodzenie ze skeletonu do brył;
- rozpoczęcie kolejnego etapu bez wpisu `ACCEPTED BY JOZZ`.

## 3. Docelowy podział 3×3

```text
z+
+----------------------+----------------------+----------------------+
| NW: techniczny łuk   | N: jedna pętla /     | NE: powrót pętli     |
| i opcjonalny branch  | osobny Vmax strip    | i łącznik E          |
+----------------------+----------------------+----------------------+
| W: drift / skid pad  | C: TECHNICAL GRID    | E: brama offroadu    |
| wejście skierowane C | CENTRAL TEST CAMPUS  | i neutralny wybieg   |
+----------------------+----------------------+----------------------+
| SW: plac fizyki      | S: spawner / stress  | SE: duże lądowania   |
| z containment        | z containment        | po kontrakcie ramp   |
+----------------------+----------------------+----------------------+
                                                     -> OFFROAD 400×400
```

Role są budżetem przestrzeni. Nie są pozwoleniem, aby od razu wypełnić każdy
kafel. Najpierw centrum, potem jedna trasa, potem jedna strefa naraz.

## 4. Stan E1 — zaakceptowany fundament

Zostają:

- płyta 400×400 m podzielona na 3×3 kafle;
- proceduralny grid tylko na pełnym kaflu C;
- heightfield offroad 400×400 m;
- deterministyczny seed/regeneracja;
- szew pod płytą;
- góra i węzły górskie;
- bezpieczne spawn/teleport.

E1 wolno dotknąć tylko, gdy nowy kampus ujawni konkretny błąd integracji:
próg szwu, brak gridu na shape'ie C, konflikt wizualny sąsiedniego kafla albo
niebezpieczny spawn. Każda poprawka jest osobnym WP i nie zmienia generatora
offroadu bez jawnej zgody Jozza.

## 5. Nowa roadmapa

### R0 — Truth and Recovery

Cel: odzyskać kontrolę nad stanem, zanim cokolwiek nowego powstanie.

- sklasyfikować dirty tree: `E1 / E2R_KEEP / E2R_FIX / E3_QUARANTINE / DOCS`;
- zachować diff, komendy, logi i rendery eksperymentu;
- wybrać z Jozzem sposób zachowania WIP (najbezpieczniej osobna gałąź/snapshot);
- nie łączyć recovery z poprawą geometrii;
- oznaczyć bieżący E3 jako odrzucony eksperyment we wszystkich źródłach prawdy.

Wyjście: odtwarzalny manifest i jednoznaczny hash/snapshot. Bez tego R1 jest
zablokowany.

### R1 — Restore Active World

Cel: uruchamiany świat zawiera E1 oraz kandydat E2R, ale nie aktywną fizykę E3.

- dodać jawny feature/state switch i domyślnie wyłączyć budowę E3;
- zachować pliki E3 w kwarantannie, bez kasowania;
- overlay kampusu/yardów/toru przenieść pod debug toggle, domyślnie off;
- naprawić gate, aby smoke-testował M5 i M6;
- uzyskać stałe zrzuty: cała płyta, C top, C 3/4, widok z core;
- potwierdzić, że runtime ma ten sam aktywny świat w M5 i M6.

Wyjście: **BASELINE_RECOVERED**, nie `E2R ACCEPTED`.

### R2 — Central Tile Completion

Cel: cały kafel C jest skupionym, czytelnym i przejezdnym kampusem lepszym od
pierwszej mapy.

Kolejność bez skrótów:

1. validator rzeczywistych shape'ów, core, spokes, loop, anchorów i kategorii;
2. stały zestaw kamer oraz manifest dowodów;
3. jedna stacja komfort/rytm N;
4. probe kategorii dyskretnej skały dla envelope'u M5/M6, potem jedna wyspa
   terenowa E przetestowana z wielu kierunków;
5. zatoka 6–8 lekkich resetowalnych propów;
6. druga wyspa/stanowisko dopiero po odbiorze poprzedniego;
7. prosty, łagodny slice S tylko po naprawie kontraktu obstacle kitu;
8. W pozostaje wolnym korytarzem albo dostaje nową koncepcję; odrzucona
   artykulacja nie wraca automatycznie;
9. pełny obwodowy przejazd i powrót do core;
10. ręczny odbiór Jozza.

Wyjście: wpis `E2R ACCEPTED BY JOZZ`, hash, obrazy i wynik jazdy. Dopiero ten
wpis odblokowuje R3.

### R3 — One Outer Driving Loop

Cel: jedna czytelna płaska pętla na W/NW/N/NE, podporządkowana centrum.

Kolejność:

1. dane jednej centerline z rzeczywistym pomiarem promieni/krzywizny;
2. jawny connector C→start i connector finish→C;
3. skeleton bez fizycznej jezdni;
4. **STOP i akceptacja Jozza**;
5. jedna płaska baza, budowana dokładnie raz;
6. test seams/coplanarity, pełne okrążenie w obu labach;
7. jeden przestrzennie rozłączny branch techniczny, opcjonalnie;
8. krawężniki/runoff/barierki tylko tam, gdzie wynikają z analizy wypadnięcia;
9. lap timer dopiero po stabilnym pełnym okrążeniu.

Zakazane: trzy równoległe pełne warianty, warstwy bez vehicle-clearance i
zaliczanie topologii przez sam enum.

### R4 — Drift i duże lądowania jako osobne produkty

R4A — W drift:

- jeden skid pad albo ósemka, nie oba naraz;
- brak fizycznego progu tarcia;
- wejście od C, containment propów, reset;
- odbiór feel przed rozbudową.

R4B — E/SE landing:

- dopiero po testach top-surface wszystkich użytych ramp;
- osobny approach, marker prędkości, landing i escape lane;
- najpierw łagodny tabletop, twardy gap jump po osobnym sign-offie.

### R5 — Physics Yard SW

- wspólna aleja od C i containment;
- jeden przyrząd na WP: shaker, rolling road, bridge, see-saw;
- ownership, reset i budżet body przed zabawkami destrukcyjnymi;
- każda maszyna ma mierzalny cel, nie tylko animację.

### R6 — Spawner/Stress S–SE

- najpierw PropRegistry i jednoznaczne ownership;
- jeden bezpieczny spawn i reset;
- mała partia przed pojazdem z twardym limitem;
- większy batch tylko w containment yard;
- profil 25/50/100/250, koszt kroku i pełne sprzątanie;
- centralny kampus nigdy nie jest domyślnym celem stressu.

### R7 — Navigation, Telemetry, Final Map 2.0

- teleporty do bram, nie do środka przeszkody;
- nazwy względem C;
- distance-culling i debug toggles;
- lap/airtime/contact telemetry dopiero po stabilizacji geometrii;
- finalna suita stałych kamer, pełnych przejazdów i regresji liczbowej;
- końcowy wpis `MAPA 2.0 ACCEPTED BY JOZZ`.

## 6. Stan jako maszyna, nie opis narracyjny

Status strumienia i status WP to dwie osobne warstwy. Strumień może mieć
dyspozycję `RECOVERY_REQUIRED` albo `REJECTED_EXPERIMENT`, jak w tabeli §0.
Każdy wykonywalny WP używa wyłącznie jednego z poniższych stanów:

- `LOCKED` — nie wolno implementować;
- `READY` — warunki wejścia są spełnione;
- `IN_PROGRESS` — dokładnie jeden aktywny WP;
- `WAITING_FOR_JOZZ` — kod zamrożony, czeka na odbiór;
- `ACCEPTED` — dowody + jawna decyzja człowieka;
- `REJECTED` — wynik WP odrzucony przez Jozza; nie rozwijać, zachować dowód;
- `SUPERSEDED` — historyczne, ze wskazaniem następcy.

Model nie może sam zmienić `WAITING_FOR_JOZZ` na `ACCEPTED`. Zielony build nie
zmienia statusu produktu.

## 7. Bramka techniczna wspólna dla każdego WP

Minimalnie:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\gate.ps1 -Numbers
.\build\bin\Debug\samples.exe --sample-name "M5 First Drivable" --frames 300
.\build\bin\Debug\samples.exe --sample-name "M6 Suspension Rig Lab" --frames 300
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\doc_drift_check.ps1
git diff --check
```

Ponadto WP mapy musi mieć:

- walidację realnie utworzonych shape'ów, nie tylko specs;
- kontrolę bounds, core, spokes, loop, approach i runoff;
- kontrolę top-surface dla profili pionowych;
- kategorie: nawierzchnia zgodna z envelope'em koła, prop `0x1`;
- stabilny body/shape count po restartach;
- brak nowych ostrzeżeń sokol/Box3D;
- test dokładnie zmienionego miejsca, nie tylko spawnu.

## 8. Bramka produktowa

Każdy slice zmieniający layout lub sylwetę wymaga:

- identycznego kadru przed/po;
- top-down płyty i kafla C;
- widoku z core;
- ujęcia z wysokości koła/kierowcy;
- przejazdu w deklarowanym kierunku i w przeciwnym, jeżeli dwukierunkowy;
- notatki o tym, co widać źle, nie tylko o sukcesach;
- jawnej decyzji `ACCEPT / REJECT / ADAPT` Jozza.

Plik nazwany `stop_gate.png` nie jest akceptacją. Akceptacją jest wpis z datą,
hashem i tekstem `ACCEPTED BY JOZZ`.

## 9. Kontrakty architektoniczne

- bez zmian `src/` i `include/` Box3D;
- `world_layout`: kafle, role, kotwice, granice; bez builderów;
- `obstacle_kit`: jedna przeszkoda i testowalny profil top-surface;
- `central_test_campus`: specs + builder + visual, ze wspólnymi ID;
- `track_layout`: jedna centerline i realne metryki geometrii;
- `track_builder`: buduje tylko wybrany aktywny layout;
- `visual`: czyta te same dane/meshe co builder albo przechodzi WYSIWYG probe;
- `m5_test_course`: cienka orkiestracja i jawne feature states;
- M5 i M6 budują identyczną geometrię należącą do mapy; całkowite liczniki
  świata mogą się różnić przez pojazd i stan sesji;
- debug overlay jest oddzielony od product view;
- validator linkuje i mierzy realny builder albo korzysta z jednoznacznego
  geometry manifestu wygenerowanego tą samą ścieżką.

## 10. Budżety jakości zamiast quota shape'ów

Każde stanowisko ma raportować przynajmniej:

- footprint i procent zajętej powierzchni;
- szerokość najwęższej poprawnej linii;
- liczbę sensownych linii jazdy;
- approach/runoff;
- maksymalny step/lip oraz ciągłość top-surface;
- body/shape count z limitem maksymalnym;
- contacts peak/mean podczas referencyjnego przejazdu;
- wheel-contact ratio i `vy RMS`, gdy istotne;
- najniższą i najwyższą prędkość zakończoną bez zakleszczenia;
- wynik ręcznej oceny czytelności i feel.

Progi ustala WP przed implementacją. Model nie może poluzować progu po porażce.

## 11. Zasady publikacji i rollbacku

- jeden WP = jeden mały diff = jeden lokalny commit `CANDIDATE` po pełnym gate;
- hash lokalnego kandydata służy do odbioru Jozza; push/publikacja dopiero po
  `ACCEPT`, a po `REJECT` kandydat jest odłączany/revertowany zgodnie z WP;
- commit nie może mieszać E2R z E3 albo kodu z niezależnym refaktorem;
- przed pracą zapisać `git status --short --branch` i listę plików użytkownika;
- istniejącego dirty WIP nie stage'ować zbiorczo;
- przy odrzuceniu slice'a wyłączyć go jednym switchem/revertem jego commita;
- nie usuwać eksperymentu, zanim nie ma obrazu, logu i decyzji `REJECTED`;
- żaden model nie commit/pushuje bieżącego mieszanego WIP bez decyzji Jozza o
  sposobie zachowania.

## 12. Co jest poza aktualnym trackiem

- streaming/LOD terenu;
- minimapa 2D;
- pogoda i dzień/noc;
- import heightmap PNG;
- AI/ghost/przeciwnicy;
- zapis dynamicznego świata w presetach;
- zmiany solvera lub core Box3D;
- dekoracyjny polish przed zamknięciem geometrii i feel.
