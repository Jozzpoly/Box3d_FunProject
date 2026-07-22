# Scan import — current state

**Updated:** 2026-07-22  
**Active branch:** `agent/r1b-source-resolution-owner-integration`  
**Active draft PR:** #9  
**Campaign base:** PR #5 exact preview head `f20357ba10618ddecfdd2e274e93917fe508a983`  
**Green functional head:** `7d3c0f20f4bc82fd893f3b4bd0e87a2acc57f1d1` — hosted workflow `29881749220`, 9/9 PASS  
**Integrated authoritative head:** `61238a842ff09be70dec821ef00c05b8e76d2718`  
**Integrated workflow:** `29885369531` — all reported jobs PASS

## Product target

Display the real seven source GLB tiles in the native render-only P2A preview and
manually confirm orientation, scale, up axis, mirror state, coverage, seams and
same-revision restart.

## What changed at the integrated head

The integrated head contains the unchanged R1B product implementation plus the
merged recurring-operator safety foundation from PR #10.

The merge did not add product capability and did not clear any private or visual
gate. It added repository authority, policy and read-only/planning automation
infrastructure. Therefore the product status remains bounded by the same real
owner-local evidence.

## Evidence table

| Stage | Status | Evidence boundary | Next action |
|---|---|---|---|
| Inspection machinery | `PASS_CODE_AND_CI` | repository contracts | preserve |
| Real 7+7 inspection | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local artifacts | do not repeat unnecessarily |
| Source-frame machinery | `PASS_CI_RESTACKED_ON_EXACT_PREVIEW` | R1A code, tests and matrix | preserve |
| Real source frame | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-confirmed local contract | reuse exact contract |
| P1B bundle machinery | `PASS_CODE_AND_CI` | transactional bundle contracts | preserve |
| Real P1B bundle/privacy receipt | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local bundle and receipt | auto-discover by exact binding |
| Exact preview code | `PASS_CODE_AND_CI` | builder/verifier contracts and native compile | real private run |
| Source asset resolution | `PASS_CODE_AND_CI` | nested 7+7 and adversarial resolver tests | real private run |
| Owner-flow orchestration | `PASS_CODE_AND_CI` | receipt-bound discovery, resume state and active selection | real private run |
| One-command owner runner | `PASS_CODE_AND_CI` | canonical contract and Windows PowerShell parse | real private run |
| Automation foundation | `INTEGRATED_PLAN_ONLY` | merged PR #10, strict contracts and adversarial tests | observe scheduled reports |
| Real preview pack | `NOT_CREATED` | no real pack yet | run owner entrypoint once |
| Native load | `NOT_PROVEN` | compile is not runtime proof | launch selected verified pack |
| Visual review | `NOT_RUN` | owner decision required | inspect seven tiles |
| Same-revision restart | `NOT_RUN` | runtime evidence required | restart exact selected pack |
| Surface evidence / derivatives | `FROZEN_ON_PR_7` | outside nearest product goal | wait for visual proof |
| Accepted surface | `NOT_STARTED` | not source evidence | blocked |
| Collision projection | `NOT_STARTED` | requires accepted surface | blocked |
| Drive readiness | `NOT_READY` | requires collision and probes | blocked |

## Active architecture

```text
verified P1B bundle + exact completed owner-gate receipt
→ receipt-bound automatic artifact discovery
→ recursive private source resolution
→ immutable canonical source view
→ exact preview build
→ preview verification
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
root. Later runs should resume through persisted private state. The owner must
not be asked to copy hashes, coordinates or internal paths between tools.

Private paths, coordinates, source hashes and raw scan data must not enter Git,
public logs, PR bodies or the control issue.

## Current gates

```text
private gate: OWNER_LOCAL_REAL_SCAN_RUN_REQUIRED
visual gate:  REAL_TERRAIN_PREVIEW_REVIEW_PENDING
owner gate:   explicit decision at visual review
```

The scheduled project operator is enabled but remains `PLAN_ONLY`. Repeated
reports of these gates are valid and must not trigger substitute implementation.

## Current status vocabulary

The current highest honest status is:

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

A built and verified real pack may report:

```text
REAL_PREVIEW_PACK_READY / VISUAL_REVIEW_PENDING
```

A successful native launch alone may not report visual acceptance.

Only the complete real visual and same-revision restart proof may report:

```text
TERRAIN_VISIBLE_PASS
```

## Next critical path

1. Run the supported owner-local flow once against the existing private evidence.
2. Verify the exact preview pack and selected revision.
3. Launch the pack in the native preview.
4. Perform owner visual review.
5. Restart the same selected revision and confirm repeatability.
6. Record the truthful result before choosing any surface, collision, map or
   vehicle campaign.

Project-wide workflow and later roadmap are in:

```text
docs/PROJECT_OPERATING_PLAN_PL.md
```
