# Project Operating Plan — Box3d_FunProject

**Status:** current operating map  
**Updated:** 2026-07-22  
**Scope:** project workflow, authority, roadmap and evidence gates  
**Product code changed by this document:** no

## 1. Purpose

This document connects the active product campaign, the recurring `PLAN_ONLY`
operator, the consolidated integration PR and the older vehicle/map documentation
without turning historical plans into active work.

It is not:

- a replacement for GitHub Control Issue #11;
- a work-item queue;
- permission to implement product changes;
- visual, owner or private-data approval;
- permission to merge PR #13.

## 2. Authority order

When facts conflict, use:

1. GitHub Issue #11 — mutable enable/mode/campaign/branch/head/gates/lease;
2. `AGENTS.md` and `.automation/CONTROL.yaml` — hard policy;
3. `AI_PROJECT_MEMORY.md` — current project router;
4. matching `docs/*/CURRENT_STATE.md`;
5. active campaign PR and its remote head;
6. this operating plan;
7. `README_FOR_AGENTS.md` — vehicle-domain rules;
8. checkpoints, tech debt and subsystem docs;
9. code and tests for the selected scope.

Exact current SHA belongs in Issue #11. A versioned document cannot safely claim
its own commit SHA as permanently current.

## 3. Current operating snapshot

```text
repository:             Jozzpoly/Box3d_FunProject
active campaign:        scan-terrain-r1b
authoritative branch:   agent/scan-terrain-r1b-consolidated-integration
exact head:             GitHub Control Issue #11
active campaign PR:     #13 (draft)
integration base:       jozz-vehicle-sandbox-m0
control issue:          #11 (open)
automation enabled:     true
automation mode:        PLAN_ONLY
schedule:               every 6 hours, Europe/Warsaw
surface parking issue:  #14
```

The consolidation branch is linearly ahead of `jozz-vehicle-sandbox-m0` with no
behind commits at creation. It was built from the owner-approved merge of PR #12
and uses no rebase, squash, force-push or history rewrite.

## 4. Honest product state

Highest supported state:

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

Code, hosted contracts and native compilation are green. Still missing:

- real preview pack from the owner's private seven-tile source set;
- proven native load of that exact pack;
- owner visual review of orientation, scale, up axis, mirror state, coverage and
  seams;
- same-revision restart proof;
- `TERRAIN_VISIBLE_PASS`.

Current gates:

```text
private: OWNER_LOCAL_REAL_SCAN_RUN_REQUIRED
visual:  REAL_TERRAIN_PREVIEW_REVIEW_PENDING
owner:   explicit decision at visual review
```

CI must never be promoted into private, visual or owner evidence.

## 5. Workstream registry

### A. Scan terrain R1B — ACTIVE

**Goal:** produce and visually review the exact seven-tile native preview.

**Authority:**

- Issue #11;
- `docs/scan_import/CURRENT_STATE.md`;
- PR #13;
- exact remote head of the branch declared in Issue #11.

**Next real action:** one owner-local execution of the supported runner, then
native visual review.

### B. Recurring project operator — ACTIVE, PLAN_ONLY

Every six hours it may audit authority, PRs, CI, gates, lease and queue state.
It may propose one bounded next action. It may not create product branches,
commits, PRs, `AGENT_READY` items, merge anything or raise its own mode.

### C. Vehicle sandbox — STABLE BASELINE, NOT CURRENT CAMPAIGN

The accepted M7/M8 foundation remains valuable and must not be casually reworked.
Its domain manual is `README_FOR_AGENTS.md`.

### D. Map / central test campus — PAUSED

The rejected six-lane layout and central-campus redesign are future campaign
material. Do not resume them while scan visual proof is the nearest boundary.

### E. Surface evidence / derivatives — PARKED

The exact parked branch/head and reactivation gates live in Issue #14. Closing PR
#7 is not design rejection and does not delete its branch or commits.

Accepted surface, collision and drive readiness remain separate owner-gated work.

## 6. Stack topology and cleanup result

The current integration line is:

```text
jozz-vehicle-sandbox-m0
→ photogrammetry/import-v2-foundation
→ P1 inspection
→ P1B contracts
→ P1B bundles
→ owner gate hardening
→ P2A exact preview
→ R1B source resolution / owner flow
→ recurring-agent safety foundation
→ project operating documentation
→ PR #13 consolidated integration
```

Historical PRs #1–#5, #8 and #9 are superseded by PR #13 and may remain closed.
Their branches and commit history are preserved.

PR #7/#8 formed a divergent branch after P2A. The final owner-flow production file
and its test from #8 are byte-identical to the versions on PR #13. Surface-evidence
work remains only on the parked #7 branch and Issue #14.

## 7. Critical-path roadmap

### Stage O0 — authority and stack cleanup — COMPLETE

Completed:

- automation foundation integrated;
- project authority documentation aligned;
- PR #12 merged after 6/6 safety CI;
- one consolidated draft PR #13 created;
- surface branch recorded in Issue #14;
- old stack classified as superseded or parked;
- no product merge, history rewrite or branch deletion.

### Stage R1 — owner-local preview activation — NEXT

**Input:** existing verified private P1B bundle/receipt and seven GLB/PLY sources.

**Action:** run the supported owner entrypoint once. Owner supplies only the
necessary private source root; technical orchestration remains automated.

**Expected output:** exact verified preview pack plus private resumable state.

**STOP:** source ambiguity, hash/revision mismatch, symlink, mutation, missing
receipt, private-data exposure or any inferred approval.

### Stage R2 — native preview proof

Launch the exact selected pack in the native sample host.

Required evidence: exact pack identity, successful native load and visible
seven-tile geometry. Compilation or process exit alone is insufficient.

### Stage R3 — owner visual review and restart proof

Owner reviews:

- orientation and signed axes;
- scale and metres conversion;
- up axis and mirror state;
- coverage, tile placement and seams;
- same-revision restart.

Only explicit acceptance may produce `TERRAIN_VISIBLE_PASS`.

### Stage R4 — integration decision

After R3, owner decides whether PR #13:

- is ready for promotion;
- needs one bounded corrective PR;
- exposed an invalid assumption and remains blocked.

No autonomous merge or auto-merge.

### Stage R5 — select exactly one next campaign

Candidates include surface evidence, collision research, central test campus,
vehicle drivetrain/tire model or rig/editor tooling. Do not start them in parallel
merely because they are documented.

## 8. Manual work workflow

1. Read Issue #11, `AGENTS.md`, memory and matching current state.
2. Fetch and verify exact remote head.
3. Classify risk and owner/visual/private gates.
4. Create a new isolated branch from the exact base.
5. Implement one coherent scope.
6. Run narrow checks plus unchanged project gate where applicable.
7. Update state docs only when state truly moved.
8. Commit in small logical units.
9. Push and open a draft PR.
10. Stop before merge or owner acceptance.

Never push directly to `main`, `jozz-vehicle-sandbox-m0` or the active campaign
branch. Never force-push or rewrite history.

## 9. Recurring PLAN_ONLY workflow

Each scheduled run should:

1. read Issue #11 before checkout;
2. verify issue identity, schema, uniqueness, enabled state and mode;
3. verify authoritative branch and exact remote head;
4. use detached HEAD for bootstrap;
5. validate control contract and read-only preflight;
6. inspect active PR, CI, gates, lease and queue;
7. select zero work items while the current queue/gates require it;
8. report the exact STOP or material change;
9. make no product mutation;
10. confirm no merge, force-push, rebase or policy self-modification.

Unchanged report:

```text
NO_MATERIAL_CHANGE
<current gate or NO_SAFE_WORK>
authority=<branch>@<sha>
```

## 10. Definition of Ready

Implementation is ready only when:

- owner selected the campaign;
- control mode permits implementation;
- a pre-existing item is owner-approved `AGENT_READY`;
- risk class is eligible;
- exact base SHA is current;
- allowed/forbidden paths are explicit;
- acceptance criteria and tests are observable;
- owner, visual and private gates are resolved or outside scope;
- no conflicting lease or automation PR exists.

Planning text alone is never implementation authority.

## 11. Definition of Done

A bounded change is done only when:

- acceptance criteria pass without relaxed thresholds;
- relevant tests pass and limitations are stated;
- visual work has correct visual authority;
- private evidence stays private;
- docs claim only proven state;
- branch and draft PR identify base and head;
- no merge occurred without owner approval;
- unresolved gates remain explicit.

## 12. Current owner decision queue

1. Execute the real private R1B runner when ready.
2. Perform native visual review on the exact generated pack.
3. Decide PASS, bounded correction or invalidated assumption.
4. Decide whether PR #13 may be promoted.
5. Select exactly one next product campaign.

The recurring operator may surface these decisions but cannot answer them for the
owner.

## 13. Known operating risks

- **Authority drift:** solved operationally by keeping exact mutable SHA in Issue
  #11 instead of hardcoding it as timeless prose.
- **False green:** CI cannot inspect private assets or visual correctness.
- **Historical opacity:** closed PRs remain useful evidence but are not task queues.
- **Parallelism pressure:** map/vehicle/surface work before R1B proof weakens causal
  validation.
- **Scheduler overlap:** platform lacks explicit max-concurrency; repository lease
  remains fail-closed protection.

## 14. Maintenance rule

Update this document only when active campaign, authority strategy, evidence
boundary, roadmap stage or project-wide workflow changes. Routine reports and
unchanged gates do not belong in Git.