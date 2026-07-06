# M6 Suspension Rig Foundation — raport

Data: 2026-07-06
Branch: `jozz-vehicle-sandbox-m0`
Status: zaimplementowane; walidacja headless zielona (`jozz_vehicle_validation: OK`),
boot smoke sampli czysty; czeka na ręczny test Jozza

Cel (z kierunku Jozza po zamknięciu M5.2): (1) fundament wieloelementowego rigu
zawieszenia gotowy na różne typy zawieszeń i klasy pojazdów (ulica/drift/offroad,
lekkie/ciężkie), (2) naturalne zachowanie kół w poślizgu (self-aligning), (3) koniec
z kolizją koła szerszą niż wizualna opona.

## 1. Architektura: hardpointy jako kontrakt zawieszenia

Nowy moduł `samples/jozz_vehicle_m6_suspension_rig.h/.cpp` (bez zależności gfx,
współdzielony przez sample i walidator — wzorzec M5).

Fundamentalna decyzja: **typ zawieszenia = zestaw punktów mocowania (hardpoints)
w przestrzeni chassis-local** + parametry sprężyny/dampera. Tak opisują zawieszenie
poważne symulatory i tak będzie wyglądał przyszły import z markerów/socketów
modeli Blockbench: importer wypełni te same struktury, fizyka się nie zmieni.

```text
Warstwa 1  Wheel envelope       JozzVehicleM6WheelEnvelopeDesc + builder
           (kolizja koła)       wymiary z markerów assetu (M3A), 4 tryby
Warstwa 2  Hardpointy           JozzVehicleM6WishboneHardpoints
           (kontrakt geometrii) dziś z generatora, jutro z modelu
Warstwa 3  Generator            JozzVehicleM6MakeWishboneHardpoints
           (parametry -> punkty) caster/KPI/długości ramion/Ackermann
Warstwa 4  Pojazd               CreateJozzVehicleM6 / UpdateJozzVehicleM6Drive
           (ciała+jointy+drive)  typ rigu per oś, wspólna telemetria
```

Typ rigu wybierany **per oś** (`frontRigType` / `rearRigType`):

```text
JOZZ_M6_RIG_INTEGRATED_STRUT   zwalidowany model M5 (b3WheelJoint per narożnik);
                               tani, dla lekkich/prostych pojazdów; opcjonalny
                               strutCasterDeg pochyla oś (MacPherson-like caster)
JOZZ_M6_RIG_DOUBLE_WISHBONE    multi-body: zwrotnica + koło jako ciała
```

Narożnik double wishbone:

```text
ciała:  zwrotnica (knuckle, bez shape'ów - masa jawnie), koło (envelope)
wahacze: 4 sztywne pręty (rigid distance joints) między hardpointami
         chassis <-> ball jointy zwrotnicy; wahacz trójkątny = 2 pręty
         zbiegające się w ball joincie - identyczna kinematyka, zero
         dodatkowych ciał, najstabilniejszy constraint w solverze
coilover: distance joint ze sprężyną (hertz/damping) + limity długości
          = jawne limity skoku (rebound/compression)
napęd:   revolute knuckle->koło z motorem (drive/brake/coast jak M5)
przód:   fizyczny RACK (ciało na prismatic w osi Z) + sztywne tie-rody
         do ramion zwrotnic; Ackermann z geometrii trapezu (nie z tablicy)
tył:     toe link (5. pręt) do chassis
```

Konwencja kierunków bez zmian (M5.2): forward=+X, up=+Y, LEWO=-Z, dodatni kąt
skrętu = skręt w lewo. Wszystkie asercje skrętu w walidatorze są PODPISANE.

## 2. Układ kierowniczy: rack servo i lekcja parking torque

Rack ma sprężynę pozycyjną (feel, powrót do centrum) **plus motor-servo
z twardym limitem siły** (`rackServoForce`, default 12 kN). Powtórka lekcji
M5.1 w nowym przebraniu: sama sprężyna prismatic (hertz na ~5 kg listwie)
daje ~625 N·m na oba zwrotnice ŁĄCZNIE, a stojąca obciążona opona potrzebuje
~700 N·m KAŻDA. Pomiar w smoke: bez serwa skręt na postoju 0.6°/32°,
z serwem 30.8°. Servo = wspomaganie kierownicy z ograniczoną siłą — fizycznie
uczciwe i strojone suwakiem w labie.

Przełożenie kąt→skok racka **liczone z geometrii w formie zamkniętej**
(`ComputeJozzVehicleM6RackStroke`): mechanizm jest nieliniowy — dla obecnej
geometrii 32° wymaga 0.075 m skoku, nie `ramię×kąt`=0.095 m. Komenda, limit
racka i trapez zawsze się zgadzają, bo przechodzą przez tę samą funkcję.

**Lekcja over-center:** pełny geometryczny Ackermann (zbieżność ramion ~22°)
przy pełnym skoku wpychał wewnętrzną zwrotnicę w martwy punkt mechanizmu
(ramię i drążek ~6° od współliniowości; zmierzone: komenda 32° → koło 46°,
skręt się zakleszczał i auto kręciło się w miejscu). Rozwiązanie jak w
prawdziwych autach: **częściowy Ackermann** — `ackermannFraction` (default
0.6). Pomiar po poprawce: wewnętrzne 30.8°, zewnętrzne 26.1°, jazda po
skręcie normalna.

## 3. Zachowanie kół w poślizgu (self-aligning) — dwie warstwy

```text
Warstwa fizyczna   casterDeg w geometrii (default 5 deg): oś zwrotnicy
                   pochylona do tyłu -> mechaniczny trail -> kontakt opony
                   sam prostuje koła, moment wraca przez tie-rody do racka
Warstwa asysty     selfAlignAssist (default ON, gain 0.65): komenda racka
                   miesza się z kierunkiem RZECZYWISTEGO ruchu auta wagą
                   (1-|input|)*gain - puszczasz A/D w ślizgu, koła same
                   idą w kontrę; trzymasz pełny skręt - Twoja wola wygrywa
```

Sonda driftu w walidatorze (wymuszony ślizg 25° przy ~13 m/s, ręce z kierownicy):

```text
                 align target   śr. kąt kół   błąd śledzenia
assist ON          -10.7 deg      -7.2 deg        3.5 deg
assist OFF         -10.8 deg      -0.2 deg       10.6 deg
```

Asercje podpisane (znak kontry!) + porównawcze (ON musi śledzić ślizg wyraźnie
lepiej niż OFF, bo sam caster też trochę pomaga — i ma pomagać).

Telemetria per koło dostała `slipAngle` (kierunek ruchu koła vs jego heading)
i `camberAngle` (widać geometryczny camber gain wahaczy) — w labie wykres
slip angle x4 obok travel.

## 4. Wheel collision envelope — problem szerokości rozwiązany

Problem z M5.2: sfera (jedyny gładki kształt) wystaje bocznie
`promień - szerokość/2` ≈ **0.29 m** poza wizualną oponę — "niewidzialna
ściana" przy propsach i koła "lewitujące" przy przeszkodach.

Zbadane 4 tryby (`JozzVehicleM6WheelEnvelopeMode`), pomiary z sondy
(pełny gaz, kontakt przednich kół / RMS prędkości pionowej):

```text
kształt          kontakt   szerokość kolizji     werdykt
cylinder 32        31%     dokładna              tarka (znane z M5.2)
phased union 4     19%     dokładna              ŚLEPA ULICZKA (zmierzona!)
sfera             100%     +0.29 m poza oponę    gładka, ale "ściana"
SPLIT             100%     dokładna              DEFAULT M6
```

**Phased union** (4 nakładane cylindry 32-ścienne obrócone o ułamek ścianki,
teoretycznie 128 ścianek): pomiar OBALIŁ pomysł — kontakt skacze między
hullami tysiące razy na sekundę i każdy skok gubi warm-start impulsów
solvera; toczy się GORZEJ niż jeden cylinder. Zostaje w kodzie jako
udokumentowany negatywny wynik i wizualizacja mechanizmu.

**SPLIT (default M6)**: dwa shape'y o komplementarnych maskach kolizji:

```text
sfera toczna       koliduje TYLKO z terenem (kategoria JOZZ_M6_TERRAIN_CATEGORY)
                   -> toczenie 100% gładkie jak sfera M5.2
cylinder boczny    pełna szerokość opony z markerów, koliduje ze WSZYSTKIM
                   POZA terenem -> propsy/ściany/krawężniki widzą prawdziwy
                   bok opony
```

Test "niewidzialnej ściany" w walidatorze: props 5 cm od boku opony —
split NIE dotyka (sfera dotykała; oba przypadki asertowane).

**Kontrakt świata (WAŻNE dla przyszłego kodu):** `b3DefaultShapeDef()` w
Box3D ustawia categoryBits na WSZYSTKIE bity (inaczej niż Box2D!). Split
wymaga tagowania OBU stron:

```text
powierzchnie jezdne    categoryBits = JOZZ_M6_TERRAIN_CATEGORY (0x2, wyłącznie)
propsy/przeszkody      categoryBits = JOZZ_M6_OBJECT_CATEGORY (0x1)
```

Otagowane: grunt labu M6, cały test course (rampy/washboard/heightfield jako
teren, propsy jako obiekty — parametr `terrainCategoryBits` w
`CreateJozzVehicleM5TestCourse`, default bez zmian dla labu M5), grunt smoke
w walidatorze. Gdy `terrainCategoryBits == 0`, builder envelope degraduje się
bezpiecznie do zwykłej sfery.

Wymiary koła (promień/szerokość) płyną z markerów assetu przez M3A — przyszłe
modele kół dostaną poprawną kolizję automatycznie. To jest dokładnie ścieżka
"wheel collider z socketów modelu".

## 5. Lekcja CCD: małe ciała strukturalne bez shape'ów

Pierwsza wersja zwrotnicy/racka miała małe shape'y (maskBits=0) tylko dla
masy. Skutek: solver flaguje ciało jako "fast", gdy ruch na krok przekracza
połowę najmniejszego wymiaru shape'u (`solver.c`), i puszcza na nim continuous
collision — listwa 3.5 cm przy zwykłej prędkości jazdy była w permanentnym
CCD vs ogromny statyczny grunt, a sweep równoległy do płaskiej powierzchni
głodził root finder TOI (twardy assert silnika w debug, `distance.c:1798`).

Rozwiązanie: **zwrotnica i rack nie mają shape'ów w ogóle** — masa i
bezwładność jawnie przez `b3Body_SetMassData` (kula dla zwrotnicy, pręt dla
racka). Zero broadphase, zero CCD, zero kosztu. Reguła na przyszłość: ciała
strukturalne rigu (przyszłe wahacze wizualne, elementy przegubów) trzymać
bez shape'ów albo z shape'ami o rozmiarze ≥ ruchu na krok.

## 6. Nowy sample i lab

```text
Jozz Vehicle / M6 Suspension Rig Lab   (indeks 97 przy obecnej rejestracji;
                                        95 = Lab M2, 96 = M5 First Drivable)
klawisze: W/S/A/D/Space/T/R - identycznie jak M5 First Drivable
```

Panel: typ rigu per oś (Apply), pełna geometria wahaczy (caster/KPI/ramiona/
Ackermann fraction/masa zwrotnicy — Apply), zawieszenie live (hertz/damping/
skale osi), sterowanie live (hertz/damping/siła serwa), sekcja driftu
(assist on/off, gain, min speed, max angle), envelope combo (Apply), contact
tuning, telemetria: kontakt/load/slip/camber + wykresy travel i slip angle.

Debug draw rigu (bez modeli — na tę bramkę): pręty wahaczy (żółte górne,
pomarańczowe dolne), coilover (zielony), tie/toe (cyjan), oś zwrotnicy
(magenta), rack widoczny jako poruszające się linie przy skręcie. Koła glTF
jak w M5 (visual-only attach z M3B.3).

## 7. Czego świadomie NIE zrobiono (i co jest następne)

```text
- montaż WIZUALNYCH modeli zawieszenia Jozza na hardpointach rigu
  (One_Sided_wheel_mount itd.) - to jest naturalna następna bramka:
  M4C-style visual proof, teraz z ŻYWYMI endpointami fizycznymi
- trailing arm / solid axle jako typy rigu - architektura hardpointów je
  przyjmie (trailing arm = 1 ciało ramienia + revolute do chassis +
  coilover; solid axle = belka łącząca dwa knuckle); dodać jako kolejne
  gałęzie w CreateCorner, nie przebudowa
- osobna geometria wahaczy per oś (dziś wspólna + skale osi); struktura
  descriptorów już na to gotowa
- anti-roll bar (distance joint między przeciwnymi zwrotnicami z miękką
  sprężyną - naturalne rozszerzenie)
- brak zmian w silniku Box3D (asercja TOI obeszła się rozwiązaniem
  po stronie warstwy Jozz, zgodnie z polityką projektu)
- corner lab M2 i moduł M5 nietknięte (M5 = baseline porównawczy)
```

## 8. Walidacja

```text
cmake --build --preset windows-debug --target samples --target test
cmake --build --preset windows-debug --target jozz_vehicle_validation
build\bin\Debug\jozz_vehicle_validation.exe   -> jozz_vehicle_validation: OK
build\bin\Debug\test.exe                      -> All Box3D tests passed
build\bin\Debug\samples.exe --sample 96 --frames 240   (M5, 0 sokol errors)
build\bin\Debug\samples.exe --sample 97 --frames 300   (M6, 0 sokol errors)
```

Nowe sekcje walidatora:

```text
RunM6SuspensionRigSmoke      settle/sag, pręty trzymają zwrotnice, skręt na
                             postoju (PODPISANY + trapez inner>outer), jazda,
                             skręt w ruchu (podpisany heading), hamowanie,
                             mixed rig (strut przód + wishbone tył)
RunM6DriftSelfAlignProbe     wymuszony ślizg, asercje znaku kontry i przewagi
                             assist ON nad OFF
RunM6WheelEnvelopeProbe      szerokości kolizji (tight hull AABB), test
                             "niewidzialnej ściany", sonda gładkości toczenia
                             4 trybów (split musi trzymać klasę sfery)
```

Walidator ma teraz niebuforowane stdout (setvbuf) — przy twardym assercie
silnika log pokazuje ostatnią osiągniętą linię, nie ucięcie bufora 4 KiB.

## 9. Checklist ręcznego testu dla Jozza

```text
1. Sample "Jozz Vehicle / M6 Suspension Rig Lab": jazda W/S/A/D jak w M5 -
   kierunki poprawne? (asercje podpisane pilnują, ale oko > wszystko)
2. Włącz "Rig diagnostics" i patrz na pracę prętów/coiloverów na washboardzie
   i rampach - to jest Twój multi-body rig na żywo
3. Drift: rozpędź się, zarzuć autem (flick), puść A/D - koła mają same iść
   w kontrę (HUD "align" pokazuje kierunek ślizizgu). Porównaj z wyłączonym
   "Self-align assist"; pokrętło gain wg gustu
4. Caster 5->10 deg (Apply) - mocniejsze samocentrowanie po wyjściu z zakrętu
5. Podjedź kołem do propsa BOKIEM - koło ma dotykać propsa dopiero wizualną
   oponą (split envelope), nie niewidzialną sferą 29 cm wcześniej
6. Przełącz envelope na "Sphere" (Apply) i powtórz punkt 5 - poczuj różnicę
   (to był Twój bug); "Phased union" pokaże czemu ta droga odpadła
7. Przełącz przód/tył na "Integrated strut" - porównaj feel multi-body vs M5
8. Stress: geometria wahaczy na skrajnych wartościach, ciężki chassis,
   Ackermann fraction 0..1 - szukaj granic fundamentu
```

## 10. Roadmapa rozbudowy na tym fundamencie

```text
M6.1  wizualny montaż modeli zawieszenia na żywych hardpointach (M4C-style,
      endpoint'y już się ruszają - wystarczy binding wizualny per hardpoint)
M6.2  trailing arm (model One_Sided_wheel_mount!) + solid axle jako typy
M6.3  hardpointy z markerów modelu przez kontrakt sidecar (importer wypełnia
      JozzVehicleM6WishboneHardpoints zamiast generatora)
M6.4  anti-roll bar, per-axle geometry, tuning presety (street/drift/offroad)
Soft-tire roadmapa z M5.2 sekcja 5 pozostaje aktualna (Etap 1: wizualne
zgniatanie opony z telemetrii suspensionLoad - telemetria już jest w M6)
```
