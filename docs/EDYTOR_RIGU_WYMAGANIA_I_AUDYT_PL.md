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
stoi) wymaga potwierdzenia na żywo (skręt A/D) — headless nie wciska klawiszy.
Walidator bez zmian (wizualnie). Pominięte świadomie: drążek (G3), dumper (G3),
cardan (G4), per-ramię górne/dolne osobno (dziś oba końce wahaczy na `lowerArm`).

## Część D — Otwarte pytania / decyzje

- **D1 — aranżacja:** ROZSTRZYGNIĘTE (Jozz, 2026-07-11): przód = nowy rig
  sterujący, tył = obecny mount. Jeden jezdny pojazd.
- Drążek kierowniczy: inboard → realny `rackId` (kontrakt tego wymaga), nie
  zmyślony punkt. Do wpięcia w gejcie G3.
- Cardan (`Cardan_shaft.gltf`, sockety cardanDrive/cardanHub) — nowy element,
  brak w labie; gejt G4.
- Duplikacja socketów (fallback vs kontrakt) — docelowo jedno źródło (W5).
