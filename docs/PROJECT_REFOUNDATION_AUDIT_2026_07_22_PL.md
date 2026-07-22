# Project Re-foundation Audit — zamknięcie preview i przygotowanie kolejnego wieku projektu

**Start audytu:** 2026-07-22  
**Branch roboczy:** `agent/project-refoundation-audit-v1`  
**Exact base:** historyczny head zapisany w draft PR; current authority zawsze odczytaj z Issue #11  
**Klasa:** manualna owner-directed reorganizacja A3  
**Status:** `PHASE_0_STARTED` — analiza nie jest jeszcze zakończona

## 1. Dlaczego ten audyt istnieje

Projekt właśnie zakończył wielki milestone: rzeczywisty, prywatny skan został
zweryfikowany i dwukrotnie pokazany w natywnym hoście.

Jednocześnie kampania urosła do rozmiaru, przy którym nie wolno po prostu dopisać
kolejnego feature'a. W jednym repo żyją dziś:

- upstream Box3D;
- zaakceptowany vehicle sandbox M7/M8;
- syntetyczny engineering world;
- scan inspection/evidence pipeline;
- native source preview;
- eksperymenty surface/heightfield/texture;
- rozbudowana dokumentacja i historia milestone'ów;
- recurring automation oraz repository governance.

Celem audytu jest zakończyć milestone nie tylko wizualnie, lecz także
architektonicznie, organizacyjnie i dokumentacyjnie — bez zgubienia wcześniejszej
duszy projektu i bez rozpoczęcia kolizji na nieuporządkowanym fundamencie.

## 2. Najważniejsza korekta kolejności

Po feedbacku właściciela obowiązuje:

```text
SOURCE_GEOMETRY_VISIBLE
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_SELECTION
→ COLLISION_REPRESENTATION
→ FIRST_REAL_SCAN_DRIVE
```

Finalna skala nie została jeszcze zwalidowana. Zostanie oceniona dopiero, gdy
zaakceptowany samochód pojawi się na drodze albo obok znanego domu lub innego obiektu.

Tekstury są warunkiem tej oceny, a nie kosmetyką po kolizji.

## 3. Wstępne ustalenia forensic

### 3.1. Dusza projektu istniała przed fotogrametrią

Trwałe jądro:

- uczciwe, emergentne zachowanie pojazdu;
- autorskie modele i realna kontrola Jozza;
- render i feel jako ludzkie gate'y;
- małe eksperymenty przed dużą produkcją;
- narzędzia twórcy jako część wizji;
- brak zmian Box3D core jako wygodnego obejścia.

Fotogrametria nie zastępuje tego celu. Łączy pojazd z prawdziwym miejscem.

### 3.2. Syntetyczna mapa i skany pełnią różne role

```text
synthetic engineering world = deterministyczny przyrząd pomiarowy
real-scan world              = autentyczne doświadczenie i authoring świata
```

Nie należy usuwać ani marginalizować syntetycznego świata tylko dlatego, że skan
jest emocjonalnie ważniejszy. Oba są potrzebne.

### 3.3. P2A był świadomie geometry-only

Format preview v1 celowo nie zawiera UV, materiałów ani tekstur. Jego boundary jest
prawidłowy dla `TERRAIN_VISIBLE_PASS`, ale nie może być po cichu rozszerzony tak, aby
manifest geometry-only zaczął oznaczać textured world albo collision authority.

Potrzebny jest następny jawny format/capability, nie reinterpretacja v1.

### 3.4. Tekstury zostały już częściowo przebadane

Historyczny eksperyment ustalił:

- tekstury są większym budżetem niż geometria;
- 1K jest sensownym profilem pierwszego preview;
- 2K powinno być profilem porównawczym;
- BC7 nie jest blockerem pierwszego renderu;
- tekstury muszą być gotowane etapowo i cache'owalnie;
- fixed-camera runtime screenshots są obowiązkowe.

Ta praca ma zostać odzyskana jako evidence, a nie napisana ponownie od zera.

### 3.5. Surface/collision eksperymenty istnieją, ale nie są authority

Repo posiada:

- rasteryzację i ground-filter hypotheses;
- object-height maps;
- kinematyczny four-wheel probe;
- heightfield seam proof;
- zaparkowaną surface-evidence branch w Issue #14.

Nie wolno automatycznie scalać starej gałęzi ani uznawać provisional kinematic PASS za
semantic ground truth. Przed kolizją trzeba przeprowadzić osobny comparative audit.

### 3.6. Dokumentacja ponownie uległa driftowi

Current memory, current state, operating plan, PR #13 i Issue #11 nadal opisują
owner-local preview jako niewykonany. Część dokumentów opisuje także PR #15 jako
niezintegrowany, mimo że jego merge znajduje się w authoritative head.

To jest rzeczywisty conflict of truth po przesunięciu milestone'u.

### 3.7. Product PR stał się zbyt dużą powierzchnią następnej kampanii

PR #13 konsoliduje długą linię scan/import/governance. Nie wolno dopisywać do niego
tekstur, sceny skali i kolizji jako kolejnych nieograniczonych warstw.

Po zamknięciu milestone'u trzeba jawnie wybrać strategię integracji i nową, małą
powierzchnię następnej kampanii.

## 4. Warstwy prawdy wymagające formalizacji

```text
PRIVATE_SOURCE_EVIDENCE
SOURCE_GEOMETRY_PREVIEW
SOURCE_TEXTURE_PREVIEW
AUTHORED_WORLD_ASSETS
RENDER_DERIVATIVES
PHYSICS_SURFACE
SURFACE_MATERIAL_MAP
GAMEPLAY_SEMANTICS
WORLD_COMPOSITION_AND_STREAMING
```

Dla każdej warstwy audyt ustali:

- input authority;
- output contract;
- prywatność;
- builder;
- independent verifier;
- runtime consumer;
- owner gate;
- warunek promocji;
- zakazane overclaims.

## 5. Zasada pełnego inwentarza

„Niczego nie zostawić nieświadomie na potem” nie oznacza wykonania wszystkiego teraz.
Oznacza, że każda rzecz dostaje jawny status:

```text
ACTIVE_NEXT
FOUNDATION_PRESERVE
PARKED_WITH_REACTIVATION_GATE
HISTORICAL_EVIDENCE
SUPERSEDED
KNOWN_DEBT
EXPERIMENTAL_NOT_AUTHORITY
OWNER_DECISION_REQUIRED
REMOVE_AFTER_VERIFICATION
```

Docelowy inventory record dla każdego istotnego elementu:

```text
identity/path
layer/domain
responsibility
current authority
status
owner
dependencies
canonical validation
privacy risk
replaces / replaced-by
reactivation or removal trigger
```

## 6. Zakres pełnego prześwietlenia

### A. Git i integracja

- wszystkie otwarte i zamknięte PR-y związane z vehicle, map i scan;
- trwałe branche, divergent ancestry i parking issues;
- exact merge bases i niewłączone commity;
- los PR #13 po owner review;
- potrzeba milestone tag/branch bez rewrite historii.

### B. Dokumentacja i authority

- wszystkie front doors i current-state documents;
- duplikaty roadmap;
- stale milestone reports podszywające się pod current;
- `CHECKPOINTS_PL.md`, `CURRENT_STATE_INDEX_PL.md` i `TECH_DEBT_PL.md`;
- archive policy;
- mechaniczny document manifest i drift checks.

### C. Vehicle

- zaakceptowane M7/M8 i jego prawdziwe owner gates;
- manual drive items nadal pending;
- config/persistence/env hooks;
- known physical debt;
- integracja samochodu ze sceną skanu bez zmiany accepted behavior.

### D. Synthetic world

- zaakceptowany teren Etapu 1;
- odrzucony 6-lane layout;
- obstacle kit jako recovery material;
- central-campus plan;
- trwała rola synthetic world jako laboratorium regresji.

### E. Scan evidence i preview

- inspektory, contracts, bundles, receipts i privacy boundaries;
- source resolution i resumable flow;
- preview v1 geometry contract;
- camera/bounds/outlier diagnostics;
- memory/performance i format identity.

### F. Textures

- embedded images, samplers, materials, TEXCOORD accessors i primitive bindings;
- color-space i unlit/baseColor assumptions;
- 1K/2K cook profiles;
- atlas kontra per-material textures;
- decoded memory, mip strategy i przyszłe GPU compression;
- fixed-camera A/B oraz no-texture fallback;
- nowy manifest capability bez naruszania v1.

### G. Surface i collision

- historyczne DEM/ground filters;
- PLY/GLB correspondence;
- manual seeds/exclusions/forced-ground;
- triangle mesh, heightfield i Blender proxy jako konkurencyjne reprezentacje;
- Box3D shape constraints;
- kontakt kół, stabilność solvera i perf budget;
- render/collision difference visualization.

### H. Authoring i toolchain

- Blockbench/glTF/asset contracts;
- Blender jako future world authoring authority;
- rig editor requirements;
- przyszłe drogi, budynki, roślinność i LOD-y;
- material maps i creator workflow;
- granica z JES.

### I. Build, tests i CI

- canonical gates per domain;
- powielone lub historyczne workflowy;
- native/private/visual capability gaps;
- testy dające green mimo błędnych liczb;
- czas, koszt i ergonomia lokalnego owner flow.

### J. Automation i governance

- realna wartość recurring PLAN_ONLY;
- koszt Control Issue, lease i work-item queue;
- protected paths oraz risk classes;
- oddzielenie manualnego workflowu od wymagań schedulera;
- zasada, że governance nie może stać się głównym produktem repo.

## 7. Fazy reorganizacji

### F0 — milestone evidence seal

Status: rozpoczęty w tym branchu.

Rezultat:

- Project Charter;
- redacted `TERRAIN_VISIBLE_PASS` record;
- poprawiona granica skali;
- textured preview jako następny product gate.

### F1 — authority and documentation truth

- zsynchronizować memory/current state/operating plan;
- zaktualizować repository audit;
- rozdzielić trwałą wizję, mutable state, roadmapę i historię;
- nie publikować private evidence.

### F2 — forensic inventory

- utworzyć machine-readable inventory;
- sklasyfikować każdą domenę, dokument, branch, PR i tool family;
- wskazać elementy nieprzejrzane i capability unavailable;
- zero product implementation.

### F3 — integration decision

- owner review PR #13;
- wybrać merge, bounded correction albo preservation strategy;
- bez rebase, squash lub force-push wynikających tylko z estetyki;
- zaktualizować mutable authority dopiero po integracji.

### F4 — textured preview campaign brief

- odzyskać historyczne texture evidence;
- zaprojektować jawny textured-pack contract;
- ustalić 1K baseline, 2K A/B, screenshot matrix i memory budget;
- zdefiniować car scale reference scene;
- dopiero potem utworzyć implementacyjny branch.

### F5 — scale and drive-region proof

- załadować teksturowany teren;
- ustawić zaakceptowany samochód bez kolizji terenu jako referencję;
- owner potwierdza skalę i wybiera Golden Drive Region;
- zapisać fixed cameras i logical ROI, bez prywatnej lokalizacji.

### F6 — collision research and first drive

- porównać reprezentacje;
- wybrać najmniejszą uczciwą powierzchnię;
- static wheel/contact probe;
- vehicle drop;
- low-speed drive;
- owner fun verdict.

## 8. Co jest obecnie zakazane

Do czasu zakończenia F1–F4:

- brak kodu kolizji;
- brak promocji surface evidence do accepted world;
- brak merge starego PR #7;
- brak tekstur dopisywanych do zamkniętego manifestu preview v1;
- brak finalnego claimu skali;
- brak rozpoczęcia full-world streamingu;
- brak zmian accepted vehicle physics;
- brak zmian `src/` lub `include/`;
- brak merge/retarget/rebase/force-push bez osobnej decyzji właściciela.

## 9. Definition of Done dla re-foundation

Reorganizacja jest gotowa dopiero, gdy nowy agent może szybko i jednoznacznie
odpowiedzieć:

1. Czym jest produkt i jaka jest jego dusza?
2. Jaki milestone jest naprawdę zamknięty?
3. Co jest aktywne i jaki exact authority obowiązuje?
4. Co jest zaakceptowanym fundamentem?
5. Co jest eksperymentem, historią albo parkingiem?
6. Jakie są warstwy świata i ich authority?
7. Dlaczego tekstury poprzedzają finalną skalę i kolizję?
8. Jakie testy i owner gate'y dotyczą każdej domeny?
9. Co jest prywatne i czego nie wolno publikować?
10. Jaki jest jeden następny product gate?

## 10. Uczciwy stan tego dokumentu

Ten dokument rozpoczyna pełny audyt i zapisuje jego metodę. Nie twierdzi, że:

- każdy plik repo został już ręcznie przeczytany;
- każda historyczna gałąź została porównana line-by-line;
- strategia teksturowanego packa jest już wybrana;
- PR #13 jest gotowy do merge;
- collision representation została wybrana.

Każdy z tych punktów pozostaje jawnym zadaniem kolejnych faz, nie nieświadomym
„później”.
