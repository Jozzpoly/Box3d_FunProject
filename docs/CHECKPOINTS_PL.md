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

## 2026-07-12 · Etap 2 mapy: obstacle kit + poligon zawieszeń 6 lane'ów · (do commitu)
- CO:     nowy `jozz_vehicle_obstacle_kit.{h,cpp}` (15 parametrycznych generatorów: rampy/kicker/tabletop/gap-jump/stairs/whoops/washboard/rock-garden/ruts/off-camber/berm/logs/articulation, ostre=boxy transformowane, zaokrąglone=kapsuły, `customColorHex` per-shape); `jozz_vehicle_m5_test_course.cpp` przebudowany na 6-lane'owy poligon (`world_layout.h` §Poligon, x:150-195/z:-60..60) zamiast starych 4 ramp+2 washboard; etykiety stacji (`DrawString3D`, distance-cull 80 m) w obu labach (M5/M6, R11); propy odsunięte z osi jazdy na skraj placu fizyki (`kPropZoneOriginX/Z`); nowy env `JOZZ_M6_TELEPORT_XZ` (dowolne x,z, headless).
- CZEMU:  plan `MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md` — zastąpić przypadkowe rampy/washboard "mądrze zaprojektowanymi miejscami do testowania zawieszeń" z progresją trudności (feedback Jozza pkt 4-5).
- EFEKT:  build 3/3 OK, walidator 18/18 OK (z roota), test.exe 11/11 PASS, boot-smoke M5+M6 0 błędów sokol. Rendery `mapa_e2_*`: top-down poligonu (6 lane'ów, kolory zielony/żółty/czerwony czytelne), close-up kickera (kapsuła-lip widoczna na krawędzi), whoops+ruts+off-camber (zaokrąglenia vs ostre V-rowki), rock garden (23 losowe kamienie), articulation+berm (łuk banda poprawnie zakrzywiony). Autodrive L1 (8-12 m/s) i L6 (gap-jump 4 m + lądowanie) bez błędów fizyki, liczba shape'ów stabilna (107, zgodna z ręcznym przeliczeniem ~74 kit + 9 plate + 1 offroad + 14 prop + rig). R5 checklist: wszystkie `kit_*` shape'y przechodzą przez wspólny `MakeKitShapeDef(terrainCategoryBits,...)` — zweryfikowane grepem.
- DALEJ:  Etap 3 (tor i drift) — czeka na sygnał Jozza.

## 2026-07-12 · Fix: "R" zabierał kamerę z trybu trzeciej osoby (T) · e4c80ca
- CO:     `Sample::Sample` (sample.cpp) oraz konstruktory M5/M6 wyłączały `m_camera->m_thirdPerson` BEZWARUNKOWO przy każdej konstrukcji, nawet przy "R" restart — teraz to samo zabezpieczenie co przy `SetView` (`if (context->restart == false)`), więc restart tej samej próbki NIE zdejmuje trybu jazdy za autem (T).
- CZEMU:  Jozz: "R restartuje kamerę [z trybu T]" — jeździł w trybie trzeciej osoby, po R kamera zamrażała się w wolnej orbicie, bo tryb T był zdejmowany bez powodu (target odtwarza się od razu w tym samym konstruktorze, więc nic nie stało na przeszkodzie zachowaniu trybu).
- EFEKT:  Build+test.exe+walidator(18 sond, z roota)+boot-smoke M5/M6 zielone. Weryfikacja interaktywna: 2 próby przez Windows-MCP (desktop automation) — pierwsza wylądowała przez pomyłkę w NIEPOWIĄZANYM oknie (osobne, nie dotykane repo/sesja Jozza); druga potwierdziła, że syntetyczne naciśnięcia klawiszy (T, W) nie docierają do natywnego okna sokol/D3D11 mimo poprawnego fokusu (prędkość/HUD bez reakcji) — ograniczenie narzędzia automatyzacji przy raw-input, nie da się tak zweryfikować. Fix pozostawiony na potwierdzenie przez Jozza w realnej jeździe.
- DALEJ:  Jozz potwierdza w praktyce, że T przeżywa R.

## 2026-07-12 · Etap 1 §12: masyw z wezlami gorskimi (druga tura polishingu) · 8ab1635
- CO:     Po jeździe po pierwszej wersji góry Jozz poprosił o: wyższy szczyt, prawdziwe "węzły górskie" (nie tylko falę obrysu) agresywnie schodzące od głównej góry i zanikające na krawędziach, z mniejszymi górami na sobie. Zrealizowane wg planu §12 (5 mechanizmów): A) peak 12.5→17, promień 95→110, sufit 22→28; B) nowy mechanizm `ComputeMountain` — kątowy ridged szum wyostrzony potęgą (4-6 dominujących ramion) w pierścieniu 0.35R→2.0R od centrum, ADDYTYWNE sub-szczyty na grani ramienia; C) edge fade — masyw→0 w ostatnich 35 m przed krawędziami mapy, baza→70%; D) roughness czyta max(elevationShape, 0.8×masaMasywu) — granie i stoki masywu łapią skały.
- CZEMU:  Konkretny feedback Jozza po realnej jeździe (nie spekulacja) + jego własna diagnoza że stary mechanizm spurs to nie węzły. Pełne uzasadnienie: `MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md` §12.
- EFEKT:  Szczyty 7 seedów: 19.67-22.54 m (cel 18-24, bez klampa 27.5). Perf 1.21-1.22 ms/step (budżet 4ms, bez zmiany vs przed §12 — koszt tylko build-time). R4 49→49. AUTODRIVE 420 klatek przez teren z ramionami: 0 błędów. Rendery `30_aerial_arms`/`31_silhouette`/`32_wide_edges`/`33_edge_zoom`/`34_along_arm` obejrzane: wyraźne dominujące ramiona z dolinami, sub-szczyty na grani, brak ucięcia na krawędzi. Build+test.exe+walidator(18 sond, z roota)+boot-smoke M5/M6 zielone.
- DALEJ:  Etap 2 (kit przeszkód) — czeka na sygnał Jozza; teren offroad uznany za gotowy.

## 2026-07-12 · Walidacja mapy: spawn po footprincie + checkpoint dla "R" · 73ece06
- CO:     Feedback Jozza z jazdy: (1) spawn teleportu próbkuje teren pod WSZYSTKIMI 4 kołami (max z 5 punktów + 0.25 m zapasu) zamiast 1 punktu z 0.05 m — koniec z kołami wbitymi w stok; (2) "R" restart odtwarza CHECKPOINT: ostatnia kotwica teleportu + seed terenu persystowane w debug-session (`spawnAnchorX/Z`, `worldSeed`), teren regenerowany PRZED spawnem pojazdu.
- CZEMU:  Teleport na górę/stok zostawiał koła pod ziemią (1-punktowe próbkowanie na pochyłości); "R" cofał na Start i PODMIENIAŁ wyregenerowany teren na domyślny seed.
- EFEKT:  Rendery `24_gora_fixed`/`25_gleboko_fixed`: 4/4 kontakt kół, normalna postawa na obu kotwicach. Dowód checkpointu: run bez env po runie z teleportem+seed 777 → `[terrain] seed=1337` potem `seed=777` (regen z checkpointu) i spawn na kotwicy góry. NOWE USTALENIE o czerwonym probe `suspensionHertz`: wynik walidatora zależy od CWD (root repo: 18 OK; build/bin/Debug: FAILED 3.4≠3.5) — to CWD-zależna resolucja assetów, nie regres; zgłoszone jako osobny task.
- DALEJ:  Plan ostatniego polishingu terenu (góra wyżej, węzły górskie, edge fade) — do akceptacji Jozza.

## 2026-07-12 · Mapa Etap 1 final polish: góra centralna (fokus terenu) · 829ff95
- CO:     Na życzenie Jozza dodany centralny punkt fokusu — jedna naturalna góra rosnąca losowo koło środka offroadu (`ComputeMountain` w `jozz_vehicle_world_terrain.cpp`). Pięć złożonych mechanizmów: (1) centrum jitterowane seedem ±45 m, (2) radialna masa `smoothstep` (gradient szczyt→podnóże), (3) domain warp obrysu (podstawa nie jest kołem), (4) modulacja promienia szumem kątowym → promieniste granie/żleby (nie kopiec-wulkan), (5) zupełnie nowy 4-oktawowy ridged FBM na rzeźbę szczytu (duże+średnie+drobne nierówności). Teren bazowy tłumiony pod masą (góra zastępuje pofalowanie, nie dubluje). Sufit heightfielda 14→22 m. Nowa kotwica `Offroad - gora` + env `JOZZ_M6_TERRAIN_DUMP`.
- CZEMU:  Terenowi brakowało centralnego fokusu; Jozz chciał górę ze szczytem "o połowę wyższym" od standardu, nowym realistycznym szumem, z prośbą o krytyczne+kreatywne własne rozwiązania. Pełne uzasadnienie: `MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md` §11.
- EFEKT:  Build+test.exe+boot-smoke M5/M6 zielone (0 błędów sokol). Szczyty 7 seedów: 13.8–17.9 m = ~1.5–1.9× standardowych ~9 m grani, zawsze <21.5 m klampa (bez płaskich wierzchołków). Perf 1.15 ms/step / 871 fps (budżet 4 ms) — szum góry to koszt build-time, zero wpływu na krok. R4: 49→49 shape'ów po 10 regeneracjach. Rendery `11_mountain_aerial`/`12_mountain_silhouette` obejrzane: masyw z graniami wyrasta ponad teren, poszarpana sylweta. UWAGA: walidator ma 1 CZERWONY probe (`suspensionHertz` preset 3.5→3.4) — regres ZASTANY, potwierdzony na czystym HEAD (git stash+rebuild), niezależny od terenu, do naprawy osobno.
- DALEJ:  Etap 2 (kit przeszkód) po akceptacji Jozza; osobno: fix pre-existing suspensionHertz preset probe.

## 2026-07-12 · Mapa Etap 1 doszlifowanie: offroad 400x400 + ridged/warp/roughness · 783d0a3
- CO:     Na wyraźne życzenie Jozza ("bardzo ważna sprawa, kilkadziesiąt godzin tygodniowo"): offroad 320×320→400×400 (równy kaflom płyty, siatka 257→321 pkt); `ComputeOffroadHeightLocal` przepisane — makro to teraz ridged noise (1-|szum|)², 2 oktawy, próbkowane przez domain warp (grzbiety wiją się, nie trzymają osi siatki); nowa warstwa `roughness` skaluje amplitudę mezo/mikro na podstawie kształtu makro (wysoko=szorstko, nisko=gładko), bramkowana osobnym szumem, zastępuje starą "maskę płaskości" niezależną od wysokości.
- CZEMU:  3 konkretne uwagi Jozza ze screena i opisu: (1) offroad ma być tej samej wielkości co płyta, (2) chropowatość ma zależeć od wysokości z "odpowiednim szumem na jej występowanie", (3) teren bliżej prawdziwych gór (erozja poza zakresem, ridged+warp jako tani substytut). Pełne uzasadnienie: `MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md` §10.
- EFEKT:  Build+walidator(18 sond)+test.exe+boot smoke M5/M6 zielone. Wydajność 1.37–1.65 ms/step (budżet 4 ms, zapas ≥2.4×) mimo +7 próbek szumu/punkt i większej siatki — koszt tylko przy budowie/regeneracji, nie w kroku fizyki. R4 nadal zamknięte: 49→49 shape'ów po 10 regeneracjach. Rendery `06_threequarter_mid`/`07_wide_threequarter`/`08_valley_vs_ridge` obejrzane: sieć organicznych grzbietów, równe kafle+szew bez uskoku, kontrast gładka dolina/szorstki grzbiet.
- DALEJ:  Etap 2 (kit przeszkód) po akceptacji Jozza.

## 2026-07-11 · Mapa Etap 1: fundament terenu (płyta 3x3 + offroad FBM) · 44170d4
- CO:     `jozz_vehicle_world_layout.h` + `jozz_vehicle_world_terrain.{h,cpp}` — płyta 400×400 (3×3 kafle, 1 ciało/9 shape'ów), offroad 320×320 z własnym generatorem FBM (3 oktawy + maska płaskości + gradient trudności), zakładka 2 m POD płytą, seed+„Przebuduj teren" w UI, teleport minimalny (Start/wjazd/głęboko); stary `b3CreateWave`-patch (wystawał NAD płytę) usunięty z `jozz_vehicle_m5_test_course`. Dodatkowo 4 nowe env (`JOZZ_M6_TELEPORT/AUTODRIVE/PERF_DUMP/REGEN_COUNT`) do headless testów mapy bez klawiatury.
- CZEMU:  Feedback Jozza (mapa za mała, offroad wystaje nad płytę, jeden rodzaj szumu) + akceptacja planu 2026-07-11; ten track wchodzi przed edytorem rigu.
- EFEKT:  Build+walidator (18 sond)+test.exe+boot smoke M5/M6 zielone. Rendery `mapa_e1_*` obejrzane: szew bez uskoku z 3 kątów, wyraźnie zróżnicowany masyw, terenu inny po regeneracji (seed 777 vs 1337), auto stabilne na 18.5 m/s nad szwem. Wydajność: 1.12–1.20 ms/step na wszystkich 3 scenariuszach (budżet 4 ms, zapas ~3.3×) — heightfield NIE jest wąskim gardłem. R4 (przeciek mesha) zamknięte liczbowo: 49→49 shape'ów po 10 regeneracjach.
- DALEJ:  Etap 2 (kit przeszkód + poligony zawieszeń) po commit+push tego etapu.

## 2026-07-11 · Plan przebudowy mapy: roadmapa + 6 etapów · docs
- CO:     `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md` + `MAPA_ETAP_1..6_*.md` — fundament terenu (płyta 400×400 z 3×3 kafli, offroad-heightfield 320×320 z 3 oktawami szumu i zakładką POD płytą), kit przeszkód + 6 lane'ów, tor+drift+stoper na sensorach, plac fizyki (shaker/taśma/most/eksplozja), spawner+stress, nawigacja+telemetria.
- CZEMU:  Feedback Jozza 2026-07-11: mapa za mała i monotonna, offroad wystaje NAD płytę, przeszkody prymitywne, brak toru/spawnera — track wchodzi PRZED edytor rigu.
- EFEKT:  Kompletna roadmapa z analizą stanu (screeny+kod), rozszerzeniami P1–P9 (teleporty, shaker 4-post, bramki czasowe, strefy tarcia, rolling road…), ryzykami R1–R11 i bramkami per etap; inwentarz ficzerów box3d zweryfikowany w nagłówkach silnika.
- DALEJ:  Akceptacja Jozza (w tym rozmiary z §5 i propozycje P1–P9) → Etap 1; przy starcie E1 aktualizacja README §2.

## 2026-07-11 · Finalizacja E3: rama+rig domyślne, kolider chowany pod skinem · 1e9a3fb
- CO:     Domyślne `bodyVisualModel="rama_rurowa"` + `frontSuspensionVisualModel="rig_kierowniczy"` (decyzje Jozza D1/D2). Kolider: bryła chassis chowa się pod nadwoziem 3D przez ISTNIEJĄCY `SetShapeHidden` (ten sam wzorzec co bryły kół; `chassisShapeId` dodany do `JozzVehicleM6`) + nowy checkbox Debug „Pokaż bryłę kolizyjną nadwozia" i env `JOZZ_M6_COLLIDER` (podgląd warstwy kolizji zawsze 1 klik). Presety built-in zostają częściowe (decyzja Jozza); sonda determinizmu: sabotaż odwrócony na „brak"/„klasyczny" (fabryka=rama/rig, inaczej asercje puste).
- CZEMU:  Zamknięcie planu finalizacji (bc9b4c5): stan z rozgrzewki G1/G3 + rama = domyślny, spersystowany stan gry.
- EFEKT:  gate 3/3 OK, walidator OK (liczby fizyczne IDENTYCZNE), doc-drift czysty; quad świeżego bootu (bez env/sesji) = rama+rig+schowana bryła w 4 ujęciach; zrzut `JOZZ_M6_COLLIDER=1` = bryła wraca na wierzch. Dokument planu Etapu 3 mylił się co do wariantu A („zbadaj czy callback ma shapeId") — `SetShapeHidden` istniał i był używany przez ten sam lab dla kół.
- DALEJ:  Wymóg Jozza z D3 (STOP-gate): import modelu z Blockbench jako CIAŁO KOLIZYJNE + importer in-game pod edytor rigu → zapisany w `EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md` (O8), wymaga własnego planu.

## 2026-07-11 · Nadwozie: model ramy (Nadwozie.gltf) sztywno na chassis · a275947
- CO:     Nowy `assets/source/Nadwozie.gltf` (rama nadwozia Jozza, samodzielny Blockbench-export: bufor+tekstura jako data URI). Wczytany `LoadStaticGltf` do `m_bodyVisual`, rysowany `DrawBodyVisual()` sztywno na żywym ciele chassis: `DrawAtTransform(chassisLive ∘ m_bodyChassisLocal)`. Placement STAŁY w lokalnym ukł. chassis policzony z ZMIERZONEJ geometrii (print jednorazowy, potem usunięty): model 3.28 m (dł, Z) × 2.73 m (szer, X) × 1.23 m (wys, Y); pojazd rozstaw osi 2.50 m (przód +X), rozstaw kół 2.10 m, chassis origin 0.60 m nad osią. Yaw −90 (model +Z tył→−X tył, model X szer→Z), p=(0,−0.60,0). Za przełącznikiem `JOZZ_M6_BODY` + checkbox Debug „Nadwozie 3D", domyślnie WYŁ.
- CZEMU:  Prośba Jozza („podłącz ten model pod nadwozie samochodu") — rozgrzewka: doświadczyć, jak rig przymocowuje CAŁĄ ramę do jednego ciała (nie per-narożnik). Domyślnie wył., bo w labie zawieszenia rama zasłania zawieszenie.
- EFEKT:  build 3/3 OK, walidator OK, test PASS, smoke 0 err; walidator NIETKNIĘTY (czysto wizualnie — sam mesh na istniejącym ciele). Render obejrzany (profil + 3/4): orientacja poprawna (spoiler nad tylnymi kołami = tył, fotele/zastrzały/klatka na górze), rozstaw osi/kół i wysokość pasują do kół. ZNANY artefakt: koliderowa bryła chassis (tan) rysuje się nieprzezroczyście na ramie — silnik nie ma per-bryła flagi „nie rysuj" (tylko globalna `drawShapes`, gasi też ziemię/przeszkody), więc świadomie NIE ruszam współdzielonej ścieżki draw.
- DALEJ:  Do decyzji Jozza: (a) domyślnie ON?, (b) przygasić/ukryć koliderową bryłę chassis pod ramą, (c) dostroić placement. G4 (cardan) opcjonalnie.

## 2026-07-11 · Edytor rigu G3: drążek kierowniczy → środek racka + dumper · e7775a9
- CO:     W `jozz_vehicle_m6_rig_lab_steering_visual.cpp` `DrawSteeringRig()`: drążek kierowniczy (node 7) rysowany `DrawPartBetween` od ŚRODKA realnego racka (`b3Body_GetWorldPoint(rackId,{0,0,0})`, inboard) do knuckla (outboard) — lewy i prawy drążek spotykają się w centrum (reguła Jozza „prawy z lewym się łączył") i przesuwają razem z maglownicą. Dumper rigu: górne oko na chassis (socket damperUpper), dolne oko niesione przez żywą pozę dolnego ramienia (`JozzVehicleComputeArmPlacement`+`MapAuthoredPoint` na socketie damperLower), rysowany `DrawTelescopingDamper`, gate `m_showDumper` (tylne dampery pomijają przód gdy rig on).
- CZEMU:  G3 z planu rozgrzewki — drążek+dumper to najważniejsza prośba Jozza po potwierdzeniu G1. Pierwsza próba (inboard na KOŃCU racka) dała za krótki kikut (końce maglownicy siedzą przy knucklu) → poprawka na środek racka.
- EFEKT:  build 3/3 OK, walidator OK, test PASS, smoke 0 err; walidator NIETKNIĘTY (czysto wizualnie). Render obejrzany (przód z dołu, symetria): drążek dłuższy, sięga do centrum; diff pikseli dumper on/off = pionowy amortyzator (rysuje się). Rozdział dynamiczny G1 potwierdzony przez Jozza na żywo (skręt A/D); rozjazd „zbyt osobny" → dług #14. Odkrycia O6 (kotwica na innym ciele potrzebuje SUB-pozycji: koniec vs środek) + O7 (trzeci tryb wiązania: socket niesiony przez pozę części stretch-between) w dokumencie wymagań.
- DALEJ:  Podłączenie modelu nadwozia (`Nadwozie.gltf`) pod chassis (prośba Jozza, poza sekwencją gejtów). G4 (cardan) opcjonalnie.

## 2026-07-11 · Edytor rigu G0+G1: import steering-rig na przód labu M6 · 1294509
- CO:     Start tracku edytora rigu (rozgrzewka=badanie+audyt). G0: żywe docy `docs/PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md` + `docs/EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md`. G1: nowy `jozz_vehicle_m6_rig_lab_steering_visual.cpp` (Load/Setup/Draw) importuje `OneSided_Steering_Suspension_Rig` na PRZEDNIE narożniki jeżdżącego labu M6; tył zostaje na starym mount (decyzja Jozza D1a). Za przełącznikiem `JOZZ_M6_STEERING_RIG` + checkbox Debug (domyślnie OFF).
- CZEMU:  Fundament pod edytor rigu — wiążąc nowy model na żywe ciała realnego pojazdu na żywo wychodzą wymogi edytora (gizmos, rodzic per część, pivot). Jozz chce testować przód+tył razem.
- EFEKT:  build 3/3 OK, walidator OK, test PASS, smoke 0 err; walidator NIETKNIĘTY (czysto wizualnie — te same ciała, zero nowych jointów). Render obejrzany (ON vs OFF, czoło, 3/4): 2 przednie narożniki = nowy rig, tył = stary, symetria L/P, przyczepiony, toggle przełącza tylko przód. **Rozdział O1 zaimplementowany od startu:** WheelCenter→`knuckleId` (skręca), ChassisMount_b + końce wahaczy→`lowerArmId` (jeździ, NIE skręca), brackety→`chassisId`. Odkrycia na żywo O2–O5 (bake=poza roota / kruchość magic-numberów węzłów / kotwice spoza modelu / konwencja mirror) w dokumencie wymagań.
- DALEJ:  Ręczny test skrętu Jozza (dynamiczny rozdział — headless nie wciska A/D). Potem G3 (drążek→realny rack + dumper), G4 (cardan). Świadomie pominięte w G1: drążek/dumper/cardan, per-ramię górne/dolne osobno.

## 2026-07-11 · R5: suspension_rig — ekstrakcja czystej geometrii · 22bfcf0
- CO:     6 publicznych funkcji world-free (MakeWishboneHardpoints, DefaultTrailingArmGeometry, ComputeRackStroke, ComputeSteeringDeadPointDeg, SanitizeConfig, DefaultConfig — ciągły blok 154–507) przeniesione bajt-w-bajt z `jozz_vehicle_m6_suspension_rig.cpp` (1758→1404 l.) do nowego `jozz_vehicle_m6_geometry.{h,cpp}`. Dodane do `JOZZ_VEHICLE_CORE_FILES` (linkowane do samples I walidatora). `suspension_rig.h` NIETKNIĘTY (deklaracje zostają) → zero churnu w ~10 callerach.
- CZEMU:  R5 z planu — "serce prep-u pod edytor" (edytor liczy hardpointy/martwy punkt bez tworzenia świata); zmniejszy suspension_rig przy okazji (ostatni watch-item TECH_DEBT #7).
- EFEKT:  build 3/3 OK (walidator linkuje geometry.cpp — placement w CORE_FILES potwierdzony); `-DiffBaseline` walidator 349 linii IDENTYCZNE (mocny dowód: geometria KARMI kotwice jointów, więc dryf FP pokazałby się w liczbach); quad IDENTYCZNY (hash); diff suspension_rig = czysto move (1 include +, 355 −); ciało geometry.cpp = usunięte linie bajt-w-bajt. **Zakres zawężony za zgodą Jozza (STOP-gate):** wyciągnięto 6 z 9 funkcji planu; 3 helpery wewnętrzne (HingeSwingLimit/SteeringLinkDroopLift/SteeringArmWithToe) ZOSTAŁY — to `static` impl-detale fizyki, SteeringArmWithToe kaskaduje zależność od IsFrontCorner/IsLeftCorner; wyciąganie = ryzyko za zero wartości edytora (§2.2/2.3/2.4).
- DALEJ:  Seria R1–R5 zamknięta. R6 (struktura katalogów) i R7 (solver kontaktu — jedyny zmieniający zachowanie) OPCJONALNE, każdy za osobną zgodą Jozza. Warstwa zdolności contentu (TECH_DEBT #6/#12/#13) — osobny track.

## 2026-07-11 · R4: visual_mesh — podział na loader + rysowanie · fd5a700
- CO:     `jozz_vehicle_visual_mesh.cpp` (1968 l.) → `_loader.cpp` (1761 l.: cały parser glTF/skin w anonimowej ns + `LoadStaticGltf`/`LoadSkinnedGltf`/`Destroy`/`IsLoaded`/`PartCount`) + `_draw.cpp` (232 l.: `Draw*`/`FindPart`/placement/`DrawTelescopingDamper`/`ComputeJozzVehicleWheelVisualCorrection`); move-only, nagłówek publiczny bez zmian.
- CZEMU:  R4 z planu — kolejny watch-item z TECH_DEBT #7 spłacony; prostszy niż R3 (każda metoda była już poza-klasowa, zero transformacji sygnatur — czyste cięcie tekstu).
- EFEKT:  cięcie skryptem z asercjami granic (zero luk/nakładek, 26..1968 pokryte dokładnie); build 3/3 OK za pierwszym razem; `-DiffBaseline` walidator 349 linii IDENTYCZNE; quad render IDENTYCZNY (hash — rig lab faktycznie ćwiczy `visual_mesh` przez mocowania/dumpery). Anonimowa przestrzeń nazw z parserem okazała się używana WYŁĄCZNIE przez oba `Load*` (zweryfikowane grepem) — trafiła w całości do `_loader.cpp`, dając czysty podział.
- DALEJ:  R5 (suspension_rig: ekstrakcja czystej geometrii, „serce prep-u pod edytor") — za sygnałem Jozza. Warstwa zdolności contentu (TECH_DEBT #6/#12/#13) osobno.

## 2026-07-11 · R3: rig_lab — podział na nagłówek wewnętrzny + 4 TU · 2de7c18
- CO:     `jozz_vehicle_m6_rig_lab.cpp` (2003 l.) → `_internal.h` (klasa, 42 metody) + 4 TU (main / `_ui_tabs` / `_persistence` / `_mount_visual`); move-only.
- CZEMU:  spłata długu monolitu (TECH_DEBT #7) + tańsza nawigacja/sesje agentów przed pracą nad contentem; fundament pod R4/R5.
- EFEKT:  build 3/3 OK; walidator 349 linii IDENTYCZNE; quad render IDENTYCZNY (hash); 6 zakładek (TAB 0-5) piksel-identycznych przed/po. Inwentarz planu zaniżony (35→42: 4 statyczne składowe); `Render()` NIE rozbijany na helpery (czysto move-only).
- DALEJ:  R4 (visual_mesh: loader vs rysowanie) — za sygnałem Jozza. Warstwa zdolności contentu (TECH_DEBT #6/#12/#13) osobno.

## 2026-07-10 · R2: config_io — tabela pól zamiast ręcznych write/read · 74b6d69
- CO:     `jozz_vehicle_m6_config_io.cpp` przepisany: 71 pól (51 root + 13 wishbone + 4 trailingArm + 3 wheelEnvelope, zmierzone dokładnie) w jednej uporządkowanej, otagowanej typem tablicy (`template <typename Owner> struct JozzFieldDesc` + anonimowa unia wskaźników-do-składowej — bez makr X), napędzającej i writer, i reader. Root podzielony na 3 segmenty wokół 2 zagnieżdżonych obiektów (kolejność JSON-a mieszała typy, więc „osobne tablice per typ" złamałoby kolejność — stąd jedna tablica z tagiem, nie tablice per typ jak sugerował dosłowny zapis planu). Legacy-migracje (suspensionPreload 1→2, 3 martwe klucze rack-friction) zostały ręczne, jak w planie.
- CZEMU:  R2 — jedyny etap „nowego kodu" w serii; usuwa klasę błędu „dodano pole do writera, zapomniano w readerze" (dwa miejsca → jedno).
- EFEKT:  Build czysty. `-DiffBaseline`: 349 linii identyczne (w tym „ran 18 probes") — litera R2 wymaga zera różnic w SAMYM walidatorze. Poprawność tabeli zweryfikowana WYKONAWCZO, nie tylko czytaniem kodu: tymczasowa sonda z unikalną wartością na każdym z 71 pól (save→load→porównaj, lista pól wpisana ręcznie z definicji structu, NIE skopiowana z tabeli pod testem) — wszystkie 71 wróciły dokładnie, exit 0, 0 „bad". Usunięta przed commitem (na stałe złamałaby `-DiffBaseline`). `rackTravel`/`filterGroupIndex` świadomie POZA tabelą (jak w starym kodzie) — tabela zbudowana z listy write/read, nie odbiciem structu.
- DALEJ:  R3 (rig_lab → TU per odpowiedzialność, move-only, wyższa stawka — kod shippingowy).

## 2026-07-10 · R1: podział validation.cpp na harness + sondy · f43bb9f
- CO:     `jozz_vehicle_validation.cpp` (2710 l., monolit) → 7 plików: `validation/jozz_validation_helpers.{h,cpp}` (CheckTrue/CheckApprox, IsM6VehicleStateValid, M6Chassis*, CreateM6SmokeGround — zewnętrzne linkowanie, wołane z 4 plików sond), `validation/jozz_probes_{m5_m6,m7,steering,config}.cpp` (18 sond wg bucketów z planu), `jozz_vehicle_validation.cpp` zredukowany do slim main (kontrakt-checki + rejestr + deklaracje 18 sond). CMake: nowa `JOZZ_VALIDATION_FILES` obok CORE. Cięcie skryptem (dokładne zakresy linii wycięte z oryginału, nie retypowane ręcznie) — zero ryzyka literówki.
- CZEMU:  R1 z `PLAN_WIELKI_REFACTOR` — trening podziału na kodzie nie-shippingowym przed R3-R5 (rig_lab, shippingowy kod).
- EFEKT:  build 3/3 OK za pierwszym podejściem (żaden brakujący include). `-DiffBaseline`: 349 linii walidatora IDENTYCZNE co do bajta z baseline sprzed R1, w tym dokładna linia „ran 18 probes". test PASS, smoke 0 err. Świadome odstępstwo od planu: „wspólne stałe kroku" (timeStep/subStepCount) NIE wydzielone do stałej — kosmetyczna zmiana bez wpływu na diff, pominięta żeby nie dodawać powierzchni edycji bez korzyści (~36 miejsc w pliku "zero ryzyka runtime").
- DALEJ:  R2 (tabela pól config_io, jedyny etap "nowego kodu" poza R0) — osobna sesja/commit.

## 2026-07-10 · R0: poprzeczka baseline-diff (start wielkiego refaktoru) · ed17457
- CO:     `tools/gate.ps1` + `-SaveBaseline` (snapshot stdout walidatora → `build/gate_baseline.txt` + quad render → `build/gate_baseline_shots/`, oba gitignored) i `-DiffBaseline [-Shots]` (pełna bramka + diff linia-po-linii vs baseline; PIERWSZA różniąca się linia = FAIL; `-Shots` porównuje hash quada). Filtr linii czasowych (duration/elapsed) defensywny — walidator dziś ich nie drukuje. README §3 + PLAN R0 zaktualizowane.
- CZEMU:  R0 z `PLAN_WIELKI_REFACTOR` — dla move-only refaktoru „bramka zielona" NIE wystarcza (kolejność tworzenia ciał/jointów wpływa na solver, a zielone tego nie widzi); poprzeczka = liczby IDENTYCZNE co do bajta.
- EFEKT:  determinizm ZMIERZONY (założenie R0, nie z opisu): walidator 349 linii identyczne w 3 uruchomieniach, render quad ten sam hash PNG. Weryfikacja OBUSTRONNA: niezmienione repo → `-SaveBaseline`+`-DiffBaseline -Shots` zielone (walidator IDENTICAL + render hash match); wstrzyknięta subtelna zmiana `suspensionHertz` 6.0→6.1 (CAŁA bramka zielona: walidator OK/test PASS/smoke 0) → diff CZERWONY na linii 87 (`settle sag 0.046→0.044 m`); po cofnięciu znów zielone. Working tree czysty (cofnięcie bit-identyczne).
- DALEJ:  R1 (podział `validation.cpp` 2691 l. na harness+sondy, trening na nie-shippingowym kodzie) — kończy się `-DiffBaseline` zero różnic. Jeden etap = jedna sesja = jeden commit.

## 2026-07-09 · Plan WIELKIEGO REFACTORU (R0–R7) · docs
- CO:     `PLAN_WIELKI_REFACTOR_2026_07_09_PL.md` — mapa zapowiedzianego przez Jozza etapu ciężkich spraw: R0 baseline-diff (poprzeczka „liczby IDENTYCZNE co do bajta", nie „zielone"), R1 podział validation.cpp (2691 l.), R2 tabela pól config_io (koniec 71+51 ręcznych linii; metadane pól = prep edytora), R3 rig_lab→TU per odpowiedzialność (35 metod, klasa do internal-header), R4 visual_mesh loader/draw, R5 ekstrakcja czystej geometrii (serce prep-u edytora), R6/R7 opcjonalne za osobną zgodą (katalogi; #12 solver — jedyny zmieniający zachowanie). Zero kodu — plan. Taski #56–63.
- CZEMU:  Jozz: zaplanować solidnie i krytycznie następny etap. Plan ugruntowany POMIAREM (inwentarze funkcji per plik) + sekcja samokrytyki (ryzyko #1: solver zależy od KOLEJNOŚCI tworzenia — stąd poprzeczka R0; scope-creep „przy okazji" zakazany; anty-cel: projektowanie pod edytor na zapas).
- EFEKT:  każdy etap = 1 sesja + `-DiffBaseline` zero różnic + commit; kolejność R0→R1 (trening na nie-shippingowym)→R2→R3→R4→R5; STOP gdy diff pokaże JAKĄKOLWIEK różnicę.
- DALEJ:  **STOP — czekam na akceptację planu przez Jozza** (+ dwie osobne decyzje: R6 katalogi? R7 solver?). Po akceptacji start od R0.

## 2026-07-09 · Porządki D+E+F+G: rejestr sond, persystencja, env, STOP-gate · 4cded89, bc209cd, 7f0e197, c0cfad4
- CO:     D: rejestr sond walidatora (tablica {nazwa,fn} + „ran 18 probes" — zapomniana sonda widoczna). E: mapa persystencji `SUBSYSTEM_UI_PRESETS §1b` + `invertSteering` persystuje (debug-session), solver kontaktu ODŁOŻONY z powodem (#12). F: rejestr 13 env-hooków przy kodzie + usunięte 2 martwe rusztowania (DIRTY_AT_FRAME, TEST_RESET_MODAL + pola). G: README §4 protokół STOP-gate (4 sytuacje = MUSISZ stanąć) + reguła anty-dryf doc↔kod + `tools/doc_drift_check.ps1` (tripwire 4 termów).
- CZEMU:  domknięcie planu porządków (etapy D–G); anty-rozjazd testu + zamknięcie klasy persystencji + zaszycie STOP-gate/anty-dryf jako PROCESU (nie intencji — to był rdzeń porażki poprzedniego wykonawcy).
- EFEKT:  cały PLAN PORZĄDKÓW A–G WYKONANY. Bramka `gate.ps1` zielona po każdym etapie (build 3/3, walidator 18 sond OK, test PASS, smoke 0). doc_drift_check zielony (4/4 termy zgodne). invertSteering round-trip potwierdzony (destruktor pisze klucz). Env-hooki: 15→13, martwe usunięte.
- DALEJ:  fundament gotowy pod dalszą pracę. Następny WIELKI ETAP (zapowiedź Jozza): refactoring ciężkich spraw (podział plików ~2000 linii: validation 2700, rig_lab ~2000, visual_mesh ~2000) — teraz bezpieczny dzięki `gate.ps1`. Potem edytor rigu. Oba czekają na sygnał Jozza.

## 2026-07-09 · Porządki A+B+C: higiena, skrypt bramki, wspólna lista CMake · a989459, e19e8db, ea1d1b7
- CO:     A: README §2↔kod (label [ARCADE], model tarcia P4b, reguła 2 semantyk load presetu), baner DONE na PLAN_STABILNOSC, usunięte 30 archeologicznych tasków. B: `tools/gate.ps1` — cała bramka jednym poleceniem (build 3 + walidator + test + smoke, 1 linia PASS/FAIL, exit≠0 z pierwszym błędem). C: `set(JOZZ_VEHICLE_CORE_FILES)` — koniec zduplikowanej listy źródeł Jozza w CMake (samples + validation z jednej zmiennej).
- CZEMU:  plan porządków fundamentu, etapy A–C (anty-rozjazd + przyspieszenie). Klasa A2 (doc↔kod) już się mnożyła; lista źródeł CMake była pułapką „dodasz plik, zapomnisz targetu".
- EFEKT:  gate.ps1 zweryfikowany OBUSTRONNIE: zielony end-to-end + wstrzyknięty #error → czerwony na „build (samples)" z dokładną linią i exit 1. CMake: zbiory źródeł PROWADZALNIE identyczne (union core+GUI = te same 21 plików), oba targety budują się, walidator OK, test PASS, smoke 0 err. README §3 + PLAN §9 wskazują na skrypt.
- DALEJ:  etapy D (rejestr sond walidatora), E (mapa persystencji + #12), F (rejestr env), G (STOP-gate jako proces). Każdy = 1 sesja + `gate.ps1` + checkpoint.

## 2026-07-09 · Plan porządków fundamentu (walidacja + rozbudowa) · docs
- CO:     `PLAN_PORZADKI_FUNDAMENT_2026_07_09_PL.md` — krytyczna walidacja dotychczasowego (nie)planu porządków + 7 etapów (A higiena, B skrypt bramki, C wspólna lista CMake, D rejestr sond, E mapa persystencji, F rejestr env, G STOP-gate jako proces). Zero zmian kodu — plan.
- CZEMU:  Jozz: przygotować projekt POWAŻNIE pod dalszą rozbudowę (i przyszły edytor rigu, którego NIE projektujemy teraz); najpierw zwalidować i rozbudować plan.
- EFEKT:  znaleziska z POMIARU (dowody): CMake duplikuje listę źródeł Jozza 2× (B1), validation.cpp 2691 linii z ręczną listą ~17 sond (B2), README już dryfuje vs kod — nazwa suwaka arcade (B3, klasa A2 już się mnoży), klasa persystencji niezmapowana (B6). Wspólny mianownik: ręcznie utrzymywane duplikaty prawdy → cel: jedno źródło albo maszynowy strażnik. Świadomie NIE ruszamy: podział plików ~2000 linii, edytor, fizyka.
- DALEJ:  **STOP — czekam na akceptację planu przez Jozza.** Rekomendacja startu: Etap A (higiena, near-zero ryzyko) → B (skrypt bramki, przyspiesza resztę). Każdy etap = 1 sesja + bramka + checkpoint.

## 2026-07-09 · Gate 2 + fix presetu ZAAKCEPTOWANE przez Jozza · —
- CO:     Jozz potwierdził ręcznym testem: ściąganie znacząco mniejsze (resztkowe delikatne — zaakceptowane, TECH_DEBT #11), stabilnie na lądowaniach, powrót kierownicy „serio odbija w drugą stronę", preset driftowy lżejszy; fix determinizmu presetu potwierdzony („błąd załatany").
- CZEMU:  dwufazowe zamknięcie — wykonawca nie ocenia sam siebie (zasada z post-mortem).
- EFEKT:  Bramki 1–2 + fix presetu odebrane. Task #48 zamknięty.
- DALEJ:  porządki fundamentu (plan wyżej).

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
