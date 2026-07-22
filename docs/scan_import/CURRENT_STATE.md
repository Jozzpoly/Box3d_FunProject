# Scan import — current state

**Updated:** 2026-07-22  
**Active branch:** `agent/r1b-source-resolution-owner-integration`  
**Base:** PR #5 exact preview head `f20357ba10618ddecfdd2e274e93917fe508a983`

## Product target

Display the real seven source GLB tiles in the native render-only P2A preview and
manually confirm orientation, scale, up axis, mirror state, coverage and seams.

## Evidence table

| Stage | Status | Evidence boundary | Next action |
|---|---|---|---|
| Inspection machinery | `PASS_CODE_AND_CI` | repository contracts | preserve |
| Real 7+7 inspection | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local artifacts | do not repeat unnecessarily |
| Source-frame machinery | `RESTACKED_ON_EXACT_PREVIEW` | R1A code and tests | validate on new branch |
| Real source frame | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-confirmed local contract | reuse exact contract |
| P1B bundle machinery | `PASS_CODE_AND_CI` | transactional bundle contracts | preserve |
| Real P1B bundle/privacy receipt | `PASS_OWNER_PRIVATE_EVIDENCE` | owner-local bundle and receipt | auto-discover |
| Exact preview code | `PASS_CODE_ONLY` | builder/verifier/native compile | integrate resolver |
| Source asset resolution | `IMPLEMENTED_PENDING_CI_AND_REAL_RUN` | R1B resolver and nested fixture | run CI, then owner-local flow |
| Real preview pack | `NOT_CREATED` | no real pack yet | orchestrator `continue` |
| Native load | `NOT_PROVEN` | compile is not runtime proof | launch selected verified pack |
| Visual review | `NOT_RUN` | owner decision required | inspect seven tiles |
| Surface evidence / derivatives | `FROZEN_ON_PR_7` | outside nearest product goal | wait for visual proof |
| Accepted surface | `NOT_STARTED` | not source evidence | blocked |
| Collision projection | `NOT_STARTED` | requires accepted surface | blocked |
| Drive readiness | `NOT_READY` | requires collision and probes | blocked |

## Active architecture

```text
verified P1B bundle
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

Until the real pack is built and shown, the maximum honest code status is:

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
