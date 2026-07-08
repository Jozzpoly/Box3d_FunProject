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
