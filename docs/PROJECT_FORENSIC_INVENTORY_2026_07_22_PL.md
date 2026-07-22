# Project Forensic Inventory — wynik pierwszego pełnego przekroju

**Data:** 2026-07-22  
**Branch audytu:** `agent/project-refoundation-audit-v1`  
**Draft PR:** #16  
**Status:** `F2_IN_PROGRESS / NO_PRODUCT_IMPLEMENTATION`  
**Machine-readable companion:** `docs/PROJECT_INVENTORY.json`

## 1. Cel

Ten raport zamienia ogólną deklarację „prześwietlić cały projekt” w kontrolowany
inventory. Nie twierdzi, że każda linia C++ i każdy historyczny dokument zostały już
ręcznie przeczytane. Ustala natomiast:

- jakie domeny faktycznie istnieją;
- co jest authority, fundamentem, eksperymentem, historią lub długiem;
- które dokumenty potrafiły skierować agenta na złą operację;
- jakie lineage PR/branch trzeba zachować;
- co musi się wydarzyć przed teksturami, skalą, kolizją i pierwszą jazdą;
- jak zredukować branche do maksymalnie pięciu, preferencyjnie trzech.

## 2. Exact snapshot i ograniczenia wykonawcze

W chwili rozpoczęcia F2:

```text
authoritative product branch: agent/scan-terrain-r1b-consolidated-integration
authoritative product head:   odczytywany z Control Issue #11
active product PR:             #13
re-foundation PR:              #16
re-foundation starting head:   4052f49d3b3b3e60a61e8d17cd4a20d7eb67d1aa
highest proven capability:     TERRAIN_VISIBLE_PASS
next product gate:             TEXTURED_SOURCE_PREVIEW
```

Connector-side GitHub pozwala odczytywać exact PR heads, pliki, commity i diffy, ale
w tej sesji nie zwrócił kompletnej listy branch refs. Dlatego żaden branch nie został
uznany za istniejący lub usunięty wyłącznie na podstawie nazwy historycznego PR-a.
Przed cleanupem obowiązuje niezależne:

```powershell
git ls-remote --heads origin
```

Wynik ma zostać zapisany jako redacted, publicznie bezpieczny branch/SHA inventory.

## 3. Co zostało przejrzane w tej iteracji

### 3.1. Topologia GitHub

Odtworzono role PR-ów:

```text
#1  P1 inspection
#2  P1B world import contracts
#3  P1B evidence bundle
#4  P1B owner gate + reliable Windows gate
#5  P2A exact geometry preview
#6  temporary CI-only trigger — never merge
#7  divergent surface-evidence foundation — parked
#8  historical owner-safe real frame flow
#9  restacked source resolution + resumable owner flow
#10 automation safety foundation
#12 operating documentation
#13 consolidated product integration — current authority
#15 repository governance — merged into #13 branch
#16 re-foundation audit — temporary, non-authoritative
```

Kampania jest grafem lineage, nie kolekcją równorzędnych feature branchy. Zamknięty PR
może być:

- osiągalnym przodkiem obecnej konsolidacji;
- divergent evidence;
- historycznym odpowiednikiem później restackowanej implementacji;
- jednorazowym technicznym triggerem.

### 3.2. Zakres integracji

Porównanie stabilnego vehicle baseline’u z początkiem F2 wykazało długą linię około
195 commitów. Największe rodziny zmian:

- scan inspection/evidence i contracts;
- native geometry preview;
- owner-local private flow;
- scan tests;
- automation/governance;
- dokumentacja i CI routing.

To potwierdza, że PR #13 nie powinien być dalej rozszerzany o tekstury, skalę i
kolizję. Następna kampania potrzebuje osobnej małej powierzchni po decyzji integracyjnej.

### 3.3. Runtime/build composition

`samples/CMakeLists.txt` ujawnia obecny praktyczny monolit hosta:

```text
samples.exe
├── upstream sample host
├── vehicle labs and visuals
├── synthetic engineering world
└── scan preview lab
```

To nie jest natychmiastowy błąd. Jest to jednak istotna granica architektoniczna:
scan preview korzysta z istniejącego produktywnego hosta, ale nie może przez przypadek
stać się właścicielem vehicle core, shared host albo przyszłego JES.

### 3.4. Dokumentacja vehicle

Przejrzano:

- `README_FOR_AGENTS.md` jako manual vehicle;
- `docs/CURRENT_STATE_INDEX_PL.md` jako szczegółowy ledger;
- `docs/CHECKPOINTS_PL.md` jako chronologiczną historię;
- `docs/TECH_DEBT_PL.md` jako debt registry.

Wykryto dwa krytyczne konflikty operacyjne w starym tech-debt:

1. historyczną instrukcję direct push na `jozz-vehicle-sandbox-m0`;
2. historyczne nazwanie `README_FOR_AGENTS.md` globalnym front doorem.

Obie instrukcje zostały zastąpione przez aktualny model:

```text
AGENTS.md
→ exact remote SHA
→ isolated branch
→ validation
→ draft PR
→ owner review
```

Szczegóły vehicle debt zostały zachowane w krótszym, domain-only rejestrze.

### 3.5. Dokumentacja scan

`docs/scan_import/00_START_HERE.md` nadal twierdził, że aktualnym etapem jest
`P1B_OWNER_GATE_HARDENING`, brak realnego bundle’a oraz następny occupancy package.
To była jawna historyczna instrukcja sprzeczna z ukończonym `TERRAIN_VISIBLE_PASS`.

Plik został przepisany jako current router:

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
```

Historyczne komendy nie zostały „unieważnione” technicznie; pozostają w historii Git i
checkpointach jako evidence lineage. Nie są już current queue.

### 3.6. Trwała granica scan architecture

`docs/scan_import/ARCHITECTURE.md` nadal zawiera zdrową nadrzędną zasadę:

```text
scan source/evidence
→ generated proposal
→ explicit authored acceptance
→ rebuildable render/collision projections
```

Dokument ma stary status P1B i starszą capability ladder, więc wymaga późniejszego
bounded update. Nie należy go jednak przepisywać przed zakończeniem inventory, bo jego
semantyczne jądro chroni projekt przed automatyczną promocją evidence do world truth.

## 4. Mapa domen

### A. Upstream Box3D core

```text
src/**
include/**
```

Status: `FOUNDATION_PRESERVE`.

Nie jest powierzchnią wygodnego patchowania. Jakakolwiek zmiana wymaga osobnej decyzji
owner/upstream impact i pełnej walidacji.

### B. Shared native host

```text
samples/host/**
samples/main.cpp
samples/sample.*
samples/CMakeLists.txt
```

Status: `FOUNDATION_PRESERVE`.

Małe opt-in hooki są dopuszczalne, ale każda zmiana wpływa potencjalnie na vehicle,
scan oraz upstream samples.

### C. Vehicle physics, rig i creator labs

Status: `FOUNDATION_PRESERVE`.

Zaakceptowane jądro:

- M7 honest real-forces architecture;
- M8 rig/poza/persistence;
- owner-controlled visuals and presets;
- headless probes i owner feel gates.

Nie wolno „ułatwiać” scan drive przez zmianę zachowania pojazdu.

### D. Synthetic engineering world

Status: `FOUNDATION_PRESERVE`.

Rola:

```text
synthetic world = deterministyczne laboratorium regresji
real scan       = autentyczne doświadczenie i przyszły authored world material
```

Pierwszy nie jest zastępowany drugim.

### E. Scan inspection/evidence

Status: `FOUNDATION_PRESERVE`.

Zachować:

- strict parsing;
- source frame contract;
- content-derived revisions;
- private/shareable split;
- immutable bundle;
- independent verifier;
- privacy fail-closed behavior.

### F. Geometry preview v1

Status: `FOUNDATION_PRESERVE / CLOSED_FORMAT`.

Udowodniono realną widoczność i repeatable restart. Nie udowodniono tekstur, finalnej
skali, accepted surface, collision ani drive.

### G. Textured source preview

Status: `ACTIVE_NEXT`.

Wymaga nowego adjacent contractu, a nie dopisania danych do manifestu, który nadal
mówi `texturesIncluded=false`.

### H. Vehicle scale-reference scene

Status: `OWNER_DECISION_REQUIRED`.

Dopiero rozpoznawalny textured teren plus zaakceptowany samochód na drodze lub obok
znanego obiektu pozwalają nadać `WORLD_SCALE_VALIDATED`.

### I. Surface/collision

Status: `PARKED_WITH_REACTIVATION_GATE`.

PR #7 przechowuje wartościowe evidence/contracts, ale nie accepted ground truth.
Porównanie triangle mesh / PLY-derived heightfield / Blender proxy nastąpi dopiero dla
jednego Golden Drive Region po teksturach i skali.

### J. Long-term world authoring

Status: `KNOWN_DEBT / OWNER_VISION`.

Przyszły workflow obejmuje:

- nowe skany w pochmurny dzień;
- mocny cleanup w Blenderze;
- osobne roads/terrain/houses/forest/vegetation;
- low-poly obiekty i LOD;
- surface material maps: asphalt, grass, mud i dalsze klasy;
- composition/streaming wielu skanów.

Ta wieloletnia wizja nie blokuje pierwszego proof of fun.

### K. Automation/governance

Status: `FOUNDATION_PRESERVE / PLAN_ONLY`.

Ma chronić pracę, nie stać się produktem. Recurring run nie może sam modyfikować
control plane ani wymyślać cleanupu przy braku bezpiecznego work itemu.

### L. JES boundary

Status: `OWNER_DECISION_REQUIRED`.

Do JES mogą przejść kontrakty, lessons, synthetic fixtures i neutralne schema po jawnej
adopcji. Nie przechodzą automatycznie prywatne skany, sample-host coupling, magiczne
progi ani struktura tego laboratorium.

## 5. Branch reduction — wymagany wynik końcowy

Właściciel ustalił:

```text
hard maximum after cleanup: 5 branches
preferred final state:       3 branches
```

### 5.1. Preferowany stan trzybranchowy

```text
main
jozz-vehicle-sandbox-m0
ONE_CURRENT_INTEGRATED_PROJECT_BRANCH
```

Trzeci branch zostanie nazwany dopiero po decyzji, czy #13:

- merge as-is;
- otrzyma bounded correction;
- pozostanie preserved milestone, a nowy baseline powstanie inną drogą.

Nie należy tworzyć nowej „finalnej” nazwy przed tą decyzją.

### 5.2. Do usunięcia po ancestry/content verification

Poniższe nazwy wynikają z historii PR. Ich aktualna obecność na remote musi zostać
potwierdzona przez `git ls-remote --heads origin`:

```text
photogrammetry/import-v2-foundation
agent/p1-dataset-inspector-staging
agent/p1b-world-import-contract-staging
agent/p1b-inspector-bundle-staging
agent/p1b-owner-gate-hardening
agent/p2a-source-visual-preview
agent/r1b-source-resolution-owner-integration
agent/autonomous-loop-foundation-v1
agent/project-operating-polish-v1
agent/r1b-repository-readiness-polish-v1
```

Powód: ich funkcjonalna zawartość powinna być osiągalna z #13 lineage. Usunięcie jest
dopuszczalne dopiero po mechanicznej weryfikacji exact SHA/ancestry.

### 5.3. Divergent heads — tag przed usunięciem brancha

```text
agent/p2a-scan-derivatives-foundation
  exact historical head: 9aacc752f331d0d47c4c9c3f6fe82c63466f592c
  proposed tag: evidence/surface-foundation-pr7

agent/p2a-real-owner-flow
  exact historical head: a36e3d2f4c76f35d138a7e8b0aa11f7889e69e90
  proposed tag: evidence/owner-flow-pr8
```

Branch #7 ma unikalną divergent pracę. Branch #8 został funkcjonalnie restackowany,
ale exact history jest inna. Przy celu trzech branchy ich wartość należy zachować
tagami, Issue #14 i PR lineage — nie trwałymi branchami.

### 5.4. Current branches

```text
agent/scan-terrain-r1b-consolidated-integration
agent/project-refoundation-audit-v1
```

Pierwszy pozostaje authority do decyzji integracyjnej. Drugi jest tymczasowym branchem
review i powinien zniknąć po integracji re-foundation. Nie usuwać żadnego podczas
otwartego review.

### 5.5. Milestone tag

Przed redukcją zalecany trwały tag:

```text
milestone/terrain-visible-2026-07-22
→ 33099413bf8f44adbe1d635f9e10bdf2d0b5c321
```

Tag zabezpiecza exact authoritative geometry milestone niezależnie od późniejszej
nazwy bieżącego brancha.

### 5.6. Delete protocol

Dla każdego brancha osobno:

1. odczytaj remote branch → exact SHA;
2. porównaj z inventory i PR;
3. sprawdź `merge-base --is-ancestor` względem zachowanego brancha albo potwierdź tag;
4. sprawdź, że branch nie jest checkoutem w żadnym owner worktree;
5. sprawdź, że nie jest aktywną bazą/headem otwartego PR-a;
6. najpierw utwórz i odczytaj wymagany tag;
7. usuń zdalny branch pojedynczo;
8. ponownie uruchom `git ls-remote --heads origin`;
9. zakończ przy ≤5; preferencyjnie 3;
10. zapisz redacted cleanup receipt bez prywatnych ścieżek.

Zakazane:

- hurtowe delete bez per-branch proof;
- force-push lub rewrite historii;
- usunięcie divergent head bez tagu;
- usunięcie current review branch;
- użycie zamkniętego PR-a jako jedynego dowodu, że branch jest redundantny.

## 6. Rzeczy nadal nieprzejrzane wystarczająco głęboko

Jawne `UNREVIEWED`, a nie ukryte „później”:

- pełna lista remote branches i tags;
- line-by-line review wszystkich 195 commitów;
- każdy niedatowany vehicle/map document;
- komplet historycznych CODEX handoffów;
- exact material/image topology realnego seven-tile GLB;
- decoded texture memory realnej sceny;
- render-distance/frustum/camera behavior przy pełnej mapie;
- najlepszy format adjacent textured pack;
- implementacyjna granica texture loadera w shared host;
- exact scale-reference scene UX;
- triangle mesh kontra heightfield kontra Blender proxy;
- przyszły streaming/LOD/world composition;
- co dokładnie migruje do JES i w którym momencie.

## 7. Natychmiastowe decyzje po F2

F2 nie implementuje produktu. Po jego zakończeniu kolejność decyzji:

```text
1. owner review re-foundation findings
2. exact remote branch inventory
3. PR #13 integration strategy
4. integrate or bound-correct project truth
5. reduce branches safely
6. write textured-preview campaign brief
7. create a fresh implementation branch
```

Nie wolno zamienić kolejności 5–7 tak, by tekstury zaczęły powstawać na nieustalonej
bazie albo branch cleanup usunął review lineage.

## 8. Definition of Done F2

F2 jest zamknięte dopiero, gdy:

- machine-readable inventory przechodzi mechaniczny audit;
- nie ma znanych stale imperative front doors;
- wszystkie domeny mają status i authority;
- wszystkie znane PR lineage mają retention class;
- exact remote branches zostały wylistowane;
- każde odstępstwo od listy PR-derived jest opisane;
- branch cleanup plan schodzi do ≤5 i ma wiarygodną drogę do 3;
- żadna prywatna dana nie została opublikowana;
- nie zmieniono vehicle physics, scan runtime, Box3D core ani product capability.
