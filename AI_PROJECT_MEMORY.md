# AI Project Memory — Box3d_FunProject

## Rola

Ten plik jest krótkim mutable routerem projektu. Nie jest twardą polityką, prywatnym
evidence store, pełną historią ani zgodą na implementację lub merge.

## Kolejność odczytu

1. `AGENTS.md`;
2. `.automation/CONTROL.yaml`;
3. `.automation/POLICY.md`;
4. GitHub Issue #11;
5. ten plik;
6. matching `docs/*/CURRENT_STATE.md`;
7. aktywny draft PR i exact remote head;
8. `docs/PROJECT_OPERATING_PLAN_PL.md`;
9. `docs/PROJECT_CHARTER_PL.md`;
10. odpowiednie kontrakty, tech debt, kod i testy.

Exact mutable SHA zawsze pochodzi z Issue #11. Historyczne SHA opisują dowody, nie
bieżącą authority.

## Trwała intencja

> Drążymy skałę kropla po kropli — ale każda kropla ma zostawić działający,
> powtarzalny fundament.

Box3d_FunProject łączy własne pojazdy, uczciwą fizykę, creator tooling i prawdziwe
miejsca. Najpierw mały grywalny proof, dopiero później produkcyjny system.

Charter:

```text
docs/PROJECT_CHARTER_PL.md
```

## Bieżąca authority

```text
campaign:             project integration / textured-preview readiness
state document:       docs/scan_import/CURRENT_STATE.md
authoritative branch: agent/project-refoundation-audit-v1
active draft PR:      #17
exact current head:   GitHub Control Issue #11
integration base:     jozz-vehicle-sandbox-m0
branches on remote:   3
```

PR #17 zastępuje zamknięte, niezmergowane powierzchnie review #13 i #16. Cała ich
zaakceptowana zawartość i liniowa historia pozostają osiągalne z obecnego brancha.

`main` zachowuje upstream/history baseline. `jozz-vehicle-sandbox-m0` zachowuje
zaakceptowany vehicle/synthetic-world baseline. Nie usuwaj żadnego z trzech branchy.

## Branch cleanup

```text
branch cleanup:        COMPLETE_TO_3
further deletion:      FORBIDDEN
retention tag debt:    3 missing tags
```

Exact plan:

```text
docs/BRANCH_RETENTION_PLAN_2026_07_22.json
```

Divergent commity PR #7 i #8 pozostają odczytywalne po exact SHA. Brak tagów jest
jawnym długiem retencyjnym, a nie utratą pracy i nie pełnym PASS-em.

## Najwyższy uczciwy capability

```text
TERRAIN_VISIBLE_PASS
```

Owner-local proof potwierdził:

- exact 7 GLB + 7 PLY source resolution;
- deterministic seven-tile pack;
- niezależną weryfikację;
- native first load i owner recognition;
- same-revision restart;
- 949 klatek, 0 błędów Sokol.

Redacted milestone:

```text
docs/scan_import/TERRAIN_VISIBLE_PASS_2026_07_22_PL.md
```

## Granica skali

```text
WORLD_SCALE_VALIDATED = false
```

Metre grid wspiera hipotezę skali, ale finalny werdykt wymaga zaakceptowanego samochodu
na rozpoznawalnej drodze albo obok znanego domu/obiektu.

## Najbliższa krytyczna ścieżka

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
→ OWNER_FUN_VERDICT
```

Tekstury nie są kosmetyką po kolizji. Są warunkiem rozpoznania terenu, walidacji skali
i świadomego wyboru pierwszego regionu jazdy.

## Current gates

```text
visual gate:   TEXTURED_SOURCE_PREVIEW_REQUIRED
owner gate:    TEXTURE_AND_SCALE_REFERENCE_REVIEW_REQUIRED
private gate:  NO_NEW_PRIVATE_SOURCE_DISCOVERY_REQUIRED
physics gate:  COLLISION_BLOCKED_UNTIL_TEXTURE_SCALE_AND_ROI
merge gate:    OWNER_REVIEW_REQUIRED
```

## Domeny do zachowania

- vehicle M7/M8: accepted behavior, bez przypadkowych zmian;
- synthetic engineering world: deterministyczne laboratorium, nie zastąpione skanem;
- geometry preview v1: zamknięty geometry-only contract;
- surface/collision: zaparkowane evidence, brak authority;
- JES: osobna granica do przyszłej jawnej adopcji.

## Recurring operator

```text
control issue: #11
mode:          PLAN_ONLY
```

Operator audytuje i raportuje. Nie tworzy product branchy, nie implementuje, nie
podnosi mode, nie zmienia własnej polityki i nie wykonuje merge’a.

## Hard boundaries

- `src/` i `include/` poza zwykłym zakresem;
- visual, scale, feel, fun i accepted behavior wymagają owner evidence;
- raw scan/preview nie są collision truth;
- brak merge, force-push, rebase, retarget i dalszego branch deletion bez jawnej zgody;
- prywatne ścieżki, lokalizacja, source hashes i raw assets pozostają poza GitHubem.
