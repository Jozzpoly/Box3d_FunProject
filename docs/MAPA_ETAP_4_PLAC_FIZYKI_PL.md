# Mapa — Etap 4: plac fizyki box3d

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`. Wymaga Etapu 1 (layout);
niezależny od E2/E3 (można przestawić kolejność, jeśli Jozz woli).

## 1. Cel

Kwadrant SE (`x∈[10,140], z∈[-190,-60]`) staje się **placem pokazowo-testowym
możliwości core box3d** (feedback pkt 7): ruchome platformy, jointy, burzenie,
eksplozje — wszystko, co silnik robi świetnie, w formie stanowisk, z których
część jest PEŁNOPRAWNYM narzędziem do testu zawieszenia (shaker, rolling road,
most), a część placem zabaw i stress-testem.

## 2. Zakres

1. Nowy moduł `samples/jozz_vehicle_physics_playground.{h,cpp}` — stanowiska
   + ich update per-step + reset.
2. Wspólny **PropRegistry** (spawn transform + bodyId): przejmuje pachołki E3
   i stare propy course'u; jeden przycisk „Zresetuj przeszkody" resetuje
   WSZYSTKO (rozszerzenie istniejącego wzorca
   `ResetJozzVehicleM5TestCourseProps`, `m5_test_course.cpp:167-176`).
3. Sekcja „Plac fizyki" w zakładce Świat (sterowanie shakerem, taśmą,
   obrotnicą, eksplozją).

**Poza zakresem:** spawner partii obiektów (E5), ragdolle (odłożone, roadmapa §9).

## 3. Stanowiska

### 3.1 Narzędzia testowe zawieszenia (kategoria TERENU — R5, koła muszą je widzieć!)

| Stanowisko | Realizacja | Sterowanie (UI) |
|---|---|---|
| **Shaker 4-post (P2)** | kinematic box 4×4×0.2 m, top ~0.2 m nad płytą, rampki najazdowe 5° z obu stron; ruch: `b3Body_SetTargetTransform` (`box3d.h:557`) co krok wg fali | amplituda 0.02–0.15 m; częstotliwość 0.5–8 Hz; tryb: sinus / sweep 0.5→8 Hz w 20 s / STOP (wraca do y bazowego) |
| **Rolling road (P5)** | STATYCZNY box 6×20 m z `baseMaterial.tangentVelocity` wzdłuż osi taśmy (`types.h:410-412`) — silnik sam wprowadza prędkość styczną w kontakcie; ściany boczne niskie (auto nie spada) | prędkość taśmy 0–30 m/s; kierunek ± |
| **Obrotnica** | kinematic body: 8 × `b3MakeTransformedBoxHull` obróconych co 22.5° = gruby 16-kąt Ø10 m, wys. 0.15 m + rampka; `b3Body_SetAngularVelocity` | obroty 0–2 rad/s |
| **Most z desek** | 2 filary (static) + 16–24 deski 3.5×0.12×0.9 m łączone **revolute** (oś pozioma poprzeczna), przęsło ~20 m, ~1.5 m nad płytą, rampy najazdowe; sztywność zawiasów (hertz/damping) dobrana, by utrzymać auto ~900 kg z zapasem | — (pasywny) |
| **See-saw** | deska 8×0.3×3 m na revolute na koźle 0.5 m, ograniczniki kąta | — (pasywny) |

### 3.2 Zabawki / burzenie (kategoria 0x1 — propy, wszystkie w PropRegistry)

| Stanowisko | Realizacja |
|---|---|
| Wrecking ball | brama 6 m (static) + kula Ø1.2 m gęstość 2000 na **distance joint** (sztywna lina 4 m); rozhuśtanie autem albo spawnem z boku |
| Piramida skrzynek | 6 poziomów, kostki 0.4 m, drewno (gęstość 500) |
| Ściana pustaków | 8×4, pustak 0.5×0.25×0.25, gęstość 1400 |
| Domino | 25 płyt 0.1×0.9×0.45 co 0.55 m, łuk na końcu (pokaz propagacji) |
| Kręgle | 10 stojących kapsuł (r 0.12, h 0.5) w trójkącie + kula Ø0.5 obok; `rollingResistance` na kuli (`types.h:407-408`) — dotoczy się i stanie |
| **Eksplozja (P9)** | przycisk „Eksplozja przed pojazdem": `b3World_Explode` (`box3d.h:176`), `b3ExplosionDef` (`types.h:1007`) — punkt 8 m przed autem; pola def (radius/falloff/impuls) doczytać z nagłówka przy implementacji | 

UI eksplozji: siła (wąski zakres, wyskalować testem), promień 3–8 m;
**bezpiecznik: nie odpala, jeśli środek < 5 m od chassis** (ochrona rigu przed
niezamierzonym one-clickiem — to narzędzie, nie broń).

## 4. Szczegóły techniczne / pułapki

- **Kinematic + koła:** `SetTargetTransform` nadaje ciału prędkość (nie
  teleport), więc kontakt koło–platforma dostaje poprawną prędkość względną —
  dokładnie dlatego shaker ma sens fizyczny. NIE używać `SetTransform` w pętli
  (teleportacja = brak prędkości w kontakcie, koła „nie czują" ruchu).
- **Rolling road NIE jest kinematyczny** — to statyczny box z materiałem
  przenośnikowym; zero kosztu ruchu, czysty efekt w solverze kontaktu.
- Most: deski = kategoria TERENU (jezdne). Revolute z limitami ±25°; gdyby
  ugięcie pod autem było za duże → podnieść hertz jointów, w ostateczności
  co druga para desek na weld (mniejsza żywość, pewna nośność).
- Obrotnica: wiele shape'ów na JEDNYM body (dozwolone i tanie) — nie 8 ciał.
- PropRegistry: jedna struktura {bodyId, spawnPos, spawnRot}; playground
  rejestruje wszystko, co dynamiczne; reset = SetTransform + zerowe prędkości
  + wake (wzorzec istniejący).
- Stanowiska rozstawić co ~25 m, wjazdy od strony centrum; etykiety
  `DrawString3D` jak w E2.

## 5. Bramka

- Build + walidator + boot M5/M6.
- **Rendery `mapa_e4_*`:** przegląd placu top-down; shaker w dwóch skrajnych
  fazach (2 zrzuty — widać ruch); auto NA moście (deski ugięte, nie zapadnięte);
  wrecking ball w wychyleniu; piramida przed i po zburzeniu; domino w połowie
  przewrotki; krater... tzn. rozrzut propów po eksplozji.
- Testy funkcjonalne: auto na shakerze 2 Hz / 0.06 m — koła trzymają kontakt,
  telemetria per-corner labu pokazuje pracę zawieszenia; rolling road 15 m/s —
  auto na hamulcu stoi, koła się kręcą; przejazd mostu tam i z powrotem;
  eksplozja rozrzuca piramidę, rig pojazdu nienaruszony (bezpiecznik dystansu
  zadziałał przy próbie odpalenia pod autem).
- „Zresetuj przeszkody" odtwarza CAŁY plac (registry kompletny).
- Checklista R5: shaker/taśma/obrotnica/most/see-saw = teren; reszta = 0x1.
- Wpis CHECKPOINTS.

## 6. Ryzyka etapu

- Stabilność łańcucha desek pod ~900 kg — dobór hertz; fallback w §4.
- Sweep shakera przechodzący przez rezonans zawieszenia może rzucić autem —
  to FICZER (do tego jest), ale amplitudę klamrujemy do 0.15 m.
- Eksplozja z propami śpiącymi: `b3World_Explode` budzi w promieniu — sprawdzić
  i odnotować (jeśli nie budzi, wake ręcznie w promieniu).
- Liczba jointów (most ~24 + see-saw + ball) — pomijalna; policzyć do
  CHECKPOINTS jako baseline przed E5.
