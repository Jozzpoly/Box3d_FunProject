> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# M8 Suspension Rig Repair — analiza źródłowa i niezawodny plan naprawy

Data: 2026-07-06
Branch: `jozz-vehicle-sandbox-m0`
Status: **analiza + plan** (bez zmian w rigu — czeka na akceptację Jozza)
Wejście: zdjęcia Jozza z faktycznej jazdy M7 (rig zawieszenia całkowicie źle)

## 0. Werdykt w jednym zdaniu

Fizyka (dynamika) M7 może być w większości poprawna, ale **warstwa wizualna
i geometria rigu są zepsute i NIGDY nie zostały obejrzane** — poprzedni agent
oddał projekt na podstawie zielonych liczb i „0 sokol errors", nie zobaczywszy
ani jednej klatki. To jest sedno porażki i sedno naprawy.

## 1. Co realnie pokazują zdjęcia (fakty, nie interpretacje)

```text
- podwozie (beżowy box) UNOSI SIĘ wysoko nad ziemią, wielki prześwit
- koła są ogromne względem podwozia, całość wygląda "rozjechana"
- tylne narożniki: model zawieszenia + koło ustawione krzywo, płaskie
  "belki" sterczą na wierzchu kół na zewnątrz
- model mountu (zdj. 3) to sztywna metalowa bryła wciśnięta w bok
  podwozia, koło zwisa z niej - NIE wygląda jak pracujące zawieszenie
- przód: linie debugowe wahaczy widoczne, koła ustawione szeroko
```

Jozz ocenia to jednoznacznie: „całkowicie źle". Z jego perspektywy (widzi
tylko obraz) — ma rację. To, co przechodziło walidację, wizualnie nie działa.

## 2. Dlaczego poprzednia próba poszła tak źle — rozliczenie procesu

To najważniejsza część. Bez szczerej diagnozy procesu naprawa się powtórzy.

```text
1. NIGDY NIE WYRENDEROWAŁEM I NIE ZOBACZYŁEM WYNIKU.
   Ani jednej klatki. Każde zdanie w raporcie M7 o wyglądzie ("model jedzie
   na żywym ramieniu", "co widzisz, to więzy solvera") było napisane NA ŚLEPO.
   "0 sokol errors" znaczy tylko tyle, że GPU nie crashnęło - nic o tym,
   czy obraz jest poprawny.

2. LICZBY DAŁY FAŁSZYWĄ PEWNOŚĆ.
   ~30 zielonych asercji testuje zgrubną dynamikę (jedzie, stoi pionowo,
   sag w zakresie, znak kontry). ŻADNA nie widzi: pozycji/orientacji/skali
   modelu, wysokości zawieszenia, czy koła są pod nadkolami. Rig wyglądający
   na zdemolowany przechodzi je wszystkie.

3. TESTY I KOD Z TEGO SAMEGO BŁĘDNEGO MODELU MYŚLOWEGO.
   Sam pisałem implementację i asercje - obie zakodowały te same błędne
   założenia (że korekta samego yaw wystarczy, że geometria trailing arm
   z kontraktu jest sensowna, że narysowanie całej bryły na ramieniu = "rig").
   Spójne wewnętrznie ≠ poprawne.

4. WDROŻYŁEM FEATURE, KTÓREGO WARUNEK KONIECZNY ODŁOŻYŁEM.
   Napisałem Ci, że model "jedzie na żywym ramieniu" (deklaracja zrigowania),
   a jednocześnie w "długach" wpisałem "rysowanie per-część - nie zrobione".
   Nie da się zrigować wieloczęściowego zawieszenia naklejając całą bryłę
   na jedno ciało. Oddałem jako gotowe coś niemożliwego obecnymi narzędziami.

5. ZBYT DUŻY ZAKRES NA DOSTĘPNĄ WERYFIKACJĘ.
   Pięć podsystemów naraz (wahacze-ciała, back-drivable rack, napęd momentem,
   ARB/aero, import+montaż modelu, przebudowa UI), a bramką były tylko liczby
   headless. Poprawność wizualna nie miała bramki, więc nie miała szans wyjść.

6. PEWNY JĘZYK ZAMASKOWAŁ NIEPEWNOŚĆ.
   "Co widzisz, to więzy solvera" było życzeniem, nie obserwacją. Ubrałem
   niesprawdzoną pracę w pewność.
```

## 3. Konkretne zdiagnozowane błędy (oparte na kodzie i danych, nie na oku)

### 3.1 Loader mesha rysuje bryłę sztywno na jednym ciele — to nie jest rig
`JozzVehicleVisualMesh::LoadStaticGltf` bierze `mesh[0].primitives[0]` i JEDEN
węzeł, ignoruje skin. `LoadMountVisual` dokłada korektę **tylko yaw** i rysuje
CAŁY model na jednym ciele ramienia. Efekt: sztywna bryła, damper się nie
ściska, wspornik chassis jedzie z ramieniem — dokładnie to, co na zdj. 3.
„Zrigowanie" wymaga wiązania KAŻDEJ części modelu do odpowiedniego ciała/jointu.

### 3.2 Fizyka przeczy własnej osi ruchu modelu (kluczowe odkrycie)
Sockety `One_Sided_wheel_mount` z audytu (BU):
```text
Socket_WheelCenter          [-1.1875, 0.5,  -0.0625]
Socket_ChassisMount         [ 0.0156, 1.625, -0.453]
Axis_SuspensionTravel_Top   [-1.1875, 1.5,   0.0]
Axis_SuspensionTravel_Bottom[-1.1875,-0.5,   0.0]
Socket_DamperUpper_L/R      [ 0.047, 1.844, ±0.8125]
Socket_DamperLower_L/R      [-0.719, 0.031, ±0.8125]
```
Oś ruchu Top→Bottom ma **stałe x i z, zmienia się tylko y** = koło ma jechać
PIONOWO względem podwozia (kolumna/ślizg). M7 zbudował z tego **wahacz wleczony
na revolute** = ruch po ŁUKU. Fizyka jest niezgodna z intencją modelu — dlatego
model naklejony na ramię wygląda źle w ruchu.

### 3.3 Skala jest spójna, ale wysokość zawieszenia za duża
Oba modele: `metersPerBlockbenchUnit = 0.35`. Koło: promień 0.514 m
(Ø 1.03 m). `restDrop = 0.55`, więc dno podwozia siedzi ~0.71 m nad ziemią —
prześwit prawie równy promieniowi koła. Stąd „unoszenie się" ze zdj. 2.
To parametr do strojenia OKIEM, nie crash, ale czyta się jako zepsute proporcje.

### 3.4 Model jest asymetryczny — proste lustrzenie z-flip jest podejrzane
Chassis mount ma z=-0.453 (jedna strona), dampery są parzyste (±0.8125).
M7 lustrzy z-flipem dla lewej/prawej — dla asymetrycznego wspornika to nie
musi dać sensownej pozy. Wymaga świadomej decyzji, nie automatu.

## 4. Zasada naczelna naprawy — RENDER JEST BRAMKĄ

```text
Nic nie jest "zrobione", dopóki nie zostało ZOBACZONE.
- ja: przez zrzut, który mogę odczytać (obraz) i/lub numeryczny szkielet
- Jozz: ostateczne oko na modelu i proporcjach
Liczby (walidator) wracają jako STRAŻNIK REGRESJI, nigdy jako definicja
"działa". Jedno koło, w spoczynku, poprawne — zanim cokolwiek się rusza
albo zwielokrotnia do czterech.
```

## 5. Plan etapowy (każdy etap kończy się OBEJRZANYM obrazem)

### Etap 0 — pętla wizualna (to, czego brakowało)
- **0a Szkielet numeryczny (tanie, bez GPU, w pełni pod moją kontrolą):**
  headless dump wszystkich punktów świata w spoczynku (róg podwozia, środek
  każdego koła, pivoty/ball jointy, końce dampera, oraz socket-world modelu
  tak jak byłby rysowany). Rzutuję to na SVG (widok z góry + z boku) i
  OGLĄDAM. To by wyłapało pływające podwozie i model-nie-na-środku-koła.
- **0b Zrzut klatki z silnika (prawda ostateczna):** dodać `--screenshot
  <plik.png>` do samples.exe (offscreen pass + readback + zapis PNG), żebym
  odczytał realny render. Jeśli okaże się zbyt oporne w sokolu — fallback:
  Jozz robi zrzut na każdym kamieniu milowym. Silnik NIETYKANY (dodatek po
  stronie hosta sampli).

### Etap 1 — statyczna prawda jednego narożnika (BRAMKA)
Minimalna scena: statyczna kotwica podwozia, JEDEN narożnik w spoczynku,
koło, model mountu. Zero jazdy, zero sił. Zweryfikować OKIEM:
```text
- koło we właściwym miejscu i rozmiarze
- Socket_WheelCenter modelu pada DOKŁADNIE na środek koła fizyki
- Socket_ChassisMount modelu pada na pivot na podwoziu
- pełna orientacja z ≥2 socketów + hint "up" (NIE sam yaw)
- damper pionowo, geometria zgodna z osią ruchu modelu
```
Dopiero gdy to JEDNO koło wygląda dobrze — dalej.

### Etap 2 — rig per-część (właściwe „zrigowanie")
Rozszerzyć loader, by ładował model jako OSOBNE części po węzłach
(wspornik chassis, korpus dampera, tłoczysko, ramię/upright, piasta) i
związać transform każdej części z odpowiednim ciałem:
```text
wspornik chassis   -> podwozie
ramię/upright      -> ciało prowadzące koło
damper górny       -> podwozie, damper dolny -> ramię (rozciąga się między)
```
Bramka wizualna PER CZĘŚĆ.

### Etap 3 — geometria fizyki zgodna z modelem
Przebudować narożnik tak, by kinematyka odpowiadała osi ruchu modelu
(pionowa/ślizg dla tego modelu — patrz 3.2), z hardpointami z jego socketów
(pivot, linia dampera, środek koła). Wtedy model jedzie zgodnie z prawdą.

### Etap 4 — wysokość, skala, proporcje OKIEM
Stroić restDrop / track / rozmiar podwozia tak, by auto w spoczynku
wyglądało dobrze (koła w nadkolach, sensowny prześwit). Dodać wizualne
podwozie albo choć zdrowo dobrać box.

### Etap 5 — replikacja na cztery narożniki + poprawne lustrzenie
Świadome lustrzenie asymetrycznego modelu, ponowna weryfikacja wizualna.

### Etap 6 — dopiero teraz z powrotem dynamika + walidator jako regresja
Włączyć jazdę, przepuścić suitę numeryczną jako STRAŻNIKA regresji, i
ZOBACZYĆ jazdę (koła w nadkolach pod obciążeniem, auto stoi poziomo).

## 6. Co zostaje z M7, a co przebudować

```text
ZOSTAJE (dynamika, osobno walidowana - ale też do obejrzenia w ruchu):
  wahacze-ciała z limitami (fix rozpadu), back-drivable rack, napęd momentem,
  ARB/aero. To były realne poprawy fizyki.
PRZEBUDOWA (warstwa wizualna + kinematyka narożnika):
  loader per-część, montaż modelu z pełną orientacją, kinematyka zgodna z osią
  ruchu modelu, wysokość/proporcje, lustrzenie.
```

## 6a. Postęp implementacji (2026-07-06, po decyzjach Jozza)

### Oczy (Etap 0a) — dałem sobie wzrok bez GPU
Napisałem w Pythonie (scratchpad) enkoder PNG na czystym `zlib` + rzutnik
ortograficzny. Wyrenderowałem model do PNG i **pierwszy raz go zobaczyłem**.
To samo narzędzie wyłapie każdy błąd geometrii/placementu w przyszłości.

### Rozłożenie szkieletu — model jest RIGID (kluczowe)
`One_Sided_wheel_mount.gltf`: 1 mesh, 1 prymityw (456 v), skin z 4 kośćmi
niosącymi geometrię. **Każdy wierzchołek należy do DOKŁADNIE jednej kości
z wagą 1** (potwierdzone pomiarem). Znaczy to: model można renderować jako
4 sztywne części, każdą na transformacie swojej kości — bez miękkiego
skinningu, idealnie pod obecny renderer sztywnych meshy. Podział kości Jozza:
```text
ChassisMount   216 v  główny korpus ramienia + coilover (strona podwozia)
WheelCenter     96 v  piasta/zwrotnica (przy kole)
Chassis_Top     72 v  wspornik górny (do podwozia)
Chassis_Bottom  72 v  wspornik dolny (do podwozia)
```

### Diagnoza orientacji — potwierdzona i naprawiona
Ramka autorska modelu: **X = oś ramienia** (koło na -X, mocowanie do podwozia
na +X), **Y = w górę**, **Z = poprzeczna** (dampery ±Z). W aucie lewe koło
potrzebuje ramienia wzdłuż osi BOCZNEJ auta (Z), a model ma je wzdłuż X →
potrzeba **yaw -90° o Y** dla lewego narożnika. M7 tego nie zrobił.
Zweryfikowane wizualnie w Pythonie: po obrocie model owija koło poprawnie
(piasta na środku, ramię do wewnątrz, dampery przód/tył). Bug był WYŁĄCZNIE
w placemencie — obecny loader ładuje CAŁY model (nie brakuje geometrii).

Transformacja (zweryfikowana, znaki sprawdzone):
```text
yaw = quat(+Y, -90 deg)          # lewy narożnik
placement.q = yaw
placement.p = wheelCenterWorld - yaw * WheelCenterAuthored
# rysuj model: DrawAtTransform(placement)  -> WheelCenter socket pada na środek koła
```

### Etap 1 — stół zawieszenia (zaimplementowany, czeka na oczy)
Nowy sample **"Jozz Vehicle / M8 Suspension Rig Bench"**
(`samples/jozz_vehicle_m8_rig_bench.*`): jeden narożnik, brak jazdy. Statyczna
kotwica podwozia + koło o wysokości/obrocie na suwakach + model z poprawną
transformacją. Overlay socketów: biały krzyż = środek koła fizyki, ZIELONY
krzyż = socket WheelCenter modelu (muszą się pokrywać), cyjan = reszta
socketów. Suwak yaw jako live-safety-check (default -90). Build zielony,
boot smoke 0 sokol errors. **Placement w C++ jest identyczny z zweryfikowanym
w Pythonie** (te same wierzchołki authored-world-meters, ten sam obrót).
Status: czeka na potwierdzenie w silniku (zrzut Jozza lub mój --screenshot).

### Następny krok techniczny — zrzut klatki (Etap 0b)
Backend to D3D11. Mam `sg_d3d11_query_image_info` (daje ID3D11Texture2D) i
podlinkowany WIC (zapis PNG). `RenderFrameOffscreen` już istnieje. Zbuduję
`--screenshot <plik>` w hoście sampli (osobna, skupiona tura — to wersjowany
plumbing sokola, nie chcę go sklecić). Wtedy każdy kolejny krok weryfikuję
sam, bez zależności od obecności Jozza.

## 6b. Etap 1 POTWIERDZONY + Etap 2/3 (2026-07-06, sesja 2)

**Etap 1 potwierdzony wizualnie przez Jozza:** zielony krzyż (socket
WheelCenter) leży na białym (środek koła), model owija koło, ramię do
wewnątrz auta. Pierwszy element rigu ZOBACZONY i potwierdzony, nie oddany
na słowo. yaw -90° dla lewego narożnika = poprawny.

**Etap 2 — loader per-część (zrobione):** `JozzVehicleRiggedMesh` w
`jozz_vehicle_visual_mesh.*`. Model skinowany RIGID → grupuję trójkąty po
dominującej kości (mapując JOINTS_0 → skin.joints → nazwa node'a) i rejestruję
osobny mesh na część. Geometria w authored-world-meters (jak whole-model),
więc `DrawPart(transform)` działa identycznie. `LoadStaticGltf` NIETKNIĘTY.
Cztery części: ChassisMount / WheelCenter / Chassis_Top / Chassis_Bottom.

**Etap 3 — artykulacja (zaimplementowane, czeka na oczy Jozza):**
Kinematyka zgodna z pionową osią ruchu modelu. Piasta (WheelCenter) jedzie
z kołem (rest placement + offset skoku), wsporniki (Chassis_Top/Bottom)
zostają na podwoziu. Ramię (ChassisMount) = przełącznik chassis/wheel
w labie (do oceny co wygląda lepiej). **Przy travel=0 wszystkie części =
potwierdzona poza Etapu 1** (kotwica bezpieczeństwa). Build zielony, boot
smoke 0 błędów.

Znany limit (do decyzji Jozza): coilover jest jedną sztywną kością z ramieniem,
więc nie teleskopuje. Pełne teleskopowanie = rozbić kość dampera na górną
(chassis) i dolną (ramię) w Blockbenchu; wtedy rig automatycznie to podchwyci.

## 6c. Poprawki po feedbacku Jozza (sesja 2, iteracja 2)

Feedback ze zdjęć (najważniejsze wnioski):
```text
1. arm follows wheel = GORZEJ (coilover odlatuje od podwozia) -> zostaje OFF
2. blok podwozia zasłania -> usunięty
3. KLUCZOWE: zawieszenie wchodziło w GEOMETRYCZNY ŚRODEK koła, a miało
   dochodzić do Socket_WheelMount z modelu koła (płaszczyzna montażu)
```

Naprawa #3 (istota): piasta modelu (Socket_WheelCenter) wpinana była w środek
bryły koła (origin ciała = środek szerokości opony). Poprawka: wpinam ją w
**wheel Socket_WheelMount**, który wg audytu leży **0.131 m do wewnątrz**
(wzdłuż osi obrotu; półszerokość opony 0.219 m). Punkt liczony solidnie przez
faktyczną transformację rysowania koła (`b3TransformWorldPoint(wheelDraw,
mpbu*SocketWheelMountBU)`), więc kierunek (do wewnątrz) wychodzi z korekty
koła, nie z hardcode. Zweryfikowane: wyprowadzenie macierzowe daje +0.131 m
w +Z (do wewnątrz dla lewego), i render offline potwierdza — piasta ląduje
na wewnętrznej krawędzi koła, środek opony zostaje osobno.

Overlay stołu zaktualizowany: szary = środek koła, ŻÓŁTY = wheel
Socket_WheelMount (cel montażu), ZIELONY = model WheelCenter (musi paść
na ŻÓŁTY).

## 6d. System zrzutów + testowania (2026-07-07) — MOJE OCZY

Zbudowany na prośbę Jozza. Backend to D3D11; ta wersja sokola eksponuje tylko
`sapp_d3d11_get_swap_chain()`, więc: swap chain → backbuffer (buffer 0) →
device/context z backbuffera → staging texture → Map → WIC PNG. Moduł
`samples/host/screenshot.cpp` (Windows-only, WIC już podlinkowany).

```text
samples.exe --sample-name "M8 Suspension" --frames 80 --screenshot plik.png
  --screenshot <plik>   przechwyt ostatniej klatki (z panelem ImGui) -> PNG
  --sample-name <substr> wybór sampla po nazwie (odporny na przesunięcia indeksu)
Stół M8 czyta ENV do pozowania bez UI:
  JOZZ_BENCH_TRAVEL, JOZZ_BENCH_WHEEL(0/1), JOZZ_BENCH_ARMFOLLOW(0/1),
  JOZZ_BENCH_CAM="yaw,pitch,radius,px,py,pz"
```

Zweryfikowane MOIMI oczami (nie proxy): rig renderuje się jako spójna rama
zawieszenia; przy travel 0 poza = potwierdzona; przy travel -0.2 piasta +
sockety strony koła (zielony/szary/cyjan) zjeżdżają, wsporniki + sockety
strony podwozia (pomarańczowy) zostają. Overlay socketów naprawiony
(per-rola: wheel_center/cardan_hub/damper_lower/travel_axis.bottom jadą z
kołem, reszta z podwoziem). Szpara hub↔rama przy dużym skoku = znany limit
jednej kości ramię+coilover (telescoping po podziale kości dampera).

## 6e. PEŁNE ZRIGOWANE AUTO (2026-07-07, sesja autonomiczna)

Cel Jozza: jak najbliżej w pełni zrigowanego samochodu, autonomicznie,
weryfikacja moimi zrzutami. Zrobione w 4 fazach:

```text
Faza 1  Stół z PRAWDZIWĄ sprężyną: dynamiczne koło na prismatic (spring/damper)
        do statycznego chassis, target=sin -> buja się samo. Rig śledzi ŻYWE
        ciało koła. Zrzuty: travel +0.109 (hub w górze) / -0.150 (hub w dole),
        wsporniki stoją. ZWERYFIKOWANE.
Faza 2  Lustro w rigged loaderze: LoadSkinnedGltf(..., mirrorX) neguje X
        wierzchołków+normalnych i odwraca winding (osobny hash mesha). Wyprow.:
        negacja X + yaw -90 = reflectZ odbicie dla prawej strony.
Faza 3  Model na ŻYWYM aucie M6 (jozz_vehicle_m6_rig_lab): wszystkie 4 narożniki
        = DOUBLE_WISHBONE (mają knuckle/upright). SetupMountRig() piecze per
        narożnik 2 transformy lokalne: wsporniki wzgl. chassis, hub wzgl.
        knuckle. Live: hub jedzie z knuckle (skok + skręt, BEZ obrotu koła),
        wsporniki+ramię z chassis. Lewe narożniki = mesh authored, prawe =
        mesh mirror. Auto osiada na fizyce, 4 koła w kontakcie. Jeżdżące
        (napęd momentem/skręt/ARB z M7 nietknięte).
Faza 4  validator OK, test.exe OK, boot smoke 95/96/97/98 = 0. Zrzuty auta z
        wielu kątów (bok, wnętrze bez kół pokazuje modele na 4 rogach).
```

Narzędzia testowe (env do M6 labu): `JOZZ_M6_CAM`, `JOZZ_M6_DIAG`,
`JOZZ_M6_WHEEL` — kadrowanie/ukrywanie do zrzutów headless.

Stan: **pełne zrigowane auto** (chassis + 4 koła + modele zawieszenia na 4
rogach, prawdziwa fizyka, jeżdżące). Znany dług: coilover nie teleskopuje
(1 sztywna kość — Jozz rozbije kość dampera), skala/proporcje modelu do
dopieszczenia okiem, model chwilowo chowa się za dużym kołem.

## 6f. TELESKOPOWY DAMPER (2026-07-07, po feedbacku o sztywnych wspornikach)

Feedback Jozza: damper/wspornik sztywno przypięty do chassis; jedna strona ma
podążać za chassis, druga za kołem, środek się rozciągać. Model `Asset_Dumper`
(RIGID skin, 3 kości: Part_Upper y=0.525 / Part_Stretch y=0.172 / Part_Lower
y=-0.164, rest gap 0.689 m) to rozwiązuje.

```text
Loader: JozzVehicleRiggedPart dostał boneRestWorld + restMin/restMax.
Helper: JozzVehicleRiggedMesh::DrawTelescopingDamper(topWorld, botWorld):
  - Part_Upper: sztywno, bone -> topWorld (chassis)
  - Part_Lower: sztywno, bone -> botWorld (koło/knuckle)
  - Part_Stretch: scale.y = liveGap/restGap, pivot=boneRestWorld, pozycja na
    tej samej frakcji odcinka co w spoczynku -> exact przy rest
  - orientacja: b3ComputeQuatBetweenUnitVectors(authoredAxis, liveAxis) -
    damper przechyla się między mocowaniami
  - skalowanie wokół pivota BEZ zmiany geometrii: T = target - R*(scale∘pivot),
    scale osobno w AppendMesh (worldVert = R*(scale∘vert) + T)
Weryfikacja w stole (zrzuty): travel -0.18 damper dłuższy (sprężyna rozciągnięta),
  travel +0.14 krótszy (sprężyna zbita), tuleje trzymają końce. Renderuje się
  jak prawdziwy coilover ze sprężyną.
Na aucie M6: dumper na 4 narożnikach, top na chassis (nad+do wewnątrz koła),
  bot na knuckle. Przechylony realistycznie. validator OK, boot smoke 97/98 = 0.
```

Decyzja dla Jozza: One_Sided_wheel_mount ma WŁASNY sztywny coilover w kości
ChassisMount — teraz jest też osobny Asset_Dumper, więc mogą się dublować.
Opcje: (a) usuń coilover z One_Sided w Blockbenchu (dumper go zastępuje), (b)
zostaw oba, (c) ukryj część ChassisMount. Do decyzji okiem.

## 7. Pytania/decyzje dla Jozza przed startem

```text
1. Jaki typ zawieszenia reprezentuje One_Sided_wheel_mount? Jego oś ruchu jest
   PIONOWA (ślizg/kolumna). Czy tak ma się poruszać koło, czy model jest tylko
   "obudową" na dowolnej kinematyce? (Ty go modelowałeś - Twoja intencja wygrywa.)
2. Zgoda na dodanie --screenshot do samples.exe (etap 0b)? To daje mi oczy i
   kończy erę "oddawania na ślepo".
3. Czy zaczynamy od JEDNEGO narożnika statycznie (rekomendacja), czy wolisz
   inną kolejność?
```

## 8. Naprawa wahaczy (Chassis_Top / Chassis_Bottom) — 2026-07-07

Feedback Jozza: `Chassis_Top` i `Chassis_Bottom` to WAHACZE (górny/dolny), a były
rysowane SZTYWNO na chassis. Miały podążać za żółtą (górny) i pomarańczową (dolny)
linią suspension debug — czyli obracać się razem z zawieszeniem.

Analiza modelu (`analyze_parts.py` + render bind-pose) potwierdziła identyfikację:
- `Socket_WheelCenter` (X[-0.416,-0.262], Y[0.022,0.328]) = PIASTA/upright -> knuckle.
- `Chassis_Top` (X[-0.284,0.175], Y≈0.34, cienka płyta) = GÓRNY WAHACZ.
- `Chassis_Bottom` (X[-0.284,0.175], Y≈0.01, cienka płyta) = DOLNY WAHACZ.
- `Socket_ChassisMount` (X[-0.016,0.357], Y[-0.03,0.668]) = wspornik chassis + wieże.
Oś ramienia w modelu = authored X (koło = -X dla mesha lewego / +X po mirrorze).

```text
Helper: JozzVehicleRiggedMesh::DrawPartBetween(idx, authoredA, authoredB,
        liveA, liveB, color):
  - authoredDir = authoredB - authoredA; ua = normalize (oś ramienia = authored X)
  - liveDir = liveB - liveA; ul = normalize
  - rotation = b3ComputeQuatBetweenUnitVectors(ua, ul)
  - scale wzdłuż osi ramienia: s = |liveDir|/|authoredDir|, skala tylko na tej osi
  - DrawPartScaled(idx, rotation, scale, pivot=authoredA, target=liveA)
  => authoredA ląduje na liveA, authoredB na liveB; płyta obraca się i rozciąga.

M6 lab Render (per narożnik, z żywych ciał każdą klatkę):
  hp = runtime.hardpoints; wheelLocal = runtime.restWheelCenterLocal
  upperChassisW = chassisLive * midpoint(upperFrontChassis, upperRearChassis)
  upperBallW    = knuckleLive * (upperBallJoint - wheelLocal)
  lowerChassisW = chassisLive * midpoint(lowerFrontChassis, lowerRearChassis)
  lowerBallW    = knuckleLive * (lowerBallJoint - wheelLocal)
  ArmEnds(part, wheelNegX): koniec-koła = X-ekstrem bliżej koła (min X lewy /
    max X prawy), koniec-chassis = drugi ekstrem, przy mid-Y/mid-Z płyty.
  Chassis_Top  -> DrawPartBetween(chassisEnd, wheelEnd, upperChassisW, upperBallW)
  Chassis_Bottom -> DrawPartBetween(..., lowerChassisW, lowerBallW)
  WheelCenter -> DrawPart(hubWorld)  (knuckle);  reszta -> DrawPart(bracketWorld)
```

KLUCZOWE: wahacze i linie debug liczone z TYCH SAMYCH hardpointów i tych samych
żywych transformów -> matematycznie nie mogą się rozjechać w ruchu. Jeśli nakrywają
się w spoczynku, nakrywają się przy każdym skoku.

Weryfikacja (zrzuty, env `JOZZ_M6_ARMTINT=1` barwi Top na czerwono, Bottom na
niebiesko; `JOZZ_M6_DUMPER=0`, `JOZZ_M6_WHEEL=0`, `JOZZ_M6_DIAG=1`):
- Nakrycie w spoczynku: czerwona płyta leży na żółtej linii, niebieska na pomarańczowej.
- RUCH: klatka 8 (auto opada, zawieszenie rozciągnięte) vs klatka 140 (osiadłe,
  ściśnięte) — cały narożnik obraca się (camber gain), a płyty i linie przechylają
  się RAZEM pod tym samym kątem. Wahacz artykułuje z fizyką.
- validator OK, boot smoke 97 = 0 sokol errors.

Nowe env M6 labu (do zrzutów bez UI): JOZZ_M6_DUMPER (0/1), JOZZ_M6_MOUNT (0/1),
JOZZ_M6_ARMTINT (0/1 — tint wahaczy do porównania z liniami).

DEFERRED (Jozz: "narazie tego nie poprawiaj"): dwa dampery na zawieszenie, z boku,
poza zawieszeniem, nie zasłaniające chassis (ref z Blockbench). Obecny pojedynczy
teleskopowy damper zostaje jak jest.

## 9. Symetria wahaczy + wchodzenie w oponę + hertz — 2026-07-07

Feedback Jozza (screeny front): (a) lewy i prawy wahacz mają "różną wagę", rig
krzywy; (b) wahacze offset/za daleko, wchodzą w oponę; (c) testy spring hertz 3.5/6;
(d) do-to-do: system 4 widoków w jednym obrazie.

**Narzędzie diagnostyczne najpierw** (koniec eyeballingu gęstych renderów):
- `tools\quad_shot.ps1` — 4 widoki (front/tył/bok/3-4) zszyte w 1 PNG (System.Drawing).
- `JOZZ_M6_DUMP=1` — wypis TWARDYCH LICZB geometrii narożników (wheelC, upBall/loBall,
  chassisMount, tireZband) na klatce 140. To rozstrzygnęło diagnozę.

**(a) Asymetria — była w RENDERZE, nie w fizyce.** Dump pokazał fizykę IDEALNIE
symetryczną (FL vs FR czysty Z-mirror: upBall ∓0.877, loBall ∓0.927, wszystkie
hardpointy lustrzane). Winowajca: `DrawPartBetween` używał rotacji minimalnej
(`b3ComputeQuatBetweenUnitVectors`), która zostawia ROLL wokół osi ramienia wolny
i rozwiązuje go inaczej dla lewej (authored) i prawej (mirror) strony -> płyty
skręcały się nierówno. FIX: pełny "płaski" układ zamiast minimalnej rotacji —
oś długa->kierunek ramienia, szerokość->wzdłuż auta (world X), twarz->góra,
budowany identycznie z ua/ul (`b3MakeQuatFromMatrix` z bazy [t1 upT wT]·[ua upA wA]^T).
Mirror wejścia -> mirror wyniku. Zweryfikowane: front z tintem symetryczny.

**(b) Wchodzenie w oponę.** Dump: loBall Z=∓0.927, wewn. ścianka opony Z=∓0.823
(opona R=0.514 W=0.438, wheelC Z=∓1.042). Sworznie są 8-10 cm ZA wewn. ścianką =
wewnątrz szerokości opony. To FIZYCZNIE poprawne (upright/sworznie siedzą w kole),
a widok z czystego przodu potwierdza że wahacz NIE przebija zewn. ściany opony.
Ale opona jest szeroka (offroad) i solidny mesh wahacza przecina wewn. ściankę z
ukosu; pogarszał to wcześniejszy asymetryczny skręt (już naprawiony). Dodany
WIZUALNY (fizyka nietknięta) `TuckZ` + `JOZZ_M6_TUCK` (domyślnie ON): chowa
rysowany koniec wahacza I piastę do wewn. ścianki opony (|z| <= |wheelZ|-halfW),
żeby mount schował się za kołem. Kompromis: z diagnostyką ON wahacz kończy się
przy oponie, nie sięga do końca żółtej/pomarańczowej linii (do decyzji Jozza).

**(c) Spring hertz** (env `JOZZ_M6_HERTZ`): 3.5 -> chassisY 0.850 (miękko, niżej,
więcej skoku); 6.0 -> chassisY 0.985 (sztywno, +13.5 cm, mniej ugięcia). Default
4.26. Oba stabilne. Slider "Spring hertz" w zakładce Susp do jazdy próbnej.

validator OK, boot smoke 97 = 0 sokol errors.

## 10. Wahacze wpięte w authored-sockety (nie w hardpointy fizyki) — 2026-07-07

Kluczowe doprecyzowanie Jozza: `Chassis_Top`/`Chassis_Bottom` mają ZACZYNAĆ się
tam gdzie w modelu ŹRÓDŁOWYM — jeden koniec nachodzi na `Socket_ChassisMount`,
drugi dotyka `Socket_WheelCenter`. "Od dawna nie umiemy poprawić tego małego offsetu."

Diagnoza błędu: wpinałem końce wahaczy w HARDPOINTY FIZYKI (sworznie kulowe,
midpoint pickupów chassis). To INNE miejsca niż authored-sockety modelu -> stąd
offset i wchodzenie w oponę. Zły punkt zaczepienia od początku (M8 §8 tuck to
było leczenie objawu, nie przyczyny).

FIX: koniec-chassis wahacza = `b3TransformPoint(bracketWorld, authoredChassisEnd)`,
koniec-koła = `b3TransformPoint(hubWorld, authoredWheelEnd)`, gdzie bracketWorld/
hubWorld to DOKŁADNIE te same transformy co rysują Socket_ChassisMount i
Socket_WheelCenter. Authored X wahacza (lewy: chassis=+0.175, wheel=-0.284) wpada
w bbox obu sąsiadów (ChassisMount X[-0.016,0.357], WheelCenter X[-0.416,-0.262]),
więc końce STYKAJĄ się z nimi bez offsetu. Wahacz nadal się gnie (hubWorld jedzie
z knuckle przy skoku). Usunięty cały mechanizm tuck (JOZZ_M6_TUCK, TuckZ) — zbędny.

UWAGA: wahacze NIE nakrywają już żółtej/pomarańczowej linii debug (te idą do
sworzni fizyki). To OK — najnowsza intencja (trzymaj się modelu) zastępuje
wcześniejsze "podążaj za liniami". Zweryfikowane zrzutami: front symetryczny,
zespół hub↔wspornik spójny, koła bez przebić, artykulacja klatka 8 vs 150 działa.
validator OK. Spring hertz default zostaje 4.26 (decyzja Jozza).

## 11. Opadające wahacze — poza jako fundament (nie przypadek fizyki) — 2026-07-07

Feedback Jozza (2 screeny): wahacze wyginają się DO GÓRY zamiast opadać w dół do
kół (jak w Blockbench referencji). Zażądał pełnego, przemyślanego systemu pozy
zawieszenia (ride height, kąt wahaczy, limity skoku) jako FUNDAMENTU pod drift,
offroad i ciężarówki — nie kosmetyki.

**Diagnoza (dump geometrii, nie zgadywanie):** przy spoczynku mocowania wahaczy
na chassis były NIŻEJ niż sworznie przy kole (upBall Y=0.691 > upChM Y=0.615) —
auto pod ciężarem siada poniżej neutralnej pozycji sprężyny, więc wahacze ZAWSZE
idą w górę względem punktu zaczepienia. Sam hertz/prześwit tego nie odwraca —
sprężyna bez preloadu nigdy nie unosi auta POWYŻEJ swojej długości spoczynkowej.

**Dwa nowe parametry fizyki (fundament pozy):**

1. `JozzVehicleM6WishboneGeometry::restArmDroopDeg` — podnosi mocowania wahaczy
   na chassis PIONOWO nad sworznie (zasięg poziomy = armLength bez zmian, więc
   tor kół i geometria kierownicy NIE są ruszane samym tym parametrem). Wahacz
   nachylony w dół do koła w pozie projektowej.
2. `JozzVehicleM6Config::suspensionPreload` — coilover ma rest length = designLength
   + preload (zamiast = designLength). To NAPRAWIA "za prosty damper": bez tego
   poza była przypadkowym skutkiem równowagi sprężyna/ciężar; z preloadem poza
   jest ŚWIADOMYM ustawieniem — auto osiada DOKŁADNIE w pozie projektowej
   (drooped arms, poprawny tor, toe≈0) niezależnie od hertz/masy. Travel stopy
   (compressionTravel/reboundTravel) liczone od designLength (NIE od restLength
   z preloadem), więc zostają zakotwiczone w prawdziwej pozie projektowej.

**KRYTYCZNE odkrycie (bump steer):** drążek kierowniczy (przód: zębatka; tył:
toe link) jest poziomy w oryginalnej geometrii. Przy droopie dolny wahacz
pochyla się, a drążek NIE — więc przestają być równoległe = klasyczny bump
steer. Pierwsza próba (droop 25°) to pokazała brutalnie: kierownica 69°/25°
zamiast 32°/32° (przełożenie prawie podwojone), walidator FAILED. Naprawione
przez `SteeringLinkDroopLift()` — podnosi wewnętrzne piwoty drążka (zębatka i
toe link) o `lowerArmLength·tan(droop)`, więc drążek zostaje równoległy do
dolnego wahacza przy KAŻDYM kącie droop. Po naprawie: kierownica 36.2°/30.5°
(bliskie 32° projektu), toe≈0.

**Granica fizyczna (zmierzona, nie założona):** droop 15° = w pełni stabilny,
deterministyczny w 3 powtórzeniach (walidator OK, camber lądowania 0.6-0.8°
przy 2m/3.5m). droop 18-20° = wchodzi w reżim over-center trapezu Ackermanna
(ten sam mechanizm co `ackermannFraction` — opisany w kodzie: geometria pcha
wewnętrzne koło w martwy punkt przy pełnym skręcie) — NIEDETERMINISTYCZNE w
testach (raz przechodzi, raz camber 13.6° przy 3.5m). **15° to bezpieczny,
zweryfikowany sufit dla TEJ geometrii** — UI go klamruje na 16° z ostrzeżeniem.
Dalsze podniesienie wymagałoby przeprojektowania geometrii Ackermanna
(steeringArmBack, ackermannFraction) pod większy droop — odłożone, nie w tej
rundzie.

**Live tuning bez rebuildu:** `b3DistanceJoint_SetLength`/`SetLengthRange`
pozwalają zmieniać ride height (preload) i skok bump/droop NA ŻYWO, bez
niszczącego rebuildu pojazdu (w przeciwieństwie do `restArmDroopDeg`, który jest
zapieczony w hardpointach i wymaga Apply). `JozzVehicleM6CornerRuntime::coiloverDesignLength`
cache'uje długość projektową per narożnik; `ApplySuspensionTuning()` liczy
target/zakres od niej. Trailing arm skaluje przez `trailingMotionRatio` (wheel-
space -> damper-space), tak jak już robił dla travel stopów.

**UI (zakładka Susp, sekcja "Default pose" na górze):**
- Arm droop (0-16°, structural/Apply) — z opisem progu over-center.
- Ride height / preload (live, -0.10..0.25 m) — z opisem "jak preload w
  regulowanym coilowerze"; zmiana hertz/masy lekko przesuwa osiadłą wysokość
  (realny odpowiednik — prawdziwe coilovery mają tę samą właściwość).
- Compression travel (bump) / Rebound travel (droop) — teraz LIVE, osobno.

Domyślne wartości: restArmDroopDeg=15°, suspensionPreload=0.07 (dostrojone tak,
by chassisY≈1.068, tor≈1.047 wobec projektu 1.05 — toe≈0 z fabrycznymi
ustawieniami). Nowe env do testów headless: JOZZ_M6_PRELOAD, JOZZ_M6_DROOP.

validator OK (3x powtórzone, deterministyczne), test.exe OK, boot smoke 0 errors.

DEFERRED: pełne przeprojektowanie Ackermanna pod droop >16° (dla bardziej
agresywnej pozy jak w referencji Blockbench) — wymaga strojenia
steeringArmBack/ackermannFraction razem z droopem, osobna runda.
