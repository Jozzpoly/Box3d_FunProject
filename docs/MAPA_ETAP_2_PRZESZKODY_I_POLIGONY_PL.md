# Mapa — Etap 2: biblioteka przeszkód i poligony zawieszeń

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`. Wymaga Etapu 1 (layout).

## 1. Cel

Zastąpić 4 przypadkowe rampy i 2 washboardy **parametryczną biblioteką
przeszkód** (różne typy, wielkości, ostrości i zaokrąglenia — feedback pkt 4)
oraz ułożyć z niej **poligon 6 lane'ów o rosnącej trudności** (feedback pkt 5:
„mądrze zaprojektowane miejsca do testowania zawieszeń w twardych warunkach").

## 2. Zakres

1. Nowy moduł `samples/jozz_vehicle_obstacle_kit.{h,cpp}`: czyste funkcje
   `Add*`, każda przyjmuje world, pozycję/yaw, parametry i
   `terrainCategoryBits` (**R5** — wszystko jezdne!), nazwa body `"kit_*"`.
2. Poligon lane'ów w pasie `x∈[150,195], z∈[-60,60]` (stałe z `world_layout.h`).
3. Demontaż starych `AddRamp`×4 i `AddWashboardLane`×2 z course'u; washboard
   przechodzi do kitu jako generator parametryczny.
4. Propy: odsunąć spawn-listę `kPropSpecs` z osi jazdy (centrum ma być czyste)
   na skraj placu fizyki (E4 i E5 je przejmą). Bez zmian mechaniki resetu.
5. Kolory trudności przez `customColor` (P8): zielony=łagodny, żółty=średni,
   czerwony=twardy; etykiety `DrawString3D` przy wjeździe każdej stacji.

**Poza zakresem:** krawężniki/bandy toru (E3 użyje kitu), ruchome przeszkody
(E4), jakiekolwiek zmiany fizyki pojazdu.

## 3. Generatory kitu (sygnatury projektowane, parametry = pełne spektrum ostrości)

Zaokrąglenia realizują **kapsuły** (jedyny okrągły prymityw statyczny obok
sfery), ostre krawędzie — transformowane boxy (`b3MakeTransformedBoxHull`,
`collision.h:224`). Każdy generator: `float` w metrach/stopniach, seed tam,
gdzie losowość.

| Generator | Parametry | Realizacja / po co |
|---|---|---|
| `AddWedgeRamp` | dł., szer., kąt 5–35° | pochylony box zakopany krawędzią (jak stare rampy, ale parametryczne) — skocznia OSTRA |
| `AddKicker` | jak wyżej + promień lipa 0.1–0.5 m | rampa + **kapsuła wzdłuż krawędzi natarcia** = zaokrąglony lip; łagodne oderwanie |
| `AddTabletop` | najazd, blat, zeskok, wys. | 3 boxy; bezpieczna skocznia z lądowaniem w dół |
| `AddGapJump` | wys., kąt, przerwa 2–10 m | 2 rampy naprzeciw; twardy test energii lotu |
| `AddStepUp` / `AddStepDown` | wys. 0.1–0.5 m | pionowy uskok; brutalny test uderzeniowy |
| `AddWhoops` | ilość, skok 1.5–4 m, promień 0.15–0.45 m | **rząd leżących kapsuł w poprzek** = zaokrąglone muldy rytmiczne (klasyka offroad) |
| `AddSpeedBump` | promień, szer. | pojedyncza kapsuła — próg zwalniający |
| `AddWashboard` | ilość, rozstaw, wys. | boxy jak dziś, parametryczne (tarka OSTRA — kontrast do whoops) |
| `AddRockGarden` | pole a×b, gęstość, rozmiar 0.15–0.6 m, seed | losowo obrócone transformed-boxy częściowo zakopane = kamienie |
| `AddRuts` | dł., głęb. 0.1–0.3 m, rozstaw kół | koleiny: 2 rynny z boxów skośnych (jazda w koleinie i wychodzenie z niej) |
| `AddOffCamber` | pochylenie boczne 5–20° | płyta przechylona w bok — test przyczepności/przechyłu |
| `AddBerm` | promień łuku, kąt bandy 10–30° | segmenty boxów po łuku (banda; E3 użyje na torze) |
| `AddStairs` | stopnie, wys./głęb. | schody (są w projekcie od zawsze — teraz parametryczne) |
| `AddLogs` | promień 0.1–0.35 m, ilość | leżące kapsuły-bale, pojedynczo lub wiązka |
| `AddArticulationRamps` | kąt, offset L/P | przeciwstawne kliny pod lewe/prawe koła — **test wykrzyżowania** zawieszenia |

## 4. Poligon: 6 lane'ów progresji (N→S, każdy ~6 m szer., przerwa 1.5 m)

| Lane | Nazwa | Zawartość (kolejność najazdu) | Testuje |
|---|---|---|---|
| L1 | Komfort | speed bumpy R0.3 → washboard niski → step 0.1 | codzienne zawieszenie, tłumienie małych amplitud |
| L2 | Szuter | whoops płytkie → ruts płytkie → off-camber 8° | stabilność rytmiczna |
| L3 | Rajd | kicker 12° z lipem → tabletop → whoops średnie | lot + lądowanie z dohamowaniem |
| L4 | Kamień | rock garden gęsty → logs pojedyncze → stairs w dół | artykulacja, uderzenia punktowe |
| L5 | Ekstrema | step-up 0.35 → articulation ramps → ruts głębokie → berm | wykrzyżowanie, limity travel |
| L6 | Skocznie | wedge 25° ostry → gap 4 m → kicker 20° R0.4 → lądowisko płaskie | twarde lądowania (kontrola integralności rigu — lekcja M7) |

Stacje w lane rozstawione co ~8 m; przed lane'em słupek koloru trudności +
`DrawString3D` z nazwą i kluczowymi parametrami (np. „Kicker 20° R0.4").
Rozmieszczenie liczbowo w `world_layout.h`, nie magic-numbers w wywołaniach.

## 5. Miejsca w kodzie

- `samples/jozz_vehicle_m5_test_course.cpp:108-154` — po tym etapie course
  woła kit + layout zamiast własnych ramp; plik ma ZMALEĆ.
- Kapsuła statyczna: `b3CreateCapsuleShape` na static body (wzorce w
  `sample_shapes.cpp`); oś kapsuły = segment między dwoma punktami.
- Kolory: `shapeDef.baseMaterial.customColor` (`types.h:418-421`).
- Etykiety: `DrawString3D` (`gfx/draw.c:412`) — wołane per-frame; zebrać
  etykiety stacji w małą tablicę rysowaną w `Step()` labu (nie per-shape hack).

## 6. Bramka

- Build + walidator + boot M5/M6.
- **Rendery `mapa_e2_*`:** top-down poligonu (6 lane'ów czytelnych kolorami),
  close-up kickera z lipem (widać kapsułę na krawędzi!), close-up whoops
  (zaokrąglenia widoczne), rock garden, articulation ramps.
- Przejazdy: L1 przy ~8 m/s czysto; L4 wolno (artykulacja — koła trzymają
  kontakt naprzemiennie); L6: skok gap 4 m z lądowaniem bez rozpadu rigu
  (landing integrity — to samo, co pilnuje walidator M7).
- Checklista R5: KAŻDY nowy shape jezdny ma kategorię terenu (grep po
  `kit_` + przegląd `filter.categoryBits`).
- Wpis CHECKPOINTS.

## 7. Ryzyka etapu

- Kontakt koła (sfera toczna) z kapsułą pod kątem — sprawdzić na whoops przy
  różnych prędkościach; gdyby ślizg po osi kapsuły dawał artefakty, muldy mają
  fallback: box z fazowanym grzbietem (2 skośne boxy).
- Ilość statycznych shape'ów (~150–200) — pomijalna dla broadphase, ale
  policzyć i odnotować w CHECKPOINTS (baseline przed E3/E4).
- `DrawString3D` przy wielu etykietach — rysować tylko w promieniu ~80 m od
  kamery (prosty dystansowy cutoff), inaczej HUD zaśmieci się z daleka.
