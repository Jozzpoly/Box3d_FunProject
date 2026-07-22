#!/usr/bin/env python3
"""Fail-closed postflight for a single autonomous work item."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Mapping, Sequence

from common import (
    AutomationContractError,
    load_control,
    load_json_document,
    machine_result,
    validate_work_item,
)


def evaluate_postflight(
    control: Mapping[str, Any],
    work_item: Mapping[str, Any],
    *,
    expected_base_sha: str,
    observed_base_sha: str,
    remote_base_sha: str,
    scope_result: Mapping[str, Any],
    tests: Sequence[Mapping[str, Any]],
    commit_messages: Sequence[str],
    changed_files: Sequence[str],
    state_moved: bool,
    state_docs_changed: bool,
) -> dict[str, Any]:
    item = validate_work_item(dict(work_item))
    if control["mode"] != "IMPLEMENT_SAFE":
        return machine_result("POLICY_CONFLICT", reason="IMPLEMENTATION_MODE_NOT_ENABLED")
    if expected_base_sha != observed_base_sha or expected_base_sha != remote_base_sha:
        return machine_result(
            "BASE_MOVED",
            expected=expected_base_sha,
            observed=observed_base_sha,
            remote=remote_base_sha,
        )
    if scope_result.get("result") != "SCOPE_VALID":
        return machine_result("POLICY_CONFLICT", reason="SCOPE_NOT_VALID", scope=dict(scope_result))
    if not 1 <= len(commit_messages) <= 3:
        return machine_result("POLICY_CONFLICT", reason="COMMIT_SERIES_NOT_SMALL")
    if any(item["id"] not in message for message in commit_messages):
        return machine_result("POLICY_CONFLICT", reason="COMMIT_MESSAGE_MISSING_WORK_ITEM_ID")
    if state_docs_changed and not state_moved:
        return machine_result("POLICY_CONFLICT", reason="STATE_DOC_CHANGED_WITHOUT_STATE_MOVE")
    required_by_item = set(item["required_tests"])
    seen: set[str] = set()
    unavailable: list[str] = []
    failed: list[str] = []
    for test in tests:
        if not isinstance(test, Mapping):
            raise AutomationContractError("test result must be an object")
        name = test.get("name")
        status = test.get("status")
        if not isinstance(name, str) or status not in ("PASS", "FAIL", "UNAVAILABLE"):
            raise AutomationContractError("test result identity is invalid")
        seen.add(name)
        if status == "FAIL":
            failed.append(name)
        elif status == "UNAVAILABLE":
            unavailable.append(name)
    missing = sorted(required_by_item - seen)
    if failed:
        return machine_result("CI_PENDING", reason="TEST_FAILURE", failed=sorted(failed))
    if missing:
        return machine_result("CI_PENDING", reason="REQUIRED_TEST_MISSING", missing=missing)
    status = "DRAFT_PR_READY"
    limitations: list[str] = []
    if unavailable:
        status = "PARTIAL_CLOUD_VALIDATION"
        limitations.extend(sorted(unavailable))
        if any("windows" in name.lower() or "gate" in name.lower() for name in unavailable):
            limitations.append("LOCAL_WINDOWS_GATE_PENDING")
    return machine_result(
        status,
        work_item_id=item["id"],
        exact_base_sha=expected_base_sha,
        files_changed=list(changed_files),
        commits=list(commit_messages),
        tests=[dict(test) for test in tests],
        limitations=sorted(set(limitations)),
        require_draft_pr=True,
        allow_merge=False,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--work-item", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve()
    try:
        control = load_control(repo / ".automation/CONTROL.yaml")
        work_item = validate_work_item(load_json_document(args.work_item))
        value = load_json_document(args.input)
        required = {
            "expected_base_sha",
            "observed_base_sha",
            "remote_base_sha",
            "scope_result",
            "tests",
            "commit_messages",
            "changed_files",
            "state_moved",
            "state_docs_changed",
        }
        if not isinstance(value, dict) or set(value) != required:
            raise AutomationContractError("postflight input fields mismatch")
        result = evaluate_postflight(control, work_item, **value)
        print(json.dumps(result, sort_keys=True))
        return 0 if result["result"] in ("DRAFT_PR_READY", "PARTIAL_CLOUD_VALIDATION") else 3
    except (AutomationContractError, OSError) as exc:
        print(json.dumps(machine_result("POLICY_CONFLICT", error=str(exc)), sort_keys=True))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
