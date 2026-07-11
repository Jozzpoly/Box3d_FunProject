# Mapa — Etap 3: tor wyścigowy i strefa driftu

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`. Wymaga Etapu 1 (layout,
kafle płyty) i Etapu 2 (kit: `AddBerm`, krawężniki dojdą tu do kitu).

## 1. Cel

Płaska część mapy przestaje być nudną szarą płytą (feedback pkt 6): powstaje
**pętla toru** z krawężnikami, bandą i barierami z opon + **stoper okrążeń
na sensorach** (P3), oraz **strefa driftu**: skid pad, ósemka i lodowisko
o obniżonym tarciu (P4).

## 2. Zakres

1. Tor w strefie `x∈[-190,140], z∈[60,190]` (stałe z `world_layout.h`).
2. Nowe generatory w kicie: `AddCurb` (krawężnik), `AddConeSlalom` (pachołki),
   `AddTireBarrier` (stos opon).
3. Moduł `samples/jozz_vehicle_lap_timer.{h,cpp}`: bramki sensorowe + stan
   stopera + linia HUD.
4. Strefa driftu w kwadrancie `x∈[-190,-30], z∈[-190,-60]`.
5. Lodowisko: wymiana kafla SW płyty na 4 mniejsze kafle, z których jeden
   (50×50 m, róg `x∈[-190,-140], z∈[-190,-140]`) ma `friction≈0.35` +
   `customColor` lodowy. Architektura kafli z E1 istnieje dokładnie po to.

**Poza zakresem:** ruchome elementy (E4), telemetria nawierzchni w HUD (E6 —
tu tylko stoper), AI/duchy/przeciwnicy (poza projektem).

## 3. Geometria toru — ramy + STOP-gate

Dokładny kształt pętli NIE jest przybijany w tym docu. Ramy obowiązkowe:

- pętla zamknięta, szerokość jezdni **12 m**;
- prosta główna ≥ 250 m (start/meta, pomiar Vmax) wzdłuż z≈80;
- prosta powrotna z **szykaną** (przesunięcie ±6 m);
- **hairpin** R≈30 m na wschodnim końcu;
- jeden łuk z **bandą** (`AddBerm` z E2, kąt ~12–15°) na rogu NW;
- 2 bramki splitów + bramka start/meta na prostej głównej.

**STOP-gate:** najpierw budujemy sam SZKIELET (krawężniki wewn./zewn., linia
startu), render top-down + 2 ujęcia z maski drogi → **akceptacja layoutu przez
Jozza** → dopiero potem bariery, pachołki, opony, bandy. To ogranicza koszt
poprawki „przesuńmy hairpin" do jednej tabeli punktów w `world_layout.h`.

## 4. Elementy fizyczne

| Element | Realizacja | Kategoria |
|---|---|---|
| Krawężnik (`AddCurb`) | skośny box wys. ~3 cm, segmenty 2 m, naprzemiennie czerwony/biały `customColor`; ciąg po łuku = segmenty co ~2 m | **TEREN (R5!)** — po krawężniku się jeździ |
| Linia start/meta | cienki box 2–3 mm (lip niewyczuwalny przy kole Ø~0.7 m), biały | teren |
| Banda | `AddBerm` z E2 | teren |
| Pachołki | mały hull-klin ~0.3 m, dynamiczne, gęstość ~80 (lekkie, przewracalne), pomarańczowy kolor | 0x1 (prop) |
| Bariera z opon | `b3CreateTorusMesh` (`collision.h:319`) + `b3CreateMeshShape` (`box3d.h:809`) — **mesh = tylko static**; stos 3–4 opon, na zewnętrznych łukach | 0x1 (nie jeździmy po nich zamierzenie) |
| Lodowisko | kafel 50×50, `friction 0.35`, `restitution 0`, kolor lodowy; top y=0 równy z resztą (zero progu) | **TEREN** |
| Skid pad | okrąg Ø40 m ze znaczników (krawężniki co 15° + pachołki), środek (−120,−120) | znaczniki: teren/prop wg typu |
| Ósemka | 2 okręgi Ø~28 m styczne, wschodnia część kwadrantu | jw. |

Budżet trójkątów opon: torus 16×8 ≈ 256 tri × 4 opony × ~20 stosów ≈ 20k tri —
mieści się bez dyskusji (offroad ma 131k).

## 5. Stoper (lap timer) na sensorach

- Bramka = niewidzialny sensor-box nad jezdnią (szer. 12 × wys. 3 × grub. 0.5):
  `shapeDef.isSensor = true` + `enableSensorEvents = true`
  (`types.h:487-490`), body statyczne, bez koloru (albo delikatny słup boczny
  wizualny po obu stronach jezdni).
- Odczyt: `b3World_GetSensorEvents` (`box3d.h:66`) co krok w `Step()` labu;
  event niesie visitor shape → **filtrować po body chassis** (porównanie
  z `bodyId` chassis pojazdu; koła/propy/pachołki IGNOROWAĆ).
- Stan: `lastCrossTime`, `currentLap`, `bestLap`, `split1/2` względem startu
  okrążenia; debounce 2 s (auto stojące na linii nie nabija okrążeń).
- HUD: jedna linia w istniejącym overlayu labu (obok prędkości/poślizgu):
  `okrążenie 0:00.0 | best 0:00.0 | S1 +0.0 | S2 -0.3`. Reset stopera przy
  „Zresetuj świat"/„Zresetuj pojazd" i teleportach.

## 6. Miejsca w kodzie

- Kafel SW: budowa kafli z E1 w `world_layout.h` — wymiana 1 wpisu na 4;
  ŻADNEJ zmiany w silniku.
- HUD labu: nagłówek tekstowy rysowany w labie M6 (istniejące linie prędkości/
  poślizgu) — dopisać linię stopera tam, gdzie reszta (spójna czcionka).
- Pachołki i opony wchodzą do PropRegistry dopiero w E4/E5 — w E3 reset pachołków
  przez istniejący wzorzec `ResetJozzVehicleM5TestCourseProps` (rozszerzyć listę).

## 7. Bramka

- Build + walidator + boot M5/M6.
- **STOP-gate szkieletu** (§3) ZALICZONY przed dobudową barier.
- **Rendery `mapa_e3_*`:** top-down całego toru; hairpin close-up; banda;
  krawężnik close-up (czerwono-białe segmenty); skid pad; lodowisko (granica
  kolorów kafli widoczna).
- Jazda: pełne okrążenie → HUD pokazuje czas, drugie okrążenie → best się
  aktualizuje, splity liczą; przejazd po krawężniku przy ~20 m/s (podbicie
  czytelne, bez utraty kontroli integralności rigu); drift na lodowisku —
  poślizg w HUD wyraźnie większy niż na asfalcie przy tym samym manewrze.
- Przejazd przez granicę lód/asfalt: zero progu (top y=0 po obu stronach).
- Checklista R5 (krawężniki! banda! lód!). Wpis CHECKPOINTS.

## 8. Ryzyka etapu

- **Sensor łapie koła zamiast chassis** → filtr po bodyId (§5); test: przejazd
  bokiem/poślizgiem przez bramkę liczy się raz.
- **Podwójny kontakt na styku kafli lód/asfalt** — topy równe, ściany pionowe
  stykają się; test przejazdu w bramce. Fallback: 1 mm fazka na krawędzi kafla.
- **Pachołki w liczbie** (~60–100 dynamicznych) — śpią do potrącenia
  (sleep default), koszt pomijalny; policzyć w CHECKPOINTS.
- Debounce stopera przy driftowaniu przez linię (przejazd tyłem) — okrążenie
  liczone tylko przy przekroczeniu w dobrym kierunku (znak prędkości wzdłuż
  osi bramki).
