#!/usr/bin/env python3
"""Run the three required synthetic recurring-loop simulations."""
from __future__ import annotations

from copy import deepcopy
import json
from pathlib import Path

from common import load_control, machine_result
from select_work_item import select_work_item
from validate_scope import evaluate_scope


def synthetic_item() -> dict[str, object]:
    return {
        "id": "synthetic-a2",
        "title": "Synthetic isolated infrastructure check",
        "campaign": "automation-test",
        "status": "AGENT_READY",
        "risk_class": "A2",
        "priority": 10,
        "exact_base_sha": "1" * 40,
        "base_resolution_rule": None,
        "allowed_paths": ["sandbox/**"],
        "forbidden_paths": ["src/**", "include/**"],
        "acceptance_criteria": ["synthetic result is deterministic"],
        "required_tests": ["synthetic-test"],
        "owner_gate": False,
        "visual_gate": False,
        "private_data_required": False,
        "dependencies": [],
        "conflicts": [],
        "maximum_scope": {
            "max_files": 2,
            "max_changed_lines": 20,
            "single_subsystem": True
        },
        "readiness_authority": "OWNER",
        "owner_approved_by": "Jozzpoly",
        "owner_approved_at": "2026-07-22T00:00:00Z"
    }


def run(repo: Path) -> dict[str, object]:
    control = load_control(repo / ".automation/CONTROL.yaml")
    scenario_a = select_work_item(control, [], purpose="implement", control_state={"enabled": True})
    scenario_b = select_work_item(
        control,
        [],
        purpose="plan",
        control_state={
            "enabled": True,
            "current_owner_gate": "NONE",
            "current_visual_gate": "SYNTHETIC_VISUAL_REVIEW_REQUIRED",
            "current_private_gate": "NONE",
        },
    )
    test_control = deepcopy(control)
    test_control["mode"] = "IMPLEMENT_SAFE"
    item = synthetic_item()
    selected = select_work_item(
        test_control,
        [item],
        purpose="implement",
        control_state={
            "enabled": True,
            "current_owner_gate": "NONE",
            "current_visual_gate": "NONE",
            "current_private_gate": "NONE",
        },
    )
    scope = evaluate_scope(
        test_control,
        item,
        changed_files=["sandbox/synthetic.txt"],
        changed_lines=2,
        patch_text="+synthetic only",
        branch_name="automation/synthetic-a2/test-run-001",
        run_id="test-run-001",
    )
    scenario_c = machine_result(
        "SYNTHETIC_DRAFT_PR_PLAN_READY",
        selection=selected,
        scope=scope,
        draft_pr=True,
        merge=False,
        repository_changes=False,
    )
    return {
        "schema_version": 1,
        "scenario_a": {
            "expected": "NO_SAFE_WORK",
            "observed": scenario_a["result"],
            "zero_changes": True,
            "zero_commits": True,
        },
        "scenario_b": {
            "expected": "VISUAL_GATE",
            "observed": scenario_b["result"],
            "zero_product_changes": True,
            "report_only": True,
        },
        "scenario_c": scenario_c,
    }


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    result = run(repo)
    print(json.dumps(result, indent=2, sort_keys=True))
    ok = (
        result["scenario_a"]["observed"] == "NO_SAFE_WORK"
        and result["scenario_b"]["observed"] == "VISUAL_GATE"
        and result["scenario_c"]["selection"]["result"] == "WORK_ITEM_SELECTED"
        and result["scenario_c"]["scope"]["result"] == "SCOPE_VALID"
        and result["scenario_c"]["merge"] is False
        and result["scenario_c"]["repository_changes"] is False
    )
    return 0 if ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
