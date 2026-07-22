# Automation Control Center

This Issue is the mutable control plane for the recurring repository agent.
Hard policy lives in `AGENTS.md`, `.automation/CONTROL.yaml` and
`.automation/POLICY.md`. Mutable fields may select state only inside those rules;
they never override no-merge, no-force-push, protected paths or A3/A4 STOP gates.

## Owner controls

- Edit `enabled`, `mode`, campaign, authoritative branch/head and gates only as an
  explicit owner action.
- Use a full 40-character remote head SHA, never a short SHA or local assumption.
- Do not edit an active lease without checking the referenced run, branch and PR.
- A stale lease is never automatically stolen. Resolve it manually after inspection.
- Never place private paths, coordinates, scan hashes or credentials here.

## Lease procedure

A write-capable run must write its claim, reread it, wait the configured settle
interval, reread again and revalidate immediately before branch creation and first
write. Any uncertainty means `LOCK_UNCERTAIN / NO_IMPLEMENTATION`.

<!-- automation-control-json:start -->
```json
{
  "active_branch": null,
  "active_campaign": "OWNER_SELECTED_CAMPAIGN",
  "active_pr": null,
  "active_run_id": null,
  "authoritative_branch": "OWNER_SELECTED_BRANCH",
  "authoritative_head": "0000000000000000000000000000000000000000",
  "current_owner_gate": "NONE",
  "current_private_gate": "NONE",
  "current_visual_gate": "NONE",
  "enabled": false,
  "last_completed_run": null,
  "last_result": "CONTROL_TEMPLATE_NOT_ACTIVATED",
  "lease_expires_at": null,
  "lease_started_at": null,
  "mode": "PLAN_ONLY",
  "schema_version": 1
}
```
<!-- automation-control-json:end -->

Replace all placeholder values before activation. The zero SHA is deliberately
non-authoritative and must never be used as an exact base.

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

Labels are navigation only. The validated JSON state and confirmed lease are
authoritative for mutable run state.
