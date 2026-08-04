> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# Audyt fizyki, zawieszenia i prowadzenia — 2026-07-08

Krytyczny audyt systemów pojazdu (fizyka, zawieszenie, skręt, stabilność
połączeń, suwaki) na zlecenie Jozza. Metoda: pełna lektura
`jozz_vehicle_m6_suspension_rig.cpp/.h` (jointy, limity, defaulty, drive),
`jozz_vehicle_m6_rig_lab.cpp` (każdy suwak → realny efekt),
`jozz_vehicle_m5_test_course.cpp` (tagowanie przeszkód). Znaleziska statyczne
z kodu; każde oznaczone, czy wymaga jeszcze weryfikacji empirycznej.
**W tej sesji NIE zmieniono kodu** — to raport + plan.

Legenda: 🔴 krytyczne · 🟠 średnie · 🟡 drobne / na przyszłość.

---

## 1. Ocena ogólna

Fundament jest **architektonicznie zdrowy**: kontrakt hardpointowy (geometria
= dane, nie kod), wahacze jako ciała z zawiasami (jedna gałąź ruchu), coilover
jako jedyne podatne połączenie, back-drivable rack, napęd momentem, ARB jako
uczciwe siły. To pasuje do filozofii projektu (zachowanie z konstrukcji).

**Ale**: lekcja M6 („distance joint ma dwie lustrzane gałęzie rozwiązania")
została naprawiona dla WAHACZY, a **drążek kierowniczy i toe-link nadal są
sztywnymi distance jointami** — i to jest niemal na pewno mechanizm zgłaszanego
„łamania skrętu". Szczegóły w §2.

---

## 2. 🔴 K1 — Mechanizm „złamanego skrętu": martwy punkt drążka + zbyt luźny płot

**Objaw (Jozz):** koło zablokowane pod bardzo dużym kątem po przeciążeniu /
złym najechaniu na bumper; nie wraca bez restartu.

**Mechanizm (z kodu, `_rig.cpp:666-704`, `CreateControlArm:576-596`):**

1. Drążek (przód: rack↔zwrotnica) i toe-link (tył: chassis↔zwrotnica) to
   `b3DistanceJoint` bez sprężyny — sztywny pręt. Distance joint jest
   spełniony w **dwóch** lustrzanych konfiguracjach.
2. Układ ramię kierownicze + drążek ma **martwy punkt (over-center)**: zmierzono
   go już wcześniej przy ackermannFraction=1.0 na ~46° (komentarz w headerze:
   „arm and tie rod within ~6 deg of collinear, steering stuck").
3. Jedyny płot za martwym punktem to twist-limit przegubów kulowych:
   **±70°** (hardcode). Komanda skrętu klamruje się do 32° (rackTravel), ale
   przy przeciążeniu solver może chwilowo przepuścić zwrotnicę dalej.
4. Gdy zwrotnica przeleci przez ~46-55° martwego punktu, drążek „zatrzaskuje
   się" na drugiej gałęzi. Rack ciągnie z pełną siłą 12 kN, ale przez ramię
   o niemal **zerowym ramieniu momentu** — koło klinuje się między drugą
   gałęzią drążka a płotem 70° i **nie ma fizycznej drogi powrotu**.

To jest dokładnie powtórka lekcji M6, jedno ogniwo dalej.

**Fix (P1, najwyższy priorytet):** twist-limit przegubów kulowych liczony z
konfiguracji, per narożnik, zamiast hardcode ±70°:
- przód: `±(maxSteeringAngle + margines ~10°)` — poniżej martwego punktu;
- tył: `±15°` (tylna zwrotnica nie skręca wcale — 70° to nonsens);
- dodatkowo wyliczać kąt martwego punktu analitycznie (domena
  `ComputeJozzVehicleM6RackStroke` — tam gdzie `reach` dobija do clampu) i
  asertować `fence < deadPoint` w walidatorze.
Weryfikacja: test w walidatorze — udar boczny/upadek na jedno koło przednie,
odczyt kąta skrętu po ustabilizowaniu (liczba, nie tylko OK).

**Czemu nie „wzmocnić" drążka:** nie da się — dwie gałęzie to własność
constraintu, nie kwestia sztywności. Płot geometryczny PRZED martwym punktem
to jedyne czyste rozwiązanie (tak jak zawiasy naprawiły wahacze).

---

## 3. 🔴 K2 — Bug: Apply nie przelicza limitu maglownicy

**Kod:** `ApplyPendingStructuralSetup()` (`rig_lab.cpp:345-365`) kopiuje nową
geometrię (steeringArmBack, kingpinOffset, ackermannFraction, długości
wahaczy…) i woła `CreateVehicle()`, ale **nie woła `RecomputeRackTravel()`**.
Przelicza się tylko przy wczytaniu sesji (l. 132) i presetu (l. 408).

**Skutek:** po zmianie suwaków „Geometria wahaczy" + Zastosuj limit skoku
maglownicy pochodzi ze STAREJ geometrii. Komentarz przy `rackTravel` w
headerze opisuje dokładnie ten błąd historycznie: zły limit → koło wewnętrzne
przestrzeliwuje do ~45° — czyli **prosto w martwy punkt z K1**. Suwaki
geometrii nie tyle „nic nie robią", co potrafią aktywnie rozstrajać skręt.

**Fix (P2, 1 linia):** `RecomputeRackTravel()` w `ApplyPendingStructuralSetup`
przed `CreateVehicle()`. Regresja w walidatorze: zmiana steeringArmBack →
przebudowa → kąt skrętu przy pełnym locku ≤ limit.

---

## 4. 🔴 K3 — Prześwit sprzężony z twardością (w złą stronę)

**Kod:** `ApplySuspensionTuning` (`rig_lab.cpp:677`) i tworzenie coilovera:
`restLength = design + preload * scale`, gdzie `scale` = mnożnik twardości osi.

**Fizyka:** ugięcie statyczne x = F/k, a k rośnie ~kwadratowo z hertz·scale.
Żeby auto stało w pozie projektowej, preload musi być RÓWNY ugięciu — czyli
przy większej twardości potrzeba **mniejszego** preloadu (~1/scale²). Kod
mnoży preload przez scale — kompensacja działa **w odwrotną stronę**, niż
deklaruje komentarz. Skutek dla gracza: suwak „Mnożnik twardości – przód"
zmienia też prześwit przodu; „Prześwit" jest globalny; nie da się ustawić
wysokości osi niezależnie od miękkości. To dokładnie zgłoszony brak.

**Fix (P3):** rozdzielić — `suspensionPreloadFront/Rear` (dwa suwaki
„Prześwit przód/tył"), bez mnożenia przez scale. Zanim zmienimy: pomiar
empiryczny wysokości przy scale 0.5 / 1.0 / 2.0 (potwierdzenie kierunku błędu
liczbami — reguła „czytaj liczby").

---

## 5. 🟠 Znaleziska średnie

**S1 — Powrót kierownicy zatrzymuje się martwo na środku.** Hands-off =
sprężyna OFF + motor v=0 z capem `rackFrictionForce` = **250 N stałego tarcia
Coulomba** (`_rig.cpp:1231-1236`, default `:370`). Siła centrująca casteru
maleje do zera przy zerowym kącie, więc rack ZAWSZE staje dokładnie tam, gdzie
siła spadnie poniżej 250 N — tuż przy centrum, bez przestrzału. Zero
zależności od prędkości/obciążenia. **Fix (P4):** obniżyć default (~80-120 N),
rozdzielić tarcie statyczne/kinetyczne (kinetyczne niższe → bezwładność
zwrotnicy+koła przenosi przez centrum → naturalny lekki przestrzał zależny od
siły odbicia — dokładnie oczekiwanie Jozza). Suwak już istnieje („Tarcie
zębatki"), dojdzie drugi.

**S2 — HingeSwingLimit saturuje.** Przy domyślnym skoku (0.42/0.28 m z hintu
0.70) i dolnym wahaczu 0.46 m: `1.25·0.42/0.46 = 1.14` → clamp 0.95 → 55°.
Strażnik pracuje na suficie, margines 25% jest fikcją — **domyślny skok jest
większy, niż geometria ramion umie pokryć**. Fix (P6): skok domyślny ~0.45 m
łącznie (0.27/0.18) LUB ostrzeżenie w UI gdy travel/armLength > 0.85; asercja
w walidatorze.

**S3 — Masa koła prawdopodobnie podwojona.** Split envelope tworzy sferę +
cylinder, oba z `wheelDensity=80` → masa koła ≈ suma obu brył (~70+ kg).
Do zweryfikowania odczytem `b3Body_GetMass` (walidator/dump); jeśli
potwierdzone — gęstość na jednej bryle albo jawna `SetMassData`.

**S4 — Bumpery/washboard: ostre krawędzie boxów.** Tagowanie jest POPRAWNE
(washboard = teren → gładka sfera je widzi; propsy = obiekt → sidewall).
„Dziwne zachowanie przy złym najechaniu" to sfera na ostrej krawędzi boxa
0.10 m + boczny impuls → moment na zwrotnicy → przy podatności z K1/K2 układ
się łamie. Po fixach K1/K2 wrócić i przetestować; ewentualnie dodać wariant
toru z fazowanymi bumperami (ale tor testowy MA być brutalny — decyzja Jozza).

**S5 — Cone limit 80°** — luźny „drugi płot"; po K1 przestaje być krytyczny.
Docelowo policzalny z geometrii travel (~55° jak zawias).

---

## 6. Audyt suwaków (zakładka po zakładce)

Kluczowa odpowiedź na zgłoszenie „Geometria wahaczy nic nie robi":
**suwaki działają na fizykę** (po Zastosuj → pełna przebudowa hardpointów),
ale (a) **model 3D się nie zmienia** — wizual jest rysowany z autorskich
socketów modelu przypiętych do bracket/hub, więc dłuższy wahacz w fizyce NIE
wydłuża wahacza na ekranie; zmiany widać tylko na liniach debug i w
zachowaniu; (b) część efektów jest subtelna (roll center, przyrost campera);
(c) K2 sprawia, że kilka z nich aktywnie psuje limit skrętu. Wrażenie Jozza
jest więc **uzasadnione**, mimo że przyczyna jest inna niż „martwe suwaki".

| Suwak (Zawieszenie) | Tryb | Realny efekt | Uwagi |
|---|---|---|---|
| Opadanie wahacza | Apply | fizyka+debug | OK, klamra 16° słuszna (Ackermann) |
| Prześwit (preload) | live | fizyka | globalny; sprzężony z twardością — K3 |
| Skok ściskania/odbicia | live | fizyka (limity coilovera) | defaulty za duże — S2 |
| Twardość/Tłumienie/Mnożniki | live | fizyka | OK; mnożnik zmienia też wysokość (K3) |
| Stabilizatory | live | fizyka (siły ARB) | OK |
| Caster / KPI / offset sworznia | Apply | fizyka+debug | działa; wizual nie — opisać |
| Długości wahaczy / rozstaw mocowań | Apply | fizyka+debug | działa; wizual nie; **K2!** |
| Cofnięcie ramienia kierown. / Ackermann | Apply | fizyka | **K2** — rozstraja limit |
| Wysokość mocowania amortyzatora | Apply | fizyka | istnieje (odpowiedź na brak z listy Jozza) — poprawić opis |
| Masy (zwrotnica/wahacz) | Apply | fizyka | OK |
| (Kierownica) Sztywność/Tłumienie | live | fizyka | OK |
| Siła wspomagania / Tarcia | live | fizyka | tarcie zębatki → S1 |
| (Napęd) wszystkie | live | fizyka | OK, uczciwy model momentu |
| (Świat) Tarcie opon / kontakt | live | fizyka | OK |
| (Debug) wszystkie | live | tylko widok | poprawnie odseparowane |

**Braki potwierdzone:** maks. skręt kół (config istnieje, suwaka brak);
prześwit przód/tył osobno (K3); toe statyczne przód/tył (długość toe-linku
nie jest tunowalna!); camber statyczny; tarcie statyczne vs kinetyczne
maglownicy; twardość limitów (bump-stopów) — o ile API box3d na to pozwala
(sprawdzić w P6).

---

## 7. Opony (NIE ruszamy — notatka na przyszłość, M7.4)

Dziś: skalar tarcia + rolling resistance + kontakt punktowy sfery. Braki
znane i zaplanowane: krzywa poślizgu (siła vs slip angle/ratio), wrażliwość
na obciążenie (load sensitivity — klucz do transferu masy w driftach),
combined slip (koło hamujące skręca słabiej), miękkość opony. Fundament
(prawdziwe siły na zwrotnicy, slip angle już w telemetrii) jest gotowy na tę
warstwę. Nie zaczynać przed domknięciem P1-P6 — opona na niestabilnym rigu
będzie się stroiła do bugów.

---

## 8. Plan naprawy (etapy; każdy = osobna bramka: build + walidator-LICZBY + jazda + checkpoint)

| Etap | Co | Ryzyko | Status |
|---|---|---|---|
| **P1** | Twist-fence z konfiguracji (przód maxSteer+10°, tył 15°) + asercja fence<deadPoint + test udarowy w walidatorze | średnie (nowe limity mogą ciąć legalny ruch — weryfikować jazdą) | do zrobienia |
| **P2** | `RecomputeRackTravel()` w Apply + regresja | niskie (1 linia) | do zrobienia |
| **P3** | Preload przód/tył, bez scale; pomiar wysokości przed/po | niskie | do zrobienia |
| **P4** | Tarcie statyczne/kinetyczne racka + niższy default; test odbicia | niskie | do zrobienia |
| **P5** | Suwak maks. skrętu + toe statyczne + opisy WSZYSTKICH suwaków wg szablonu (co/na co/+/-/kiedy/typ: fizyczny-wizualny-debug) + jawna informacja że geometria nie zmienia modelu 3D | niskie | do zrobienia |
| **P6** | Sanity skoku (S2), masa koła (S3), cone limit z geometrii (S5), bump-stop stiffness jeśli API pozwala | średnie | do zrobienia |

Kolejność jest celowa: P1+P2 usuwają „łamanie", zanim zaczniemy cokolwiek
stroić; strojenie na łamliwym układzie to strata czasu.

**Nie ruszamy:** rdzeń box3d; model M7 (napęd/ARB/aero — zdrowe); opony
(§7); geometria kierownicy pod droop >16° (osobny research, TECH_DEBT #5);
wizualny rig (dekoracyjne dumpery — osobny plan w SUBSYSTEM_RIG_DAMPER_MOUNT).

**Osobny research (nie w tych etapach):** sprzężenie wizualu z hardpointami
(skalowanie części modelu do faktycznej geometrii — duży temat, dotyka
importera); zniszczalność połączeń (joint break force — kiedyś, dla crashów);
solver substeps vs sztywność limitów przy udarach.
