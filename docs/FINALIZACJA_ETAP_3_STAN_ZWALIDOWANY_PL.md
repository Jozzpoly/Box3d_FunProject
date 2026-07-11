# Etap 3: „aktualnie zwalidowany stan" — domyślne ON, kolider, docs

Część planu `PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md`. Wymaga
ZAKOŃCZONYCH Etapów 1 i 2. Agent: **Fable 5** (badanie współdzielonej
infrastruktury + decyzje z Jozzem). Jedna sesja; dopuszczalne 2 commity
(defaults+presety / kolider), każdy gate-green.

**Cel jednym zdaniem:** rig kierowniczy przodu i nadwozie przestają być
eksperymentem opt-in — stają się domyślnym, spersystowanym, udokumentowanym
stanem gry, a artefakt kolidera chassis jest rozwiązany albo świadomie
zaakceptowany decyzją Jozza.

---

## 0. STOP-GATE na starcie: trzy decyzje Jozza

Zanim powstanie jakikolwiek kod: przygotuj rendery porównawcze (stan obecny
vs proponowany default) i zadaj Jozzowi TRZY pytania. Bez odpowiedzi nie
implementować — to jest fork „jak ma wyglądać gra".

- **D1**: `frontSuspensionVisualModel` domyślnie `"rig_kierowniczy"`?
  (Rekomendacja: TAK — G1+G3 potwierdzone na żywo przez Jozza; stary mount
  zostaje na tyle i jako opcja „klasyczny".)
- **D2**: `bodyVisualModel` domyślnie `"rama_rurowa"`? (Rekomendacja: TAK —
  „pełna integracja do gry"; w labie zawieszenia Jozz ma checkbox „Pokaż
  nadwozie 3D" i preset roboczy, żeby ją chować. Pokaż Jozzowi render z D3,
  bo sensowność D2 zależy od wyniku D3.)
- **D3**: wariant kolidera (§2): B (przygaszenie), A (ukrycie per-shape),
  C (zostaje jak jest). Pokaż rendery wariantu B zanim spytasz — tanio go
  zprototypować.
- Bonus-pytanie: czy presety built-in (uliczny/drift/offroad) mają dostać
  jawne pola visual (§3)?

## 1. Zmiana domyślnych (po D1/D2)

- `JozzVehicleM6DefaultConfig` (suspension_rig.cpp): wartości z decyzji
  (`snprintf` do buforów — wzorzec z Etapu 1).
- **Konsekwencje, które MUSISZ sprawdzić:**
  - `m_factoryConfig` (rig_lab.cpp:69) = nowe defaults → „Przywróć wszystkie
    ustawienia domyślne" przywraca rig+ramę. Zamierzone.
  - Walidator/gate: defaults konsumuje też walidator — sondy fizyczne NIE
    czytają pól visual, więc liczby nie mają prawa drgnąć. Jeżeli drgnęły —
    STOP, coś czyta więcej niż deklaruje.
  - Boot-smoke gate'a i quad-shot: OBRAZ się zmieni (rama na aucie) — to
    oczekiwane; gate nie porównuje obrazów poza `-DiffBaseline`. Zrób ŚWIEŻE
    zrzuty referencyjne po zmianie i OBEJRZYJ je (render is the gate).
  - Stare pliki sesji użytkownika nadal wygrywają nad defaults (in-place
    load) — Jozz po updacie zobaczy swój ostatni stan, nie nowy default.
    Odnotuj to w komunikacie do Jozza (może chcieć skasować plik sesji, żeby
    zobaczyć nowe defaults).

## 2. Kolider chassis pod ramą (ryzyko R5) — badanie i warianty

**Problem** (commit a275947): bryła kolizyjna chassis (tan box) rysuje się
nieprzezroczyście NA ażurowej ramie. Fakty zastane:

- Rysowaniem brył steruje globalny `drawShapes`
  (`samples/gfx/debug_adapter.c:154`) — wyłączenie gasi też ziemię, przeszkody
  i koła. Core (`include/box3d/types.h`, DebugDraw) NIE ma per-shape flagi
  „nie rysuj"; `customColor` na shape (types.h:421/2898) ustawia KOLOR, nie
  widoczność, a ścieżka auto-rysowania nie honoruje alfy.
- Rdzeń `src/`+`include/` NIETYKALNY. `samples/gfx/debug_adapter.c` to
  infrastruktura SAMPLI (wolno), ale współdzielona przez WSZYSTKIE sample —
  każda zmiana musi być opt-in i niewidoczna dla nie-Jozzowych sampli.

**Wariant B — przygaszenie (zbadaj i zprototypuj NAJPIERW):**
`b3ShapeDef.customColor` przy tworzeniu bryły chassis (nasz kod —
`CreateJozzVehicleM6` w suspension_rig.cpp; znajdź tworzenie chassis shape).
Ciemny, niskokontrastowy kolor (np. grafit zbliżony do tła) sprawia, że
rama czyta się na bryle, a bryła wciąż komunikuje swoje wymiary (to wartość
w labie: suwaki wymiarów coś pokazują). Sprawdź: (a) czy box3d udostępnia
runtime-setter customColor (grep `CustomColor` w include/box3d) — jeśli nie,
kolor ustawiony przy tworzeniu wystarczy: bryła i tak jest odtwarzana przy
każdym rebuildzie pojazdu; (b) czy walidator tworzy ten sam chassis — kolor
nie wpływa na sondy, ale nie zmieniaj ścieżki wspólnej inaczej niż o pole
koloru w shapeDef. Zalety: zero dotykania debug_adapter, zero wpływu na inne
sample, odwracalne jednym polem. Wady: bryła nadal zasłania fragmenty ramy
poniżej swojej górnej ściany.

**Wariant A — pomijanie per-shape (tylko jeśli B za mało po obejrzeniu):**
w `debug_adapter.c` ścieżka rysująca solid shapes — ZBADAJ, co callback
dostaje: jeżeli w zasięgu jest `b3ShapeId` (lub kontekst per-shape), dodaj
maleńki, jawny rejestr „shapes to skip" po stronie adaptera (API w stylu
`DebugAdapter_SetShapeHidden(b3ShapeId, bool)`), wołany wyłącznie przez nasz
lab przy tworzeniu/toggle'u nadwozia. Jeżeli callback dostaje TYLKO
wierzchołki+kolor — jedyną drogą byłby kolor-sentinel; to brzydkie i kruche:
w takim wypadku wracamy do B albo C, NIE wdrażamy sentinela bez rozmowy z
Jozzem. Warunki twarde dla A: default = nic nie ukrywa; inne sample
niedotknięte (przejrzyj, kto jeszcze linkuje adapter); komentarz w kodzie
tłumaczy, czemu to żyje w adapterze a nie w core.

**Wariant C — artefakt zostaje:** udokumentowany w TECH_DEBT (nowa pozycja ze
wskazaniem na to badanie), checkbox „Pokaż nadwozie 3D" pozostaje szybkim
obejściem. Wybieralny, jeśli B wygląda źle, a A okazuje się kruche.

Kolejność pracy: prototyp B (≤30 min) → rendery → pytanie D3 do Jozza z
obrazkami → implementacja wybranego wariantu.

## 3. Presety built-in (po bonus-decyzji)

Jeżeli Jozz potwierdzi: dopisz do `assets/vehicle_presets/{uliczny,drift,offroad}.json`
jawne pola visual (rama + rig + offset 0), żeby preset był kompletnym opisem
auta niezależnie od przyszłych zmian defaults. Ręczna edycja JSON — pilnuj
przecinków; po edycji `gate.ps1` (sonda determinizmu czyta offroad.json!
sprawdź, czy jej asercje „unlisted → factory" nie wymagają aktualizacji —
pola przestają być „unlisted", więc DOSTOSUJ sondę: wartości z presetu mają
wygrywać; to jest zmiana scenariusza sondy, zrób ją świadomie w tym samym
commicie). Jeżeli Jozz odmówi — presety zostają partial (dziedziczą nowe
defaults) i NIC nie robisz.

## 4. Dokumentacja „zwalidowanego stanu" (ten sam commit co defaults)

- `docs/CURRENT_STATE_INDEX_PL.md` — nowy stan: rig przodu + nadwozie
  domyślne, gdzie żyje rejestr, gdzie persystencja.
- `docs/SUBSYSTEM_UI_PRESETS_PL.md` — pola visual w presetach + doktryna
  §2.2 planu (jeśli Etap 2 tego nie domknął — zweryfikuj).
- `docs/SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` — status rigu kierowniczego:
  domyślny na przodzie.
- `README_FOR_AGENTS.md` — aktywny track (plan finalizacji → zamknięty,
  wskaż następny), rejestr env aktualny.
- `docs/CHECKPOINTS_PL.md` — wpis Etapu 3 Z HASHEM (nie `<commit>`)
  **oraz** uzupełnij dwa istniejące placeholdery `<commit>` z wpisów
  G3/Nadwozie: `e7775a9` (G3 drążek+dumper) i `a275947` (Nadwozie skin) —
  dług dokumentacyjny z 2026-07-11.
- `docs/TECH_DEBT_PL.md` — nowa pozycja TYLKO przy wariancie C; przy A/B
  sprawdź, czy któraś pozycja nie wymaga aktualizacji.
- `docs/PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md` — dopisek w statusie:
  stan rozgrzewki wszedł do gry jako domyślny (ten plan tylko wskazuje,
  szczegóły tutaj).

## 5. Bramka wyjścia

1. `gate.ps1` zielona (sondy fizyczne liczbowo IDENTYCZNE; sonda presetów
   zaktualizowana świadomie, jeśli §3 wszedł).
2. **Render is the gate**: quad-shot nowego stanu domyślnego + zbliżenie
   przodu (rig+drążek+dumper) + ujęcie wariantu kolidera — obejrzane i
   pokazane Jozzowi w raporcie.
3. Decyzje D1-D3 (+bonus) odnotowane w tym pliku, sekcja „Wynik".
4. `doc_drift_check.ps1` czysty; wszystkie doki z §4 w tych samych commitach
   co odpowiadający kod.
5. Commit(y)+push; odhacz Etap 3 w §7 planu; zaktualizuj pamięć projektową
   (aktywny track domknięty).

## Wynik (wypełnia agent wykonujący)

- Decyzje Jozza: D1 … / D2 … / D3 … / presety …
- Commity: …
- Rendery: …
- Rozbieżności: …
