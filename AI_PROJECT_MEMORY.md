# AI Project Memory — Box3d_FunProject

## Rola

Ten plik jest krótkim routerem bieżącego projektu. Wskazuje:

- aktywny mutable control plane;
- authoritative branch/PR;
- najwyższy uczciwy capability;
- najbliższy product gate;
- dokumenty, które należy przeczytać dalej.

Nie jest twardą polityką, historią milestone'ów, dokładnym current SHA, prywatnym
evidence store, work-item queue ani zgodą na implementację albo merge.

## Kolejność odczytu

Najpierw twarda polityka:

1. `AGENTS.md`;
2. `.automation/CONTROL.yaml`;
3. `.automation/POLICY.md`.

Następnie fakty i intencja:

1. GitHub Issue #11 `[AUTOMATION CONTROL] Box3d_FunProject recurring agent`;
2. ten plik;
3. właściwy `docs/*/CURRENT_STATE.md`;
4. aktywny PR i jego remote head;
5. `docs/PROJECT_OPERATING_PLAN_PL.md`;
6. `docs/PROJECT_CHARTER_PL.md`;
7. matching domain manual, checkpointy, tech debt, subsystem docs, kod i testy.

Exact mutable `authoritative_head` zawsze pochodzi z Issue #11. Historyczny SHA w
checkpointcie opisuje dowód, nie bieżącą authority.

## Trwała intencja produktu

Karta projektu:

```text
docs/PROJECT_CHARTER_PL.md
```

Najkrótsza dewiza:

> Drążymy skałę kropla po kropli — ale każda kropla ma zostawić działający,
> powtarzalny fundament.

Box3d_FunProject łączy własne pojazdy, uczciwą fizykę, narzędzia twórcy oraz prawdziwe
miejsca. Najpierw powstaje mały grywalny proof, dopiero potem produkcyjny system.

## Authority i bieżąca powierzchnia integracji

```text
campaign:             scan-terrain-r1b milestone closure / project re-foundation
goal:                 seal geometry preview proof and define textured-preview gate
state document:       docs/scan_import/CURRENT_STATE.md
authoritative branch: agent/scan-terrain-r1b-consolidated-integration
active draft PR:      #13
exact current head:   GitHub Control Issue #11
integration base:     jozz-vehicle-sandbox-m0
```

PR #13 pozostaje jedyną bieżącą powierzchnią integracji P0–R1B i nie jest automatycznie
powierzchnią następnej kampanii. Nie dopisuj do niego kolizji ani nieograniczonego
textured-world work.

PR #15 został owner-reviewed i zintegrowany do authoritative head PR #13. Starsze
opisy przedstawiające go jako niezintegrowany draft są historycznym driftem.

Historyczne PR-y #1–#5, #8 i #9 są zastąpione przez #13 z zachowaniem branchy i
commitów. Divergent surface-evidence line pozostaje zaparkowana w Issue #14 i nie
jest automatycznie reaktywowana.

## Najwyższy uczciwy stan produktu

```text
TERRAIN_VISIBLE_PASS
```

Publiczny redacted evidence record:

```text
docs/scan_import/TERRAIN_VISIBLE_PASS_2026_07_22_PL.md
```

Potwierdzone owner-locally na historycznej rewizji milestone'u:

- realny zestaw 7 GLB + 7 PLY został rozwiązany względem zweryfikowanego bundle'a;
- exact seven-tile preview pack został zbudowany i niezależnie zweryfikowany;
- native host załadował 7 kafli;
- owner rozpoznał rzeczywisty teren i zaakceptował geometry-only preview;
- same-revision restart ponownie załadował pack;
- restart przepracował 949 klatek bez błędów Sokol;
- artefakty obrzeży są świadomie zaakceptowanym ograniczeniem pierwszego źródła.

Prywatne ścieżki, lokalizacja, source hashes, receipts i raw scan data pozostają poza
Gitem, Issue, PR-em i publicznym CI.

## Ważna granica: skala nie jest jeszcze finalnie zwalidowana

```text
WORLD_SCALE_VALIDATED = false
```

Metre grid i spójny transform wspierają hipotezę skali, ale finalna walidacja wymaga
zaakceptowanego samochodu stojącego na drodze lub obok znanego domu/innego obiektu.

Nie promuj skali na podstawie geometry-only preview.

## Najbliższy product gate

```text
TEXTURED_SOURCE_PREVIEW
```

Obowiązująca kolejność:

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
→ OWNER_FUN_VERDICT
```

Tekstury są warunkiem świadomego rozpoznania drogi, pobocza, trawy, zabudowy i
właściwego ROI. Nie są kosmetycznym etapem po kolizji.

Historyczne eksperymenty wskazują profil 1K jako pierwszy rozsądny runtime baseline,
2K jako quality A/B i BC7 dopiero po działającym renderze. Te wnioski wymagają
ponownego audytu względem obecnych siedmiu źródeł i native preview v1.

## Current gates

```text
owner gate:    textured preview and scale-reference visual review
private gate:  no new private source discovery required for milestone closure
visual gate:   TEXTURED_SOURCE_PREVIEW_REQUIRED
physics gate:  COLLISION_BLOCKED_UNTIL_TEXTURE_AND_SCALE_REVIEW
```

Mutable Issue #11 musi zostać zsynchronizowany owner-directed po review tej zmiany.
Nie przesuwaj authority na niezreviewowany branch tylko po to, aby scheduler zobaczył
nowe dokumenty.

## Project re-foundation

Aktywny manualny zakres reorganizacji:

```text
docs/PROJECT_REFOUNDATION_AUDIT_2026_07_22_PL.md
```

Jego zadaniem jest:

- zapieczętować milestone;
- sklasyfikować każdą domenę, branch, PR, dokument i tool family;
- naprawić authority/documentation drift;
- oddzielić scan evidence, textured preview, authored world, render, collision,
  surface materials i gameplay semantics;
- przygotować mały brief następnej kampanii.

Audyt nie jest pozwoleniem na produktowy cleanup, merge starego PR #7 ani rozpoczęcie
kolizji.

## Inne domeny

### Vehicle sandbox

M7/M8 pozostaje zaakceptowanym stabilnym baseline'em. Accepted vehicle behavior nie
może zostać przypadkowo zmienione podczas integracji skanu. Manual:
`README_FOR_AGENTS.md`.

### Synthetic engineering world

Zaakceptowany Etap 1 pozostaje deterministycznym laboratorium fizyki. Odrzucony
6-lane layout i plan central-campus nie są automatycznie aktywne. Synthetic world nie
jest zastępowany przez skany.

### Surface evidence / collision

Issue #14 zachowuje dokładny parking state. Po `TERRAIN_VISIBLE_PASS` wolno go
przeanalizować, lecz nie wolno automatycznie merge'ować, rebase'ować ani promować do
accepted surface. Najpierw tekstury, scale reference i owner-selected drive region.

### JES

JES może później współdzielić world pipeline albo pozostać osobnym odbiorcą. Nie
sprzęgaj obecnego projektu z jego przyszłą architekturą przed realnym drive/authoring
proofem.

## Recurring operator

```text
control issue: #11
enabled:       read from Issue #11
mode:          PLAN_ONLY
active lease:  read from Issue #11
```

W `PLAN_ONLY` operator audytuje i raportuje. Nie tworzy product branch, commitu ani
PR-a, nie podnosi mode i nie modyfikuje własnego control plane.

## Hard boundaries

- `src/` i `include/` Box3D są poza zwykłym zakresem;
- visual, scale, feel, fun i accepted behavior wymagają owner evidence;
- raw scan i geometry preview nie są automatycznie collision truth;
- bez zgody ownera nie ma merge, auto-merge, force-push, rebase, retarget, zamykania
  PR-ów ani branch deletion;
- manualna praca startuje na nowym branchu z exact remote SHA;
- dokumentacja ma opisywać wyłącznie udowodniony stan.
