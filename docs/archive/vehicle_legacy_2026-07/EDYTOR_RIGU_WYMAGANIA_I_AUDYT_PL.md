> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# Edytor rigu — wymagania i audyt (żywy dokument)

Data startu: 2026-07-11. Język: PL. **To dokument ŻYWY** — dopisuję do niego przy
każdym gejcie rozgrzewki (`PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md`). Cel:
zebrać na żywo, przez realny import modelu na jeżdżący pojazd, czego edytor rigu
naprawdę potrzebuje — żeby Jozz miał największą kontrolę i największą
intuicyjność. Zero-trust: wszystko poniżej jest ze zweryfikowanego kodu, nie z
opisów.

---

## Część A — Audyt: jak rig działa DZIŚ (przed rozgrzewką)

Dwie żywe ścieżki rigowania tej samej rodziny modeli:

1. **Lab M6 (`jozz_vehicle_m6_rig_lab.cpp`, jezdny pojazd)** — riguje STARY
   `One_Sided_wheel_mount.gltf` per-kość w `Render()` (480–532) na ŻYWE ciała
   realnej fizyki M6.
2. **Bench M9 (`jozz_vehicle_m9_steering_rig_bench.cpp`, izolowany)** — riguje
   NOWY `OneSided_Steering_Suspension_Rig.gltf` na uproszczoną fizykę (statyczny
   chassis + prismatic travel + revolute steer, brak jazdy, brak realnego racka).

**Prymitywy wiązania (dzisiejszy „język rigu"):**

| Prymityw | Co robi | Gdzie |
|---|---|---|
| **bake ramki lokalnej** | `b3InvMulWorldTransforms(bodyRest, placementRest)` — zamraża pozę modelu względem ciała; live część podąża za ciałem | `SetupMountRig` (`_mount_visual.cpp:103`), M9 `CreateCorner:361` |
| **rodzic per część** | ta sama poza pieczona względem RÓŻNYCH ciał; część rysowana wybranym rodzicem | `bracketLocal` (chassis) vs `hubLocal` (knuckle) |
| **stretch-between (2 kotwice)** | część rozciągnięta między dwoma żywymi punktami na dwóch ciałach | `DrawPartBetween` + `ArmEnds`/`PartXEnds` (końce wzdłuż authored X) |
| **mirror-X** | jeden authored asset, lewy autorski / prawy lustro (negacja X) | `LoadSkinnedGltf(..., mirrorX)`, `MirrorX()` |
| **korekcja pozy koła** | authored orientacja mesha → oś koła na +Y ramki | `ComputeJozzVehicleWheelVisualCorrection`, `WheelAxleFix` |
| **authored sockety** | pozycje z kontraktu (fallback hardkodowany) | `Resolve...Sockets`, `FindJozzVehicleContractBindingByRole` |

**Źródło pozycji socketów:** kontrakt `*.asset.json` (rola → node → złożone
transformy → metry), z fallbackiem hardkodowanym gdy kontrakt nie załaduje.
Pole `ridesBody` w kontrakcie to DEKLARACJA rodzica (patrz niżej — bywa za grubo).

**Realna topologia narożnika M6** (`JozzVehicleM6CornerRuntime`):
`knuckleId` (JEŹDZI **i** SKRĘCA, bez spinu) · `upperArmId`/`lowerArmId` (ramiona
na zawiasach chassis: JEŻDŻĄ, nie skręcają) · `wheelId` (kręci się) · na poziomie
pojazdu realny `rackId`/`rackJointId`. **Brak ciała „carrier".**

## Część B — Wymagania edytora (wyprowadzone z audytu + wizji Jozza)

- **W1. Per-część picker rodzica z realnej listy ciał.** `{chassis, knuckle,
  upperArm, lowerArm, wheel, rack}`. To decyzja o znaczeniu KINEMATYCZNYM
  (skręca / tylko jeździ / kręci się), nie kosmetyka — patrz odkrycie O1.
- **W2. Tryby wiązania części:** (a) sztywno-do-jednego-rodzica; (b)
  stretch-between-dwóch-kotwic (ramiona, drążki, dumper, cardan); (c) mirror L/P.
- **W3. Pivot / punkt obrotu per część.** Bake ramki lokalnej = dziś to robi, ale
  ukryte w kodzie. Edytor musi go pokazać i pozwolić ustawić ręcznie (gizmo).
- **W4. Gizmos:** translacja/rotacja/skala ramki wiązania + podgląd na żywo; oraz
  edycja pozycji authored socketu (który jest kotwicą stretch-between).
- **W5. Model danych = jedno źródło prawdy.** Dziś: kontrakt + hardkodowane
  fallbacki (rozjazd, patrz `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md §3`). Edytor musi
  zapisywać binding do JEDNEGO miejsca (rozszerzenie kontraktu v2 lub osobny plik
  rigu) — bez duplikacji.
- **W6. Determinizm/headless.** Rejestr env + zrzuty `--screenshot`; bramka
  walidatora bajt-w-bajt gdy rig jest czysto wizualny (nie dotyka fizyki).
- **W7. `ridesBody` z kontraktu bywa za grubo** i musi być nadpisywalny per
  realna topologia (O1). Edytor = miejsce tego nadpisania.

## Część C — Odkrycia na żywo (dopisywane per gejt)

**O1 (G0, z kodu). Rodzic części to decyzja FIZYCZNA, nie kosmetyczna.**
Kontrakt deklaruje `ChassisMount_b.ridesBody = knuckle` i `WheelCenter.ridesBody
= knuckle` — oba na knuckle. Ale Jozz wielokrotnie powtórzył: WheelCenter i
ChassisMount_b muszą być OSOBNO (WheelCenter skręca z kołem, ChassisMount_b nie).
Bench M9 rozwiązał to zmyślonym ciałem „carrier" (jeździ, nie skręca). **Realny
M6 nie ma carriera** — knuckle jeździ i skręca naraz. Wniosek: żeby utrzymać
rozdział w prawdziwym poje­ździe, ChassisMount_b musi jechać na **ramieniu**
(`lowerArm` — jeździ, nie skręca), a WheelCenter na `knuckle`. Mapowanie
ról M9 → realne ciała M6: chassis→`chassisId`, carrier→`lowerArmId`,
knuckle→`knuckleId`. To pierwszy i najtwardszy wymóg edytora (W1/W7).

**O2 (G1, na żywo). Bake = „poza roota modelu", rodzic = osobna warstwa.**
Import zadziałał przez DOKŁADNIE tę samą matematykę co stary mount (`SetupMountRig`:
yaw −90, wyląduj socket WheelCenter na inboard-face koła), tylko rozbitą na 3
ciała zamiast 2. Wniosek dla edytora: są DWIE niezależne warstwy — (a) poza roota
modelu (jedna ramka world, ustawiana gizmem), (b) przypisanie każdej części do
ciała-rodzica. Dziś obie są zaszyte w kodzie; edytor musi je rozdzielić i pokazać
osobno (W3/W4). Trzy piecze z jednej pozy `placementRest` = potwierdzenie, że to
naturalny model.

**O3 (G1, na żywo). Klasyfikacja per `boneNodeIndex` jest krucha (magic numbers).**
Rysowanie zależy od twardych indeksów węzłów (3,5 ramiona; 6 ChassisMount_b; 8
WheelCenter; 7 drążek). Jak asset zmieni układ węzłów — rig rozjedzie się PO
CICHU. To ta sama kruchość co w benchu M9. **Twardy wymóg edytora:** wiązać po
ROLI z kontraktu (semantyka), nie po numerze węzła (wzmacnia W1/W5/W7). Edytor =
miejsce, gdzie rola→ciało jest jawną, edytowalną tabelą.

**O4 (G1, na żywo). Część elementów potrzebuje kotwic SPOZA modelu.** Drążek
(inboard→realny `rackId`), dumper (sockety upper/lower na dwóch ciałach), cardan
(osobny asset `Cardan_shaft.gltf`, chassis↔knuckle) nie dają się związać samą
klasyfikacją części. Dlatego świadomie pominąłem drążek w G1. **Wymóg edytora:**
tryb wiązania „kotwica = punkt na INNYM ciele/joincie" (np. koniec racka), nie
tylko „część sztywno na ciele" (rozszerza W2).

**O5 (G1, na żywo). Konwencja rodziny jednostronnej trzyma się dla nowego modelu.**
Mirror-X i korekcja pozy koła zadziałały bez zmian (jeden asset, L autorski / P
lustro, yaw −90). Edytor może przyjąć tę konwencję jako DOMYŚLNĄ, z jawną opcją
nadpisania per model.

**Stan weryfikacji G1:** render potwierdza — nowy rig na przedniej osi, tył =
stary mount, symetria L/P, przyczepiony do żywych ciał, toggle przełącza tylko
przód. Rozdział statyczny poprawny; DYNAMICZNY (WheelCenter skręca / ChassisMount_b
stoi) — **POTWIERDZONY na żywo przez Jozza** (test skrętu A/D, 2026-07-11): koło +
wewnętrzna piasta skręcają, ChassisMount_b + ramiona stoją. Uwaga: rozdział jest
DYNAMICZNIE „zbyt osobny" — części rozjeżdżają się przy skręcie za bardzo (dług
techniczny #14, odłożone decyzją Jozza; to kandydat na wczesny test edytora, bo
pivot per część steruje właśnie tym rozjazdem). Walidator bez zmian (wizualnie).

**O6 (G3, na żywo). Kotwica na innym ciele potrzebuje SUB-pozycji, nie tylko
ciała.** Drążek związany z KOŃCEM racka (`{0,0,±rackHalfWidth}`) wyszedł za krótki
— końce maglownicy M6 siedzą tuż przy knucklu, więc drążek był kikutem. Jozz:
„prawy z lewym się łączył". Rozwiązanie: inboard obu drążków → ŚRODEK racka
(`{0,0,0}`), więc lewy i prawy spotykają się w jednym punkcie i jadą razem z
maglownicą. Wniosek dla edytora: tryb „kotwica na innym ciele" (O4/W2) to nie tylko
WYBÓR ciała, ale i PUNKT na nim (koniec vs środek vs socket) — parametr sub-pozycji
kotwicy. To samo dotyczyłoby np. dwóch końców racka jako osobnych kotwic.

**O7 (G3, na żywo). Trzeci tryb wiązania: socket zwisający z części stretch-between.**
Dolne oko dumpera nie jest ani „część na ciele", ani „stretch między ciałami" —
to PUNKT authored zaczepiony na dolnym RAMIENIU, niesiony przez żywą pozę tego
ramienia (`JozzVehicleComputeArmPlacement` + `JozzVehicleMapAuthoredPoint`). Górne
oko to socket na chassis. Wniosek dla edytora: oprócz W2(a) sztywno-na-ciele i
W2(b) stretch-between potrzebny jest tryb (d) „socket niesiony przez pozę części
stretch-between" (interpolacja wzdłuż ramienia), inaczej dumper/przewody nie dadzą
się związać poprawnie.

**Stan weryfikacji G3:** render potwierdza — drążek rysowany od środka racka do
knuckla (dłuższy, L/P łączą się w centrum), dumper rigu rysowany między górnym
okiem na chassis a dolnym na ramieniu (diff pikseli on/off = pionowy amortyzator).
Walidator bez zmian (wizualnie). Bo drążek pinuje inboard tym SAMYM punktem świata
co linia racka w `DrawRigDiagnostics` — zbieżność z konstrukcji. Pominięte dalej:
cardan (G4), per-ramię górne/dolne osobno.

**O8 (Etap 3 finalizacji, decyzja D3 Jozza 2026-07-11). Importer in-game +
model z Blockbench jako CIAŁO KOLIZYJNE.** Przy pytaniu o artefakt kolidera
chassis Jozz rozszerzył wymaganie poza rysowanie: (a) warstwa kolizji ma być
zawsze przełączalna do podglądu (ZROBIONE w Etapie 3: `SetShapeHidden` na bryle
chassis + checkbox Debug „Pokaż bryłę kolizyjną nadwozia" + `JOZZ_M6_COLLIDER`);
(b) docelowo ma dać się IMPORTOWAĆ model z Blockbench, który ROBI ZA collision
body — szybkie podmienianie i testowanie innych koliderów; (c) importer
in-game jest konieczny dla edytora rigu w ogóle: „tam będziemy rigować
wszystko" — modele zawieszeń, body, cardan shaft. Wniosek dla edytora: obok
warstwy WIZUALNEJ (rejestr nadwozi, Etap 1) potrzebna jest warstwa KOLIZYJNA
zasilana importem (mesh → hull(e) fizyki, zapewne przez istniejący pipeline
kontraktów assetów) + UI importu w aplikacji, nie tylko pliki wrzucane ręcznie.
To jest FIZYKA (tworzenie shape'ów), nie rysowanie — wymaga własnego planu i
osobnej zgody na start; nie mieściło się w zakresie Etapu 3 (defaults+draw).

## Część D — Otwarte pytania / decyzje

- **D1 — aranżacja:** ROZSTRZYGNIĘTE (Jozz, 2026-07-11): przód = nowy rig
  sterujący, tył = obecny mount. Jeden jezdny pojazd.
- Drążek kierowniczy: ROZSTRZYGNIĘTE (G3) — inboard → ŚRODEK realnego `rackId`
  (`{0,0,0}`), lewy i prawy łączą się w centrum (Jozz), jadą z maglownicą.
- Cardan (`Cardan_shaft.gltf`, sockety cardanDrive/cardanHub) — nowy element,
  brak w labie; gejt G4.
- Duplikacja socketów (fallback vs kontrakt) — docelowo jedno źródło (W5).
