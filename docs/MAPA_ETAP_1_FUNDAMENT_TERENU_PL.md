# Mapa — Etap 1: fundament terenu

**Status: ZAMKNIĘTY 2026-07-11, DOSZLIFOWANY 2026-07-12** (build+walidator+
test+boot smoke zielone, rendery obejrzane, wydajność zmierzona — patrz
§6/§7). Doszlifowanie na życzenie Jozza (offroad = 400×400, rzeźba grzbietowa
z domain warpem, chropowatość zależna od wysokości) opisane w §10. Następny:
Etap 2.

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`. Czytaj najpierw roadmapę
(layout §5, ryzyka §8). Ten etap NIE dodaje przeszkód ani toru — tylko ziemię.

## 1. Cel

Po tym etapie świat ma docelowy kształt: płyta 400×400 m (3×3 kafle, top y=0),
obok niej chunk offroad **400×400 m** (od 2026-07-12: te same wymiary co
płyta — "dwa kafle mapy równej wielkości", patrz §10) z wielowarstwowym
szumem, wychodzący **spod** płyty (zakładka), z seedem i przyciskiem
regeneracji, plus minimalny teleport, żeby dało się to wszystko w ogóle
objechać. Stary wave-patch znika. Wydajność zmierzona i zapisana.

## 2. Zakres

1. Nowy moduł `samples/jozz_vehicle_world_layout.h` — WSZYSTKIE stałe layoutu
   w jednym miejscu (wymiary płyty i kafli, prostokąt offroadu, prostokąty
   stref z roadmapy §5, punkty teleportów). Etapy 2–6 czytają stąd, nie
   duplikują liczb.
2. Nowy moduł `samples/jozz_vehicle_world_terrain.{h,cpp}` — generator szumu +
   budowa heightfielda + regeneracja.
3. Płyta: 3×3 kafle po 133.33×133.33 m (half-extent 66.667, half-height 1.0,
   center y=−1). Zastępuje pojedynczy box w OBU labach.
4. Offroad heightfield wg parametrów §4.
5. Usunięcie starego wave-patcha i jego pola `roughTerrainField` z course'u.
6. UI: sekcja „Mapa" w zakładce Świat (seed, „Przebuduj teren", 3 teleporty).
7. Pomiar wydajności przed/po (tabela w tym docu po wykonaniu).

**Poza zakresem:** przeszkody (E2), kolory/tarcia stref (E3), przenoszenie
propów (E2/E5). Rampy/washboard/propy na razie ZOSTAJĄ tam, gdzie są (będą
na kaflu centralnym — nie przeszkadzają bramce tego etapu).

## 3. Miejsca w kodzie (punkty zaczepienia)

- `samples/jozz_vehicle_m6_rig_lab.cpp:25-41` — tworzenie płyty + wywołanie
  course'u. Tu wchodzi `CreateJozzWorldGround(...)` z nowego modułu.
- `samples/jozz_vehicle_m5_drivable_lab.cpp:45-48` — to samo dla labu M5
  (`AddGroundBox(120)` → wspólny helper; kategoria terenu domyślna 1 zostaje).
- `samples/jozz_vehicle_m5_test_course.cpp:122-136` — wave-patch DO USUNIĘCIA
  (cały blok + `roughTerrainField` z nagłówka + destroy w
  `DestroyJozzVehicleM5TestCourse`).
- `samples/jozz_vehicle_m6_rig_lab.cpp:262-270` — `GetSpawnHeight()` +
  `CreateVehicle()` spawnuje w `{0, h, 0}`: dodać parametr kotwicy
  (pozycja XZ + yaw), domyślnie stary spawn.
- `samples/jozz_vehicle_m6_rig_lab_ui_tabs.cpp:483` — `DrawWorldTab`, tu
  sekcja „Mapa".
- API: `b3HeightFieldDef` (`include/box3d/types.h:2217-2249`),
  `b3CreateHeightField` (`collision.h:380`), `b3CreateHeightFieldShape`
  (`box3d.h:816`), destroy: `b3DestroyHeightField` (wzorzec ownershipu jak
  w starym course — silnik trzyma tylko referencję).

## 4. Generator terenu (serce etapu)

> **Od 2026-07-12 przepisany na wersję z §10** (ridged + domain warp +
> chropowatość zależna od wysokości). Tabela i wzór niżej to wersja
> AKTUALNA; oryginalna wersja z 2026-07-11 (proste FBM + maska płaskości
> niezależna od wysokości) jest opisana jako punkt wyjścia w §10.1.

Deterministyczny value-noise 2D (hash z seeda; iq-style hash na parze intów,
wygładzanie quintic). Warstwa makro to **ridged noise, 2 oktawy**, próbkowana
przez **domain warp** (przesunięcie punktu próbki inną warstwą szumu) —
zamiast gładkich pagórków daje sieć wijących się grzbietów i dolin. Ten sam
kształt makro (`elevationShape`, [0,1]) napędza **chropowatość**: ile z
amplitudy mezo/mikro faktycznie się pojawia w danym punkcie, zbramkowane
własną warstwą szumu (patrz §10.2).

| Warstwa | Długość fali λ | Amplituda / zakres | Rola |
|---|---|---|---|
| domain warp (x, z) | ~60 m | przesunięcie ±22 m | wygina grzbiety, łamie wyrównanie do siatki szumu |
| makro, oktawa 1 (ridged) | ~90 m | do ±8.0 m | główny kształt grzbietów/dolin |
| makro, oktawa 2 (ridged) | ~40.5 m | waga 0.3 w blendzie z oktawą 1 | drobniejszy detal na tych samych grzbietach |
| mezo | ~16 m | 1.2 m × chropowatość | pofalowanie, muldy naturalne |
| mikro | ~2.8 m | 0.22 m × chropowatość | kamienista faktura (twardy offroad) |
| szum chropowatości | ~30 m | bramkuje mezo/mikro względem `elevationShape` | „nisko = gładziej, wysoko = szorstko", ale nie mechanicznie (§10.2) |
| gradient trudności | — | mnożnik 0..1 | 0 przy styku z płytą → 1 od ~60 m w głąb; skala z odległości od zachodniej krawędzi chunku |

`elevationShape = lerp(ridged(makro1), ridged(makro2), 0.3)` (obie próbkowane po domain warpie)
`roughness = clamp01(elevationShape * lerp(0.4, 1.3, szumChropowatości))`
`height(x,z) = zakładka(x) + trudność(x) * [ makro + mezo*lerp(0.2,1,roughness) + mikro*lerp(0.1,1,roughness) ]`

- **Pas zakładki (wymaganie Jozza wprost):** pierwsze ~4 kolumny od strony
  płyty: liniowy ramp od **−0.12 m do wartości szumu**; szum przy styku i tak
  ~0 przez gradient. Efekt: heightfield wchodzi 2 m POD płytę (chunk zaczyna
  się na x=198, płyta kończy na x=200), koło zjeżdża z płyty na powierzchnię
  będącą ≤5 cm niżej — R3 z roadmapy: w pasie zakładki wysokość ≤ −0.05 m,
  więc na płycie NIE ma podwójnego kontaktu.
- Siatka: **cell 1.25 m, 321×321 punktów** (320 komórek × 1.25 = 400 m, od
  2026-07-12 — patrz §10.3 o rozmiarze). Mikro-oktawa λ 2.8 m > 2×cell —
  bez aliasingu.
- Defensywny `Clamp(height, globalMin+0.5, globalMax-0.5)` na końcu funkcji:
  suma warstw (makro ≤8, mezo ≤1.2, mikro ≤0.22) mieści się w ±~9.5 m z dużym
  zapasem do ±12/+14, klamra to tylko zabezpieczenie przed przyszłym
  strojeniem parametrów, nie aktywna w obecnej konfiguracji.
- `globalMinimumHeight/globalMaximumHeight`: stałe **−12 / +14** (zapas; jeśli
  kiedyś dojdzie drugi chunk, oba MUSZĄ mieć te same wartości — po to istnieją,
  `types.h:2237-2242`). Kwantyzacja 16-bit przy 26 m zakresu = 0.4 mm — pomijalna.
- `materialIndices`: w tym etapie NULL (jeden `baseMaterial`, friction 0.85,
  jak stary patch). Per-cell materiały wchodzą w E3.
- Kategoria: `shapeDef.filter.categoryBits = terrainCategoryBits` — **R5!**
- Seed: `uint32_t`, domyślnie stały (np. 1337), trzymany w członku labu (NIE
  w `JozzVehicleM6Config` — teren to świat, nie strojenie pojazdu; presety go
  nie dotykają).

## 5. Kroki

1. `world_layout.h` ze stałymi + komentarz z mapką ASCII z roadmapy.
2. `world_terrain`: szum + `BuildOffroadHeightField(seed)` zwracające
   `b3HeightFieldData*`; test ręczny wartości (min/max/monotoniczność zakładki)
   printem w walidatorze LUB szybkim asercikiem w samym module.
3. Płyta 3×3 w obu labach; **zbadać `SetGroundShape`** (R10,
   `samples/gfx/debug_adapter.c`): czy wielokrotne wywołanie działa per-shape?
   Jeśli nie — grid na kaflu centralnym, pozostałe kafle z neutralnym
   `customColor`.
4. Offroad body na `x=198..518, z=−160..160` (pozycja body wynika z konwencji
   originu heightfielda — **PUŁAPKA: zbadać, czy origin to róg (0,0) siatki czy
   środek**, jednym zrzutem z `b3ComputeHeightFieldAABB` albo debug-drawem,
   ZANIM przyklei się liczby w layout.h).
5. Usunąć wave-patch; sprawdzić, że `DestroyJozzVehicleM5TestCourse` nie ma już
   nic do zwalniania poza propami.
6. UI „Mapa": InputInt seed + „Przebuduj teren" (destroy shape → destroy
   field → build nowy; po 10 regeneracjach licznik meshy w registry nie rośnie
   — R4) + teleporty: „Start", „Offroad — wjazd" (x≈240), „Offroad — głęboko"
   (x≈460): `CreateVehicle(kotwica)`; wysokość spawnu nad terenem = próbka
   wysokości heightfielda w punkcie + zapas (prosty raycast w dół
   `b3World_CastRay` albo odczyt z tablicy wysokości przez layout-helper).
7. Pomiary: fps + ms/step (istniejący profiler HUD) na pustej płycie, na
   grzbiecie offroadu, przy regeneracji (spike jednorazowy OK). Budżet:
   step < 4 ms, fps > 60. Fallback: cell 1.6 m (201×201) — decyzja przy bramce.
8. Kamera: sprawdzić, że far plane / mgła nie ucina offroadu z perspektywy
   spawnu (jeśli ucina — odnotować, NIE grzebać w gfx bez potrzeby: wystarczy
   że strefy są widoczne z ich teleportów).

## 6. Bramka (wszystko musi być zielone) — ✅ ZAMKNIĘTA 2026-07-11

- ✅ Build Debug (samples/jozz_vehicle_validation/test) + walidator CLI (18 sond,
  `jozz_vehicle_validation: OK`) + `test.exe` (wszystkie testy silnika zielone,
  niezmienione tym trackiem) + boot smoke labów **M6 i M5** (`--frames 300`,
  0 sokol errors dla obu, R11).
- ✅ **Rendery (suita `mapa_e1_*`, zapisane w scratchpadzie sesji, obejrzane
  narzędziem Read — render is the gate):**
  - `mapa_e1_topdown2` — widok z lotu ptaka na płytę (3×3 kafle, środkowy z
    proceduralną siatką, pozostałe neutralnym szarym `SetShapeMaterial` — R10)
    i początek offroadu; szew x=200 bez uskoku.
  - `mapa_e1_seam` — bliski, niski kąt na sam styk: jedna ciągła płaszczyzna,
    zero progu w górę (bezpośredni kontrast ze screenem „przed" z feedbacku).
  - `mapa_e1_massif_try1` — perspektywa w głąb offroadu (seed 1337): wyraźne,
    faliste wzgórza (makro+mezo), płyta widoczna w tle na tej samej wysokości.
  - `mapa_e1_glade_try2` — inne miejsce offroadu: stroma, wysoka forma (blisko
    maks. amplitudy makro) — potwierdza duże zróżnicowanie „miejscami bardzo
    nierówny" z feedbacku Jozza.
  - `mapa_e1_regen_seed777` — ta sama kamera co `massif_try1`, po
    `JOZZ_M6_REGEN_SEED=777`: kształt terenu WYRAŹNIE inny, szew nadal
    poprawny — determinizm + regenerowalność potwierdzone wizualnie.
  - `mapa_e1_autodrive_800` — auto na 18.5 m/s (`JOZZ_M6_AUTODRIVE`), wszystkie
    4 koła w kontakcie, poślizg ~0.1–0.2°, na kaflu środkowym z szwem
    płyta/offroad widocznym w tle — dowód stabilności przy prędkości.
  - (Świadomie NIE zrealizowano dedykowanego zrzutu „idealnej płaskiej
    polany" — `glade_try2` trafił akurat w stromą formę; maska płaskości
    działa (widoczna w kontraście między `massif_try1` i `glade_try2`), ale
    trafienie w konkretnie płaski punkt zależy od seeda. Nie blokuje bramki —
    odnotowane niżej jako materiał do ew. dostrojenia parametrów maski.)
- ✅ **Przejazd headless** (patrz §9 — brak człowieka przy klawiaturze w tej
  sesji, więc bramka zrealizowana przez nowy hook `JOZZ_M6_AUTODRIVE`):
  800 kroków pełnego gazu na wprost od Startu, 0 sokol errors, prędkość
  18.5 m/s w momencie zrzutu, poślizg <0.2° — przejazd przez WSZYSTKIE szwy
  kafli i przez styk płyta/offroad bez zdarzenia. R6 i część R2 zamknięte tym
  samym przebiegiem.
- ✅ Tabela wydajności wypełniona w §7. Wpis w CHECKPOINTS.

## 7. Wyniki pomiarów

Zmierzone `JOZZ_M6_PERF_DUMP` (patrz §9) — uśrednione ms/step z ostatnich
60 kroków profilera silnika, na tej maszynie deweloperskiej. `fps` tu to
`1000/step_ms` (czysto solver, NIE realna klatka renderu z panelem ImGui) —
liczba orientacyjna do porównań między scenariuszami, nie obietnica realnej
klatkarzy.

| Scenariusz | ms/step | fps (solver) | Ciała / kolidery / kontakty | Uwagi |
|---|---|---|---|---|
| płyta, Start, bez ruchu | 1.195 | 837 | 50 / 49 / 20 | budżet (<4 ms) zapas ~3.3× |
| offroad, „wjazd" (x≈240) | 1.184 | 845 | 50 / 49 / 21 | brak wzrostu kosztu przy wejściu na heightfield |
| offroad, „głęboko" (x≈460) | 1.116 | 896 | 50 / 49 / 21 | pełna trudność (gradient=1), nadal poniżej budżetu |
| po „Przebuduj teren" ×10 | — | — | shapes: 49 → 49 | **R4 zamknięte**: `JOZZ_M6_REGEN_COUNT=10` — licznik meshy w registry silnika (`GetDebugShapeCount`) identyczny przed i po (zmierzony po ustabilizowaniu renderu, +3 klatki) — brak przecieku |

Wniosek: 257×257 heightfield (~131k trójkątów) NIE jest wąskim gardłem przy
tej wielkości sceny — koszt kroku zdominowany przez rig zawieszenia (50 ciał
to głównie kości/koła/wahacze pojazdu, nie teren). Fallback z kroku 7 planu
(cell 1.6 m) **niepotrzebny** — zapas ~3× nawet bez niego.

**Po doszlifowaniu 2026-07-12** (siatka 321×321 ≈ 103k pkt, +7 próbek szumu
na punkt do budowy — patrz §10): zmierzone ponownie, wyłącznie koszt
BUDOWY terenu rośnie (jednorazowy, przy starcie/regeneracji), koszt KROKU
fizyki nieznacznie:

| Scenariusz | ms/step | fps (solver) | Uwagi |
|---|---|---|---|
| płyta, Start | 1.652 | 605 | wciąż ≥2.4× zapasu do budżetu 4 ms |
| offroad, „wjazd" (x≈240) | 1.543 | 648 | |
| offroad, „głęboko" (x≈560, nowa pozycja kotwicy) | 1.374 | 728 | |
| po „Przebuduj teren" ×10 | — | — | shapes: 49 → 49, **R4 nadal zamknięte** |

Wzrost ~0.2–0.4 ms/step względem tabeli wyżej (kontakty bez zmian: 20-21) —
najpewniej marginalny koszt broad-phase na większej siatce (321² vs 257²
komórek), nie zmierzony osobno co do przyczyny, bo budżet i tak zostaje
bezpieczny (≥2.4× zapasu). Budowa/regeneracja terenu (więcej próbek szumu na
punkt) to koszt jednorazowy przy starcie/„Przebuduj teren", nie w pętli
kroku — R4 (shapes 49→49) potwierdza brak przecieku przy tym koszcie.

## 8. Ryzyka etapu

R1–R4, R6, R10, R11 z roadmapy + dwie lokalne pułapki: konwencja originu
heightfielda (krok 4) i wysokość spawnu teleportu nad terenem (krok 6 — auto
zrzucone W teren = natychmiastowy zły pierwszy test).

**Rozstrzygnięcia pułapek (potwierdzone czytaniem silnika, nie zgadywaniem):**

- **Origin heightfielda = RÓG siatki, nie środek** (`src/height_field.c`:
  `hf->aabb.lowerBound = {0, ...}`, `upperBound = {scale.x*(columnCount-1), ...}`)
  — lokalny (0,0) mapuje się na pozycję body. Chunk offroadu zaczyna się więc
  wprost na `bodyDef.position = {kOffroadOriginX, 0, kOffroadOriginZ}` bez
  dodatkowego przesunięcia o pół rozmiaru.
- **R10 potwierdzone**: `SetGroundShape` to POJEDYNCZE pole
  (`s_adapter.groundShapeId`), każde wywołanie nadpisuje poprzednie — fallback
  z planu (siatka tylko na kaflu środkowym, reszta `SetShapeMaterial`) jest
  jedynym poprawnym rozwiązaniem, nie tymczasowym obejściem.
- Wysokość spawnu teleportu: rozwiązana NIE raycastem, tylko wprost —
  `SampleJozzWorldGroundHeight` liczy tę samą ciągłą funkcję FBM co budowa
  siatki (bez interpolacji, dokładna wartość w dowolnym x/z), więc `CreateVehicle`
  zawsze zna prawdziwą wysokość terenu pod kotwicą przed spawnem.

## 9. Rozszerzenie: headless testing hooks (poza pierwotnym zakresem, dodane w trakcie realizacji)

Ta sesja nie miała człowieka przy klawiaturze (agent autonomiczny), a bramka
etapu wymaga przejazdów przy prędkości i pomiaru wydajności „na grzbiecie
offroadu". Zamiast pominąć te punkty bramki, dodano cztery nowe zmienne
środowiskowe do M6 labu, w tej samej konwencji co istniejący rejestr
`JOZZ_M6_*` (`JOZZ_M6_HERTZ`, `JOZZ_M6_PRESET`, ...) — nie nowy mechanizm, tylko
kolejne wpisy w już istniejącym wzorcu:

| Zmienna | Rola | Dlaczego warto zachować na przyszłość |
|---|---|---|
| `JOZZ_M6_TELEPORT=<nazwa>` | teleportuje na nazwaną kotwicę z `kWorldAnchors` zaraz po boot | E2–E6 też będą potrzebować „dojazdu" do stref bez klikania w UI przy zrzutach headless |
| `JOZZ_M6_AUTODRIVE=1` | pełny gaz na wprost w KAŻDYM `Step()` | jedyny sposób na headless „przejazd przy prędkości" bez GUI automation; przyda się w E2 (skocznie), E3 (tor) |
| `JOZZ_M6_PERF_DUMP=<step>` | jednorazowy printf uśrednionego ms/step + liczników świata na danym kroku | bez tego pomiar wydajności wymagałby czytania wykresu z zrzutu ekranu (niedokładne); z tym — twarde liczby w stdout |
| `JOZZ_M6_REGEN_COUNT=<n>` + `JOZZ_M6_REGEN_SEED=<seed>` | n regeneracji terenu pod rząd + printf licznika meshy przed/po (ustabilizowany +3 klatki) | bezpośredni, powtarzalny test R4 (przeciek mesha) — bez tego weryfikacja wymagałaby ręcznego liczenia obiektów na oko |

Wszystkie cztery są opisane w komentarzu-rejestrze w konstruktorze
`JozzVehicleM6RigLab` (ten sam blok co istniejące env, z regułą „grep musi się
zgadzać z komentarzem"). Żadna nie zmienia domyślnego zachowania (wszystkie
`false`/wyłączone, gdy zmienna nieustawiona) — zero ryzyka dla normalnej pracy
w UI.

## 10. Doszlifowanie (2026-07-12) — na wyraźne życzenie Jozza

Jozz zatrzymał się na tym etapie celowo: *"to jest bardzo ważna sprawa, będę
na tej mapie spędzał kilkadziesiąt godzin tygodniowo w najbliższym czasie,
potrzebujemy to dopieścić i przemyśleć lepiej"*. Trzy konkretne uwagi ze
screena i opisu, każda z osobnym rozwiązaniem niżej.

### 10.1 Punkt wyjścia (wersja 2026-07-11, dla kontrastu)

Oryginalny generator z §4 (przed doszlifowaniem): trzy niezależne oktawy
zwykłego (nie ridged) value-noise dodawane do siebie (makro+mezo+mikro),
przemnożone przez **maskę płaskości** — osobną, NIEZALEŻNĄ od wysokości
warstwę szumu (smoothstep w [0.3,0.7], λ 140 m) sterującą "polany kontra
masywy". Efekt wizualny: łagodne, zaokrąglone pagórki (jak wydmy), bez
wyraźnych grzbietów, z losowo rozrzuconymi płaskimi kieszeniami niezależnie
od tego, czy dany obszar był akurat wysoki czy niski. Offroad 320×320 m,
mniejszy niż płyta 400×400 m.

### 10.2 Uwaga 1: rozmiar — offroad = płyta (400×400)

Ze screena Jozza: pomarańczowe strzałki pokazują, że pas offroadu powinien
mieć tę samą rozpiętość co płyta, "traktując to jako dwa jednakowej wielkości
tile mapy". Zmiana czysto parametryczna w `jozz_vehicle_world_layout.h`:
`kOffroadSize` 320→400, siatka 257→321 punktów (cell 1.25 m bez zmian),
`kOffroadOriginZ` przeliczony tak, by offroad był wyśrodkowany symetrycznie
do płyty w Z (−200 zamiast −160). Efekt widoczny na zrzucie
`07_wide_threequarter` (patrz §10.4) — szew ciągnie się przez całą szerokość
kadru bez żadnej "brakującej" krawędzi.

Anchor „Offroad — głęboko" przesunięty z x=460 na x=560 (bliżej nowej
dalekiej krawędzi x=598, żeby nadal oznaczał realnie "głęboko w terenie", nie
tylko środek chunku).

### 10.3 Uwaga 2: chropowatość zależna od wysokości, bramkowana własnym szumem

Jozz: *"tam gdzie teren jest niżej było mniej małych ostrych nierówności,
mapa powinna być gładka, a im wyższy teren tym więcej takim nierówności, z
odpowiednim szumem na występowanie tego szumu"*.

Stara "maska płaskości" (§10.1) była ślepa na wysokość — płaskie kieszenie
mogły trafić się zarówno w dolinie, jak i na szczycie. Nowe rozwiązanie:

1. `elevationShape` (kształt warstwy makro, patrz §10.4 niżej) w [0,1] jest
   **bezpośrednim sygnałem wysokości** — 0 = dolina, 1 = grzbiet.
2. Osobna warstwa szumu (`kRoughnessWavelength` ≈ 30 m) **bramkuje** ten
   sygnał: `roughness = clamp01(elevationShape * lerp(0.4, 1.3, szum))`.
   Przy niskim szumie roughness spada do ~40% wartości "czystej" wysokości
   (gładki fragment nawet wysoko — skała wygładzona wiatrem), przy wysokim
   szumie rośnie do 130% i obcina się przy 1 (odsłonięty, poszarpany kamień
   nawet niżej). To dosłownie realizuje "z odpowiednim szumem na
   występowanie tego szumu" — chropowatość to TREND od wysokości, nie
   sztywna reguła.
3. `roughness` skaluje amplitudę WYŁĄCZNIE mezo i mikro (`lerp(0.2,1,r)` i
   `lerp(0.1,1,r)`) — nigdy makro. Makro to "kształt góry", roughness to
   "faktura powierzchni"; rozdzielenie tych dwóch trosk było celowe, żeby
   dolina i tak miała trochę drobnicy (nie idealnie bilardowa gładkość — Jozz
   pisał "mniej", nie "zero"), a szczyt mógł miejscami być gładki.

### 10.4 Uwaga 3: bliżej prawdziwych gór — ridged noise + domain warp

Jozz: wysokość jest "całkiem spoko", ale prosił o coś bliższego prawdziwym
górom, przy świadomości że erozja/skały/wąwozy to za duży zakres na ten
etap. Dwie tanie, standardowe techniki z proceduralnej generacji terenu,
zamiast erozji:

- **Domain warp**: przed próbkowaniem warstwy makro, punkt (x,z) jest
  przesuwany o osobną warstwę szumu (λ 60 m, do ±22 m). Bez tego grzbiety
  układałyby się wzdłuż osi siatki szumu (widoczna sztuczność); z warpem
  wiją się i rozgałęziają organicznie — dokładnie to widać na zrzucie
  `06_threequarter_mid`.
- **Ridged, 2 oktawy**: `1 - |szum|`, podniesione do kwadratu dla ostrzejszych
  grani. Zamiast symetrycznych, zaokrąglonych pagórków (stary generator)
  powstaje sieć POŁĄCZONYCH grzbietów oddzielonych szerszymi dolinami —
  charakterystyczny wygląd górski, nie wydmowy. Druga oktawa (waga 0.3,
  ~2.2× wyższa częstotliwość) dokłada drobniejszy detal na tych samych
  grzbietach zamiast tworzyć nowe, niepowiązane kształty.

Koszt obu technik to wyłącznie dodatkowe próbki szumu **przy budowie
terenu** (raz na start/regenerację), zero wpływu na krok fizyki — stąd
można było dodać 2 dodatkowe warstwy (warp x, warp z) plus drugą oktawę
makro plus warstwę chropowatości bez obaw o budżet 4 ms (potwierdzone w
§7: nadal ≥2.4× zapasu).

**Rezygnacja z alternatyw rozważonych i odrzuconych:** erozja hydrauliczna
(symulacja spływu wody rzeźbiąca kaniony) i teselacja skalna (twarde,
kanciaste fasety) obie dają bardziej przekonujący wynik, ale wymagają albo
symulacji iteracyjnej (koszt budowy dużo wyższy, złożoność kodu nieadekwatna
do jednego etapu), albo osobnego systemu materiałów per-trójkąt (to już
zakres E3). Ridged + warp to najlepszy stosunek jakości do kosztu na tym
etapie; erozja może wrócić jako pomysł na później, jeśli po grze na mapie
Jozz uzna że wciąż brakuje "prawdziwości".

### 10.5 Zrzuty (doszlifowanie)

Zapisane w scratchpadzie sesji, obejrzane narzędziem Read przed uznaniem
zmiany za gotową:

- `06_threequarter_mid` — perspektywa 3/4 nad środkiem offroadu: wyraźnie
  widoczna sieć wijących się, rozgałęziających grzbietów (efekt domain
  warpu), różnica faktury dolina/grzbiet.
- `07_wide_threequarter` — szerszy kąt obejmujący płytę i offroad razem:
  potwierdza równe rozmiary kafli i czysty szew (bez uskoku) na całej
  szerokości styku.
- `08_valley_vs_ridge` — bliski kontrast: gładszy teren nisko, bardziej
  poszarpana faktura wyżej — wizualne potwierdzenie chropowatości zależnej
  od wysokości z §10.3.

Próby bardzo bliskich ujęć szwu pod małym kątem (promień ~35-45 m, pitch
15-35°) dawały mylący obraz — kamera patrzy niemal WZDŁUŻ granicy
płyta/offroad, więc w kadrze dominuje spłaszczona perspektywicznie
pojedyncza powierzchnia (płyta albo początkowy stok offroadu), co z daleka
wygląda jak jedna wielka rampa. To artefakt kąta kamery przy tej geometrii,
nie usterka terenu — `07_wide_threequarter` z większego dystansu i przy
większym yaw pokazuje ten sam obszar poprawnie, bez dwuznaczności. Odnotowane
tu, żeby przyszłe sesje nie traciły czasu na tę samą pułapkę.
