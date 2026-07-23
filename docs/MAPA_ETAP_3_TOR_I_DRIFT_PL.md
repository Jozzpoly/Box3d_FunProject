# Mapa — Etap 3: jedna pętla, osobny drift i osobne lądowania

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.
Status bieżącej implementacji: **REJECTED_EXPERIMENT / DO DEZAKTYWACJI**.
Status nowego Etapu 3: **LOCKED DO `E2R ACCEPTED BY JOZZ`**.

## 1. Decyzja po audycie

Obecne trzy warianty Green/Yellow/Red nie są bazą dalszego toru. Builder tworzy
je jednocześnie; na długich odcinkach nachodzą prawie całymi szerokościami, a
różnica wysokości nie daje prześwitu pojazdu. Aktywny profil ramp ma dodatkowo
błędne uskoki top-surface.

Należy:

- zachować pliki i rendery jako materiał eksperymentalny;
- odłączyć fizyczny `BuildJozzTrackBase/Profiles` od domyślnego course'u;
- nie poprawiać dekoracji, profili ani miterów tego układu;
- rozpocząć nowy E3 od jednej centerline dopiero po odbiorze E2R.

Pełne dowody: `AUDYT_REALIZACJI_MAPY_2026_07_13_PL.md`.

## 2. Cel nowego E3

Zbudować jedną czytelną, płaską pętlę prowadzenia na `W/NW/N/NE`, która:

- ma jawny i krótki connector z kampusu;
- nie dominuje wizualnie nad kaflem C;
- jest przejezdna pełnym okrążeniem przed dodaniem profili;
- ma realne promienie, run-off i escape paths;
- może później dostać jeden rozłączny branch techniczny;
- nie miesza driftu i dużych lądowań z odbiorem bazowej pętli.

## 3. Zakazane rozwiązania

- trzy pełne warianty aktywne jednocześnie;
- drogi piętrowe bez prześwitu obliczonego dla całego pojazdu;
- przypisywanie `Hairpin/Chicane/Arc` bez pomiaru krzywizny;
- bardzo ostre polilinie naprawiane gigantycznym miterem;
- coplanarny slab zanurzony w bazowej płycie;
- centerline renderowana inaczej niż budowana fizyka;
- profile/rampy przed pełnym przejazdem płaskiej pętli;
- lap timer przed ustabilizowaniem topologii;
- stały debug overlay w product view;
- automatyczne przejście dalej po samym zielonym buildzie.

## 4. Kafle i kolejność produktów

| Produkt | Kafle | Warunek wejścia |
|---|---|---|
| pętla główna | W/NW/N/NE | E2R accepted |
| branch techniczny | NW/N | płaska pętla accepted |
| drift/skid pad | W | pętla nie koliduje i ma accepted footprint |
| landing yard | E/SE | naprawiony obstacle kit i osobny skeleton accepted |

Brama offroadu E pozostaje wolna. SW pozostaje zarezerwowane dla placu fizyki,
S dla kontrolowanego stressu. Trasa może przekroczyć szew kafla wyłącznie jako
jawny connector.

## 5. Kontrakt centerline

Jedynym źródłem prawdy jest centerline/segment graph. Każdy segment ma:

- stabilne ID i jawnych sąsiadów;
- start/end i kierunek jazdy;
- typ wynikający z geometrii, nie z etykiety;
- długość, krzywiznę/minimalny promień;
- szerokość jezdni;
- lewy/prawy runoff;
- profil wysokości — dla bazowej pętli dokładnie płaski;
- gate wejścia/wyjścia;
- prędkość projektową.

Validator wylicza długość i krzywiznę z punktów/krzywych. `ConstantRadiusArc`
failuje, jeśli promień nie jest w tolerancji; `Hairpin` failuje bez wymaganej
zmiany kierunku; `Chicane` wymaga dwóch przeciwnych zmian krzywizny.

## 6. Kolejność bez skrótów

### E3-N0 — wymagania i budżet

- określić jedną rolę pętli;
- zdecydować, czy 220 m Vmax jest częścią pętli czy osobnym stripem;
- ustalić minimalne promienie na podstawie testowej prędkości;
- ustalić footprint connectora z C;
- zero kodu buildera.

### E3-N1 — centerline data + validator

- jedna zamknięta centerline;
- jeden connector C→start i jeden powrót;
- kontrola bounds, self-intersection, promieni i run-off;
- brak warstw, branchy i profili.

### E3-N2 — skeleton

- tylko centerline, krawędzie i gate markers;
- debug toggle, domyślnie off;
- obrazy: plate top, center view, driver start, cztery krytyczne zakręty;
- **STOP: `E3 SKELETON ACCEPTED BY JOZZ` z hashem**.

### E3-N3 — jedna płaska baza

- builder tworzy centerline dokładnie raz;
- spójne top y i jawne rozwiązanie kontaktu z bazową płytą;
- builder i visual używają tej samej geometrii krawędzi;
- zero profili, barier, timerów i dekoracji;
- pełne okrążenie M5 i M6 bez teleportu przez fragment;
- test seams, `vy RMS`, contacts i wheel-contact ratio.

### E3-N4 — bezpieczeństwo

- runoff z realnej prędkości i kierunku wypadnięcia;
- krawężnik tylko z funkcją;
- bariera wyłącznie poza escape path;
- sprawdzenie linii widzenia z C;
- ponowny pełny przejazd.

### E3-N5 — jeden branch techniczny

- branch przestrzennie rozłączny w planie;
- wspólny odcinek budowany raz;
- gate wyboru i merge z bezpiecznym kątem;
- branch może pozostać wyłączony bez zmiany pętli;
- osobny skeleton STOP przed fizyką.

### E3-N6 — pomiar

- lap timer filtruje chassis, ignoruje koła i propy;
- kierunkowe start/split/finish;
- teleport/reset anuluje okrążenie;
- debounce i test jazdy pod prąd;
- timer nie jest kryterium odbioru geometrii.

## 7. Vehicle clearance i profile wysokości

Bazowa pętla jest płaska. Jakakolwiek późniejsza warstwa/overpass wymaga:

- envelope całego M5 i M6 z marginesem, nie tylko średnicy koła;
- prześwitu mierzonego od najwyższego punktu dolnej nawierzchni do najniższego
  elementu górnej konstrukcji;
- approach gradient, crest radius i sight distance;
- collision/raycast probe w całym korytarzu;
- osobnego skeletonu i ręcznego odbioru.

Domyślną decyzją jest **brak overpassu**. Różnica topów 0,28 m nie jest
vehicle clearance.

Każdy profil wysokości ma top-surface samples entry/25%/50%/75%/exit, tolerancję
ciągłości i limit lip. Grubość slabu nie może dodawać nieplanowanego wzniosu.

## 8. Drift W — osobny odbiór

Po zaakceptowaniu footprintu pętli:

- najpierw jeden skid pad 40–50 m albo jedna ósemka;
- powierzchnia zmienia tarcie bez fizycznego progu;
- wejście skierowane ku C;
- propy/pachołki resetowalne i zawarte w strefie;
- brak zajęcia SW;
- osobny feel-test oraz STOP przed drugim układem.

## 9. Landing E/SE — osobny odbiór

Landing yard nie używa żadnego generatora rampy, zanim jego top-surface probe
nie jest zielony.

Minimalny kontrakt:

- prosty approach co najmniej 45 m;
- marker zakresu prędkości;
- łagodny tabletop przed gap jump;
- landing co najmniej 35 m;
- emergency braking bez twardej przeszkody;
- boczny escape/return;
- shape nie kończy się poza płytą;
- pełny reset i brak wpływu na bramę offroad.

## 10. Bramka techniczna

- wspólny gate oraz osobny smoke M5/M6;
- real topology probe, nie enum-presence;
- brak self-intersection i niejawnych overlapów z runoff;
- pełna pętla przejechana bez teleportu;
- żadnego fragmentu budowanego więcej niż raz;
- WYSIWYG builder/visual;
- brak coplanarnych konkurencyjnych powierzchni kontaktu;
- stabilny body/shape count;
- profile wysokości spełniają jawne top-surface tolerances;
- 0 nowych błędów Box3D/sokol.

## 11. Bramka produktowa

- top-down płyty przed/po;
- z Central Core pętla ma czytelne wejście, ale nie „dusi” centrum;
- widok kierowcy: start, każdy zakręt krytyczny, merge i powrót;
- pełne okrążenie wykonane ręcznie przez Jozza;
- branch/drift/landing oceniane osobno;
- jawne `ACCEPT / ADAPT / REJECT`, bez automatycznego kolejnego WP.

## 12. Definition of Done

E3 jest gotowy, gdy jedna pętla i jej connector są zaakceptowane technicznie i
produktowo. Drift, branch i landing mogą pozostać niezrealizowane bez blokowania
odbioru samej pętli, o ile status każdego jest jawny. Trzy warianty, ładny
top-down, długość z validatora albo stabilne 300 klatek na anchorze nie są
dowodem przejezdnego toru.
