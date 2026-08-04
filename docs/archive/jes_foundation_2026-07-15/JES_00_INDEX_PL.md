> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# JES — Jozz Engineering Sandbox: FINALNY PAKIET ZAŁOŻYCIELSKI (indeks)

Wersja pakietu: **1.0-kandydat** (2026-07-15, po scaleniu dwóch niezależnych
pakietów agentowych). Właściciel projektu i wszystkich decyzji: **Jozz
(Przemek)** — odpowiadamy mu po polsku. Pakiet jest samowystarczalny dla
agenta bez kontekstu (Claude, Codex/GPT Sol i kolejni).

**Status:** kandydat do ratyfikacji Jozza; zaprojektowany do przeniesienia
W CAŁOŚCI do nowego repo (następne zadanie: plan czystego projektu
i migracja). Do tego czasu żyje tutaj jako komplet `docs/JES_*`.

---

## 1. Czym jest projekt (jedno zdanie)

Nowy, wieloletni projekt gry: **otwarty sandbox inżynieryjny** — gracz
konstruuje maszyny (lądowe, robocze, latające), przekształca teren, buduje
infrastrukturę i automatyzuje pracę, a wszystko ocenia wspólna fizyka,
bilans materii i mierzalny rezultat. Start **od zera** w nowym repo;
VAW i JozzVehicle = zamrożone dema-dowody: dawcy wiedzy, scenariuszy
i porażek — **nigdy kodu**.

## 2. Rodowód tego pakietu (skąd się wziął i co zastępuje)

Powstały niezależnie dwa pakiety przygotowawcze:

1. **Pakiet Claude** (`JES_00–03` v0.2, repo JV `docs/`) — konstytucja,
   stack, twarde fakty silnika, program X, prawa inżynierskie.
2. **Pakiet Codex/Sol** („pre-foundation pack", repo VAW
   `docs/jes_pre_foundation_2026_07_15/00–07`) — epistemologia źródeł,
   protokół clean-room, Capability Ledger VAW/JV, program HKP (P-1…P10),
   szablony decyzji.

Zbieżność obu pakietów wyniosła ~85% (m.in. lab-first, canary artu/feelu/
terenu wcześnie, teren = ryzyko nr 1, pojazd-instrument przed integracją
gruntu, zakaz kopiowania kodu, deletowalne eksperymenty, sabotage-testy
bramek). **Ten pakiet jest jedyną aktualną wersją**: konflikty rozstrzygnięte
jawnie w `JES_03 §7` (nie uśrednione), oba pakiety źródłowe zachowane bez
zmian do audytu. Planowana przez Sol wielorundowa „finalna burza" (R0–R10)
została ZASTĄPIONA tą udokumentowaną konfrontacją + lekkim przeglądem
adwersaryjnym na bramkach (`JES_05 §6`) — uzasadnienie w `JES_03 §7`.

## 3. Struktura pakietu (czytać w tej kolejności)

| Plik | Warstwa | Zawartość |
|---|---|---|
| `JES_00_INDEX_PL.md` | wejście | ta mapa + autorytet + stan repo |
| `JES_01_WIZJA_I_KONSTYTUCJA_PL.md` | **MARZENIE / PRAWA** | destylat wizji, konstytucja (16 zasad), rejestr dyrektyw właściciela, stanowiska gracz/drogi |
| `JES_02_PROPOZYCJA_WYKONANIA_PL.md` | **HIPOTEZY WYKONANIA** | stack (D1), architektura docelowa vs cienki start, granice zależności, twarde fakty silnika, prawa inżynierskie L1–L12, UI Blender/UE |
| `JES_03_PROGRAM_REALIZACJI_PL.md` | **PROGRAM / STAN / DECYZJE** | scalony program fundamentu (HKP+X: P-1…P10, Canary A/M/T, S0), doktryna dowodów, rozstrzygnięcia konfliktów, dziennik decyzji, minimalny kodeks, gotowość repo |
| `JES_04_DZIEDZICTWO_I_CLEAN_ROOM_PL.md` | **DZIEDZICTWO** | klasy materiału, zachowanie-vs-mechanizm, co wolno/zakazane dziedziczyć, klasyfikacja starych testów, wskaźnik na Capability Ledger, karty |
| `JES_05_DECYZJE_I_EPISTEMOLOGIA_PL.md` | **MASZYNERIA DECYZJI** | hierarchia źródeł, słownik statusów, ścieżki promocji, szablony PDR/ADR/Lab/Run/Konflikt, lekki przegląd adwersaryjny, reguły anty-dryf |
| `JES_06_SYMULACJA_STARTU_I_PLAN_FUNDAMENTU_PL.md` | **SYMULACJA / PLAN FUNDAMENTU** | symulacja mentalna pierwszych dni/tygodni/miesięcy nowego repo (SIM-01…20), plan fundamentu F0–F5, drzewo katalogów v0, manifest migracji, decyzje D17–D20, pałeczka dla Sol-a |

Dokumenty źródłowe (zachowane, nieedytowane; nowsze warstwy JES wygrywają):

- `WIZJA_JOZZ_ENGINEERING_SANDBOX_V0_1.md` — oryginał marzenia Jozza;
- `ANALIZA_KRYTYCZNA_WIZJI_JES_2026_07_15_PL.md` — analiza Claude (K1–K11);
- `PLAN_NOWY_PROJEKT_ULTIMATE_2026_07_15_PL.md` — analiza przedzałożycielska
  (lekcje dem, uzasadnienie stacku D1);
- pakiet Sol: `<repo VAW>/docs/jes_pre_foundation_2026_07_15/00–07` —
  w szczególności `03_VAW_JV_CAPABILITY_LEDGER.md` (29 kart CAP-*) i pełne
  wersje protokołów intake/burzy (na wypadek reaktywacji — trigger w JES_05 §8).

## 4. Hierarchia autorytetu (przy sprzecznościach)

1. bieżąca, jawna decyzja/wypowiedź Jozza;
2. `JES_01` (konstytucja + rejestr dyrektyw);
3. `JES_03` (dziennik decyzji, program, rozstrzygnięcia konfliktów);
4. `JES_02` / `JES_04` / `JES_05` (hipotezy i doktryny);
5. dokumenty źródłowe obu pakietów;
6. kod + odtwarzalne dowody (najwyższy autorytet **dla twierdzeń o realnej
   implementacji** — nie dla decyzji produktowych);
7. reszta `docs/` obu starych repo (historia INNYCH projektów).

Dokument agenta (ten pakiet też!) nie podejmuje decyzji — rekomenduje.
Statusy twierdzeń: słownik w `JES_05 §3`. Zielony build niczego nie
akceptuje produktowo.

## 5. ⚠ STAN TEGO REPOZYTORIUM — przeczytać przed jakimkolwiek git

> **AKTUALIZACJA 2026-07-24.** Kwarantanna WP-00 w tym repo **ZAMKNIĘTA** decyzją Jozza.
> Mieszany WIP został sklasyfikowany, odzyskany (E2R central campus **bez toru E3** + poprawki
> regresji skanu) i scommitowany na `main` („zapisujemy aktualny stan na maina" przed skokiem
> w bok). Snapshot mieszanego WIP: `56c04c1` @ `origin/jozz-map-wip-snapshot-2026-07-24`. Opis
> poniżej (pkt 1) i zakaz `git add -A` dotyczą **fazy kwarantanny, która już minęła**. Szczegóły:
> `../ODZYSK_UTRACONYCH_ZMIAN_2026_07_24_PL.md`.

To repo (fork box3d, projekt JozzVehicle) jest tymczasową bazą planowania:

1. **JozzVehicle zamrożony, worktree BRUDNY:** niezacommitowany mieszany WIP
   mapy (15 M + kilkanaście untracked) w kwarantannie do osobnej decyzji
   Jozza (WP-00, `PLAN_WYKONAWCZY_MAPA_GPT_LUNA_PL.md`). **Zakaz:
   `git add -A`, `git reset --hard`, `git clean`, stage'owania/commitowania
   cudzego WIP.** Zawsze.
2. **Pakiet JES + eksperymenty:** pliki `docs/JES_*`, `docs/WIZJA_*`,
   `docs/ANALIZA_*`, `docs/PLAN_NOWY_*` i `spike/**` — celowo untracked.
   Eksperymenty JES wyłącznie w `spike/<nazwa>/` (własny CMake; wzór
   `spike/kernel_v0/`; box3d linkowany z `build/src/Debug/box3dd.lib`
   + nagłówki; statyczny CRT `/MTd`). Nie dotykać `src/`, `include/`,
   `samples/`.
3. Repo VAW (`.../VAW/voxel-aeronautics-workshop-...`): również zamrożone,
   też ma dirty checkout; pakiet Sol żyje tam jako untracked draft.

## 6. Zasady komunikacji z Jozzem (minimum)

- Po polsku, wnioski przed szczegółami, bez lania wody.
- Jozz podejmuje wszystkie decyzje produktowe/estetyczne/feel; agent
  rekomenduje `ACCEPT/ADAPT/REJECT` i przedstawia najmocniejszy argument
  mniejszości.
- Nieosiągalne kryterium → STOP i pytanie; nigdy ciche poluzowanie progu.
- Praca wizualna: nie raportować „zrobione" bez obejrzenia zrzutu.
- `WAITING_FOR_JOZZ` kończy turę agenta — nie obchodzi się go
  „doprecyzowaniem" zadania.
