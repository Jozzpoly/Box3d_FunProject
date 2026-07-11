# Plan: finalizacja nadwozia i przedniego rigu — pełna integracja do gry

Data: 2026-07-11. Autor planu: Fable 5 (sesja planistyczna). Zleceniodawca: Jozz.
Status: **AKTYWNY TRACK** (przejmuje pałeczkę po rozgrzewce edytora rigu —
`PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md`, G1+G3 zrobione i potwierdzone).

Dokumenty etapów (czytaj PRZED implementacją danego etapu — tam są szczegóły,
pułapki i dokładne miejsca w kodzie):

- Etap 1 → `FINALIZACJA_ETAP_1_MODEL_I_UI_PL.md`
- Etap 2 → `FINALIZACJA_ETAP_2_PERSYSTENCJA_PL.md`
- Etap 3 → `FINALIZACJA_ETAP_3_STAN_ZWALIDOWANY_PL.md`

---

## 1. Feedback Jozza (2026-07-11) — co dokładnie zamówił

1. W zakładce **Nadwozie** ma być możliwość **wymiany modelu nadwozia**.
   Aktualna bryła (kolizyjna skrzynia chassis) **zostaje** razem ze swoim
   manualnym dostosowywaniem; wymienny jest WYGLĄD. Nowe nadwozia na razie
   **tylko z dostosowywaniem pozycji** (bez skali/rotacji — dosłownie).
   Nadwozi będzie więcej. Zawieszenie do nadwozia Jozz dopasuje sam
   (istniejącymi suwakami).
2. **Dokończyć i sfinalizować** przednie zawieszenie (rig kierowniczy) i
   nadwozie — pełna integracja do gry „jako aktualnie zwalidowany stan",
   nie eksperyment za checkboxem Debug.
3. **Przemyślana persystencja**: klawisz **R nie może resetować** ustawień
   nadwozia; **presety** mają zapisywać ustawienie nadwozia (offset) i wybrany
   **typ nadwozia**, oraz analogicznie **jaki model zawieszenia** jest wybrany.
4. Przyszłość (poza tym planem): edytor ram — projektowanie własnych ram albo
   system dopasowania ramy do rozstawu osi/kół.

## 2. Analiza krytyczna i konstruktywna

### 2.1 Co bierzemy dosłownie (reguła: słuchać dosłownie)

- **Tylko pozycja** przy nowych nadwoziach. ŻADNYCH suwaków skali/rotacji w tym
  planie — baza rotacji (np. yaw −90° dla `Nadwozie.gltf`) siedzi w rejestrze
  modeli jako stała autorska, nie w UI.
- **Bryła kolizyjna zostaje.** Fizyka chassis (wymiary, gęstość, CG — sekcje
  „Wymiary nadwozia" w `DrawChassisTab`) jest NIETYKALNA w tym planie. Wymienny
  jest wyłącznie wizualny „skin".
- **Presety zapisują typ + ustawienie.** Czyli te dane wchodzą do
  `JozzVehicleM6Config` (to jego serializują presety), a nie do pliku
  debug-view.

### 2.2 Zderzenie z istniejącą doktryną — rozstrzygnięcie

`jozz_vehicle_m6_rig_lab_internal.h:55-64` mówi: „Debug/view toggles są CELOWO
poza JozzVehicleM6Config — to nie jest strojenie pojazdu, nie mogą przeciekać
do presetów". Feedback Jozza redefiniuje granicę: **wybrany model nadwozia,
jego offset i wybrany model zawieszenia to TOŻSAMOŚĆ POJAZDU** (wzorzec BeamNG:
preset opisuje całe auto), a nie widok debugowy. Doktryna zostaje w mocy dla
prawdziwych view-toggles (linie diagnostyczne, tint wahaczy, surowe kształty
kół, widoczność wizualizacji). Konsekwencja:

- `m_useSteeringRig` (internal.h:146) → przestaje być memberem-togglem, staje
  się polem configu `frontSuspensionVisualModel` (string-klucz).
- wybór modelu nadwozia + offset → nowe pola configu (`bodyVisualModel`,
  `bodyVisualOffset`).
- `m_showBodyVisual` → ZOSTAJE view-togglem („czy chcę to teraz widzieć w tym
  labie"), dopisany do istniejącego pliku debug-session. Config mówi „jakie
  nadwozie MA pojazd", debug-view mówi „czy je teraz POKAZUJĘ".

### 2.3 Zidentyfikowane ryzyka (każde ma właściciela w etapie)

| # | Ryzyko | Mitygacja | Etap |
|---|--------|-----------|------|
| R1 | Format JSON: nowe klucze na końcu segmentu C tabeli pól — ostatni wiersz niesie `lastInObject=true`; zostawienie go na starym miejscu produkuje JSON bez przecinka między kluczami = plik nieczytelny dla jsmn | jawny krok „przenieś lastInObject" + test wczytania świeżo zapisanego pliku | 2 |
| R2 | Stringi w POD-owym configu: `std::string` zepsułby trywialną kopiowalność i wzorce inicjalizacji | `char key[32]` + stała `JOZZ_M6_MODEL_KEY_CAP`; zerowanie buforów w DefaultConfig (padding!) | 1 |
| R3 | Sonda determinizmu presetów (`jozz_probes_config.cpp:377`) nie zna nowych pól → persystencja bez strażnika | rozszerzyć sondę o pola visual (porównania per-pole, jak istniejące) | 2 |
| R4 | Każda ścieżka zmiany configu musi odświeżyć wizual: konstruktor po session-load, `LoadPresetByName`, combo w UI, env `JOZZ_M6_PRESET`, „Przywróć wszystkie ustawienia domyślne" | jeden punkt wejścia `ApplyBodyVisualFromConfig()` wołany ze WSZYSTKICH tych miejsc (checklist w docu etapu) | 1+2 |
| R5 | Kolider chassis rysuje się nieprzezroczyście NA ramie (znany artefakt z commita a275947); brak per-shape „nie rysuj" w core; `samples/gfx/debug_adapter.c` jest współdzielony przez wszystkie sample | wydzielone do Etapu 3 z badaniem i STOP-gate; opcje B→A→C opisane w docu etapu | 3 |
| R6 | Zmiana domyślnych (rig+nadwozie ON) zmienia pierwsze wrażenie labu i renderowe baseline'y | decyzja Jozza na STOP-gate Etapu 3, rendery przed/po | 3 |
| R7 | Stare pliki sesji/presetów bez nowych kluczy | semantyka partial-load już to załatwia (config_io.h:17-19: brakujący klucz = wartość bez zmian / factory dla presetów) — wymaga tylko TESTU, nie kodu | 2 |
| R8 | Bug tożsamości zakładek ImGui (naprawiony przez `###TabChassis` — ui_tabs.cpp:698-707) | nie zmieniać identyfikatorów zakładek; nowe kontrolki tylko WEWNĄTRZ | 1 |

### 2.4 Co świadomie ODKŁADAMY (nie realizować w tym planie)

- **Skan folderu assets** w poszukiwaniu nadwozi — kurowana, ręczna lista
  rejestru (skan złapałby np. `Cardan_shaft.gltf`; dodanie nadwozia = 1 wiersz
  rejestru, to wystarczająco tanie).
- Rotacja/skala nadwozia w UI (dosłownie: „narazie tylko pozycja").
- Wymienne modele KÓŁ / tylnego rigu — analogiczna mechanika, osobna decyzja.
- Edytor ram i auto-fit ramy do rozstawu osi/kół — horyzont, §6.
- Dług #14 (rozjazd części rigu przy skręcie) — odłożony decyzją Jozza.

## 3. Etapy

Kolejność ścisła 1 → 2 → 3 (2 zapisuje pola zdefiniowane w 1; 3 zmienia
domyślne wartości pól spersystowanych w 2). Każdy etap = osobna sesja, osobny
commit (lub dwa małe), bramka zielona przy każdym, doki w TYM SAMYM commicie.

### Etap 1 — model danych + rejestr nadwozi + UI w zakładce Nadwozie

**Cel:** wybór modelu nadwozia (combo: Brak / Rama rurowa Jozza / …przyszłe)
i offset pozycji (XYZ, osie chassis) w zakładce Nadwozie; wybór modelu
zawieszenia jako pole configu. Wszystko działa na żywo; persystencji JESZCZE
nie ma (znany limit etapu — R gubi wybór do Etapu 2).

**Zakres:** nowe pola w `JozzVehicleM6Config` (+ sanitize), rejestr modeli
nadwozia, `ApplyBodyVisualFromConfig()`, sekcja UI „Model nadwozia (wygląd)"
NA GÓRZE `DrawChassisTab` (poza flow `m_structuralSetupDirty` — działa live,
bez Apply), migracja `m_useSteeringRig` → config, `m_showBodyVisual` do pliku
debug-session (default: włączony), env `JOZZ_M6_BODY_MODEL`.

**Bramka:** `tools/gate.ps1` zielona; **render is the gate**: zrzut zakładki
(JOZZ_M6_TAB=1) z widocznym combo+suwakami oraz zrzut auta z ramą i offsetem
testowym; walidator — wyniki fizyczne bez zmian; `doc_drift_check.ps1` czysty.

**Agent: Sonnet 5 (high).** Dużo konwencji repo (UI, config, mirror,
render-gate), ale doc etapu prowadzi krok po kroku; zero decyzji otwartych.

### Etap 2 — persystencja: auto-sesja, R, presety, sonda

**Cel:** nowe pola przeżywają R i restart aplikacji (plik auto-sesji), wchodzą
do presetów nazwanych (zapis/odczyt), a determinizm presetów obejmuje pola
visual. Zero zmian w UI.

**Zakres:** typ `JozzFieldType::String` w tabeli pól config_io (+ Write/Read
string), 3 nowe wiersze na końcu segmentu C (z przeniesieniem `lastInObject`),
odświeżanie wizualu po KAŻDEJ ścieżce load (checklist R4), rozszerzenie sondy
determinizmu + round-trip nowych pól w `jozz_probes_config.cpp`, test ręczny
cyklu R (checklist w docu).

**Bramka:** gate zielona (walidator z rozszerzoną sondą: PASS; wyniki
fizycznych sond IDENTYCZNE), manualny cykl R z checklisty, wsteczna zgodność
(stary plik sesji wczytuje się bez błędu), doc-drift czysty.

**Agent: Sonnet 5 (high).** Robota mechaniczna, ale z pułapkami serializacji
(R1/R2/R7) — wszystkie opisane w docu etapu z dokładnymi liniami.

### Etap 3 — „aktualnie zwalidowany stan": domyślne ON, kolider, docs

**Cel:** rig kierowniczy + nadwozie przestają być opt-in: stają się domyślnym,
udokumentowanym, zwalidowanym stanem gry. Rozwiązanie (lub świadome
zaakceptowanie) artefaktu kolidera chassis pod ramą.

**Zakres:** STOP-gate z Jozzem na 3 decyzje (D1 domyślny rig przodu ON?, D2
domyślne nadwozie „rama_rurowa" ON?, D3 wariant kolidera), badanie ścieżki
rysowania kolidera (opcje B→A→C w docu etapu), aktualizacja presetów
built-in (uliczny/drift/offroad) o pola visual, aktualizacja
`CURRENT_STATE_INDEX_PL.md`, `SUBSYSTEM_UI_PRESETS_PL.md`, README (rejestr
env, licznik hooków, aktywny track), CHECKPOINTS (w tym uzupełnienie dwóch
placeholderów `<commit>` z wpisów G3/Nadwozie: e7775a9 / a275947).

**Bramka:** gate zielona, quad-shot przed/po + zbliżenia obejrzane, decyzje
Jozza odnotowane w docu etapu, doc-drift czysty.

**Agent: Fable 5.** Jedyny etap z badaniem współdzielonej infrastruktury
(`samples/gfx/debug_adapter.c` — ryzyko regresji INNYCH sampli), decyzjami
estetycznymi i dwoma STOP-gate'ami z Jozzem.

## 4. Zasady obowiązujące każdy etap (przypomnienie dla agentów)

- Rdzeń silnika `src/` + `include/` — **NIETYKALNY**. `samples/gfx/` wolno
  dotykać tylko w Etapie 3, w wąsko opisany sposób.
- Gałąź `jozz-vehicle-sandbox-m0`; commit+push po zielonej bramce; `main` jest
  Jozza — nigdy nie pushować.
- **Render is the gate** — praca wizualna nie jest skończona, dopóki nie
  obejrzysz PNG. Liczby ≠ poprawny obraz.
- Doki (plan etapu — sekcja „Wynik", README, CHECKPOINTS gdzie dotyczy)
  aktualizowane w TYM SAMYM commicie co kod.
- Zero zaufania do opisów (w tym do tego planu): każde odwołanie plik:linia
  zweryfikuj grepem zanim na nim zbudujesz — kod mógł się przesunąć.
- STOP przy niejasności zamiast zgadywania; pytania do Jozza po polsku.

## 5. Przydział agentów i mocy — zbiorczo

| Etap | Agent | Moc / tryb | Dlaczego |
|------|-------|-----------|----------|
| 1 | Sonnet 5 | high | nowy kod po utartych wzorcach; render-gate; doc prowadzi |
| 2 | Sonnet 5 | high | serializacja z pułapkami, ale w 100% rozpisanymi; sonda pilnuje wyniku |
| 3 | Fable 5 | standard (max niepotrzebny) | badanie shared-infra, decyzje, STOP-gaty z Jozzem |

Uwaga praktyczna: jeżeli agent Sonnet w Etapie 1/2 trafi na COKOLWIEK
niezgodnego z docem etapu (przesunięte linie to norma; sprzeczna STRUKTURA to
sygnał) — ma się zatrzymać i opisać rozbieżność, nie improwizować.

## 6. Horyzont (po tym planie — osobne plany, nie zaczynać)

- Więcej nadwozi: dodanie = plik glTF w `assets/source/` + 1 wiersz rejestru
  (+ ewentualny kontrakt w `assets/contracts/`). Kandydat na pierwszą
  rozbudowę: baza rotacji/pozycji per model mierzona tak jak dla ramy
  (jednorazowy print boundsów — wzorzec z commita a275947).
- Analogiczny rejestr dla modeli zawieszenia (tył; cardan jako G4 rozgrzewki).
- Edytor ram / auto-fit ramy do rozstawu osi i kół — konsumuje ustalenia
  O1–O7 z `EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md`; wymaga własnego planu.

## 7. Postęp

- [x] Etap 1 — model danych + rejestr + UI (Sonnet 5 high; szczegóły "Wynik" w
      `FINALIZACJA_ETAP_1_MODEL_I_UI_PL.md`; render obejrzany, gate zielony)
- [ ] Etap 2 — persystencja R/sesja/presety (commit: …)
- [ ] Etap 3 — stan zwalidowany + kolider + docs (commit: …)
