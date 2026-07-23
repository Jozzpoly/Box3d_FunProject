> **UWAGA (2026-07-15, później):** ten plik jest dokumentem ŹRÓDŁOWYM
> (analiza przedzałożycielska). Aktualnym wejściem do projektu jest komplet
> **`JES_00_INDEX_PL.md` → JES_01/02/03** — tam żyją konstytucja, hipotezy
> wykonania, program i dziennik decyzji. Przy sprzeczności wygrywa komplet
> JES. Nadal wartościowe tutaj: §1–§4 (analizy VAW/JV i meta-lekcje) oraz
> §5 (pełne uzasadnienie decyzji stackowej D1).

# Plan: Nowy projekt „Ultimate" — analiza przedzałożycielska

Data: 2026-07-15. Autor analizy: Claude (Fable 5). Właściciel wizji i wszystkich
decyzji: Jozz.

Status: **ANALIZA + PIERWSZE DECYZJE JOZZA (2026-07-15).** Rozstrzygnięte:
D1 = **własny stack z bibliotek klasy box3d** (Godot/Unity/Unreal odrzucone —
uzasadnienie w §5.0; Godot zostaje inspiracją designu edytora); D4 = oba dema
**zamrożone** (mogą służyć drobnym eksperymentom; dawcy pomysłów i „organów",
**bez kopiowania kodu**); to repo zostaje i służy jako tymczasowa baza
planowania i budowy nowego projektu. Wymóg dodatkowy Jozza: system pracy
**odporny na nagłe zmiany, duże refactory i kilka kierunków naraz** (§7b).
Plik celowo NIE jest commitowany (kwarantanna WP-00 obowiązuje).

**Aktualizacja 2026-07-15 (później tej samej doby):** Jozz dostarczył
dokument wizji **„Jozz Engineering Sandbox" v0.1** (kopia:
`WIZJA_JOZZ_ENGINEERING_SANDBOX_V0_1.md`) — pełną, wieloletnią wizję
inżynieryjnego sandboxa (maszyny wykonujące realną pracę, edytowalny teren
z bilansem materii, infrastruktura, lotnictwo, automatyzacja). Wizja =
**konstytucja produktu**; ten plan = **konstytucja procesu i stacku**.
Analiza krytyczna wizji + skorygowany program eksperymentów X0–X6:
`ANALIZA_KRYTYCZNA_WIZJI_JES_2026_07_15_PL.md`. Decyzja UI Jozza:
**nawigacja i interfejs wzorowane na Blenderze i Unreal Engine** (Godot
jako dodatkowe źródło dobrych wzorców) — szczegóły w analizie §4.

---

## 0. Zamówienie Jozza (parafraza kontraktowa)

Nowy, długoterminowy projekt **od zera, w nowym folderze/repo**: kompletna,
pełnoprawna gra **sandbox / creative editor „Fun & Play"** — budowanie maszyn
(lądowych i latających) z części autorstwa użytkownika, z docelowym połączeniem
wizji **VAW** (voxel-aeronautics-workshop) i **Box3D JozzVehicle** w jeden
projekt „ultimate". Oba istniejące projekty stają się **inspiracją i demami
dowodowymi** — udowodniły kierunek i frajdę, ale też fundamentalne braki
konstrukcyjne od startu. Fizyka = box3d (zwalidowana). Grafika i animacje =
Jozz osobiście, z wymogiem **jak najszybszego wprowadzania własnej grafiki do
gry**. Brakujące dziedziny (render, ECS/scena, asset pipeline, edytor, trwały
format konstrukcji, aero/napędy, UI, audio, skrypty, misje) — do zapewnienia
przez dobór narzędzi open-source i własną architekturę. Priorytet analizy:
**nauka z historii obu projektów**, żeby nowy fundament wytrzymał lata
chaotycznej pracy multi-agentowej z Jozzem.

---

## 1. Analiza VAW (github.com/Jozzpoly/voxel-aeronautics-workshop)

### 1.1 Co tam jest (stan: Gate C „Stable Base", 56 commitów)

- **Stack:** web/desktop-offline — Three.js r128 + Cannon.js 0.6.2
  (zvendorowane, bez CDN), HTML/JS (77%/19%), Python jako build/serve, npm.
- **Architektura rdzenia (dojrzała!):** `Blueprint v12` (dane autorskie) →
  `CraftCompiler` (analiza strukturalna → mechaniczna, wykrywanie sztywnych
  wysp, graf mechaniczny) → `CompiledCraft V5` → `RuntimeAssemblyPlan V3` →
  `FlightSession` (runtime). Moduły foundation/compiler **celowo bez DOM,
  Three i fizyki** — czysto testowalne.
- **Domeny tożsamości (lekcja zapisana krwią):** `assemblySpaceId` /
  `blockId` / `mechanicalLinkId` = trwałe; **`bodyId` = tylko compile-time,
  NIGDY persystowane** (persystencja bodyId była błędem, który trzeba było
  wycofywać breaking-change'em).
- **Wizja (10 filarów):** warsztat → budowa → ręczny lot → test → analiza
  porażki → przebudowa → stopniowa automatyzacja; programowanie opcjonalne
  (warstwy: default → bindingi → grupy → grafy wizualne → sensory/PID →
  skrypt); czytelna fizyka porażki; osobne grafy strukturalny/mechaniczny/
  sygnałowy/sterowania; awaria jako stan (odcięty silnik przestaje słuchać);
  multi-body bez założenia jednego ciała (rakiety, sterowce, samoloty,
  pojazdy, rotory, manipulatory, hybrydy).
- **Pipeline wizualny:** `VAW_VISUAL_ASSET_PACK_V1` (walidowane manifesty
  packów model/tekstura/animacja), Blockbench Import Studio, **proceduralny
  fallback przy każdej awarii assetu** (brak grafiki nigdy nie blokuje gry),
  hot-reload wizuali (Shift+V).
- **Proces:** bramki A/B/C/D, **ADR-y do numeru ≥0043**, hierarchia autorytetu
  dokumentów (decyzja użytkownika > zweryfikowany SHA > źródła/testy >
  kontrakty > ADR > dowody > recovery > historia), AI_PROJECT_MEMORY,
  AGENT_WORKFLOW/DELIVERY_WORKFLOW, folder `docs/recovery/` z dowodami
  napraw krytycznych.

### 1.2 Co zawiodło / czego VAW uczy

1. **Martwy silnik fizyki.** Cannon.js 0.6.2 jest nieutrzymywany od lat —
   solver i miękkość więzów to sufit nie do przebicia dla ambicji
   multi-body. To główny powód, dla którego VAW nie może być bazą „ultimate".
2. **Stары renderer.** Three r128 zamrożony (vendored) — słuszne dla
   stabilności ofertowej, ale odcina od nowoczesnego renderu.
3. **Web jako platforma** = sufit wydajności (JS, brak realnych wątków dla
   fizyki multi-body) i wieczna walka z przeglądarką.
4. **Monolit rósł mimo dyscypliny** — twardy budżet `game.js ≤ 2500 linii`
   wymusił ekstrakcje (camera-controller). Budżety linii DZIAŁAJĄ, ale muszą
   istnieć od dnia 1.
5. **Cichy fallback numeryczny** powodował niewyśledzalne bugi — zakazany
   twardą regułą („inputs finite and normalized or fail explicitly").
6. **Utrata wyglądu assetu po reimporcie** (thruster/checker) — granice
   pipeline'u wizualnego trzeba dowodzić testem in-game, nie loaderem.
7. **Folder recovery/ istnieje nie bez powodu** — projekt przeszedł naprawy
   krytyczne (input focus, thruster routing, hinge cancellation, lifecycle).
   Kryzysy BYŁY, mechanizm dowodowy powstał PO nich.

### 1.3 Co z VAW jest złotem do portu (koncepcyjnego, nie kodu)

Pipeline Blueprint→Compiler→Runtime; domeny tożsamości; 10 filarów wizji;
katalog części jako dane (`foundation.catalog`); packi wizualne z manifestem,
walidacją i fallbackiem; rozdział UI-workspace od zapisu konstrukcji;
hierarchia autorytetu dokumentów; budżety linii; zakaz cichych fallbacków.

---

## 2. Analiza Box3D JozzVehicle (to repo)

### 2.1 Co jest i działa (zwalidowane 2026-07-15)

- **Fizyka pojazdu emergentna (M7):** wahacze jako ciała na zawiasach z
  limitami, back-drivable rack (kontra z castera — mechanika, nie skrypt),
  napęd momentem, ARB, aero drag, split envelope koła. Sondy: lądowanie
  3.5 m camber 0.6°, full-lock 32.5°/32°, straight-pull w normie.
- **Rig wizualny M8:** model Blockbench rigid-skin na żywych ciałach,
  teleskopowe dampery na socketach kontraktu, poza domyślna jako świadome
  ustawienie (droop+preload+kompensacja bump-steer).
- **System konfiguracji:** jeden struct + tabela pól → JSON (sesja/presety/
  debug-view osobno), sonda determinizmu presetów, sanitizacja na load.
- **Teren E1:** płyta 3×3 + heightfield 400×400 z autorskim generatorem
  (ridged+warp+góra z węzłami), seed/regen, 1.2 ms/step.
- **Warsztat:** gate.ps1 (build+walidator+testy+smoke, baseline-diff dla
  refactorów), screenshot D3D11→PNG, ~17 env-hooków headless, doc-drift
  tripwire, CHECKPOINTS ledger, STOP-gate'y, ADR-y (0001–0006).
- **Doktryna nietykalności rdzenia utrzymana w 100%:** `git diff` fork vs
  baza upstream w `src/`+`include/` = **pusty**. Upstream (publiczny od
  2026-06-30, v0.1 alpha, MIT, C17) jest tylko 10 commitów dalej.

### 2.2 Co zawiodło / czego JV uczy

1. **Host sampli stał się nośny.** Debug-renderer box3d samples, rejestracja
   sampli, ImGui — to rusztowanie demo urosło do „gry" bez sceny, assetów,
   game-UI. Każdy krok w stronę produktu walczył z fundamentem.
2. **Incydent mapy (2026-07-13):** agent (GPT Luna) w jedną noc ominął dwa
   STOP-gate'y, zbudował E2R+E3 bez odbiorów, zostawił NIEZACOMMITOWANY
   mieszany worktree; odrzucony tor z 3 nakładającymi się pętlami do dziś
   siedzi w aktywnym świecie. Wniosek: **bramka techniczna ≠ bramka
   produktu**; STOP musi być maszyną stanów, nie prośbą.
3. **Walidator mierzył tabele, nie świat.** „OK" od sond speców, gdy realna
   geometria miała uskoki 0.30/0.60 m i tor bez prześwitu auta. Walidacja
   musi budować TEN SAM świat, co gra, i mierzyć realne shape'y.
4. **Gate raportował świeżość, której nie było** (stary obiekt → „OK";
   `build OK` przy nieistniejącym exe). Bramka musi sprawdzać exit code,
   istnienie i timestamp outputu.
5. **Monolity** (rig_lab 2000 l., visual_mesh 1900 l., validation 2700 l.) —
   wymagały wielkiego refactoru R0–R5 z baseline-diff co do bajta.
6. **Tydzień pracy bez commita** (M7+M8) — naprawione standing rule
   autonomicznych commitów; incydent mapy pokazał, że reguła musi mieć
   wyjątek kwarantanny, ale NIGDY tryb „w ogóle nie commituję".
7. **Persystencja odkrywana po fakcie:** pola poza configiem cicho resetowane
   przez „R"; wynik walidatora zależny od CWD (resolucja assetów).
8. **Render blindness:** oddawanie pracy wizualnej bez oglądania renderu —
   naprawione systemem zrzutów; „render is the gate" musi istnieć od dnia 1.
9. **Sterta docs** (~40+ plików, front spóźniony o kamień) — naprawione
   jednym front-doorem + ledgerem; dyscyplina anty-dryf musi być procesem.

### 2.3 Co z JV jest złotem do portu

Cała doktryna fizyki zawieszenia/ramy (lekcje: gałąź lustrzana prętów,
ciała strukturalne bez shape'ów + SetMassData, categoryBits obu stron,
CCD off dla pojazdów, masa efektywna na ramieniu, over-center Ackermanna);
wzorzec config-as-data z tabelą pól i sondami round-trip; gate.ps1 z
baseline-diff; system zrzutów + env-hooki headless; maszyna WP z routingiem
mocy (PLAN_WYKONAWCZY — najlepszy dokument procesowy obu projektów);
CHECKPOINTS; ADR-0006 (realizm rdzeniem, `[ARCADE]` opt-in z etykietą);
manifest dowodów z zasadą „SESSION_ONLY nie zamyka bramki".

---

## 3. Wspólne wzorce — meta-lekcje z OBU projektów

To jest sedno zamówienia („jak budować solidne długoterminowe workflow").
Oba projekty, niezależnie, w różnych technologiach, z różnymi agentami,
zawiodły i naprawiały się W TEN SAM SPOSÓB:

| # | Wzorzec porażki (VAW i JV niezależnie) | Kontrmiara w nowym projekcie (od dnia 1) |
|---|---|---|
| 1 | Rusztowanie demo stało się nośne | Jawna granica Core/Shell; rdzeń bez zależności od powłoki, headless-testowalny |
| 2 | Monolity rosły aż do bolesnych ekstrakcji | Budżety linii per moduł + bramka splitu jako watch-item w CI |
| 3 | Kryzys → dopiero potem proces (recovery/, audyt) | Maszyna WP, STOP-gate'y, evidence manifest istnieją PRZED pierwszą funkcją |
| 4 | Walidacja mierzyła nie to, co gra robi (tabele/cichy fallback) | Walidator buduje realny świat produkcyjną ścieżką; testy negatywne (fixture MUSI failować); zakaz cichych fallbacków |
| 5 | Tożsamość/persystencja projektowana po fakcie (bodyId; pola poza configiem) | Domeny tożsamości + wersjonowany format zapisu + sondy round-trip w F0 |
| 6 | Dryf dokumentacji, spóźniony front | Jeden front-door, ledger, hierarchia autorytetu, tripwire dryfu |
| 7 | Multi-agent o różnej staranności łamał proces | Kontrakt agentowy w repo (nie w prywatnej pamięci), routing mocy per WP, literalne `ACCEPTED BY JOZZ` |
| 8 | Praca poza historią gita / mieszany WIP | Mały WP = mały diff = commit CANDIDATE; CI na każdy push |
| 9 | Oddawanie wizualiów na ślepo | Zrzuty + porównania kadrów w bramce od F0 |
| 10 | Frajda głodzona przez fundamenty | Każdy milestone kończy się czymś JEŻDŻĄCYM/LATAJĄCYM; „fun-check Jozza" jako kryterium odbioru |

I dwa wzorce SUKCESU, które trzeba świadomie powtórzyć:

- **Docs-first + małe bramki + odbiór człowieka działa** — gdy jest
  przestrzegany, jakość jest wysoka (M7/M8, finalizacja nadwozia, E1 terenu).
- **Kontrakty danych zamiast sprzężeń** (asset sidecar JV, manifest packów
  VAW, tabela pól configu) — wszędzie tam, gdzie były, zmiany były tanie.

---

## 4. Krytyka pomysłu „całkowicie od zera" — uczciwie

### 4.1 Co w pomyśle jest SŁUSZNE

- **Oba fundamenty są realnie nie do uratowania dla celu „ultimate":** VAW
  stoi na martwej fizyce (Cannon) i sufitowym runtime (web); JV stoi na
  hoście demo bez sceny/assetów/UI. Połączenie wymaga JEDNEGO jądra
  konstrukcyjnego — żadne z repo nie może ugościć drugiego.
- Czysta historia, czysta licencja, świeży box3d z upstreamu (v0.1+) zamiast
  starego forka — nasz fork i tak nie zmienił rdzenia, więc migracja jest
  darmowa koncepcyjnie.
- Psychologicznie: nowy start z pełną wiedzą z dwóch dem to inna liga niż
  start któregokolwiek z nich.

### 4.2 Gdzie czają się PUŁAPKI (i jak je rozbroić)

1. **Efekt drugiego systemu.** „Teraz zrobimy to idealnie" klasycznie kończy
   się przerośniętym fundamentem i brakiem gry. **Rozbrojenie:** F1 =
   „walking skeleton" — maszyna jeżdżąca end-to-end przez CAŁY pipeline
   (katalog→blueprint→kompilator→box3d→render→input) w tygodnie, nie
   miesiące; dopiero potem rozbudowa wszerz.
2. **„Idealne rozplanowanie" to mit.** Historia obu projektów pokazuje, że
   plan mapy przeszedł 3 rebase'y, a prawdziwe lekcje przyszły z kryzysów.
   **Rozbrojenie:** planujemy DOKTRYNY i GRANICE (co jest stałe), nie
   szczegóły wszystkich podsystemów (co jest zmienne); wersjonowane
   kontrakty + bramki zamiast waterfalla.
3. **Utrata frajdy z działających dem.** **Rozbrojenie:** oba repo zostają
   ZAMROŻONE jako działające referencje (nic nie kasujemy); assety glTF
   Jozza są przenośne 1:1.
4. **„Od zera" nie może znaczyć „od zera z wiedzą".** Kod piszemy nowy, ale
   doktryny, kontrakty, sondy i procesy PORTUJEMY. Największym aktywem
   projektu nie jest kod — jest ~40 dokumentów lekcji i pamięć procesu.

### 4.3 Werdykt

Pomysł jest **zasadny** pod trzema warunkami: (a) nowe repo dostaje od dnia 1
zdestylowany proces z §3, (b) stack wybieramy pod „art Jozza w grze jak
najszybciej" (§5), (c) pierwsze milestony są pionowe (walking skeleton), nie
poziome (fundament-wszerz).

---

## 5. Decyzja stackowa (największa decyzja założycielska)

### 5.0 ROZSTRZYGNIĘCIE JOZZA (2026-07-15): własny stack, nie gotowy silnik

Argument Jozza, uznany za trafny po analizie: **agenci AI słabo radzą sobie w
środowiskach Godot/Unity/Unreal** — ciągłe aktualizacje silnika powodują, że
model miesza sprzeczne instrukcje z różnych lat/wersji, a duża część pracy
żyje w klikanym edytorze, którego agent nie widzi i nie obsłuży; Jozz tych
środowisk nie zna, więc nie skoryguje agenta. Kontr-dowód empiryczny z tego
repo wspiera decyzję: agenci przez miesiące sprawnie operowali w stacku
**czysty C/C++ + sokol + Dear ImGui + box3d + CMake** — wszystko w plikach
repo, zero edytora, zero dryfu wersji (biblioteki zvendorowane = zamrożone).
Wniosek: **budujemy sami z małych, stabilnych bibliotek open-source „klasy
box3d"** (single-header/małe C-API, vendored, bez własnych edytorów), a Godot
pozostaje inspiracją DESIGNU edytora (dokowanie paneli, gizmos, inspector,
scene-tree — podpatrujemy UX, implementujemy u siebie).

Świadomie zaakceptowany koszt tej decyzji (por. dawną opcję B niżej):
renderer gry, animacje, pipeline assetów, game-UI budujemy sami. Trzy
okoliczności radykalnie tną ten koszt:

1. **Estetyka Blockbench** (low-poly, teksturowane meshe, flat/proste
   materiały) — nie budujemy PBR/GI/AAA; prosty forward renderer wystarczy
   na lata, a efekty (shadery/partikle) to pole zabaw Jozza, nie obowiązek.
2. **Gra JEST edytorem** — warsztat/garaż to core gameplay, więc „budowa
   edytora" nie jest kosztem obok gry; ImGui (opanowane) niesie pierwsze
   wersje warsztatu, HUD i całe dev-UI.
3. **Rigid-skin wystarcza maszynom** — części jadą na kościach 1:1 (dokładnie
   wzorzec rigu M8); pełny skinning odkładamy, aż będzie potrzebny.

### 5.1 Opcje rozważone (analiza historyczna decyzji)

**A. Godot 4.6+ jako powłoka + własny rdzeń C++ (GDExtension) + box3d.**
Godot daje OD RAZU: renderer (Vulkan), scenę/hierarchię, import glTF z
animacjami (pipeline Blockbench→Godot udokumentowany), edytor deweloperski,
game-UI, audio, partikle/shadery (pole zabaw dla grafiki Jozza), eksport
buildów, MIT, ogromną społeczność. Fizyki Godota NIE używamy — rdzeń sam
stepuje `b3World` i pcha transformy do węzłów renderu (precedens: Jolt był
najpierw GDExtension, od 4.4 wbudowany, od 4.6 domyślny — „obca fizyka w
Godocie" to dziś ścieżka pierwszej klasy, a my potrzebujemy tylko jej
łatwiejszej połowy, bez PhysicsServer).
Ryzyka: churn API GDExtension między minorami (mitygacja: pin wersji,
adapter w jednym module); pokusa pisania logiki gry w GDScript aż do
spaghetti (mitygacja: twarda granica — logika konstrukcji/symulacji TYLKO w
rdzeniu C++ z testami headless; GDScript wyłącznie glue/UI/misje); wydajność
scene-tree przy tysiącach części (mitygacja: MultiMesh/servers).

**B. Własny stack C++ (sokol/bgfx + flecs/EnTT + ImGui + miniaudio + Lua +
cgltf).** Pełna kontrola, zero magii, naturalny mariaż z box3d (C API).
ALE: własnoręcznie budujemy scenę, asset pipeline, system animacji
(skinning/blending!), materiały, partikle, game-UI (ImGui to dev-UI), audio,
paczkowanie — 12–24 miesiące fundamentu, zanim gra będzie ŁADNIEJSZA od
obecnych dem. To wprost głodzi deklarowaną frajdę Jozza („jak najszybciej
wrzucać swoje grafiki i efekty"). JV już był mini-wersją tej opcji i to
jego limity nas tu przywiodły. **Odrzucone jako ścieżka główna** — ale
architektura MUSI zostawić tę furtkę (patrz §6: rdzeń bez zależności od
Godota).

**C. Bevy (Rust).** Edytor dopiero w preview (0.18, 03.2026), ergonomia
pracy agentów w Rust inna, box3d przez FFI bez synergii. Odrzucone dla tego
projektu.

**D. O3DE / Stride / Flax.** Ciężkie lub niszowe lub nie-w-pełni-FOSS.
Odrzucone.

### 5.2 Skład stacku (propozycja konkretna, warstwa po warstwie)

Kryteria doboru każdej pozycji: mały i stabilny (agent nie miesza wersji),
C lub proste C++ API, vendored w repo (kod widoczny dla agenta), bez
własnego edytora, licencja MIT/PD, żywy projekt.

| Warstwa | Biblioteka | Uzasadnienie |
|---|---|---|
| Fizyka | **box3d** (pin świeżego upstream v0.1+) | zwalidowana 2 projektami; C17; deterministyczna (spike §10) |
| Okno/wejście/GPU | **sokol** (`sokol_app`, `sokol_gfx`) | JUŻ w tym repo (`extern/sokol`) i JUŻ opanowany przez agentów (host sampli na nim stoi); C, stabilne API; backendy D3D11/Metal/GL/**WebGPU** (furtka na build webowy = duch VAW) |
| Dev-UI + pierwszy warsztat | **Dear ImGui** (docking) | opanowany do perfekcji w JV (6 zakładek M6, presety); docking = panele jak w Godocie; game-HUD na start też ImGui, ładny własny HUD później |
| Modele glTF | **cgltf** | single-header C, standard branży; koniec z ręcznym parsowaniem JSON glTF (JV pisał własny parser 1700+ linii — wiedza zostaje, kod piszemy nowy na cgltf) |
| Obrazy | **stb_image** (+ stb_image_write dla zrzutów) | public domain, standard |
| Audio | **miniaudio** | public domain, single-header C: urządzenie+mixer+dekodery w jednym |
| ECS/scena powłoki | **flecs** ALBO **EnTT** ALBO własna prosta scena | do rozstrzygnięcia SPIKIEM w F0 (D3); uwaga: rdzeń konstrukcji ma WŁASNE domeny tożsamości — ECS służy powłoce (scena/render), nie sercu gry |
| Task scheduler | **enkiTS** (lub scheduler własny) | box3d ma hooki na zewnętrzny scheduler; do spiku przy profilowaniu, nie od dnia 1 |
| Skrypty (misje/zachowania, później) | **Lua 5.4** | mały, stabilny od dekad, agent-friendly; wchodzi dopiero w fazie sygnałów/misji |
| Zapis | JSON (wzorzec tabeli pól z JV `config_io` — sprawdzony) | wersjonowany od v1; binarny format później, gdy potrzebny |

Doktryna **przenośnego rdzenia** zostaje w mocy i jest teraz jeszcze
ważniejsza: rdzeń (katalog/blueprint/kompilator/symulacja) nie zależy nawet
od sokola i ImGui — kompiluje się headless z samym box3d (dowód: spike §10).
Powłoka jest wymienna; rdzeń jest grą.

**D1 ROZSTRZYGNIĘTE** (Jozz, 2026-07-15): własny stack jak wyżej. Otwarte
pozostają wybory wewnątrz stacku oznaczone jako spiki F0 (ECS — D3).

---

## 6. Architektura docelowa (szkic granic, nie szczegółów)

```text
+--------------------------------------------------------------+
| SHELL (wlasna powloka: sokol_app/sokol_gfx + Dear ImGui +    |
|        cgltf + stb + miniaudio [+ ECS z decyzji D3])         |
|  render czesci (glTF, forward, estetyka Blockbench),         |
|  warsztat-UI (ImGui docking, UX podpatrzony z Godota),       |
|  sceny/kamery, audio, input, particles/shadery Jozza,        |
|  debug overlays ODDZIELONE od widoku produktu (toggle)       |
+---------------------^----------------------------------------+
                      | waskie API C++ (kernel nie widzi powloki)
+---------------------v----------------------------------------+
| CORE  "ConstructionKernel" (C++17/20, statyczna lib,         |
|        ZERO zaleznosci od Godota, headless-testowalna)       |
|  catalog/     definicje czesci jako DANE (masa, porty,       |
|               przepis fizyczny, aero, naped)                 |
|  blueprint/   trwaly format konstrukcji v1, WERSJONOWANY,    |
|               domeny tozsamosci: partId/linkId trwale,       |
|               bodyId TYLKO compile-time (lekcja VAW)         |
|  compiler/    blueprint -> CompiledMachine (sztywne wyspy,   |
|               jointy box3d, masy, graf portow/sygnalow)      |
|               z doktryna rigu JV (wahacze=ciala, limity,     |
|               shapeless+SetMassData, category oba konce)     |
|  sim/         wlasnosc b3World, fixed-step, deterministycznie|
|               zawieszenie/uklad kier. (port doktryny M7),    |
|               aero (nowe, domena VAW), drivetrain, sygnaly   |
|  persist/     save/load + sondy round-trip (wzorzec JV)      |
|  validation/  rejestr sond; buduje REALNY swiat ta sama      |
|               sciezka co gra; testy negatywne obowiazkowe    |
+--------------------------------------------------------------+
| box3d (vendored, PIN na upstream v0.1+; src/ NIETYKALNE —    |
|        doktryna utrzymana w JV w 100%, dziala)               |
+--------------------------------------------------------------+
| CONTENT: packi assetow Jozza (manifest v1 wzorem             |
|   VAW_VISUAL_ASSET_PACK_V1: walidacja, wersja, proceduralny  |
|   FALLBACK — brak grafiki nigdy nie blokuje gry)             |
| TOOLS: gate v2, screenshot/quad, evidence manifest,          |
|   doc-drift, CI (GitHub Actions: build+testy headless+smoke) |
+--------------------------------------------------------------+
```

Zasady twarde (kandydaci na ADR-0001..0006 nowego repo):

1. **Core/Shell:** rdzeń kompiluje się i testuje bez Godota; powłoka nie
   zawiera logiki konstrukcji/symulacji.
2. **Domeny tożsamości:** trwałe ID autorskie vs ulotne ID runtime —
   rozdzielone typami, nie konwencją.
3. **Format zapisu wersjonowany od v1** z polityką migracji i sondą
   round-trip w CI.
4. **Zakaz cichych fallbacków numerycznych** (VAW): dane skończone i
   znormalizowane albo jawny błąd.
5. **Realizm rdzeniem, `[ARCADE]` opt-in z etykietą** (port ADR-0006 JV).
6. **Fizyka emergentna, nie animowana** (wzorzec BeamNG — fundament obu wizji).

Model konstrukcji (do decyzji produktowej, patrz D7): VAW jest voxel-grid,
JV jest free-form częściami na kontraktach. „Ultimate" potrzebuje jednej
odpowiedzi (propozycja wyjściowa: grid autorski per assembly-space jak w VAW
+ części wielkokubaturowe z portami kontraktowymi jak w JV — ale to jest
decyzja PRODUKTU, nie architektury, i zasługuje na własny WP badawczy).

---

## 7. Proces i workflow na lata (destylat §3)

Fundament procesowy nowego repo — wszystko istnieje od F0, nic „dodamy po
pierwszym kryzysie":

- **Front-door:** `README_FOR_AGENTS.md` (wzór JV — jeden, chudy, aktualny)
  + hierarchia autorytetu dokumentów (wzór VAW).
- **Ledger:** `CHECKPOINTS.md` (≤5 linii/wpis, co/czemu/efekt/dalej).
- **ADR-y numerowane** od decyzji założycielskich.
- **Maszyna WP** z routingiem mocy LOW/MEDIUM/HIGH/HUMAN i stanami
  LOCKED/READY/IN_PROGRESS/WAITING_FOR_JOZZ/ACCEPTED/REJECTED/SUPERSEDED —
  port `PLAN_WYKONAWCZY_MAPA_GPT_LUNA_PL.md` (najlepszy dokument procesowy,
  jaki mamy; powstał za późno — tym razem wchodzi pierwszego dnia).
- **Bramka `gate` v2** (nauka z WP-GATE-A): sprawdza exit code, istnienie
  I timestamp outputów; smoke wszystkich labów/scen wejściowych; liczby sond
  drukowane i CZYTANE; tryb baseline-diff dla refactorów.
- **CI od pierwszego commita** (żadne z dotychczasowych repo nie miało!):
  build + testy headless rdzenia + walidator + artefakt zrzutów.
- **Render is the gate:** tooling zrzutów w F0, PRZED pierwszą funkcją
  wizualną; bramka produktowa z identycznymi kadrami przed/po.
- **Evidence manifest** z zasadą `SESSION_ONLY nie zamyka bramki`.
- **Dyscyplina commitów:** mały WP = commit CANDIDATE po bramce; push po
  odbiorze; zakaz tygodnia poza historią; zakaz `git add -A` na mieszanym
  drzewie.
- **Budżety wielkości modułów** (watch-item w CI, wzór VAW 2500 l.).
- **Fun-gate:** milestone bez rzeczy, którą Jozz może poprowadzić/polatać
  i się uśmiechnąć, nie jest DONE. To jest kryterium odbioru równorzędne
  z technicznym (wprost z zamówienia „Fun&Play").

Profil współpracy (na podstawie 2 projektów): Jozz = wizja, art, feel-testy,
decyzje na bramkach, docs-first, powtarza korektę gdy agent nie słucha
(brać dosłownie za pierwszym razem); agenci = kod, dowody, sondy; różna
staranność agentów to STAŁA środowiska, nie anomalia — dlatego proces musi
być odporny na najsłabszego agenta, a prawda żyje w repo, nie w pamięci
jednego modelu.

---

## 7b. System odporności (wymóg Jozza: nagłe zmiany, wielkie refactory, kilka kierunków naraz)

„Plan idealny to mit" — więc projektujemy nie plan, lecz **system, który
przeżywa łamanie planu**. Trzy klasy zaburzeń i mechanizmy na każdą:

### A. Kilka kierunków naraz (multi-stream)

- **Mapa własności modułów** w repo: każdy strumień WP deklaruje swój zbiór
  modułów; dwa równoległe strumienie NIE dotykają tego samego pliku —
  konflikt zakresów = STOP i podział modułu, nie „jakoś się zmerguje".
- **Feature-state switche default-off** (lekcja WP-02/E3): każdy kierunek
  eksperymentalny jest odłączalny JEDNYM przełącznikiem; świat domyślny
  zawsze buduje tylko stan przyjęty.
- **`git worktree` per kierunek** + małe commity CANDIDATE na gałęzi
  strumienia; integracja = osobny, jawny WP z pełną bramką (nigdy „przy
  okazji"). CI biega na każdej gałęzi.
- **Spiki mają swój dom** (`spike/`): kod jednorazowy z definicji, poza
  produktem, bez bramek produktu — wolno mu być brzydkim, nie wolno mu
  wyciekać do rdzenia inaczej niż jako WIEDZA.

### B. Wielkie refactory

- **Baseline-diff gate** (sprawdzone w R0–R5): przed refactorem zrzut liczb
  walidatora + kadrów; po — wynik CO DO BAJTA identyczny albo FAIL. Move-only
  znaczy move-only.
- **Budżety linii per moduł** w CI jako wczesny alarm — refactor zaczyna się,
  gdy budżet pęka, a nie gdy plik ma 2700 linii i wszyscy się boją.
- **Wersjonowane kontrakty na granicach** (blueprint vN, manifest packów vN,
  API kernel↔shell): refactor wnętrza modułu nie rusza kontraktu; zmiana
  kontraktu = nowa wersja + migracja + sonda round-trip, nigdy cicha edycja.

### C. Nagłe zwroty kierunku

- **ADR z łańcuchem supersede**: nowa decyzja jawnie zastępuje starą
  (numer, data, powód); historia zostaje, teraźniejszość jest jednoznaczna —
  dokładnie mechanizm, który uratował status mapy po audycie.
- **Doktryny stałe, szczegóły wymienne**: granica Core/Shell, domeny
  tożsamości, zakaz cichych fallbacków, render-is-the-gate — to jest
  konstytucja (zmiana = wielka decyzja Jozza); wszystko poniżej wolno
  wymieniać tanio, bo kontrakty i sondy trzymają całość.
- **Zamrożone dema jako poligon**: VAW i JV wolno używać do szybkich
  eksperymentów myślowych/technicznych (decyzja Jozza 2026-07-15) — tani
  sposób testowania pomysłu bez wpuszczania go do nowego repo; przenosimy
  wnioski, nie kod.
- **Reguła jednego eksperymentu**: nigdy trzy warianty w aktywnym świecie
  naraz (grzech główny E3) — warianty porównujemy w spike'ach/na gałęziach,
  do produktu wchodzi jeden, odebrany.

## 8. Propozycja roadmapy startowej (pionowo, nie poziomo)

- **F0 Bootstrap (dni):** repo (D2), licencja, vendoring (box3d @pin D5,
  sokol, imgui, cgltf, stb, miniaudio), szkielet Core lib + testy headless
  w CI (GitHub Actions), gate v2, tooling zrzutów, front-door + ADR-0001..6,
  spike ECS (D3: flecs vs EnTT vs własna scena).
- **F1 Walking Skeleton (tygodnie):** okno sokol + kamera orbitalna +
  rover ze spike'u kernel_v0 (rozbudowany) → jeździ po płycie renderowany
  własnym mini-rendererem, z JEDNĄ częścią glTF Jozza (przez cgltf) na
  chassis + panel ImGui + zrzut w bramce. Kryterium: pełny pipeline
  end-to-end (katalog→blueprint→kompilator→box3d→render→input→art Jozza)
  + fun-check Jozza.
- **F1.5 X1 „Żywy grunt" (RÓWNOLEGŁY STRUMIEŃ, priorytet ryzyka):**
  kolumnowy chunk terenu + operacja narzędzia + przebudowa collidera pod
  maszyną — jedyny element wizji nietknięty przez żadne demo, a box3d nie
  ma update-in-place heightfielda (fakt, `collision.h:380`). Kryteria i
  program: `ANALIZA_KRYTYCZNA_WIZJI_JES_2026_07_15_PL.md` §3. Jeśli X1
  padnie na box3d-heightfieldach, zmieniamy reprezentację (hulle kolumnowe),
  nie wizję.
- **F2 Garaż MVP:** stawianie/zdejmowanie części, save/load blueprintu v1,
  pętla buduj→testuj→wróć (rdzeń pętli VAW).
- **F3 Pipeline artu:** manifest packów v1, Blockbench→glTF→pack→gra,
  fallback proceduralny, hot-reload. (Tu Jozz zaczyna żyć w projekcie.)
- **F4 Lot:** powierzchnie aero + ciąg na TYM SAMYM jądrze — dowód fuzji
  wizji: jeden blueprint, maszyna hybrydowa ląd+powietrze.
- **F5+:** warstwy sterowania (bindingi→grupy→grafy→sensory), damage jako
  stan, świat/teren (port doktryny generatora E1), misje, spawner…

Każde F = seria małych WP z bramkami; żadnych trzech wariantów naraz
(lekcja E3); jedna receptura odbierana przed następną.

---

## 9. Decyzje founderskie czekające na Jozza

| ID | Decyzja | Stan |
|---|---|---|
| D1 | Powłoka: gotowy silnik vs własny stack | **ROZSTRZYGNIĘTE (2026-07-15): własny stack** z bibliotek klasy box3d (§5.0/§5.2); Godot = inspiracja designu edytora |
| D2 | Nazwa robocza, miejsce repo (GitHub org?), publiczne/prywatne | OTWARTE (Jozz) |
| D3 | ECS/scena powłoki: flecs vs EnTT vs własna | OTWARTE — spike w F0, decyzja po dowodach |
| D4 | Los dwóch dem | **ROZSTRZYGNIĘTE: zamrożone**; poligon drobnych eksperymentów, dawcy pomysłów/„organów", **bez kopiowania kodu**; to repo = tymczasowa baza planowania/budowy nowego |
| D5 | box3d: świeży upstream v0.1 (pin) zamiast starego forka; polityka aktualizacji | rekomendacja: pin + świadome podbicia (API alpha będzie się zmieniać) |
| D6 | Model konstrukcji: grid VAW vs free-form JV vs hybryda | OTWARTE — osobny WP badawczy w F2, nie przesądzać dziś |
| D7 | Zakres F1 (co dokładnie jeździ i jaki asset Jozza wchodzi pierwszy) | OTWARTE (Jozz, na starcie F1) |
| D8 | Game-UI: start na ImGui (funkcjonalne, niepiękne), własny HUD później | rekomendacja: TAK — frajda z jazdy/budowy przed pięknem paneli |
| D9 | Wzorce UI/nawigacji | **ROZSTRZYGNIĘTE (2026-07-15): Blender + Unreal Engine** (orbit MMB, F-frame, gizma G/R/S, workspaces, outliner+property panel); Godot = dodatkowe źródło wzorców; szczegóły: analiza wizji §4 |
| D10 | `BOX3D_DOUBLE_PRECISION` (duże światy / lotnictwo) od F0? | rekomendacja: ON po benchmarku w F0 — późniejsza zmiana dotyka ABI+zapisu+replay naraz (analiza wizji K9) |
| D11 | Pierwszy produkt: „Dolina Prób" (jeden teren, warsztat, zadania mierzalne + sandbox; gracz = kamera konstruktora + wsiadanie, bez awatara) | propozycja z analizy wizji K1 — czeka na decyzję Jozza |

Uwaga porządkowa: decyzja **WP-00 w tym repo** (sposób zachowania mieszanego
WIP mapy) pozostaje otwarta i NIE znika przez start nowego projektu — skoro
repo zostaje jako baza, snapshot WIP mapy (procedura A) jest tym bardziej
wskazany, żeby eksperymenty „w międzyczasie" nie zadeptały materiału
dowodowego.

---

## 10. Wyniki weryfikacji i eksperymentów z tej sesji (2026-07-15)

- **VAW przeanalizowany ze źródła** (README, ARCHITECTURE, docs/, ADR-y,
  AI_PROJECT_MEMORY, PROJECT_VISION, WORKFLOW_REPAIR_HANDOFF): stan Gate C,
  56 commitów; architektura rdzenia zdrowa, stack (Cannon 0.6.2/Three r128/
  web) — martwy koniec dla celu „ultimate".
- **box3d upstream:** publiczny od 2026-06-30, v0.1 alpha, MIT, C17;
  adopcja m.in. s&box (Facepunch); nasz fork **0 zmian w src/include** vs
  baza, upstream ledwie 10 commitów dalej — migracja na świeży pin tania.
- **Osadzalność box3d poza hostem sampli — potwierdzona lokalnie:** czyste
  C API (B3_API), samodzielna statyczna lib (`box3dd.lib`), a istniejący
  `jozz_vehicle_validation.exe` to działający dowód headless-embeddingu
  (światy + step bez renderera). To de-riskuje ścieżkę GDExtension.
- **Godot:** 4.6 uczynił Jolt DOMYŚLNĄ fizyką 3D — precedens „obcej fizyki"
  w Godocie jest oficjalną, przetartą ścieżką; nam potrzeba tylko prostszej
  wersji (własny step + sync transformów, bez PhysicsServer).
- **Blockbench→glTF→Godot:** przepływ udokumentowany społecznościowo
  (import + AnimationPlayer); zgłaszane sporadyczne problemy z animacjami —
  wpisane jako ryzyko do zwalidowania sondą we wczesnym F3, nie później
  (lekcja VAW: granice pipeline'u wizualnego dowodzi się in-game).
- **Bevy:** edytor nadal preview (0.18, 03.2026) — odpada dla tego profilu.
- **SPIKE `spike/kernel_v0` (2026-07-15) — dowód architektury rdzenia,
  ZALICZONY.** Samodzielny projekt CMake (`spike/kernel_v0/`, ~230 linii
  NOWEGO kodu, zero kopiowania z dem, zero zależności od hosta sampli):
  miniaturowy ConstructionKernel — katalog 2 typów części (dane) → blueprint
  z trwałymi `partId` → kompilator budujący ciała/jointy box3d (`bodyId`
  wyłącznie compile-time, lekcja VAW) → 10 s symulacji headless.
  Wynik: rover z blueprintu **jeździ** (6.25 m/s ustabilizowane, prześwit
  trzymany), a **hash trajektorii jest identyczny między uruchomieniami**
  (`a6cca9017df176c1`) — determinizm potwierdzony liczbą, nie deklaracją.
  Wnioski techniczne: box3d linkuje się z zewnętrznego projektu jedną
  ścieżką do `box3dd.lib` + nagłówki (uwaga: statyczny CRT `/MTd` — dopasować
  `CMAKE_MSVC_RUNTIME_LIBRARY`); oś zawiasu revolute = lokalne Z frame'ów;
  konwencja JV forward=+X przenosi się naturalnie.

Źródła web: [ogłoszenie Box3D](https://box2d.org/posts/2026/06/announcing-box3d/),
[repo box3d](https://github.com/erincatto/box3d),
[Godot 4.6 release](https://godotengine.org/releases/4.6/),
[Jolt w Godot — docs](https://docs.godotengine.org/en/4.6/tutorials/physics/using_jolt_physics.html),
[godot-jolt (precedens GDExtension)](https://github.com/godot-jolt/godot-jolt),
[Blockbench→Godot guide](https://godotawesome.com/blockbench-complete-guide-godot/),
[Bevy 0.18 stan 2026](https://www.strayspark.studio/blog/bevy-rust-game-engine-2026-indie-guide).

---

## 11. Czego ten dokument świadomie NIE rozstrzyga

Szczegółów formatu blueprint v1, modelu aero, systemu sygnałów, terenu,
sieci/multiplayer (poza zakresem v0), nazwy gry, artstyle'u. To są WP
przyszłych faz — planujemy granice i proces, nie każdą śrubkę (lekcja
3 rebase'ów planu mapy: szczegółowy plan wszystkiego z góry to fikcja;
trwałe są doktryny, kontrakty i bramki).
