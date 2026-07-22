## Purpose

Describe the one coherent outcome this PR proposes.

## Non-goals

- 

## Exact refs

```text
base branch:
base SHA:
head branch:
head SHA:
```

- [ ] The base SHA was rechecked against the remote immediately before first write.
- [ ] This PR does not write directly to `main`, `jozz-vehicle-sandbox-m0` or an active owner branch.

## Authority and classification

- **Campaign:**
- **Risk class:** A0 / A1 / A2 / A3 / A4
- **Control Issue:**
- **Relevant current-state document:**
- **Owner-directed scope:** yes/no

## Scope

### Allowed paths

- 

### Forbidden paths

- `src/**`
- `include/**`
- 

### Diff budget

- **Maximum files:**
- **Maximum changed lines:**
- **Actual files/lines:**

## Changes

- 

## Acceptance criteria

- [ ]

## Evidence and validation

### Tests executed

- [ ] `python tools/automation/validate_control.py`
- [ ] `python tools/project/repository_audit.py`
- [ ] Relevant narrow tests
- [ ] `python tools/scan_pipeline/run_p1_contracts.py` when scan paths changed
- [ ] `.\tools\gate.ps1` when native/vehicle/shared-host capability changed

### Tests unavailable or deliberately not run

List each unavailable capability. `NOT_RUN` and `UNAVAILABLE` are not PASS.

- 

### Visual / runtime evidence

- Not applicable / describe exact screenshot, render, runtime or owner evidence.

## Gates

- **Owner gate:** clear / unresolved
- **Visual gate:** clear / unresolved
- **Private-data gate:** clear / unresolved
- **Threshold/default/behavior gate:** clear / unresolved

## Privacy review

- [ ] No private paths, coordinates, source hashes, credentials or raw scan data.
- [ ] Generated/private outputs remain under ignored `build/`.
- [ ] PR text uses logical IDs or redaction where required.

## Documentation impact

- [ ] No state document changed because proven state did not move.
- [ ] Matching `CURRENT_STATE.md` updated because evidence/state moved.
- [ ] `AI_PROJECT_MEMORY.md` updated only because campaign/authority/gates moved.
- [ ] `PROJECT_OPERATING_PLAN_PL.md` updated only because workflow/roadmap stage moved.

## Migration and rollback

Describe how this change can be backed out or what state must be restored.

- 

## Known limitations

- 

## Non-actions confirmed

- [ ] Draft PR only.
- [ ] No merge or auto-merge.
- [ ] No force-push, rebase or history rewrite.
- [ ] No retarget of another PR.
- [ ] No branch deletion.
- [ ] No unrelated cleanup or substitute work.
- [ ] No private evidence or periodic report committed.
