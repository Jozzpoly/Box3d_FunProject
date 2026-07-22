# Project Operating Plan — Box3d_FunProject

**Status:** current operating map  
**Updated:** 2026-07-22  
**Scope:** project workflow, authority, roadmap and evidence gates  
**Product code changed by this document:** no

## 1. Purpose

This document is the project-level operating plan. It connects the currently
active product campaign, the recurring `PLAN_ONLY` operator, the stacked pull
requests and the older vehicle/map documentation without turning historical
plans into active work.

It is not:

- a replacement for GitHub Control Issue #11;
- a work-item queue;
- permission to implement product changes;
- visual, owner or private-data approval;
- permission to merge any pull request.

## 2. Authority order

Use the following order when facts conflict:

1. GitHub Issue #11 — mutable enable/mode/campaign/branch/head/gates/lease;
2. `AGENTS.md` and `.automation/CONTROL.yaml` — hard operating policy;
3. `AI_PROJECT_MEMORY.md` — current project router;
4. the matching domain `docs/*/CURRENT_STATE.md`;
5. the active campaign PR and its exact remote head;
6. this operating plan;
7. `README_FOR_AGENTS.md` — vehicle-domain rules and accepted foundations;
8. checkpoint, tech-debt and subsystem documentation;
9. code and tests for the selected scope.

A historical checkpoint, old roadmap or open stacked PR never selects work by
itself.

## 3. Current authoritative snapshot

At the time of this update:

```text
repository:             Jozzpoly/Box3d_FunProject
active campaign:        scan-terrain-r1b
authoritative branch:   agent/r1b-source-resolution-owner-integration
authoritative head:     61238a842ff09be70dec821ef00c05b8e76d2718
active campaign PR:     #9 (draft, open)
automation foundation:  PR #10 merged at 61238a842ff09be70dec821ef00c05b8e76d2718
control issue:          #11 (open)
automation enabled:     true
automation mode:        PLAN_ONLY
schedule:               every 6 hours, Europe/Warsaw
active lease:           none
```

The active campaign head includes the automation foundation but does not move
the product beyond the existing R1B capability boundary.

## 4. Honest product state

The highest currently supported product status is:

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

Code, hosted contracts and native compilation are green. The following evidence
still does not exist publicly or automatically:

- a real preview pack built from the owner's private seven-tile source set;
- a proven native load of that exact pack;
- owner visual review of orientation, scale, up axis, mirror state, coverage and
  seams;
- same-revision restart proof;
- `TERRAIN_VISIBLE_PASS`.

Active project gates remain:

```text
private: OWNER_LOCAL_REAL_SCAN_RUN_REQUIRED
visual:  REAL_TERRAIN_PREVIEW_REVIEW_PENDING
owner:   no unresolved product-choice gate before the private run
```

CI must never be promoted into private, visual or owner evidence.

## 5. Workstream registry

### A. Scan terrain R1B — ACTIVE

**Goal:** produce and visually review the exact seven-tile native preview.

**Authority:**

- `docs/scan_import/CURRENT_STATE.md`;
- PR #9;
- exact branch/head from Issue #11.

**Next real action:** one owner-local execution of the supported runner against
private data, followed by native visual review.

### B. Recurring project operator — ACTIVE, PLAN_ONLY

**Goal:** every six hours audit authority, PRs, CI, gates, lease and safe work
availability.

**Allowed now:** read, compare, diagnose, propose a bounded next action and
produce a compact report.

**Not allowed now:** product branch, product commit, product PR, queue promotion,
`AGENT_READY`, `IMPLEMENT_SAFE`, merge or policy self-modification.

### C. Vehicle sandbox — STABLE BASELINE, NOT CURRENT CAMPAIGN

The accepted M7/M8 vehicle foundation remains valuable and must not be casually
reworked. Its domain manual is `README_FOR_AGENTS.md`; detailed history remains
in `docs/CURRENT_STATE_INDEX_PL.md` and subsystem documents.

Vehicle roadmap items are candidates, not active work, until the owner selects a
new campaign.

### D. Map / central test campus — PAUSED

The old six-lane Etap 2 layout was rejected. The central-campus redesign remains
a documented future campaign. Do not resume it while the scan visual gate is the
nearest product boundary unless the owner explicitly changes the campaign.

### E. Surface evidence / derivatives — FROZEN

PR #7 remains outside the nearest product goal. Accepted surface, collision and
drive readiness require evidence that does not yet exist.

## 6. Critical-path roadmap

### Stage O0 — authority and documentation alignment

**Purpose:** remove contradictions between the merged automation foundation,
Issue #11 and repository documentation.

**Done when:**

- global memory points to the merged head;
- the scan current-state document distinguishes functional and integrated heads;
- vehicle documentation no longer claims to be the global front door;
- the operating plan is committed on an isolated branch and reviewed.

### Stage R1 — owner-local preview activation

**Input:** existing verified private P1B bundle/receipt and seven GLB/PLY sources.

**Action:** run the supported owner entrypoint once. The owner supplies only the
necessary private source root; technical orchestration remains automated.

**Expected output:** exact verified preview pack plus private resumable state.

**STOP conditions:** source ambiguity, hash/revision mismatch, symlink, mutation,
missing receipt, private-data exposure or any attempt to infer approval.

### Stage R2 — native preview proof

**Action:** launch the exact selected preview pack in the native sample host.

**Required evidence:** exact pack identity, successful native load and visible
seven-tile geometry.

**Not sufficient:** compilation, process exit code or generated pack alone.

### Stage R3 — owner visual review and restart proof

The owner reviews:

- orientation and signed axes;
- scale and metres conversion;
- up axis and mirror state;
- coverage, tile placement and seams;
- same-revision restart.

Only explicit owner acceptance may produce `TERRAIN_VISIBLE_PASS`.

### Stage R4 — stack decision

After R3, the owner decides how to promote or consolidate the stacked PR chain.
No agent may merge, retarget, close or rewrite #1–#9 without explicit approval.
The decision should preserve evidence boundaries and avoid treating frozen PR #7
as part of the visual-preview critical path.

### Stage R5 — next campaign selection

Only after the scan campaign reaches a truthful boundary should the owner choose
one next campaign, for example:

- accepted-surface evidence;
- collision/drive-readiness research;
- central test campus;
- vehicle drivetrain or tire model;
- rig/editor tooling.

Do not run these in parallel merely because they are documented.

## 7. Manual work workflow

Owner-directed manual work follows this sequence:

1. reread Issue #11, `AGENTS.md`, memory and matching current state;
2. fetch and verify the exact remote head;
3. classify the task and its owner/visual/private gates;
4. create a new isolated branch from the exact base;
5. implement one coherent scope;
6. run the narrow checks plus the unchanged project gate where applicable;
7. update current-state documentation only if state actually moved;
8. commit in small logical units;
9. push and open a draft PR;
10. stop before merge or owner acceptance.

Do not push directly to `main`, `jozz-vehicle-sandbox-m0` or the active campaign
branch. Do not force-push or rewrite history.

## 8. Recurring `PLAN_ONLY` workflow

Each scheduled run should:

1. read Issue #11 before checkout;
2. verify issue title, schema, uniqueness, enabled state and mode;
3. verify authoritative branch and exact remote head;
4. use detached HEAD for bootstrap;
5. validate the repository control contract and read-only preflight;
6. inspect active PRs, CI, gates, lease and queue;
7. select zero work items in the current empty/gated state;
8. report the exact gate or material change;
9. make no repository or GitHub mutation except policy-permitted lease/report
   handling;
10. end with explicit confirmation of no merge, force-push, rebase or policy
   modification.

If nothing material changed, the preferred report is:

```text
NO_MATERIAL_CHANGE
<current gate or NO_SAFE_WORK>
authority=<branch>@<sha>
```

## 9. Definition of Ready

A future implementation item is ready only when all of the following are true:

- the owner explicitly selected the campaign;
- the control mode permits implementation;
- a pre-existing work item is owner-approved `AGENT_READY`;
- risk class is eligible;
- exact base SHA is current;
- allowed and forbidden paths are explicit;
- acceptance criteria are observable;
- required tests are named;
- owner, visual and private gates are resolved or explicitly outside scope;
- no conflicting active lease or automation PR exists.

Planning text alone is never implementation authority.

## 10. Definition of Done

A bounded change is done only when:

- its acceptance criteria are met without relaxing thresholds;
- relevant tests pass and their limitations are stated;
- visual work has visual evidence reviewed by the correct authority;
- private evidence stays private;
- docs reflect only proven state;
- branch and draft PR identify the exact base and resulting head;
- no merge occurred without owner approval;
- unresolved gates are listed as unresolved.

## 11. Decision queue for the owner

The current decisions are deliberately small:

1. Execute the real private R1B runner when ready.
2. Perform the native visual review on the exact generated pack.
3. Decide whether the observed result is `TERRAIN_VISIBLE_PASS`, needs a bounded
   correction, or invalidates an assumption.
4. Decide how to integrate/retire the stacked PR chain after evidence exists.
5. Select exactly one next product campaign.

The recurring operator may surface these decisions but must not answer them for
the owner.

## 12. Known operating risks

- **Authority drift:** branch heads can move while docs retain old SHAs.
- **Stack opacity:** many open draft PRs make historical and active work look equal.
- **False green:** CI cannot inspect private assets or visual correctness.
- **Documentation inversion:** old vehicle docs may appear newer or more complete
  than the active scan campaign.
- **Parallelism pressure:** starting map/vehicle/surface work before R1B evidence
  increases unfinished work and makes causal validation weaker.
- **Scheduler overlap:** the platform has no explicit maximum-concurrency setting;
  the repository lease remains the fail-closed safeguard.

## 13. Maintenance rule

Update this document only when one of these changes:

- active campaign;
- authoritative branch/head strategy;
- evidence boundary;
- roadmap stage;
- project-wide workflow.

Routine reports, unchanged gates and periodic audit output do not belong in Git.
