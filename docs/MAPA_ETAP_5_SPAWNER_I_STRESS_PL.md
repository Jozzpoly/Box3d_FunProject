# Mapa — Etap 5: spawner obiektów i stress-testy

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`. Wymaga Etapu 1; korzysta
z PropRegistry z Etapu 4 (jeśli E4 przestawiono — registry powstaje tutaj).

## 1. Cel

Feedback pkt 8 wprost: **dużo więcej obiektów fizycznych — od bardzo małych po
bardzo duże, różne kształty — i opcja w menu do spawnowania kolejnych, do
stress-testów dużej ilości obiektów.** Do tego protokół pomiarowy: ile ciał
świat trzyma przy budżecie czasu kroku.

## 2. Zakres

1. Nowy moduł `samples/jozz_vehicle_prop_spawner.{h,cpp}`:
   `SpawnBatch(world, SpawnDef) → batchId` + rejestr partii (na PropRegistry).
2. **Nowa zakładka „Spawner"** w labie M6: po „Świat", przed „Debug"
   (`jozz_vehicle_m6_rig_lab_ui_tabs.cpp:769-799`). Nowy stały identyfikator
   `###TabSpawner`; istniejących `###Tab*` NIE dotykać (zasada 4 roadmapy).
3. Protokół stress + tabela wyników (§6).

**Poza zakresem:** persystencja zespawnowanych obiektów (świadomie BRAK —
reset czyści, presety pojazdu ich nie znają), edytory kształtów.

## 3. Model danych

`SpawnDef`: kształt, przedział rozmiaru, materiał, ilość, wzorzec, punkt
odniesienia, seed. Wszystko deterministyczne przy tym samym seedzie.

| Wymiar | Warianty |
|---|---|
| Kształt | kostka · kula · kapsuła · deska (1:4:0.25) · klin (transformed box) · płyta · MIX |
| Rozmiar (presety) | Żwir 0.05–0.12 m · Gruz 0.15–0.45 · Skrzynki 0.5–1.2 · Głazy 1.5–3.0 · Kontener 2×2×6 m |
| Materiał (gęstość, tarcie, odbicie, kolor) | styropian 30 · drewno 500 · guma 1100 + rest 0.6 · beton 2200 · stal 7800; kule/kapsuły dostają `rollingResistance` 0.02–0.1 |
| Ilość na klik | 1 · 10 · 50 · 250 |
| Wzorzec | deszcz (kolumna 8 m nad punktem, jitter XZ) · siatka N×N · stos · piramida · ściana |
| Punkt | 10 m przed pojazdem · środek placu fizyki · wjazd offroadu (combo) |

Wszystkie propy: `filter.categoryBits = 0x1` (NIE teren — wzorzec z
`m5_test_course.cpp:55-60`, sfera toczna koła ma ich nie widzieć), sleep
włączony (domyślny), nazwa body `"spawn_<batch>"`.

## 4. UI zakładki (wg preferencji Jozza: opis+tooltip, ciasne zakresy, flow)

1. Sekcja **Co spawnować**: combo kształt / preset rozmiaru / materiał
   (+ krótkie opisy, tooltipy z liczbami).
2. Sekcja **Jak**: wzorzec, ilość (radio 1/10/50/250), punkt, seed (InputInt
   + „losuj").
3. Przycisk główny **„Spawnuj"** + skrót w tooltipie.
4. Sekcja **Zarządzanie**: licznik „partii: N · ciał zespawnowanych: M ·
   ciał w świecie: K" (`b3World_GetCounters`, `box3d.h:213`); przyciski
   „Usuń ostatnią partię" / „Usuń wszystkie" / „Uśpij wszystkie";
   linia „czas kroku: X ms" (odczyt z istniejącego profilu sampla —
   `samples/sample.cpp` trzyma `m_profiles`, nie budować własnego zegara).
5. Sekcja **Stress (zaawansowane)**: przycisk „+250 kostek 0.4" w pętli
   ręcznej + wskazówka, gdzie patrzeć (wykres profilu w panelu sampla).

Usuwanie partii: `b3DestroyBody` dla bodyId partii (szybkie, silnik to lubi);
licznik weryfikowany counters PO usunięciu (wyłapie wycieki rejestru).

## 5. Szczegóły / pułapki

- **Żwir 0.05 m pod kołami przy CCD OFF** (decyzja M7): drobnica może się
  przeciskać/tunelować pod dociążonym kołem — AKCEPTOWANE (to stress-zabawka,
  nie fizyka opon po żwirze); zaobserwować i odnotować w CHECKPOINTS.
  NIE włączać CCD globalnie z tego powodu.
- Spawn „deszcz" nad punktem: kolumna z odstępem pionowym ≥ maks. wymiar × 1.2
  (bez initial-overlap między sobą); nad terenem offroad wysokość bazowa =
  próbka heightfielda (helper z E1).
- „Kontener" (2×2×6, stal) to ~19 ton — świetny test zderzenia i podnoszenia
  na skoczni; spawnować pojedynczo, z dala od auta (punkt przed pojazdem
  odsuwa się do 15 m przy tej klasie rozmiaru).
- MIX losuje kształt per obiekt z seeda — do „gruzowiska".
- Piramida/ściana budowane od podłoża (raycast wysokości), nie od y=0 —
  inaczej na offroadzie wiszą/przenikają.

## 6. Protokół stress (wypełnić tabelę przy realizacji)

Scenariusz bazowy: kostki 0.4 m, drewno, wzorzec siatka na placu fizyki,
partie po 250. Po każdej partii: 5 s symulacji, odczyt ms/step (średnia z
sekundy) i fps. Stop przy step > 8 ms albo 2500 ciał.

| Ciała łącznie | ms/step | fps | Uwagi (sleep? kontakt z autem?) |
|---|---|---|---|
| 250 | — | — | — |
| 500 | — | — | — |
| 1000 | — | — | — |
| 1500 | — | — | — |
| 2000 | — | — | — |
| 2500 | — | — | — |

Drugi przebieg: to samo z autem wjeżdżającym w stos (kontakty aktywne, nie
śpiące). Wynik = rekomendowany „miękki limit" w tooltipie zakładki.

## 7. Bramka

- Build + walidator + boot M5/M6.
- **Rendery `mapa_e5_*`:** deszcz 500 kostek w locie; piramida 10 poziomów;
  gruzowisko MIX (żwir+gruz) przed autem; kontener obok auta (skala!).
- Funkcjonalnie: spawn → licznik rośnie; „Usuń ostatnią partię" → wraca
  dokładnie do poprzedniej liczby (counters!); „Usuń wszystkie" → stan
  wyjściowy; ten sam seed = identyczny układ.
- Tabela §6 wypełniona, miękki limit wpisany do tooltipa.
- UI: zakładki istniejące niezmienione (`###Tab*` nietknięte), kolejność:
  Spawner po Świat, przed Debug.
- Wpis CHECKPOINTS (z wynikiem stress — liczba ciał @ 8 ms).

## 8. Ryzyka etapu

- R7 z roadmapy (solver ms) — protokół §6 właśnie to mierzy, partiami.
- Wielki spawn na raz (250 dużych) może zbić fps chwilowo przy budowie —
  akceptowalny spike; NIE rozkładać budowy na klatki (komplikacja bez potrzeby).
- Rejestr batchy a „Zresetuj świat" (pełny restart) — zespawnowane znikają
  (świat budowany od nowa), rejestr musi się wyczyścić razem z nim (test).
