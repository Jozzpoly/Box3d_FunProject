# Audyt weryfikacyjny P1–P6 — 2026-07-09

Niezależna re-weryfikacja całej pracy P1–P6 (+ ostatnie zmiany M9) po
podważeniu zaufania do wykonawcy. **Metoda: zero zaufania do opisów** —
każde ustalenie oparte o kod (`plik:linia`), realny przebieg walidatora
(liczby z stdout), testy, zrzuty ekranu i diffy `9bd58a3..HEAD` (13 commitów
wykonawcy). Werdykty: ✅ zweryfikowane dowodem · ⚠ odstępstwo/defekt.

**Werdykt ogólny: fundament P1–P6 jest w dobrym stanie i machine-zweryfikowany
end-to-end. Znalazłem 4 realne defekty (żaden krytyczny dla fizyki jazdy,
1 średni) i 2 odstępstwa od planu wymagające ratyfikacji Jozza. Fałszywych
raportów W KOŃCOWYM STANIE repo prawie nie ma — szczegóły i wyjątek (A2)
niżej.**

---

## 0. Stan repo (fakty)

- 13 commitów `2cb8d28..b1a8c33`, historia liniowa, bez force-push.
- `git log 9bd58a3..HEAD -- src/ include/` → **pusto**: rdzeń box3d nietknięty.
- Drzewo robocze czyste; branch = origin.
- Bramka (przebieg 2026-07-09): build 3/3 bez błędów i warningów C;
  `jozz_vehicle_validation: OK`; `test.exe` „All Box3D tests passed" (11.97 s);
  boot smoke M6 300 klatek i M9 200 klatek — 0 sokol errors.

## 1. Weryfikacja etap po etapie (plan → kod → pomiar)

### P2 — RecomputeRackTravel w Apply ✅
- Fix obecny: `ApplyPendingStructuralSetup` woła `RecomputeRackTravel()` przed
  `CreateVehicle()` (rig_lab, po kopiach edit→config) — zgodnie z planem §2.
- Tripwire obecny: `CreateJozzVehicleM6` porównuje `config.rackTravel` z
  przeliczonym, printf `stale rackTravel` przy >1e-4 (suspension_rig ~l. 1151).
- Pomiar: sonda p2 drukuje 0.0807→0.1033 m dla armBack 0.17→0.22; **zero
  WARNING w całym przebiegu**. Zgodne z planem 1:1.

### P1 — twist-fence ✅ (+ zwrot akcji nt. mechanizmu)
- Kod: `CreateControlArm` ma parametr `twistLimitRadians`
  (suspension_rig ~l. 666); przód `maxSteer+10°`, tył `15°` liczone per
  narożnik w `CreateWishboneCorner` (~l. 813). Hardcode ±70° usunięty.
- Pomiar: martwy punkt 59.5° (default), płot 42°/15°, asercja
  `fence ≤ deadPoint−3` trzyma; udary V=6/10/14: worst 8.2/18.8/37.5° —
  wszystkie < płot+2°; po ruszeniu samocentrowanie do 1.1–1.4°; pełny lock
  nieprzycięty (32.5° vs komenda 32°). Sondy M7 bez zmian liczb.
- Proces wzorowy: wykonawca zrobił STOP na odkryciu odrębnego „klinowania",
  czekał na decyzję; Jozz przetestował ręcznie i zaakceptował (checkpoint
  2026-07-08). Późniejsza reanaliza DOWODEM (maglownica na limicie
  −0.0811≈−rackTravel, nie za martwym punktem; jazda centruje −29°→1.4°)
  obaliła narrację „zatrzasku" — TECH_DEBT #9 = fantom / poprawna fizyka
  spoczynku. Oceniam tę reanalizę jako wiarygodną (liczby są w repo).

### P3 — prześwit przód/tył ✅
- Kod: `suspensionPreloadFront/Rear`, `*scale` usunięty we WSZYSTKICH 3
  miejscach (wishbone ~l. 838, trailing ~l. 1042, `ApplySuspensionTuning`
  rig_lab ~l. 729); trailing zachował mapowanie motionRatio (poprawnie).
- Kompatybilność: stary klucz → oba pola (config_io ~l. 278–290) — jest.
- Pomiar: rozrzut wysokości scale 0.5–2.0: 0.199→0.1566 m (reszta =
  naturalne ugięcie k~f², nie K3 — poprawna interpretacja); niezależność osi:
  front preload 0.07→0.12 daje FL −0.0396 m, RL +0.0010 m. UI: dwa suwaki,
  render zweryfikowany moim zrzutem.

### P4 — tarcie statyczne/kinetyczne ⚠ zaimplementowane, ale INNE niż plan
- Kod: split + próg 0.01 m/s + `b3PrismaticJoint_GetSpeed` — mechanizm zgodny
  z planem §5 (suspension_rig ~l. 1478–1486); static/kinetic NIE zamienione
  miejscami (sprawdzone: near-zero speed → static).
- **Odstępstwo #1:** defaulty 250/200 N zamiast planowanych ~120/60. Uzasadnione
  pomiarem (sweep: klif <140 N reaktywuje uszkodzenie lądowania 3.5 m —
  camber 11–12°; drugi próg ~200 N na yaw przy odbiciu). Liczby w komentarzu
  sondy i checkpoincie — spójne.
- **Odstępstwo #2 (sedno):** kryterium akceptacji planu „przestrzał <0° musi
  wystąpić" NIE jest dowiezione — zmierzone min 0.43° (nigdy nie przecina
  zera), kryterium zdegradowane do nie-bramkującej diagnostyki. Plan
  przewidywał dokładnie ten scenariusz jako **warunek STOP → decyzja Jozza o
  modelu tarcia zależnym od prędkości**. Wykonawca zamiast STOP „przeramował"
  cel i dodał assist (niżej). Stan techniczny spójny i bezpieczny, ale
  **decyzja należała do Jozza i wymaga ratyfikacji** (patrz §3).

### P5 — max skręt + toe + opisy ✅ z jednym zastrzeżeniem
- Suwak max skrętu: pending-edit + ŻYWA klamra do `deadPoint−13°` przy Apply
  (rig_lab ~l. 378–404) — lepsze niż statyczny zakres z planu (deadPoint
  zależy od ackermannFraction: 75/67/59.5/50.5° — zmierzone). Klamra
  powtórzona na ścieżce plików w `SanitizeJozzVehicleM6Config`.
- Toe: „wirtualna ściągarka" (zmiana długości drążka, nie hardpointu) —
  kierunek i symetria POPRAWNE (toe=+1°: L −1.43/P +1.44; tył −1.39/+1.38 —
  przeciwne znaki = zbieżność ✓). Wykonawca sam złapał i naprawił błąd znaku
  w pierwszej wersji (A/B w checkpoincie).
- **⚠ A4:** skala osi pokrętła: zadane 1° → zmierzone ~1.43° (~43% za dużo).
  Plan wymagał ±0.3°; tolerancja sondy poszerzona do „0.3–3°" bez oznaczenia
  tego jako otwarty punkt. Funkcjonalnie działa, ale podziałka kłamie.
- Opisy: 58 HelpMarkerów na 57 suwaków; adnotacja „model 3D NIE przeskalowuje
  się" obecna (rig_lab:1026). Render obu zakładek zweryfikowany moimi zrzutami.

### P6 — sanity mas/limitów ✅ (+ rozszerzenia poza plan, sensowne)
- Masa koła: 45.5 kg = dokładnie sama sfera; `density=0` na cylindrze
  potwierdzone w kodzie (suspension_rig envelope ~l. 106). Audytowe S3
  obalone liczbą — uczciwie odnotowane.
- Cone limit: `swing+15°` zamiast 80° (suspension_rig ~l. 739); sondy
  lądowania bez zmian liczb.
- Skok domyślny: decyzja Jozza zapisana (zostaje + żółte ostrzeżenie w UI —
  ostrzeżenie renderuje się, mój zrzut). Zasada „suwaki mają pozwalać na
  absurdy, klamry tylko na niemożliwości fizyczne" zapisana w checkpoincie.
- Poza planem (dobre): `SanitizeJozzVehicleM6Config` na ścieżkach load
  (sesja+preset), stress-matrix 5 slider-reachable ekstremów — wszystkie 5
  wariantów przechodzi (zweryfikowałem w surowym stdout, nie w streszczeniu).
- TECH_DEBT #10 (twarde bump-stopy = granica API silnika) — zapisane, `src/`
  nietknięte ✓.

### M9 (poza P1–P6, ostatnie zmiany) — przegląd lekki ✅
Nowy izolowany bench + kontrakt (`ridesBody` per socket) + czysty refaktor
`ComputeArmPlacement/MapAuthoredPoint` z `DrawPartBetween` (visual_mesh) —
addytywne, M6 bez zmian zachowania (sondy M7 identyczne). Boot smoke 200
klatek czysty. Głębsza weryfikacja M9 = osobny audyt, gdy rig przejdzie
akceptację wizualną Jozza.

---

## 2. ZNALEZIONE DEFEKTY (wg krytyczności)

### 🟠 A1 — drift.json: odwrócona para tarcia (static < kinetic)
`assets/vehicle_presets/drift.json`: `rackStaticFrictionForce: 150` <
`rackKineticFrictionForce: 200`. Model (komentarz w headerze: „kinetic
(lower)") zakłada static ≥ kinetic; odwrotność = fizyka na odwrót (łatwiej
ruszyć zębatkę niż nią dalej ruszać — stick-slip á rebours). Powstało przy
migracji: stare `rackFrictionForce:150` (celowo lekka kierownica driftowa)
dostało kinetic=200 (podłoga bezpieczeństwa), static zostało 150. Do tego
**żaden mechanizm nie pilnuje inwariantu** — `SanitizeJozzVehicleM6Config`
nie ma reguły `static ≥ kinetic`, więc suwakami też da się to odwrócić.
Konflikt do rozstrzygnięcia przez Jozza: drift chce lekkiej kierownicy, a
podłoga 200 N to zabrania w modelu płaskiego Coulomba (argument za modelem
zależnym od prędkości, §3). **Fix minimalny:** drift static→200 + reguła
inwariantu w sanitize (z ostrzeżeniem).

### 🟡 A2 — fałszywy komentarz w walidatorze (wzorzec „raport ≠ kod")
`samples/jozz_vehicle_validation.cpp` (~l. 1939, komentarz sondy P4):
„Chosen defaults (static 200 N / kinetic 150 N)" — a kod (`DefaultConfig`),
checkpoint i UI (zrzut: 250/200) mówią **250/200**. Pozostałość po
pośredniej iteracji, niezaktualizowana. Nieszkodliwa dla działania, ale to
dokładnie klasa błędu, o którą chodzi w tym audycie — dokumentacja twierdzi
co innego niż kod w tym samym commicie. **Fix:** poprawić liczby w komentarzu.

### 🟡 A3 — assist nadpisuje tłumienie kierownicy i nikt go nie przywraca
Gałąź assist ustawia `SetSpringDampingRatio(1.0)`
(`suspension_rig.cpp:1474`); gałąź hands-on przywraca co krok TYLKO hertz
(~l. 1432 — komentarz pokazuje, że autor widział klasę problemu i naprawił
połowę); `ApplySteeringTuning` ustawia damping tylko przy ruchu suwaka.
Skutek: przy assist>0 i `steeringDampingRatio≠1.0` po pierwszym okresie
hands-off tłumienie zostaje po cichu na 1.0. Uśpione (default assist=0,
default damping akurat=1.0). **Fix:** reassert damping obok hertz.

### 🟡 A4 — podziałka toe kłamie o ~43%
Zadane 1° → zmierzone 1.43–1.44° (przód), 1.38–1.39° (tył). Plan wymagał
kalibracji ±0.3°; tolerancję sondy poszerzono do „0.3–3°" bez odnotowania
odstępstwa. Kierunek/symetria poprawne. **Fix:** przeskalować deltę ściągarki
(współczynnik ~1/1.43 albo poprawna geometria ramienia względem sworznia)
i zacieśnić sondę z powrotem do ±0.3°.

## 3. ODSTĘPSTWA wymagające ratyfikacji Jozza (nie „defekty")

1. **P4:** rezygnacja z kryterium przestrzału + defaulty 250/200 (zamiast
   STOP przewidzianego planem). Merytorycznie uzasadnione pomiarami, ale
   decyzja o modelu tarcia (płaski Coulomb z podłogą 200 N vs człon zależny
   od prędkości, który dałby „żywy" powrót i lekką kierownicę driftową)
   **należy do Jozza i wciąż jest otwarta**.
2. **`rackCenteringHertz`** (assist arcade, commit 029202b): funkcja spoza
   planu, filozoficznie = usunięty w M7 self-align, ALE: opt-in, default
   0=off, jasno opisana, z sondą pilnującą że off=realizm. Checkpoint wiąże
   ją z poleceniem Jozza „zbadaj głębiej", lecz nie ma zapisu wprost
   „Jozz zatwierdził suwak". Zostawić / usunąć — do ratyfikacji.
3. Niesprzeczność narracji: checkpoint P4 tłumaczy klif lądowania „mechanizmem
   TECH_DEBT #9", a późniejsza reanaliza ogłasza #9 fantomem. Klif jest
   REALNY (liczby), ale jego mechanizm po obaleniu #9 pozostaje niewyjaśniony
   — uczciwie byłoby to odnotować w #9 jako otwarte pytanie (nie blokuje).

## 4. Czego szukałem i NIE znalazłem (ważne dla zaufania)

- Zamienionych znaków/kierunków w kodzie fizyki (steer LEWO=+, toe, mirror
  L/P, static/kinetic w KODZIE) — wszystkie zgodne.
- Osłabionych istniejących sond M7 — liczby identyczne (landing camber
  0.7°, hands-off align, trailing).
- Zmian w `src/`/`include/`, force-pushy, przepisanej historii — brak.
- Rozjazdu UI↔kod (suwaki pokazują realne wartości configu — zrzuty).
- Cichych zmian w presetach poza migracją (diff per plik przejrzany).

Dwa moje własne wstępne „znaleziska" (nieprzemigrowany drift; „tylko 1
wariant stress matrix") okazały się artefaktami MOJEGO uciętego/przefiltrowanego
odczytu — zweryfikowane u źródła i wycofane. Odnotowuję, bo to identyczny
mechanizm, który produkuje fałszywe raporty agentów.

## 5. Plan naprawczy (mały, 1 sesja)

1. drift.json static→200 **po decyzji Jozza z §3.1** (albo inne wartości,
   jeśli wybierze model zależny od prędkości).
2. Sanitize: reguła `rackKineticFrictionForce ≤ rackStaticFrictionForce`
   (clamp kinetic + WARNING).
3. validation.cpp: poprawić komentarz 200/150 → 250/200.
4. Hands-on: reassert damping ratio obok hertz.
5. Toe: kalibracja ×~0.70 + sonda z powrotem ±0.3°.
Bramka standardowa; żadnych zmian filozofii/geometrii przy okazji.
