> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# JES_02 — Propozycja wykonania

Warstwa: **HIPOTEZY WYKONANIA**. Wersja: 0.2 (2026-07-15).
Wszystko tutaj jest falsyfikowalne programem eksperymentów (JES_03).
Sprzeczność między tym dokumentem a wynikiem eksperymentu → wygrywa
eksperyment i aktualizacja tego dokumentu, nigdy naciąganie wyniku.

---

## 1. Kontekst: skąd ten projekt wie to, co wie

Dwa zamrożone projekty-dema udowodniły kierunek i dostarczyły lekcji:

- **VAW** (`github.com/Jozzpoly/voxel-aeronautics-workshop`): webowy sandbox
  latających maszyn (Three.js r128 + Cannon.js 0.6.2, vendored). Dojrzała
  architektura rdzenia: `Blueprint v12` (dane autorskie) → `CraftCompiler`
  → `CompiledCraft` → `FlightSession` (runtime); trwałe domeny tożsamości
  (`blockId`/`mechanicalLinkId` persystentne, `bodyId` TYLKO compile-time —
  persystowanie bodyId było błędem wycofywanym breaking-changem); packi
  wizualne z manifestem, walidacją i proceduralnym fallbackiem; osobne grafy
  (strukturalny/mechaniczny/sygnałowy). Powód zamrożenia: martwa fizyka
  (Cannon nieutrzymywany) i sufit platformy web.
- **JozzVehicle** (to repo, nakładka na box3d w `samples/jozz_vehicle_*`):
  natywny pojazd z emergentną fizyką klasy BeamNG — wahacze jako CIAŁA na
  zawiasach z limitami, back-drivable przekładnia kierownicza (kontra ze
  śladu castera, nie ze skryptu), napęd momentem, teren heightfield
  400×400 m z proceduralną górą, system presetów/persystencji, warsztat
  narzędziowy (bramka jakości, zrzuty ekranu, sondy headless). Powód
  zamrożenia: rusztowanie demo (host sampli box3d) nie uniesie produktu.

**Decyzja Jozza:** nowy projekt od zera; dema są dawcami pomysłów
i „organów" — **bez kopiowania kodu** (portujemy wiedzę, kod piszemy nowy).

## 2. Decyzje już podjęte (dziennik skrócony; pełny w JES_03 §8)

| Decyzja | Treść | Data |
|---|---|---|
| D1 | Własny stack z małych bibliotek open-source („klasa box3d"); NIE Godot/Unity/Unreal | 2026-07-15 |
| D4 | Dema zamrożone; poligon drobnych eksperymentów; zero kopiowania kodu | 2026-07-15 |
| D9 | UI/nawigacja wzorem **Blender + Unreal Engine**; Godot dodatkowym źródłem wzorców | 2026-07-15 |

Uzasadnienie D1 (ważne dla każdego agenta): agenci AI pracują źle
w środowiskach silnikowych z klikanym edytorem i szybkim dryfem wersji
(model miesza instrukcje z różnych lat; stan projektu żyje poza plikami).
Pracują świetnie w czystym kodzie z bibliotekami zvendorowanymi w repo
(wszystko widoczne w plikach, wersje zamrożone). Dowód empiryczny: wiele
miesięcy sprawnej pracy agentów w tym repo (C/C++, sokol, ImGui, box3d,
CMake). Jozz nie zna Godota/Unity/UE — nie skoryguje agenta, gdy ten
pobłądzi w edytorze.

## 3. Stack (hipoteza silna; każda pozycja vendored w repo)

| Warstwa | Biblioteka | Licencja | Uwagi |
|---|---|---|---|
| Fizyka | **box3d** | MIT | C17; publiczny od 2026-06-30, **v0.1 alpha — API będzie się zmieniać** (pin + świadome podbicia); adopcja m.in. s&box |
| Okno/wejście/GPU | **sokol** (`sokol_app`, `sokol_gfx`) | zlib | C, stabilne; backendy D3D11/Metal/GL/WebGPU; sprawdzony w tym repo |
| Dev-UI + warsztat v1 | **Dear ImGui** (docking) | MIT | panele/dokowanie; opanowany przez agentów do perfekcji |
| Modele | **cgltf** | MIT | glTF ze standardowego eksportu Blockbench (format artu Jozza) |
| Obrazy | **stb_image**(+write) | PD/MIT | tekstury + zapis zrzutów |
| Audio | **miniaudio** | PD/MIT | urządzenie+mixer+dekodery w jednym pliku |
| ECS/scena powłoki | flecs LUB EnTT LUB własna | MIT | **NIEROZSTRZYGNIĘTE (D3)** — spike w F0; rdzeń konstrukcji ma własne ID i NIE zależy od ECS |
| Zadania wielowątkowe | enkiTS lub własne | zlib | dopiero gdy profil pokaże potrzebę |
| Skrypty | Lua 5.4 | MIT | dopiero w fazie sygnałów/misji |
| Zapis | JSON, ręczny writer + tabela pól | — | wzorzec sprawdzony w JozzVehicle (`config_io`); wersjonowanie od v1 + sondy round-trip |

Grafika: estetyka Blockbench (low-poly, teksturowane meshe) → **prosty
forward renderer** wystarczy na lata; bez PBR/GI. Animacje: **rigid-skin**
(części na kościach 1:1 — wzorzec sprawdzony w JozzVehicle) wystarcza
maszynom; pełny skinning odłożony. Efekty/shadery/partikle = domena
twórcza Jozza, nie wymaganie fundamentu.

## 4. Architektura — warstwy i granice

⚠ **Poprawka po scaleniu z pakietem Sol (2026-07-15):** poniższy diagram to
**KSZTAŁT DOCELOWY (hipoteza kierunkowa)**, nie plan pierwszego commita.
Pierwszą zaimplementowaną strukturą danych jest **cienki, wersjonowany
`ExperimentAssemblySpec v0`** (2–3 części, stabilne authored ID, jeden
constraint/aktuator — dokładnie tyle, ile wymuszą realne canary z JES_03).
Pełny `MachineDocument`/CraftGraph może z niego WYROSNĄĆ wyłącznie przez
osobną bramkę promocji (≥2 realne fixture'y + problem nierozwiązywalny
cienkim specem + migration story). Projektowanie pełnego schematu świata
z wyobraźni = kolonizacja schematu, wprost zakazana (drugi-system effect).

```text
+----------------------------------------------------------------+
| SHELL (powloka): sokol + ImGui + cgltf + stb + miniaudio       |
|   viewport (nawigacja Blender/UE), warsztat, LAB HOST (par. 5),|
|   render czesci, audio, input, kamery, debug overlays          |
|   (oddzielone od widoku produktu, toggle, domyslnie OFF)       |
+------------------------------^---------------------------------+
                               | waskie API; kernel NIE widzi powloki
+------------------------------v---------------------------------+
| KERNEL "CraftGraph core" (C++17, statyczna lib, headless)      |
|   catalog: czesci jako DANE (masa, porty, przepis fizyczny)    |
|   craftgraph: OSOBNE grafy dzielace endpointy {partId,portId}: |
|     strukturalny / mechaniczny / energii / sygnalow / materialu|
|   compiler: craftgraph -> ciala/jointy box3d (bodyId ULOTNE)   |
|   worldmatter: kolumny materialu, warstwy, operacje narzedzi   |
|   sim: wlasnosc b3World, fixed-step, commity na granicy ticku  |
|   persist: zapis wersjonowany + sondy round-trip               |
|   validation: sondy buduja REALNY swiat produkcyjna sciezka    |
+----------------------------------------------------------------+
| box3d (vendored @pin) - integrator mechaniczny; src NIETYKALNE |
+----------------------------------------------------------------+
| CONTENT: packi assetow Jozza (manifest v1: walidacja, wersja,  |
|   proceduralny fallback - brak grafiki nigdy nie blokuje gry)  |
| TOOLS: gate (build+testy+sondy+zrzuty, kontrola timestampow!), |
|   CI od pierwszego commita, tooling zrzutow ekranu             |
+----------------------------------------------------------------+
```

Zasady graniczne (kandydaci na pierwsze ADR-y nowego repo):

1. **Kernel headless:** kompiluje się i testuje bez powłoki (dowód
   wykonalności: `spike/kernel_v0` — patrz §7).
2. **Domeny tożsamości:** ID autorskie trwałe (`partId`, `linkId`,
   `columnChunkId`), ID runtime ulotne (`bodyId`, `shapeId`) — rozdzielone
   typami. Lekcja VAW okupiona breaking-changem.
3. **Osobne grafy, wspólne endpointy** — odpowiedź na ryzyko „CraftGraph
   monolit": połączenie niosące siłę+energię+sygnał to KILKA krawędzi
   w KILKU grafach na tej samej parze `{partId, portId}`.
4. **box3d integruje rezultat mechaniczny** — nie definiuje, czym jest
   droga, grunt, skrzydło ani materiał w łyżce (systemy domenowe liczą
   siły, integrator je składa).
5. **Zakaz cichych fallbacków numerycznych** (lekcja VAW: powodowały
   niewyśledzalne bugi): dane skończone i znormalizowane albo jawny błąd.
6. **Model operatora v0** (kamera + wsiąście) + zarezerwowany uchwyt
   interakcji fizycznej (JES_01 §5).

Twarde reguły zależności (scalone z pakietem Sol §5; łamanie = STOP):

```text
produkt/workbench ─X→ kod z labs/**        (tylko promowane kontrakty)
domena/spec       ─X→ API solvera/renderera/ECS (b3BodyId, Entity, Node...)
metadane assetu   ─X→ autorytet gameplay   (wygląd ≠ fizyka, zasada 14)
uchwyty runtime   ─X→ trwały zapis         (zasada 13)
typy backend-native kończą się na granicy adaptera
wspólna abstrakcja: dopiero przy 2. realnym konsumencie (zasada 16)
każdy promowany kontrakt ma migration/exit story
```

Rola box3d doprecyzowana (konwergencja obu pakietów): box3d jest
**pierwszym realnym backendem i punktem odniesienia za granicą adaptera**
— nie fundamentem domeny. Domena mówi o częściach, przestrzeniach, siłach
i jointach semantycznie; wymiana solvera w przyszłości nie może dotykać
dokumentów autorskich ani zapisów. (JV udowodniło frajdę mechaniczną
box3d; nie udowodniło wystarczalności Coulomb friction dla miękkiego
gruntu/opon — to osobny program labów terenu.)

## 5. Model rozwoju: LAB-FIRST (pomysł Jozza podniesiony do fundamentu)

Decyzja kierunkowa Jozza (2026-07-15): rozwijać systemy **osobno, jak
sample box3d**, potem powoli składać, ciągle optymalizując. Formalizacja:

- Powłoka od F0 zawiera **LAB HOST** — rejestr scen-laboratoriów
  (odpowiednik rejestru sampli box3d): każdy lab = nazwana scena
  z własnym UI, uruchamialna z linii komend (`--lab <nazwa>`), z hookami
  headless (env/parametry) i zrzutem ekranu.
- **Jeden lab = jedno sprzężenie systemowe** (np. „łyżka–kolumny",
  „walec–zagęszczenie", „skrzydło–pas"). Eksperymenty X (JES_03) żyją jako
  laby.
- **Rozstrzygnięcie konfliktu „laby trwałe vs laby usuwalne" (scalenie
  z pakietem Sol, 2026-07-15 — pełny zapis: JES_03 §7):** trzy warstwy
  o RÓŻNEJ trwałości. (1) **LAB HOST** (rejestr scen, hooki headless,
  zrzuty) = trwała architektura produktu od dnia 1. (2) **Implementacja
  labu** = z założenia USUWALNA (konstytucja, zasada 15) — odpowiada na
  jedno pytanie i może zostać wyrzucona po ekstrakcji wiedzy.
  (3) **Promowany wynik labu** (kontrakt + fixture + test regresji +
  ewentualna trwała scena diagnostyczna PRZEPISANA na promowane API) =
  trwały. Produkt/workbench nigdy nie linkuje wnętrza labu — dokładnie
  błąd monolitu M6 (pojazd+UI+tor+persystencja+sondy w jednym ekosystemie,
  gdzie każda zmiana fałszowała kilka osi naraz).
- ⚠ To ROZBRAJA obie znane pułapki naraz: „host sampli stał się nośny
  przypadkiem" (JV) — bo host jest naszą architekturą; oraz
  „laboratorium-muzeum" (ryzyko R2 z JES_03) — bo po każdej promocji
  ciągły szkielet integracyjny S0 musi skonsumować nową zdolność.
- Mapa/świat na start **mała i ograniczona** (decyzja Jozza); skalowanie
  rozmiaru dopiero za dojrzałością mechanik. Projekt akceptuje, że przez
  długi czas NIE nadaje się do casualowego grania.

## 6. Reprezentacja świata roboczego (hipotezy do X1–X3)

- **Kolumny 2.5D** (`solidHeight, looseVolume, materiały, zagęszczenie,
  wilgotność`) jako pierwsza reprezentacja robót ziemnych — świadome
  ograniczenie (bez tuneli/nawisów), wystarcza pętli: odspój → przenieś →
  rozłóż → wyprofiluj → zagęść → przetestuj pojazdem.
- Świat pocięty na **chunki kolumn**; operacja narzędzia (`MaterialOperation`
  z wizji §5.4 — narzędzie inicjuje, domena materiałowa interpretuje)
  brudzi chunk; collider chunka odtwarzany na granicy ticku.
- **Twardy fakt silnika:** box3d NIE MA aktualizacji heightfielda in-place
  — tylko `b3CreateHeightField`/`b3DestroyHeightField`/`b3CreateHeightFieldShape`
  (`include/box3d/collision.h:380–393`, sprawdzone 2026-07-15). Strategia
  A: per-chunk heightfield destroy+create. Strategia B: kolumny jako zestaw
  box-hulli per chunk (łatwiejsza częściowa przebudowa, droższy broad-phase).
  A vs B rozstrzyga **eksperyment X1**, nie dyskusja. Ryzyka mierzalne:
  utrata manifoldów/warm-startu pod stojącą maszyną (znany koszt z lekcji
  phased-union kół w JV), koszt create dla chunka NxN, impulsy-widmo.
- **Własność powierzchni:** każdy punkt kontaktu ma dokładnie jednego
  fizycznego właściciela (konstrukcja > warstwa nawierzchni > teren gładki),
  nawet gdy dane pochodzą z kilku domen. Zakaz koplanarnych, konkurencyjnych
  colliderów (lekcja: dublujące się powierzchnie generowały podwójne
  manifoldy w mapie JV).
- **Bilans masy świata jako sonda:** suma (teren + luźny + w maszynach +
  w buforach) = const ± jawne operacje; w walidatorze od X1.
- Drogi: zgodnie z JES_01 §6 — bez autorytetu edycji świata; wytyczenie
  (korytarz pomiarowy) + metryki jakości (profil, spadek, zagęszczenie,
  zachowanie pojazdu testowego).

## 7. Twarde fakty silnika (zweryfikowane w źródłach/eksperymentem, 2026-07-15)

- box3d: MIT, C17, publiczny od 2026-06-30, **v0.1 alpha**; czyste C API
  (`B3_API`), samodzielna statyczna lib.
- **`BOX3D_DOUBLE_PRECISION`** — oficjalna opcja CMake „large worlds"
  (`b3Pos` double, rotacje float — wzorzec DMat44 Jolta). Odpowiedź na
  pytanie o precyzję współrzędnych dla lotnictwa. Decyzja D10 otwarta
  (rekomendacja: ON od F0 po benchmarku — późniejsza zmiana dotyka ABI,
  formatu zapisu i replay naraz). Render niezależnie musi być
  camera-relative.
- **Eksperyment X0 ZALICZONY** (`spike/kernel_v0/`, ~230 linii nowego
  kodu): katalog części (dane) → blueprint z trwałymi `partId` → kompilator
  → ciała/jointy box3d → 10 s headless. Rover jeździ (6,25 m/s
  ustabilizowane, trzyma prześwit), **hash trajektorii identyczny między
  uruchomieniami** (`a6cca9017df176c1`) — determinizm potwierdzony liczbą.
- Detale integracyjne: box3d w tym repo budowany ze statycznym CRT →
  `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"`
  w projekcie linkującym; oś zawiasu `b3RevoluteJoint` = **lokalna oś Z**
  frame'ów jointu; konwencja przyjęta z JV: forward=+X, up=+Y, right=+Z.
- Ograniczenie znane z JV: światy pojazdów jeżdżą z
  `b3World_EnableContinuous(false)` — koła przy prędkości same są „fast"
  i CCD głodzi walidację TOI, a cienkiej geometrii brak.

## 8. Prawa inżynierskie z dem (aneks obowiązkowy — okupione błędami)

Lekcje KODU, których żadna wizja nie zawiera; łamanie ich kosztowało
tygodnie:

- **L1. Rusztowanie demo nie może stać się nośne.** (JV: host sampli;
  VAW: rosnący `game.js` z twardym limitem 2500 linii.) Kontrmiara: granica
  kernel/powłoka + lab-host jako własna architektura + budżety linii per
  moduł od F0.
- **L2. Pręty distance-joint mają gałąź lustrzaną** — twarde lądowanie
  przerzuca układ w odbite rozwiązanie („złamane zawieszenie"). Ramiona
  i wahacze = CIAŁA na zawiasach z limitami kąta.
- **L3. Małe ciała strukturalne BEZ shape'ów + jawne `b3Body_SetMassData`**
  — inaczej solver flaguje je jako „fast" i CCD vs grunt ubija assert TOI.
- **L4. `b3DefaultShapeDef()` ma categoryBits = WSZYSTKIE bity** — wąskie
  maski wymagają tagowania OBU stron pary kolizji.
- **L5. Sztywność sprężyny podąża za masą efektywną więzu** — na smukłym
  obracającym się ramieniu masa efektywna to kilka kg (człon rotacyjny
  dominuje); kompensację liczyć od docelowej sztywności na kole.
- **L6. Walidacja musi budować REALNY świat produkcyjną ścieżką** i mieć
  testy negatywne (fixture, który MUSI failować). Zielony walidator tabel
  danych przepuścił fizycznie nieprzejezdny tor (incydent mapy JV,
  2026-07-13). Bramka musi też sprawdzać exit code, istnienie
  I TIMESTAMP artefaktów builda („build OK" przy nieistniejącym exe —
  realny przypadek).
- **L7. Render is the gate.** Pracy wizualnej nie oddaje się bez obejrzenia
  zrzutu; liczby ≠ poprawny obraz (asymetria L/R w JV była błędem renderu
  przy idealnie symetrycznej fizyce — rozstrzygnął dump liczb + zrzut).
- **L8. Persystencja projektowana od razu:** stan poza „configiem" cicho
  ginie przy restarcie (JV: pola resetowane przez „R"); zapis = tabela pól
  + sondy round-trip; rozdział „strojenie/tożsamość" od „stanu widoku".
- **L9. Ścieżki assetów niezależne od CWD** (JV: wynik walidatora zależał
  od katalogu uruchomienia) — resolucja od markera roota projektu,
  testowana z dwóch CWD.
- **L10. Zakaz cichego fallbacku numerycznego** (VAW) i cichego luzowania
  progów (JV): próg zmienia tylko jawna decyzja Jozza.
- **L11. Granice pipeline'u wizualnego dowodzi się IN-GAME** (VAW: asset
  po reimporcie stracił wygląd; loader „przeszedł", gra nie) — sonda
  Blockbench→glTF→pack→scena należy do wczesnej fazy pipeline'u artu.
- **L12. Model zastępczy ma jedno źródło prawdy geometrii** — inaczej
  debug-render kłamie (dot. przyszłego patcha gąsienic i envelope koła).

## 9. UI i nawigacja (decyzja D9: Blender + Unreal Engine)

- Viewport: orbit MMB, pan Shift+MMB, zoom scroll, `F` = frame selected;
  tryb fly/walk RMB+WASD (UE) w widoku operacyjnym.
- Manipulacja: gizma translate/rotate/scale (akceleratory G/R/S), snap do
  siatki/portów, numeryczny input podczas transformacji.
- Układ: workspaces (Build / Operate / Inspect), outliner konstrukcji po
  lewej, panel właściwości po prawej, konsola diagnostyczna na dole.
- Technologia: panele = ImGui docking; viewport i gizma własne (sokol).
- Godot pozostaje źródłem dobrych wzorców (inspector, scene-dock) — Jozz
  prosił, by go nie ignorować.
- Granica uczciwości: kopiujemy WZORCE, nie zakres Blendera. Wersja 1 =
  viewport + 3 workspaces + outliner + properties. Nic więcej.

## 10. Znane niewiadome (jawna lista)

- X1: czy destroy+create chunka pod maszyną jest stabilne i tanie (§6).
- D3: ECS powłoki (flecs/EnTT/własna) — spike.
- D10: double precision od F0 — benchmark.
- Zmiana masy/COM ciała w locie (ładunek w łyżce) a stabilność solvera —
  częściowo sprawdzone w JV (`SetMassData` używany statycznie), dynamika
  do X2.
- Model oporu gruntu bez geotechniki (X2) i model zagęszczania (X3).
- Persystencja zmutowanego świata (delta terenu) — projekt formatu przy X1.
- Wydajność sceny z tysiącami części (instancing/MultiMesh-odpowiednik) —
  odroczona do pierwszego profilu.
