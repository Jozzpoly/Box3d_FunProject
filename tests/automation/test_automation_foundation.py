from __future__ import annotations

from copy import deepcopy
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).parents[2]
AUTOMATION_TOOLS = ROOT / "tools" / "automation"
sys.path.insert(0, str(AUTOMATION_TOOLS))

import common
import lease
import postflight
import preflight
import select_work_item
import simulate_scenarios
import validate_scope


def control() -> dict[str, object]:
    return common.load_control(ROOT / ".automation" / "CONTROL.yaml")


def issue_state(**overrides: object) -> dict[str, object]:
    value: dict[str, object] = {
        "schema_version": 1,
        "enabled": True,
        "mode": "PLAN_ONLY",
        "active_campaign": "synthetic",
        "authoritative_branch": "agent/synthetic",
        "authoritative_head": "1" * 40,
        "current_owner_gate": "NONE",
        "current_visual_gate": "NONE",
        "current_private_gate": "NONE",
        "active_run_id": None,
        "lease_started_at": None,
        "lease_expires_at": None,
        "active_branch": None,
        "active_pr": None,
        "last_completed_run": None,
        "last_result": "NONE",
    }
    value.update(overrides)
    return common.validate_control_issue_state(value)


def item(**overrides: object) -> dict[str, object]:
    value: dict[str, object] = {
        "id": "synthetic-a2",
        "title": "Synthetic safe work",
        "campaign": "synthetic",
        "status": "AGENT_READY",
        "risk_class": "A2",
        "priority": 10,
        "exact_base_sha": "1" * 40,
        "base_resolution_rule": None,
        "allowed_paths": ["sandbox/**"],
        "forbidden_paths": ["src/**", "include/**"],
        "acceptance_criteria": ["synthetic criterion"],
        "required_tests": ["synthetic-test"],
        "owner_gate": False,
        "visual_gate": False,
        "private_data_required": False,
        "dependencies": [],
        "conflicts": [],
        "maximum_scope": {
            "max_files": 2,
            "max_changed_lines": 20,
            "single_subsystem": True,
        },
        "readiness_authority": "OWNER",
        "owner_approved_by": "Jozzpoly",
        "owner_approved_at": "2026-07-22T00:00:00Z",
    }
    value.update(overrides)
    return common.validate_work_item(value)


class ControlContractTests(unittest.TestCase):
    def test_default_mode_is_plan_only(self) -> None:
        self.assertEqual(control()["mode"], "PLAN_ONLY")

    def test_plan_only_blocks_implementation(self) -> None:
        result = select_work_item.select_work_item(
            control(), [item()], purpose="implement", control_state={"enabled": True}
        )
        self.assertEqual(result["result"], "NO_SAFE_WORK")
        self.assertIn("PLAN_ONLY", result["reason"])

    def test_automation_cannot_raise_itself_to_implement_safe(self) -> None:
        old = control()
        new = deepcopy(old)
        new["mode"] = "IMPLEMENT_SAFE"
        with self.assertRaisesRegex(common.AutomationContractError, "cannot modify"):
            common.validate_control_transition(old, new, "AUTOMATION")

    def test_unknown_control_field_is_error(self) -> None:
        document = deepcopy(control())
        document["surprise"] = True
        with self.assertRaisesRegex(common.AutomationContractError, "unknown"):
            common.validate_control(document)

    def test_periodic_reports_cannot_be_committed(self) -> None:
        document = control()
        self.assertFalse(document["reporting"]["commit_periodic_reports"])
        self.assertTrue(document["reporting"]["local_report_path"].startswith("build/"))
        changed = deepcopy(document)
        changed["reporting"]["commit_periodic_reports"] = True
        with self.assertRaises(common.AutomationContractError):
            common.validate_control(changed)

    def test_agent_ready_requires_real_owner_identity(self) -> None:
        with self.assertRaisesRegex(common.AutomationContractError, "owner identity"):
            item(owner_approved_by="automation-agent")

    def test_a3_cannot_be_agent_ready(self) -> None:
        with self.assertRaises(common.AutomationContractError):
            item(risk_class="A3")


class LeaseAndPreflightTests(unittest.TestCase):
    def test_second_active_lease_fails_closed(self) -> None:
        result, claimed = lease.evaluate_claim(
            issue_state(), run_id="run-first-001", exact_base_sha="1" * 40, lease_minutes=45
        )
        self.assertEqual(result["result"], "LEASE_CLAIM_PROPOSED")
        self.assertIsNotNone(claimed)
        second, second_state = lease.evaluate_claim(
            claimed or {}, run_id="run-second-002", exact_base_sha="1" * 40, lease_minutes=45
        )
        self.assertEqual(second["result"], "ACTIVE_AGENT_DETECTED")
        self.assertIsNone(second_state)

    def test_stale_lock_is_not_taken_over(self) -> None:
        old = datetime.now(timezone.utc) - timedelta(hours=2)
        stale = issue_state(
            active_run_id="stale-run-001",
            lease_started_at=(old - timedelta(minutes=45)).isoformat(),
            lease_expires_at=old.isoformat(),
        )
        result, proposed = lease.evaluate_claim(
            stale, run_id="new-run-002", exact_base_sha="1" * 40, lease_minutes=45
        )
        self.assertEqual(result["result"], "LOCK_UNAVAILABLE")
        self.assertIsNone(proposed)

    def test_existing_automation_pr_blocks_second(self) -> None:
        result = preflight.evaluate_preflight(
            control(),
            issue_state(),
            worktree_clean=True,
            current_branch="agent/foundation",
            current_head="1" * 40,
            remote_authoritative_head="1" * 40,
            open_automation_prs=["#12:automation/x/run"],
        )
        self.assertEqual(result["result"], "ACTIVE_AGENT_DETECTED")

    def test_base_moved_stops_before_write(self) -> None:
        result = preflight.evaluate_preflight(
            control(),
            issue_state(),
            worktree_clean=True,
            current_branch="agent/foundation",
            current_head="1" * 40,
            remote_authoritative_head="2" * 40,
            open_automation_prs=[],
        )
        self.assertEqual(result["result"], "BASE_MOVED")
        self.assertFalse(result["implementation_allowed"])

    def test_interrupted_run_remains_diagnosable(self) -> None:
        now = datetime.now(timezone.utc)
        active = issue_state(
            active_run_id="interrupted-001",
            lease_started_at=now.isoformat(),
            lease_expires_at=(now + timedelta(minutes=30)).isoformat(),
            active_branch="automation/synthetic/interrupted-001",
            active_pr="#99",
        )
        result = preflight.evaluate_preflight(
            control(),
            active,
            worktree_clean=True,
            current_branch="agent/foundation",
            current_head="1" * 40,
            remote_authoritative_head="1" * 40,
            open_automation_prs=[],
            now=now,
        )
        self.assertEqual(result["result"], "ACTIVE_AGENT_DETECTED")
        self.assertEqual(result["active_run_id"], "interrupted-001")

    def test_remote_uncertainty_blocks_implementation(self) -> None:
        result = preflight.evaluate_preflight(
            control(),
            issue_state(),
            worktree_clean=True,
            current_branch="agent/foundation",
            current_head="1" * 40,
            remote_authoritative_head=None,
            open_automation_prs=[],
            remote_available=False,
        )
        self.assertEqual(result["result"], "LOCK_UNCERTAIN")


class QueueAndStopTests(unittest.TestCase):
    def test_owner_gate_does_not_select_replacement(self) -> None:
        result = select_work_item.select_work_item(
            control(),
            [item()],
            purpose="plan",
            control_state={
                "enabled": True,
                "current_owner_gate": "OWNER_DECISION_PENDING",
                "current_visual_gate": "NONE",
                "current_private_gate": "NONE",
            },
        )
        self.assertEqual(result["result"], "OWNER_GATE")
        self.assertNotIn("work_item_id", result)

    def test_visual_gate_is_report_only(self) -> None:
        result = select_work_item.select_work_item(
            control(),
            [item()],
            purpose="plan",
            control_state={
                "enabled": True,
                "current_owner_gate": "NONE",
                "current_visual_gate": "VISUAL_REVIEW_REQUIRED",
                "current_private_gate": "NONE",
            },
        )
        self.assertEqual(result["result"], "VISUAL_GATE")

    def test_private_gate_stops_selection(self) -> None:
        result = select_work_item.select_work_item(
            control(),
            [item()],
            purpose="plan",
            control_state={
                "enabled": True,
                "current_owner_gate": "NONE",
                "current_visual_gate": "NONE",
                "current_private_gate": "PRIVATE_DATA_REQUIRED",
            },
        )
        self.assertEqual(result["result"], "PRIVATE_DATA_REQUIRED")

    def test_no_safe_work_has_no_selected_item(self) -> None:
        result = select_work_item.select_work_item(
            control(), [], purpose="plan", control_state={"enabled": True}
        )
        self.assertEqual(result["result"], "NO_SAFE_WORK")
        self.assertNotIn("work_item_id", result)


class ScopeTests(unittest.TestCase):
    def implementation_control(self) -> dict[str, object]:
        value = deepcopy(control())
        value["mode"] = "IMPLEMENT_SAFE"
        return value

    def test_file_outside_allowed_paths_fails(self) -> None:
        result = validate_scope.evaluate_scope(
            self.implementation_control(),
            item(),
            changed_files=["other/file.txt"],
            changed_lines=1,
            patch_text="+x",
            branch_name="automation/synthetic-a2/run-test-001",
            run_id="run-test-001",
        )
        self.assertIn("OUTSIDE_ALLOWED_PATHS:other/file.txt", result["violations"])

    def test_src_and_include_are_always_forbidden(self) -> None:
        risky = item(allowed_paths=["**"])
        for path in ("src/world.c", "include/box2d/box2d.h"):
            result = validate_scope.evaluate_scope(
                self.implementation_control(),
                risky,
                changed_files=[path],
                changed_lines=1,
                patch_text="+x",
                branch_name="automation/synthetic-a2/run-test-001",
                run_id="run-test-001",
            )
            self.assertTrue(any(value.startswith("ALWAYS_FORBIDDEN") for value in result["violations"]))

    def test_threshold_change_causes_stop(self) -> None:
        threshold_item = item(allowed_paths=["tests/**"])
        result = validate_scope.evaluate_scope(
            self.implementation_control(),
            threshold_item,
            changed_files=["tests/fake_validator.py"],
            changed_lines=2,
            patch_text="-threshold = 0.01\n+threshold = 0.10",
            branch_name="automation/synthetic-a2/run-test-001",
            run_id="run-test-001",
        )
        self.assertIn("THRESHOLD_CHANGE_STOP", result["violations"])

    def test_policy_self_modification_is_blocked(self) -> None:
        policy_item = item(allowed_paths=["**"])
        result = validate_scope.evaluate_scope(
            self.implementation_control(),
            policy_item,
            changed_files=[".automation/CONTROL.yaml"],
            changed_lines=1,
            patch_text="+change",
            branch_name="automation/synthetic-a2/run-test-001",
            run_id="run-test-001",
        )
        self.assertTrue(any(value.startswith("SELF_POLICY_MODIFICATION") for value in result["violations"]))

    def test_small_synthetic_scope_passes(self) -> None:
        result = validate_scope.evaluate_scope(
            self.implementation_control(),
            item(),
            changed_files=["sandbox/change.txt"],
            changed_lines=2,
            patch_text="+synthetic",
            branch_name="automation/synthetic-a2/run-test-001",
            run_id="run-test-001",
        )
        self.assertEqual(result["result"], "SCOPE_VALID")


class PostflightAndSimulationTests(unittest.TestCase):
    def test_postflight_detects_base_moved(self) -> None:
        impl = deepcopy(control())
        impl["mode"] = "IMPLEMENT_SAFE"
        result = postflight.evaluate_postflight(
            impl,
            item(),
            expected_base_sha="1" * 40,
            observed_base_sha="1" * 40,
            remote_base_sha="2" * 40,
            scope_result={"result": "SCOPE_VALID"},
            tests=[{"name": "synthetic-test", "status": "PASS"}],
            commit_messages=["synthetic-a2: change"],
            changed_files=["sandbox/change.txt"],
            state_moved=False,
            state_docs_changed=False,
        )
        self.assertEqual(result["result"], "BASE_MOVED")

    def test_cloud_partial_validation_is_not_full_pass(self) -> None:
        impl = deepcopy(control())
        impl["mode"] = "IMPLEMENT_SAFE"
        result = postflight.evaluate_postflight(
            impl,
            item(required_tests=["synthetic-test", "windows-gate"]),
            expected_base_sha="1" * 40,
            observed_base_sha="1" * 40,
            remote_base_sha="1" * 40,
            scope_result={"result": "SCOPE_VALID"},
            tests=[
                {"name": "synthetic-test", "status": "PASS"},
                {"name": "windows-gate", "status": "UNAVAILABLE"},
            ],
            commit_messages=["synthetic-a2: change"],
            changed_files=["sandbox/change.txt"],
            state_moved=False,
            state_docs_changed=False,
        )
        self.assertEqual(result["result"], "PARTIAL_CLOUD_VALIDATION")
        self.assertIn("LOCAL_WINDOWS_GATE_PENDING", result["limitations"])
        self.assertFalse(result["allow_merge"])

    def test_three_required_scenarios(self) -> None:
        scenarios = simulate_scenarios.run(ROOT)
        self.assertEqual(scenarios["scenario_a"]["observed"], "NO_SAFE_WORK")
        self.assertTrue(scenarios["scenario_a"]["zero_changes"])
        self.assertTrue(scenarios["scenario_a"]["zero_commits"])
        self.assertEqual(scenarios["scenario_b"]["observed"], "VISUAL_GATE")
        self.assertTrue(scenarios["scenario_b"]["report_only"])
        self.assertEqual(
            scenarios["scenario_c"]["selection"]["result"], "WORK_ITEM_SELECTED"
        )
        self.assertEqual(scenarios["scenario_c"]["scope"]["result"], "SCOPE_VALID")
        self.assertFalse(scenarios["scenario_c"]["merge"])
        self.assertFalse(scenarios["scenario_c"]["repository_changes"])


if __name__ == "__main__":
    unittest.main()
