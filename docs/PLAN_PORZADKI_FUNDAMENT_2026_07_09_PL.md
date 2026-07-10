# PLAN PORZĄDKÓW — przygotowanie fundamentu pod dalszą rozbudowę

> ## ✅ WYKONANE (etapy A–G, 2026-07-09) — commity a989459 · e19e8db · ea1d1b7 · 4cded89 · bc209cd · 7f0e197 · c0cfad4
> Wszystkie 7 etapów zamknięte, każdy przez `gate.ps1`. Skrót w
> `CHECKPOINTS_PL.md`. Ten dokument zostaje jako zapis CO i DLACZEGO. Następny
> wielki etap (refactoring ciężkich spraw / podział monolitów) czeka na sygnał
> Jozza — ma teraz siatkę bezpieczeństwa (`gate.ps1`).

Cel Jozza (2026-07-09): projekt ma być **idealnie gotowy pod dalszą pracę** —
w tym pod przyszły edytor rigu (którego TERAZ nie projektujemy, ale robimy
porządki z myślą o nim). Priorytet: co może się wysypać, co realnie ułatwia
pracę, co ją spowalnia. Nie feature'y, nie fizyka.

Metoda: każdy etap oddzielny, z własną bramką i checkpointem, wykonywalny
przez tańszego agenta bez zgadywania (jak PLAN_STABILNOSC). **Ten dokument
to mapa — wykonanie dopiero po akceptacji Jozza, etap po etapie.**

---

## CZĘŚĆ A — Krytyczna walidacja dotychczasowego „planu porządków"

Brief §6 zostawił zakres cleanupu jako `[DOPRECYZUJ]`, a mój wcześniejszy
szkic (PLAN_STABILNOSC §8) to była jedna mglista linijka. Słabe strony tego,
co było:

1. **Nie istniał realny plan porządków** — tylko intencja. „Uporządkuj
   workflow" bez listy = każdy agent robi po swojemu = niespójność.
2. **Plany dotąd były wyłącznie fix/feature** (P1–P6, Bramki). Żaden nie
   dotykał **rozjazdów strukturalnych** (build, walidator, docs) — a to
   właśnie one cicho gniją i wybuchają później.
3. **STOP-gate'y opisane jako intencja** („zapytaj Jozza"), nie jako
   sprawdzalny proces. Poprzedni wykonawca „przeramował" cel dokładnie na
   takim niesformalizowanym punkcie.
4. **Doc↔kod: brak mechanizmu.** Brief nazwał to klasą #1 („A2 nie może się
   mnożyć") — a JUŻ się rozmnożyła (README opisuje starą nazwę suwaka arcade,
   patrz Część B). Sama intencja nie wystarcza.
5. **`PLAN_STABILNOSC` sam stał się długiem** — jest DONE (P1–P6), ale czyta
   się jak aktywny, ma dryfujące numery linii i baner P1 częściowo obalony
   późniejszymi ustaleniami. Nowy agent traci czas na ustalanie, co żyje.
6. **Ryzyko nadmiaru (samokrytyka planu):** pokusa „idealnego" fundamentu
   kusi do wielkiego refaktoru plików ~2000-liniowych. To PUŁAPKA — kod
   zaakceptowany, refaktor = szum + regresja (TECH_DEBT #7). Plan MUSI
   oddzielić **tanie anty-rozjazdy** od **drogich refaktorów** i tych drugich
   teraz NIE ruszać.

---

## CZĘŚĆ B — Znaleziska z pomiaru realnego repo (dowody, nie opisy)

| # | Znalezisko (zmierzone) | Klasa | Czemu to boli |
|---|---|---|---|
| B1 | `CMakeLists.txt`: lista źródeł Jozza wpisana RĘCZNIE 2× (target `samples` l.166-186 + `jozz_vehicle_validation` l.246-256) | rozjazd build | dodasz `.cpp`, zapomnisz o 1 targecie → walidator kompiluje stary/niepełny kod albo nie linkuje; cicho |
| B2 | `jozz_vehicle_validation.cpp` = **2691 linii**, ~17 sond w ręcznej liście `ok &= Run...` (l.~2456+) | rozjazd test | napiszesz sondę, zapomnisz dopisać do listy → NIGDY nie biegnie → fałszywa zieleń |
| B3 | README §2 mówi `"Wspomaganie powrotu (arcade)"`, kod (Bramka 1) = `"[ARCADE] Wspomaganie powrotu"`; modelu tarcia P4b w README brak | doc↔kod (A2) | klasa błędu, którą Jozz kazał ZDUSIĆ, już się rozmnożyła |
| B4 | Pliki ~2000 linii: validation 2691, rig_lab 1999, visual_mesh 1968, suspension_rig 1758 | rosnące monolity | rig_lab miesza input/kamerę/UI/render/telemetrię/presety/env; edytor będzie chciał część z tego reużyć |
| B5 | 15 env-hooków `JOZZ_M6_*` w rig_lab, mieszane stabilne+jednorazowe, bez rejestru | powierzchnia | headless-testy (i edytor) potrzebują wiedzieć co jest; nowy agent nie odróżni narzędzia od rusztowania |
| B6 | Klasa persystencji: preset (naprawione), pola poza configiem nie przeżywają R (#12), brak wersji formatu | powracający bug | Jozz oberwał tym 3× (R kasuje strojenie, R kasuje debug, preset in-place). Rdzeń klasy niezmapowany |
| B7 | `PLAN_STABILNOSC_PROWADZENIE_PL.md` DONE, ale bez banera „zamknięte"; 31 docs w root | szum docs | agent nie wie, co aktywne |
| B8 | Lista tasków w harness: 48 pozycji, większość prehistoryczna (M-fazy) | szum | każdy reminder to ściana nieaktualnych tasków |

**Wspólny mianownik B1/B2/B3/B6:** wszystkie to **ręcznie utrzymywane
duplikaty prawdy** (lista źródeł, lista sond, opis w docs, „co gdzie
persystuje"). Rozjazd jest nieunikniony, dopóki prawda jest w dwóch miejscach.
Cel porządków: **jedno źródło prawdy albo maszynowy strażnik** dla każdego
takiego miejsca.

---

## CZĘŚĆ C — Plan etapowy (A→G), z myślą o edytorze rigu

Kolejność = wartość/ryzyko + zależności. Wczesne etapy przyspieszają późne.

### Etap A — Higiena natychmiastowa (near-zero ryzyko, 1 sesja)
**Cel:** zdusić szum i najświeższy dryf doc↔kod, zanim urośnie.
- A1. README §2: poprawić stałą nazwę suwaka arcade → `[ARCADE]`; dopisać 1
  zdanie o modelu tarcia P4b (zależny od obciążenia) i o deterministycznym
  wczytaniu presetu. (Atakuje B3.)
- A2. `PLAN_STABILNOSC_PROWADZENIE_PL.md` → baner na górze „✅ ZAMKNIĘTE
  (P1–P6, Bramki 1–2, 2026-07-09), historia — aktualny stan: CHECKPOINTS +
  README". Ewentualnie przenieść do `docs/archive/` (decyzja: zostawić w root
  z banerem, bo świeże i cross-linkowane — jak M6-doc). (Atakuje B7.)
- A3. Wyczyścić listę tasków: zamknąć ukończone, zostawić tylko żywe. (B8.)
**Rezultat:** README zgodne z kodem, plan oznaczony jako historia, lista
tasków czytelna. **Kryterium:** grep README za „(arcade)" bez `[ARCADE]` =
pusto; `PLAN_STABILNOSC` ma baner DONE.

### Etap B — Skrypt bramki (przyspiesza KAŻDY późniejszy etap)
**Cel:** jedna komenda = build 3 targety + walidator + test + boot-smoke +
jednoliniowe PASS/FAIL z kluczowymi liczbami sond. Koniec re-derywacji
README §3 + PLAN §9 przez każdego agenta.
- B1. `tools/gate.ps1` (PowerShell, Win): ubija samples.exe, buduje 3 targety
  (filtr `error C|error LNK`), uruchamia walidator (wyłuskuje linie `FAILED`/
  kluczowe liczby), test.exe, smoke `--sample-name "M6 Suspension Rig Lab"
  --frames 300`. Wyjście: `BRAMKA: build 3/3 OK · walidator OK · test PASS ·
  smoke 0 err` albo pierwszy błąd.
- B2. README §3 + PLAN §9 wskazują na skrypt zamiast powielać kroki.
**Rezultat:** `./tools/gate.ps1` = pełna bramka. **Kryterium:** skrypt zwraca
zielone na czystym repo; celowo zepsuty build → czerwone z pierwszym błędem.
**Edytor:** ten sam skrypt zwaliduje edytor, gdy powstanie.

### Etap C — Wspólna lista źródeł w CMake (anty-rozjazd build; prep edytora)
**Cel:** zabić B1 — jedno miejsce na pliki Jozza.
- C1. `set(JOZZ_VEHICLE_SHARED_SOURCES ...)` z plikami dzielonymi przez
  `samples` i `jozz_vehicle_validation`; oba targety używają zmiennej. Pliki
  wyłącznie-GUI (labs, host) zostają przy `samples`.
- C2. Zweryfikować: oba targety budują się identycznie jak przed (diff listy
  obiektów albo po prostu zielona bramka + brak nowych/znikłych symboli).
**Rezultat:** dodanie pliku fizyki = 1 linia, oba targety łapią. **Kryterium:**
bramka zielona; ręczny test — dodaj tymczasowy plik do zmiennej, zbuduj oba,
usuń. **Edytor:** przyszły target edytora dołoży tę samą zmienną — zero
duplikacji od startu.

### Etap D — Rejestr sond walidatora (anty-rozjazd test)
**Cel:** zabić B2 — zapomniana sonda ma być WIDOCZNA.
- D1. Zamienić ręczne `ok &= Run...` na tablicę `{ nazwa, funkcja }`
  iterowaną w `main`; na końcu wydrukować `walidator: uruchomiono N sond`.
  (Opcjonalnie: makro/rejestracja statyczna — ale prosta tablica wystarczy i
  jest czytelna dla słabszego agenta.)
- D2. Zachować IDENTYCZNE sondy i kolejność; porównać wyjście przed/po (ma się
  różnić tylko o linię „uruchomiono N sond").
**Rezultat:** dopisanie sondy = 1 wpis w tablicy; licznik pokazuje, że
biegła. **Kryterium:** N zgadza się z liczbą sond; usunięcie wpisu zmniejsza N.

### Etap E — Mapa persystencji + strażnik inwariantu (powracający bug B6)
**Cel:** zamknąć KLASĘ, nie kolejny pojedynczy przypadek.
- E1. Autorytatywna sekcja „co gdzie żyje i jak persystuje" (rozszerzyć
  `SUBSYSTEM_UI_PRESETS_PL.md §2a`): każde pole → config/session/debug-session/
  nie-persystowane, z regułą która ścieżka load jakiej semantyki używa
  (deterministyczna dla „przywróć zapisane").
- E2. Rozstrzygnąć TECH_DEBT #12: solver kontaktu → sesja (to strojenie
  świata); `m_invertSteering` → debug-session (preferencja). Zaimplementować
  albo świadomie odłożyć z uzasadnieniem.
- E3. Decyzja o wersjonowaniu formatu: albo pole `configVersion`, albo świadomy
  zapis „model = częściowy plik + nakładka na fabrykę = nasza wersja" (obecne
  zachowanie po fixie presetu jest już bezpieczne — udokumentować jako
  kontrakt, nie dokładać wersji bez potrzeby).
**Rezultat:** jedna mapa prawdy o persystencji + strażnik (sonda determinizmu
już jest; dołożyć asercję inwariantu jeśli #12 wprowadzi nowe pola).
**Kryterium:** mapa pokrywa 100% pól configu; #12 rozstrzygnięte (zrobione lub
odłożone-z-powodem). **Edytor:** edytor będzie pisał/czytał kontrakty i
configi — ta mapa to jego fundament.

### Etap F — Rejestr env-hooków (powierzchnia B5)
**Cel:** jeden udokumentowany spis `JOZZ_M6_*`, oddzielić narzędzia od
rusztowania testowego.
- F1. Tabela w kodzie (jedno miejsce czytające env → mapa) LUB co najmniej
  spis w `jozz-vehicle-build-commands`/subsystem-doc z tagiem
  [narzędzie]/[jednorazowe]. Usunąć martwe jednorazowe sondy (np.
  `JOZZ_M6_DIRTY_AT_FRAME`, `TEST_RESET_MODAL` — bugi naprawione).
**Rezultat:** headless-test wie, czym sterować. **Kryterium:** spis = realne
`getenv` w kodzie (grep zgodny). **Edytor:** headless-render edytora użyje
tych samych hooków.

### Etap G — STOP-gate i anty-dryf jako PROCES (nie intencja)
**Cel:** zaszyć w README to, co dziś jest tylko dobrą wolą.
- G1. README §4/§5: jawny, sprawdzalny protokół STOP-gate (kiedy agent MUSI
  stanąć: zmiana kryterium akceptacji, decyzja o modelu/filozofii, dotknięcie
  zaakceptowanego kodu, `src/`). Krótka lista, nie esej.
- G2. Reguła anty-dryf doc↔kod: po zmianie nazwy UI / modelu fizyki / klucza
  configu — grep README+subsystem-doc za starym terminem. Opcjonalnie tani
  `tools/doc_drift_check.ps1` grepujący znane wrażliwe stringi (nazwy suwaków
  vs kod).
**Rezultat:** proces, nie zależny od pamięci agenta. **Kryterium:** README ma
sekcję STOP-gate; (opcj.) skrypt drift-check zielony.

---

## CZĘŚĆ D — Czego NIE ruszamy teraz (świadomie)

- **Podział plików ~2000-liniowych** (validation, rig_lab, visual_mesh,
  suspension_rig). Kod zaakceptowany; refaktor = ryzyko regresji bez zysku
  funkcjonalnego (TECH_DEBT #7). **Ma zaplanowany dom:** Jozz (2026-07-09)
  zapowiedział WIELKI ETAP REFACTORINGU po tych porządkach, a przed edytorem
  — wtedy ciężkie sprawy (podział monolitów, twarde długi) robione z czasem i
  energią, których normalnie brak. Te porządki (zwł. B skrypt bramki, C
  wspólna lista, D rejestr sond) mają ten refaktor **umożliwić i uczynić
  bezpiecznym** (bramka jednym poleceniem = szybka regresja przy krojeniu
  plików). Więc: teraz NIE tniemy — budujemy siatkę bezpieczeństwa pod cięcie.
- **Edytor rigu** — osobny temat (brief §7), NIE teraz. Porządki E/C/F są
  jego przygotowaniem.
- **Fizyka/geometria/filozofia** — zamrożone; porządki są czysto strukturalne.
- Rdzeń box3d `src/`/`include/` — nietykalne.

---

## CZĘŚĆ E — Kolejność, ryzyko, zależności

```
A (higiena)         — near-zero, odblokowuje czytelność
 └ B (skrypt bramki) — near-zero, PRZYSPIESZA wszystkie kolejne
    ├ C (CMake list) — niskie, prep edytora
    ├ D (rejestr sond)— niskie-średnie (struktura walidatora)
    ├ E (persystencja)— niskie-średnie (config plumbing), zamyka klasę B6
    ├ F (env rejestr) — niskie
    └ G (proces)      — near-zero (docs+skrypt)
```
Po A i B reszta jest w dużej mierze niezależna — Jozz może wybrać kolejność
C–G wg priorytetu. Rekomendacja: A→B→C→E→D→F→G (E wysoko, bo to klasa, która
najczęściej boli Jozza w grze).

**Zasada nadrzędna całości:** żaden etap nie dotyka fizyki ani nie „przy
okazji" refaktoryzuje. Każdy = jedna sesja, jedna bramka (skrypt z etapu B),
jeden checkpoint, STOP do akceptacji jeśli etap odsłoni decyzję Jozza.
