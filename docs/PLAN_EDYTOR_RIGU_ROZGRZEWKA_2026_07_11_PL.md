# Plan: rozgrzewka pod edytor rigu — import steering-suspension do labu M6

Data: 2026-07-11. Autor: sesja Jozz Vehicle. Język: PL.
Status: **W TOKU.** D1 = D1a (przód nowy / tył stary, decyzja Jozza). **G0 ZROBIONE**
(żywy dokument audytu + O1). **G1 ZROBIONE** (import na przód, związany na żywe
ciała z rozdziałem WheelCenter/ChassisMount_b; toggle `JOZZ_M6_STEERING_RIG` +
checkbox Debug; walidator bez zmian; render obejrzany). Rozdział dynamiczny
**POTWIERDZONY przez Jozza na żywo** (skręt A/D); rozjazd „zbyt osobny" → dług #14.
**G3 ZROBIONE** (drążek: inboard→ŚRODEK racka, L/P łączą się w centrum — reguła
Jozza; dumper rigu: górne oko chassis / dolne ramię; walidator bez zmian; render
obejrzany). Odkrycia O2–O7 w dokumencie wymagań. Nadwozie (`Nadwozie.gltf`)
podłączone sztywno pod chassis (a275947, poza sekwencją gejtów, prośba Jozza).
**FINALIZACJA ZAMKNIĘTA (2026-07-11, Etapy 1-3
`PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md`):** stan rozgrzewki wszedł
do gry jako DOMYŚLNY — rama rurowa + rig kierowniczy na przodzie są fabrycznym
configiem, przeżywają R/presety, kolider chassis chowa się pod ramą (szczegóły
tam, ten plan tylko wskazuje). G4 (cardan) zostaje opcjonalny. Nowe twarde
wymaganie z decyzji D3: **O8 — importer in-game + model z Blockbench jako
collision body** (dokument wymagań, część C) — kandydat na pierwszy plan
właściwego edytora.

Powiązane: `EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md` (żywy dokument wymagań+audytu,
zakładany przy G1), `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md`, kontrakt
`assets/contracts/one_sided_steering_suspension.asset.json`.

---

## 0. Po co to (wizja Jozza, moja analiza)

Docelowy cel to **edytor rigu**: gizmos (translacja/rotacja/skala), ręczna
kontrola zależności rodzic→dziecko między modelami, kontrola punktu obrotu
(pivota) per część. To długa i poważna procedura.

Zamiast projektować edytor „na sucho", robimy **rozgrzewkę = jednocześnie
ultimate research + audyt rigowania**: importujemy nowy model
`OneSided_Steering_Suspension_Rig` do **jezdnego** labu M6, tak aby przednie i
tylne zawieszenie dało się testować razem w jednej scenie. W trakcie wiązania
tego modelu na ŻYWE ciała realnego pojazdu na własnej skórze przeżywam, czego
edytor naprawdę potrzebuje — a każdy taki wymóg trafia do żywego dokumentu
wymagań. Render jest bramką: nie „liczby OK", tylko obejrzany obraz.

## 1. Stan faktyczny (zweryfikowany w kodzie, nie z opisów)

**Nowy model jest już zrigowany — ale tylko w izolacji.**
`samples/jozz_vehicle_m9_steering_rig_bench.cpp` pokazuje 2 narożniki nowego
modelu na UPROSZCZONEJ fizyce (statyczny chassis anchor + prismatic travel +
revolute steer; brak jazdy; brak realnego racka; ramię bez własnego ciała).
Kontrakt: `v0Treatment = visual_only_isolated_bench_before_vehicle_integration`.

**Lab M6 to realny, jezdny pojazd.**
`CreateJozzVehicleM6` buduje na narożnik: `knuckleId` (upright: JEŹDZI **i**
SKRĘCA, bez spinu), `upperArmId`/`lowerArmId` (ramiona na zawiasach chassis:
jeżdżą, NIE skręcają), `wheelId` (kręci się). Na poziomie pojazdu: realny
`rackId` + `rackJointId` (przednia oś wahacza). Dziś riguje STARY
`One_Sided_wheel_mount.gltf` (`m_riggedMountL/R`) w `Render()`
(`jozz_vehicle_m6_rig_lab.cpp:480–532`): brackety+ramię na chassis
(`bracketWorld`), hub/WheelCenter na knuckle (`hubWorld`), ramiona
`DrawPartBetween(chassisEnd→wheelEnd)`. Bake per-narożnik: `SetupMountRig`
(`jozz_vehicle_m6_rig_lab_mount_visual.cpp:103`) piecze `m_bracketLocal[corner]`
(chassis-local) i `m_hubLocal[corner]` (knuckle-local).

**Prymitywy wiązania (to samo w obu ścieżkach — to jest „język rigu" dziś):**
- bake dwóch ramek lokalnych względem dwóch RÓŻNYCH ciał (`bracketLocal`/
  `hubLocal`) — część podąża za swoim rodzicem.
- `DrawPartBetween(A,B)` — część rozciąga się między dwoma żywymi kotwicami na
  różnych ciałach (ramiona, drążki, dumpery). `ArmEnds`/`PartXEnds` liczą końce
  wzdłuż authored X.
- konwencja mirror-X (jeden authored asset, L autorski / P lustro).
- authored sockety z kontraktu (`ResolveJozzVehicleSteeringSuspensionSockets`)
  z fallbackiem hardkodowanym.

## 2. Kluczowe odkrycie badawcze (rdzeń rozgrzewki) — niezgodność topologii

Bench M9 rozdziela **WheelCenter** (na knuckle: SKRĘCA) od **ChassisMount_b**
(na carrierze: JEŹDZI, nie skręca) — realizując Twój wielokrotnie powtórzony
wymóg „WheelCenter i ChassisMount_b OSOBNO" (`[[feedback-listen-literally...]]`).

Ale **realny narożnik M6 nie ma ciała „carrier"** — knuckle jeździ **i** skręca
naraz. Rola „jeździ-ale-nie-skręca" w prawdziwym pojeździe należy do **ramion**
(upper/lower arm). Wniosek, który definiuje edytor:

> **Rodzic części to decyzja o znaczeniu FIZYCZNYM, nie kosmetyka.** Ten sam
> socket (ChassisMount_b) w izolowanym benchu jedzie na „carrierze", a w realnym
> pojeździe musi jechać na **ramieniu**, żeby zachować rozdział od WheelCenter.
> Kontrakt (`ridesBody: knuckle` dla obu) jest tu za grubo — realna topologia
> wymaga per-część wyboru rodzica z listy realnych ciał.

To pierwszy twardy wymóg edytora: **per-część picker rodzica** z realnej listy
ciał (`chassis, knuckle, upperArm, lowerArm, wheel, rack`), z konsekwencją
kinematyczną (skręca / tylko jeździ / kręci się). Dalsze wymogi zbiera żywy
dokument.

## 3. Decyzja D1 — aranżacja (DO POTWIERDZENIA, definiuje gejty)

Trzy warianty „przednie i tylne zawieszenie razem":
- **D1a (rekomendacja): przód = nowy rig sterujący, tył = obecny mount.** Jeden
  jezdny pojazd, dwie różne konstrukcje naraz — dokładnie „przód i tył razem".
  Zgodne z kontraktem (`front ... replacement`). Nowy model MA drążek kierowniczy
  i cardana → należy na oś skrętną (przód).
- **D1b: wszystkie 4 narożniki = nowy rig.** Spójny wygląd, ale tył dostaje
  drążek/cardana bez sensu fizycznego (tył nie skręca) — więcej pytań niż
  wartości na tym etapie.
- **D1c: nowy rig OBOK obecnego** (drugie stanowisko / drugi pojazd do A/B),
  bez mieszania na jednym aucie.

## 4. Domyślne zasady wykonania (moja decyzja, do zawetowania)

- **Najpierw wyłącznie WIZUALNIE.** Nowy model to skóra na ISTNIEJĄCEJ, realnej
  fizyce M6 (te same ciała, te same jointy). Zero zmian w fizyce → **walidator
  zostaje bajt-w-bajt identyczny** (spójne z rygorem serii R). „Test zawieszenia"
  jest realny, bo fizyka jest realna; nowy mesh tylko ją wizualizuje.
- **Przełącznik, nie kasowanie.** Stary mount zostaje jako fallback; nowy rig
  włączany env `JOZZ_M6_STEERING_RIG=0/1` + checkbox w zakładce Debug. Pozwala na
  A/B i chroni przed regresją.
- **Mały gejt = obejrzany render.** Każdy etap: zmiana → build 3/3 → walidator
  (czytaj liczby, musi być identyczny) → `--screenshot` → checkpoint. Osobno
  aktualizacja żywego dokumentu wymagań.
- **Drążek kierowniczy → REALNY rack.** Inboard drążka pinujemy do realnego
  `rackId` w `rackEndZ` (jak `DrawRigDiagnostics` już robi linią racka,
  `mount_visual.cpp:210–214`), NIGDY do zmyślonego punktu — kontrakt tego wprost
  zabrania.

## 5. Gejty (arc dla D1a; przy D1b/c drobne korekty)

- **G0 (audyt, bez kodu):** założyć `EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md`;
  spisać część A (jak dziś działa binding) + seed odkryć z §2. Zarejestrować oba
  docy w `CURRENT_STATE_INDEX_PL.md` + liście `doc_drift_check.ps1`.
- **G1 (load + toggle):** wczytać nowy rig L/P w labie (obok starego), env+UI
  toggle, status kontraktu/rigu w Debug. Jeszcze BEZ wiązania — tylko że się
  ładuje i pokazuje surowo. Render: model widoczny gdzieś w scenie.
- **G2 (bind przód wizualnie):** na przednich narożnikach związać części na
  WŁAŚCIWE realne rodziny: WheelCenter+drążek-outboard na `knuckleId`;
  ChassisMount_b + końce ramion na `lowerArmId`/`upperArmId` (rozdział z §2);
  ChassisMount_a + damperMount na `chassisId`; ramiona `DrawPartBetween`. Bake
  w stylu `SetupMountRig`. Render: przód nowy, tył stary, symetria L/P, skręt
  rusza tylko WheelCenter+koło. **Tu spływa najwięcej wymogów edytora.**
- **G3 (dumpery + drążek + sockety):** dumper na authored socketach; drążek
  inboard→realny rack; overlay socketów (opcjonalnie). Render.
- **G4 (cardan, opcjonalnie):** wał cardana (`cardanDrive` chassis →
  `cardanHub` knuckle) jako stretch-between; nowy element, brak w labie dziś.
- Każdy gejt: wpis do żywego dokumentu (co nowego wymusił na edytorze).

## 6. Bramka / weryfikacja

- Walidator: **bajt-w-bajt identyczny** przez cały czas (dowód, że rozgrzewka jest
  czysto wizualna i nie ruszyła fizyki). Jeśli się rozjedzie — coś nieświadomie
  dotknęło fizyki, STOP.
- Render `--screenshot` na każdym gejcie (przód+tył w jednym kadrze, skręt A/D,
  travel). Obraz decyduje, nie liczby.
- `doc_drift_check.ps1` czysty przed każdym commitem; docy zarejestrowane.

## 7. Czego NIE robimy w rozgrzewce

- Nie budujemy jeszcze samego edytora (gizmos/UI) — to następny etap, karmiony
  wymaganiami z tej rozgrzewki.
- Nie zmieniamy fizyki M6 (żadnych nowych ciał/jointów; drążek to wizual pięty do
  istniejącego racka).
- Nie ruszamy silnika (`src/`,`include/`), `main`, benchu M9 (zostaje jako
  referencja izolowana).
