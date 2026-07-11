# Mapa — Etap 6: nawigacja, pomiar, polish

Część planu `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`. Etap ZAMYKAJĄCY — wymaga
wszystkich poprzednich (spina strefy w całość i domyka dokumentację tracku).

## 1. Cel

Duża mapa ma być NARZĘDZIEM, nie przestrzenią do błądzenia: pełna nawigacja
teleportami (P1), czytelne oznaczenia stref (P8), telemetria nawierzchni,
skoków i lądowań (P6) oraz finalny polish + aktualizacja dokumentacji repo.

## 2. Zakres

1. **Pełny rejestr teleportów** w `world_layout.h` (nazwa PL, pozycja, yaw):
   Start · Tor — start/meta · Drift — skid pad · Lodowisko · Poligon L1 ·
   Poligon L4 · Poligon L6 · Plac fizyki · Most · Offroad — wjazd ·
   Offroad — środek · Offroad — głęboko.
2. UI: combo + przycisk „Teleportuj" w zakładce Świat (rozbudowa minimalnej
   wersji z E1) + **hotkeye** — PRZED przydziałem przejrzeć
   `docs/HOTKEY_AUDIT_PL.md` (propozycja: Shift+1..9, ale audyt decyduje).
   Teleport = `CreateVehicle(kotwica)` (mechanizm z E1), wysokość z próbki
   terenu + zapas.
3. **Bramy stref**: 2 słupki + belka w kolorze strefy przy każdym wjeździe;
   spójna paleta (tabela §4); etykiety `DrawString3D` z cutoffem ~80 m (E2).
4. **Telemetria** (linie w istniejącym HUD labu):
   - „nawierzchnia: asfalt/lód/offroad/krawężnik/platforma" — z
     `userMaterialId` materiałów (`types.h:414-416`), nadanych strefom w E1/E3/E4,
     odczyt z kontaktów kół (istniejąca pętla kontaktowa telemetrii labu);
   - **airtime**: 4 koła bez kontaktu → stoper; po lądowaniu HUD trzyma 5 s:
     „skok: 1.24 s · 14.2 m";
   - **kompresja lądowania**: maks. dobicie zawieszenia w pierwszych 0.5 s po
     kontakcie (dane per-corner JUŻ SĄ w telemetrii rigu — tylko zatrzask max).
5. **Polish**: przegląd kolorów całej mapy na renderach; korekty rozstawienia
   po doświadczeniach E2–E5; domyślna kamera startowa obejmująca centrum +
   wjazdy stref.
6. **Dokumentacja repo** (zamknięcie tracku):
   - `README_FOR_AGENTS.md` §1/§2 — mapa jako nowy stan zwalidowany;
   - `CURRENT_STATE_INDEX_PL.md` — wpis kamienia „Mapa 2.0";
   - `TECH_DEBT_PL.md` — wszystko, co świadomie odłożone/zaobserwowane
     (np. tunelowanie żwiru z E5, import heightmapy PNG z horyzontu);
   - CHECKPOINTS — wpis końcowy.

**Poza zakresem:** minimapa, zapis „stanu mapy" do presetów pojazdu (teren to
świat, nie pojazd — doktryna z E1), nowe strefy.

## 3. Szczegóły techniczne

- `userMaterialId`: nadać stałe (enum w `world_layout.h`): 1=asfalt, 2=lód,
  3=offroad, 4=krawężnik, 5=platforma, 0=nieznane. Uzupełnić WSTECZ materiały
  stref z E1/E3/E4 (kilka linii per moduł — dlatego ten punkt jest w E6,
  po powstaniu wszystkich powierzchni).
- Odczyt nawierzchni: przy istniejącym zbieraniu kontaktów kół (telemetria
  per-corner) dopisać odczyt materiału z drugiej strony kontaktu; pokazywać
  materiał większości kół (2+ koła = ta nawierzchnia).
- Airtime/lądowanie korzysta z już liczonych flag kontaktu per koło (HUD ma
  dziś „kontakt PL:T PP:T TL:T TP:T" — to samo źródło).
- Teleporty a stoper okrążeń (E3): teleport resetuje bieżące okrążenie
  (bez kasowania best) — inaczej „teleport na metę" nabija fałszywy czas.

## 4. Paleta stref (finalna — do użycia wstecz w E2–E5, jeśli odbiegały)

| Strefa | Kolor (hex) | Użycie |
|---|---|---|
| Centrum / neutralne | 0x9AA0A6 (szary) | kafle płyty, słupki neutralne |
| Tor | 0x2F6FED (niebieski) + krawężniki 0xD5453C/0xFFFFFF | bramy, linie |
| Drift/lód | 0x7EC8E3 (lodowy) | kafel lodu, brama driftu |
| Poligony | zielony 0x3FA34D / żółty 0xE0A62E / czerwony 0xD5453C | trudność stacji (z E2) |
| Plac fizyki | 0x8E5BD1 (fiolet) | bramy, platformy ruchome |
| Offroad | 0x8B6B4A (brąz) | słupki wjazdu, tyczki trasy |

(Wartości orientacyjne — finalny dobór NA RENDERACH, render is the gate.)

## 5. Bramka (zamyka CAŁY track — sign-off Jozza)

- Build + walidator + boot M5/M6.
- **Przejście po WSZYSTKICH teleportach** po kolei: auto ląduje poprawnie
  (nie w terenie, nie w przeszkodzie), yaw sensowny (przodem do atrakcji).
- Telemetria: przejazd asfalt→lód→offroad zmienia linię nawierzchni; skok na
  L6 pokazuje airtime i kompresję; wartości wiarygodne (sanity, nie kalibracja).
- **Finalna suita renderów `mapa_final_*`** (quad_shot): total top-down,
  centrum, tor, drift+lód, poligony, plac fizyki, offroad łagodny, offroad
  dziki, styk płyta/offroad. To jest NOWY zestaw referencyjny mapy (R9:
  starych baseline'ów nie nadpisujemy).
- Dokumentacja z §2.6 zaktualizowana W TYM SAMYM commicie.
- Wpis CHECKPOINTS + **akceptacja Jozza całego tracku** (jak D1/D2/D3 przy
  finalizacji nadwozia).

## 6. Ryzyka etapu

- Kolizje hotkeyów z istniejącymi skrótami labu/sampla — dlatego najpierw
  `HOTKEY_AUDIT_PL.md`, przydział dopiero po nim.
- `userMaterialId` wstecz w E1/E3/E4 — rozproszona zmiana; checklista per
  moduł (terrain/kafle/kit/playground) w PR-ze etapu.
- Pokusa „jeszcze jednej strefy" na polish — NIE; nowe strefy = nowy plan
  (scope-creep zabija zamknięcie tracku).
