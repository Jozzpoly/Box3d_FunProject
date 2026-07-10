# Checkpoints / handoff — Jozz Vehicle Box3D Native

Dziennik decyzji technicznych i handoffów. **To jest domyślny sposób
dokumentowania zmian** — zamiast pełnego `docs/*.md` na każdy drobiazg.

**Format wpisu (≤5 linii, najnowsze u góry):**
```
## YYYY-MM-DD · Tytuł · <commit lub „docs">
- CO:     jedno zdanie, co zmieniono
- CZEMU:  jedno zdanie, po co
- EFEKT:  weryfikowalny rezultat (walidator/render/liczby)
- DALEJ:  następny krok albo „—"
```

Zasady: pełne raporty (analizy, plany) zostają w dedykowanych `docs/*.md`; tutaj
tylko skrót + link. Gdy przekroczy ~30 wpisów — najstarsze usuń (są w gicie).

---

## 2026-07-09 · KRYTYCZNY fix: preset = deterministyczny powrót do fabryki+nadpisań · dc3a4a3
- CO:     nowa semantyka wczytania presetu (`LoadJozzVehicleM6PresetConfig`): wynik = `m_factoryConfig` (jedno źródło prawdy, komponowane w konstruktorze) + klucze pliku — NIEZALEŻNIE od stanu suwaków przed wczytaniem. Przycisk „Przywróć domyślne" przywraca tę samą bazę. Sonda `RunPresetDeterminismProbe` pilnuje kontraktu na stałe. Reguła + tabela semantyk load w `SUBSYSTEM_UI_PRESETS_PL.md` §2a. TECH_DEBT #11 (resztkowe delikatne ściąganie — zaakceptowane) i #12 (pola poza configiem nie przeżywają R: solver kontaktu, invert).
- CZEMU:  KRYTYCZNY bug złapany przez Jozza w grze: kręcił suwakami „na brudno", wczytał preset licząc na pełny powrót — presety są CZĘŚCIOWE a load był IN-PLACE, więc wszystko czego preset nie wymieniał ZOSTAŁO (i auto-sesja utrwaliła to na dysku). Z fotela gracza „preset zapisał zmiany bez pozwolenia". Pliki presetów na dysku nietknięte (git czysty) — wadliwa była semantyka, nie zapis. Dotyczyło WSZYSTKICH ~50 suwaków, nie tylko tarcia.
- EFEKT:  sonda: sabotaż rackFrictionBase=999/brakeTorque=123/toe=3° → po wczytaniu offroad wracają do fabrycznych 40/650/0°, a suspensionHertz bierze 3.5 z presetu — deterministycznie. Przegląd klasy problemu: sesja (plik KOMPLETNY na świeże defaulty) — poprawna; SaveCurrentAsPreset pisze pełny config — poprawny; envelope mode/layers są w io — OK; JEDYNE pozostałe luki to pola poza configiem (#12, mniejsza klasa: nie persystują, ale niczego nie zapisują po cichu). Bramka: build 3/3, walidator OK zero bad, test.exe PASS 12.15 s, smoke 300 kl.
- DALEJ:  **STOP utrzymany do potwierdzenia Jozza, że system presetów zachowuje się teraz jak oczekiwał** (test: pokręć suwakami dowolnie → wczytaj preset → wszystko wraca). Potem dopiero porządki §6 + edytor rigu §7.

## 2026-07-09 · BRAMKA 2: tarcie maglownicy zależne od obciążenia (P4b) · 8df320b
- CO:     rework hands-off tarcia racka: `cap = stiction(1.4 przy postoju) × (rackFrictionBase + rackFrictionLoadCoeff × poprzeczne obciążenie drążków)` zamiast płaskiej pary static/kinetic. Defaulty 40 N / 0.10. Config_io: klucze legacy IGNOROWANE z notką (inny model, brak uczciwej konwersji); presety uliczny (40/0.10) i drift (25/0.08 — lżejsza kierownica driftowa) zmigrowane ręcznie → **A1 rozwiązane u źródła** (odwrócona para przestaje istnieć). Sanitize: base [0,1000], coeff [0,1]. UI: 2 nowe suwaki z opisami wg szablonu. Sonda ściągania dostała bramkę (heading <6°, |z| <5 m po 10 s).
- CZEMU:  decyzja Jozza (brief §3.1, odbiór Bramki 1): model realistyczny zamiast płaskiego Coulomba z podłogą 200 N. Jeden fizyczny mechanizm godzi lądowanie i jazdę na wprost: drążki przy lądowaniu niosą kN → tarcie samo skacze; na wprost prawie znika → rack nie parkuje off-center.
- EFEKT:  LICZBY: lądowanie 3.5 m camber 0.6°/rear 0.1° — IDENTYCZNE z podłogą 200 N (twarda poprzeczka utrzymana); **ściąganie w lewo NAPRAWIONE**: heading @10 s z +14° → wędrówka ±2.7° wokół zera, rack oscyluje wokół 0 (±0.0003 m) zamiast parkować +1 mm; parking hold 0.00°; kopnięte koło na postoju zostaje −28.9° (stiction, realizm zachowany); kontra w ślizgu bez zmian (rack FREE −11.8° vs target −10.1, rms 0.18); powrót po locku: final 0.25° (było 0.62), amp 0.31 bez shimmy, przestrzał nadal nie występuje (uczciwa własność linkage'u — siła casteru gaśnie w centrum; diagnostyka pozostaje). Bramka: build 3/3, walidator OK, test.exe PASS 12.0 s, smoke 300 kl. (stara sesja z legacy kluczami wczytana poprawnie z notką), zrzut UI.
- DALEJ:  **STOP — czekam na odbiór Bramki 2 przez Jozza** (najlepiej test ręczny: jazda na wprost, lądowania z ramp, feel powrotu kierownicy, preset driftowy). Potem: porządki/workflow (brief §6) + propozycja edytora rigu (§7).

## 2026-07-09 · BRAMKA 1: diagnoza ściągania w lewo + A2/A3/A4 + ADR-0006 · 371e99b
- CO:     (1) sonda `RunStraightPullDiagnosisProbe` + 2 warianty — ściąganie ZDIAGNOZOWANE, naprawa → Bramka 2; (2) A2: komentarz 200/150→250/200; (3) A3: hands-on reassert damping obok hertz; (4) A4: toe przez DOKŁADNĄ rotację wokół osi sworznia (`SteeringArmWithToe`) + sonda mierzy DELTĘ od bazy toe=0 z tolerancją ±0.3°; (5) ADR-0006 (realistyczny rdzeń, nakładki [ARCADE] default-off) + etykiety `[ARCADE]` na 2 kontrolkach.
- CZEMU:  brief Jozza 2026-07-09 §5/Bramka 1; zasada rdzenia wcześniej nigdzie nie była zapisana (stąd spór o rackCenteringHertz).
- EFEKT:  DIAGNOZA ściągania (liczby): spoczynek SYMETRYCZNY (FL/FR ∓0.4°, rack +0.00007 m) → to NIE offset wyrównania; kopnięcie powstaje pod momentem napędu (t=2s FL +1.45/FR −1.04; AWD +14°/10s vs RWD +3.4° — reakcja momentu na przednich zwrotnicach + mały bias solvera), a rack zaparkowany +1 mm w lewo TRZYMA tarcie statyczne 250 N (caster przy ~0.2° poślizgu nie przebija progu) → splecione z modelem tarcia → **Bramka 2** (wg §4 briefu). Reverse hands-off = flop do limitu (−60°, rack −0.0757) — poprawna fizyka casteru na wstecznym, nie bug. TOE: delta przód −0.97/+0.98, tył −0.89/+0.88 (było efektywnie ±1.43) — dial skalibrowany, kluczem było odjęcie bazowego rozstawu osiadania (∓0.4°), który wcześniej zawyżał pomiar absolutny. Bramka: build 3/3, walidator OK (regresje P1/M7 identyczne: impact −29→1.4, full lock 32.5°, rack rms 0.17), test.exe PASS 11.58 s, smoke 300 kl. 0 err, zrzut UI z [ARCADE].
- DALEJ:  **STOP — czekam na odbiór Jozza.** Potem Bramka 2: model tarcia zależny od sił/prędkości (tam też: domknięcie ściągania, drift.json, inwariant sanitize).

## 2026-07-09 · Audyt weryfikacyjny P1-P6 (zero zaufania do opisów) · docs
- CO:     niezależna re-weryfikacja 13 commitów P1-P6+M9 na kodzie/testach/zrzutach → `AUDIT_WERYFIKACJA_P1_P6_2026_07_09_PL.md`. Zero zmian w kodzie (audyt czysty).
- CZEMU:  Jozz podważył zaufanie do wykonawcy (odwrócone implementacje, fałszywe raporty w trakcie sesji).
- EFEKT:  fundament POTWIERDZONY liczbami (bramka zielona end-to-end, rdzeń nietknięty, sondy M7 bez regresji). 4 defekty: 🟠 drift.json static150<kinetic200 (odwrócona para, brak inwariantu w sanitize) · 🟡 fałszywy komentarz 200/150 w validation.cpp vs kod 250/200 · 🟡 assist nadpisuje damping (hands-on przywraca tylko hertz, rig:1474/1432) · 🟡 podziałka toe kłamie ~43% (1°→1.43°, tolerancja sondy po cichu poszerzona). 2 ratyfikacje Jozza: P4 bez przestrzału (STOP ominięty, defaulty 250/200) + suwak rackCenteringHertz spoza planu.
- DALEJ:  plan naprawczy §5 audytu (1 sesja, po decyzji Jozza nt. modelu tarcia); post-mortem procesu w rozmowie z Jozzem.

## 2026-07-09 · M9: nowy model OneSided_Steering_Suspension_Rig + izolowany bench · 9eaab34
- CO:     Jozz przysłał nowy model skrętnego zawieszenia (inna organizacja niż One_Sided_wheel_mount: Socket_ChassisMount_b to upright/knuckle nie chassis, damper dolny na dolnym wahaczu, nowy Socket_SteeringRod). Nowy kontrakt `one_sided_steering_suspension.asset.json` (jawne `ridesBody` per sokiet) + izolowany bench M9 (2 narożniki L/R, chassis→carrier(skok)→knuckle(skręt), bez auta). Wydzielono `JozzVehicleComputeArmPlacement`/`JozzVehicleMapAuthoredPoint` z `DrawPartBetween` (czysty refaktor) żeby damper dolny mógł jechać z żywej pozycji wahacza bez osobnego ciała fizyki.
- CZEMU:  Jozz zażądał krytycznej analizy PRZED kodem (nie kopiować 1:1 starego rigu) + walidacji na izolowanym benchu przed wpięciem w M6. Drążek kierowniczy celowo NIE dostał nowej fizyki — pinowany do stałego punktu (rozciąga się jak wahacze), prawdziwa integracja ma czytać istniejący rack/steerLinkJoint.
- EFEKT:  Walidator: nowe liczby kontraktu zgodne z analizą (travelAxis 0.700 m, damperSpan 0.689 m, wheelCenter-chassisMountB 0.217 m) — OK. `JOZZ_M9_DUMP` przy skręcie 30°: chassisMountB przesuwa się lustrzanie (+0.0786/-0.0786), drążek kompresuje się z jednej strony (0.781×) i rozciąga z drugiej (1.236×) — potwierdza poprawne wiązanie do knuckle i brak błędu gałęzi lustrzanej. Zrzuty: spoczynek/skręt/droop/bump bez klipowania. Regresja M6+M8 sprawdzona zrzutem — bez zmian. Build 3/3 OK, test.exe 11/11 PASS, boot-smoke 0 sokol errors. Zapushowane na `jozz-vehicle-sandbox-m0`.
- DALEJ:  Jozz waliduje wizualnie nowy rig na benchu; jeśli zaakceptuje — integracja z przednimi narożnikami M6 (czytanie realnego racka dla drążka) i decyzja o migracji starego rigu.

## 2026-07-09 · Decyzja Jozza: default skoku zostaje + filozofia limitów na przyszłość · docs
- CO:     zamknięta otwarta decyzja z P6 (saturacja S2 skoku przy domyślnym wahaczu) — zostaje jak jest, tylko żółte ostrzeżenie w UI (już wdrożone).
- CZEMU:  Jozz: niedługo dojdą nowe modele zawieszenia ze ZNACZNIE dłuższymi wahaczami — dłuższe ramię przy tym samym skoku w cm potrzebuje mniejszego kąta, więc saturacja przy nowych modelach zniknie sama. Zmiana defaultu pod dzisiejszy (krótki) wahacz byłaby krótkowzroczna.
- EFEKT:  **Ważna zasada projektowa na przyszłość (Jozz, wprost):** suwaki MAJĄ pozwalać na absurdalne wartości — to celowe, przydatne do testów i „zabawy", nie błąd do naprawienia. System ma obsłużyć wszelaką maść typów pojazdów, nie tylko dzisiejszy szablon auta osobowego. To DOPRECYZOWUJE (nie zaprzecza) starszą zasadę „ciasne zakresy suwaków" z `jozz-vehicle-ui-ux-preferences` — ciasny zakres to wygoda przeciągania suwakiem dla typowego auta (Ctrl+klik nadal wpisuje dowolną liczbę), a NIE twardy sufit fizyki. Twarde klamry bezpieczeństwa z P1/P5/P6 (płot martwego punktu drążka, sanitize) chronią przed złamaniem SOLVERA (NaN, zdegenerowana geometria) — to zostaje, bo to nie jest „sensowny zakres", tylko fizyczna niemożliwość. Watch-item: sufity w `SanitizeJozzVehicleM6Config` (np. długość wahacza 2.0 m) dobrane pod dzisiejszą skalę auta — sprawdzić/rozszerzyć przy wdrażaniu nowych, znacznie dłuższych modeli, żeby sanitize nie przycinał poprawnej nowej geometrii.
- DALEJ:  gdy przyjdą nowe modele zawieszenia — najpierw sprawdzić czy ich wymiary mieszczą się w sufitach sanitize (P6), rozszerzyć jeśli nie.

## 2026-07-09 · P6: sanity mas/limitów + stress matrix + sanitize ścieżek load · (do commitu)
- CO:     dwie permanentne sondy (`RunP6MassAndLimitSanityProbe` — masy/stosunki/saturacja S2 + regresja sanitize; `RunP6StressMatrixProbe` — 5 slider-reachable ekstremów: 2000 N·m AWD grip 2.5, lekkie elementy nieamortyzowane, odwrócony stosunek mas kola>nadwozie, max preload+travel, najsztywniejszy setup z drop 2 m); cone limit z geometrii (`swing+15°` zamiast hardcode 80°, audyt S5); ostrzeżenie saturacji skoku w UI (S2); NOWE `SanitizeJozzVehicleM6Config` na ścieżkach load sesji/presetu (NaN/degeneracje/floors + **domknięcie dziury P5**: klamra martwego punktu maxSteer działała tylko w UI Apply — ręcznie edytowany preset ją omijał); TECH_DEBT #10 (brak API miękkich bump-stopów — granica silnika).
- CZEMU:  task #45 + rozszerzony brief Jozza (testy ekstremalne, odporność na błędne dane); plan pisany przed P1 wymagał konfrontacji z aktualnym stanem.
- EFEKT:  **S3 audytu OBALONE liczbą**: masa koła 45.5 kg = dokładnie sama sfera (guard `density=0` na cylindrze istniał od początku; sonda pina to na stałe). Stosunki mas zdrowe (chassis/koło 10.5). **Stress matrix: wszystkie 5 wariantów PASS** (finite, brak teleportacji, camber<15°, rear toe<8°, jitter przy postoju 0.000-0.221 m/s) — fundament M7+P1-P5 wytrzymuje slider-reachable ekstremy bez poprawek. Sanitize: zepsuty preset (arm=0, density=-50, NaN travel, maxSteer 45+ackermann 1.0) → przycięty, buduje się i osiada finite; default przechodzi NIETKNIĘTY. Cone z geometrii: sondy lądowania IDENTYCZNE (camber 0.7°/0.7°). Walidator OK, test.exe PASS, boot-smoke 0, zrzut ostrzeżenia S2 (zawijany, domyślny config saturuje 120% — zmiana defaultu skoku to decyzja Jozza, odnotowana).
- DALEJ:  plan P1-P6 DOMKNIĘTY. Otwarte decyzje Jozza: (a) zmniejszyć domyślny skok 0.42/0.28→0.27/0.18 czy zostawić z ostrzeżeniem; (b) roadmapa §8 README (P7 opona / P9 drivetrain / boczne dampery).

## 2026-07-08 · P5: suwak max-skrętu (z żywym klamrowaniem) + statyczne toe + audyt opisów · (do commitu)
- CO:     suwak „Maksymalny skręt kół" (strukturalny, pending-edit, 20-45°) z ŻYWYM klamrowaniem do bezpiecznej wartości przy Apply (nowa wspólna funkcja `ComputeJozzVehicleM6SteeringDeadPointDeg` używana przez P1/sondę/klamrę); statyczne toe przód/tył (`frontToeDeg/rearToeDeg`, mechanizm „wirtualnej ściągarki" - zmiana długości drążka/toe-linku, NIE punktu mocowania); pełny audyt `HelpMarker` (Napęd, Zawieszenie-zaawansowane, wahacz wleczony, kształt kolizji); update `SUBSYSTEM_UI_PRESETS_PL.md`.
- CZEMU:  plan P5 (task #44) rozbudowany o wiedzę z tej sesji — audyt zakładał statyczny zakres 20-45°, ale zmierzyłem że `ackermannFraction=1.0` (inny suwak) obniża bezpieczny sufit do 37.5°, więc statyczny zakres byłby albo niebezpieczny, albo niepotrzebnie ciasny.
- EFEKT:  **Znalezisko #1:** interakcja dwóch suwaków potwierdzona liczbami (deadPoint 75°/67°/59.5°/50.5° dla ackermannFraction 0/0.3/0.6/1.0) — rozwiązana klamrą żywą, nie statycznym zakresem (wzorzec identyczny jak droop 16°). **Znalezisko #2 (bug w pierwszej wersji toe):** naiwna implementacja ze znakiem `IsLeftCorner`-flip dawała SKRĘT (oba koła ten sam znak, ~+2.1°/+2.1° dla toe=+2°) zamiast ZBIEŻNOŚCI (przeciwne znaki) — geometria już ma wbudowane lustrzane odbicie, dodatkowy flip je kasował. Naprawione (bez `in`, ta sama delta obu stron), zweryfikowane A/B (+2°→-2.09/+2.11°, -2°→+2.20/-2.18°, czysto liniowe i symetryczne). maxSteer=40° → pełny lock 40.5°; toe=+1° przód → -1.43/+1.44°, tył → -1.39/+1.38°. Walidator OK (P1-P5 + M7 bez regresji), test.exe PASS, boot-smoke 0, zrzuty UI (Kierownica z nowym suwakiem, Zawieszenie-zaawansowane z opisami).
- DALEJ:  P6 (sanity mas/limitów) — ostatni etap oryginalnego planu P1-P6.

## 2026-07-08 · Reanaliza: „zakleszczenie kierownicy" to fantom + opcja arcade-centrowania · (do commitu)
- CO:     głęboka reanaliza całego audytu+planu na prośbę Jozza. Decydujący eksperyment (kolumna vs wahacz + odczyt maglownicy + jazda) OBALIŁ narrację TECH_DEBT #9: nie ma zatrzasku geometrycznego. Naprawiona mylona sonda P1 (uderzenie→JAZDA→asercja samocentrowania), przepisany TECH_DEBT #9 na ✅ROZWIĄZANE/fantom, dodany opcjonalny suwak `rackCenteringHertz` (arcade auto-centrowanie na postoju, domyślnie 0=off).
- CZEMU:  dwa razy trafiłem w to samo „zakleszczenie" (P1, P4) i opisałem jako nierozwiązany watch-item — Jozz kazał zbadać głębiej i podejść inaczej.
- EFEKT:  DOWÓD: przy „zakleszczeniu" maglownica stoi na LIMICIE (-0.0811≈-rackTravel), nie w centrum; jazda centruje koło -29°→1.4° @12.7m/s (caster działa dopiero w ruchu — poprawna fizyka, potwierdził Jozz). Błąd był w sondzie (mierzyła samocentrowanie na STOJĄCYM aucie). Zmierzone przy okazji: słaba sprężyna centrująca nie rusza stojącej opony (moment parkingowy) — suwak działa od ~10 Hz (hz=2 nic, hz≥10 pełne), przy włączeniu tarcie statyczne ustępuje sprężynie. Walidator OK (P1 samocentrowanie w ruchu ~1.3°, P4 centering-assist off→trzyma -29°/on→1.0°, jazda ze wspomaganiem prosto, M7 bez regresji rack rms 0.17), test.exe PASS, boot-smoke 0, zrzut UI (suwak „Wspomaganie powrotu (arcade)" 0 Hz).
- DALEJ:  domyślnie realizm bez zmian; arcade-centrowanie czeka jako opt-in gdyby Jozz chciał. Kolejny wg planu: P5 (brakujące suwaki max-skręt/toe + opisy).

## 2026-07-08 · P4: tarcie statyczne/kinetyczne zębatki + odkrycie bezpiecznego progu · (do commitu)
- CO:     `rackFrictionForce` → `rackStaticFrictionForce`/`rackKineticFrictionForce` (Coulomb, próg prędkości 0.01 m/s), config_io z kompatybilnością (stary klucz → static=wartość, kinetic=0.5×), UI dwa suwaki, 3 presety zmigrowane, nowa sonda walidatora `RunP4SteeringReturnProbe`.
- CZEMU:  audyt S1 — płaskie 250N zatrzymywało kierownicę martwo w centrum bez zależności od prędkości; cel: naturalny powrót + lekki przestrzał po mocnym odbiciu.
- EFEKT:  **sugerowany przez audyt zakres (~80-120N) okazał się NIEBEZPIECZNY** — sweep walidatora ujawnił OSTRY próg 130→140N (camber lądowania 3.5m: 11-12°→0.6-0.8°, ten sam mechanizm co TECH_DEBT #9) ORAZ osobny, wyższy próg ~150→200N dla stabilności yaw nadwozia podczas samego odbicia (heading przed jazdą: -40°@150N vs -13°@250N stare; dz/dx ratio 0.85@150N *fail* vs 0.43@200N *pass*, próg 0.6). Przestrzał (kąt<0° po puszczeniu z pełnego zablokowanego skrętu) NIE występuje NIGDZIE w całym testowanym zakresie 40-250N — ani przy statycznym zwolnieniu, ani przy łagodnym "kopnięciu" 1-3 m/s w trakcie jazdy (za słabe kopnięcie = wraca bez przestrzału, za mocne = trafia w TECH_DEBT #9). Finalne bezpieczne wartości: static=250N (bez zmian), kinetic=200N (obniżone ze starych 250N, z zapasem nad obydwoma progami). Zamiast forsować nieosiągalne "przestrzał istnieje", asercja zamieniona na diagnostykę (patrz komentarz w kodzie sondy). Walidator OK (wszystkie sondy P1-P4 + M7 bez regresji, w tym hands-off align rack rms 0.17 - bez zmian), test.exe PASS, boot-smoke 0 błędów, zrzut UI potwierdza suwaki 250N/200N.
- DALEJ:  P5 (brakujące suwaki + opisy) — **przy każdej przyszłej zmianie tarcia zębatki najpierw sonda lądowania 3.5m**, nie tylko udarowa (TECH_DEBT #9 aktualizacja).

## 2026-07-08 · P3: prześwit przód/tył rozprzęgnięty od twardości · (do commitu)
- CO:     `suspensionPreload` → `suspensionPreloadFront/Rear` (config, config_io z kompatybilnością wsteczną, 3 presety, UI - dwa suwaki), usunięte mnożenie `* scale` przy wszystkich 3 miejscach tworzenia/aktualizacji coilovera (wahacz, wahacz wleczony, `ApplySuspensionTuning` na żywo).
- CZEMU:  audyt K3 — `restLength = design + preload*scale` sprzęgało prześwit z twardością W ZŁĄ STRONĘ (kod sam przyznawał w komentarzu intencję kompensacji, ale kierunek był odwrotny do fizyki ugięcia F/k).
- EFEKT:  pomiar PRZED: chassis.y 0.9204→1.0682→1.1194 m dla scale 0.5/1.0/2.0 (rozrzut 0.199 m). PO: 0.9337→1.0682→1.0903 m (rozrzut 0.1566 m) — identyczne przy scale=1.0 (sanity check), reszta to NATURALNE ugięcie sprężyny (k~scale², nie bug — udokumentowane w komentarzu sondy, próg asercji 0.18 m, nie "blisko zera"). Niezależność osi: preloadFront 0.07→0.12 (rear bez zmian) → przód -0.0396 m travel, tył +0.0010 m (praktycznie zero crosstalk). Kompatybilność wsteczna zweryfikowana RĘCZNIE zrzutem (stary plik sesji z samym kluczem `suspensionPreload: 0.15` → oba suwaki UI pokazują 0.150 m). Walidator OK, test.exe PASS, boot-smoke 0 błędów.
- DALEJ:  P4 (powrót kierownicy, rozdział tarcia statyczne/kinetyczne) — z tą samą samodzielną krytyczną analizą co P3.

## 2026-07-08 · Decyzja Jozza: P1 zaakceptowane, plan kontynuowany · docs
- CO:     Jozz przetestował ręcznie ~10 min ekstremalnej jazdy po P1 — ani razu nie odtworzył pierwotnie zgłoszonego zerwania kierownicy (wcześniej łatwe do wywołania przy małych siłach/prędkościach).
- CZEMU:  odpowiedź na STOP z poprzedniego wpisu (sonda syntetyczna pokazywała klinowanie niezależne od płotu P1).
- EFEKT:  P1 = sukces z perspektywy realnej jazdy. Decyzja: zostawić kod jak jest, obniżyć TECH_DEBT #9 z 🔴 na 🟡 (watch-item, nie blokujący), zapisać warning w dokumentacji, kontynuować plan P2→P6.
- DALEJ:  P3 (prześwit przód/tył) — tym razem z samodzielną krytyczną analizą planu przed implementacją (prośba Jozza), nie mechaniczne wykonanie.

## 2026-07-08 · P1: twist-fence z configu + STOP — inny mechanizm klinowania · (do commitu)
- CO:     `CreateControlArm` dostał `twistLimitRadians` (przód=maxSteer+10°=42°, tył=15°, liczone w `CreateWishboneCorner`), zastępując hardcode ±70° na przegubie kulowym; sonda P1 w walidatorze (martwy punkt, udar boczny FL, pełny skręt) — patrz TECH_DEBT #9.
- CZEMU:  audyt P1-P6 (task #40) — stary płot ±70° był POZA policzonym martwym punktem drążka (59.5°), co teoretycznie pozwalało na przeskok gałęzi rozwiązania bez powrotu.
- EFEKT:  martwy punkt 59.5° > nowy płot 42°/15° (asercja trzyma); pełny skręt w miejscu nieprzycięty (32.5°). **STOP** — sonda udarowa pokazuje, że koło (V≥10 m/s) i tak NIE wraca do zera (~16-34°) IDENTYCZNIE z i bez płotu (różnica <0.1°, kąty nigdy nie zbliżają się do żadnego limitu), niemonotonicznie względem siły uderzenia, niezależnie od tarcia zębatki (test z tarciem≈0 - bez zmiany). To ODRĘBNY mechanizm niż zdiagnozowany w audycie (prawdopodobnie druga gałąź `sqrt` w `ComputeJozzVehicleM6RackStroke`). Sonda zostawiona jako diagnostyczna (nie blokuje bramki) — pełne dane w TECH_DEBT #9. Walidator OK, test.exe PASS, boot-smoke 0 błędów, żadna istniejąca sonda M7 się nie zmieniła.
- DALEJ:  **czekam na decyzję Jozza** (TECH_DEBT #9) — jak zaadresować przeskok gałęzi w drążku. Dopóki nierozwiązane, P3-P6 z planu stroją się do wciąż złamanego układu (zasada planu) — nie kontynuuję automatycznie.

## 2026-07-08 · P2: RecomputeRackTravel() w Apply + tripwire + regresja · (do commitu)
- CO:     `ApplyPendingStructuralSetup()` (rig_lab) woła teraz `RecomputeRackTravel()` przed `CreateVehicle()`; nowy tripwire w `CreateJozzVehicleM6` (printf, nie assert) porównuje `config.rackTravel` z przeliczonym na miejscu; nowa sonda walidatora `RunP2RackTravelRegressionProbe`.
- CZEMU:  audyt P1-P6 (task #41) — bez tego suwaki geometrii kierownicy (Apply) rozstrajały limit maglownicy względem realnej geometrii, otwierając drogę do przestrzału w martwy punkt drążka.
- EFEKT:  walidator OK, brak "stale rackTravel" WARNING; sonda P2 pokazuje rackTravel 0.0807 m → 0.1033 m dla steeringArmBack 0.17→0.22 (dowód że przeliczanie coś zmienia); test.exe PASS, boot-smoke 0 błędów. Zmiana czysto logiczna (brak renderu do obejrzenia).
- DALEJ:  P1 (twist-fence z konfiguracji) — rdzeń planu, zależny od P2.

## 2026-07-08 · Plan wykonawczy P1-P6 dla słabszych agentów · docs
- CO:     `PLAN_STABILNOSC_PROWADZENIE_PL.md` — krok-po-kroku (pliki, funkcje, liczby oczekiwane, warunki STOP, checklista bramki); taski #40-45 z zależnościami; kolejność P2→P1→P3→P4→P5→P6.
- CZEMU:  Jozz będzie przydzielał etapy tańszym modelom — plan musi być wykonywalny bez zgadywania.
- EFEKT:  API zweryfikowane przed planowaniem (b3PrismaticJoint_GetSpeed box3d.h:1377, b3SphericalJoint_SetTwistLimits — istnieją); każdy etap ma reprodukcję-przed-naprawą i kryteria liczbowe.
- DALEJ:  wykonawca zaczyna od P2 (task #41), potem P1 (#40). Jeden etap = jedna sesja.

## 2026-07-08 · Audyt fizyki/skrętu/stabilności · docs
- CO:     pełny audyt kodu rigu+labu+toru → `AUDIT_PHYSICS_STEERING_2026_07_08_PL.md` z planem P1-P6. Zero zmian w kodzie.
- CZEMU:  Jozz zgłosił łamanie skrętu pod przeciążeniem, martwe suwaki geometrii, sztuczny powrót kierownicy.
- EFEKT:  zidentyfikowany mechanizm „złamanego skrętu" (over-center drążka + płot twist ±70°), bug Apply bez RecomputeRackTravel (rig_lab:345), odwrócone sprzężenie preload·scale, saturacja HingeSwingLimit; suwaki geometrii DZIAŁAJĄ na fizykę, ale wizual ich nie pokazuje.
- DALEJ:  P1 (twist-fence z konfigu) i P2 (1-liniowy fix Apply) — od nich zależy sens dalszego strojenia.

## 2026-07-08 · Archiwizacja M0-M5 + subsystem-doc UI/presetów · docs
- CO:     31 historycznych docs (M0-M5, CODEX_*, PROJECT_AUDIT/STABILIZATION) przeniesione `git mv` → `docs/archive/`; nowy `SUBSYSTEM_UI_PRESETS_PL.md`.
- CZEMU:  domknięcie 2 drobiazgów z przeglądu technicznego (rozrost docs, TECH_DEBT #3 UI bez raportu).
- EFEKT:  `docs/` root czystszy (historia w jednym miejscu, git history zachowana); TECH_DEBT #3 ✅ zamknięte; README §9 i CURRENT_STATE_INDEX ścieżki poprawione.
- DALEJ:  workflow domknięty — czekam na sygnał do Etapu B (polish rigu/dumpera, P1 = deduplikacja socketów).

## 2026-07-08 · Sesja Debug + „Zresetuj świat" + CondenseDebugOverlay · fa9110a
- CO:     toggle zakładki Debug przeżywają „R" (osobny `build/..._debug_session.txt`); przycisk „Zresetuj świat" (reset in-place bez restartu hosta); opt-in `CondenseDebugOverlay()` w hoście sampli zwija blok silnika. Praca z równoległej konwersacji Claude Code.
- CZEMU:  „R" cicho wskrzeszał twarde domyślne debug (linie diagnostyczne wracały mimo wyłączenia); panel z 6 zakładkami potrzebował miejsca.
- EFEKT:  zwalidowane zielono (build 3/3, walidator OK, testy PASS, boot-smoke 0 błędów). Zero wpływu na inne sample (virtual domyślnie false).
- DALEJ:  `sample.h/.cpp` nie są już czystym upstreamem (odnotowane); 3. format zapisu (txt) — watch. Potem: realny polish zawieszenia/dumperów.

## 2026-07-08 · Analiza rig/dumper/mount + workflow + checkpointy · docs
- CO:     nowy `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` (stan wizualnego rigu) + ten dziennik; README §5 token-economy.
- CZEMU:  przygotowanie pod dalszą pracę; wizual dumpera był odłączony od fizyki i nigdzie zwięźle nieopisany.
- EFEKT:  wiedza o podsystemie zapisana w repo (nie tylko w pamięci agenta); handoff ma stały format.
- DALEJ:  polish rigu/dumpera wg planu P1–P4 w subsystem-doc — po zgodzie Jozza, mały krok = render.

## 2026-07-08 · Zasady oszczędności tokenów · b02a5df
- CO:     README §5 — zwięzłe outputy narzędzi, quiet git flags, grep>Read, batchowanie, 1 milestone/sesja.
- CZEMU:  Jozz: minimalizacja kosztu bez utraty jakości/rygoru.
- EFEKT:  bramka (build+walidator+test) i dyscyplina commitów bez zmian; skraca się tylko narracja w czacie.
- DALEJ:  stosować co sesję; TECH_DEBT #1 zamknięty (M7/M8 zacommitowane).

## 2026-07-08 · Commit + push całego M7+M8 · 1446c9d, d2da267
- CO:     ~tydzień niezacommitowanej pracy pogrupowany w 2 commity i wypchnięty na `jozz-vehicle-sandbox-m0`.
- CZEMU:  ryzyko #1 z przeglądu — praca poza historią gita, jeden zły reset ją kasuje.
- EFEKT:  branch zdalny kompletny; ustanowiona zasada autonomicznego commit/push agentów (main = tylko Jozz).
- DALEJ:  po każdym zielonym etapie — commit, nie czekać na prośbę.

## 2026-07-08 · Przegląd techniczny / porządkowanie · d2da267
- CO:     README_FOR_AGENTS przepisany na 1 front (M8); nowy TECH_DEBT_PL; CURRENT_STATE_INDEX odchudzony.
- CZEMU:  fronty były na M6/M7, sprzeczne z kodem (usunięty self-align), rozrost docs.
- EFEKT:  jedno repo-widoczne źródło prawdy dla Claude i Codex.
- DALEJ:  trzymać dyscyplinę: po realnej zmianie aktualizować README §2 + ten dziennik.

## 2026-07-08 · UI PL + presety + auto-sesja · 1446c9d
- CO:     6 zakładek PL, font Segoe UI+/utf-8, presety (`config_io`, `assets/vehicle_presets`), auto-zapis sesji, fix ID zakładek.
- CZEMU:  UI chaotyczne/po angielsku; „R" kasowało strojenie; zakładki skakały przy Apply.
- EFEKT:  strojenie przeżywa restart; zakładki stabilne; render zweryfikowany.
- DALEJ:  brak własnego raportu UI (TECH_DEBT #3) — zamknąć przy okazji.

## 2026-07-07 · M8 rig + opadająca poza · 1446c9d
- CO:     wahacze wpięte w authored-sockety, opadanie (`restArmDroopDeg`+`suspensionPreload`), kompensacja bump-steer.
- CZEMU:  fundament pod drift/offroad/ciężarówki; wahacze wyginały się do góry, wchodziły w oponę.
- EFEKT:  walidator OK; render potwierdzony; droop klamrowany na 16° (over-center Ackermanna).
- DALEJ:  droop >16° wymaga przeprojektowania kierownicy (TECH_DEBT #5, odłożone).

## 2026-07-06 · M7 realne siły · 1446c9d (rozwój przed commitem)
- CO:     wahacze jako ciała na zawiasach, back-drivable rack, napęd momentem, trailing-arm tył; usunięty skryptowy self-align.
- CZEMU:  kierunek BeamNG — zachowanie ma wynikać z konstrukcji, nie skryptu.
- EFEKT:  landing integrity, hands-off align, wheelspin — w walidatorze.
- DALEJ:  drivetrain (dyfry), model opony — roadmapa README §8.
