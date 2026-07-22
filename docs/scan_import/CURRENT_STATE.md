# Scan import — current state

**Updated:** 2026-07-22  
**Active branch:** `agent/r1b-source-resolution-owner-integration`  
**Base:** PR #5 exact preview head `f20357ba10618ddecfdd2e274e93917fe508a983`  
**Green functional head:** `b6d9f5b6559f3b82616a1ff6ef737ef89457f8e2` — hosted workflow 9/9 PASS

## Product target

Display the real seven source GLB tiles in the native render-only P2A preview and
manually confirm orientation, scale, up axis, mirror state, coverage and seams.

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
| Real preview pack | `NOT_CREATED` | no real pack yet | orchestrator `continue` |
| Native load | `NOT_PROVEN` | compile is not runtime proof | launch selected verified pack |
| Visual review | `NOT_RUN` | owner decision required | inspect seven tiles |
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

## Current status vocabulary

The current highest honest status is:

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

A built and verified real pack may report:

```text
REAL_PREVIEW_PACK_READY / VISUAL_REVIEW_PENDING
```

Only the complete real visual and restart proof may report:

```text
TERRAIN_VISIBLE_PASS
```
