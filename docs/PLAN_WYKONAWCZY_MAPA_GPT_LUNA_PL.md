# Plan wykonawczy mapy dla GPT Luna

Wersja: 2026-07-13.
Właściciel produktu i odbiorów wizualnych/jezdnych: Jozz.
Źródło statusu: `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.
Diagnoza wejściowa: `AUDYT_REALIZACJI_MAPY_2026_07_13_PL.md`.

## 0. Przeznaczenie tego dokumentu

To nie jest lista pomysłów. To instrukcja wykonywania roadmapy przez GPT Luna
na niskim, średnim albo wysokim poziomie mocy. Jej zadaniem jest ograniczyć
swobodną interpretację, wymusić małe odwracalne kroki i zatrzymać model przed
powtórzeniem obecnego błędu: wejściem w kolejny etap bez odbioru człowieka.

Najważniejsze reguły:

1. dokładnie jeden work-package (WP) na jedną turę implementacyjną;
2. dokładnie jeden cel produktu na WP;
3. Luna nie uruchamia kolejnego WP automatycznie;
4. `WAITING_FOR_JOZZ` jest stanem kończącym turę, nie przeszkodą do obejścia;
5. zielony build nie oznacza akceptacji mapy;
6. nowe progi i odstępstwa zatwierdza Jozz przed implementacją;
7. model nie stage'uje całego dirty tree i nie „porządkuje” cudzych zmian;
8. każdy WP ma własny rollback;
9. jeśli polecenie i repo są sprzeczne, Luna zatrzymuje się i raportuje dowód;
10. dekoracje nigdy nie wyprzedzają geometrii, przejazdu i feel.

## 1. Routing mocy obliczeniowej

### LOW

Dozwolone:

- inventory, grep, status, manifest dowodów;
- aktualizacja statusu dokumentu z gotowego werdyktu;
- uruchomienie literalnych komend i zebranie outputu;
- stałe screenshoty przy podanych kamerach;
- mechaniczna zmiana tabel danych z już zaakceptowanymi wartościami;
- prosty toggle według istniejącego wzorca, jeśli WP wskazuje dokładne pliki.

Zakazane:

- projekt topologii;
- geometria ramp, dróg i miterów;
- dobór progów jakości;
- refaktor architektury;
- decyzja estetyczna lub akceptacja produktu;
- naprawa „przy okazji”.

LOW zatrzymuje się po pierwszej nietrywialnej porażce albo gdy rozwiązanie nie
wynika jednoznacznie z WP.

### MEDIUM

Dozwolone:

- jeden mały data-driven slice;
- do 3–4 plików źródłowych plus test/doc/checkpoint;
- użycie istniejącego, już zwalidowanego helpera;
- lokalna naprawa toggle, manifestu, station spec lub pojedynczej receptury;
- analiza wyników dwóch wariantów, jeśli próg ustalono przed uruchomieniem.

Zakazane:

- zmiana topologii całej mapy;
- nowy algorytm geometrii drogi;
- jednoczesna implementacja kilku stacji;
- rozszerzenie allowed files bez zatrzymania;
- obniżenie progu po niepowodzeniu.

MEDIUM zatrzymuje się po dwóch różnych nieudanych hipotezach. Trzecia próba
wymaga HIGH albo decyzji człowieka.

### HIGH

Wymagane dla:

- topologii i curvature;
- top-surface ramp i geometrii convex hull;
- walidatora realnych shape'ów;
- collision-category/envelope M6;
- coplanarity i contact diagnostics;
- refaktoru źródła prawdy builder/visual;
- bezpiecznego izolowania zmieszanego WIP;
- trudnego crasha albo regresji solver/contact.

HIGH nadal nie podejmuje decyzji produktowej. Po maksymalnie trzech hipotezach
bez rozstrzygającego dowodu zatrzymuje się z macierzą wyników.

### HUMAN / JOZZ

Wyłącznie Jozz decyduje o:

- zachowaniu/publikacji obecnego mieszanego WIP;
- wyborze layoutu/skeletonu;
- fokusie, czytelności, skali i feel;
- akceptacji E2R, E3 skeletonu, pełnej pętli i finalnej mapy;
- rozszerzeniu scope'u;
- zmianie progów po obejrzeniu nieudanego wyniku.

Model może rekomendować `ACCEPT / ADAPT / REJECT`, lecz nie może podpisać się
za Jozza.

## 2. Maszyna stanów i identyfikatory

`RECOVERY_REQUIRED` i `REJECTED_EXPERIMENT` są dyspozycjami całego strumienia
z master planu. Nie są stanami wykonawczymi. Każdy WP ma dokładnie jeden z
poniższych stanów:

| Stan | Znaczenie | Dozwolone przejście |
|---|---|---|
| `LOCKED` | niespełnione preconditions | tylko do `READY` po dowodzie |
| `READY` | można rozpocząć | do `IN_PROGRESS` |
| `IN_PROGRESS` | jeden aktywny WP | do `WAITING_FOR_JOZZ`, `ACCEPTED` technicznego WP lub `REJECTED` |
| `WAITING_FOR_JOZZ` | wymagany odbiór człowieka | tylko Jozz wybiera dalej |
| `ACCEPTED` | dowody i wymagany podpis istnieją | odblokowuje wskazany następnik |
| `REJECTED` | wynik nie jest bazą | rollback/quarantine, nie dalszy polish |
| `SUPERSEDED` | zastąpione i historyczne | brak dalszej pracy |

Nie wolno używać niejednoznacznego `DONE`, `mostly done`, `green` albo `in
progress` bez identyfikatora WP i następnego legalnego przejścia.

Master używa `R0…R7` wyłącznie jako faz programu. Wykonawca używa wyłącznie
`WP-*`. Mapowanie bieżącej ścieżki:

| Faza mastera | Wykonywalne WP |
|---|---|
| R0 | WP-00, WP-01 |
| R1 | WP-02, WP-03, WP-GATE-A, WP-GATE-B |
| R2 | WP-C0…WP-C6 oraz wybrany branch C5 |
| R3 | WP-T0…WP-T6 |
| R4 | WP-D*, WP-L* |
| R5 | WP-PHYS-* |
| R6 | WP-SPAWN-* |
| R7 | WP-NAV-*, WP-TELEM-*, WP-FINAL |

Nazwy `E2R-C*` i `E3-N*` w dokumentach etapowych opisują cele produktu; w
zleceniu i checkpointcie zawsze podaje się odpowiadający im `WP-*`.

## 3. Protokół początku każdej tury

### 3.1 CWD i inventory

Luna zaczyna literalnie:

```powershell
Set-Location 'C:\Pliki_Joza\Gamo_devovo\Box3d_FunProject\box3d'
git status --short --branch
git diff --name-only
git diff --stat
git diff --cached --name-only
git diff --cached --check
git ls-files --others --exclude-standard
```

Następnie zapisuje w komentarzu:

- bieżący branch i HEAD;
- listę plików już zmienionych przez użytkownika;
- ID wykonywanego WP;
- poziom mocy;
- allowed files;
- czy preconditions są spełnione.

Jeżeli status różni się od stanu opisanego w ostatnim checkpointcie, Luna nie
zgaduje. Wykonuje read-only diff i zatrzymuje implementację z raportem driftu.

### 3.2 Kolejność czytania

Zawsze:

1. `README_FOR_AGENTS.md`;
2. `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md` §0 i bieżący etap;
3. ten dokument: reguły ogólne + bieżący WP;
4. ostatnie trzy wpisy `CHECKPOINTS_PL.md`;
5. allowed source files WP;
6. test/validator dotyczący zmiany.

Nie czytać wszystkich historycznych planów jako równorzędnych instrukcji.

### 3.3 Preflight

Przed edycją:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\doc_drift_check.ps1
git diff --check
```

Jeżeli baseline ma błąd:

- zapisać dokładny output;
- sprawdzić, czy błąd dotyczy allowed files;
- jeśli nie dotyczy — nie naprawiać go przy okazji;
- jeśli uniemożliwia WP — WP wraca do `LOCKED` z powodem `baseline failure`,
  raport do Jozza.

## 4. Protokół końca każdej tury

Minimalny gate:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\gate.ps1 -Numbers
.\build\bin\Debug\samples.exe --sample-name "M5 First Drivable" --frames 300
.\build\bin\Debug\samples.exe --sample-name "M6 Suspension Rig Lab" --frames 300
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\doc_drift_check.ps1
git diff --check
git status --short --branch
```

Uwaga: w stanie z dnia 2026-07-13 `gate.ps1` smoke-testuje tylko M6. Do
ukończenia WP-GATE-A osobny M5 powyżej jest obowiązkowy. `PASS` wymaga także,
aby obiekty/binarki objęte WP były nowsze od zmienionych źródeł; jeżeli build
inkrementalny tego nie zapewnia, wykonać zatwierdzony clean rebuild.

Końcowy raport WP zawiera:

```text
WP: <ID> / <stan>
HEAD_START: <hash>
HEAD_END: <hash albo UNCOMMITTED>
FILES_CHANGED: <lista>
TECH_GATE: PASS/FAIL + najważniejsze liczby
VISUAL_GATE: PASS/FAIL/NOT_APPLICABLE
DRIVE_GATE: PASS/FAIL/NOT_RUN
JOZZ_GATE: WAITING/ACCEPTED/REJECTED/NOT_REQUIRED
KNOWN_LIMITS: <lista>
ROLLBACK: <dokładna instrukcja>
ONLY_LEGAL_NEXT_WP: <ID albo NONE>
```

Lokalny commit `CANDIDATE` jest dozwolony po technicznym gate i służy jako hash
do ręcznego odbioru. Push/publikacja jest dozwolona dopiero, gdy:

- Jozz zaakceptował WP, jeśli wymaga bramki ludzkiej;
- diff zawiera wyłącznie allowed files i świadome docs/testy;
- pełny gate jest zielony;
- nie stage'owano obcego dirty WIP;
- standing workflow repo i Jozz nie blokują publikacji.

Po `REJECT` nie poprawiać tego samego commita; wykonać rollback/quarantine
opisany w WP. Wyjątek: przed zakończeniem WP-00 nie wolno tworzyć nawet commita
`CANDIDATE` z bieżącego mieszanego worktree.

## 5. Manifest dowodów

Każdy WP wizualny zapisuje obok checkpointu:

```text
evidence_id
wp_id
git_hash_or_worktree_fingerprint
sample_name
command
seed
teleport
camera
frames
png_path
expected_observation
actual_observation
known_limitation
```

Stałe nazwy kadrów:

- `map_<wp>_plate_top.png`;
- `map_<wp>_center_top.png`;
- `map_<wp>_center_3q.png`;
- `map_<wp>_core_driver.png`;
- `map_<wp>_<station>_driver.png`;
- `map_<wp>_<station>_profile.png`.

Każda kamera jest zapisana liczbowo. „Podobny kadr” nie jest porównaniem.

Przykładowa forma uruchomienia — wartości kamery bierze się z zaakceptowanego
manifestu, nie dobiera na nowo:

```powershell
$env:JOZZ_M6_CAM = '<yaw,pitch,distance,targetX,targetY,targetZ>'
$env:JOZZ_M6_TELEPORT_XZ = '<x,z>'
.\build\bin\Debug\samples.exe --sample-name "M6 Suspension Rig Lab" --frames 120 --screenshot 'build\map_<wp>_center_top.png'
Remove-Item Env:JOZZ_M6_CAM -ErrorAction SilentlyContinue
Remove-Item Env:JOZZ_M6_TELEPORT_XZ -ErrorAction SilentlyContinue
```

WP-01 ustala literalne kamery oraz `frames=120` dla statycznego kadru; próby
jezdne używają wartości zapisanej w swoim WP (domyślnie co najmniej 300).
Jeżeli rzeczywisty format `JOZZ_M6_CAM` różni się od przykładu, Luna najpierw
odczytuje parser w kodzie i dokumentuje literalny format. Nie metodą prób i
błędów zmieniając przy tym kod.

## 6. Warunki globalnego STOP

Natychmiast zatrzymaj WP, gdy:

- trzeba dotknąć pliku spoza allowed files;
- plan i kod mają sprzeczny status;
- nie ma wymaganego hasha/sign-offu;
- rozwiązanie wymaga zmiany `src/` albo `include/` Box3D;
- pojawia się potrzeba poluzowania tolerancji;
- wynik wygląda źle mimo zielonego validatora;
- test przechodzi tylko po wyłączeniu wcześniejszego wymogu;
- body/shape count rośnie bez wyjaśnienia;
- zmienia się więcej niż jeden produkt/strefa;
- trzeba usunąć cudze zmiany lub zrobić destructive git;
- pojawia się crash, NaN, assertion, nowe ostrzeżenie sokol/Box3D;
- trzy hipotezy HIGH albo dwie MEDIUM nie rozwiązują problemu;
- człowiek odrzucił skeleton/slice.

Po STOP: zero kolejnych edycji. Raport: dowód, ostatni zielony stan, możliwe
opcje i rekomendacja.

## 7. Recovery — aktualnie jedyna otwarta sekwencja

### WP-00 — decyzja o zachowaniu mieszanego WIP

> **ROZSTRZYGNIĘTE 2026-07-24 (decyzja Jozza).** WP-00 zamknięty. Procedura A wykonana:
> snapshot mieszanego WIP = `56c04c1` na `origin/jozz-map-wip-snapshot-2026-07-24`. Następnie
> odzyskany i sklasyfikowany stan (E2R central campus BEZ toru E3 + poprawki regresji skanu)
> scommitowano na `main` na jawne polecenie Jozza „zapisujemy aktualny stan na maina" przed
> skokiem w bok. Blokada „nie stage'uj / nie commituj" poniżej dotyczyła FAZY KWARANTANNY i już
> nie obowiązuje. Szczegóły: `ODZYSK_UTRACONYCH_ZMIAN_2026_07_24_PL.md`.

Moc: **HUMAN/JOZZ + HIGH read-only preparation**.
Stan: **READY**.
Cel: zachować E2R/E3 bez przypadkowego uznania ich za baseline.

Preconditions:

- HEAD nadal odpowiada znanemu baseline'owi albo drift został opisany;
- brak destructive operacji.

Allowed files:

- brak plików źródłowych;
- opcjonalnie manifest/checkpoint po decyzji Jozza.

Kroki:

1. inventory i pełna klasyfikacja plików;
2. wydruk `git diff --stat` i lista untracked;
3. klasyfikacja `E2R_KEEP / E2R_FIX / E3_QUARANTINE / DOCS / UNKNOWN`;
4. policzyć osobno tracked, staged i untracked;
5. przedstawić Jozzowi dokładnie dwie bezpieczne procedury:
   - **A — rekomendowana:** osobna gałąź WIP + jawna lista `git add -- <paths>`
     obejmująca wszystkie sklasyfikowane pliki + pojedynczy commit oznaczony
     `WIP snapshot, not accepted`; następnie sprawdzenie zawartości commita;
   - **B — archiwum:** patch binarny dla tracked/staged + archiwum wszystkich
     untracked, lista ścieżek i SHA-256 archiwum w katalogu wskazanym przez
     Jozza poza aktywnym drzewem;
6. Jozz wybiera A albo B i zatwierdza literalne polecenia;
7. wykonać wyłącznie zatwierdzoną procedurę i zweryfikować odtwarzalność.

STOP:

- Luna nie może sama commitować/pushować obecnego mieszanego WIP;
- nie używać `git add -A`;
- nie usuwać untracked;
- sama nowa gałąź, sam `git diff` albo sam stash bez potwierdzenia untracked nie
  są snapshotem;
- nie przechodzić do WP-01 bez odtwarzalnego identyfikatora snapshotu.

Acceptance:

- istnieje jawny identyfikator zachowanego WIP;
- dokument mówi, że nie jest on zaakceptowanym baseline'em.

Rollback: nie dotyczy; WP jest zachowawczy.
Jedyny następny WP: `WP-01`.

### WP-01 — inventory i evidence baseline

Moc: **LOW**.
Stan początkowy: **LOCKED** — odblokowuje wyłącznie zaakceptowany WP-00.
Cel: odtworzyć bieżący stan bez edycji geometrii.

Allowed files:

- `docs/MAPA_EVIDENCE_MANIFEST_PL.md` (utworzenie lub dopisanie sekcji WP-01);
- `CHECKPOINTS_PL.md` maksymalnie 5 linii.

Forbidden:

- wszystkie `.cpp/.h`;
- zmiana statusu na accepted.

Kroki:

1. zapisać HEAD, snapshot WIP, status i stat;
2. uruchomić pełny gate oraz osobny M5;
3. zapisać body/shape count, perf i output sond;
4. wykonać stałe kadry obecnego C i toru;
5. oznaczyć ograniczenia: brak interactive feel, validator nie buduje świata;
6. porównać listę dowodów z audytem.

Acceptance: manifest jest kompletny i odtwarzalny; nie ocenia jakości za Jozza.
Failure: brak kamery/formatu → STOP i odczyt parsera, bez zmiany kodu.
Rollback: revert tylko manifest/checkpoint.
Następny: `WP-02`.

### WP-02 — odłączenie odrzuconego E3 od aktywnego świata

Moc: **HIGH**.
Stan początkowy: **LOCKED** — wymaga WP-01 `ACCEPTED`.
Cel: domyślny M5/M6 buduje E1+E2R, nie fizyczny E3.

Preconditions:

- WP-00 i WP-01 accepted;
- eksperyment E3 zachowany;
- znany ostatni zielony gate.

Allowed files:

- `samples/jozz_vehicle_m5_test_course.cpp`;
- `samples/jozz_vehicle_m5_test_course.h`;
- `samples/validation/jozz_probes_map.cpp`;
- `samples/CMakeLists.txt`, wyłącznie jeśli probe wymaga istniejącego źródła;
- `docs/CHECKPOINTS_PL.md` i status mapy w masterze.

Forbidden:

- edycja track layout/builder/visual;
- poprawa geometrii E2R;
- kasowanie plików E3;
- zmiana E1.

Kroki:

1. zidentyfikować jedyne wywołania budowy E3;
2. dodać jawny, domyślnie wyłączony state/flag o nazwie wskazującej
   `RejectedExperiment`, nie `temporary fix`;
3. potwierdzić, że M5 i M6 korzystają z tego samego course'u;
4. dodać probe: default world nie zawiera track bodies/profiles;
5. zachować opcję diagnostycznego uruchomienia eksperymentu tylko jeśli nie
   zwiększa scope'u; inaczej kod pozostaje po prostu niepodłączony;
6. pełny gate, M5, M6 i stałe kadry.

Acceptance:

- default body/shape manifest nie zawiera E3;
- geometria mapy E1 i E2R jest identyczna między M5/M6; całkowity body/shape
  count pojazdów może się różnić;
- pliki E3 nadal istnieją;
- brak zmiany ich geometrii.

Typowe awarie:

- linker usuwa symbole E3: to nie błąd, jeśli kod nadal się kompiluje w target;
- CMake przestaje kompilować E3: nie usuwaj źródeł z targetu w tym WP;
- screenshot nadal pokazuje linie: prawdopodobnie overlay, obsłuży WP-03.

Rollback: odwrócić wyłącznie switch/callsite i test.
Następny: `WP-03`.

### WP-03 — debug overlay toggle

Moc: **MEDIUM**.
Stan początkowy: **LOCKED** — wymaga WP-02 `ACCEPTED`.
Cel: product view jest czysty; diagnostyka nadal dostępna.

Allowed files:

- `samples/jozz_vehicle_m5_drivable_lab.cpp`;
- `samples/jozz_vehicle_m6_rig_lab.cpp`;
- `docs/CHECKPOINTS_PL.md`.

Kroki:

1. zinwentaryzować wszystkie wywołania campus/yard/track skeleton;
2. dodać ten sam jawny setting `JOZZ_MAP_OVERLAY` w obu labach; jeśli
   wymagałby nowego shared modułu, STOP i nowy HIGH WP z dokładną listą plików;
3. default `false`;
4. overlay on nie zmienia bodies/shapes/contactów;
5. wykonać pary tych samych kadrów OFF/ON;
6. dodać distance culling napisów, jeśli istniejący helper już go zapewnia;
   w przeciwnym razie osobny WP.

Acceptance:

- OFF pokazuje mapę, nie blueprint;
- ON pokazuje wszystkie potrzebne granice;
- body/shape count identyczny;
- stan toggle jest identyczny w M5/M6.

Failure: input raw nie działa w automatyzacji → testować wartość domyślną i
jawny env/config path; nie twierdzić, że klawisz sprawdzono interaktywnie.
Rollback: jeden switch i callsites.
Następny: `WP-GATE-A`.

### WP-GATE-A — wiarygodny gate M5+M6 i licznik sond

Moc: **MEDIUM**.
Stan początkowy: **LOCKED** — wymaga WP-03 `ACCEPTED`.
Cel: gate naprawdę przebudowuje zmienione źródła, smoke-testuje oba laby i
raportuje prawdziwą liczbę sond.

Allowed files:

- `tools/gate.ps1`;
- `samples/jozz_vehicle_validation.cpp`;
- `docs/CHECKPOINTS_PL.md`.

Wymagania:

1. `gate.ps1` boot-smoke M5 i M6;
2. raport poprawnie mówi o liczbie 3 map probes albo ją wylicza;
3. gate wykrywa źródło nowsze od obiektu/binarki objętej targetem albo wymusza
   poprawny rebuild; nie może ponownie nazwać starej binarki „fresh”;
4. po każdym buildzie sprawdza exit code, istnienie outputu i jego timestamp;
   napis `build: OK` przy nieistniejącym `.exe` jest FAIL;
5. nie wymaga renderera poza smoke;
6. nie zmienia physics core.

Acceptance: świeży target ma obiekt/binarkę nowszą od zmienionego źródła,
M5/M6 po 300 klatek mają 0 błędów, licznik sond jest prawdziwy.
Rollback: revert dwóch dokładnych plików i checkpointu.
Następny: `WP-GATE-B`.

### WP-GATE-B — realny probe aktywnego kampusu

Moc: **HIGH**.
Stan początkowy: **LOCKED** — wymaga WP-GATE-A `ACCEPTED`.
Cel: walidacja E2R mierzy shape'y utworzone przez ten sam builder co runtime.

Allowed files:

- `samples/CMakeLists.txt`;
- `samples/validation/jozz_probes_map.cpp`;
- `samples/jozz_vehicle_central_test_campus.cpp`;
- `samples/jozz_vehicle_central_test_campus.h`;
- `samples/jozz_vehicle_central_test_campus_builder.cpp`;
- `samples/jozz_vehicle_central_test_campus_builder.h`;
- `samples/jozz_vehicle_obstacle_kit.cpp`;
- `samples/jozz_vehicle_obstacle_kit.h`;
- `docs/CHECKPOINTS_PL.md`.

Wymagania:

1. izolowany world wywołuje produkcyjny builder E2R;
2. probe mierzy realne AABB, body/shape count i category bits;
3. fixture z shape'em w core failuje;
4. fixture poza station footprint failuje;
5. probe nie kopiuje receptur placementu;
6. top-surface ramp nie wchodzi do tego WP;
7. physics core i E3 pozostają bez zmian.

STOP:

- jeżeli builder jest zbyt sprzężony z Sample/UI, nie kopiować jego logiki do
  validatora; WP wraca do `LOCKED`, a HIGH przygotowuje osobny WP ekstrakcji z
  dokładną listą plików;
- nie nazywać testu „world probe”, jeśli nadal sprawdza tylko specs.

Acceptance: testy negatywne rzeczywiście failują przed fixem i przechodzą po
nim; realny builder i runtime używają tej samej ścieżki.
Rollback: revert wyłącznie probe/linkage; aktywna mapa nie zmienia się w tym WP.
Następny: `WP-C0`.

### WP-OBS-01 — kontrakt top-surface obstacle kitu

Moc: **HIGH**.
Stan początkowy: **LOCKED** — nie jest częścią recovery C; odblokowuje go tylko
wybór S-impact w WP-C5 albo późniejszy WP-L0.
Cel: zablokować nieplanowane schodki przed ponownym użyciem ramp.

Allowed files:

- `samples/jozz_vehicle_obstacle_kit.cpp`;
- `samples/jozz_vehicle_obstacle_kit.h`;
- `samples/validation/jozz_probes_map.cpp`;
- `samples/CMakeLists.txt`;
- `docs/CHECKPOINTS_PL.md`;
- bez placementu mapy.

Pierwszy test przed zmianą ma odtworzyć aktywny błąd 6 m / 1,2°:

- oczekiwany matematyczny rise około 0,1257 m;
- bieżący top: 0,3000→0,4257 m;
- bieżący kolejny top: 0,7257→0,6000 m;
- summit step 0,3000 m;
- exit drop 0,6000 m.

Kroki:

1. dodać sampler top-surface niezależny od renderera;
2. test RED dla entry/summit/exit;
3. ustalić jawny datum: `surface top at entry`, nie body center/corner;
4. poprawić jedną rodzinę ramp;
5. test continuity w tolerancji ustalonej przed fixem;
6. dopiero potem zastosować ten kontrakt do kolejnych generatorów osobnymi WP.

Nie naprawiać wszystkich 15 generatorów naraz. Pierwszy WP może zakończyć się
testem + jedną rampą referencyjną.

Acceptance: top-surface odpowiada specowi, widok z koła nie pokazuje lip i cały
gate jest zielony. WP nie może zakończyć się z permanentnie czerwonym testem.
Rollback: revert helpera, receptury i fixture albo wrócić do ostatniego
zielonego kandydata; brak aktywnej rampy jest poprawnym wynikiem.
Następny: dokładnie WP wskazany przez WP-C5 albo WP-L0, który odblokował ten WP.

## 8. Centralny kafel — małe work-package'e

### WP-C0 — baseline i budżety C

Moc: **LOW**.
Stan początkowy: **LOCKED** — wymaga WP-GATE-B `ACCEPTED`.
Cel: przed zmianą zapisać faktyczne core/spokes/loop/occupied ratio.

Allowed files: wyłącznie `docs/MAPA_EVIDENCE_MANIFEST_PL.md` i
`docs/CHECKPOINTS_PL.md`; wszystkie `.cpp/.h` są forbidden.
Wynik: tabela realnych AABB i trzy stałe kadry.
STOP: jeśli specs i builder się rozjeżdżają, nie „poprawiać liczb”; eskalacja do
HIGH.
Następny: `WP-C1`.

### WP-C1 — jedno źródło prawdy stacji i contentu

Moc: **HIGH**.
Stan początkowy: **LOCKED** — wymaga WP-C0 `ACCEPTED`.
Cel: każdy realny shape ma station ID i jest walidowany względem stacji.

Allowed files:

- `samples/jozz_vehicle_central_test_campus.cpp`;
- `samples/jozz_vehicle_central_test_campus.h`;
- `samples/jozz_vehicle_central_test_campus_builder.cpp`;
- `samples/jozz_vehicle_central_test_campus_builder.h`;
- `samples/validation/jozz_probes_map.cpp`;
- `docs/CHECKPOINTS_PL.md`.

Invariants:

- zero zmiany widocznej geometrii, jeśli nie jest konieczna do naprawy
  naruszenia;
- core/spokes/loop nie mogą się zwęzić;
- brak nowych stacji.

Acceptance: kontrolowane przesunięcie fixture poza footprint failuje.
Następny: `WP-C1B-CONTENT-BRIEF`.

### WP-C1B-CONTENT-BRIEF — ratyfikacja feedbacku o gęstości

Moc: **HUMAN/JOZZ**.
Stan początkowy: **LOCKED** — wymaga WP-C1 `ACCEPTED`.
Bez kodu. Jozz wybiera jawnie:

- `KEEP DENSITY` — zachować odczucie bogatego centrum;
- `ADAPT COMPOSITION` — zachować bogactwo, zmniejszyć fragmentację i poprawić
  linie jazdy;
- `REJECT QUOTA` — liczba 401/147 nie jest progiem odbioru.

Można wybrać `KEEP DENSITY + ADAPT COMPOSITION + REJECT QUOTA`; jest to
rekomendacja audytu. Brief określa także, czy docelowo E ma 1, 2 czy maksymalnie
3 role wysp. Bez zapisu tej decyzji WP-C2 i WP-C3 pozostają `LOCKED`.
Następny: `WP-C2-N-COMFORT`.

### WP-C2-N-COMFORT — jedna receptura bumperów

Moc: **MEDIUM**.
Stan początkowy: **LOCKED** — wymaga WP-C1B `ACCEPTED`.
Cel: wyodrębnić i odebrać jedną czytelną recepturę komfortu; pozostałe banki
nie są automatycznie kasowane ani akceptowane, pozostają wyłączonym materiałem
do późniejszych osobnych decyzji.

Przed implementacją zapisać:

- wysokość i promień profilu;
- długość sekcji i odstęp;
- min/max speed;
- max lip;
- oczekiwany contacts/wheel-contact signal;
- minimalny approach/runoff;
- maksymalny shape count.

Allowed files:

- `samples/jozz_vehicle_central_test_campus.cpp`;
- `samples/jozz_vehicle_central_test_campus.h`;
- `samples/jozz_vehicle_central_test_campus_builder.cpp`;
- `samples/jozz_vehicle_central_test_campus_builder.h`;
- `samples/validation/jozz_probes_map.cpp`;
- `docs/MAPA_EVIDENCE_MANIFEST_PL.md`;
- `docs/CHECKPOINTS_PL.md`.

Forbidden: E/S/W, tor, nowy generator, dekoracje.

Testy:

- przejazd w obie strony przy min i max speed;
- widok z koła;
- brak niespodziewanego wyrzutu;
- wolny bypass nadal istnieje;
- porównanie OFF/ON overlay.

Stan końcowy: `WAITING_FOR_JOZZ`. Jozz wybiera `ACCEPT / ADAPT / REJECT`.
Następny po ACCEPT: `WP-C3-E-CATEGORY`.

### WP-C3-E-CATEGORY — kontakt dyskretnej skały

Moc: **HIGH**.
Stan początkowy: **LOCKED** — wymaga WP-C2 `ACCEPTED`.
Cel: wybrać kategorię/geometrię skały na podstawie M5/M6, nie nazwy materiału.

Allowed files:

- `samples/jozz_vehicle_obstacle_kit.cpp`;
- `samples/jozz_vehicle_obstacle_kit.h`;
- `samples/validation/jozz_probes_map.cpp`;
- `samples/CMakeLists.txt`;
- `docs/CHECKPOINTS_PL.md`.

Porównać: całość terrain, całość object oraz rozdzielony top/side. Mierzyć
rolling-sphere i sidewall contacts M6, najazd bokiem, zakleszczenie i M5.
Nie zmieniać placementu wysp. Wynik ma postać `ADAPT: <wariant> + dowód`.
Następny: `WP-C3-E-ISLAND`.

### WP-C3-E-ISLAND — jedna wyspa terenowa

Moc: **MEDIUM**, HIGH jeśli zmienia się generator/kategoria.
Stan początkowy: **LOCKED** — wymaga WP-C3-E-CATEGORY `ACCEPTED`.
Cel: jedna duża, czytelna wyspa z 2–3 liniami, nie setki drobin.

Allowed files:

- `samples/jozz_vehicle_central_test_campus.cpp`;
- `samples/jozz_vehicle_central_test_campus.h`;
- `samples/jozz_vehicle_central_test_campus_builder.cpp`;
- `samples/jozz_vehicle_central_test_campus_builder.h`;
- `samples/jozz_vehicle_obstacle_kit.cpp` i
  `samples/jozz_vehicle_obstacle_kit.h` wyłącznie dla zaakceptowanego
  wariantu kategorii/geometrii;
- `samples/validation/jozz_probes_map.cpp`;
- `docs/MAPA_EVIDENCE_MANIFEST_PL.md`;
- `docs/CHECKPOINTS_PL.md`.

Kontrakt:

- jawny seed;
- occupied ratio w przedziale ustalonym przed zmianą;
- minimalna szerokość każdej deklarowanej linii = większa z obwiedni M5/M6
  przy skręcie ±10° plus 0,5 m z każdej strony; probe raster/sweep potwierdza
  ciągłość entry→exit;
- wolny bypass;
- max shape count;
- brak kamienia niewidocznego na entry;
- najazd prosto, ±30° i bokiem;
- brak zakleszczenia M5/M6;
- kategoria kontaktu potwierdzona przez probe.

Nie zwiększać liczby shape'ów, aby zaliczyć gęstość. Jeżeli wyspa jest
nieczytelna, poprawić kompozycję grup, nie quota.

Stan końcowy: `WAITING_FOR_JOZZ`.
Następny: `WP-C3-COUNT-CHOICE` po ACCEPT.

### WP-C3-COUNT-CHOICE — liczba ról wysp E

Moc: **HUMAN/JOZZ**.
Stan początkowy: **LOCKED** — wymaga pierwszej wyspy `ACCEPTED`.
Jozz wybiera `STOP_AT_1 → WP-C4-PROPS` albo `ADD_ROLE_2 → WP-C3B-E-ISLAND`.
Po odbiorze drugiej wybiera `STOP_AT_2 → WP-C4-PROPS` albo
`ADD_ROLE_3 → WP-C3C-E-ISLAND`. WP-C3B i WP-C3C kopiują pełny kontrakt/testy
WP-C3, mają osobny footprint, seed, commit i sign-off. Trzecia wyspa jest
maksimum, nie obowiązkiem.

### WP-C4-PROPS — zatoka interakcyjna

Moc: **MEDIUM**.
Stan początkowy: **LOCKED** — wymaga zakończonej ścieżki WP-C3-COUNT-CHOICE.
Cel: odzyskać interakcyjność pierwszej mapy bez scatteru w centrum.

Kontrakt:

- 6–8 lekkich propów;
- kategoria `0x1`;
- ręczny deterministyczny placement;
- żadnego AABB w core/spoke/loop/approach/runoff;
- reset przywraca pose/velocity;
- 10 resetów bez wzrostu body count;
- przycisk Reset props ma widoczny skutek;
- propy nie przemieszczają się do innej strefy po restarcie.

Allowed files:

- `samples/jozz_vehicle_m5_test_course.cpp`;
- `samples/jozz_vehicle_m5_test_course.h`;
- `samples/jozz_vehicle_central_test_campus.cpp`;
- `samples/jozz_vehicle_central_test_campus.h`;
- `samples/validation/jozz_probes_map.cpp`;
- `samples/jozz_vehicle_m5_drivable_lab.cpp` i
  `samples/jozz_vehicle_m6_rig_lab.cpp` wyłącznie dla podłączenia istniejącego
  resetu;
- `docs/CHECKPOINTS_PL.md`.

STOP: potrzeba nowego ogólnego PropRegistry → osobny HIGH WP.
Stan końcowy: `WAITING_FOR_JOZZ`.
Następny: `WP-C5-CHOICE`.

### WP-C5-CHOICE — wybór kolejnej luki przez Jozza

Moc: **HUMAN**.
Stan początkowy: **LOCKED** — wymaga WP-C4 `ACCEPTED`.
Nie implementować niczego. Przedstawić stałe kadry C oraz trzy opcje:

- zachować W/S jako negative space;
- dodać jeden łagodny S impact po WP-OBS;
- zaprojektować nowy, płaski W test bez dawnej artykulacji.

Jozz wybiera dokładnie jedną albo kończy kampus bez niej. Model nie zakłada, że
cztery ćwiartki muszą być symetrycznie wypełnione.

Mapowanie bez zgadywania:

- `NONE / NEGATIVE_SPACE → WP-C6-INTEGRATION`;
- `S_IMPACT → WP-OBS-01 → WP-C5-S-IMPACT → WP-C6-INTEGRATION`;
- `W_FLAT → WP-C5-W-BRIEF → WP-C5-W-FLAT → WP-C6-INTEGRATION`.

`WP-C5-S-IMPACT` używa jednej naprawionej łagodnej rampy i kontraktu WP-OBS;
allowed files są takie jak WP-OBS plus central builder/spec/probe, zero E3.
`WP-C5-W-BRIEF` jest HUMAN i zamraża footprint/cel bez brył;
`WP-C5-W-FLAT` jest MEDIUM/HIGH, ma dokładną listę plików dopisaną do zlecenia
po briefie i nie może użyć dawnej articulation/off-camber. Każdy kończy się
`WAITING_FOR_JOZZ`.

### WP-C6-INTEGRATION — pełny kampus

Moc: **HIGH test/analysis + HUMAN feel**.
Stan początkowy: **LOCKED** — odblokowuje go tylko jedna z trzech ścieżek C5.
Cel: udowodnić całość, nie dodawać geometrii.
Allowed files: wyłącznie `docs/MAPA_EVIDENCE_MANIFEST_PL.md` i
`docs/CHECKPOINTS_PL.md`; source jest read-only.

Test:

1. core spawn i obrót;
2. core→N→loop→E→bay→core bez cofania;
3. każda aktywna stacja w deklarowanym kierunku;
4. dwukierunkowe stacje w obu kierunkach;
5. 10 restartów/resetów;
6. M5 i M6;
7. product overlay OFF;
8. stałe kadry porównawcze;
9. ręczna jazda Jozza.

Wyjście: tylko Jozz może dodać `E2R ACCEPTED BY JOZZ <hash>`.
Jedyny następny po ACCEPT: `WP-T0`.

## 9. Jedna pętla — work-package'e

### WP-T0 — brief i metryki

Moc: **HIGH + HUMAN**.
Stan początkowy: **LOCKED** — wymaga `E2R ACCEPTED BY JOZZ <hash>`.
Allowed files: master, dokument E3, `docs/MAPA_EVIDENCE_MANIFEST_PL.md` i
checkpoint; wszystkie źródła są read-only.
Bez buildera. Ustalić rolę pętli, prędkości, promienie, footprint, relację prostej
220 m i connector C. Brief musi także zamrozić wolny footprint przyszłego
driftu W; pętla nie może skonsumować całego kafla W. Jozz akceptuje brief.
Następny: `WP-T1`.

### WP-T1 — jedna centerline i topology validator

Moc: **HIGH**.
Stan początkowy: **LOCKED** — wymaga WP-T0 `ACCEPTED`.
Allowed files: `samples/jozz_vehicle_track_layout.cpp`,
`samples/jozz_vehicle_track_layout.h`,
`samples/validation/jozz_probes_map.cpp`, `samples/CMakeLists.txt` i
`docs/CHECKPOINTS_PL.md`. Forbidden: builder/visual/profiles.

Validator mierzy:

- closure i direction;
- długość;
- min radius/curvature;
- self-intersection;
- runoff bounds;
- connector C;
- prawdziwy hairpin/chicane/arc;
- brak kolizji z C, offroad gate i zarezerwowanymi yardami.

Stan końcowy: techniczne ACCEPT, bez akceptacji layoutu.
Następny: `WP-T2`.

### WP-T2 — skeleton i STOP

Moc: **MEDIUM render + HUMAN**.
Stan początkowy: **LOCKED** — wymaga WP-T1 `ACCEPTED`.
Allowed files: `samples/jozz_vehicle_track_visual.cpp`,
`samples/jozz_vehicle_track_visual.h`,
`docs/MAPA_EVIDENCE_MANIFEST_PL.md` i `docs/CHECKPOINTS_PL.md`.
Forbidden: physical builder i layout data.

Dowody: plate top, center view, start driver, każdy krytyczny zakręt, powrót.
Skeleton musi używać tej samej centerline i realnej szerokości co builder będzie
używał później.
Wyjście: `WAITING_FOR_JOZZ`; bez `E3 SKELETON ACCEPTED BY JOZZ` WP-T3 jest
LOCKED.

### WP-T3 — płaska baza, budowana raz

Moc: **HIGH**.
Stan początkowy: **LOCKED** — wymaga `E3 SKELETON ACCEPTED BY JOZZ <hash>`.
Allowed files: `samples/jozz_vehicle_track_builder.cpp`,
`samples/jozz_vehicle_track_builder.h`, `samples/jozz_vehicle_m5_test_course.cpp`,
`samples/jozz_vehicle_m5_test_course.h`,
`samples/validation/jozz_probes_map.cpp`, `samples/CMakeLists.txt` i
`docs/CHECKPOINTS_PL.md`. Forbidden: profiles, barriers, timer, drift, landings.

Kontrakt:

- jedna aktywna pętla;
- shared segment tylko raz;
- top y ma jedno źródło prawdy;
- brak coplanarnego konkurencyjnego contact surface;
- miter limit i fallback na łagodne połączenie;
- builder/visual WYSIWYG;
- pełne okrążenie M5/M6 bez teleportu.

Failure:

- giant miter → nie clampować arbitralnie; wrócić do centerline/min radius;
- z-fighting/jitter → A/B z bazą, zmierzyć contacts i `vy RMS`;
- niepełna pętla → nie teleportować przez wadę dla dowodu.

Stan: `WAITING_FOR_JOZZ`.
Następny po ACCEPT: `WP-T4`.

### WP-T4 — runoff i bezpieczeństwo

Moc: **HIGH/MEDIUM**.
Stan początkowy: **LOCKED** — wymaga WP-T3 `ACCEPTED`.
Allowed files: `samples/jozz_vehicle_track_layout.cpp`,
`samples/jozz_vehicle_track_layout.h`, `samples/jozz_vehicle_track_builder.cpp`,
`samples/jozz_vehicle_track_builder.h`,
`samples/validation/jozz_probes_map.cpp`, evidence i checkpoint. Każdy konkretny
WP-T4 wybiera tylko jedną grupę zabezpieczeń z tej listy.
Jedna analiza wypadnięcia i jedna grupa zabezpieczeń na WP. Bariera nie może
zastąpić brakującego runoff. Ponowny full lap.
Następny: `WP-T5-BRANCH-CHOICE`.

### WP-T5-BRANCH-CHOICE — opcjonalny branch

Moc: **HUMAN**.
Stan początkowy: **LOCKED** — wymaga WP-T4 `ACCEPTED`.
Allowed files: tylko dokument briefu/evidence i checkpoint; source read-only.
Jozz wybiera `NONE` albo jeden branch. `NONE` jest poprawnym wynikiem. Jeżeli
branch: `WP-T5B-CENTERLINE → WP-T5C-SKELETON → WP-T5D-BUILDER → full lap`.
`NONE → WP-T6`. Każdy WP branch ma dokładne allowed files odziedziczone z
odpowiedniego T1/T2/T3 i nie zmienia bazowej pętli.

### WP-T6 — lap timer

Moc: **MEDIUM**.
Stan początkowy: **LOCKED** — wymaga decyzji `NONE` albo zaakceptowanego branchu.
Dopiero po zaakceptowanej geometrii. Chassis-only, direction, debounce,
teleport/reset invalidate, wrong-way tests. Timer nie zmienia buildera. Allowed
files: nowe `samples/jozz_vehicle_lap_timer.cpp` i
`samples/jozz_vehicle_lap_timer.h`; callsite, test i CMake muszą dostać literalne
ścieżki w zleceniu przed zmianą stanu z `LOCKED`; żadnej geometrii toru.

## 10. Dalsze strefy — zawsze osobno

### Drift W

Sekwencja: `WP-D0 footprint` → `WP-D1 friction contract` → `WP-D2 skeleton STOP`
→ `WP-D3 one skid pad` → feel-test → opcjonalna ósemka.

Nie wolno:

- tworzyć skid padu i ósemki w jednym WP;
- robić fizycznego progu materiału;
- zajmować SW;
- wypuszczać propów poza containment.

### Landing E/SE

Sekwencja: `WP-L0 ramp probes all green` → `WP-L1 footprint/approach/landing`
→ `WP-L2 skeleton STOP` → `WP-L3 mild tabletop` → ręczny test → opcjonalny
`WP-L4 gap jump`.

Każdy skok raportuje entry speed, airtime, landing footprint i escape. Twardy
gap nie jest automatycznym następnikiem tabletop.

### Physics Yard SW

Sekwencja: ownership/reset infrastructure → containment → dokładnie jedno z:
shaker / rolling road / bridge / see-saw. Każdy przyrząd ma osobny WP, własny
measurement signal i 10-cyklowy reset test.

### Spawner/Stress S–SE

Sekwencja:

1. PropRegistry i ownership;
2. jeden spawn w yardzie;
3. reset/despawn;
4. mała partia z twardym limitem;
5. batch 25/50/100/250;
6. perf i cleanup;
7. dopiero potem UI convenience.

Zakazane: domyślny spawn 250 przed autem i używanie C jako stress yard.

### Nawigacja i telemetria

Po stabilizacji wszystkich aktywnych stref:

- anchor zawsze przed wejściem i poza shape;
- nazwa mówi relację do C;
- teleport zeruje stan właściwego testu;
- UI nie pokazuje nieaktywnych/odrzuconych stref;
- telemetry nie jest używana do maskowania złej geometrii.

## 11. Typowe awarie i reakcja

| Objaw | Najpierw sprawdź | Czego nie robić |
|---|---|---|
| validator OK, render zły | czy probe buduje realne shape'y | nie zwiększaj quota |
| próg na rampie | datum top-surface i half-thickness | nie zmniejszaj tylko kąta |
| track wygląda jak klin | curvature i miter length | nie clampuj bez pomiaru |
| auto przejechało, ręcznie źle | kamera, feel, linia wejścia | nie uznawaj auto za sign-off |
| wheel łapie skałę bokiem | category/envelope M6 | nie wyłączaj wszystkich kontaktów terenu |
| podwójne kontakty | coplanarne surface'y | nie zwiększaj solver iterations w core |
| shape count nagle rośnie | generator count i restart cleanup | nie podnoś limitu po fakcie |
| overlay zasłania mapę | default toggle i culling | nie usuwaj narzędzia diagnostycznego |
| brak miejsca w C | negative space i scope stacji | nie wciskaj generatora, bo istnieje |
| WP wymaga 8 plików | rozbij odpowiedzialności | nie rozszerzaj cicho allowed files |
| build Windows ma Path/PATH issue | użyj repo-znanego wrappera `cmd /c "set PATH=& ..."` | nie zmieniaj globalnego PATH |
| parallel build rusza ZERO_CHECK | buduj sekwencyjnie | nie diagnozuj jako losowy błąd kodu |

## 12. Reguły rollbacku

- WP danych: przywrócić jego commit/diff, nie cały branch;
- WP feature: default-off switch pozostaje drogą bezpiecznego powrotu;
- odrzucony skeleton: zachować obraz i decyzję, usunąć aktywację;
- odrzucona geometria: nie polerować, tylko quarantine/revert WP;
- crash/NaN: wrócić do ostatniego zielonego WP przed dalszą diagnozą;
- nie używać `git reset --hard`, `git clean` ani zbiorczego restore cudzych
  plików;
- rollback musi pozostawić pełny gate zielony albo jawnie wrócić do znanego
  baseline'u.

## 13. Szablon zlecenia dla Luny

Kopiować i wypełnić bez usuwania pól:

```text
WP_ID:
POWER: LOW / MEDIUM / HIGH
OBJECTIVE:
WHY_NOW:
SOURCE_OF_TRUTH:
PRECONDITIONS:
ACCEPTED_PREDECESSOR_HASH:
READ_ORDER:
ALLOWED_FILES:
FORBIDDEN_FILES:
INVARIANTS:
PRE_CHANGE_METRICS:
STEPS:
COMMANDS:
EXPECTED_OUTPUT:
SCREENSHOT_MANIFEST:
AUTOMATIC_ACCEPTANCE:
DRIVE_ACCEPTANCE:
HUMAN_ACCEPTANCE:
STOP_CONDITIONS:
KNOWN_FAILURES:
ROLLBACK:
CHECKPOINT_TEXT:
ONLY_LEGAL_NEXT_WP:
```

Jeśli którekolwiek pole wymagane dla danego WP jest puste, Luna nie zaczyna
implementacji. Uzupełnia brak read-only, a jeśli wymaga decyzji produktu — pyta
Jozza i kończy turę.

## 14. Pierwsze polecenie dla następnej sesji Luny

```text
Wykonaj wyłącznie WP-00 z docs/PLAN_WYKONAWCZY_MAPA_GPT_LUNA_PL.md.
Pracuj read-only do chwili, gdy Jozz wybierze sposób zachowania obecnego
mieszanego WIP. Nie stage'uj, nie commituj, nie pushuj i nie edytuj geometrii.
Najpierw przeczytaj README_FOR_AGENTS.md, master plan §0, audyt oraz WP-00.
Zwróć inventory z klasyfikacją E1/E2R_KEEP/E2R_FIX/E3_QUARANTINE/DOCS/UNKNOWN,
rekomendowany sposób snapshotu i dokładny zakres WP-01. Zatrzymaj się.
```

To jest celowe. Najbliższym zadaniem nie jest kolejna przeszkoda ani naprawa
toru. Najpierw trzeba odzyskać możliwość bezpiecznego rozdzielenia i cofania
pracy.
