> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# JES_03 — Program realizacji, rozstrzygnięcia i stan

Warstwa: **PROGRAM / STAN / DECYZJE**. Wersja: 1.0-kandydat (2026-07-15,
scalenie programów obu pakietów: „X" Claude + „HKP" Sol). Dokument żywy.

Zasada scalenia: szkielet programu przejęty z HKP (dojrzalszy podział faz,
poprawiony przez red-team Sol na wczesne canary), z wstrzykniętymi twardymi
faktami silnika, wynikami eksperymentu X0 i kryteriami z pakietu Claude.
Konflikty rozstrzygnięte jawnie w §7 — nie uśrednione.

---

## 1. Filozofia realizacji (dyrektywy Jozza + korekty scalenia)

- **Lab-first** (OD-06): systemy powstają osobno w labach, składanie
  później. Trzy warstwy trwałości labów: JES_02 §5.
- **Eksperyment przed dyskusją**: wiele decyzji zapadnie „po tym, jak
  sprawdzają się w rzeczywistości" — program akceptuje okresowy chaos;
  amortyzują go laby, kontrakty granic i dziennik decyzji.
- **Mała mapa, brak presji casual** (OD-07); game-loop produktowy na końcu.
- **Korekta S0 (z pakietu Sol, przyjęta):** „składanie na końcu" dotyczy
  GRY, nie hydrauliki projektu — od pierwszej promocji kontraktu istnieje
  **ciągły szkielet integracyjny S0**: mały, stale zielony test-przepływ
  konsumujący każdy promowany kontrakt. Zapobiega big-bang integracji
  i „laboratorium-muzeum". To nie jest game-loop — to instalacja.
- **Feel per lab teraz, produkt na końcu** (konstytucja, zasada 11);
  prymitywna jazda pojawia się już w Canary M, nie po roku infrastruktury.
- **Pionowo, nie poziomo**: każdy etap przecina cały stos i dowodzi
  sprzężenia (konstytucja, zasada 6).

## 2. Scalony program fundamentu

Kryteria każdego etapu doprecyzowuje jego Lab Charter (szablon: JES_05)
PRZED implementacją; progów nie luzuje się po porażce (kodeks §11).

| Etap | Cel / treść | Kluczowe kryteria i fakty | Status |
|---|---|---|---|
| **P-1 Heritage Freeze** | zamrozić baseline'y studium (VAW: commit/archiwum po weryfikacji manifestu; JV: `445db88` + osobno kwarantanna dirty), rejestr źródeł, zgoda Jozza na listę przenoszonych assetów właściciela | żadnego benchmarku przeciw niezamrożonemu dirty; szczegółowa charakteryzacja per-capability dopiero przy jej labie (JES_04) | OTWARTE |
| **P0 Evidence Bootstrap** | minimalny zapis dowodu: `RUN-ID`/run.json (komenda, exit code, rewizja, hashe wejść, artefakty) + **testy-sabotaże**: build non-zero, nieuruchomiony test, brak artefaktu, stary timestamp, zła wersja fixture'a, dirty udający clean | każda kontrolowana awaria wykryta i niemaskowalna późniejszym sukcesem; to jest bezpośrednia odpowiedź na realny incydent JV („build OK" przy nieistniejącym exe, zielony gate na starym obiekcie) | OTWARTE |
| **P1 Trzy realne canary** (disposable) | **A — Art Truth:** pierwszy asset Jozza → preview → stały kadr → ręczny werdykt. **M — Mechanics/Feel:** force-at-point / one-corner + prymitywny rolling rig; telemetria, input, latencja, pierwszy werdykt feelu. **T — Terrain/Stack Risk:** stanowy deformowalny patch, update collidera/mesha, readback, throughput | każde canary: 2 osobne werdykty (`EVIDENCE_COMPLETE` i `CAPABILITY_OR_FEEL`) + `NEXT = CONTINUE/ITERATE/REJECT_STACK`. **Fakty dla T:** box3d NIE MA update-in-place heightfielda (`collision.h:380` — tylko Create/Destroy) → porównać strategię A (per-chunk heightfield destroy+create) vs B (box-hulle kolumn); kryteria: 0 utraty kontaktów kół maszyny stojącej na sąsiednim chunku, bilans masy Δ=0, rebuild < 1 ms/chunk, brak impulsów-widmo | OTWARTE (T ma gotową specyfikację) |
| **P2 Cienki spec** | `ExperimentAssemblySpec v0` (2–3 części, trwałe authored ID, 1 constraint/aktuator z realnych potrzeb A/M) → walidacja → semantic IR → diagnostyka | round-trip zachowuje ID i znaczenie; spec/IR bez typów silnika; **częściowo zderyskowane eksperymentem X0** (`spike/kernel_v0`: katalog→blueprint→kompilator→box3d, rover jedzie 6,25 m/s, hash trajektorii identyczny run-do-run `a6cca9017df176c1`) | OTWARTE (X0 = dowód wykonalności ścieżki) |
| **P3 Atomowy authoring** | jedna transakcja, jedno undo, save/load, snapshot do Run, Stop bez mutacji źródła | nieudana transakcja zostawia identyczny hash speca; UI i automatyzacja używają JEDNEJ ścieżki mutacji | OTWARTE |
| **P4 Granica backendu** | capability check → backend-specific lowering → lifecycle build/start/step/stop/dispose; mapy authored↔runtime; rollback częściowej budowy | typy natywne kończą się na adapterze; partial failure nie zostawia zasobów; fakty integracyjne z X0: statyczny CRT `/MTd`, oś revolute = lokalne Z, `b3World_EnableContinuous(false)` dla światów pojazdów (L-prawa, JES_02 §8) | OTWARTE |
| **P5 Formalizacja asset truth** | wiedza z Canary A → kontrakt: manifest (semantic ID/GUID, role, units, axes, provenance, renderer-only) → validate → preview → bind → runtime compare; hot-reload | mechanika identyczna po podmianie grafiki; fallback jawny i diagnostyczny (nie udaje sukcesu); duplikaty nazw węzłów glTF = znany przypadek testowy; sonda IN-GAME (lekcja L11) | OTWARTE |
| **P6A Force Truth Lab** | force-at-point, kierunek lokalny, moment, telemetria; opcjonalny Flight Lab (thruster/gimbal, 24 orientacje) — dziedzictwo VAW (VectorThruster 192 przypadki jako punkt odniesienia) | wyłączenie urządzenia usuwa jego wkład sił; visual mismatch = osobny FAIL; lot NIE blokuje programu pojazdu | OTWARTE |
| **P6B One-Corner Lab** | najcenniejszy instrument JV w formie engine-neutral: `SuspensionCornerSpec`, rest/design pose, bump/droop, mirror L/R, branch safety | over-center/gałąź lustrzana wykrywane PRZED runtime (L2); analityczna część może biec zaraz po P1, fizyczna po P4 | OTWARTE |
| **P7 Two-Corner Steering** | rack, tie-rody, lokalne przestrzenie, reparent/persistence | zamrożenie racka gasi badaną ścieżkę momentu (falsyfikacja mechanizmu — CAP-JV-09); sockety wizualne nie definiują frame'ów | OTWARTE |
| **P7.5 Drivable Canary** | jawnie tymczasowy chassis/rolling rig: input, kamera, napęd, hamulec — feel wraca szybko | Jozz przejeżdża stały fixture; latencja zmierzona; canary NIE rośnie w drugi M6 — po ekstrakcji wiedzy wyrzucany | OTWARTE |
| **P8 Four-Corner + Visual Rig** | pełny chassis-instrument: masy, preload, bump/droop, ARB jako jawna hipoteza, lądowania, osobny rig wizualny | po dropie bezpieczna gałąź + jazda/skręt/hamowanie (post-abuse function, CAP-JV-05); bind/rest/live overlay; Jozz widzi WŁAŚCIWY model | OTWARTE |
| **P9 Reference Vehicle** | minimalny pojazd-instrument: napęd momentem z limitem mocy, coast, brake, `RigidSurfaceContact v0` (jawnie tymczasowy), 2 nawierzchnie, rampa/washboard/szew, presety factory+overrides | wheelspin z walki moment-przyczepność (nie speed servo); settle→drive→steer→brake i drop→land→drive→steer; ręczna jazda Jozza z opisanym werdyktem | OTWARTE |
| **S0 Integration Spine** (ciągły od P2) | mały przepływ konsumujący każdy promowany kontrakt | `ADOPT` bez wpisu S0/`NOT_FOR_INTEGRATION` jest niekompletny | OTWARTE |
| **P10 Acceptance Slice** | końcowy odbiór zakresu: utwórz→zmień→skompiluj→przypnij asset→jedź→zapisz dowód→wróć do niezmienionego dokumentu; usunięcie implementacji labów nie psuje slice'a | workbench importuje tylko promowane API; Jozz przejeżdża i ogląda | OTWARTE |

## 3. Mapowanie programu X (pakiet Claude) → program scalony

Nic z programu X nie zginęło; zmieniło adres:

| X | Los w programie scalonym |
|---|---|
| X0 blueprint→kompilator→box3d, determinizm | **ZALICZONY 2026-07-15** (`spike/kernel_v0`); dowód wykonalności ścieżki P2+P4 |
| X1 „Żywy grunt" | = **Canary T** (P1) + dalszy Terrain Research Track; kryteria i strategie A/B przeniesione 1:1 (tabela §2) |
| X2 praca przez maszynę (opór łyżki→aktuator; ładunek zmienia masę/COM) | Terrain Track: laby **Blade–Soil / Wheel–Deformable Patch / Material Accounting** (konwergencja z Sol §14) — po instrumencie z P9 |
| X3 stan nawierzchni→jazda (walec, zagęszczenie) | Terrain Track, po P9; dwie twarde nawierzchnie wchodzą wcześniej w P9 |
| X4 powietrze na tym samym rdzeniu | P6A opcjonalny Flight Lab → osobny program lotu po HKP |
| X5 logistyka z bilansem | post-HKP (po Material Accounting) |
| X6 sygnał sensor→kontroler→aktuator | post-HKP; kontrakt portów `{partId, portId}` rezerwowany od P2 |

## 4. Dwa progi programu terenu (przyjęte z pakietu Sol — precyzyjniejsze)

- **TERRAIN_RESEARCH_START (wcześnie, po P0/P1):** disposable Canary T
  z własnym fixture i dowodem; jawnie tymczasowy model materiału; ma
  OBALIĆ nieodpowiedni stack wcześnie, nie stworzyć świat. Jeżeli obie
  strategie collidera padną — zmieniamy reprezentację materii, nie wizję.
- **TERRAIN_VEHICLE_INTEGRATION (później):** pełny Wheel–Soil/Blade–Soil
  dopiero po wiarygodnym pojeździe-instrumencie (P9): powtarzalne
  obciążenie, telemetria slip/nacisk/praca, rozróżnienie „błąd maszyny"
  od „błąd gruntu", potwierdzenie Jozza że instrument wystarcza.

## 5. Doktryna dowodów

- **Drabina dowodowa** (raportowana OSOBNO, nie jako jedno „PASS"):
  STATIC / UNIT / CONTRACT / INTEGRATION / SOAK-ABUSE / PERFORMANCE /
  VISUAL / MANUAL / PROVENANCE. Wynik etapu to wektor, np. „INTEGRATION
  PASS, VISUAL FAIL, MANUAL NOT RUN" — koniec z zielonym totalem
  zasłaniającym zły obraz.
- **Testy negatywne rosną per zdolność** (obowiązek z P0): runtime dodaje
  test martwej ścieżki spec, asset — celowy visual mismatch, persystencja
  — uszkodzone wejście, performance — zły workload ID.
- **Wydajność:** każdy claim ma nazwany workload, sprzęt, profil
  debug/release, p50/p95/max i granicę pomiaru; optymalizacja tylko po
  profilu (szczegóły: pakiet Sol 02 §9 — obowiązujące).
- **Dowód wizualny/feel:** stałe kamery, capture, ręczny sign-off Jozza —
  osobny rodzaj dowodu (zasada 11, L7).

## 6. Pipeline promocji z labu

```text
PROPOSE → CHARACTERIZE legacy (JES_04) → SPECIFY kontrakt → IMPLEMENT w labie
→ FALSIFY → COMPARE (MATCH/IMPROVE/BREAK świadomie) → ADOPT | ITERATE | REJECT | DEFER
→ ekstrakcja minimalnego kernela → dowód usuwalności labu → wpis S0
```

`ADOPT` wymaga: zamkniętej Lab Charter, dowodu właściwej ścieżki runtime,
audytu granic, jawnych limitów, baseline'u wydajności, werdyktu
visual/manual gdy dotyczy, migration/exit story, wpisu S0 albo jawnego
`NOT_FOR_INTEGRATION`. `REJECT` zachowuje fixture, wyniki, Failure Card
i reopen trigger.

## 7. Rozstrzygnięcia konfliktów obu pakietów (jawne, bez uśredniania)

| # | Konflikt | Rozstrzygnięcie + powód |
|---|---|---|
| 1 | Sol: pełny intake (atomizacja twierdzeń, 2–3 ślepe ekstrakcje, fidelity audit 100% Tier A) + burza R0–R10 przed czymkolwiek. Claude: groźba paraliżu analitycznego. | **Procedura Sol ZREDUKOWANA, esencja zachowana.** Konfrontacja pakietów odbyła się TUTAJ: oba źródła zachowane bez zmian, konflikty rozstrzygnięte jawnie, wszystko nieratyfikowane ma status kandydata. Zachowane na stałe: hierarchia źródeł, słownik statusów, szablony, reguły anty-dryf, lekki przegląd adwersaryjny na bramkach (JES_05). Pełny protokół = uśpiony, z triggerem reaktywacji (JES_05 §8). Powód: jeden właściciel + kilku agentów; koszt pełnej procedury (tygodnie pracy agentów, wynik = dokumenty) łamie OD-14 ducha „eksperyment przed esejem" i własne ryzyko R3 Sol-a. |
| 2 | Sol: stack w pełni otwarty (OQ-01/02: język? silnik gotowy vs custom?). Claude: D1 podjęte. | **D1/D9 są OWNER_DIRECTIVE** (OD-04/OD-05, z datą) — Sol pisał równolegle, bez wiedzy o tych decyzjach. NIE reotwieramy „engine vs custom". Otwarte pozostaje wnętrze stacku (ECS, dokładne biblioteki, double precision) — przez spiki, zgodnie z duchem RFC Sol-a. Wymóg Sol-a „exit strategy zależności" przyjęty w całości (zasada 16). |
| 3 | Claude: laby trwałe („obywatele produktu"). Sol: laby usuwalne, produkt nie importuje labów. | **Sol ma rację w warstwie kodu, Claude w warstwie infrastruktury.** Trzy trwałości: host labów trwały / implementacja labu usuwalna / promowany wynik (kontrakt+fixture+regresja) trwały. Zapis: JES_02 §5, konstytucja 15. Powód: CAP-JV-10 (monolit M6) to najlepiej udokumentowana porażka JV. |
| 4 | Claude: pełna architektura CraftGraph w JES_02. Sol: cienki `ExperimentAssemblySpec v0`, zakaz projektowania MachineDocument z wyobraźni. | **Sol ma rację.** CraftGraph = kształt docelowy (hipoteza); pierwsza struktura = cienki spec rosnący przez bramki promocji. Zapis: JES_02 §4. |
| 5 | Claude: „cała wizja czeka na X1". Sol: badanie terenu wcześnie, integracja po instrumencie. | **Sol precyzyjniejszy — przyjęte jego dwa progi** (§4). Zachowana z Claude: konkretna specyfikacja Canary T z faktem `collision.h:380` i kryteriami liczbowymi. |
| 6 | Claude: gate v2 (timestampy). Sol: P0 Evidence Gate Integrity z testami-sabotażami. | **Sol szerszy — przyjęty P0**; wymogi timestampów z gate v2 wchłonięte do listy sabotaży. |
| 7 | Claude: składanie gry na końcu (za Jozzem, OD-07). Sol: ciągły S0 spine od P2. | **Oba — bo mówią o czym innym.** S0 = instalacja/regresja przepływu (ciągła); game-loop produktowy = na końcu (OD-07). Zapis: §1. |
| 8 | Sol: „Windows-first, box3d jako kandydat" = hipotezy. Claude: box3d zwalidowany dwoma projektami. | box3d = **wybrany pierwszy backend za granicą adaptera** (D5-kierunek + SH-03 Sol-a jednocześnie): budujemy na nim, ale domena go nie zna, a Terrain Track ma prawo wskazać jego granice (Coulomb friction vs miękki grunt). |
| 9 | Numeracja programów: X (Claude) vs P/HKP (Sol). | **P-numeracja Sol-a przyjęta** (granularniejsza, po rewizji red-team); mapowanie X→P w §3, nic nie zginęło. |
| 10 | Sol: ciężka bramka NEW_REPO_READINESS za burzą i ratyfikacjami. Jozz: następne zadanie = plan czystego projektu i przenosiny. | Bramka **ODCHUDZONA do checklisty §12** — decyzje właściciela + P-1 freeze + brak blokera szkieletu; bez wielorundowej ceremonii. Dyrektywa właściciela (OD-15 zrealizowana tym pakietem) wygrywa z procedurą agenta. |

## 8. Ryzyka programu (przyjęte z Sol, obowiązujące)

R1 second-system (sygnał: dziesiątki interfejsów przed 2. konsumentem) ·
R2 laboratorium-muzeum (sample bez promocji → S0 wymusza konsumpcję) ·
R3 architecture astronautics (3. milestone dokumentów bez assetu/feelu →
STOP, wracamy do kodu) · R4 adapter theatre (port = przemianowane API
jednego backendu) · R5 feel za późno (prymitywna jazda już w Canary M,
potem P7.5, P9) · R6 fałszywa wymienność (capability queries zamiast
najniższego wspólnego mianownika) · R7 lot+samochód naraz (pierwszy
drivable target = pojazd lądowy; lot opcjonalny) · R8 clean-room jako
mechaniczna translacja (nowe nazwy 1:1 ze starymi klasami = STOP).

## 9. Stan bieżący (2026-07-15)

- **Zrobione:** audyt obu dem; analiza wizji v0.1 (K1–K11) + feedback
  Jozza; decyzje OD-01…OD-15; **X0 zaliczony** (`spike/kernel_v0`);
  twarde fakty silnika (JES_02 §7); pakiet Sol przeczytany w całości
  i scalony (ten pakiet 1.0); Capability Ledger Sol-a (29 kart CAP-*)
  wskazany jako rejestr dziedzictwa (JES_04 §6).
- **Bazy tymczasowe:** to repo (⚠ JES_00 §5) + repo VAW (pakiet Sol,
  również untracked draft na dirty checkoucie).
- **Świeży stan dowodowy VAW** (z pakietu Sol, status SESSION_OBSERVED):
  smoke/testy/mission-validate PASS na dirty checkoucie, ale release-build
  provenance NIEPOTWIERDZONE (`SOURCE_MANIFEST.json` ≠ worktree) — do
  odtworzenia na zamrożonym baseline w P-1.

## 10. Dziennik decyzji

| ID | Decyzja | Stan |
|---|---|---|
| D1/OD-04 | własny stack agent-friendly (nie Godot/Unity/UE) | **PRZYJĘTE** |
| D2 | nazwa projektu, miejsce/hosting nowego repo | **OTWARTE — blokuje start repo** |
| D3 | ECS powłoki (flecs/EnTT/własna scena) | OTWARTE — spike |
| D4/OD-03 | dema zamrożone, zero kopiowania kodu | **PRZYJĘTE** |
| D5 | pin świeżego box3d upstream + polityka podbić | OTWARTE (rekomendacja: pin; box3d = backend za adapterem, §7.8) |
| D6 | model konstrukcji: grid/free-form/hybryda | OTWARTE — lab przy P2/P3 |
| D7 | zakres pierwszych canary (który asset Jozza pierwszy; co jeździ w M) | OTWARTE (Jozz, na starcie P1) |
| D8 | game-UI na ImGui na start | OTWARTE (rekomendacja: TAK) |
| D9/OD-05 | UI wzorem Blender+UE | **PRZYJĘTE** |
| D10 | `BOX3D_DOUBLE_PRECISION` od F0 | OTWARTE (benchmark w P1; rekomendacja ON — zmiana później = ABI+zapis+replay naraz) |
| D11 | pierwszy produkt („Dolina Prób") | zdjęte z agendy (OD-07); wraca przy składaniu gry |
| D12 | system gracza — osobny dokument | OTWARTE (Jozz, „na żywo") |
| D13 | poziom determinizmu/replay (tier) | OTWARTE PDR (zasada 12 = default tymczasowy) |
| D14 | lista assetów właściciela do przeniesienia + provenance | **OTWARTE — wymagane w P-1** |
| D15 | polityka nieznanych pól w zapisie (reject-with-diagnostic / extension bag / jawny discard) | OTWARTE (z CAP-VAW-01; decyzja przy P2) |
| D16 | ratyfikacja konstytucji JES_01 (16 zasad) przez Jozza | **OTWARTE — warunek startu repo** |
| D17 | los `spike/kernel_v0` (snapshot referencyjny w heritage / zostaje w JV / kod startowy labu) | OTWARTE (JES_06 §10; rekomendacja: snapshot) |
| D18 | pakiet Sol: snapshot w nowym repo vs wskaźnik na repo VAW | OTWARTE (JES_06 §10; rekomendacja: snapshot) |
| D19 | licencja nowego repo | OTWARTE (Jozz; wpływa na hosting i VENDOR.md) |
| D20 | polityka vendoringu: kopie w `vendor/` vs FetchContent+SHA | OTWARTE (JES_06 §10/SIM-04; rekomendacja: kopie, build bez sieci) |

## 11. Minimalny kodeks pracy (tymczasowy — do czasu projektu workflow, OD-12)

1. Jeden eksperyment/lab naraz na strumień; nigdy trzy warianty w aktywnej
   scenie.
2. Kryteria PRZED implementacją; porażka → STOP i raport, nigdy ciche
   poluzowanie progu.
3. Odbiór feel/wizji/estetyki należy do Jozza; `WAITING_FOR_JOZZ` kończy
   turę.
4. Małe commity po zielonej bramce; zakaz pracy tygodniami poza historią;
   w starych repo zakazy z JES_00 §5.
5. Bramka sprawdza exit code, istnienie i timestampy artefaktów; praca
   wizualna zawsze z obejrzanym zrzutem; wektor werdyktów, nie jedno PASS.
6. Eksperymenty tylko w `spike/` (tu) / `labs/` (nowe repo); produkt nie
   importuje kodu labów.
7. Agent twierdzący „to zasada / to działa / to decyzja" musi wskazać
   status i źródło (JES_05 §7).

## 12. Gotowość do nowego repo (odchudzona checklista) i następny krok

Repo można utworzyć, gdy:

- [ ] Jozz ratyfikował konstytucję (D16) i rejestr dyrektyw (JES_01);
- [ ] D2 rozstrzygnięte (nazwa, miejsce);
- [ ] P-1: baseline'y VAW/JV zamrożone (hash + manifest), lista assetów
      właściciela wskazana (D14);
- [ ] pierwsze Lab Chartery istnieją: P0 + Canary A/M/T (z kryteriami);
- [ ] wiadomo, które decyzje są provisional (dziennik §10);
- [ ] brak nierozstrzygniętego konfliktu blokującego minimalny szkielet
      repo i izolację canary (disposable canary nie zamrażają żadnego
      trwałego formatu).

NIE trzeba przed startem: rozwiązanego dynamicznego gruntu, finalnego
renderera, lotu, automatyzacji, wielkiego świata, finalnego formatu
maszyny/świata/assetów, produktu casualowego.

**Następny krok (dyrektywa Jozza):** plan budowy czystego projektu
i przenosin — struktura nowego repo, migracja tego pakietu (JES_00–05 +
źródła + manifest przenosin), vendoring, CI, bootstrap P0. **Dokument
wykonawczy istnieje: `JES_06_SYMULACJA_STARTU_I_PLAN_FUNDAMENTU_PL.md`**
(symulacja SIM-01…20 → plan F0–F5 → manifest migracji → krytyka Sol-a
→ decyzje F0 Jozza → utworzenie repo).
