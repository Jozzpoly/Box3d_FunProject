# PLAN — WIELKI REFACTOR (przed edytorem rigu)

Zapowiedziany przez Jozza etap „trudnych i ciężkich spraw, na które normalnie
brakuje czasu": podział monolitów i spłata twardych długów strukturalnych.
**Ten dokument to mapa — wykonanie etap po etapie, po akceptacji Jozza.**
Fizyka/feel ZAMROŻONE: każdy etap (poza jawnie oznaczonym R7) jest
**move-only / behavior-preserving**, z poprzeczką wyżej niż „bramka zielona".

---

## 0. Komu ten refactor służy (i czego kto wymaga)

| Interesariusz | Czego potrzebuje | Co z tego wynika dla planu |
|---|---|---|
| **Jozz** | feel auta NIE MOŻE drgnąć; commity audytowalne | poprzeczka = liczby walidatora **IDENTYCZNE co do bajta**, nie „zielone" (R0); etapy małe, osobne commity |
| **Przyszli agenci** (także tańsi) | pliki per-odpowiedzialność zamiast 2000-liniowych; tańsze sesje (mniej kontekstu do wczytania); nawigacja po nazwie pliku | tnij po ODPOWIEDZIALNOŚCI, nie po liczbie linii; nazwy TU = nazwa odpowiedzialności |
| **Przyszły edytor rigu** | czysta matematyka geometrii BEZ świata fizyki; metadane pól configu (nazwy/typy) do generycznego UI/IO | R5 (ekstrakcja geometrii) i R2 (tabela pól) to bezpośredni prep — reszta NIE jest projektowana „pod edytor" na zapas |

**Anty-cel:** spekulacyjne interfejsy „bo edytor może kiedyś...". Wyciągamy
tylko to, co ma wartość TERAZ albo jest pewne.

---

## 1. Zmierzony stan (2026-07-09, nie z opisów)

| Plik | Linie | Zawartość (zmierzone inwentarzem funkcji) |
|---|---|---|
| `jozz_vehicle_validation.cpp` | 2691 | 30 funkcji: 18 sond + ~11 helperów testowych + main z inline kontrakt-checkami |
| `jozz_vehicle_m6_rig_lab.cpp` | ~2000 | **35 metod jednej klasy** (zdefiniowanej w .cpp; header = tylko fabryka): persystencja/presety (8), lifecycle pojazdu (5), aplikatory live-tuningu (4), wizual mocowania (3+Render), zakładki UI (7 — największy kawał), runtime (6) |
| `jozz_vehicle_visual_mesh.cpp` | 1968 | 45 funkcji: parser glTF/skin + rysowanie/placement |
| `jozz_vehicle_m6_suspension_rig.cpp` | 1758 | 25 funkcji: **czysta matematyka geometrii** (hardpointy, rack-stroke, dead-point, toe, droop-lift — bez zależności od świata) + budowa ciał/jointów + drive update + telemetria |
| `jozz_vehicle_m6_config_io.cpp` | ~390 | **71 linii WriteX + 51 ReadX** = każde pole utrzymywane ręcznie w 2 miejscach (plus struct, plus sanitize, plus suwak — do 5 miejsc na pole) |

## 2. Krytyka zanim zaczniemy (słabe punkty tego planu — nazwane z góry)

1. **Największe ryzyko: cicha zmiana zachowania.** Kolejność tworzenia
   ciał/jointów wpływa na iteracyjny solver (wiemy to z pomiarów — bias
   kolejności narożników). „Bramka zielona" NIE wykryje subtelnej zmiany.
   → Odpowiedź: R0 podnosi poprzeczkę do **diff-u liczb walidatora co do
   bajta** + diff zrzutów quad_shot dla etapów wizualnych.
2. **Scope creep „przy okazji".** Move-only znaczy move-only: żadnych
   ulepszeń, rename'ów, „lepszych" nazw w etapach przenoszących kod. Ulepszenia
   strukturalne (R2 tabela pól) są OSOBNYMI etapami, jawnie oznaczonymi.
3. **Eksplozja nagłówków / cykle include.** Klasa labu przejdzie do
   nagłówka WEWNĘTRZNEGO (`*_internal.h`), nie do publicznego API. Jeśli
   podział wymusza zmianę zachowania (np. inicjalizacji) → STOP.
4. **Ryzyko przeprojektowania pod edytor.** Patrz anty-cel w §0.
5. **Zmęczenie/utrata uwagi w długiej serii przenosin** — dokładnie na to
   choruje „wielki refactor". → Etapy = 1 sesja, po każdym pełny baseline-diff
   i commit; żadnych „dokończę dwa etapy w jednej sesji".

## 3. Etapy

### R0 — Poprzeczka: baseline-diff (OBOWIĄZKOWY PIERWSZY) — ✅ WYKONANE 2026-07-10
**Cel:** narzędziowo wymusić „identyczne", nie „zielone".
> **Status:** zrobione w `tools/gate.ps1` (`-SaveBaseline`/`-DiffBaseline`/`-Shots`).
> Zweryfikowane obustronnie: niezmienione repo = zielone; subtelna zmiana defaultu
> (6.0→6.1 Hz, przy której CAŁA bramka jest zielona) = czerwone na diffie (linia 87
> `settle sag 0.046→0.044 m`). Determinizm potwierdzony pomiarem: walidator 349
> linii identyczne w 3 uruchomieniach; render quad ten sam hash PNG.
- `tools/gate.ps1 -SaveBaseline`: zapisuje stdout walidatora (z odfiltrowanymi
  liniami czasowymi, np. „Test duration") do `build/gate_baseline.txt` +
  komplet quad_shot PNG do `build/gate_baseline_shots/`.
- `tools/gate.ps1 -DiffBaseline`: pełna bramka + diff stdout vs baseline
  (byte-identical po filtrze) → różnica = FAIL z pierwszą różniącą się linią.
  Zrzuty: porównanie rozmiarów/hashy + instrukcja obejrzenia obu przy różnicy.
- Kryterium: `-SaveBaseline` → `-DiffBaseline` na NIEZMIENIONYM repo = zielone;
  celowa zmiana liczby (np. tymczasowo hertz w defaultach) = czerwone.
**Każdy etap R1–R6 kończy się `-DiffBaseline` = zero różnic.**

### R1 — validation.cpp: podział na harness + sondy (trening, zero ryzyka runtime)
> **Status:** ✅ WYKONANE 2026-07-10. `-DiffBaseline` na 7-plikowym podziale =
> 349 linii walidatora IDENTYCZNE co do bajta z baseline sprzed R1 (w tym
> dokładna linia „jozz_vehicle_validation: ran 18 probes"), build 3/3 OK,
> test PASS, smoke 0 err. Cięcie zrobione skryptem (dokładne zakresy linii
> 1-indeksowane, wycięte z oryginału i wklejone bez retypowania — zero ryzyka
> literówki w 2710-liniowym pliku), nie ręcznym kopiowaniem.
>
> Odstępstwo od listy niżej, udokumentowane świadomie: „wspólne stałe kroku"
> (timeStep=1/60, subStepCount=4) NIE zostały wydzielone do stałej w
> helpers.h — są identyczne w ~18 miejscach, ale to czysto kosmetyczna
> deduplikacja bez wpływu na `-DiffBaseline`; podmiana ~36 wystąpień w pliku
> „zero ryzyka runtime" dodawałaby powierzchnię edycji bez żadnej korzyści
> funkcjonalnej. Zostawione jak było (dosłowny move).
2691 linii → ~6 plików; kod NIE-shippingowy (najbezpieczniejszy start):
- `validation/jozz_validation_helpers.{h,cpp}` — CheckTrue/CheckApprox,
  CreateM6SmokeGround, IsM6VehicleStateValid, wspólne stałe kroku.
- `validation/jozz_probes_m5_m6.cpp` (smoki M5/M6 + envelope),
  `validation/jozz_probes_m7.cpp` (landing/hands-off/torque/trailing),
  `validation/jozz_probes_steering.cpp` (P1/P2/P4/P4b/pull/P5),
  `validation/jozz_probes_config.cpp` (P3/P6/sanitize/preset-determinism/stress).
- `jozz_vehicle_validation.cpp` → slim main: kontrakt-checki + rejestr sond
  (z Porządków D) + wywołanie.
- CMake: pliki dopisane do targetu walidatora (osobna zmienna
  `JOZZ_VALIDATION_FILES` obok CORE).
Kryterium: `-DiffBaseline` zero różnic (w tym „ran 18 probes").

### R2 — config_io: tabela pól zamiast 71+51 ręcznych linii (STRUKTURA, nie move-only)
> **Status:** ✅ WYKONANE 2026-07-10. 71 pól zmierzone dokładnie (51 root +
> 13 wishbone + 4 trailingArm + 3 wheelEnvelope) — zgadza się z liczbą z tego
> akapitu. Dwie krytyczne korekty względem litery planu:
> 1. **„jedna tabela" vs „tablice per typ" — sprzeczność.** Kolejność kluczy
>    w JSON-ie miesza typy (vec3, float×5, int×2, potem zagnieżdżony obiekt...);
>    osobne tablice per typ zmieniłyby kolejność wyjścia = złamałoby „tekstowo
>    identyczne". Rozwiązanie: JEDNA uporządkowana tablica z tagiem typu
>    (anonimowa unia wskaźników-do-składowej + enum), bez makr — `template
>    <typename Owner> struct JozzFieldDesc`, osobna instancja per właściciel
>    (Config/Wishbone/TrailingArm/WheelEnvelope), bo wskaźnik-do-składowej
>    niesie typ właściciela. Zagnieżdżone obiekty NIE są wierszem tabeli
>    (nie da się zmieszać `JozzFieldDesc<Config>` z `JozzFieldDesc<Wishbone>`
>    w jednym typie tablicy bez type erasure) — root podzielony na 3 segmenty
>    (przed wishbone / między trailingArm a wheelEnvelope / po wheelEnvelope),
>    każdy zagnieżdżony obiekt to 2 linie kleju (otwórz/zamknij) + własna mała
>    tabela — dokładnie „małe pod-tabele" z akapitu niżej.
> 2. **Tabela ≠ odbicie struktury.** `rackTravel` (POCHODNE — przeliczane po
>    wczytaniu, tripwire w kodzie + sonda p2) i `filterGroupIndex` (runtime)
>    świadomie NIE są w tabeli, dokładnie jak nie były w starym ręcznym
>    kodzie. Tabela zbudowana ręcznie z listy write/read, nie odbiciem structu.
>
> **Weryfikacja silniejsza niż litera kryterium:** zamiast/obok diffu
> tekstowego configu domyślnego (który ma ślepy punkt — dwa pola z tym samym
> defaultem maskują zamienione wskaźniki), dodałem TYMCZASOWĄ sondę z
> unikalną wartością na KAŻDYM z 71 pól (save→load→porównaj), niezależnie
> wypisaną z definicji structu (nie z tabeli pod testem — inaczej błąd w
> tabeli byłby niewidoczny sam dla siebie). Uruchomiona: wszystkie 71 pól
> wróciły dokładnie, exit 0, 0 linii „bad". Usunięta przed commitem (patrz
> niżej — dodanie jej NA STAŁE złamałoby `-DiffBaseline`).
> `-DiffBaseline`: 349 linii walidatora identyczne (w tym „ran 18 probes") —
> bo litera R2 tego etapu wymaga zero różnic w SAMYM walidatorze, nowe
> pokrycie testowe to osobna decyzja (nie dodane na stałe w tej sesji).
**Jedyny etap „nowego kodu" w serii (poza R0):** jedna tabela
`{klucz, wskaźnik-na-pole, typ}` (zwykłe tablice per typ, wskaźniki na
składowe — BEZ makr X, czytelne dla słabszego agenta), z której generuje się
i writer, i reader. Zagnieżdżone struktury (wishbone/trailing/envelope) = małe
pod-tabele. Legacy-klucze i specjalne migracje zostają ręczne (są wyjątkiem,
nie regułą).
- Wartość: dodanie pola configu = 1 wpis zamiast 2–3; **metadane pól = prep
  pod edytor** (generyczne UI/IO po tej samej tabeli).
- Kryterium twarde: **pliki sesji/presetów zapisane przed i po są
  tekstowo IDENTYCZNE** (zapisz sesję starym kodem → nowym → diff = 0);
  `-DiffBaseline` zero różnic; sonda determinizmu presetu zielona.
- Sanitize NIE wchodzi do tabeli w tym etapie (clampy zostają jak są) —
  odnotowane jako możliwe rozszerzenie później.

### R3 — rig_lab: klasa do nagłówka wewnętrznego + podział na TU (move-only)
> **Status:** ✅ WYKONANE 2026-07-11. `rig_lab.cpp` (2003 l.) → nagłówek
> wewnętrzny `jozz_vehicle_m6_rig_lab_internal.h` (klasa: 42 metody + składowe)
> + 4 TU: główny `.cpp` (625 l.: ctor/dtor, lifecycle, aplikatory, Step, Render,
> Keyboard, env-registry, fabryka), `_ui_tabs.cpp` (751 l.: 6 zakładek +
> DrawControls + HelpMarker/SectionHeader), `_persistence.cpp` (240 l.: sesja/
> debug-sesja/presety/Sync/Apply/Recompute), `_mount_visual.cpp` (265 l.:
> Load/SetupMountRig + CornerIsLeft/ArmEnds + DrawRigDiagnostics/DumpCornerGeometry).
> Cięcie skryptem z asercjami granic (jak R1); transformowana tylko linia
> sygnatury każdej metody na definicję poza-klasową, ciała bajt-w-bajt.
>
> **Dwie korekty względem litery planu (krytyczne):**
> 1. **„35 metod" było zaniżone — jest 42.** Inwentarz w §1 przeoczył 4 STATYCZNE
>    metody składowe (`CornerIsLeft`, `ArmEnds`, `HelpMarker`, `SectionHeader` —
>    wzorzec nie łapał `static` przed typem). Wykryte przez KOMPILATOR (main.cpp
>    nie widział `CornerIsLeft`/`ArmEnds` z `Render`, gdy skrypt wrzucił je jako
>    funkcje plikowe do innego TU) — dodane jako statyczne składowe deklarowane w
>    nagłówku, definiowane w swoim TU. Re-inwentarz przez detekcję klamer Allman
>    (linia przed każdym `^\t{`) potwierdził dokładnie 42.
> 2. **NIE wydzieliłem wewnętrznych bloków `Render()` do nowych metod-helperów**
>    (plan to SUGEROWAŁ: „kawał Rendera... wydzielony jako metody pomocnicze").
>    To jedyna nietrywialna transformacja z ryzykiem zmiany zachowania (nowe
>    granice metod). Bloki są samodzielne (własne `chassisLive`, tylko składowe),
>    więc byłoby to bezpieczne — ale §2.2 mówi „żadnych ulepszeń w etapach
>    przenoszących". `Render()` został w main VERBATIM; przeniesione tylko całe,
>    już-osobne metody (DrawRigDiagnostics/DumpCornerGeometry → mount_visual).
>
> **Weryfikacja (silniejsza niż litera):** build 3/3 czysty (po naprawie 42-metod);
> `-DiffBaseline` walidator 349 linii IDENTYCZNE; quad render IDENTYCZNY (hash);
> ORAZ 6 zrzutów zakładek (JOZZ_M6_TAB 0-5, 150 klatek, neutralne env) PIKSEL-w-
> piksel identycznych przed/po (MD5) + obejrzany render (poprawny lab). Stałe
> `kSessionFilePath`/`kPresetDirectory`/`kDebugSessionFilePath` → nagłówek
> wewnętrzny verbatim (każda używana w 1 TU; `static` = kopia per-TU, bez ODR).
Mechanizm C++: definicje metod klasy MOGĄ żyć w osobnych TU — klasa idzie do
`jozz_vehicle_m6_rig_lab_internal.h` (prywatny nagłówek, nie API), ciała metod
rozjeżdżają się po odpowiedzialnościach:
- `_ui_tabs.cpp` — Draw*Tab + DrawControls (największy kawał),
- `_persistence.cpp` — sesja/debug-session/presety/Sync/ApplyPending/Recompute,
- `_mount_visual.cpp` — LoadWheelVisual/LoadMountVisual/SetupMountRig + kawał
  Rendera rysujący mocowanie/dumpery (wydzielony jako metody pomocnicze
  TYLKO przez przeniesienie istniejących bloków — bez zmiany treści),
- główny plik: konstruktor/destruktor, Step, Render (szkielet), lifecycle,
  aplikatory, env-registry.
Kolejność inicjalizacji, kolejność rysowania, kolejność tworzenia — BEZ ZMIAN.
Kryterium: `-DiffBaseline` + quad_shot diff + zrzut każdej zakładki
(JOZZ_M6_TAB 0–5) wizualnie identyczny z baseline.

### R4 — visual_mesh: loader vs rysowanie (move-only)
- `jozz_vehicle_visual_mesh_loader.cpp` (glTF/skin/kości/kontrakty punktów),
- `jozz_vehicle_visual_mesh_draw.cpp` (Draw*, placement, damper teleskopowy).
Nagłówek publiczny bez zmian. Kryterium jak R3 (quad_shot obowiązkowy).

### R5 — suspension_rig: ekstrakcja czystej geometrii (NAJOSTROŻNIEJSZY)
Czysta matematyka (MakeWishboneHardpoints, ComputeRackStroke,
ComputeSteeringDeadPointDeg, SteeringArmWithToe, HingeSwingLimit,
SteeringLinkDroopLift, DefaultConfig/DefaultTrailingArmGeometry, Sanitize) →
`jozz_vehicle_m6_geometry.{h,cpp}` — zero zależności od b3World. Budowa
ciał/jointów, drive update i telemetria ZOSTAJĄ w suspension_rig nietknięte
(zmienia się tylko #include). **To jest serce prep-u pod edytor** (edytor
liczy hardpointy/martwy punkt bez tworzenia świata).
Kryterium: `-DiffBaseline` zero różnic; przegląd diffa = wyłącznie przenosiny.

### R6 (OPCJONALNY — decyzja Jozza) — struktura katalogów
40+ plików `jozz_vehicle_*` leży płasko w `samples/`. Opcja: podkatalogi
`samples/jozz/{core,labs,visual,validation}/`. Zysk: nawigacja; koszt: churn
rename'ów w historii + poprawa include'ów (CMake to 1 miejsce dzięki
Porządkom C). Wykonalne bezpiecznie (git śledzi rename), ale to głównie
estetyka — **Jozz decyduje, czy warto**.

### R7 (OPCJONALNY — decyzja Jozza; JEDYNY zmieniający zachowanie) — #12 solver kontaktu
Aplikacja wartości solvera przy starcie + persystencja w sesji (dziś świat
startuje na defaultach silnika, a suwaki 30/10/3 aplikują się dopiero przy
dotknięciu). Mała, ale REALNA zmiana zachowania startu → wymaga Twojego
świadomego OK i osobnego testu ręcznego.

## 4. Kolejność i zależności

```
R0 (poprzeczka)  → obowiązkowy fundament wszystkiego
 └ R1 (validation) — trening na kodzie nie-shippingowym
    └ R2 (config tabela) — struktura + editor-prep
       └ R3 (rig_lab) → R4 (visual_mesh) → R5 (geometria)  — rosnąca ostrożność
          └ R6, R7 — opcjonalne, każdy za osobną zgodą
```
Zasady serii: 1 etap = 1 sesja = 1 commit; `-DiffBaseline` po każdym; STOP gdy
diff pokaże JAKĄKOLWIEK różnicę liczb (nie „wyjaśniam w locie") albo gdy
podział wymusza zmianę inicjalizacji/kolejności; żadnych rename'ów publicznych
nazw; README §2/§9 + CHECKPOINTS aktualizowane w tym samym commicie
(doc_drift_check rozszerzany, gdy przenosiny zmieniają ścieżki cytowane w docs).

## 5. Czego ten refactor świadomie NIE robi

Zmian fizyki/feelu/defaultów (poza R7 za zgodą) · edytora rigu · integracji
M9 z autem · modelu opony · zmian w `src/`/`include/` · rename'ów kluczy
configu/env (to osobna klasa zmian z migracjami) · „ulepszeń" treści
przenoszonych funkcji.
