# JES_06 — Symulacja startu nowego repo i plan fundamentu

Warstwa: **SYMULACJA + PLAN WYKONAWCZY FUNDAMENTU**. Wersja: 0.9-kandydat
(2026-07-15, autor: Claude). Wejście: pakiet JES_00–05 (1.0-kandydat).
Przeznaczenie: Sol wykonuje na tym dokumencie finalną krytykę, poprawę
i uzupełnienie (§11); po jego przejściu i decyzjach Jozza z §7/F0 powstaje
nowe repo. Dokument samowystarczalny w ramach pakietu JES.

Metoda: przed zaprojektowaniem fundamentu przeprowadzono **symulację
mentalną** pierwszych dni, tygodni i miesięcy nowego projektu — każdy
scenariusz to przewidywana awaria z uzasadnieniem (w miarę możliwości
popartym realnym incydentem z dziedzictwa VAW/JV lub faktem zmierzonym
w tym repo) i kontrśrodkiem wpiętym w plan F0–F5 (§7). Symulacja nie jest
dowodem — jest tanim falsyfikatorem planu, zanim zapłacimy za niego czasem.

---

## 1. Format wpisu symulacji

`SIM-xx: scenariusz → czemu tak się stanie (dowód/precedens) → kontrśrodek
(adres w planie)`. Statusy przewidywań: wszystkie `HYPOTHESIS` — ale
kontrśrodki są tanie i wchodzą do fundamentu niezależnie od tego, czy
scenariusz by się ziścił (asymetria kosztów).

## 2. Symulacja: DZIEŃ 0 (utworzenie repo)

**SIM-01 — start bez bramki decyzji.** Agent „pomocnie" tworzy szkielet
repo, zanim Jozz ratyfikował konstytucję (D16) i nazwał projekt (D2).
Konstytucja zostaje „kandydatem" na zawsze — dokładnie tak, jak status
`1.0-kandydat` potrafi wisieć miesiącami, a nieratyfikowane zasady nie
wiążą nikogo. Precedens: w JV plan mapy był realizowany równolegle
z pisaniem planu (audyt 2026-07-13: E3 REJECTED fizycznie w świecie).
→ **F0 to twarda bramka: bez kompletu decyzji startowych żaden agent nie
tworzy struktury.** Zapis w AGENT_ENTRY nowego repo od pierwszego commita.

**SIM-02 — dwa harnessy, dwie prawdy.** Claude Code czyta `CLAUDE.md`,
Codex czyta `AGENTS.md`. Po miesiącu każdy plik ewoluował osobno i daje
sprzeczne instrukcje (inne zakazy git, inne ścieżki, inne statusy).
Precedens: już dziś pamięć Claude i kontekst Sol-a wymagały jawnego
scalania (JES_03 §7). → **Jeden kanoniczny `docs/AGENT_ENTRY.md`;
`CLAUDE.md` i `AGENTS.md` to ≤5-liniowe wskaźniki na niego** (F1). Edycja
AGENT_ENTRY = edycja wspólnej umowy, widoczna dla obu.

**SIM-03 — koniec linii i polskie znaki psują dowody.** Windows + git
`autocrlf` + fixture'y tekstowe = hash fixture'a inny po checkout'cie niż
przy zapisie → „sabotaż" P0 odpala się na fałszywym alarmie albo, gorzej,
progi hashowe są luzowane „bo Windows". Do tego MSVC bez `/utf-8` sypie
C4819/krzaki na polskich komentarzach i docs. → **F1: `.gitattributes`
(jawne `eol` per typ; fixture'y i wejścia hashowane jako `-text`),
`.editorconfig`, globalne `/utf-8`** — zanim powstanie pierwszy fixture.

**SIM-04 — build zależny od sieci.** Obecny host samples ciągnie ImGui
v1.92.7, implot, nfd i sokol-shdc przez `FetchContent` z GitHuba
(`samples/CMakeLists.txt:3–41`). W nowym repo oznacza to: klon na czystej
maszynie bez sieci / z paranoicznym proxy nie zbuduje się; upstream może
zniknąć lub przepisać tag; build przestaje być odtwarzalny = łamie
odtwarzalność RUN-EVIDENCE. → **D20 + F2: pełny vendoring kopii
w `vendor/` z `VENDOR.md` (wersja, SHA, źródło, data, licencja);
aktualizacja wersji tylko przez ADR.** Sieć w buildzie = zero.

## 3. Symulacja: PIERWSZE DNI (bootstrap, dni 1–3)

**SIM-05 — kto bramkuje bramkę?** P0 (testy-sabotaże) wymaga
infrastruktury dowodów, która sama jeszcze nie jest bramkowana —
klasyczny problem startu. Ryzyko: uprząż dowodów pisana „na szybko"
przechodzi własne sabotaże przypadkiem (np. sprawdza istnienie pliku,
a nie jego świeżość). → **F3/F4: uprząż najpierw, sabotaże jako JEJ
odbiór; werdykt F4 podpisuje Jozz ręcznie** (jedyny etap, gdzie recenzentem
maszynerii jest człowiek, bo maszynerii jeszcze nie ma).

**SIM-06 — pin box3d dziedziczy założenia bez weryfikacji.** Upstream
odjechał ~10 commitów od merge-base tego forka; świeży pin (D5) może
zmienić fakty, na których stoi program (oś zawiasu = lokalne Z, brak
update-in-place heightfielda, statyczny CRT, `EnableContinuous`).
Przeniesienie tych faktów „na wiarę" = grzech z JES_04 §2 (mechanizm
starego hosta jako prawda). → **F2: „suita charakteryzacji pinu"** — mały
zestaw testów, który przy KAŻDYM podbiciu pinu na nowo dowodzi faktów
z JES_02 §7 (asserty na API + jeden test behawioralny jointa + test CRT
przez faktyczne linkowanie).

**SIM-07 — D10 (double precision) odkładane aż zaboli.** Precyzja to flaga
kompilacji box3d: zmiana później unieważnia naraz ABI, zapisy, replaye
i baseline'y wydajności. Jeżeli F2 zbuduje pin bez decyzji, pierwsze
fixture'y zabetonują ją milcząco. → **D10 wchodzi do bramki F0 jako
decyzja provisional z benchmarkiem w P1** (rekomendacja z JES_03: ON);
suita charakteryzacji jest uruchamiana w wybranym wariancie.

**SIM-08 — zrzut ekranu „później".** Presja na pierwsze okno sokol+ImGui
jest duża; uprząż screenshotów wypada z zakresu „na razie". Dwa tygodnie
później Canary A jest odbierany „po liczbach" — złamane L7 (render is the
gate), czyli powtórka najdroższej lekcji JV. → **F3: narzędzie zrzutu
stałego kadru to warunek DONE fundamentu; Canary A bez PNG w RUN-EVIDENCE
nie istnieje.**

## 4. Symulacja: PIERWSZE TYGODNIE (canary, laby)

**SIM-09 — cztery konwencje osi spotykają się bez umowy.** Blockbench
(Y-up, jednostka=piksel/16), glTF (Y-up, prawoskrętny, metry), box3d
(Y-up, metry, Z-lokalne zawiasy), sokol/render (własne NDC). Bez pinu
konwencji każdy asset dostaje ad-hoc „fixup" w kodzie — po miesiącu nikt
nie wie, która macierz jest prawdą (lekcja VAW: semantyczne przestrzenie
współrzędnych; lekcja JV M9: WheelCenter/ChassisMount rozjazd).
→ **F1: `docs/conventions/UNITS_AXES.md` jako KANDYDAT + asset-wzorzec
„kostka osi" (oznaczone ściany X/Y/Z)**; ratyfikacja konwencji = round-trip
kostki przez cały pipeline w Canary A (Blockbench→glTF→render→zrzut).

**SIM-10 — host labów rośnie w drugi M6.** „Trwały host labów" zaczyna
przygarniać kamerę, loader assetów, zapis, gizma… — po kwartale host JEST
produktem, tylko nikt tego nie zdecydował (dokładna etiologia monolitu M6,
CAP-JV-10). → **F1: host ma jawną BIAŁĄ LISTĘ zdolności** (okno, input,
pętla czasu, rejestr paneli ImGui, zrzut, zapis RUN-EVIDENCE) wpisaną
w jego README; wszystko poza listą żyje w labie albo w promowanym module;
przegląd adwersaryjny (soczewka 2) pilnuje listy na każdej bramce.

**SIM-11 — granice zależności istnieją tylko na papierze.** Reguły
z JES_02 §4 (produkt ─X→ laby; domena ─X→ typy silnika; uchwyty ─X→ zapis)
są dokumentem — a pod presją feature'u ktoś „na chwilę" wstawi `b3BodyId`
do UI albo `#include "box3d/..."` poza adapterem. Dokument tego nie
zatrzyma; w JV overlaye E3 weszły do aktywnego świata bezwarunkowo mimo
planu. → **F3: mechaniczny `boundary-check`** (skrypt: zakazane include'y
/ zakazane typy w katalogach; ~50 linii) uruchamiany w każdej bramce S0.
Granica, której nie sprawdza maszyna, nie jest granicą.

**SIM-12 — wydajność mierzona w Debug.** Pierwsze liczby Canary T
(rebuild < 1 ms/chunk) ktoś zmierzy w Debug/MTd — wynik 5–20× gorszy albo
(gorzej) przypadkiem lepszy przez inny layout. Precedens: liczby JV
(1.15 ms/step) to Debug i nie wolno ich porównywać z niczym. → **F3:
RUN-EVIDENCE wymaga pola „profil buildu"; progi wydajności definiowane
wyłącznie dla Release; uprząż odmawia zapisu claimu PERFORMANCE z Debug.**

**SIM-13 — dowody puchną w repo.** PNG-i, replaye i logi trafiają do gita
„żeby nie zginęły" — po miesiącu repo waży setki MB, klony się wleką,
agenci czekają. → **F1/F3: `evidence/` jest gitignored; commitowane są
MANIFESTY (hash, ścieżka, RUN-ID) + świadomie wybrane małe „złote kadry".**
Ciężkie artefakty żyją lokalnie/w archiwum poza gitem (polityka w D18-bis
razem z decyzją o hostingu D2).

**SIM-14 — spike'i i fixture'y każdy w innym JSON.** Trzy równoległe
kierunki (art / mechanika / teren) wymyślają trzy dialekty zapisu:
inne nazwy pól, inna obsługa wersji, nieznane pola raz ignorowane, raz
walone błędem. Scalanie po fakcie = ból VAW. → **F1:
`docs/conventions/JSON.md`** (tabela pól, pole `version` obowiązkowe,
polityka nieznanych pól wg D15 — decyzja przy P2, do tego czasu
reject-with-diagnostic) obowiązuje każdy plik zapisywany przez każdy lab.

## 5. Symulacja: PIERWSZE MIESIĄCE (równoległość, dryf, ludzie)

**SIM-15 — dokumentacja zaczyna kłamać.** Docs opisują stan zamierzony
jako dokonany („generator terenu wspiera…"), bo plan i realizacja żyją
w tych samych plikach. Precedens: audyt mapy JV 2026-07-13 — plan mówił
E2/E3, świat fizyczny mówił co innego. → **F1: JEDEN plik `as-built`:
`docs/CURRENT_STATE.md`, aktualizowany WYŁĄCZNIE przy promocji z labu
(wpis S0); wszystko inne nosi nagłówek statusu i datę.** Soczewka 3
(prokurator dowodów) porównuje CURRENT_STATE z rzeczywistością na bramkach.

**SIM-16 — kolizje równoległych strumieni.** Dwóch agentów (Claude
i Sol) + Jozz edytują ten sam rejestr decyzji / ten sam spec → konflikty
merge, zduplikowane ID (dwa razy „D17"), renumeracja niszcząca odwołania.
→ **F1: dyscyplina rejestrów — pliki-rejestry są append-only, ID nigdy
renumerowane, każdy rejestr ma jednego stewarda; praca kodowa
w krótkich gałęziach per strumień (osobne katalogi labów = naturalna
separacja).** Konflikt ID = konflikt do Conflict Record, nie do „poprawki".

**SIM-17 — głód decyzji Jozza.** Kolejka PDR rośnie szybciej, niż Jozz ma
czas sesji; agenci albo stoją, albo (gorzej) decydują sami i po cichu.
→ **Protokół provisional (F1, do AGENT_ENTRY): agent może podjąć decyzję
ODWRACALNĄ ze statusem `PROVISIONAL` + revisit trigger + wpisem do
kolejki; decyzje konstytucyjne/produktowe/feel — nigdy** (`WAITING_FOR_JOZZ`
kończy turę, JES_00 §6). Pakiety decyzyjne 5–7/sesję (JES_05 §5) z polem
„koszt niewiedzy" wymuszają priorytet.

**SIM-18 — erozja progów.** Trzeci z rzędu FAIL na progu (np. 1 ms/chunk)
rodzi pokusę „podnieśmy na 2 ms, przecież to arbitralne". Raz poluzowany
próg bez śladu = koniec zaufania do wszystkich progów (L10). → **Zmiana
progu wymaga wpisu ADR z uzasadnieniem i ponownym przebiegiem sabotaży
danej bramki** (kodeks JES_03 §11 pkt 2 + F4).

**SIM-19 — stare repo nie umiera.** Inercja: „szybka poprawka" w JV,
„mały eksperyment" w VAW — nowe repo głoduje, a wiedza znów rozjeżdża się
na trzy miejsca. → **Po migracji (F5): oba stare repo dostają baner
FREEZE w README** (co tu jest, gdzie jest kontynuacja, czego nie wolno);
eksperymenty wyłącznie w nowym repo `labs/`. Wyjątek: odtworzenie dowodu
dziedzictwa przy jego labie (JES_04 §2).

**SIM-20 — spalanie kontekstu agentów.** Każda sesja zaczyna się od
wczytania 6+ plików JES (dziesiątki tysięcy tokenów) → wolny start, drogi
plan (dyrektywa oszczędności tokenów Jozza). → **F1: AGENT_ENTRY ≤150
linii z mapą „przy zadaniu X czytaj tylko Y"**; pakiet JES już jest
pocięty warstwami — wystarczy nie kazać czytać całości; CURRENT_STATE
zastępuje archeologię po docs.

## 6. Wnioski zbiorcze z symulacji (meta-wzorce)

1. **Największe ryzyka startu nie są techniczne, tylko proceduralne:**
   start bez ratyfikacji (SIM-01), dwie prawdy agentowe (SIM-02), głód
   decyzji (SIM-17), kłamiące docs (SIM-15). Technika (box3d, sokol) jest
   zderyskowana X0 — proces nie jest zderyskowany niczym oprócz tego planu.
2. **Każda reguła musi mieć maszynowego strażnika albo jest życzeniem:**
   granice → boundary-check; dowody → run.json+sabotaże; render → zrzut
   w DONE; progi → ADR-gate. Dokument sam nie zatrzymał jeszcze żadnej
   awarii w historii obu dem.
3. **Kolejność fundamentu wynika z zależności dowodowych, nie z ambicji:**
   konwencje przed pierwszym assetem, uprząż przed pierwszym labem,
   charakteryzacja pinu przed pierwszym fixture, decyzje przed strukturą.
4. **Fundament ma być NUDNY i mały.** Wszystko, co pachnie produktem
   (kernel/, adapters/, sceny), powstaje dopiero w P2/P4 — cienki-spec-first
   dotyczy też drzewa katalogów.

## 7. Plan fundamentu F0–F5 (realizuje checklistę JES_03 §12 + P-1 + P0)

Każdy etap ma kryteria DONE; progów nie luzujemy (kodeks §11).

**F0 — Bramka decyzji startowych (Jozz; blokuje wszystko).**
Do rozstrzygnięcia: **D16** (ratyfikacja konstytucji + rejestru dyrektyw),
**D2** (nazwa, miejsce, hosting), **D14** (lista assetów właściciela +
provenance), **D10** (precyzja — provisional, benchmark w P1), **D17–D20**
(§10), **WP-00** w repo JV (domyka P-1 po stronie JV). DONE: wpisy
w dzienniku decyzji ze statusami; baseline'y VAW/JV zamrożone (hash +
manifest) = P-1 zamknięte.

**F1 — Szkielet, higiena, umowy (½–1 dzień agenta).**
`git init`; `.gitattributes` (eol + `-text` dla fixture'ów),
`.editorconfig`, `.gitignore` (`evidence/` ciężkie, build), LICENSE (D19);
drzewo z §8; `docs/AGENT_ENTRY.md` (≤150 linii: bramka F0, zakazy, mapa
czytania, protokół provisional, dyscyplina rejestrów) + `CLAUDE.md`/
`AGENTS.md` jako wskaźniki; `docs/CURRENT_STATE.md` (pusty, z regułą
aktualizacji); migracja pakietu wg manifestu §9; konwencje-kandydaci
(`UNITS_AXES.md`, `JSON.md`, `NAMING.md`); README hosta labów z białą
listą. DONE: świeży klon czyta się od AGENT_ENTRY do pierwszego zadania
bez wiedzy plemiennej.

**F2 — Kręgosłup budowania (1–2 dni agenta).**
Vendoring kopii (D20): box3d@pin (D5, wariant precyzji z D10), sokol,
Dear ImGui (docking), cgltf, stb, miniaudio + `VENDOR.md`; CMake presets
(x64 Debug/Release, `/utf-8`, statyczny CRT udokumentowany); **build
jednym poleceniem bez sieci**; test świeżego klonu (klon do drugiego
katalogu → build → run); **suita charakteryzacji pinu** (fakty JES_02 §7
jako testy). Okno sokol + ImGui + trójkąt = smoke hosta. DONE: zielony
przebieg na świeżym klonie + suita charakteryzacji PASS.

**F3 — Uprząż dowodów (1–2 dni agenta; mięsień P0).**
`tools/`: zapis `run.json` (RUN-ID, komenda, exit, rewizja+dirty, profil
buildu, hashe wejść, artefakty — szablon JES_05 §4); zrzut stałego kadru
z hosta; hash trajektorii (logika z X0 napisana NA NOWO jako narzędzie);
`boundary-check` (zakazane include'y/typy per katalog); manifest
`evidence/`; spięcie w CTest. Zasada: claim PERFORMANCE tylko z Release
(SIM-12). DONE: jeden przebieg referencyjny zapisany kompletnym RUN-EVIDENCE.

**F4 — P0: sabotaże bramki (½ dnia; odbiera Jozz).**
Sześć kontrolowanych awarii z JES_03 §2/P0 (build non-zero, nieuruchomiony
test, brak artefaktu, stary timestamp, zła wersja fixture, dirty udający
clean) + siódma: fixture z podmienionym EOL (SIM-03). DONE: każda awaria
wykryta i niemaskowalna; werdykt ręczny Jozza (SIM-05).

**F5 — Chartery canary i zamknięcie fundamentu (½ dnia + sesja Jozza).**
Lab Chartery P1: **A** (pierwszy asset Jozza — zakres z D7; round-trip
kostki osi ratyfikuje UNITS_AXES), **M** (force-at-point/one-corner +
rolling rig; hash trajektorii w dowodzie), **T** (spec gotowy w JES_03 §2
— strategie A/B collidera, progi). Banery FREEZE w obu starych repo
(SIM-19). DONE: trzy chartery zaakceptowane; checklista JES_03 §12
w całości odhaczona → **start P1**.

Szacunek łączny: **3–5 dni pracy agentów + 2 sesje decyzyjne Jozza.**
Fundament świadomie NIE zawiera: kernela, adapterów, ECS (D3 — spike przy
P2), gracza, terenu, sieci, CI w chmurze (lokalny skrypt bramki wystarcza
do czasu decyzji o hostingu w D2).

## 8. Drzewo katalogów v0 (minimalne; celowo bez kernel/ i adapters/)

```text
<repo>/
  CLAUDE.md, AGENTS.md          → wskaźniki (≤5 linii) na docs/AGENT_ENTRY.md
  README.md, LICENSE, .gitattributes, .editorconfig, .gitignore
  CMakeLists.txt, CMakePresets.json
  docs/
    AGENT_ENTRY.md              # jedyna umowa agentowa; ≤150 linii
    CURRENT_STATE.md            # jedyny plik „as-built"; edycja tylko przy promocji
    jes/                        # JES_00–06 po migracji (§9)
    vision/                     # WIZJA v0.1 Jozza — nietykalna
    heritage/                   # snapshot pakietu Sol (D18), archiwum analiz, kod X0 (D17)
    decisions/                  # PDR/ADR/Conflict — plik per wpis; ID nigdy renumerowane
    conventions/                # UNITS_AXES / JSON / NAMING (kandydaci → ratyfikacja testem)
  vendor/                       # kopie: box3d@pin, sokol, imgui, cgltf, stb, miniaudio + VENDOR.md
  tools/                        # run.json writer, screenshot, traj-hash, boundary-check
  labs/
    host/                       # trwały; README z białą listą zdolności (SIM-10)
    <nazwa-labu>/               # usuwalne implementacje
  fixtures/                     # wejścia testów; hashowane, `-text`
  evidence/                     # gitignored ciężkie; commitowane manifesty i złote kadry
```

`kernel/` powstaje przy P2 (pierwszy spec), `adapters/` przy P4 (granica
backendu) — wtedy boundary-check dostaje ich reguły.

## 9. Manifest migracji (co, dokąd, z jaką transformacją)

| Materiał (źródło) | Cel w nowym repo | Transformacja |
|---|---|---|
| `docs/JES_00–05` (to repo) | `docs/jes/` | przepisanie ścieżek wzajemnych; JES_00 §5 (stan repo JV) → zastąpione sekcją o stanie NOWEGO repo + wskaźnikiem na archiwum; statusy `1.0-kandydat` → `1.0` po D16 |
| ten plik (JES_06) | `docs/jes/` | po krytyce Sol-a i akceptacji Jozza; sekcje symulacji zachowane jako uzasadnienie fundamentu |
| `WIZJA_JOZZ_ENGINEERING_SANDBOX_V0_1.md` | `docs/vision/` | kopia 1:1, bajt w bajt; NIGDY nie edytować |
| `ANALIZA_KRYTYCZNA_...`, `PLAN_NOWY_PROJEKT_...` | `docs/heritage/archiwum/` | kopia z istniejącymi nagłówkami supersede |
| pakiet Sol `jes_pre_foundation_2026_07_15/00–07` (repo VAW) | `docs/heritage/sol_pre_foundation/` | wg D18 (rekomendacja: snapshot read-only — nowe repo nie może zależeć od starych repo, JES_04 §3); Capability Ledger (29 kart) staje się roboczym rejestrem dziedzictwa |
| `spike/kernel_v0/` (kod + wyniki X0) | `docs/heritage/spike_kernel_v0/` | wg D17 (rekomendacja: snapshot jako REFERENCJA + RUN-EVIDENCE X0; do labów przy P2 pisany NA NOWO wg JES_04 §2 — snapshot wolno czytać, bo to materiał nowego projektu, nie dem) |
| assety Jozza z listy D14 | `fixtures/` / `assets/` wg charterów | każdy z wpisem provenance (JES_04 §3) |
| **NIE migruje się:** kod/dane JV i VAW, dokumentacja mapy JV, build systemy dem, stare liczby tuningu | — | dostępne w zamrożonych repo; klasa `REFERENCE_ONLY` |

Po migracji: banery FREEZE w obu starych repo (F5); WP-00 pozostaje
niezależną decyzją Jozza w repo JV (kwarantanna z JES_00 §5 obowiązuje
do końca).

## 10. Nowe pozycje dziennika decyzji (dopisane do JES_03 §10)

- **D17** — los `spike/kernel_v0`: snapshot referencyjny w heritage
  (rekomendacja) vs pozostawienie w zamrożonym JV vs migracja jako kod
  startowy labu (odradzane — omija bramkę P2).
- **D18** — pakiet Sol: snapshot w nowym repo (rekomendacja —
  samowystarczalność) vs sam wskaźnik na repo VAW.
- **D19** — licencja nowego repo (prywatne? MIT jak box3d? decyzja Jozza;
  wpływa na hosting i vendoring licencji zależności w `VENDOR.md`).
- **D20** — polityka vendoringu: kopie w `vendor/` (rekomendacja; build
  bez sieci, SIM-04) vs FetchContent z pinem SHA.

## 11. Pałeczka dla Sol-a (zakres finalnej krytyki)

Sol wykonuje jeden przebieg trzech soczewek (JES_05 §6) na tym dokumencie
oraz konkretnie:

1. **Braki symulacji:** czego ta symulacja nie przewiduje z perspektywy
   dziedzictwa VAW (transakcje edycji, art pipeline, persystencja
   z recovery)? Dopisać SIM-y z kontrśrodkami albo jawnie uznać pokrycie.
2. **Atak na kolejność F0–F5:** czy któryś krok da się skreślić (fundament
   ma być mniejszy, nie większy)? Czy któraś zależność jest odwrócona?
3. **Manifest migracji (§9):** kompletność względem pakietu Sol —
   zwłaszcza czy snapshot heritage zawiera wszystko, czego wymaga jego
   protokół clean-room; werdykt do D17/D18.
4. **Chartery canary:** uzupełnić szkice charterów A/M/T (F5) o kryteria
   z jego programu HKP, jeśli plan z JES_03 §2 czegoś nie pokrywa.
5. **Boundary-check i biała lista hosta:** czy reguły są egzekwowalne
   mechanicznie od F3, czy któraś wymaga doprecyzowania katalogów?
6. Raport wg formatu przeglądu: 3 najmocniejsze elementy, 3 najgroźniejsze
   błędy, ukryte założenia, falsyfikatory, pytania do Jozza. Poprawki
   nanosi do TEGO pliku (wersja 1.0-kandydat) z oznaczeniem zmian.

Po przejściu Sol-a i decyzjach F0 Jozz tworzy repo — i zaczyna się F1.
