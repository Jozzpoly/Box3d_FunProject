# Automation Control Center

This issue is the mutable control plane for the recurring repository agent.
Repository policy lives in `.automation/CONTROL.yaml`; this issue never overrides
hard prohibitions such as no merge, no force-push and no autonomous A3/A4 work.

## Owner controls

- Edit `enabled`, `mode`, campaign, authoritative branch/head and gates only as an
  explicit owner action.
- Do not manually edit an active lease without first checking the referenced run,
  branch and draft PR.
- A stale lease is not automatically stolen. Resolve it manually after inspection.
- Never place private paths, coordinates, scan hashes or credentials here.

## Lease procedure

A future run must write its claim, reread it, wait the configured settle interval,
reread again, and revalidate immediately before branch creation and first write.
Any uncertainty means `LOCK_UNCERTAIN / NO_IMPLEMENTATION`.

<!-- automation-control-json:start -->
```json
{
  "active_branch": null,
  "active_campaign": "scan-terrain-r1b",
  "active_pr": null,
  "active_run_id": null,
  "authoritative_branch": "agent/r1b-source-resolution-owner-integration",
  "authoritative_head": "3a0d63e700108155886e1e00df7293f9c3d52db7",
  "current_owner_gate": "NONE",
  "current_private_gate": "OWNER_LOCAL_REAL_SCAN_RUN_REQUIRED",
  "current_visual_gate": "REAL_TERRAIN_PREVIEW_REVIEW_PENDING",
  "enabled": false,
  "last_completed_run": null,
  "last_result": "FOUNDATION_PREPARED_NOT_SCHEDULED",
  "lease_expires_at": null,
  "lease_started_at": null,
  "mode": "PLAN_ONLY",
  "schema_version": 1
}
```
<!-- automation-control-json:end -->

## Label convention

- `automation-control`
- `automation-enabled`
- `automation-disabled`
- `agent-ready`
- `automation-active`
- `owner-needed`
- `visual-review`
- `private-gate`
- `blocked`

Labels are navigation only; the JSON state and confirmed lease are authoritative.
