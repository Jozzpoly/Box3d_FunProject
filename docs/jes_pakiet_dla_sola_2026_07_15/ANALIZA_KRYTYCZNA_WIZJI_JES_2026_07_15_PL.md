> **UWAGA (2026-07-15, później):** Jozz odniósł się do krytyk K1–K6 —
> rozstrzygnięcia są wcielone do kompletu **JES_01/02/03** (tabela:
> `JES_03_PROGRAM_REALIZACJI_PL.md` §5). Ten plik pozostaje dokumentem
> źródłowym analizy; przy sprzeczności wygrywa komplet JES. Wejście do
> projektu: `JES_00_INDEX_PL.md`.

# Analiza krytyczna wizji „Jozz Engineering Sandbox" v0.1

Data: 2026-07-15. Autor: Claude (Fable 5), w roli zdefiniowanej przez samą
wizję (§15: krytyczny architekt, przeciwnik własnych założeń).
Dokument analizowany: `WIZJA_JOZZ_ENGINEERING_SANDBOX_V0_1.md` (kopia w repo).
Dokumenty siostrzane: `PLAN_NOWY_PROJEKT_ULTIMATE_2026_07_15_PL.md` (proces,
stack, lekcje dem) — razem tworzą komplet założycielski.

Zgodnie z poleceniem nadrzędnym wizji §15.1: podważam mocno, nie redukuję.
Każda krytyka ma formę: zarzut → dowód/uzasadnienie → propozycja.

---

## 1. Ocena ogólna: to jest najlepszy dokument w historii obu projektów

Mówię to jako agent, który przeczytał ~40 dokumentów JV i dokumentację VAW.
Wizja robi dobrze rzeczy, których TAMTE dokumenty nauczyły się dopiero po
kryzysach:

- **Realizm kontraktowy** (§3.1) — to jest dojrzalsza, ogólniejsza forma
  naszego ADR-0006 (realizm rdzeniem, `[ARCADE]` opt-in) i wzorca BeamNG.
- **Semantyka > mesh, mesh = cache** (§3.2) — to jest uogólnienie lekcji
  tożsamości VAW (bodyId nie persystuje) na cały świat. Złota zasada.
- **Zakaz klas Car/Plane/Excavator jako fundamentu** (§4) — dokładnie
  filar VAW „no single-body assumption", teraz z lepszym uzasadnieniem.
- **Stopnie wierności per domena** (§3.5) — chroni przed „jednym solverem
  wszystkiego" i przed CFD-paraliżem.
- **Jawne nie-cele i kierunki odrzucone** (§0.2, §12.4, §13.1) — dyscyplina,
  której pierwsze wersje VAW i JV nie miały.
- **„Każdy etap udowadnia sprzężenie"** (§11.1) — to jest nasz walking
  skeleton i fun-gate w wersji systemowej. Tabela „słabe osiągnięcie vs
  właściwy dowód" powinna wisieć nad każdym WP.

Werdykt wstępny: wizja jest **architektonicznie zdrowa i warta lat pracy**.
Poniższe krytyki jej nie podważają — uszczelniają ją.

---

## 2. Krytyki twarde

### K1. Wizja nie ma gracza — ma systemy

**Zarzut:** dokument definiuje prawa świata i osiem domen, ale pętla
minutowa gracza (co robię przez pierwsze 15 minut i czemu chcę wrócić?)
jest odroczona do pytań 14.1. Ryzyko „pięciu gier naraz" (§12.2) jest
nazwane, ale struktura dokumentu — 8 równoległych domen — sama je zaprasza.

**Dowód z historii:** JV zaczął od pętli („jedź i czuj") i dlatego przeżył;
mapa E2R/E3 upadła dokładnie wtedy, gdy budowano „domeny" (3 pętle toru,
setki przeszkód) bez odbioru doświadczenia. VAW analogicznie: najpierw
latało, potem rosło.

**Propozycja:** odpowiedź na pytanie 14.1.1 brzmi: **„Dolina Prób"** —
jeden ograniczony teren (rząd wielkości mapy JV), warsztat, JEDEN łańcuch:
zbuduj maszynę → wykop/przemieść/zagęść → zmierz rezultat → przejedź po nim
własnym pojazdem. Kampania techniczna = seria mierzalnych zadań na tym
terenie; sandbox = ten sam teren bez zadań. Gracz jako swobodna kamera
konstruktora + „wsiadanie" do maszyny (hybryda z pytania 14.1.3) — awatar
fizyczny NIE jest potrzebny w pierwszym produkcie i dodałby osobną domenę
(character controller) bez dowodu sprzężenia.

### K2. Teren pod aktywną maszyną to ryzyko nr 1 — i mam twardy fakt

**Zarzut:** wizja poprawnie wskazuje ryzyko (§12.1 „Aktualizacja świata pod
pojazdem"), ale traktuje je równorzędnie z innymi. Ono nie jest równorzędne
— jest JEDYNYM elementem wizji, którego żadne z naszych dem nawet nie
dotknęło, a na którym wisi cała fantazja robót ziemnych.

**Twardy fakt (sprawdzone w nagłówkach box3d, 2026-07-15):** box3d **nie ma
API aktualizacji heightfielda in-place** — jest tylko `b3CreateHeightField`
/ `b3DestroyHeightField` / `b3CreateHeightFieldShape` (`collision.h:380-393`).
Regeneracja terenu w JV (E1) niszczy i tworzy shape od nowa — działa, ale
była używana TYLKO jako operacja „między jazdami", nigdy pod kołami.
Konsekwencje do zmierzenia, nie do przegadania:

- świat robót musi być pocięty na **chunki** (osobne shape'y heightfield /
  hulle), a operacja narzędzia przebudowuje wyłącznie dirty chunk;
- odtworzenie shape'a pod stojącą/kopiącą maszyną gubi manifoldy kontaktu
  i warm-start solvera — pytanie brzmi: czy da się to zrobić na granicy
  ticku bez widocznego „drgnięcia" maszyny? (JV lekcja: utrata warm-start
  była mierzalna przy phased-union kół — to ta sama klasa problemu);
- koszt `b3CreateHeightField` dla chunka NxN na tick — do benchmarku;
  alternatywa: kolumny jako zestaw box-hulli per chunk (prostsze do
  częściowej przebudowy, droższe w broad-phase — do porównania A/B.

**Propozycja:** eksperyment pionowy **X1 „Żywy grunt"** jest PIERWSZYM
nowym eksperymentem programu (przed aero, przed automatyzacją, przed
światem): statyczna maszyna z łyżką na aktuatorze nabiera materiał z
kolumnowego chunka, teren się obniża, collider odtwarza się pod kołami
stojącej obok wywrotki. Kryteria z góry: zero utraty kontaktu kół
(telemetria JV to już mierzy), bilans masy = 0 delta, koszt przebudowy
chunka < 1 ms, brak impulsów-widmo. Jeżeli X1 nie przejdzie na box3d,
wizja robót ziemnych wymaga zmiany reprezentacji (np. hulle kolumnowe) —
lepiej wiedzieć to w tygodniu 2 niż w roku 2.

### K3. Monotonia pracy: automatyzacja to odpowiedź, która przyjdzie za późno

**Zarzut:** wizja odpowiada na ryzyko monotonii automatyzacją (§4.5, §12.2),
ale automatyzacja w każdej realnej kolejności budowy przychodzi późno
(wymaga sensorów, kontrolerów, sygnałów). Pierwszy produkt będzie MANUALNY —
i jeśli ręczne kopanie nie będzie frajdą samo w sobie, gra umrze zanim
automatyzacja ją uratuje.

**Dowód z historii:** JV przeżył, bo JAZDA była frajdą od M5 — zanim
powstał teren, presety i mapa. „Fun-gate" (plan ULTIMATE §7) istnieje
dokładnie po to.

**Propozycja:** do konstytucji (§13) dopisać zasadę 11: **„Praca ręczna
musi być przyjemna zanim zostanie zautomatyzowana"** — feel łyżki (opór,
dźwięk, wibracja ramienia, satysfakcja pełnej łyżki) jest kryterium odbioru
X1/X2 na równi z bilansem masy. Automatyzacja skaluje frajdę — nie zastępuje
jej.

### K4. Trylemat autorytetu drogi — rozstrzygnąć „projektem i pomiarem", nie autorytetem

**Zarzut:** §6.2 uczciwie pokazuje trzy modele (autorytatywna droga /
stempel / warstwy operacji), ale zostawia wybór otwarty, a to jest decyzja,
która definiuje pół architektury świata.

**Propozycja (v1):** w pierwszym produkcie **RoadGraph nie edytuje świata w
ogóle**. Jest PROJEKTEM (overlay: przebieg, profil, przekrój) i POMIAREM
(porównanie stanu faktycznego z projektem — dokładnie §3.4). Świat zmieniają
WYŁĄCZNIE maszyny. Droga „istnieje", bo została fizycznie usypana,
wyprofilowana i zagęszczona — a RoadGraph mówi, jak daleko jej do projektu.
To: (a) usuwa cały problem konfliktu autorytetów, (b) czyni diagnostykę
gameplayem (§11.3), (c) jest najbliższe duszy wizji („wynik pracy musi być
mierzalny"). Autorytatywne generowanie dróg może wrócić później jako
narzędzie „zleć wykonanie" — wtedy będzie automatyzacją, nie magią.

### K5. Determinizm vs asynchroniczny meshing — potrzebna poprawka konstytucyjna

**Zarzut:** wizja wymaga deterministycznego replay (§11, pytanie 14.4.4)
ORAZ asynchronicznej generacji cache (§9.3). Te dwa wymagania zderzą się
w pierwszym tygodniu implementacji, jeśli nie ustalimy prawa nadrzędnego.

**Dowód:** spike `kernel_v0` (2026-07-15) potwierdził determinizm box3d
(hash trajektorii identyczny run-do-run) — ale tylko dlatego, że wszystko
działo się synchronicznie w pętli fixed-step.

**Propozycja — zasada 12 konstytucji:** **stan symulacji zmieniają wyłącznie
commity na granicy ticku, w deterministycznej kolejności** (posortowane po
trwałych ID, nie po czasie ukończenia wątku). Asynchroniczność wolno stosować
tylko do PRZYGOTOWANIA danych (mesh, cache), nigdy do ich APLIKACJI. Replay
zapisuje tick commitu, nie moment obliczenia. To samo prawo obejmuje
przyszły streaming (§9.1).

### K6. Wizja nie ma procesu — i nie musi, ale małżeństwo musi być jawne

**Zarzut:** dokument milczy o workflow (bramki, agenci, dowody, WP) — a
historia obu dem dowodzi, że proces jest równie krytyczny jak architektura
(incydent E3 zniszczył dobrą architekturę mapy złym procesem).

**Propozycja:** przyjąć DWA dokumenty założycielskie o równej randze:
wizja JES v0.1 = **konstytucja produktu**; plan ULTIMATE (§3, §7, §7b) =
**konstytucja procesu i stacku** (maszyna WP, routing mocy, gate v2 z
kontrolą timestampów, CI od 1. commita, render-is-the-gate, fun-gate,
baseline-diff, budżety linii, evidence manifest). Trzeci element kompletu:
**aneks „Prawa inżynierskie"** — lekcje kodu, których wizja (pisana bez
dostępu do naszych repo) nie zna, a które są prawami, nie anegdotami:
kategorie kolizji tagowane po OBU stronach; CCD off dla światów pojazdów;
ciała strukturalne bez shape'ów + jawna masa; pręty distance-joint mają
gałąź lustrzaną (wahacz = ciało z limitem); masa efektywna na smukłym
ramieniu; walidator MUSI budować realny świat produkcyjną ścieżką i mieć
testy negatywne; zakaz cichych fallbacków numerycznych (VAW); zapis =
tabela pól + sondy round-trip; asset pipeline dowodzi się in-game.

### K7. Brak budżetów liczbowych

**Zarzut:** §9 i §12 mówią o skali jakościowo. Bez liczb „budżet" nie
istnieje — JV nauczył nas, że budżet 4 ms/step i licznik shape'ów w bramce
łapią regresję w dniu powstania, nie po miesiącu.

**Propozycja:** każda domena dostaje budżet per-tick w F0 (nawet zgrubny:
fizyka ≤ 4 ms, materiał ≤ 2 ms, przebudowy collidera ≤ 1 ms/tick
amortyzowane, render ≤ 8 ms) + liczniki w bramce (body/shape/chunk count,
bilans masy świata). Budżet wolno ZMIENIĆ świadomą decyzją — nie wolno go
po cichu przekroczyć (dokładnie zasada progów z planu wykonawczego mapy).

### K8. Persystencja zmutowanego świata — niedoszacowany nowy problem

**Zarzut:** wizja dokładnie opisuje bilans materii w RUNTIME (§3.3), ale
zapis/odczyt świata po godzinie robót ziemnych (delta terenu, stan warstw,
materiał w maszynach, historia operacji) pojawia się tylko pośrednio w
tabeli ryzyk modelu drogi. To jest osobna, twarda domena — i pierwsza,
w której „magiczne znikanie" (złamanie §3.1-Bilans) wejdzie niezauważone.

**Propozycja:** sonda bilansu masy świata (suma: teren + luźny + w maszynach
+ w buforach = const ± jawne operacje) w walidatorze od X1; format zapisu
świata wersjonowany od v1 z sondą round-trip (dokładnie wzorzec configu JV).

### K9. Skala lotnictwa: silnik już ma odpowiedź — trzeba podjąć decyzję świadomie

**Twardy fakt (sprawdzone 2026-07-15):** box3d ma oficjalną opcję
`BOX3D_DOUBLE_PRECISION` („Enable double precision for large worlds", OFF
domyślnie; `b3Pos` staje się double, rotacje zostają float — wzorzec DMat44
Jolta). Pytanie 14.4.5 wizji ma więc gotową ścieżkę bez własnego origin
shifting.

**Propozycja (decyzja founderska D10):** rozstrzygnąć w F0 po benchmarku
(koszt double vs float na scenie referencyjnej). Rekomendacja wstępna: ON
od początku — zmiana później dotyka ABI, formatu zapisu i determinizmu
replay, czyli trzech najdroższych rzeczy naraz. Render i tak musi być
camera-relative (standard przy dużych światach).

### K10. Czy CraftGraph nie stanie się monolitem? (pytanie Sol 15.3.1)

**Odpowiedź z dowodem:** nie stanie się, jeżeli od dnia 1 będzie tym, czym
w VAW były „separated graphs" (filar 7): **kilka osobnych grafów dzielących
trwałe ID węzłów** — strukturalny (części/połączenia nośne), mechaniczny
(jointy/aktuatory), energetyczny (paliwo/prąd/hydraulika), sygnałowy
(sensory/kontrolery), materiałowy (bufory/porty zasobów). Pytanie 14.1.3
(„połączenie przenoszące siły, energię, płyny i sygnały naraz") ma wtedy
naturalną odpowiedź: to nie jest jedna krawędź w jednym grafie — to kilka
krawędzi w kilku grafach, współdzielących parę endpointów `{partId, portId}`
(dokładnie kontrakt portów Gate D z VAW). Nasz `ConstructionKernel` z planu
ULTIMATE to embrion tego rdzenia — nazewnictwo ujednolicamy do CraftGraph.

### K11. Drobniejsze uwagi

- **§5.2 „voxel ≠ sześcian"** — świetna zasada, ale nazwa projektu/opisu
  powinna w ogóle porzucić słowo „voxel" (dokument sam to sugeruje w §1);
  proponuję mówić o „kolumnach materiału" i „polu materii".
- **§7.2 gąsienice jako ciągły patch kontaktowy** — zgoda, ale uwaga z JV:
  to samo zrobiliśmy z kołem (split envelope) i lekcja brzmi: model
  zastępczy musi mieć JEDNO źródło prawdy geometrii, inaczej debug-render
  kłamie (asymetria L/R w M8 była błędem renderu, nie fizyki).
- **§10 destrukcja** — zgoda na poziomy; dopisać jawnie: poziom 2
  (odłączane komponenty) wymaga PropRegistry/ownership zanim powstanie
  (lekcja planu E5 mapy: spawn/reset/ownership przed zabawkami).
- **§15 (brief dla Sol)** — ta analiza realizuje ten brief; kolejne
  iteracje wizji powinny adresować K1–K10 zamiast czekać na „jeszcze
  jedną głęboką analizę". Ryzykiem meta jest **paraliż analityczny**:
  dokument sam mówi „nie zastępuje eksperymentów" (§0.2) — więc następnym
  krokiem po tej analizie jest X1, nie kolejny dokument.

---

## 3. Program eksperymentów pionowych (skorygowany, z kryteriami)

Zasada: jeden eksperyment = jedno sprzężenie = kryteria sukcesu/porażki
ustalone PRZED implementacją (prawo z planu wykonawczego mapy). Kolejność
wynika z ryzyka, nie z atrakcyjności.

| X | Sprzężenie dowodzone | Kryteria (szkic) | Status |
|---|---|---|---|
| X0 | blueprint→kompilator→box3d→jazda, determinizm | hash run-do-run identyczny; rover jedzie | **ZALICZONY** (`spike/kernel_v0`, 2026-07-15) |
| X1 | „Żywy grunt": operacja narzędzia → kolumny → przebudowa chunk-collidera pod maszyną | 0 utraty kontaktów kół; bilans masy Δ=0; rebuild < 1 ms/chunk; brak impulsów-widmo | NASTĘPNY |
| X2 | praca przechodzi przez maszynę: opór łyżki → ramię → limit aktuatora; ładunek zmienia masę/COM na żywo | siła na jointach spójna z oporem; maszyna przeciąża się zgodnie z §7.5; stabilność solvera przy zmianie masy | po X1 |
| X3 | stan nawierzchni → fizyka jazdy: luźny vs zagęszczony; walec zmienia stan | mierzalna różnica trakcji/oporu (telemetria JV); N przejazdów walca → malejący efekt | po X1 (równolegle z X2) |
| X4 | powietrze na tym samym CraftGraphie: powierzchnie aero + start z pasa zbudowanego w X3 | samolot z części lata; jakość pasa wpływa na start (tabela §11.1) | po X2+X3 |
| X5 | pętla logistyczna: załaduj→przewieź→wysyp z pełnym bilansem | masa śledzona end-to-end; cut/fill liczony (§5.9) | po X2 |
| X6 | pierwszy sygnał: sensor→kontroler→aktuator bez skryptu per-pojazd | ten sam kontroler działa na 2 różnych maszynach | po X2 |

Fun-gate obowiązuje od X1: „czy nabranie pełnej łyżki daje satysfakcję?"
jest kryterium odbioru, nie miłym dodatkiem (K3).

---

## 4. Interfejs: decyzja Jozza (2026-07-15) i jej konsekwencje

**Decyzja:** nawigacja i interfejs wzorowane na **Blenderze i Unreal
Engine**; z Godota bierzemy to, co najlepsze (bez ignorowania) — ale to
wzorce Blender/UE są podstawą.

Konsekwencje praktyczne (do ADR UI w F0):

- **Viewport:** orbit na MMB, pan Shift+MMB, zoom scroll; `F` = frame
  selected; tryby kamery fly/walk (UE-style RMB+WASD) w widoku operacyjnym;
  siatka i gizmo orientacji jak w Blenderze.
- **Manipulacja:** gizma translate/rotate/scale (klawisze G/R/S jako
  akceleratory — wzorzec Blender), snap do siatki/portów, numeryczny input
  wartości podczas transformacji.
- **Struktura ekranu:** workspaces (Build / Operate / Inspect — odpowiednik
  layoutów build/flight z VAW Workbench v4, ale w konwencji zakładek
  Blendera), panel właściwości po prawej (N-panel/Details), outliner/drzewo
  konstrukcji po lewej (UE World Outliner), konsola diagnostyczna na dole.
- **Technologia:** panele = Dear ImGui (docking); viewport i gizma = nasze
  (sokol). ImGui docking dobrze niesie ten wzorzec — dowód: workbenche
  ImGui w narzędziach branżowych + nasz własny system zakładek M6.
- **Uczciwa granica:** Blender to dekady dopracowania — kopiujemy WZORCE
  nawigacji i rozmieszczenia, nie zakres. Pierwsza wersja = viewport +
  3 workspaces + property panel + outliner. Nic więcej.

---

## 5. Werdykt końcowy

Wizja v0.1 jest **przyjęta jako konstytucja produktu z poprawkami K1–K10**
(propozycje zasad 11 i 12 do §13; rozstrzygnięcie trylematu drogi per K4;
program eksperymentów per §3 tej analizy). Nie znalazłem w niej sprzeczności
architektonicznej, która wymagałaby redukcji marzenia — znalazłem jeden
niedoszacowany krytyczny punkt techniczny (K2: brak update-in-place
heightfielda w box3d) i jedną strukturalną pustkę (K1: brak gracza), obie
adresowalne bez naruszania rdzenia.

Marzenie jest ogromne — ale jego test jest mały: **X1**. Koparka, która
uczciwie nabiera ziemię z terenu, który uczciwie się obniża, pod maszyną,
która uczciwie stoi na kołach. Wszystko inne w tej wizji czeka na ten jeden
dowód.
