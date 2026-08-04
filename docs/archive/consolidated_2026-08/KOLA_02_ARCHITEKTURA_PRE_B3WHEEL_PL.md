> **ARCHIWUM — nie jest bieżącą instrukcją.** 2026-08-04 treść została scalona lub zastąpiona przez nowy `docs/KOLA_02_ARCHITEKTURA_PL.md` opisujący faktyczną architekturę `b3Wheel`. Plik pozostaje jako zapis historii.

# Koła i opony — architektura i kontrakty

Data: 2026-07-25 | Korekta po audycie: 2026-07-28
Wejście: `KOLA_01_DOWODY_PL.md` (wszystkie liczby stamtąd, statusy §7 `PROVISIONAL`)
Status: **MIESZANY** — kontrakty W1/W3/W4 pozostają propozycją; warstwa W2 ma już pierwszy natywny backend `b3Wheel`, ale nie realizuje pełnego `WheelSpec`/`TireSpec`.


> **Korekta 2026-08-04.** Commity `9800af9…5b92e9c` zaimplementowały pierwszy
> backend W2: obrotowo symetryczny `b3Wheel` z profilem bieżni. Nie oznacza to,
> że cała architektura z tego dokumentu została przyjęta. `WheelSpec`, model
> materiału W3 i wielopoziomowe fidelity nadal są hipotezami. Najbliższy krok
> dotyczy poprawności manifoldu i lokalnej podatności, nie rozbudowy kontraktów.

> **Ten plik NIE jest dziś źródłem prawdy o kierunku.** Powstał przed pomiarami
> z 2026-07-27 i przed audytem z 2026-07-28. Trwałe są w nim: podział na warstwy
> W1–W4, kontrakt `WheelSpec`/`TireSpec`/`WheelState`, `WheelContactSet` (§2.1),
> własność oporu toczenia (§2.2) i drabina zdolności F0–F4 (§6).
> **Nietrwałe i skorygowane:** wszystko, co wskazuje konkretny backend jako
> domyślny (§4.1), wszystkie niezmierzone koszty i wszystkie zdania sugerujące,
> że jakaś rodzina już wygrała. W razie sprzeczności z `KOLA_01` §7 i `KOLA_04`
> — wygrywa `KOLA_01`.

---

## 1. Czego ta architektura musi dowieźć (z wizji właściciela)

Jozz, 2026-07-25, dosłownie: *„nie jestem typem jednego tematu"*. System ma
obsłużyć **trzy światy naraz**, nie wybrać jednego:

| Świat | Charakter opony | Co musi być parametrem |
|---|---|---|
| **Drift / tor twardy** | wąska, sztywna, przegrzewalna, płaski bieżnik | niska podatność, płaska korona, temperatura, degradacja μ |
| **Ciężki offroad skalny** | wielka, miękka, „wtula się w kamienie i krawędzie" | wysoka podatność, mocno wypukła korona, niskie ciśnienie, konformowanie do krawędzi |
| **Zwykła jazda / roleplay** | przewidywalna, spokojna, tania | środek zakresu, niski koszt CPU |

Dodatkowe wymagania właściciela:

- felga i opona będą **osobnymi assetami** (dopasowywanie felg do opon po wymiarach);
- pierwszy target to 4 koła jednego auta, ale **architektura nie zakłada jednego auta**;
  koszt mierzymy **per koło**, z kilkoma poziomami wierności;
- system zniszczeń **nie jest teraz budowany**, ale dane i interfejsy muszą już
  umieć reprezentować: ciśnienie, uszkodzenie, oddzielenie opony od felgi,
  degradację struktury i zmianę parametrów w runtime.

**Wniosek architektoniczny nr 1:** nie projektujemy „opony". Projektujemy
**rodzinę opon parametryzowaną danymi**, w której trzy światy Jozza to trzy
punkty w tej samej przestrzeni parametrów. Każda decyzja, która zabetonuje
jeden punkt, jest błędem.

---

## 2. Cztery warstwy, cztery kontrakty

```
┌─ W1  WheelSpec / TireSpec ────────────────── CZYSTE DANE, zero fizyki
│      opisuje koło; nie wie nic o Box3D
│      ŹRÓDŁO: markery assetu (dowolny model) + preset + runtime override
│      ODPOWIADA ZA: wymiary, masę, bezwładność, materiał, stan (ciśnienie/uszkodzenie)
│
│      kontrakt w dół:  WheelSpec  ->  (masa, bezwładność, obrys, materiał)
├─ W2  Reprezentacja kolizji ────────────────── STRATEGIA, wymienna w runtime
│      buduje shape'y na ciele koła; NIE liczy sił opony
│      kontrakt w górę:  WheelContactSet (patrz §2.1) - ZBIÓR kontaktów
│                        plus agregat, nie pojedynczy punkt
├─ W3  Prawo opony ─────────────────────────── ORTOGONALNE do W2
│      konsumuje agregat ALBO pełny zbiór, zależnie od poziomu wierności;
│      nie wie, jaki shape je wygenerował
│      kontrakt w górę:  siły/momenty + stan (temperatura, zużycie)
└─ W4  Warstwa wizualna ────────────────────── maszyneria JUŻ ISTNIEJE
       modele Jozza, deformacja per-część, opona i felga osobno
       kontrakt w dół: czyta WheelSpec + stan; NIGDY nie steruje fizyką
```

**Reguła świętości:** W2 i W3 wolno zmieniać **tylko osobno**. Zmiana obu naraz
w jednym eksperymencie jest zakazana (`KOLA_00` reguła 3). Powód jest zmierzony:
w `KOLA_01` §5.3 pięć wariantów geometrii dało pięć różnych rankingów w zależności
od metryki — dorzucenie drugiej zmiennej uczyniłoby wynik nieinterpretowalnym.

### 2.1 `WheelContactSet` — kontrakt neutralny wobec backendu

Poprzednia wersja miała `ContactPatch { normalna, obciążenie, punkt, poślizg,
kierunek }`, czyli **zakładała jedną plamę i jeden punkt**. Program bada wiele
manifoldów, rozkład impulsów, kilka powierzchni naraz i footprint — kontrakt
nie może tego wykluczać z góry.

```text
WheelContactSet
  contacts[]                 // po jednym na punkt manifoldu
    worldPoint
    wheelLocalPoint          // GDZIE na oponie: korona / bark / bok
    normal
    separation
    normalImpulse
    tangentialImpulse
    material
    otherShape / otherBody
    featureIdentity          // do sledzenia tozsamosci miedzy krokami
    persistence              // czy przetrwal warm start
  aggregate
    resultantForce / resultantMoment
    effectiveRadius          // INNY na koronie, barku i boku - patrz §7
    loadCentroid
    longitudinalFrame / lateralFrame
```

Prawo opony (W3) konsumuje **agregat** przy niskiej wierności i **pełny zbiór**
przy wysokiej. To jest jedyny szew, który musi przetrwać zmianę backendu.

### 2.2 Właściciel oporu toczenia — jedna odpowiedź, cztery pytania

Stockowy `rollingResistance` obsługuje tylko wybrane prymitywy. Pytanie „czy da
się do niego podpiąć nową bryłę" trzeba rozbić, bo techniczna łatwość nie
dowodzi poprawności modelu:

```
API:        czy material/shape moze w ogole przekazac parametr?
SOLVER:     wokol jakiej osi i przy jakim limicie dziala rolling impulse?
GEOMETRIA:  jaki jest promien efektywny na koronie, barku i boku?
            (przy przejsciu korona-bark-bok zmienia sie ramie momentu)
TIRE LAW:   czy opor jest juz liczony w W3 - zeby nie liczyc energii dwa razy?
```

**Reguła:** opór toczenia ma dokładnie jednego właściciela w danej konfiguracji
i musi to być zapisane w presecie. Test wykrywający podwójne liczenie: bilans
energii przy `rollingResistance` OFF/ON i prawie opony OFF/ON — cztery przebiegi,
suma strat musi się zgadzać. Bez tego testu nie wolno włączyć obu naraz.
Status: `TECHNICALLY PLAUSIBLE, PHYSICALLY AND ARCHITECTURALLY OPEN`.

---

## 3. W1 — kontrakt `WheelSpec` (szkic docelowy)

Podział na trzy niezależne bloki, bo tak wygląda przyszłość „felga osobno od opony":

```text
RimSpec                      // felga - osobny asset
  rimRadius                  // m, promień obręczy (osadzenie stopki opony)
  rimWidth                   // m, szerokość obręczy
  rimMass                    // kg
  boltPattern / offset       // przyszłość, dopasowywanie felg
  assetId                    // model wizualny

TireSpec                     // opona - osobny asset
  innerRadius                // m, musi pasować do rimRadius (walidacja dopasowania)
  outerRadius                // m, promień zewnętrzny
  sectionWidth               // m, szerokość przekroju
  macroProfile               // OPIS POTRZEBY, nie geometrii konkretnego backendu:
                             // próbkowany przekrój albo nazwana rodzina profilu.
                             // NIE wpisywać tu wykładnika superelipsoidy ani
                             // innego pola zdradzającego wybraną bryłę - backend
                             // ma się dopasować do kontraktu, nie odwrotnie.
  shoulderRadius             // m, promień barku (0 = ostra krawędź) - deklaracja
                             // potrzeby; backend może jej NIE umieć spełnić
                             // i musi to jawnie raportować jako swoją zdolność
  carcassMass                // kg
  compound                   // mieszanka: mu_nom, wrażliwość na obciążenie,
                             //            wrażliwość na temperaturę, zużycie
  nominalPressure            // kPa
  stiffness { radial, lateral, torsional }   // podatność struktury
  assetId

WheelState                   // STAN RUNTIME - to jest miejsce na przyszłe zniszczenia
  pressure                   // kPa, zmienialne w runtime (upuszczanie powietrza!)
  temperature                // K, per pas bieżnika w przyszłości
  wear                       // 0..1
  structuralDamage           // 0..1
  beadSeated                 // bool - opona na feldze czy zeszła (debeading)
  puncture { rate }          // przeciek; 0 = brak
```

**Dlaczego tak, mimo że zniszczeń nie budujemy teraz:** `WheelState` jest osobną
strukturą od `Spec`, więc dodanie przebicia w przyszłości **nie zmienia żadnego
kontraktu** — zmienia tylko, kto czyta `pressure`. Gdyby ciśnienie siedziało
w `TireSpec`, każda zmiana runtime wymagałaby przebudowy koła.

**Wyliczane z kontraktu, nie z collidera (naprawa F-02):**

```text
mass    = rimMass + carcassMass
I_spin  = I_rim(pierścień) + I_carcass(pierścień na outerRadius)
I_trans = 0.5 * I_spin + poprawka na szerokość
```

Ustawiane jawnie przez `b3Body_SetMassData`. **Od tego momentu zmiana strategii
W2 nie zmienia bezwładności ani o gram** — i dopiero wtedy porównania kształtów
cokolwiek znaczą.

**Blocker do usunięcia:** `samples/jozz_vehicle_asset_dimensions.cpp` ma na sztywno
wpisane `"Offroad_Big_Wheels.gltf"` i nazwy markerów. Dopóki tak jest, „dowolny
rozmiar koła" nie istnieje. `WheelSpec` musi powstawać z **dowolnego** assetu
po konwencji markerów, z walidacją i czytelnym błędem przy braku markera.

---

## 4. W2 — reprezentacja kolizji: co jest realnie na stole

`KOLA_01` §6 dowiódł: w stockowym Box3D **nie ma** reprezentacji jednocześnie
gładkiej i o poprawnej szerokości. Kandydaci, z zmierzonym statusem:

| # | Kandydat | Gładkość | Szerokość | Bark | Patch | Core patch | Status |
|---|---|---|---|---|---|---|---|
| S0 | sfera | idealna | **NIE** (+0.295 m) | — | 1 pkt | nie | odrzucona przez właściciela |
| S1 | split sfera+cylinder | pozorna | pozorna | nie | 1–4 | nie | **MARTWA** (F-04) |
| S2 | pryzmat N≤42 | zła (F-03) | tak | nie | do 4 | nie | odrzucona jako cel |
| S3 | hull z barkiem N≤18 | najgorsza | tak | tak | do 4 | nie | odrzucona (F-01) |
| S4 | elipsoida (skalowana sfera) | idealna | tak | wymuszony | 1 pkt | TAK, klasa X | **JEDEN Z KANDYDATÓW** |
| S4b | analityczny swept-disk (Rc⊕odcinek⊕r) | idealna | tak | sterowalny | linia | TAK, klasa X | **JEDEN Z KANDYDATÓW** |
| S5 | compound pasm w poprzek | ? | tak | tak | wiele | nie | **NIEZBADANY** |
| S6 | miękki pierścień (wiele ciał) | ? | tak | tak | wiele | prawdop. nie | **NIEZBADANY, odblokowany przez F-07** |

### 4.1 S4 — elipsoida jako rozszerzenie istniejącej sfery (JEDEN Z KANDYDATÓW)

> **KOREKTA 2026-07-26, decyzja właściciela.** Ta sekcja była napisana jako
> propozycja wiodąca. Ten status został **cofnięty**. Elipsoida jest jednym
> z kilku kandydatów do zbadania i nie jest podstawą żadnego kontraktu,
> drabiny wierności ani polityki forka.
>
> **Twardy limit geometryczny odkryty po napisaniu tej sekcji (fakt matematyczny,
> nie opinia):** przekrój elipsoidy o półosiach (R, W/2) ma promień krzywizny
> korony `ρ = (W/2)² / R`. Dla obecnego koła (R=0.514, W=0.4375) daje to
> **ρ = 9.3 cm** — koronę ostrzejszą niż opona motocyklowa. Bok tej samej
> elipsoidy ma `R²/(W/2) = 1.21 m`, czyli jest niemal płaski.
> **To jest odwrócony profil względem prawdziwej opony** (płaska korona,
> zaokrąglone barki). Maksymalna możliwa płaskość korony elipsoidy to promień
> kuli — czyli elipsoida NIE MOŻE być bardziej płaska niż sfera.
> Skalowana sfera ma **zero swobodnych parametrów profilu**.

Obserwacja: jedyne bryły w Box3D o **zerowym biciu** to sfera i kapsuła —
bo ich funkcja wsparcia jest dokładna. Sfera ma promień `R` we wszystkich
kierunkach; dlatego wystaje bokiem.

Elipsoida o półosiach `(R, W/2, R)` — gdzie lokalne Y jest osią obrotu koła —
ma **dokładnie promień R w płaszczyźnie toczenia** (zero bicia, jak sfera)
i **dokładnie W/2 wzdłuż osi** (poprawna szerokość opony). Barki są z definicji
zaokrąglone.

> **KOREKTA 2026-07-28: elipsoida i superelipsoida to DWIE RÓŻNE RODZINY
> i dwa różne patche.** Ta sekcja mieszała je w jedno. Wektor skali na `b3Sphere`
> daje **elipsoidę**, a nie superelipsoidę. Superelipsoida wymaga dodatkowego
> wykładnika i innej procedury wsparcia (`h(d) = ((R·s)^q + (b·|d·a|)^q)^(1/q)`,
> `1/p + 1/q = 1`) — zamkniętej analitycznie, ale **nie tej samej**.
> Zmierzone konsekwencje obu: `KOLA_01` §7.7 (P-07, P-08).
>
> **Elipsoida REAGUJE na camber** geometrycznie: wysokość jazdy się zmienia,
> a punkt podparcia wędruje w stronę boku (1.6 mm/stopień). Wcześniejsze zdanie
> „nie ma reakcji na camber" było nieprecyzyjne. Czego elipsoida **nie** daje:
> realistycznego footprintu, camber thrust, rozkładu nacisku, pneumatic trail.

```text
       sfera R=0.514            elipsoida (0.514, 0.219, 0.514)
      ,--------------.                   ,-----.
     /                \                 /       \      <- ta sama krzywizna toczenia
    |        o         |               |    o    |     <- prawidłowa szerokość
     \                /                 \       /
      `--------------'                   `-----'
   toczy się idealnie              toczy się idealnie
   szeroka na 1.03 m               szeroka na 0.44 m
```

**Poniższy szkic implementacyjny jest WYCOFANY jako domyślny** (2026-07-28).
Zostaje wyłącznie jako zapis rozważanego wariantu, bo `KOLA_03` §5.1 klasyfikuje
go jako patch klasy **`X`**, a nie „tani generyczny szew". Zmiana publicznego
układu `b3Sphere` dotyka serializacji (`src/world_snapshot.c:489` zapisuje
strukturę bajtowo), ABI i wszystkich procedur sfery — **omijanie 129 miejsc
dispatchu nie czyni tego tanim**. Nie budować tego bez przejścia V1b i jawnej
decyzji właściciela.

```c
typedef struct b3Sphere {
    b3Vec3 center;
    float  radius;
    b3Vec3 scale;   // JOZZ-PATCH: {1,1,1} == stockowa sfera, bit w bit
} b3Sphere;
```

Konsekwencje, po kolei:

- **zero nowych wartości w `b3ShapeType`** → żaden ze 129 `case` sites nie musi
  dostać nowej gałęzi, format nagrań nie przesuwa indeksów;
- dotykamy **5 procedur** (sfera×sfera, kapsuła×sfera, hull×sfera, mesh×sfera,
  heightfield×sfera), nie 7 nowych;
- **`rollingResistance` dalej działa** (jest „tylko dla sfer i kapsuł") — pułapka
  z `KOLA_01` §3.4 znika sama;
- przy `scale = {1,1,1}` ścieżka musi przejść Zero-Delta-Off (`KOLA_03` §4)
  — testowalne maszynowo, ale dla klasy `X` punkt 4 (identyczność pliku)
  prawdopodobnie nieosiągalny;
- klasa merge: **`X`, nie `E`** (korekta wobec `KOLA_03` §5.1). Przeznaczenie
  patcha (`E` = przydatny każdemu) to inna oś niż koszt utrzymania.

**Uczciwe ryzyka S4 (do rozstrzygnięcia eksperymentem, nie deklaracją):**

1. Procedury sferyczne w Box3D są **zamknięte analitycznie** (rzut środka,
   clamp). Elipsoida nie jest. Standardowa technika: przejść do przestrzeni,
   w której elipsoida jest sferą jednostkową (skalowanie niejednorodne), znaleźć
   tam punkt najbliższy, wrócić — z **normalną przez odwrotność transponowaną**
   i **korektą separacji**, bo odległości w przestrzeni skalowanej nie są
   euklidesowe. To jest miejsce, gdzie patch może się okazać dwa razy droższy
   niż wygląda. **Do zmierzenia w spike'u S4-SPIKE przed decyzją.**
2. Elipsoida daje **jeden punkt kontaktu**, tak jak sfera. Reaguje na camber
   geometrycznie (zmiana wysokości jazdy i wędrówka punktu podparcia), ale nie
   daje footprintu, rozkładu nacisku, camber thrust ani pneumatic trail.
   To jest sufit tej rodziny.
3. Mesh terenu ze skanu ma ostre krawędzie trójkątów; zachowanie elipsoidy na
   krawędzi (ghost contacts) jest niezbadane.
4. Bezwładność liczona z objętości elipsoidy jest inna niż ze sfery — nieistotne,
   bo po naprawie F-02 bezwładność i tak pochodzi z `WheelSpec`.

### 4.2 S5 i S6 — dlaczego wracają na stół

Pomiar F-07 (koszt kolizji ~2 µs/koło, faseta darmowa, koszt rośnie z **liczbą
shape'ów**, nie ich złożonością) zmienia ocenę rodziny wielociałowej:

- ~~16 × 2 µs ≈ 32 µs/koło, 4 koła ≈ 130 µs = 0.8% klatki~~
  **WYCOFANE 2026-07-28 jako ekstrapolacja niepoparta pomiarem.** Zmierzono
  koszt *shape'ów*, nie kompletnej struktury (ciała + jointy + ciśnienie +
  substepy + wyspa solvera). Pomiar `union-4` na meshu — 12.19 µs/koło, ponad
  6× sfera — pokazuje, że mnożenie liniowe zaniża. Koszt miękkiego pierścienia
  pozostaje **nieznany** (`U-05`) i musi być zmierzony na kompletnym prototypie.
- S6 jest jedyną rodziną, która daje Jozzowi *„wielkie miękkie opony wtulające się
  w kamienie i krawędzie"* w sposób emergentny, a nie udawany.
- S6 rozwiązuje też F-01 od drugiej strony: każdy segment jest mały, więc mieści
  się w budżecie hulla z zapasem, a podatność **filtruje** bicie geometryczne.
  W BeamNG opona ma grubszą fasetę niż nasz cylinder-32 i nie ma tarki —
  bo jest miękka.

**Hipoteza kierunkowa (H-01, nieudowodniona):** miękkość nie jest ozdobą na końcu
roadmapy, tylko **alternatywnym rozwiązaniem problemu fasety**. Falsyfikator:
pierwszy pierścień strukturalny, który jest stabilny, ale nadal pokazuje
agitację pionową porównywalną z pryzmatem.

---

## 5. W3 — prawo opony

Kolejność wchodzenia, od najtańszego do najdroższego. **Nie wchodzimy w kolejny
poziom, dopóki poprzedni nie ma zmierzonego zakresu użyteczności.**

| # | Model | Co daje | Core patch |
|---|---|---|---|
| T0 | Coulomb izotropowy (dziś) | baseline | nie |
| T1 | baza styczna opony + elipsa tarcia | różnica wzdłuż/w poprzek, charakter | **TAK, mały, klasa E** |
| T2 | krzywa poślizgu + wrażliwość na obciążenie | szczyt przyczepności, zerwanie | zależy od T1 |
| T3 | brush / rozkład w plamie | pneumatic trail, moment samonastawny | wymaga W2 z wieloma punktami |
| T4 | termika + zużycie | przegrzewanie opony driftowej | nie |

**T1 w jednym zdaniu:** solver liczy jedno centralne tarcie w bazie
`tangent1 = b3Perp(normal)` — arbitralnej. Jeśli kontakt może nieść **własny
kierunek wzdłużny** i **dwa współczynniki**, koło tarcia staje się **elipsą
tarcia**. To jest różnica między „auto się ślizga" a „opona ma charakter".

**Zastrzeżenie przyjęte od Second Brain:** T1 **nie jest** pełnym prawem opony.
Nie daje dynamiki przejściowej, relaksacji, historii deformacji ani rozkładu
nacisku. Jest kontrolą i rozszerzeniem, nie modelem. T3 wymaga W2 z prawdziwym
footprintem.

> **Korekta 2026-07-28.** Zdanie „pełny charakter opony wymaga miękkiej opony"
> było za mocne i zostaje wycofane. Część zachowań — krzywe poślizgu, elipsa
> tarcia, wrażliwość na obciążenie, termika driftu, moment samonastawny w wersji
> półempirycznej — da się dostać **prawem opony bez pełnej deformacji**.
> Podatności/struktury wymagają w sposób twardy: ciężki offroad, konformowanie
> do skał i realny rozkład nacisku w plamie.

**Ostrzeżenie o podwójnym tarciu (FAIL-SOL-001):** żadna implementacja W3 nie
dokłada siły z zewnątrz przy aktywnym tarciu stockowym. Albo kształtujemy tarcie
w solverze (T1), albo jawnie zerujemy stockowe i liczymy własne — nigdy oba.

---

## 6. Drabina wierności (odpowiedź na „kilka poziomów fidelity")

Ta sama `WheelSpec`, różny koszt i różna wierność. Wybierana per pojazd,
docelowo automatycznie po dystansie/ważności.

> **PRZEBUDOWA 2026-07-28.** Poprzednia drabina wpisywała konkretne backendy
> (elipsoida jako L1/L2, miękki pierścień jako L3) i niezmierzone koszty — to
> była **ukryta decyzja architektoniczna** przebrana za tabelę. Drabina opisuje
> teraz **zdolności**, nie implementacje. Koszty: `TBD` do czasu pomiaru.

```text
F0  BASELINE            dzisiejsza sfera + Coulomb                          TBD
F1  RIGID MACRO-CONTACT poprawny obrys i szerokosc; kontakt z kazda
                        powierzchnia i orientacja; jawna masa/bezwladnosc   TBD
F2  F1 + PRAWO OPONY    charakter: poslizg, elipsa tarcia, wrazliwosc
                        na obciazenie, termika jako stan                    TBD
F3  PODATNOSC /         ugiecie, konformowanie, footprint zalezny
    ENVELOPING          od obciazenia i cisnienia                           TBD
F4  STRUKTURA /         debeading, uszkodzenie, kontakt felgi,
    USZKODZENIA         przejscia miedzy stanami                            TBD
```

Który backend realizuje który poziom — **jest pytaniem otwartym** i to jest
przedmiotem iteracji R1–R5 w `KOLA_04`. Jedno wiemy: F1 nie musi być tą samą
rodziną co F3.

**Trzy światy a poziomy — hipoteza, nie ustalenie.** Robocze przypuszczenie:
drift i zwykła jazda żyją głównie w F2 (prawo opony), a ciężki offroad wymaga
F3. To jest teza do **rozstrzygnięcia iteracją R2** (atlas zjawisk trzech
światów), a nie przesłanka do planowania kolejności prac. Zdanie „trzy światy
to trzy punkty w tej samej przestrzeni parametrów" jest dobrą hipotezą dla
wspólnego **kontraktu danych** (`TireSpec`), ale nie dowodem, że dzielą jedną
ciągłą rodzinę implementacyjną: mogą dzielić `TireSpec`, a wymagać różnych
backendów i różnych stanów wewnętrznych.

---

## 7. Co ta architektura świadomie odkłada

```text
- deformowalny grunt (koleiny, sinkage) - osobny program, kontrakt tire-terrain
  ma być gotowy, ale nie implementowany
- drivetrain, dyferencjały
- pełny model zniszczeń - tylko reprezentacja w WheelState
- nadwozie jako wiele ciał - jawnie poza zakresem (decyzja właściciela)
- per-voxel bieżnik w fizyce - dowiedzione niemożliwe (F-01), zostaje wizualny
```
