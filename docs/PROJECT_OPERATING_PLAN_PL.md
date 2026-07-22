# Project Operating Plan — Box3d_FunProject

**Status:** current operating map  
**Updated:** 2026-07-22  
**Scope:** project workflow, authority, roadmap and evidence gates  
**Product code changed by this document:** no

## 1. Purpose

This is the single project-wide operating plan. It connects:

- the active scan-terrain product campaign;
- product integration PR #13;
- repository-readiness PR #15;
- the recurring `PLAN_ONLY` operator;
- stable vehicle, paused map and parked surface-evidence domains.

It is not a work-item queue, implementation authority, visual/private approval or
permission to merge either PR.

## 2. Policy and authority

Hard policy is loaded first:

1. `AGENTS.md`;
2. `.automation/CONTROL.yaml`;
3. `.automation/POLICY.md`.

Mutable state and project facts are then resolved in this order:

1. GitHub Control Issue #11 — `enabled`, mode, campaign, exact branch/head, gate'y
   and lease;
2. `AI_PROJECT_MEMORY.md` — current project router;
3. matching `docs/*/CURRENT_STATE.md`;
4. active campaign PR and its remote head;
5. this operating plan;
6. matching domain manual, for example `README_FOR_AGENTS.md` for vehicle work;
7. checkpoints, tech debt, subsystem docs, code and tests.

A mutable Issue cannot legalize an operation forbidden by hard policy. Exact
current SHA belongs in Issue #11; a versioned document cannot safely claim its own
commit SHA as timelessly current.

## 3. Current operating snapshot

```text
repository:                   Jozzpoly/Box3d_FunProject
active campaign:              scan-terrain-r1b
authoritative branch:         agent/scan-terrain-r1b-consolidated-integration
exact head:                   GitHub Control Issue #11
active campaign PR:           #13 (draft)
repository readiness PR:      #15 (draft, stacked on #13)
integration base:             jozz-vehicle-sandbox-m0
control issue:                #11 (open)
automation enabled:           true
automation mode:              PLAN_ONLY
schedule:                     every 6 hours, Europe/Warsaw
surface parking issue:        #14
```

PR #13 remains product authority until PR #15 is separately reviewed and
integrated. Opening #15 does not move the scheduler, product state or evidence
gates to an unreviewed control-plane branch.

## 4. Honest product state

Highest supported state:

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

Confirmed:

- code contracts and native build are green on the consolidated campaign;
- real inspection, source frame and P1B evidence exist owner-locally;
- exact preview builder/verifier and resumable owner flow are implemented;
- product history is consolidated in one draft PR #13.

Still missing:

- a real preview pack from the owner's private seven-tile source set;
- proven native load of that exact pack;
- owner visual review of orientation, scale, axes, mirror state, coverage and seams;
- same-revision restart proof;
- `TERRAIN_VISIBLE_PASS`.

Current gates:

```text
private: OWNER_LOCAL_REAL_SCAN_RUN_REQUIRED
visual:  REAL_TERRAIN_PREVIEW_REVIEW_PENDING
owner:   explicit decision at visual review
```

CI, documentation completeness and repository polish do not clear these gates.

## 5. Workstream registry

### A. Scan terrain R1B — ACTIVE PRODUCT CAMPAIGN

**Goal:** produce and visually review the exact seven-tile native preview.

**Authority:** Issue #11, `docs/scan_import/CURRENT_STATE.md`, PR #13 and the exact
remote head declared in Issue #11.

**Next real action:** one owner-local execution of `run_real_terrain_flow.ps1`, then
native visual review.

### B. Repository readiness — UNDER REVIEW

PR #15 is an owner-directed governance package stacked on PR #13. It prepares:

- a truthful root README and contribution workflow;
- a documentation index and repository ownership map;
- unambiguous policy-vs-mutable-state routing;
- manual and automation PR templates;
- mechanical authority/workflow drift audit;
- canonical CI routing after closure of the historical stack.

It changes no product capability and remains non-authoritative until separately
reviewed and integrated.

### C. Recurring operator — ACTIVE, PLAN_ONLY

Every six hours it may audit authority, PRs, CI, gates, lease and queue state. It
may recommend one bounded next action. It may not create product branches,
commits, PRs or `AGENT_READY` items; it cannot merge or raise its own mode.

### D. Vehicle sandbox — STABLE BASELINE, NOT CURRENT CAMPAIGN

The accepted M7/M8 foundation remains valuable and must not be casually reworked.
Its domain manual is `README_FOR_AGENTS.md`.

### E. Map / central test campus — PAUSED

The rejected six-lane layout and central-campus redesign remain future campaign
material. Do not resume them while scan visual proof is the nearest product gate.

### F. Surface evidence / derivatives — PARKED

The exact parked branch/head and reactivation gates live in Issue #14. Closing PR
#7 did not reject or delete that work. Accepted surface, collision and drive
readiness remain separate owner-gated campaigns.

## 6. Integration topology

```text
main
└─ preserved upstream/history line

jozz-vehicle-sandbox-m0
└─ stable vehicle baseline
   └─ P0–R1B lineage
      └─ PR #13 consolidated product integration
         └─ PR #15 repository-readiness governance layer
```

Historical PRs #1–#5, #8 and #9 remain closed as superseded by #13. Their branches
and commits are preserved. The #7/#8 divergent surface branch remains parked in
Issue #14.

No current plan requires rebase, squash, force-push, retarget or branch deletion.

## 7. Critical-path roadmap

### Stage O0 — authority and stack cleanup — COMPLETE

- automation foundation integrated;
- PR #12 merged after safety CI;
- one consolidated product draft PR #13 created;
- old stack classified as superseded or parked;
- surface work preserved in Issue #14;
- no product merge or history rewrite.

### Stage O1 — durable repository readiness — UNDER REVIEW IN PR #15

Done in the proposed layer:

- global agent policy separated from vehicle-domain manual;
- root README and CONTRIBUTING made truthful for this fork;
- documentation and repository maps created;
- governance drift checks and adversarial tests added;
- CI routes updated to current/future campaign branches;
- obsolete duplicate P2A workflow removed.

Done only after review/integration:

- exact final CI is green;
- diff contains governance/docs/workflows only;
- Issue #11 is updated to the resulting integrated head;
- scheduler observes the new files without policy conflict.

O1 may be reviewed in parallel with preparing the private R1 run, but it must not
be mistaken for product progress.

### Stage R1 — owner-local preview activation — NEXT PRODUCT GATE

**Input:** existing verified private P1B bundle/receipt and seven GLB/PLY sources.

**Action:** run the supported owner entrypoint once. Owner supplies only the
necessary private source root; technical orchestration remains automated.

**Expected:** exact verified preview pack plus private resumable state.

**STOP:** source ambiguity, hash/revision mismatch, symlink, mutation, missing
receipt, private-data exposure or inferred approval.

### Stage R2 — native preview proof

Launch the exact selected pack in the native sample host.

Required evidence:

- selected pack identity;
- successful native load;
- visible seven-tile geometry.

Compilation, generated files or process exit alone are insufficient.

### Stage R3 — owner visual review and restart proof

Owner reviews:

- orientation and signed axes;
- scale/metres conversion;
- up axis and mirror state;
- coverage, tile placement and seams;
- same-revision restart.

Only explicit owner acceptance may produce `TERRAIN_VISIBLE_PASS`.

### Stage R4 — bounded decision after evidence

After R3, choose exactly one outcome:

```text
A. PASS
   → record TERRAIN_VISIBLE_PASS
   → review integration readiness of PR #13/#15

B. BOUNDED CORRECTION
   → create one isolated corrective PR
   → preserve exact failing evidence and retest

C. INVALIDATED ASSUMPTION
   → keep product PR draft
   → document the disproven assumption
   → re-plan before implementation
```

No autonomous merge or automatic reinterpretation of failure.

### Stage R5 — select one next product campaign

Candidates include:

- surface evidence;
- accepted-surface/collision research;
- central test campus;
- vehicle drivetrain/tire model;
- rig/editor tooling.

Do not start them in parallel merely because plans already exist.

## 8. Manual work workflow

1. Read hard policy, Issue #11, memory and matching current state.
2. Fetch and verify the exact remote head.
3. Inspect active PRs, CI, gates and overlapping paths.
4. Classify risk A0–A4.
5. Declare allowed/forbidden paths, acceptance criteria, tests and STOP conditions.
6. Create a new isolated branch from the exact SHA.
7. Implement one coherent scope.
8. Run narrow checks plus unchanged project gate where applicable.
9. Update state docs only when proven state moved.
10. Commit logically and open a draft PR using the manual template.
11. Stop before merge or owner acceptance.

Never write directly to `main`, `jozz-vehicle-sandbox-m0` or active campaign
branches. Never force-push or rewrite history without separate explicit approval.

## 9. Recurring PLAN_ONLY workflow

Each scheduled run should:

1. load `AGENTS.md`, `CONTROL.yaml` and policy;
2. validate repository/control contracts;
3. read exactly one Control Issue;
4. verify authoritative branch and exact remote head;
5. inspect active PR, CI, gates, lease and queue;
6. select zero implementation items in current `PLAN_ONLY` state;
7. report the exact material change or STOP;
8. make no product mutation;
9. confirm no merge, rebase, force-push or policy self-modification.

Preferred unchanged report:

```text
NO_MATERIAL_CHANGE
<current gate or NO_SAFE_WORK>
authority=<branch>@<sha>
```

## 10. Definition of Ready

Implementation is ready only when:

- owner selected the campaign;
- mode permits implementation;
- a pre-existing work item is owner-approved `AGENT_READY` where automation applies;
- risk class is eligible;
- exact base SHA is current;
- allowed and forbidden paths are explicit;
- acceptance criteria and tests are observable;
- owner, visual and private gates are resolved or outside scope;
- no conflicting lease, PR or path overlap exists.

Planning text alone is never implementation authority.

## 11. Definition of Done

A bounded change is done only when:

- acceptance criteria pass without relaxed thresholds;
- all available required tests pass;
- unavailable capability is explicitly listed;
- visual/private/owner evidence comes from the correct authority;
- private data stays private;
- docs claim only proven state;
- exact base/head and actual diff are recorded;
- no unauthorized merge, rebase, force-push, retarget or deletion occurred;
- unresolved gates remain explicit.

## 12. Owner decision queue

1. Review PR #15 as a governance-only layer; do not confuse it with product proof.
2. Execute the real private R1B runner when ready.
3. Perform native visual review and same-revision restart.
4. Decide PASS, bounded correction or invalidated assumption.
5. Decide integration order for #15 and #13 based on actual evidence.
6. Select exactly one next product campaign.

The recurring operator may surface these decisions but cannot answer them for the
owner.

## 13. Known operating risks

- **Authority drift:** exact SHA lives in Issue; mechanical repository audit checks
  the versioned routing around it.
- **Default-branch illusion:** GitHub `main` is not automatically the active project
  line; all work resolves Issue #11 first.
- **False green:** CI cannot inspect private assets, visual correctness or feel.
- **Governance self-reference:** PR #15 changes policy and therefore remains a
  manual A3 review, never recurring-agent self-modification.
- **Historical opacity:** closed PRs are evidence lineage, not task queues.
- **Parallelism pressure:** vehicle/map/surface work before R1B proof weakens causal
  validation.
- **Scheduler overlap:** external scheduling has no atomic GitHub lock; repository
  lease stays fail-closed.
- **Granular API commits:** contents API creates small sequential commits; preserve
  them rather than rewriting shared history merely for aesthetics.

## 14. Maintenance rule

Update this document only when active campaign, authority strategy, evidence
boundary, roadmap stage, repository topology or project-wide workflow changes.
Routine CI reports and unchanged gates do not belong in Git.
