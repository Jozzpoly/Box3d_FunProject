# Plan: fundamentalna przebudowa mapy i terenu

Data: 2026-07-11. Autor planu: Fable 5 (sesja planistyczna). Zleceniodawca: Jozz.
Status: **ZAAKCEPTOWANY przez Jozza (2026-07-11, wraz z rozmiarami §5 i
propozycjami P1–P9)** — track wchodzi PRZED edytor rigu (decyzja Jozza
2026-07-11: „zanim zaczniemy długą drogę z edytorem rigu"). **Etap 1
ZAMKNIĘTY** (fundament terenu — patrz `MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md`).

Dokumenty etapów (czytaj PRZED implementacją danego etapu — tam są szczegóły,
pułapki i dokładne miejsca w kodzie):

- Etap 1 → `MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md`
- Etap 2 → `MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md`
- Etap 3 → `MAPA_ETAP_3_TOR_I_DRIFT_PL.md`
- Etap 4 → `MAPA_ETAP_4_PLAC_FIZYKI_PL.md`
- Etap 5 → `MAPA_ETAP_5_SPAWNER_I_STRESS_PL.md`
- Etap 6 → `MAPA_ETAP_6_NAWIGACJA_POMIAR_POLISH_PL.md`

---

## 1. Feedback Jozza (2026-07-11) — co dokładnie zamówił

1. **Co najmniej 2× większy płaski teren.**
2. **5–10× większy teren „górzysty"**, dużo bardziej zróżnicowany: „jakby
   połączyć ze sobą trzy różne skale szumu" — miejscami bardzo nierówny
   i wysoki, miejscami gładki i płaski. Solidny, „niemalże jak prawdziwy"
   teren pod twarde testy offroadu.
3. Teren offroad może być **osobnym chunkiem obok płaskiego**; obecny wystaje
   ponad płytę (widoczne na screenie) — nowy ma być **odrobinę POD głównym
   terenem**, żeby ładnie na siebie nachodziły.
4. **Dużo szczegółowsze i realistyczniejsze bumpery/skocznie**: różne typy,
   wielkości, kształty, różne ostrości i zaokrąglenia.
5. **Mądrze zaprojektowane miejsca do testowania zawieszeń w twardych
   warunkach** — obecne są niewystarczające i pogarszają jakość realtestów.
6. Płaski teren jest monotonny — po powiększeniu zaprojektować coś, co
   przypomina **tor wyścigowy i tor do driftu**.
7. **Więcej rzeczy z fizyki core box3d** na mapie — przemyśleć, co niesamowitego
   w box3d pasuje do projektu.
8. **Dużo więcej obiektów fizycznych**: od bardzo małych po bardzo duże, różne
   kształty; **opcja w menu do spawnowania kolejnych** (stress-testy dużej
   ilości).
9. Rozwinąć feedback o dodatkowe pozycje, jeśli analiza je ujawni (§4).

## 2. Stan obecny i jego wady (screeny + kod)

| Element | Stan (kod) | Wada |
|---|---|---|
| Płyta | box 240×240 m, top y=0 (`jozz_vehicle_m6_rig_lab.cpp:29-40`) | pusta, monotonna; za mała na tor |
| Teren „górzysty" | `b3CreateWave(28,28,{1.6,0.30,1.6},0.07,0.10)` = ~43×43 m, JEDNA sinusoida ±0.3 m (`jozz_vehicle_m5_test_course.cpp:134`) | 3% powierzchni mapy; zero zróżnicowania (jedna skala szumu); żadnych wzniesień ani polan |
| Styk terenów | heightfield posadzony **+0.35 m NAD płytą** (`m5_test_course.cpp:127`) | widoczny próg/uskok na wjeździe (screen 2) — dokładnie to, co Jozz wytknął |
| Skocznie | 4 pochylone boxy 3–4 m, kąty 8–16° (`m5_test_course.cpp:113-116`) | tylko proste klinki, brak zaokrągleń, brak progresji, za małe na twarde testy |
| Washboard | 2 rzędy po 6 poprzeczek (`:119-120`) | jedyny „poligon zawieszenia"; wszystko rozrzucone wokół spawnu bez ładu |
| Propy | 14 sztuk (8 skrzynek, 6 kul), jeden przedział rozmiarów (`:96-104`) | mało, jednorodne, brak spawnera, brak stress-testów |
| Pomiar | brak | żadnych bramek czasowych, telemetrii nawierzchni, punktów odniesienia |
| Nawigacja | brak | przy większej mapie dojazd do stref zje czas realtestów |

## 3. Analiza krytyczna

- Obecna mapa powstała jako **scenografia M5** („scatter of props") i nigdy nie
  była projektowana jako narzędzie testowe. Wymagania Jozza to de facto zmiana
  klasy obiektu: z dekoracji na **poligon pomiarowy + plac zabaw fizyki**.
- Największy błąd konstrukcyjny do naprawy w fundamencie: teren nierówny jako
  łatka NA płycie. Poprawny model: **osobny chunk heightfield PRZYLEGAJĄCY do
  płyty, schowany pod nią w pasie zakładki** (silnik wprost to wspiera:
  `b3HeightFieldDef.globalMin/MaximumHeight` istnieje po to, „żeby heightfieldy
  kładzione obok siebie zgrywały się idealnie" — `types.h:2237-2242`).
- `b3CreateWave` nie nadaje się na docelowy teren (jedna częstotliwość).
  Piszemy **własny deterministyczny generator FBM (3 oktawy + maska
  płaskości + gradient trudności)** i karmimy `b3CreateHeightField` własną
  tablicą wysokości — dokładnie „trzy różne skale szumu" z feedbacku.
- Zaokrąglenia przeszkód: jedyny naprawdę okrągły prymityw dynamiczno-statyczny
  to **kapsuła** — idealna na progi, muldy (whoops), bale i zaokrąglone krawędzie
  natarcia skoczni (capsule lip). Ostre wersje robimy z transformowanych boxów.
  To daje pełne spektrum „ostrości i zaokrągleń" bez dotykania core.
- Płyta budowana od Etapu 1 jako **siatka 3×3 kafli** (zamiast 1 boxa): kafel =
  przyszła strefa (drift/lód może dostać inne tarcie CAŁEGO kafla albo
  sub-kafla) — zero nakładek, zero progów, topy idealnie równe. Koszt: ~0.
- Wielkości: „2×" i „5–10×" traktujemy z zapasem, ale z bramką wydajności
  w Etapie 1 (liczby niżej, §5).

## 4. Rozszerzenia feedbacku (propozycje — domyślnie WCHODZĄ, Jozz wykreśla)

| # | Propozycja | Po co | Etap |
|---|---|---|---|
| P1 | **Teleporty pojazdu** do stref (combo + hotkeye) | duża mapa bez tego marnuje czas realtestów | 1 (min.), 6 (pełne) |
| P2 | **Płyta wibracyjna 4-post (shaker)** — kinematyczna, sin/sweep amplituda+częstotliwość | deterministyczny, powtarzalny test zawieszenia bez umiejętności kierowcy | 4 |
| P3 | **Bramki czasowe (sensory) + stoper okrążenia/splitów** | obiektywna miara „czy strojenie jest szybsze", nie tylko feel | 3 |
| P4 | **Strefy tarcia** (lód/mokro) jako kafle płyty | drift/aquaplaning bez czekania na system pogody | 3 |
| P5 | **Rolling road (taśmociąg)** — materiał `tangentVelocity` | test „jazdy" w miejscu, obserwacja rigu z bliska przy dowolnej prędkości | 4 |
| P6 | **Telemetria nawierzchni + airtime + kompresja lądowania** (userMaterialId) | ocena skoczni i zawieszenia liczbami, nie okiem | 6 |
| P7 | **Seedowany teren + przycisk „Przebuduj teren"** | nieskończone warianty offroadu do testów; determinizm przy tym samym seedzie | 1 |
| P8 | **Malowanie stref kolorami** (`customColor` materiałów) | czytelność trudności/stref na renderach i w jeździe | 2–6 |
| P9 | **Przycisk eksplozji** (`b3World_Explode`) | stress rigu impulsem + świetny pokaz możliwości silnika | 4 |

## 5. Docelowy layout świata

Wszystko w jednym świecie fizyki. Płyta: **400×400 m** (2.8× obecnej
powierzchni), top y=0, siatka 3×3 kafli. Offroad: **400×400 m** heightfield
(od doszlifowania 2026-07-12 — te same wymiary co płyta, "dwa równe kafle
mapy", patrz `MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md` §10.2; pierwotnie 320×320),
przyklejony do wschodniej krawędzi płyty z 2-metrową zakładką POD płytą.

```
z+ (północ)
┌─────────────────────────────────┬────────┐ ─ ─ ─ ─ ─ ─ ─ ┐
│  TOR WYŚCIGOWY (pętla, E3)      │ POLIGO-│
│  x:-190..140, z:60..190         │ NY     │   OFFROAD (E1)
├─────────────────────────────────┤ ZAWIE- │   heightfield
│        bufor / dojazdy          │ SZEŃ   │   x: 198..598
│    CENTRUM: spawn, strojenie    │ (E2)   │   z: -200..200
│    czysty plac Ø~80 m @ (0,0)   │ 6 lane │   ridged+warp+roughness
├───────────────┬─────────────────┤ x:150..│   (patrz E1 §10)
│ DRIFT (E3)    │ PLAC FIZYKI (E4)│    195 │   + gradient trudności
│ skid pad,     │ shaker, most,   │ z:-60..│   (łagodny→dziki im
│ ósemka, LÓD   │ taśmociąg, ...  │     60 │    dalej na wschód)
│ x:-190..-30   │ x:10..140       │        │
│ z:-190..-60   │ z:-190..-60     │        │
└───────────────┴─────────────────┴────────┘ ─ ─ ─ ─ ─ ─ ─ ┘
                              styk x=200 (zakładka 2 m POD płytą)
```

Liczby kluczowe (do potwierdzenia bramką wydajności E1):

- Offroad: cell 1.25 m → 321×321 punktów = ~205k trójkątów fizyki i renderu
  (renderer buduje mesh RAZ i cache'uje po hashu — `gfx/debug_shapes.c:832`).
- Wysokości (od doszlifowania 2026-07-12, pełny opis w E1 §10): makro ridged
  2 oktawy (λ≈90/40 m, amp do 8 m) przez domain warp (λ≈60 m, ±22 m); mezo
  λ≈16 m amp do 1.2 m i mikro λ≈2.8 m amp do 0.22 m skalowane chropowatością
  zależną od wysokości (gradient 0→1 na pierwszych ~60 m od styku).
- Pas zakładki: pierwsze ~4 kolumny heightfielda liniowo od −0.12 m do wartości
  szumu — teren wychodzi SPOD płyty (wymaganie Jozza wprost).

## 6. Inwentarz box3d wykorzystany w planie (odpowiedź na „co z box3d pasuje")

| Ficzer silnika | Zastosowanie na mapie | Etap |
|---|---|---|
| `b3CreateHeightField` + własne wysokości, `globalMin/Max` | teren offroad, idealny styk chunków | 1 |
| materiały per-komórka heightfielda (+ dziury `0xFF`) | strefy tarcia i kolory terenu; opcjonalne wyrwy | 1/3 |
| `b3SurfaceMaterial.customColor` | malowanie stref, krawężników, trudności | 2–6 |
| `b3SurfaceMaterial.tangentVelocity` | rolling road (taśmociąg) | 4 |
| `b3SurfaceMaterial.rollingResistance` | kule/beczki, które naturalnie hamują | 5 |
| `b3SurfaceMaterial.userMaterialId` | telemetria „po czym jadę" | 6 |
| kapsuły statyczne | progi, muldy, bale, zaokrąglone lipy skoczni | 2 |
| `b3MakeTransformedBoxHull` | kliny, rampy, kamienie (rock garden), bandy | 2/3 |
| `b3CreateTorusMesh` + mesh statyczny | stosy opon jako bariery toru | 3 |
| ciała kinematyczne + `b3Body_SetTargetTransform` | shaker 4-post, obrotnica | 4 |
| jointy: revolute/spherical/distance | most z desek, see-saw, wrecking ball | 4 |
| sensory + `b3World_GetSensorEvents` | bramki czasowe start/meta/splity | 3 |
| `b3World_Explode` | przycisk eksplozji | 4 |
| `b3World_GetCounters` + profiler sampli | licznik ciał, stress-testy | 5 |
| `DrawString3D` (`gfx/draw.c:412`) | etykiety stref i stacji | 2/6 |
| `b3DumpHeightData`/`b3LoadHeightField`, `image_decode` | (horyzont) teren z pliku / malowany PNG | — |

## 7. Etapy

Kolejność: **1 → 2 → 3 → 4 → 5 → 6**. Twarde zależności: 2 i 3 wymagają 1;
3 używa krawężników/band z kitu 2; 4 i 5 wymagają tylko 1 (można przestawić);
6 zamyka całość. Każdy etap = osobna sesja, osobny commit, bramka zielona,
doki w tym samym commicie.

| Etap | Nazwa | Deliverable | Rozszerzenia |
|---|---|---|---|
| 1 ✅ | Fundament terenu | płyta 400×400 (3×3 kafle), offroad **400×400** (od 2026-07-12) ridged+warp+roughness z zakładką POD płytą, seed+regeneracja, teleport minimalny, pomiar wydajności | P1(min), P7 + 4 nowe env `JOZZ_M6_TELEPORT/AUTODRIVE/PERF_DUMP/REGEN_COUNT` (headless testing, patrz Etap 1 §9) + doszlifowanie na życzenie Jozza (Etap 1 §10) |
| 2 | Przeszkody i poligony | parametryczny obstacle kit (~15 generatorów, ostre↔zaokrąglone), 6 lane'ów progresji, demontaż starych ramp | P8 |
| 3 | Tor i drift | pętla toru z krawężnikami/bandami/oponami, bramki czasowe + HUD stopera, skid pad + ósemka + lodowisko | P3, P4 |
| 4 | Plac fizyki | shaker 4-post, rolling road, obrotnica, most z desek, see-saw, wrecking ball, stosy/domino/kręgle, eksplozja | P2, P5, P9 |
| 5 | Spawner i stress | zakładka Spawner (kształt/rozmiar/materiał/wzorzec/ilość), liczniki + profil, protokół stress z tabelą wyników | — |
| 6 | Nawigacja, pomiar, polish | pełne teleporty + hotkeye, bramy i etykiety stref, telemetria nawierzchni/airtime/lądowania, finalna suita renderów, aktualizacja README/INDEX | P1, P6, P8 |

## 8. Ryzyka wspólne

| # | Ryzyko | Mitygacja | Etap |
|---|---|---|---|
| R1 | Wydajność dużego heightfielda (131k tri, render+fizyka) | bramka pomiarowa w E1 (ms/step, fps); fallback: cell 1.6/2.0 m | 1 |
| R2 | Koła na krawędziach trójkątów przy dużej prędkości (CCD wyłączone decyzją M7) | mikro-oktawa stłumiona w strefach szybkich; test przejazdowy 25+ m/s w bramce E1 | 1 |
| R3 | Podwójny kontakt płyta+heightfield w pasie zakładki | wysokości zakładki ≤ −0.05 m pod topem płyty (kontakt tylko z płytą) | 1 |
| R4 | Regeneracja terenu → wyciek mesha w rendererze | cache jest refcountowany (`ReleaseMeshReference`); sprawdzić licznik registry po 10 regeneracjach | 1 |
| R5 | **Nowa jezdna powierzchnia bez `JOZZ_M6_TERRAIN_CATEGORY`** (`jozz_vehicle_m6_suspension_rig.h:97`) → sfera toczna koła jej NIE WIDZI | checklista w KAŻDYM etapie: wszystko jezdne = kategoria terenu; propy dynamiczne = 0x1 | wszystkie |
| R6 | Szwy kafli płyty pod kołami przy 30+ m/s | topy idealnie równe (ten sam y, te same wymiary); test przejazdu przez szew w bramce E1 | 1 |
| R7 | Tysiące ciał ze spawnera → solver ms | partie z licznikiem, budżet 8 ms/step, sleep włączony; tabela wyników | 5 |
| R8 | Rozrost plików (lekcja Problem A z audytu 2026-07-03) | osobne moduły: `world_layout` / `world_terrain` / `obstacle_kit` / `track` / `playground` / `prop_spawner` | wszystkie |
| R9 | Stare zrzuty referencyjne przestają odpowiadać mapie | nowe nazwy sample-shotów (suita `mapa_*`); starych baseline'ów NIE nadpisywać | 1+ |
| R10 | `SetGroundShape` (grid proceduralny) mógł zakładać JEDEN shape płyty | zbadać w E1 (`gfx/debug_adapter.c`); fallback: grid na kaflu centralnym, reszta customColor | 1 |
| R11 | M5 drivable lab współdzieli course z labem M6 | oba sample bootowane w bramce każdego etapu | wszystkie |

## 9. Co świadomie ODKŁADAMY (nie realizować w tym tracku)

- Import terenu z heightmapy PNG (mamy `jozz_vehicle_image_decode` — kuszące:
  Jozz maluje mapę w GIMP-ie; osobna decyzja po E1, wpis do horyzontu).
- Tekstury/materiały wizualne poza `customColor` (renderer jest debug-level).
- Minimapa, system pogody, dzień/noc.
- LOD / streaming terenu (jeden chunk statyczny wystarcza przy tych rozmiarach).
- Jakiekolwiek zmiany w `src/`/`include/` — **core zostaje box3d** (doktryna).
- Ragdoll-manekiny (fajne, ale nic nie testują w zawieszeniu; ewentualnie E4+).

## 10. Zasady realizacji (obowiązują w każdym etapie)

1. **Bramka etapu**: build Debug + walidator CLI zielony + boot smoke labów
   M5 i M6 + **RENDERY** (render is the gate — praca wizualna bez obejrzenia
   PNG nie istnieje) + wpis w `CHECKPOINTS_PL.md` (≤5 linii).
2. Kategoria terenu na każdej jezdnej powierzchni (R5) — pozycja checklisty
   w bramce, nie „dobra praktyka".
3. Pliki małe, moduły osobne (R8); sample host bez zmian poza dotychczasowymi
   zasadami (patrz README_FOR_AGENTS §1).
4. Identyfikatory zakładek ImGui `###Tab*` nietykalne; nowa zakładka = nowy
   stały identyfikator (lekcja R8 poprzedniego planu).
5. Przy starcie Etapu 1 zaktualizować `README_FOR_AGENTS.md` §2 (front pracy =
   ten track); po Etapie 6 wpis kamienia w `CURRENT_STATE_INDEX_PL.md`.
6. UI zgodnie z preferencjami Jozza: ciasne zakresy suwaków, opis + tooltip
   zamiast akapitów, Podstawowe/Zaawansowane, kolejność zakładek = flow.
