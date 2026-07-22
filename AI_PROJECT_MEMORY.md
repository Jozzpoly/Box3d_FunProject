# AI Project Memory — Box3d_FunProject

## Rola tego pliku

Ten plik jest globalnym routerem aktualnego stanu projektu. Wskazuje aktywną
kampanię, jej domenowy `CURRENT_STATE.md`, dokładny branch/head, bieżące bramki i
najbliższą granicę produktu.

Nie jest:

- szczegółową dokumentacją subsystemu;
- historią wszystkich milestone'ów;
- kolejką pracy;
- zgodą na implementację albo merge.

## Kolejność autorytetu

1. GitHub Issue #11 `[AUTOMATION CONTROL] Box3d_FunProject recurring agent`;
2. `AGENTS.md` i `.automation/CONTROL.yaml`;
3. ten plik;
4. właściwy `docs/*/CURRENT_STATE.md`;
5. aktywny PR kampanii i jego exact remote head;
6. `docs/PROJECT_OPERATING_PLAN_PL.md`;
7. `README_FOR_AGENTS.md` jako podręcznik domeny pojazdu;
8. checkpointy, tech debt, subsystem docs, kod i testy.

Historyczny checkpoint, stary roadmap ani otwarty stacked PR nie wybiera pracy
samodzielnie.

## Aktywna kampania produktu

```text
campaign:             scan-terrain-r1b
goal:                 real seven-tile native render-only preview
state document:       docs/scan_import/CURRENT_STATE.md
active branch:        agent/r1b-source-resolution-owner-integration
active draft PR:      #9
authoritative head:   61238a842ff09be70dec821ef00c05b8e76d2718
campaign base:        PR #5 head f20357ba10618ddecfdd2e274e93917fe508a983
```

PR #9 remains the active integration campaign. PR #7 surface-evidence work is
frozen outside the nearest product goal. Older PRs #1–#8 preserve the stacked
history and are not independent task selectors.

## Aktualny uczciwy stan produktu

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

Confirmed:

- real 7 GLB + 7 PLY inspection passed in owner-private evidence;
- source-frame owner contract passed privately;
- P1B bundle/privacy receipt passed privately;
- R1B source resolution and owner-flow code passed hosted contracts;
- native sample build passed on the integrated head;
- automation foundation is integrated without changing product behavior.

Not yet proven:

- real preview pack creation from the private source set;
- native runtime load of that exact pack;
- owner visual review;
- same-revision restart;
- `TERRAIN_VISIBLE_PASS`.

Private paths, coordinates, hashes and raw scan data remain outside GitHub and
public logs.

## Najbliższa realna granica produktu

The next real action is one owner-local invocation of the supported runner:

```text
run_real_terrain_flow.ps1
```

The runner must discover the exact historical private bundle/receipt, resolve the
seven GLB/PLY source pairs, build and verify the preview pack, select it and stop
at visual review.

The owner should only provide the unavoidable private source root and perform the
actual visual decision. The owner is not a manual technical orchestrator.

Expected intermediate state:

```text
REAL_PREVIEW_PACK_READY / VISUAL_REVIEW_PENDING
```

Only an explicit review of orientation, scale, axes, mirror state, coverage,
seams and same-revision restart may produce:

```text
TERRAIN_VISIBLE_PASS
```

CI, compilation, generated files or a running process are not visual proof.

## Current gates

```text
owner gate:    NONE before the private run; owner decision required at visual review
private gate:  OWNER_LOCAL_REAL_SCAN_RUN_REQUIRED
visual gate:   REAL_TERRAIN_PREVIEW_REVIEW_PENDING
```

Do not clear or reinterpret these gates to create activity.

## Recurring operator

The automation foundation from PR #10 is merged into the active campaign head:

```text
merge commit: 61238a842ff09be70dec821ef00c05b8e76d2718
control issue: #11
enabled:       true
mode:          PLAN_ONLY
cadence:       every 6 hours, Europe/Warsaw
active lease:  none at the time of this update
```

In `PLAN_ONLY`, the scheduled operator may audit authority, PRs, CI, gates, lease
and the safe queue. It may plan or recommend a bounded next action, but it may not
create product branches, commits or PRs and may not promote itself to
`IMPLEMENT_SAFE`.

Detailed project workflow and roadmap:

```text
docs/PROJECT_OPERATING_PLAN_PL.md
```

## Other project domains

### Vehicle sandbox

The M7/M8 vehicle foundation remains an accepted/stable domain baseline, not the
current campaign. Its manual is `README_FOR_AGENTS.md`; detailed historical state
is in `docs/CURRENT_STATE_INDEX_PL.md` and subsystem documents.

Do not restart drivetrain, tire, steering-geometry or editor work merely because
it appears in an old roadmap.

### Map

The six-lane Etap 2 layout was rejected. The documented central-campus redesign
is paused until the owner selects map work as the active campaign.

### Surface evidence / collision

Accepted surface, collision and drive readiness are outside R1B and remain
blocked by missing real visual/evidence boundaries.

## Hard capability boundaries

- `src/` and `include/` Box3D are outside autonomous scope;
- accepted physics, feel, UX and defaults require owner decisions;
- A3/A4 are never autonomous;
- visual and private evidence cannot be synthesized from CI;
- no merge, auto-merge, force-push, rebase, retarget or PR closure without owner
  approval;
- never write directly to `main`, `jozz-vehicle-sandbox-m0` or an active campaign
  branch;
- never use `Git_Diff_Patcher_Bridge`;
- recurring runs cannot modify their own control plane.

## Branch and handoff rule

Manual agent work starts from the exact remote head on a new isolated branch,
uses small logical commits and ends in a draft PR. Documentation is updated only
when proven state moves or when a real authority conflict is repaired.

After any owner-approved integration, Issue #11 must be updated to the resulting
current branch and full SHA before the scheduler can treat the new state as
authoritative.
