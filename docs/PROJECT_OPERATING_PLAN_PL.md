# Project Operating Plan — Box3d_FunProject

**Status:** current operating map  
**Updated:** 2026-07-22  
**Scope:** authority, milestone closure, project re-foundation and next product gates  
**Product code changed by this document:** no

## 1. Purpose

This is the single project-wide operating plan. It connects:

- the accepted vehicle foundation;
- the synthetic engineering world;
- the completed geometry-preview milestone;
- the upcoming textured-source preview;
- future scale, collision and real-drive proof;
- repository authority, integration and recurring governance.

It is not a work-item queue, exact SHA authority, private evidence, owner visual
approval or permission to merge.

Durable product intent:

```text
docs/PROJECT_CHARTER_PL.md
```

## 2. Policy and authority

Load hard policy first:

1. `AGENTS.md`;
2. `.automation/CONTROL.yaml`;
3. `.automation/POLICY.md`.

Resolve mutable state and facts:

1. GitHub Control Issue #11;
2. `AI_PROJECT_MEMORY.md`;
3. matching `docs/*/CURRENT_STATE.md`;
4. active campaign PR and remote head;
5. this operating plan;
6. `docs/PROJECT_CHARTER_PL.md` for durable intent;
7. matching domain manual, checkpoints, technical debt, contracts, code and tests.

A mutable Issue cannot legalize an operation forbidden by policy. A versioned plan
cannot safely claim its own commit as timelessly current.

## 3. Current operating snapshot

```text
repository:                   Jozzpoly/Box3d_FunProject
control campaign:             scan-terrain-r1b milestone closure
authoritative branch:         agent/scan-terrain-r1b-consolidated-integration
exact head:                   GitHub Control Issue #11
active campaign PR:           #13 (draft)
integration base:             jozz-vehicle-sandbox-m0
surface parking issue:        #14
project re-foundation branch: agent/project-refoundation-audit-v1
recurring mode:               PLAN_ONLY
```

PR #15 repository-readiness work has been owner-reviewed and merged into the
current PR #13 authority. It is no longer a separate pending layer.

The re-foundation branch is a manual review surface. It does not become authority
until owner-reviewed integration and an explicit Issue #11 update.

## 4. Highest honest product state

```text
TERRAIN_VISIBLE_PASS
```

Completed:

- real private seven-tile source resolution;
- exact geometry preview pack;
- independent verification;
- native first load;
- owner visual recognition and acceptance of geometry proof;
- same-revision restart;
- 949-frame restart run with zero Sokol errors.

Not completed:

```text
TEXTURED_SOURCE_PREVIEW
WORLD_SCALE_VALIDATED
GOLDEN_DRIVE_REGION_SELECTED
ACCEPTED_SURFACE
COLLISION_REPRESENTATION_SELECTED
FIRST_REAL_SCAN_DRIVE
OWNER_FUN_VERDICT
```

Milestone record:

```text
docs/scan_import/TERRAIN_VISIBLE_PASS_2026_07_22_PL.md
```

## 5. Owner doctrine now controlling the critical path

The first scan is intentionally imperfect but sufficient in its centre. The nearest
product question is not whether the entire source can be cleaned automatically. It is
whether a real, recognizable place can become a fun driving surface for the accepted
vehicle.

However, the owner requires real colour before final scale and collision work.
The canonical product-gate sequence is:

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
→ OWNER_FUN_VERDICT
```

Human interpretation:

```text
geometry proof
→ textured recognition
→ vehicle as scale reference
→ owner-selected road region
→ collision research
→ drive and fun proof
```

No agent may reorder collision ahead of texture/scale merely because geometry and
ground-filter experiments already exist.

## 6. Project domains and status

### A. Vehicle simulation — FOUNDATION_PRESERVE

The accepted M7/M8 architecture remains stable:

- emergent multi-body suspension;
- back-drivable steering;
- torque drive/braking;
- accepted rig/persistence/diagnostics;
- full Windows gate and owner feel boundaries.

The vehicle will become the scale reference and later the drive probe. Scan work must
not silently change accepted vehicle physics.

### B. Synthetic engineering world — FOUNDATION_PRESERVE

The accepted Etap 1 terrain remains the deterministic laboratory for:

- regression tests;
- controlled obstacles;
- reproducible suspension comparisons;
- performance baselines.

The rejected six-lane layout remains historical/recovery material. The central-campus
plan is paused unless separately selected.

### C. Scan source/evidence — FOUNDATION_PRESERVE

Inspection, source-frame, bundle, receipt, source resolution and privacy boundaries
are proven and remain canonical for exact source identity.

### D. Geometry preview v1 — MILESTONE_COMPLETE

Geometry-only pack and native lab produced `TERRAIN_VISIBLE_PASS`. Version 1 remains
closed: no textures, accepted-world or collision claims may be inserted into it.

### E. Textured source preview — ACTIVE_NEXT_AFTER_REFOUNDATION

Goal:

> Display the same verified source geometry with authenticated colour/material
> bindings and obtain owner visual approval before scale/collision promotion.

Initial evidence favours max-1K textures as baseline and max-2K as quality A/B. The
final contract must be re-designed against the current seven-tile source set.

### F. Scale-reference scene — BLOCKED_ON_TEXTURES

The accepted vehicle is rendered on the road or beside a known object without yet
promoting terrain collision. The owner evaluates physical plausibility of scale.

### G. Surface/collision — PARKED_FOR_COMPARATIVE_REVIEW

Issue #14 preserves historical surface evidence. Existing DEM, morphology, drive
probe and seam experiments are evidence, not authority.

Collision starts only after:

- textured preview acceptance;
- scale-reference acceptance;
- owner-selected Golden Drive Region;
- comparative representation brief.

### H. Authoring/world production — LONG_HORIZON

Future Blender and creator workflow may separate:

- roads and ground;
- buildings;
- trees and vegetation;
- render LODs;
- collision proxies;
- asphalt/grass/mud maps;
- world sectors and streaming.

Do not build the full pipeline before first drive proof.

### I. Automation/governance — PLAN_ONLY_SUPPORT

The recurring operator audits authority and gates. It must not become the product or
force manual owner work through unnecessary scheduler ceremony.

## 7. Critical-path roadmap

### Stage R0 — consolidated scan integration — COMPLETE

- historical scan stack consolidated into PR #13;
- governance/readiness integrated;
- exact authority and CI established;
- no history rewrite.

### Stage R1 — owner-local source activation — COMPLETE

- verified private evidence found;
- fourteen source assets resolved exactly;
- pack built transactionally.

### Stage R2 — native geometry preview — COMPLETE

- independent verifier passed;
- seven tiles loaded;
- owner observed recognizable terrain.

### Stage R3 — same-revision visual proof — COMPLETE

- owner accepted geometry visibility and known source limitations;
- same revision restarted;
- deterministic pack selection/layout repeated;
- `TERRAIN_VISIBLE_PASS` earned.

### Stage F0 — milestone evidence seal — IN PROGRESS

Outputs:

- Project Charter;
- redacted milestone checkpoint;
- current-state truth correction;
- explicit boundary that scale is not yet final;
- texture-before-collision rule.

### Stage F1 — project re-foundation inventory — NEXT GOVERNANCE GATE

Perform a repository-wide forensic inventory covering:

- branches/PRs/issues;
- documentation and authority;
- vehicle and synthetic world;
- scan/evidence/preview;
- textures;
- surface/collision experiments;
- authoring/editor tooling;
- build/tests/CI;
- automation/governance;
- privacy and JES boundary.

Every item receives one classification:

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

No product implementation is permitted merely to make the inventory look cleaner.

### Stage F2 — documentation and workflow re-foundation

Targets:

- one concise global route;
- one durable charter;
- one mutable project router;
- one global operating plan;
- one current state per active domain;
- machine-readable documentation/inventory manifest;
- explicit history/archive roles;
- manual workflow kept simpler than recurring automation workflow.

Protected governance changes are manual A3 and require separate review.

### Stage F3 — integration decision for PR #13

Choose exactly one:

```text
A. owner-review and merge into the vehicle baseline
B. bounded correction before merge
C. preserve as milestone branch while re-planning integration
```

Do not rebase, squash or force-push merely to beautify the 185-commit lineage.

### Stage T0 — textured preview campaign brief

Before implementation define:

- exact source image/material/UV identity;
- pack/version boundary;
- privacy and manifest claims;
- texture cook profiles;
- colour-space and sampler policy;
- memory/performance budget;
- independent verification;
- native fallback and error semantics;
- fixed-camera screenshot matrix;
- owner acceptance criteria;
- same-revision restart proof.

### Stage T1 — textured source preview implementation

Minimal first target:

```text
same seven source tiles
+ unlit/authentic base colour
+ max-1K profile
+ deterministic pack
+ independent verifier
+ native render
+ no physics
```

2K is an A/B profile, not the baseline. GPU compression is deferred until a working
render can be measured.

### Stage S0 — vehicle scale-reference scene

After texture acceptance:

- render the accepted vehicle on/near a recognizable road or house;
- preserve terrain as render-only;
- record fixed cameras;
- owner confirms or rejects scale;
- no collision claim.

Result vocabulary:

```text
WORLD_SCALE_VALIDATED
or
WORLD_SCALE_CORRECTION_REQUIRED
```

### Stage D0 — Golden Drive Region selection

Owner chooses one small, visually understood road region. Selection must be stored as
logical/redacted evidence without publishing private location data.

### Stage D1 — collision representation comparison

Compare at least:

- bounded source triangle mesh;
- PLY/DEM-derived heightfield;
- parked surface-evidence approach;
- manually authored Blender collision proxy.

Evaluate fidelity, wheel stability, CPU/memory, authoring cost and future chunking.

### Stage D2 — static wheel/contact proof

- same transform as render;
- contact at four wheels;
- no catastrophic penetration;
- measured surface steps/noise;
- visual render/collision difference overlay.

### Stage D3 — vehicle drop and low-speed drive

- spawn over selected road;
- settle suspension;
- move at controlled speed;
- monitor contacts, solver stability and frame cost.

### Stage D4 — owner fun verdict

The real milestone:

> Does driving the accepted vehicle on the real scan make Jozz want to launch a
> higher-quality scan campaign?

CI cannot answer this gate.

## 8. Integration topology

```text
main
└─ preserved upstream/history line

jozz-vehicle-sandbox-m0
└─ accepted vehicle + synthetic-world baseline
   └─ PR #13 consolidated geometry-preview milestone
      └─ project re-foundation review branch
```

The future textured-preview implementation must use a new isolated product branch
after the milestone/integration decision. It must not grow indefinitely from the
current audit branch.

## 9. Manual work workflow

1. Read policy, Issue, memory, current state and charter.
2. Verify exact remote head.
3. Inspect open PRs, CI and overlapping paths.
4. Declare one scope, allowed/forbidden paths, evidence and STOP conditions.
5. Create an isolated branch.
6. Implement or audit one coherent unit.
7. Run existing canonical gates; do not clone their logic.
8. Inspect visual evidence when relevant.
9. Update documents only when their responsibility moved.
10. Open a draft PR and stop before merge.

Manual work must not be made artificially difficult only because recurring automation
needs lease and queue machinery.

## 10. Recurring PLAN_ONLY workflow

Each scheduled run may:

- validate control and authority;
- inspect Issue/PR/CI/gates/lease;
- report material change or no safe work;
- recommend one bounded next action.

It may not:

- implement re-foundation;
- add or promote its own work item;
- modify protected control plane;
- infer texture/scale/feel acceptance;
- merge or move authority.

## 11. Current gates

```text
milestone evidence:  TERRAIN_VISIBLE_PASS_EARNED
project transition:  REFOUNDATION_REVIEW_REQUIRED
visual:               TEXTURED_SOURCE_PREVIEW_REQUIRED
scale:                VEHICLE_SCALE_REFERENCE_SCENE_REQUIRED
ROI:                  OWNER_SELECTION_REQUIRED
physics:              BLOCKED_UNTIL_TEXTURE_SCALE_AND_ROI
merge:                OWNER_DECISION_REQUIRED
```

## 12. Definition of Ready for textured implementation

Implementation is ready only when:

- milestone/integration base is explicitly selected;
- textured-preview contract is written;
- source image/material/UV assumptions are inspected on the real seven-tile set;
- memory budget and 1K/2K profiles are defined;
- manifest/verifier/privacy boundaries are explicit;
- allowed/forbidden paths and tests are observable;
- no overlapping PR exists;
- owner approves the campaign brief.

Planning text alone is not implementation authority.

## 13. Definition of Done for the current re-foundation

- milestone evidence is recorded without private data;
- versioned state no longer claims the real preview is unrun;
- scale remains explicitly unproven;
- texture-before-collision is enforced in current docs and audit;
- PR #15 integration state is truthful;
- every major project area has an inventory classification or explicit unreviewed
  status;
- PR #13 has an owner-reviewed integration outcome;
- next campaign has one bounded brief;
- no collision code or accepted-physics change is smuggled into governance work.

## 14. Maintenance rule

Update this document only when the critical path, authority strategy, evidence
boundary, integration topology or project-wide workflow changes. Routine CI and
unchanged gate reports do not belong here.
