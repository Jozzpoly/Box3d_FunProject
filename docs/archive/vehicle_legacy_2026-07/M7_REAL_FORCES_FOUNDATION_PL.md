> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# M7 Real Forces Foundation — analiza krytyczna, plan i raport

Data: 2026-07-06
Branch: `jozz-vehicle-sandbox-m0`
Status: **ZAIMPLEMENTOWANE**; `jozz_vehicle_validation: OK`, `test.exe` zielony,
boot smoke sampli 95/96/97 czysty; czeka na ręczny test Jozza (sekcja 9)
Wejście: feedback Jozza po ręcznym teście M6 (2026-07-06)

## 0. Feedback i kierunek

Jozz po jeździe M6:

```text
+ nowa kolizja kół (split envelope) działa dobrze
- zawieszenie za łatwo "się łamie": skok na skoczni = samochód się rozpada
- odbijanie kół w poślizgu działa sztucznie, "jakby robione pod drift";
  ma być NATURALNYM skutkiem sił działających na pojazd w ruchu,
  geometrii zawieszenia i układu kierowniczego - jak w rzeczywistości
* wzorzec kierunkowy: BeamNG.drive (siły mają WYNIKAĆ z konstrukcji,
  nie być animowane); drift ma być efektem ubocznym fizyki
* zadania: (1!!) naprawić i rozszerzyć fundamenty sił, (2) zrigować model
  zawieszenia Jozza + fundament importu typów zawieszeń, (3) przebudować
  UI labu na prawdziwe zakładki
```

Zasada decyzyjna projektu bez zmian: najpierw trudne do zepsucia, potem
efektowne. M7 realizuje ją dosłownie — usuwamy dwa źródła kruchości
(flip prętów, skryptowany align) i zastępujemy je mechanizmami, które
w rzeczywistych autach odpowiadają za te same zjawiska.

## 1. Analiza krytyczna stanu po M6 (co jest nie tak i DLACZEGO)

### 1.1 Rozpad zawieszenia przy lądowaniu — pręty nie mają "dobrej strony"

Wahacz M6 = dwa sztywne distance jointy zbiegające się w ball joincie.
Constraint odległości ma **dwa rozwiązania** (lustrzane względem prostej
przez punkt chassis). Twarde lądowanie: impuls kontaktu koła (dziesiątki kN
przez ~2 substepy) chwilowo przełamuje więzy, solver odzyskuje do
NAJBLIŻSZEGO rozwiązania — a po przejściu przez konfigurację osobliwą
(pręt współliniowy) najbliższa jest gałąź LUSTRZANA. Zwrotnica osiada w
odwróconej konfiguracji i nigdy nie wraca: wygląda jak złamane zawieszenie.

Pogłębiacze:
- jedynym ogranicznikiem skoku jest limit długości coilovera — działa wzdłuż
  JEGO osi i nie zabrania gałęzi lustrzanej;
- koło nie koliduje z chassis (wspólny filterGroupIndex), więc nic
  fizycznie nie blokuje złożenia się koła pod auto;
- zwrotnica (28 kg) między kołem (~44 kg) a chassis (~480 kg) dostaje cały
  impuls przez sztywny revolute.

W prawdziwym aucie ta konfiguracja jest niemożliwa nie dlatego, że siły są
mniejsze, tylko dlatego, że wahacz jest CIAŁEM na zawiasie z odbojami —
ma dokładnie jedną gałąź ruchu.

### 1.2 "Sztuczny drift" — serwo nigdy nie puszcza kierownicy

Dwie warstwy problemu:

1. `selfAlignAssist` MIESZA komendę racka z kierunkiem ruchu auta —
   to jest wprost animowanie kół pod ślizg (Jozz to wyczuł bezbłędnie).
2. Głębiej: nawet z asystą OFF sprężyna pozycyjna racka (14 Hz) + serwo
   12 kN **zawsze** ciągną rack do targetu (przy zerowym wejściu = do
   centrum). Fizyczny caster (5°) nie ma szans zadziałać, bo "ręce"
   nigdy nie schodzą z kierownicy. Pomiar z sondy M6 potwierdza: assist
   OFF = koła trzymane ~0° mimo ślizgu 10°.

Fizyka, która ma to robić naprawdę, JEST w rigu: caster → mechaniczny
trail → siła boczna opony × trail = moment prostujący na zwrotnicy →
drążek → rack. Trzeba tylko pozwolić rackowi się cofać, gdy kierowca
puszcza A/D. Szacunek rzędów: ślizg przy obciążeniu ~1.9 kN/koło,
μ=1.25 → ~2.4 kN siły bocznej; trail = tan(5°)·0.51 m ≈ 0.045 m →
~105 N·m na zwrotnicę → przez ramię 0.17 m → ~1.2 kN na rack z obu kół.
Tarcie układu rzędu 200–300 N nie zatrzyma tego — kontra "sama wejdzie".

### 1.3 Napęd = serwo prędkości, nie moment

Motor chase'uje ±26 rad/s: koło fizycznie NIE MOŻE zbudować poślizgu
napędowego powyżej tej prędkości, a poniżej ciągnie pełnym momentem
niezależnie od obrotów — czuć "pociąg", nie silnik. Bez zerwania
przyczepności nie ma naturalnych wejść w poślizg z gazu, burnoutów ani
power-slide'ów — czyli połowy zachowań, o które chodzi w feedbacku.

### 1.4 Upright assist = niefizyczna ręka boża

Parallel joint chassis↔świat (0.4 Hz) tłumi przechył, nurkowanie i rotacje
W POWIETRZU. Wszystko, co dzieje się na skoczni i w ślizgu, jest przez to
stępione. W BeamNG nie ma niczego takiego — auto trzyma się na kołach, bo
ma geometrię, CG i stabilizatory.

### 1.5 Braki po stronie "prawdziwych sił" (niedobory, nie błędy)

- brak stabilizatora poprzecznego (ARB) — bez upright assist przechył
  będzie realny, więc potrzebny realny mechanizm;
- brak oporu aerodynamicznego — top speed pochodzi z rev limitu zamiast
  z równowagi sił;
- opona = tarcie Coulomba w punkcie kontaktu (bez krzywej poślizgu, bez
  wrażliwości na obciążenie) — świadomie ZOSTAJE na później (roadmapa
  soft-tire z M5.2 §5); M7 nie dotyka modelu opony.

## 2. Architektura M7 (co się zmienia)

### M7.A Wahacze jako ciała z zawiasami i odbojami (fix rozpadu)

```text
było:  chassis --4x sztywny pręt--> zwrotnica          (gałęzie lustrzane!)
jest:  chassis --revolute(oś X przez stare hardpointy   (jedna gałąź ruchu,
        mocowań, LIMITY kąta = odboje droop/bump)        limity = odboje jak
       --> ciało wahacza (bez shape'ów, masa jawnie)     gumowe stopy w aucie)
       --spherical(cone+twist limit) w ball joincie-->
       zwrotnica
```

- Hardpointy (kontrakt geometrii) NIE zmieniają się ani o milimetr —
  zmienia się tylko realizacja więzów. Import z markerów (M6.3) bez zmian.
- Sworzeń kulisty: oś z frame'ów wzdłuż kingpina; cone limit ~30°
  (artykulacja), twist limit ±50° (skręt 32° + zapas) — drugi bezpiecznik
  po zawiasach.
- Tie-rod/toe-link zostają prętami: w rzeczywistości TO SĄ drążki
  z przegubami kulowymi na końcach; ich gałęzie pilnowane są teraz przez
  ograniczoną zwrotnicę, a lekcja over-center (ackermannFraction) zostaje.
- Coilover bez zmian (chassis↔zwrotnica, limity = precyzyjne stopy skoku;
  zawiasy dają stopy awaryjne szersze o ~25%).
- Ciała wahaczy bez shape'ów + `b3Body_SetMassData` (lekcja CCD z M6),
  masa default 5 kg, inercja płytki L×S.
- DOF (Gruebler): 3 ciała=18; 2×zawias(5)+2×sworzeń(3)+drążek(1)=17 → 1
  stopień swobody (skok). Skręt = przesuw racka. Zero nadokreślenia.

### M7.B Back-drivable rack — kontra z fizyki, nie ze skryptu

Model chwytu kierownicy (dwustanowy, wejście klawiaturowe i tak jest 0/±1):

```text
|steer| > próg  "ręce na kierownicy": sprężyna pozycyjna ON (feel) +
                serwo z limitem siły (wspomaganie/parking torque, jak M6)
|steer| ~ 0    "ręce puszczone": sprężyna OFF, serwo speed=0 z małym
                limitem siły = TARCIE układu kierowniczego (default ~250 N);
                caster/trail fizycznie cofa rack przez zwrotnice i drążki
```

- `selfAlignAssist` + gain/minSpeed/maxSlip: **USUNIĘTE** (udokumentowany
  negatywny wynik: animowanie kół pod ślizg czuć natychmiast).
- Kontra w driftach, powrót kierownicy po zakręcie, brak powrotu na
  postoju (tarcie > siły) — wszystko wynika z geometrii; siła efektu
  skaluje się casterem (suwak Apply już jest).
- Oś strut: analogicznie — hands-off obniża maxSteeringTorque do wartości
  tarcia; przy strutCasterDeg=0 koła po prostu zostają (M5 baseline
  w module M5 nietknięty).
- Ryzyko shimmy/death wobble (realne zjawisko!): tłumione tarciem racka;
  sonda w walidatorze mierzy uspokojenie racka po ślizgu.

### M7.C Prawdziwe siły jazdy

1. **Napęd momentem**: motor target = rev limit (maxDriveSpeed), a
   `maxMotorTorque = throttle · maxDriveTorque · taper(|ω|)`, taper=1 do
   60% ωmax, potem liniowo do 0. Koło MOŻE zerwać przyczepność (default
   320 N·m/koło jej nie zrywa — słusznie; suwak do 6000 = burnouty).
2. **Stabilizator (ARB) per oś**: siła `F = k·(travelL − travelR)` w dół
   na bardziej ścieśniętą zwrotnicę, w górę na przeciwną + reakcje na
   chassis w punktach mocowań — czysta para przeciw przechyłowi, zero
   momentu netto. To jest prawdziwy mechanizm (drążek skrętny), tylko
   liczony z telemetrii skoków zamiast z ciała drążka. Default: przód
   16 kN/m, tył 10 kN/m, suwaki live.
3. **Opór aero**: `F = −0.5·ρ·CdA·|v|·v` w środek masy (ρ=1.2, CdA
   default 0.9 m²) — top speed staje się równowagą sił.
4. **Upright assist: default OFF** (toggle zostaje jako koło ratunkowe).

### M7.D Trailing arm z kontraktu + montaż modelu Jozza

Model `One_Sided_wheel_mount` to jednostronne ramię wleczone — dostaje
swój typ rigu:

```text
JOZZ_M6_RIG_TRAILING_ARM (oś tylna; przód: dozwolone, ale bez skrętu)
  ciało ramienia (bez shape'ów, masa param, origin w osi obrotu)
  revolute chassis->ramię, oś = chassis Z (czyste ramię wleczone),
    limity kąta = odboje skoku
  koło: revolute na ramieniu w wheelCenter
  coilover: chassis(damper upper mid) <-> ramię(damper lower mid),
    sprężyna + limity jak wishbone
```

Hardpointy ramienia pochodzą z **kontraktu sidecar** (pierwszy realny
import geometrii zawieszenia — M6.3 w wersji minimalnej):

```text
suspension.visual.chassis_mount   -> oś obrotu ramienia
suspension.visual.wheel_center    -> środek koła (kotwica: trafia w rest
                                     wheel center narożnika)
damper_upper_L/R (midpoint)       -> oko coilovera na chassis
damper_lower_L/R (midpoint)       -> oko coilovera na ramieniu
```

Fallback bez kontraktu: generator (pivot 0.55 m przed kołem). Wizual:
cały model glTF rysowany na ŻYWYM ciele ramienia (korekta: authored
chassis_mount ↦ origin ramienia, skala z kontraktu). Znany dług v1:
części chassisTop/Bottom jadą z ramieniem (mesh nie umie jeszcze rysować
per-część); orientacja per ADR-0002 = korekta tymczasowa w imporcie,
ostateczna ocena należy do Jozza.

### M7.E UI labu M6: prawdziwe zakładki

- `ImGui::BeginTabBar` w DrawControls; szerszy panel (`InfoPanelWidthEm`).
- Zakładki: **Drive** (napęd/hamulec/aero), **Steering** (rack, tarcie,
  wspomaganie), **Suspension** (sprężyny/tłumienie/ARB live),
  **Rig (Apply)** (typy osi, geometria wahaczy, trailing arm, chassis,
  envelope — wszystko co przebudowuje), **World** (kontakt, tarcie opon,
  widoczność, propsy), **Telemetry** (liczby + wykresy).
- Pending zmiany strukturalne: gwiazdka na zakładce Rig + stały pasek
  Apply na dole panelu (widoczny z każdej zakładki).
- Separacja live vs Apply (świętość z M2.5) zachowana co do joty.

## 3. Czego M7 świadomie NIE robi

```text
- model opony (krzywa poślizgu, load sensitivity, relaksacja) — zostaje
  na roadmapie soft-tire; Coulomb per kontakt wystarcza na tę bramkę
- drivetrain (dyferencjały, sprzęgło, biegi) — napęd momentem to fundament,
  na którym to kiedyś stanie
- łamliwość zawieszenia jako FEATURE (beam breaking jak BeamNG) — najpierw
  rig ma być niezniszczalny w normalnym użyciu; kontrolowane zniszczenia
  to osobna przyszła bramka
- per-część rysowanie modelu mountu (chassisTop/Bottom osobno od ramienia)
- zmiany w silniku Box3D: ZERO
- moduł M5 i corner lab: nietknięte (baseline porównawczy)
```

## 4. Plan walidacji (dowody, nie nadzieja)

```text
RunM7LandingIntegrityProbe   lot 14 m/s z 2.0 m i z 3.5 m; po lądowaniu:
                             camber/toe/travel w granicach, zwrotnice przy
                             restach, auto dalej jedzie i skręca (pełny
                             test funkcjonalny po nadużyciu)
RunM7HandsOffAlignProbe      ślizg 25° jak w M6, ręce puszczone:
                             caster 5° -> kontra w znaku ślizgu i wyraźna;
                             caster 0° -> efekt ginie (DOWÓD, że kontra
                             pochodzi z geometrii, nie ze skryptu);
                             rack ma się uspokoić (anty-shimmy)
RunM7TorqueDriveProbe        default: rusza bez wheelspinu; 2500 N·m:
                             wheelspin na starcie; obroty zbiegają do
                             rev limitu
RunM7TrailingArmSmoke        tylna oś trailing z kontraktu (fallback gdy
                             brak pliku): settle, jazda, hamowanie, kąt
                             ramienia w limitach
RunM6SuspensionRigSmoke      aktualizacja pod nowe wahacze (te same
                             asercje: sag, skręt podpisany, trapez,
                             jazda, hamowanie, mixed rig)
RunM6DriftSelfAlignProbe     USUNIĘTA (asertowała skrypt) -> zastąpiona
                             przez RunM7HandsOffAlignProbe
```

Build + `test.exe` + boot smoke sampli 95/96/97 jak zawsze.

## 5. Wyniki implementacji (pomiary z walidatora, 2026-07-06)

### 5.1 Lądowania — rig nie do złamania w normalnym użyciu

```text
zrzut 2.0 m przy 14 m/s:  worst camber 0.6 deg, rear steer 0.6 deg,
                          |travel| 0.085 m; jedzie i skręca po lądowaniu
zrzut 3.5 m przy 14 m/s:  IDENTYCZNE wyniki - odboje zawiasów + limity
                          coiloverów sufitują energię niezależnie od zrzutu
```

Dla porównania: rig prętowy M6 składał się na skoczni z rampy (~1 m zrzutu).

### 5.2 Fizyczny self-align — pomiary

```text
zjazd z zakrętu, puszczone A/D przy ~15 m/s:
  28.2 deg na kołach -> -0.4 deg po 3 s (czysty caster, zero serwa)
parking: 30.7 deg -> 30.6 deg po puszczeniu (tarcie > siły na postoju;
  koła NIE wracają magicznie do centrum - jak w prawdziwym aucie)
ślizg 25 deg, ręce puszczone:
  rack WOLNY:     śr. kąt kół -11.1 deg w kierunku ślizgu (kontra!)
  rack ZAMROŻONY:  -3.4 deg (efekt ginie -> droga momentu jest mechaniczna:
                   opona -> zwrotnica -> drążek -> rack; skrypt by przeżył)
  rack RMS 0.20 m/s (brak shimmy)
```

**Zmierzona lekcja:** wyzerowanie SAMEGO castera NIE gasi kontry — scrub
radius (kingpin offset) i offset masy zwrotnicy/koła od osi zwrotu też
prostują koła w ślizgu. Prawdziwe zawieszenia walczą dokładnie z tymi
członami. Falsyfikacja w sondzie używa więc zamrożonego racka, nie
zerowego castera.

### 5.3 Napęd momentem — pomiary

```text
320 N*m (default): slip ratio 0.00 przy starcie (opony trzymają), 2.56 m/s
                   po 1 s; long pull: 17.8 m/s, obroty 35.7/40 rad/s
2600 N*m:          slip ratio 0.91 - zerwanie przyczepności/burnout
```

### 5.4 Trailing arm z kontraktu — pierwszy import geometrii do fizyki

```text
import: jozz.one_sided_wheel_mount.v0, pivot {0.44, 0.39, 0.00} m od środka
        koła, damper rest 0.69 m, korekta yaw -18 deg (ADR-0002)
settle: worst travel 0.193 m z 0.420 m (bez kompensacji: 0.542 m = narożnik
        LEŻAŁ na limicie coilovera!)
```

**Nowa lekcja silnika (rozszerzenie lekcji "hertz podąża za masą
efektywną"):** distance joint zaczepiony o smukłe OBRACAJĄCE SIĘ ramię ma
masę efektywną kilka kg (1/m + (r×û)ᵀI⁻¹(r×û) — człon rotacyjny dominuje),
więc te same 6 Hz daje wielokrotnie za miękką sprężynę. M7 liczy kompensację
w `CreateTrailingArmCorner`: docelowa sztywność NA KOLE = unsprung·ω²,
przełożona przez motion ratio (dDamper/dWheelY z geometrii spoczynkowej)
i masę efektywną z NASZYCH własnych danych masowych → hertz dampera.
Suwak zawieszenia znaczy to samo na każdym typie rigu
(`runtime.trailingCoiloverHertzScale` niesie kompensację do live tuningu).

### 5.5 Decyzja: światy pojazdów bez continuous collision (CCD)

Solver flaguje ciało jako "fast", gdy ruch na krok > połowy najmniejszego
wymiaru shape'u — powyżej ~15 m/s dotyczy to SAMYCH KÓŁ (sfera r=0.51).
Sweep toczącego się koła startuje w kontakcie z gruntem i głodzi walidację
debug w push-backu TOI (`distance.c:1798`) — za każdym razem. Świat
pojazdów nie ma cienkiej geometrii (najcieńszy collider >> ruch na krok),
więc CCD nic tu nie kupuje. `b3World_EnableContinuous(false)` w labie M6/M7
(z przywróceniem przy wyjściu; checkbox w Solver działa) i w światach sond
walidatora. Silnik NIETKNIĘTY. Przyszłe cienkie ściany = z powrotem CCD
albo grube collidery — zapisane w komentarzach przy obu wyłączeniach.

## 6. Co dokładnie weszło do kodu

```text
jozz_vehicle_m6_suspension_rig.h/.cpp
  - DOUBLE_WISHBONE: ciała wahaczy (CreateControlArm) + revolute z limitami
    (HingeSwingLimit: asin(1.25·travel/L), cap 55 deg) + spherical
    cone 80/twist ±70 (czyste bezpieczniki anty-złożeniowe)
  - TRAILING_ARM: nowy typ rigu (ramię + revolute Z + coilover z hardpointów
    kontraktu + kompensacja motion-ratio/masy efektywnej)
  - back-drivable rack: hands-on = spring+servo; hands-off = spring OFF,
    motor speed 0 z siłą = rackFrictionForce (strut analogicznie przez
    steeringFrictionTorque i target=bieżący kąt)
  - USUNIĘTE: selfAlignAssist/gain/minSpeed/maxSlipDeg (negatywny wynik)
  - napęd momentem: TaperedDriveTorque (pełny moment do driveTaperStart·rev,
    taper liniowy do zera; poślizg w komendowanym kierunku liczy się do
    taperu, przeciwny nie - moment hamujący zostaje)
  - ApplyAxleAntiRollBar: para sił z różnicy skoków, reakcje na chassis
  - aero drag kwadratowy w środek masy
  - uprightAssist default OFF; maxDriveSpeed 26->40 rad/s
jozz_vehicle_m7_suspension_import.h/.cpp (NOWE)
  - kontrakt sidecar -> JozzVehicleM6TrailingArmGeometry (pivot/dampery
    względem wheel_center, korekta yaw na +X, fallback na generator)
jozz_vehicle_m6_rig_lab.cpp (przebudowa)
  - UI w zakładkach: Drive / Steering / Susp / Rig* / World / Telemetry,
    panel 26 em, pasek Apply/Discard widoczny z każdej zakładki
  - model One_Sided_wheel_mount rysowany na ŻYWYM ciele ramienia (korekta:
    authored chassis_mount -> origin ramienia + ten sam yaw co import)
  - default: przód wishbone, TYŁ TRAILING (model Jozza widoczny od startu)
  - diagnostyka rigu z żywych ciał wahaczy + gałąź trailing
  - CCD off z przywróceniem, kompensacja hertza w live tuningu zawieszenia
jozz_vehicle_validation.cpp
  - M6 smoke przełożony na nową fizykę (jazda -> skręt -> puszczenie ->
    asercja samocentrowania -> hamowanie -> parking steer + trzymanie kąta)
  - RunM7LandingIntegrityProbe (2.0 m i 3.5 m)
  - RunM7HandsOffAlignProbe (wolny vs zamrożony rack + anty-shimmy)
  - RunM7TorqueDriveProbe (brak wheelspinu @320, wheelspin @2600, rev limit)
  - RunM7TrailingArmSmoke (import z kontraktu + settle/drive/brake)
  - USUNIĘTA RunM6DriftSelfAlignProbe (asertowała skrypt)
```

## 7. Ryzyka i długi (świadome)

```text
- prawy tylny narożnik pokazuje NIEODBITY model mountu (transformacje
  sztywne nie lustrzą; model ma symetryczne sockety damperów L/R, więc
  wizualnie może być OK - ocena należy do Jozza)
- części chassisTop/Bottom modelu jadą z ramieniem (mesh nie umie jeszcze
  rysować per-część) - następny krok warstwy wizualnej
- trailing arm na przedniej osi nie skręca (uczciwe; UI ostrzega)
- wejście klawiaturowe jest binarne, więc model "rąk" jest dwustanowy;
  przy padzie analogowym warto zmiękczyć przejście (przyszła bramka)
- ARB liczony z telemetrii skoków (uczciwy wzór, ale bez ciała drążka);
  wizualizacja/ciało drążka = przyszłość
```

## 8. Walidacja końcowa

```text
cmake --build --preset windows-debug --target jozz_vehicle_validation
cmake --build --preset windows-debug --target samples --target test
build\bin\Debug\jozz_vehicle_validation.exe  -> jozz_vehicle_validation: OK
build\bin\Debug\test.exe                     -> All Box3D tests passed
build\bin\Debug\samples.exe --sample 95/96 --frames 240   (0 sokol errors)
build\bin\Debug\samples.exe --sample 97 --frames 300      (0 sokol errors)
```

## 9. Checklist ręcznego testu dla Jozza

```text
1. Sample "Jozz Vehicle / M6 Suspension Rig Lab" - nowe UI w zakładkach;
   tył jeździ na TWOIM mouncie (białe ramię diagnostyki + model glTF).
2. SKOCZNIA: rozpędź się na maksa w rampę - auto ma wylądować i jechać
   dalej. Prawdziwy test bramki: to, co wcześniej rozwalało zawieszenie.
3. Drift: flick + puszczenie A/D - kontra ma wejść SAMA (HUD: "steering:
   free (caster in charge)"). Zakładka Rig: caster 5->9 deg (Apply) =
   szybsza kontra; Steering: rack friction w dół = żywszy powrót.
4. Zjazd z zakrętu: puść A/D przy prędkości - kierownica sama wraca do
   prostej jazdy. Na postoju NIE wraca (tarcie) - tak ma być.
5. Gaz: default 320 N*m nie zrywa kół; Drive -> torque 2500+ = burnout
   i power-slide z gazu (drift jako efekt uboczny sił, nie skrypt).
6. Przechyły: Susp -> ARB przód/tył. Upright assist jest OFF - auto ma
   naturalne przechyły kontrolowane stabilizatorami. Więcej tyłu = chętniej
   zarzuca, więcej przodu = stabilniej.
7. Model mountu: obejrzyj tylne narożniki w ruchu (washboard!) - model ma
   PRACOWAĆ z ramieniem. Oceń orientację/strony (ADR-0002: korekty
   importera są tymczasowe, Twoje oko decyduje).
8. Rig tab: przełącz tył na wishbone / przód na trailing (ciekawostka:
   przód wtedy nie skręca - uczciwa fizyka) i wróć.
9. Stress: geometria na skrajach, ciężki chassis, Ackermann 0..1, rev
   limit 150 - szukaj granic. R = restart, Reset props w World.
```

## 10. Następne bramki na tym fundamencie

```text
M7.1  per-część montaż wizualny (chassis bracket na chassis, ramię na
      ramieniu) + lustrzenie prawej strony
M7.2  hardpointy wishbone z markerów modeli (import wypełnia
      JozzVehicleM6WishboneHardpoints - struktura gotowa od M6)
M7.3  drivetrain: dyferencjały (open/locked), rozdział momentu, engine
      braking z krzywej - na torque-driverze z M7
M7.4  model opony (krzywa poślizgu, load sensitivity) - roadmapa soft-tire
      z M5.2 sekcja 5; telemetria load/slip już czeka
M7.5  pad analogowy + miękkie przejście rąk na kierownicy
```
