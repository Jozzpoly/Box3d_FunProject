# M5.1 Feel Tuning — Handoff po pierwszym ręcznym teście Jozza

Data: 2026-07-05
Branch: `jozz-vehicle-sandbox-m0`
Status: M5 First Drivable działa, Jozz przejechał ~20 min testów. Ten dokument to handoff/plan na kontynuację w TEJ SAMEJ rozmowie po odnowieniu limitów. Nic z poniższego nie zostało jeszcze zaimplementowane — to analiza + plan, zgodnie z wyraźnym poleceniem Jozza "nie naprawiaj niczego na razie".

Czytaj też: `docs/M5_FIRST_DRIVABLE_PL.md`, `docs/adr/0005-m5-first-drivable-before-m4c.md`.

## 1. Surowy feedback Jozza (2026-07-05, po ~20 min jazdy)

Co działa:
```text
zawieszenie działa całkiem dobrze
hamulce działają
suwaki wpływają na zachowanie (ale zakresy za małe do stress-testów)
ogólnie: "mega zadowolony", pierwsze 20 min frajdy pomimo niedoskonałości
```

Problemy:
```text
1. chassis (klocek) zbyt szeroki, może nawet o połowę węższy
2. sterowanie A/D odwrócone (lewo/prawo)
3. skręcanie kołami: ciężkie, nieintuicyjne, czasem w ogóle nie skręca,
   czasem tylko jedno koło; przy małej prędkości prawie niemożliwe;
   przy większej prędkości nagle działa; gdy auto przewrócone albo
   koło w powietrzu - skręca swobodnie
4. przy większej prędkości: dziwna niestabilność kół, "teleportowanie",
   przeskakiwanie - wymaga głębszego zbadania
```

Życzenia (dla zabawy/eksperymentowania):
```text
teren podstawowy 2x większy
więcej ramp/przeszkód/nierównego terenu
kilkanaście fizycznych obiektów do interakcji rozmieszczonych po mapie
```

## 2. Co jest zwalidowane i NIE WOLNO przypadkiem zepsuć

- Model rest-anchor M2.4/M2.5 (Frame A/Frame B, spring rest = translation 0) — Jozz go nie kwestionuje, problem jest w warstwie steering/tuning, nie w rdzeniu joint modelu.
- `suspensionHertz = 6.0f` (podniesione z 2.5 podczas tej sesji po headless smoke) — Jozz potwierdził w praktyce, że zawieszenie działa dobrze. NIE cofać tej zmiany bez powodu.
- Hamulec działa dobrze w praktyce — nie dotykać `brakeTorque`/logiki hamowania bez wyraźnego powodu.
- Corner lab M2 (`Lab M2 Primitive Corner`) — nietknięty, nie mieszać zmian M5 z jego kodem.

## 3. Krytyczna analiza problemów (hipotezy, priorytety, dowody)

### 3.1 Chassis zbyt szeroki — POTWIERDZONE liczbowo, tania naprawa

Obecne stałe w `JozzVehicleM5DefaultConfig` (`samples/jozz_vehicle_m5_vehicle.cpp`):

```text
chassisHalfExtents = { 1.55, 0.35, 0.80 }   -> pełna szerokość 1.60 m
trackHalfWidth = 1.05                       -> pełny rozstaw kół 2.10 m
wheelWidth ~= 0.4375                        -> połowa ~= 0.22 m
```

Wewnętrzna ściana opony: `1.05 - 0.22 = 0.83 m`. Krawędź nadwozia: `0.80 m`.
Różnica to tylko **3 cm** — nadwozie praktycznie styka się z oponą, zero
"nadkola". To wyjaśnia wrażenie "klocek zbyt szeroki" bez potrzeby zgadywania.

Propozycja startowa dla następnej sesji (do eksperymentowania, nie sztywna):
`chassisHalfExtents.z` ~ 0.55-0.62 (zamiast Jozzowego "o połowę" = 0.40, które
zostawiłoby chyba zbyt duży prześwit w drugą stronę). Do stress-testu i tak
potrzebny będzie live slider (patrz 3.5).

### 3.2 A/D odwrócone — potwierdzić w OBU trybach kamery przed naprawą

Kod:
```cpp
// jozz_vehicle_m5_drivable_lab.cpp Step()
if (IsKeyDown(KEY_A)) input.steer += 1.0f;
if (IsKeyDown(KEY_D)) input.steer -= 1.0f;

// jozz_vehicle_m5_vehicle.cpp UpdateJozzVehicleM5Drive()
float targetSteering = -maxAngle * input.steer;  // komentarz: steer=+1 = lewo
```

Headless smoke (`RunM5DriveSmoke` w `jozz_vehicle_validation.cpp`) ustawia
`input.steer = 1.0f` i mierzy `headingDelta = +1.232 rad`, co INTERPRETOWAŁEM
jako "w lewo". Jozz w praktyce widzi odwrotność. Jeden z nas ma złą definicję
"lewo" (nie wiadomo które: mapowanie klawiszy, znak w module fizyki, czy
interpretacja heading w teście).

Plan naprawy (następna sesja):
1. Zweryfikować empirycznie w GUI który kierunek jest faktycznie "w lewo" z
   perspektywy gracza, w domyślnym widoku ORAZ w trybie third-person (`T`) -
   to może się różnić między trybami kamery, osobno sprawdzić oba.
2. Poprawić dokładnie w JEDNYM miejscu (albo mapowanie klawiszy, albo znak w
   module fizyki - nie w obu, żeby się nie skasowały).
3. Zaktualizować komentarz w kodzie I asercję/interpretację w headless smoke,
   żeby nie rozjechały się ponownie przy następnej zmianie.

### 3.3 Skręcanie ciężkie/nierówne/swobodne w powietrzu — GŁÓWNY temat sesji

**Podważam własną pierwotną hipotezę.** Geometria rigu: oś skrętu jest pionowa
i przechodzi przez `frame A` (rest wheel-center anchor), a Frame B (środek
koła) leży na tej samej pionowej linii również w trakcie kompresji/odbicia
(travel jest czysto pionowy, równoległy do osi skrętu). Więc **promień scrubu
(scrub radius) wychodzi geometrycznie na zero** - to NIE jest problem
offsetu kingpina, jak można by pomyśleć na pierwszy rzut oka.

**Lepsza hipoteza: opór skrętu nieruchomej/wolno jadącej opony o skończonej
powierzchni styku** (realne zjawisko - to dokładnie dlatego w prawdziwych
autach istnieje wspomaganie kierownicy; postój wymaga zaskakująco dużego
momentu). Szacunek rzędu wielkości z obecnych stałych:

```text
masa chassis: density 200 * objętość (2*1.55 * 2*0.35 * 2*0.80) ~= 694 kg
obciążenie na koło: ~700*9.81/4 ~= 1716 N
tarcie wheelFriction = 1.25 -> siła tarcia max ~= 2145 N
moment potrzebny na obrót łapy styku (szerokość opony ~0.4375m,
  współczynnik <1 dla rozkładu nacisku): rząd wielkości 200-500 N·m
```

Obecny `maxSteeringTorque = 80` jest o rząd wielkości za mały względem tego
oszacowania. To spójnie tłumaczy WSZYSTKIE obserwacje Jozza:
- "ciężko przy niskiej prędkości" -> opona "stoi", skrobie całą powierzchnią styku
- "swobodnie w powietrzu / auto przewrócone" -> zero nacisku = zero tarcia = zero oporu (silny dowód, że przyczyna to tarcie o podłoże, nie coś wewnątrz jointu)
- "łatwiej przy prędkości" -> tocząca się opona zmienia kąt znoszenia dużo taniej energetycznie niż stojąca (inny reżim tarcia - toczenie vs skrobanie)
- "czasem tylko jedno koło" -> moment na granicy wystarczalności + drobna asymetria obciążenia (przechył nadwozia, upright assist, nierówność terenu) = jedno koło dochodzi do celu, drugie nie

**Alternatywne/dodatkowe hipotezy do niewykluczenia:**
- asymetria od `uprightAssist` (miękki parallel joint) podczas skrętu -
  sprawdzić z wyłączonym uprightAssist czy asymetria "jedno koło" znika.
- nie jest to problem tylko torque, ale też `steeringHertz`/`steeringDampingRatio`
  - zbyt niska sztywność serwa może nie dogonić celu nawet przy wyższym torque.

**Kluczowa decyzja projektowa dla Jozza (do rozstrzygnięcia na starcie
następnej sesji, NIE cicho zdecydowana przeze mnie):**

```text
Opcja A (bardziej symulacyjna): zostawić skręt jako prawdziwe DOF fizyczne
  napędzane torque, ale zwiększyć steeringHertz/maxSteeringTorque o rząd
  wielkości (np. hertz 15-20, torque 800-1500), żeby serwo "przebijało się"
  przez opór opony szybko. Zachowuje "engine-native steering" jako realną
  fizykę (cel ADR-0003 - testujemy czy b3WheelJoint daje dobry feel).

Opcja B (bardziej arcade): potraktować kąt skrętu jako cel kinematyczny
  (bardzo sztywne serwo, efektywnie natychmiastowe), ignorując realistyczny
  opór opony. Prostsze, bardziej "gamey", traci część realizmu.

Opcja C (hybryda): jak A, plus dodatkowy "power steering assist" - mnożnik
  torque przy niskiej prędkości, malejący z prędkością. Najbliżej realnego
  auta, ale to nowa mechanika, większy zakres pracy.
```

Rekomendacja (do potwierdzenia z Jozzem): zacząć od Opcji A (tania zmiana
dwóch liczb, testowalna w minuty), ocenić feel, dopiero potem rozważać C
jeśli nadal nieintuicyjne.

### 3.4 Niestabilność/teleportowanie przy prędkości — NIE zakładać z góry że to fizyka

Dwie konkurencyjne hipotezy, do rozdzielenia eksperymentalnie PRZED naprawą:

```text
(a) prawdziwy problem solvera: sprzężenie suspension+spin+steering pod
    dużym obciążeniem/poślizgiem, ewentualnie za mało substepów (obecnie 4)
    względem sztywności sprężyny (suspensionHertz=6) przy dużych siłach
(b) artefakt czysto wizualny: mesh koła czyta transform ciała w momencie
    rysowania bez interpolacji; przy ~26 rad/s spinu i przy spadkach FPS
    duży skok orientacji/pozycji między klatkami może wyglądać jak "skok"
    mimo poprawnej fizyki
```

Plan diagnostyczny (tani, w tej kolejności):
1. Użyć wbudowanego nagrywania/replay Box3D (`samples/sample_replay.cpp`,
   feature "Recording and replay") żeby nagrać deterministyczne repro
   niestabilności do analizy offline zamiast zgadywać na żywo.
2. Sprawdzić czy problem znika przy dużej liczbie substepów (np. 8-16
   zamiast 4) - jeśli tak, to wskazuje na (a) niedostateczną konwergencję
   solvera, nie na wizualia.
3. Sprawdzić czy problem koreluje z aktywnym skrętem (być może to ten sam
   mechanizm co 3.3 - duży moment walczący z tarciem przy dużej prędkości
   obrotowej koła może destabilizować kontakt).
4. Dopiero po rozdzieleniu przyczyny - celowana naprawa (nie "strzelanie
   na oślep" w suspensionHertz/substeps/tarcie jednocześnie).

### 3.5 Meta-odkrycie: headless smoke NIE testował zepsutego scenariusza

`RunM5DriveSmoke` w `jozz_vehicle_validation.cpp` steruje kołami DOPIERO PO
6 sekundach pełnego gazu (`input.drive` zostaje na 1.0 przez całą fazę
skrętu - nigdy nie jest wyzerowane przed testem steru). To dokładnie
scenariusz, który Jozz opisuje jako "działający dobrze" (przy prędkości).
Test NIGDY nie sprawdzał skrętu przy postoju/niskiej prędkości - czyli
dokładnie tam, gdzie Jozz znalazł realny problem. To wyjaśnia, czemu
walidator mówił "OK" mimo realnego problemu z jazdą.

**Obowiązkowe dla następnej sesji:** rozszerzyć `RunM5DriveSmoke` o
sub-test "steer while stationary" (zero drive input, sprawdzić czy oba
przednie koła osiągają zbliżony kąt skrętu w rozsądnym czasie), żeby ten
dokładny regres nie mógł się powtórzyć niezauważony. Rozważyć też asercję
symetrii lewe/prawe koło (różnica kątów < tolerancja).

## 4. Konkretny plan działania na następną sesję (priorytety)

Kolejność zaproponowana - taniej/bezpieczniejsze rzeczy najpierw, żeby
szybko oddać Jozzowi lepszą wersję do testów, cięższe śledztwo później:

```text
Krok 0  Rozstrzygnąć z Jozzem decyzję projektową z sekcji 3.3
        (Opcja A/B/C - realizm vs arcade skrętu)

Krok 1  Zwęzić chassis (sekcja 3.1) + zbadać/naprawić znak A/D w obu
        trybach kamery (sekcja 3.2). Szybkie, niskie ryzyko.

Krok 2  Rozszerzyć teren i propsy (życzenia Jozza, sekcja 1):
        - 2x większy grunt (zweryfikować semantykę AddGroundBox przed
          zmianą argumentu - nie zakładać, sprawdzić w kodzie)
        - więcej ramp; rozważyć też strefę nierównego terenu przez
          b3CreateHeightFieldShape/b3CreateWave (wzorzec ze stockowego
          Driving sample) jako alternatywę/uzupełnienie dla ręcznie
          układanych ramp - bardziej organiczne wyboje
        - kilkanaście scatter propsów (mix boxy/sfery/rozmiary), przycisk
          "Reset props" (będą się rozjeżdżać po mapie w testach)
        - architektura: wydzielić do nowego modułu
          samples/jozz_vehicle_m5_test_course.h/.cpp zamiast rozdymać
          jozz_vehicle_m5_drivable_lab.cpp (powtórka lekcji z
          PROJECT_STABILIZATION_AUDIT "Problem A")

Krok 3  Steering torque/hertz tuning wg decyzji z Kroku 0 (sekcja 3.3).
        Przenieść geometrię pojazdu (szerokość, rozstaw osi, promień/
        szerokość koła) na wzorzec pending/Apply znany z corner lab
        (m_editWheelRadius/m_structuralSetupDirty/"Apply rig rebuild"),
        żeby Jozz mógł live-tunować i "stress testować aż się rozwali"
        bez rebuildowania kodu za każdym razem. Rozszerzyć zakresy
        WSZYSTKICH suwaków (drive/suspension/steering) znacznie szerzej
        niż obecnie - to świadomy cel Jozza (szukanie granic modelu).

Krok 4  Rozszerzyć RunM5DriveSmoke o sub-test "steer while stationary"
        (sekcja 3.5) PRZED dalszym tuningiem, żeby mieć automatyczną
        siatkę bezpieczeństwa na ten dokładny regres.

Krok 5  Zdiagnozować niestabilność przy prędkości metodą z sekcji 3.4
        (replay capture, substep eksperyment, korelacja ze skrętem)
        PRZED próbą naprawy - to najcięższy temat, nie zgadywać.
```

## 5. Checklist walidacji (uruchamiać PO KAŻDEJ zmianie z Kroku 1-5)

```powershell
cmake --build --preset windows-debug --target test
cmake --build --preset windows-debug --target jozz_vehicle_validation
cmake --build --preset windows-debug --target samples
build\bin\Debug\test.exe                       # All Box3D tests passed
build\bin\Debug\jozz_vehicle_validation.exe    # m5 drive smoke: ok, OK
build\bin\Debug\samples.exe --sample <M5 index> --frames 300   # 0 sokol errors
```

UWAGA ŚRODOWISKOWA (znaleziona w tej sesji): wrapper `cmd /c "set PATH=&
..."` z historycznej dokumentacji CICHO NIC NIE ROBI w Git Bash - wychodzi
kodem 0 bez budowania. Zawsze wołać `cmake` bezpośrednio i weryfikować
timestamp exe albo świeży output.

Po zmianach w Krokach 1-3: **obowiązkowy ręczny playtest Jozza**, ze
szczególnym naciskiem na:
```text
skręt PRZY POSTOJU (nie tylko w ruchu) - to była ślepa plamka poprzedniego testu
oba kierunki kamery (domyślna i third-person po T)
oba przednie koła skręcają symetrycznie
zachowanie przy większej prędkości na nowym, większym terenie
```

## 6. Nie-cele / przypomnienia

```text
nie dotykać rdzenia Box3D
nie dotykać Lab M2 Primitive Corner (corner lab) - to osobne środowisko
  do tuningu, M5 go nie zastępuje
nie cofać suspensionHertz=6.0 bez powodu - Jozz potwierdził że działa
nie mieszać naprawy A/D z inną zmianą znaku w tym samym commicie -
  osobny, czytelny commit dla łatwego cofnięcia jeśli źle zgadniemy kierunek
nie zgadywać przyczyny niestabilności przy prędkości bez replay/diagnostyki
nie decydować cicho Opcji A/B/C ze sekcji 3.3 - to pytanie do Jozza
zachować pending/Apply pattern przy dodawaniu live-tunable geometrii
  (nie live-mutować body/shape bez rebuildu)
aktualizować docs/HOTKEY_AUDIT_PL.md jeśli zmieni się mapowanie klawiszy
```
