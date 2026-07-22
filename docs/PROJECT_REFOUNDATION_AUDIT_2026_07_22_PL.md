# Project Re-foundation Audit — wynik F2 i wejście w F3

**Updated:** 2026-07-22  
**Branch:** `agent/project-refoundation-audit-v1`  
**Class:** manual owner-directed A3 re-foundation  
**Status:** `F2_FORENSIC_INVENTORY_COMPLETE / F3_INTEGRATION_REVIEW_ACTIVE`

## Dlaczego ten audyt powstał

Projekt zakończył pierwszy realny milestone skanu: prywatny zestaw siedmiu kafli został
zweryfikowany, pokazany w natywnym hoście, zaakceptowany przez właściciela i ponownie
załadowany na tej samej rewizji.

Jednocześnie w jednym repo żyją:

- upstream Box3D;
- zaakceptowany vehicle sandbox;
- syntetyczny engineering world;
- scan evidence i preview pipeline;
- eksperymenty tekstur oraz surface/collision;
- creator tooling;
- dokumentacja, CI i recurring governance.

Audyt miał zapobiec rozpoczęciu następnego dużego feature'u na rozdrobnionym
fundamencie.

## Trwałe ustalenia

### Dusza projektu

- uczciwa, emergentna fizyka pojazdu;
- własne modele i creator control;
- małe grywalne proofy przed produkcyjnymi systemami;
- owner visual/feel/fun jako realne gate'y;
- synthetic world jako deterministyczne laboratorium;
- real scan jako autentyczny świat, nie zamiennik fizyki pojazdu.

Charter:

```text
docs/PROJECT_CHARTER_PL.md
```

### Obowiązująca kolejność produktu

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
→ OWNER_FUN_VERDICT
```

Tekstury są warunkiem rozpoznania miejsca, finalnej walidacji skali i wyboru drogi.
Nie są kosmetyką po kolizji.

### Warstwy prawdy świata

```text
PRIVATE_SOURCE_EVIDENCE
→ SOURCE_VISUAL_PREVIEW
→ AUTHORED_WORLD_ASSETS
→ RENDER_DERIVATIVES
→ PHYSICS_SURFACE
→ SURFACE_MATERIAL_MAP
→ GAMEPLAY_SEMANTICS
```

Żadna warstwa nie otrzymuje authority tylko dlatego, że poprzednia istnieje.

## Wynik forensic inventory

Sklasyfikowano:

- wszystkie główne domeny repo;
- 25 project-authored active/non-archive documents;
- lineage PR #1–#17;
- branch topology i historyczne exact commits;
- current authority/front doors;
- scan, vehicle, map, tooling, CI, automation, privacy i JES boundary.

Machine-readable records:

```text
docs/PROJECT_INVENTORY.json
docs/DOCUMENT_LIFECYCLE_2026_07_22.json
docs/BRANCH_RETENTION_PLAN_2026_07_22.json
```

Żaden z 25 historycznych planów nie może sam aktywować pracy.

## Naprawione konflikty

- `TECH_DEBT_PL.md` nie instruuje już do direct pushu na vehicle baseline;
- `README_FOR_AGENTS.md` jest vehicle-domain manualem, nie globalną polityką;
- martwy P1B tutorial nie jest current start page;
- upstreamowy default PR template nie blokuje forkowego workflow;
- dawne active tracks stabilizacji, refactoru, finalizacji i rig-editor warm-up mają
  jawny lifecycle;
- current hotkey reference został odtworzony z kodu;
- geometry preview v1 pozostaje jawnie bez tekstur i bez collision authority.

## Branch cleanup — rzeczywisty wynik

Remote zawiera obecnie dokładnie:

```text
main
jozz-vehicle-sandbox-m0
agent/project-refoundation-audit-v1
```

```text
branch cleanup:       COMPLETE_TO_3
hard maximum:         5
further deletion:     FORBIDDEN
retention tag debt:   3
```

Cleanup zamknął bez merge'a PR #13 i #16, ponieważ ich dawna base/head topology
zniknęła. Ich zawartość pozostała w obecnym liniowym branchu.

Trzy zaplanowane tagi nie zostały utworzone przed cleanupem. Exact target commits są
nadal odczytywalne, dlatego praca nie została utracona, ale `TAG_RETENTION_COMPLETE`
nie może zostać przyznany.

## Aktualna integracja

```text
authoritative branch: agent/project-refoundation-audit-v1
active draft PR:      #17
integration base:     jozz-vehicle-sandbox-m0
```

PR #17 jest jedyną bieżącą powierzchnią review. Nie jest zgodą na merge.

## Stan produktu

```text
highest capability:       TERRAIN_VISIBLE_PASS
WORLD_SCALE_VALIDATED:    false
next visual gate:         TEXTURED_SOURCE_PREVIEW
collision:                blocked
```

Znane artefakty obrzeży pierwszego skanu są zaakceptowane dla geometry proofu i nie
muszą zostać naprawione przed jednym ograniczonym drive proofem.

## Zachowane, lecz zaparkowane evidence

- former PR #7 exact surface-evidence commit pozostaje recoverable;
- former PR #8 exact owner-flow commit pozostaje recoverable;
- ground filters, DEM/heightfield i kinematic drive probes są hypothesis/evidence,
  nie accepted surface;
- synthetic world oraz accepted M7/M8 vehicle pozostają foundation.

## Co F2 świadomie nie zrobiło

- nie wdrożyło tekstur;
- nie zwalidowało finalnej skali;
- nie wybrało Golden Drive Region;
- nie promowało surface/collision;
- nie zmieniło accepted vehicle physics;
- nie wykonało merge'a;
- nie utworzyło brakujących tagów bez odpowiedniego narzędzia Git.

## Aktywny etap F3

F3 kończy się dopiero, gdy:

1. PR #17 ma zielone exact-head governance, scan i Windows/native CI;
2. Issue #11 wskazuje istniejący branch i exact head;
3. żaden current document nie wskazuje PR #13/#16 jako aktywnych;
4. branch count pozostaje równy 3;
5. retention tag debt pozostaje jawny;
6. owner wybiera integracyjny outcome PR #17.

## Następny etap T0

Po F3 powstaje bounded `TEXTURED_SOURCE_PREVIEW` brief. Musi określić:

- source primitive/material/UV/image identity;
- explicit adjacent pack/version;
- 1K baseline i optional 2K A/B;
- decoded memory/mip budget;
- colour-space i sampler policy;
- deterministic builder i independent verifier;
- no-texture fallback;
- fixed-camera evidence;
- same-revision restart;
- owner visual acceptance.

Dopiero po texture acceptance można przygotować samochód jako wzorzec skali.
