# Scan import — current state

**Updated:** 2026-07-22  
**Authoritative branch:** `agent/scan-terrain-r1b-consolidated-integration`  
**Active draft PR:** #13  
**Exact current head:** read from GitHub Control Issue #11  
**Integration base:** `jozz-vehicle-sandbox-m0`

## Product target

Display the real seven source GLB tiles in the native render-only P2A preview and
manually confirm orientation, scale, up axis, mirror state, coverage, seams and
same-revision restart.

## Consolidation state

PR #13 is the single current integration surface for the P0–R1B campaign. It
preserves the existing linear ancestry from the vehicle sandbox through:

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

Historical PRs #1–#5, #8 and #9 are superseded by PR #13. Their branches remain
preserved. PR #7 is a divergent surface-evidence experiment and is parked in
Issue #14 rather than included in the visual-preview critical path.

## Validation lineage

```text
R1B green functional head:
7d3c0f20f4bc82fd893f3b4bd0e87a2acc57f1d1
workflow 29881749220: 9/9 PASS

pre-consolidation automation/docs integration merge:
0078257cab632510ddf3cca8c449fd44e2327a3a

current exact authoritative head:
GitHub Control Issue #11
```

The later automation, documentation and stack-cleanup changes do not add product
capability and do not clear private or visual gates.

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
| Stack consolidation | `DRAFT_PR_13` | ancestry audit; no rewrite | keep draft |
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

`ScanSourcePackage` remains the authority for expected byte identity. The private
resolution receipt is the authority for physical local source location. Neither
accepted-world nor collision authority exists in this stage.

## Supported owner entrypoint

```text
run_real_terrain_flow.ps1
```

The first successful run may require the owner to identify the private source
root. Later runs resume through persisted private state. The owner must not be
asked to copy hashes, coordinates or internal paths between tools.

Private paths, coordinates, source hashes and raw scan data must not enter Git,
public logs, PR bodies or the control issue.

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

1. Keep PR #13 draft and treat Issue #11 as exact authority.
2. Run the supported owner-local flow once against existing private evidence.
3. Verify the exact preview pack and selected revision.
4. Launch it in the native preview.
5. Perform owner visual review.
6. Restart the same revision and confirm repeatability.
7. Record the truthful result before selecting surface, collision, map or vehicle
   work.

Project-wide workflow and roadmap:

```text
docs/PROJECT_OPERATING_PLAN_PL.md
```