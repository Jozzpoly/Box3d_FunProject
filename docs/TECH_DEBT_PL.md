# TECH_DEBT — Jozz Vehicle Box3D Native

Rejestr znanego długu technicznego, ryzyk i świadomie odłożonej pracy.
Utworzony 2026-07-08 w ramach przeglądu porządkującego. Każda pozycja ma:
**opis → ryzyko → plan**. Przeczytaj to przed „sprzątaniem" projektu — część
rzeczy wygląda na bałagan, a jest świadomą decyzją.

Legenda ryzyka: 🔴 wysokie · 🟠 średnie · 🟡 niskie.

---

## 1. ✅ ROZWIĄZANE — Cała praca M7+M8 była NIEZACOMMITOWANA

**Opis (historyczny):** do 2026-07-08 ostatni commit to był `f09139b` (M6,
2026-07-06). M7 real forces, M8 rig/poza/droop, system zrzutów,
`jozz_vehicle_m6_config_io`, presety, przebudowa UI, poprawka fontu, naprawa
bugów zakładek/sesji — 25 niezacommitowanych plików, ~tydzień pracy poza
historią gita.

**Rozwiązanie (2026-07-08):** pogrupowane w 2 commity (`1446c9d` kod+narzędzia,
`d2da267` dokumentacja) i wypchnięte na `jozz-vehicle-sandbox-m0`. Przy okazji
Jozz ustanowił trwałą zasadę: **agenci odtąd samodzielnie commitują i pushują
na `jozz-vehicle-sandbox-m0`**, gdy bramka (build+walidator+test) jest zielona
— nie czekają na osobną prośbę per commit. `main` zostaje wyłącznie dla Jozza.
Zasada opisana w README_FOR_AGENTS §4/§5.

**Watch-item na przyszłość:** ten scenariusz (tydzień pracy bez commitów) nie
powinien się już powtórzyć przy nowej zasadzie — jeśli się powtórzy, to sygnał
że agent nie stosuje bramki/dyscypliny z README.

---

## 2. 🟠 Dokumentacja spóźnia się o ~1 kamień i się rozrasta

**Opis:** wzorzec pracy był „każdy kamień = nowy `docs/*_PL.md` + dopisanie do
dwóch indeksów, bez przycinania". Efekt: **~40 plików w `docs/`**, dwa nakładające
się pliki wejściowe (README_FOR_AGENTS był na M6, CURRENT_STATE_INDEX na M7 —
kod jest na M8), zdublowane 40-punktowe listy czytania, README wprost zaprzeczał
kodowi (mówił o „self-align assist" którego M7 usunął).

**Ryzyko:** nowy agent czyta nieaktualny front i albo powiela zrobioną pracę,
albo łamie zasadę, albo traci godziny na archeologię M3B/M4.

**Plan (częściowo zrobiony w tym przeglądzie):** README_FOR_AGENTS przepisany na
jeden chudy, aktualny front (M8). CURRENT_STATE_INDEX odchudzony i skorygowany.
**Do zrobienia dalej:** trzymać dyscyplinę — po realnej zmianie aktualizować
§2 README + ledger, a NOWY `docs/*.md` tworzyć tylko dla znaczącego kamienia, nie
per drobiazg. Rozważyć przeniesienie plików M0–M5 do `docs/archive/` (odłożone —
duży szum w gicie, decyzja Jozza).

---

## 3. 🟠 System UI + presetów nie ma własnego raportu

**Opis:** przebudowa UI (polski, 6 zakładek, poprawka fontu Segoe UI + `/utf-8`),
system presetów (`jozz_vehicle_m6_config_io`, `assets/vehicle_presets/`,
auto-zapis sesji naprawiający „R" kasujące strojenie) oraz naprawione bugi
(niestabilne ID zakładek ImGui, combo bez zaznaczenia, brak potwierdzenia resetu)
— to praca z 2026-07-08 opisana **tylko** w README §2 i w prywatnej pamięci agenta
Claude. Raport M8 kończy się na 2026-07-07 (opadające wahacze).

**Ryzyko:** agent inny niż Claude (albo Claude w świeżym projekcie bez pamięci)
nie ma repo-widocznego zapisu jak działa system presetów/sesji ani czemu UI
wygląda tak jak wygląda.

**✅ ZAMKNIĘTE (2026-07-08):** `docs/SUBSYSTEM_UI_PRESETS_PL.md` — zwięzła mapa
trzech plików zapisu (sesja/presety/debug-session) i dlaczego, kolejność
zakładek, wzorzec `###stableID`. Podpięte w README §9.

---

## 4. 🟡 Rozrost env-hooków w `jozz_vehicle_m6_rig_lab.cpp`

**Opis:** 15 zmiennych `JOZZ_M6_*` w jednym pliku, wymieszane bez oznaczenia:

- **Stabilne narzędzia** (zostają): `JOZZ_M6_CAM`, `JOZZ_M6_DIAG`, `JOZZ_M6_WHEEL`,
  `JOZZ_M6_DUMPER`, `JOZZ_M6_MOUNT`, `JOZZ_M6_HERTZ`, `JOZZ_M6_DAMP`,
  `JOZZ_M6_PRELOAD`, `JOZZ_M6_DROOP`, `JOZZ_M6_PRESET`, `JOZZ_M6_TAB`,
  `JOZZ_M6_DUMP`, `JOZZ_M6_ARMTINT`.
- **Jednorazowe sondy regresji** (można usunąć): `JOZZ_M6_DIRTY_AT_FRAME`
  (weryfikacja bugu ID zakładek), `JOZZ_M6_TEST_RESET_MODAL` (weryfikacja modala).
  Ich bugi są naprawione i zweryfikowane; są nieszkodliwe (env-gated, domyślnie
  off), ale to rusztowanie testowe.

**Ryzyko:** niskie (nieszkodliwe), ale nowy agent nie odróżni narzędzia od
rusztowania; powierzchnia rośnie organicznie.

**Plan:** albo usunąć dwie sondy testowe (mała zmiana + rebuild + walidator), albo
zostawić i traktować ten wpis jako ich dokumentację. Świadomie ZOSTAWIONE w tym
przeglądzie (nie ruszam runtime kodu bez powodu) — do decyzji przy okazji.

---

## 5. 🟠 Sufit droop 16° (over-center Ackermanna) — fizyka

**Opis:** agresywne opadanie wahaczy > ~16° wpycha trapez kierowniczy w martwy
punkt (over-center): zmierzono podwojone przełożenie kierownicy (69° zamiast 32°)
i camber lądowania 13.6°, niedeterministycznie. 15° to zweryfikowany bezpieczny
sufit dla obecnej geometrii (UI klamruje suwak na 16° z ostrzeżeniem).

**Ryzyko:** pełna agresywna poza (poziom referencji Blockbench, którą Jozz
pokazywał) jest niedostępna bez przeprojektowania geometrii kierownicy
(`steeringArmBack`, `ackermannFraction`, bump-steer lift dopasowany do większego
kąta). Jozz wybrał ten kierunek, ale to wieloetapowa, ostrożna robota.

**Plan:** odłożone. Gdy wracamy — współprojektować kierownicę z wahaczami krok po
kroku, weryfikując walidatorem po każdym kroku (NIE ufać samemu `OK`, czytać
drukowane kąty). Opisane w `M8_SUSPENSION_RIG_REPAIR_PLAN_PL.md` §9.

---

## 6. 🟡 Pasek „Zastosuj" może zjechać poza widok

**Opis:** panel prawy jest przycięty do wysokości viewportu i przy przekroczeniu
scrolluje się w CAŁOŚCI. Po rozwinięciu wszystkich sekcji „Zaawansowane" w
zakładce Zawieszenie na niższym ekranie pasek „Zastosuj (przebuduj pojazd)" na
dole może zniknąć poza widokiem — użytkownik ma niezastosowane zmiany i nie widzi
przycisku.

**Ryzyko:** niskie-średnie UX; user może myśleć że Apply nie działa.

**Plan:** rozważyć osobny scrollowalny `BeginChild` na treść zakładek, a pasek
presetów (góra) i pasek Apply (dół) trzymać zawsze widoczne poza scrollem.
Odłożone (drobna, ale realna zmiana UI; Jozz właśnie zaakceptował layout).

---

## 7. 🟡 Trzy pliki ~1500–2000 linii (watch-item, nie refactor teraz)

**Opis:** `jozz_vehicle_visual_mesh.cpp` (1941), `jozz_vehicle_m6_rig_lab.cpp`
(1632), `jozz_vehicle_m6_suspension_rig.cpp` (1482). `rig_lab` miesza najwięcej
odpowiedzialności (input, kamera, UI 6 zakładek, render modeli, telemetria,
presety, env-hooki). `config_io` i `m5_test_course` już wydzielone (dobrze).

**Ryzyko:** niskie dziś, rośnie z każdą zakładką/funkcją UI dokładaną do `rig_lab`.

**Plan:** NIE refaktoryzować teraz (kod zaakceptowany, refactor = szum + ryzyko
regresji). Watch-item: gdy `rig_lab` znów urośnie, wydzielić rysowanie zakładek do
osobnego TU. `visual_mesh` jest duży, ale spójny (ładowanie+rysowanie glTF) —
zostawić.

---

## 8. 🟠 Dwa narzędzia agentowe (Codex + Claude), rozdzielona pamięć

**Opis:** projekt był prowadzony przez agentów **Codex** (handoffy
`docs/CODEX_*`) i **Claude Code** (prywatna pamięć w `~/.claude`, aktualna do M8).
Nie ma jednego źródła prawdy, które oba czytają: repo-docs były Codex-era i
spóźnione, pamięć Claude jest aktualna, ale prywatna i niewidoczna dla Codeksa.

**Ryzyko:** średnie dla ciągłości wielo-agentowej — stan zależy od tego, który
agent i czy ma pamięć.

**Plan (częściowo zrobiony):** `README_FOR_AGENTS.md` jest teraz repo-widocznym,
aktualnym, wspólnym źródłem prawdy dla OBU narzędzi. Trzymać go jako front — każdy
agent aktualizuje jego §2 po realnej zmianie stanu, niezależnie od prywatnej
pamięci.

---

## 9. Świadomie odłożone (roadmapa, nie „dług") — żeby nie zaskoczyło

Nie są zepsute, są planowo poza zakresem v0. Wypisane, żeby nikt nie „odkrył" ich
jako braków: soft-tire (deformacja opony), drivetrain (dyfry/split momentu/engine
brake), model opony (krzywa poślizgu, wrażliwość na obciążenie), import
hardpointów wahaczy z markerów assetu, dwa boczne dampery. Kolejność w
README_FOR_AGENTS §7.
