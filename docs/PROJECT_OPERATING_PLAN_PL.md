# Project Operating Plan — Box3d_FunProject

**Status:** current operating map  
**Updated:** 2026-07-22  
**Scope:** three-branch integration, milestone closure and next product gates

## Purpose

This is the single project-wide operating plan. It connects the accepted vehicle,
synthetic engineering world, completed real-geometry milestone, repository authority
and the next textured-preview campaign.

It is not an exact SHA authority, private evidence store, work-item queue or merge
permission.

Durable product intent:

```text
docs/PROJECT_CHARTER_PL.md
```

## Authority

Read in this order:

1. `AGENTS.md`;
2. `.automation/CONTROL.yaml`;
3. `.automation/POLICY.md`;
4. GitHub Issue #11;
5. `AI_PROJECT_MEMORY.md`;
6. matching `docs/*/CURRENT_STATE.md`;
7. active draft PR and exact remote head;
8. this plan;
9. matching contracts, code and tests.

## Current operating snapshot

```text
repository:            Jozzpoly/Box3d_FunProject
authoritative branch: agent/project-refoundation-audit-v1
exact head:            GitHub Control Issue #11
active campaign PR:    #17 (draft)
integration base:      jozz-vehicle-sandbox-m0
remote branches:       3
recurring mode:        PLAN_ONLY
```

PR #17 is the only current review surface. Closed PR #13 and #16 remain lineage and
historical discussion, not active authority.

## Branch topology

```text
main
└─ upstream/history baseline

jozz-vehicle-sandbox-m0
└─ accepted vehicle + synthetic-world baseline
   └─ PR #17: integrated scan milestone + re-foundation review
```

The third branch is the single current project head. No fourth campaign branch may be
created before the integration/base decision for PR #17.

```text
branch cleanup:       COMPLETE_TO_3
further deletion:     FORBIDDEN
retention tag debt:   3
```

Missing tags remain a real retention debt; exact commits are still recoverable.

## Highest honest product state

```text
TERRAIN_VISIBLE_PASS
```

Completed:

- exact private source resolution for seven GLB + seven PLY;
- deterministic geometry pack and independent verifier;
- native seven-tile load;
- owner recognition and acceptance of known source limitations;
- same-revision restart;
- 949 frames / zero Sokol errors.

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

## Owner doctrine controlling the critical path

The first scan is intentionally imperfect but sufficient in its centre. The goal is
not to clean the whole source before learning whether a real familiar place can become
a fun driving surface.

Real colour is required before final scale and collision:

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
→ OWNER_FUN_VERDICT
```

No agent may move collision ahead of texture and scale because geometry filters or
surface experiments already exist.

## Domain status

### Vehicle simulation — FOUNDATION_PRESERVE

Accepted M7/M8 behavior stays unchanged. It later becomes the visual scale reference
and physical drive probe.

### Synthetic engineering world — FOUNDATION_PRESERVE

The accepted deterministic world remains the regression laboratory. It is not
replaced by real scans.

### Scan evidence and geometry preview — FOUNDATION_PRESERVE

Inspection, frame, bundle, privacy, source resolution and preview v1 contracts remain
canonical. Geometry preview v1 stays closed and texture-free.

### Textured Source Preview — ACTIVE_NEXT

Goal:

> Show the same authenticated geometry with recognizable source colour, bounded
> memory and deterministic verification.

Initial baseline: max-1K. Optional comparison: max-2K. Compression is not a blocker
before the first working native render.

### Scale-reference scene — BLOCKED_ON_TEXTURES

Render the unchanged accepted vehicle on the road or beside a known object. Terrain
remains render-only. Owner returns either:

```text
WORLD_SCALE_VALIDATED
or
WORLD_SCALE_CORRECTION_REQUIRED
```

### Surface/collision — PARKED

Former PR #7 evidence remains recoverable by exact SHA but has no authority. Compare
source triangle mesh, PLY/DEM heightfield, parked evidence and Blender-authored proxy
only after texture, scale and owner ROI gates.

### Authoring/world production — LONG_HORIZON

Future Blender workflow may separate roads, terrain, buildings, trees, LODs,
collision proxies and asphalt/grass/mud maps. Do not build the full world-production
pipeline before the first fun drive proof.

### Automation/governance — PLAN_ONLY_SUPPORT

Governance supports the project; it is not the product. The recurring operator may
audit and recommend one next action, but cannot implement or move authority.

## Current transition stages

### F0 — milestone evidence seal — COMPLETE

Charter, redacted checkpoint, capability boundary and texture-before-collision rule
are recorded.

### F1/F2 — forensic inventory and documentation lifecycle — COMPLETE

Major domains, 25 project-authored documents and branch lineage are classified.
Tracked history remains available; old plans cannot activate work by themselves.

### F3 — three-branch integration review — ACTIVE

Required outcomes:

- exact-head governance and native CI green on PR #17;
- Issue #11 points to the existing current branch and head;
- no stale PR #13/#16 authority remains;
- missing retention tags remain explicit debt;
- owner decides whether PR #17 becomes the next durable baseline.

No merge is implied by green CI.

### T0 — Textured Source Preview brief — NEXT AFTER F3

Before implementation define:

- source primitive/material/UV/image identity;
- new pack/version boundary;
- privacy and manifest claims;
- 1K baseline and optional 2K A/B;
- colour-space, sampler, mip and memory policy;
- independent verifier;
- fallback/error semantics;
- fixed-camera screenshot matrix;
- same-revision restart;
- owner acceptance criteria.

### T1 — Textured Source Preview implementation — NOT STARTED

Minimal result:

```text
same seven authenticated tiles
+ source base colour
+ max-1K baseline
+ deterministic pack
+ independent verifier
+ native render
+ no physics
```

### S0 — vehicle scale-reference — BLOCKED_ON_T1

### D0 — Golden Drive Region owner selection — BLOCKED_ON_S0

### D1–D4 — collision comparison, contact, drive and fun — BLOCKED_ON_D0

## Manual workflow

1. Resolve policy and Issue #11.
2. Verify exact remote head and clean isolated workspace.
3. Inspect PR/CI/path overlap.
4. Declare one bounded scope and STOP conditions.
5. Use an isolated branch only after the current integration decision permits it.
6. Run canonical gates rather than reimplementing them.
7. Obtain owner visual/feel evidence when required.
8. Update only documents whose responsibility moved.
9. Open or update one draft PR; stop before merge.

## Current gates

```text
milestone:     TERRAIN_VISIBLE_PASS_EARNED
integration:   PR_17_OWNER_REVIEW_REQUIRED
retention:     THREE_TAGS_PENDING
visual:        TEXTURED_SOURCE_PREVIEW_REQUIRED
scale:         VEHICLE_SCALE_REFERENCE_SCENE_REQUIRED
ROI:           OWNER_SELECTION_REQUIRED
physics:       BLOCKED_UNTIL_TEXTURE_SCALE_AND_ROI
merge:         OWNER_DECISION_REQUIRED
```

## Definition of Ready for textured implementation

- PR #17 integration/base decision is explicit;
- Issue #11 authority is valid and current;
- textured-preview contract exists;
- real seven-tile UV/material/image assumptions are inspected;
- memory budget and 1K/2K profiles are defined;
- verifier/privacy boundaries are explicit;
- no overlapping product PR exists;
- owner approves the brief.

Planning text alone is not implementation authority.
