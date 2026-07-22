# Project Forensic Inventory — wynik F2

**Date:** 2026-07-22  
**Current branch:** `agent/project-refoundation-audit-v1`  
**Draft PR:** #17  
**Status:** `F2_COMPLETE / F3_INTEGRATION_REVIEW_ACTIVE`  
**Machine-readable companion:** `docs/PROJECT_INVENTORY.json`

## Executive result

The repository-wide inventory classified the major product, engine, world, scan,
tooling, documentation, CI, automation and privacy domains. It also classified 25
project-authored non-archive documents and the PR lineage through #17.

```text
highest product capability: TERRAIN_VISIBLE_PASS
next product gate:          TEXTURED_SOURCE_PREVIEW
WORLD_SCALE_VALIDATED:      false
collision:                  blocked
```

## Current Git topology

After owner-directed cleanup the remote contains exactly 3 branches:

```text
main
jozz-vehicle-sandbox-m0
agent/project-refoundation-audit-v1
```

```text
preferred final state: 3 branches
hard maximum:          5 branches
current count:         3 branches
further deletion:      forbidden
```

The current branch is linearly ahead of the accepted vehicle baseline and contains the
former PR #13 integration ancestry plus the re-foundation work formerly reviewed in
PR #16.

PR #13 and #16 are closed and unmerged because their old base/head topology was removed.
PR #17 is the only current draft review surface.

## Retention tag debt

Three planned tags were not created before branch cleanup:

```text
milestone/terrain-visible-2026-07-22
  → 33099413bf8f44adbe1d635f9e10bdf2d0b5c321

evidence/surface-foundation-pr7
  → 9aacc752f331d0d47c4c9c3f6fe82c63466f592c

evidence/owner-flow-pr8
  → a36e3d2f4c76f35d138a7e8b0aa11f7889e69e90
```

All exact commits remain readable, so work was not lost. Nevertheless:

```text
retention tag debt:      OPEN
TAG_RETENTION_COMPLETE:  not granted
```

Tags must be created from a capable git environment. Recreating old branches is not
required and would violate the three-branch target.

## Domain verdicts

### FOUNDATION_PRESERVE

- upstream Box3D core/support;
- shared native host and renderer;
- accepted M7/M8 vehicle;
- deterministic synthetic engineering world;
- asset tooling and evidence;
- scan inspection/source contracts;
- geometry preview v1;
- PLAN_ONLY automation and governance.

### ACTIVE_NEXT

```text
TEXTURED_SOURCE_PREVIEW
```

### OWNER_DECISION_REQUIRED

- vehicle scale-reference verdict;
- Golden Drive Region selection;
- eventual JES adoption boundary;
- integration outcome of PR #17.

### PARKED_WITH_REACTIVATION_GATE

- surface/collision research from former PR #7 and related probes;
- future synthetic-map stages after explicit owner reactivation.

### KNOWN_DEBT

- three missing retention tags;
- future Blender world authoring;
- roads/terrain/buildings/vegetation separation;
- render/collision LODs and material maps;
- large-world composition and streaming.

## Documentation conflicts resolved

- global policy routes through `AGENTS.md`, not the vehicle manual;
- no current instruction permits direct push to `jozz-vehicle-sandbox-m0`;
- the old P1B tutorial is not the current start page;
- the upstream default PR template no longer blocks fork PR workflow;
- stale active plans are completion/history records or parked domain plans;
- the hotkey reference reflects current code;
- geometry preview v1 remains explicitly texture-free and collision-free.

Machine-readable lifecycle:

```text
docs/DOCUMENT_LIFECYCLE_2026_07_22.json
```

No project-authored historical document may activate work by itself.

## Product critical path

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
→ OWNER_FUN_VERDICT
```

This ordering is deliberate. Real colour provides the perceptual context needed to
judge scale and choose the first road region. Collision cannot bypass those gates.

## F3 exit conditions

F3 integration review is complete only when:

1. exact-head Repository Governance passes;
2. Automation Foundation Safety passes;
3. scan contracts and native Windows gate pass;
4. Issue #11 points to the existing current branch and exact head;
5. current documents point to PR #17 rather than closed PR #13/#16;
6. branch count stays equal to 3;
7. retention tag debt stays explicit;
8. owner chooses the PR #17 integration outcome.

Green CI is evidence, not merge authorization.

## Next action after F3

Prepare one bounded `TEXTURED_SOURCE_PREVIEW` campaign brief. Do not implement
collision, alter accepted vehicle physics or build the full Blender/world-production
pipeline during that brief.
