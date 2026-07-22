# Scan import — current state

**Updated:** 2026-07-22  
**Authoritative branch:** `agent/scan-terrain-r1b-consolidated-integration`  
**Active draft PR:** #13  
**Exact current head:** read from GitHub Control Issue #11  
**Integration base:** `jozz-vehicle-sandbox-m0`  
**Repository-readiness layer:** PR #15 — draft, stacked, not authoritative

## Product target

Display the real seven source GLB tiles in the native render-only P2A preview and
manually confirm orientation, scale, up axis, mirror state, coverage, seams and
same-revision restart.

## Product integration state

PR #13 is the single current product integration surface for P0–R1B. It preserves
the existing linear ancestry:

```text
photogrammetry/import-v2-foundation
→ P1 inspection
→ P1B contracts
→ P1B evidence bundles
→ owner-local gate hardening
→ P2A exact source preview
→ R1B private source resolution and resumable owner flow
→ recurring-agent safety foundation
→ project operating documentation
```

The consolidation used no rebase, squash, force-push or history rewrite.
Historical PRs #1–#5, #8 and #9 are superseded by #13 with branches preserved. The
divergent surface-evidence line is parked in Issue #14.

## Repository-readiness state

PR #15 is an owner-directed governance layer based on the exact head of PR #13.
It proposes:

- truthful root README and fork contribution workflow;
- unambiguous global policy routing through `AGENTS.md`;
- documentation index and repository ownership map;
- manual PR template;
- fail-closed repository authority/workflow audit with sabotage tests;
- CI routing to the consolidated campaign;
- removal of obsolete duplicate P2A workflow.

PR #15 changes no scan implementation, asset format, native sample or capability.
It is not authoritative until separately reviewed and integrated. Issue #11
therefore continues to point at the validated PR #13 head rather than at an
unreviewed change to the agent's own policy.

## Validation lineage

```text
R1B green functional head:
7d3c0f20f4bc82fd893f3b4bd0e87a2acc57f1d1
workflow 29881749220: 9/9 PASS

pre-consolidation automation/docs integration merge:
0078257cab632510ddf3cca8c449fd44e2327a3a

consolidated product integration:
PR #13 — workflow 29888259464: 6/6 PASS

current exact authoritative head:
GitHub Control Issue #11

repository-readiness validation:
PR #15 live checks and PR body are authoritative for its exact head
```

Routine CI state is intentionally not mirrored here. Automation, documentation,
stack cleanup and repository polish do not add product capability and do not clear
private or visual gates.

## Evidence table

| Stage | Status | Evidence boundary | Next action |
|---|---|---|---|
| Inspection machinery | `PASS_CODE_AND_CI` | repository contracts | preserve |
| Real 7+7 inspection | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local artifacts | reuse |
| Source-frame machinery | `PASS_CI_RESTACKED_ON_EXACT_PREVIEW` | code/tests | preserve |
| Real source frame | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-confirmed local contract | reuse exact contract |
| P1B bundle machinery | `PASS_CODE_AND_CI` | transactional contracts | preserve |
| Real P1B bundle/privacy receipt | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local bundle/receipt | auto-discover |
| Exact preview code | `PASS_CODE_AND_CI` | builder/verifier/native compile | real private run |
| Source asset resolution | `PASS_CODE_AND_CI` | adversarial resolver tests | real private run |
| Owner-flow orchestration | `PASS_CODE_AND_CI` | receipt-bound resume/selection | real private run |
| One-command runner | `PASS_CODE_AND_CI` | contracts and PowerShell parse | real private run |
| Automation foundation | `INTEGRATED_PLAN_ONLY` | strict contracts/adversarial tests | observe |
| Product stack consolidation | `DRAFT_PR_13_CI_GREEN` | ancestry audit; no rewrite | keep draft |
| Repository readiness | `DRAFT_PR_15_UNDER_REVIEW` | governance/docs/workflow only | live checks and review |
| Real preview pack | `NOT_CREATED` | private evidence absent | run owner entrypoint |
| Native load | `NOT_PROVEN` | compile is not runtime proof | launch exact selected pack |
| Visual review | `NOT_RUN` | owner decision required | inspect seven tiles |
| Same-revision restart | `NOT_RUN` | runtime evidence required | restart exact pack |
| Surface evidence | `PARKED_ISSUE_14` | outside nearest goal | wait for visual proof |
| Accepted surface | `NOT_STARTED` | separate authority | blocked |
| Collision projection | `NOT_STARTED` | requires accepted surface | blocked |
| Drive readiness | `NOT_READY` | requires collision/probes | blocked |

## Active architecture

```text
verified P1B bundle + exact completed owner-gate receipt
→ receipt-bound automatic artifact discovery
→ recursive private source resolution
→ immutable canonical source view
→ exact preview build
→ independent preview verification
→ ACTIVE_PREVIEW.json
→ native launch with explicit pack binding
→ owner visual review
→ same-revision restart
→ TERRAIN_VISIBLE_PASS
```

`ScanSourcePackage` remains authority for expected byte identity. The private
resolution receipt is authority for physical local source location. Neither
accepted-world nor collision authority exists in this stage.

## Supported owner entrypoint

```text
run_real_terrain_flow.ps1
```

The first successful run may require the owner to identify the private source
root. Later runs resume through persisted private state. The owner must not be
asked to copy hashes, coordinates or internal paths between tools.

Private paths, coordinates, source hashes and raw scan data must not enter Git,
public logs, PR bodies or the Control Issue.

## Current gates

```text
private gate: OWNER_LOCAL_REAL_SCAN_RUN_REQUIRED
visual gate:  REAL_TERRAIN_PREVIEW_REVIEW_PENDING
owner gate:   explicit decision at visual review
```

The scheduled operator remains enabled in `PLAN_ONLY`. Repeated reports of these
gates are valid and must not trigger substitute implementation.

## Status vocabulary

Current highest honest status:

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

After a real pack is built and independently verified:

```text
REAL_PREVIEW_PACK_READY / VISUAL_REVIEW_PENDING
```

A successful native launch alone is not visual acceptance. Only complete visual
review and same-revision restart may report:

```text
TERRAIN_VISIBLE_PASS
```

## Next critical path

1. Keep PR #13 draft and treat Issue #11 as exact product authority.
2. Validate and review PR #15 independently as governance-only work.
3. Run the owner-local flow once against existing private evidence.
4. Verify the exact preview pack and selected revision.
5. Launch it in the native preview.
6. Perform owner visual review.
7. Restart the same revision and confirm repeatability.
8. Record PASS, a bounded correction need or an invalidated assumption.
9. Only then decide integration order and the next product campaign.

Project-wide workflow, O1 readiness gate and later roadmap:

```text
docs/PROJECT_OPERATING_PLAN_PL.md
```
