#!/usr/bin/env python3
"""Select zero or one work item without inventing replacement work."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Mapping, Sequence

from common import (
    AutomationContractError,
    load_control,
    load_json_document,
    load_work_items,
    machine_result,
)


def select_work_item(
    control: Mapping[str, Any],
    items: Sequence[Mapping[str, Any]],
    *,
    purpose: str,
    control_state: Mapping[str, Any] | None = None,
) -> dict[str, Any]:
    mode = control["mode"]
    state = dict(control_state or {})
    if mode == "DISABLED" or state.get("enabled") is False:
        return machine_result("NO_SAFE_WORK", reason="AUTOMATION_DISABLED")
    for field, result in (
        ("current_owner_gate", "OWNER_GATE"),
        ("current_visual_gate", "VISUAL_GATE"),
        ("current_private_gate", "PRIVATE_DATA_REQUIRED"),
    ):
        value = state.get(field)
        if value not in (None, "NONE", "CLEAR"):
            return machine_result(result, gate=value)
    active = [item for item in items if item["status"] == "ACTIVE"]
    if active:
        return machine_result("ACTIVE_AGENT_DETECTED", active_work_items=[item["id"] for item in active])
    done = {item["id"] for item in items if item["status"] == "DONE"}
    active_ids = {item["id"] for item in active}

    def available(item: Mapping[str, Any]) -> bool:
        return (
            all(dep in done for dep in item["dependencies"])
            and not any(conflict in active_ids for conflict in item["conflicts"])
            and not item["owner_gate"]
            and not item["visual_gate"]
            and not item["private_data_required"]
        )

    if purpose == "implement":
        if mode != "IMPLEMENT_SAFE":
            return machine_result("NO_SAFE_WORK", reason=f"MODE_{mode}_BLOCKS_IMPLEMENTATION")
        candidates = [
            item
            for item in items
            if item["status"] == "AGENT_READY"
            and item["risk_class"] == "A2"
            and item["readiness_authority"] == "OWNER"
            and item["owner_approved_by"]
            and available(item)
        ]
    elif purpose == "plan":
        candidates = [
            item
            for item in items
            if item["status"] in ("PROPOSED", "AGENT_READY")
            and item["risk_class"] in ("A1", "A2")
            and available(item)
        ]
    else:
        raise AutomationContractError("purpose must be plan or implement")

    candidates.sort(key=lambda item: (item["priority"], item["id"]))
    if not candidates:
        return machine_result("NO_SAFE_WORK", reason="QUEUE_HAS_NO_ELIGIBLE_ITEM")
    selected = candidates[0]
    return machine_result(
        "WORK_ITEM_SELECTED",
        purpose=purpose,
        mode=mode,
        work_item_id=selected["id"],
        risk_class=selected["risk_class"],
        exact_base_sha=selected["exact_base_sha"],
        base_resolution_rule=selected["base_resolution_rule"],
        implementation_allowed=purpose == "implement" and mode == "IMPLEMENT_SAFE",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--purpose", choices=("plan", "implement"), default="plan")
    parser.add_argument("--control-state", type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    try:
        control = load_control(repo / ".automation/CONTROL.yaml")
        items = load_work_items(repo / control["work_items"]["path"])
        state = load_json_document(args.control_state) if args.control_state else None
        result = select_work_item(control, items, purpose=args.purpose, control_state=state)
        print(json.dumps(result, sort_keys=True))
        return 0 if result["result"] in ("WORK_ITEM_SELECTED", "NO_SAFE_WORK") else 3
    except AutomationContractError as exc:
        print(json.dumps(machine_result("POLICY_CONFLICT", error=str(exc)), sort_keys=True))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
