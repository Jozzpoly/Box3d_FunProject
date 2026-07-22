#!/usr/bin/env python3
"""Claim, inspect and release the GitHub control-issue lease.

GitHub Issue edits are not transactional compare-and-swap. Therefore a claim is
accepted only after two stable rereads. A stale lease is never stolen
implicitly; owner intervention is required.
"""
from __future__ import annotations

import argparse
from datetime import timedelta
import json
from pathlib import Path
import tempfile
import time
from typing import Any, Mapping

from common import (
    AutomationContractError,
    extract_control_issue_state,
    load_control,
    machine_result,
    parse_utc,
    replace_control_issue_state,
    run_command,
    utc_now,
    validate_control_issue_state,
)


def evaluate_claim(
    state: Mapping[str, Any],
    *,
    run_id: str,
    exact_base_sha: str,
    lease_minutes: int,
) -> tuple[dict[str, Any], dict[str, Any] | None]:
    current = validate_control_issue_state(dict(state))
    now = utc_now()
    if not current["enabled"]:
        return machine_result("NO_SAFE_WORK", reason="AUTOMATION_DISABLED"), None
    if current["authoritative_head"] != exact_base_sha:
        return machine_result(
            "BASE_MOVED",
            expected=exact_base_sha,
            observed=current["authoritative_head"],
        ), None
    if current["active_run_id"] is not None:
        expires = parse_utc(current["lease_expires_at"])
        if current["active_run_id"] == run_id and expires and expires > now:
            return machine_result("LEASE_HELD_BY_CALLER", run_id=run_id), current
        if expires and expires <= now:
            return machine_result(
                "LOCK_UNAVAILABLE",
                reason="STALE_LEASE_REQUIRES_OWNER_REVIEW",
                stale_run_id=current["active_run_id"],
            ), None
        return machine_result(
            "ACTIVE_AGENT_DETECTED",
            active_run_id=current["active_run_id"],
            lease_expires_at=current["lease_expires_at"],
        ), None
    claimed = dict(current)
    claimed.update(
        {
            "active_run_id": run_id,
            "lease_started_at": now.isoformat().replace("+00:00", "Z"),
            "lease_expires_at": (now + timedelta(minutes=lease_minutes))
            .isoformat()
            .replace("+00:00", "Z"),
            "active_branch": None,
            "active_pr": None,
            "last_result": "RUN_ACTIVE",
        }
    )
    return machine_result("LEASE_CLAIM_PROPOSED", run_id=run_id), claimed


def evaluate_release(
    state: Mapping[str, Any], *, run_id: str, final_result: str
) -> tuple[dict[str, Any], dict[str, Any] | None]:
    current = validate_control_issue_state(dict(state))
    if current["active_run_id"] != run_id:
        return machine_result(
            "LOCK_UNCERTAIN",
            reason="RUN_DOES_NOT_OWN_LEASE",
            active_run_id=current["active_run_id"],
        ), None
    released = dict(current)
    released.update(
        {
            "active_run_id": None,
            "lease_started_at": None,
            "lease_expires_at": None,
            "active_branch": None,
            "active_pr": None,
            "last_completed_run": run_id,
            "last_result": final_result,
        }
    )
    return machine_result("LEASE_RELEASE_PROPOSED", run_id=run_id), released


def _find_issue(repo: Path, title: str) -> tuple[int, str]:
    listed = run_command(
        [
            "gh",
            "issue",
            "list",
            "--state",
            "open",
            "--search",
            f'\"{title}\" in:title',
            "--json",
            "number,title",
            "--limit",
            "10",
        ],
        repo,
    )
    if listed.returncode != 0:
        raise AutomationContractError("control issue lookup failed")
    values = json.loads(listed.stdout or "[]")
    exact = [item for item in values if item.get("title") == title]
    if len(exact) != 1:
        raise AutomationContractError("exactly one open control issue is required")
    return int(exact[0]["number"]), title


def _read_issue(repo: Path, number: int) -> tuple[str, dict[str, Any]]:
    viewed = run_command(
        ["gh", "issue", "view", str(number), "--json", "body,title,number"], repo
    )
    if viewed.returncode != 0:
        raise AutomationContractError("control issue read failed")
    document = json.loads(viewed.stdout)
    return document["body"], extract_control_issue_state(document["body"])


def _write_issue(repo: Path, number: int, body: str) -> None:
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False, suffix=".md") as handle:
        handle.write(body)
        temp_path = Path(handle.name)
    try:
        edited = run_command(
            ["gh", "issue", "edit", str(number), "--body-file", str(temp_path)], repo
        )
        if edited.returncode != 0:
            raise AutomationContractError("control issue update failed")
    finally:
        temp_path.unlink(missing_ok=True)


def _stable_confirm(
    repo: Path,
    number: int,
    expected: Mapping[str, Any],
    settle_seconds: int,
) -> bool:
    first_body, first = _read_issue(repo, number)
    if first != dict(expected):
        return False
    time.sleep(settle_seconds)
    second_body, second = _read_issue(repo, number)
    return second == dict(expected) and first_body == second_body


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("inspect", "claim", "release"))
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--run-id")
    parser.add_argument("--exact-base-sha")
    parser.add_argument("--final-result", default="NO_MATERIAL_CHANGE")
    args = parser.parse_args()
    repo = args.repo.resolve()
    try:
        control = load_control(repo / ".automation/CONTROL.yaml")
        issue_number, _ = _find_issue(repo, control["control_issue"]["title"])
        body, state = _read_issue(repo, issue_number)
        if args.command == "inspect":
            print(json.dumps(machine_result("LEASE_INSPECTED", issue=issue_number, state=state), sort_keys=True))
            return 0
        if not args.run_id:
            raise AutomationContractError("--run-id is required")
        if args.command == "claim":
            if not args.exact_base_sha:
                raise AutomationContractError("--exact-base-sha is required for claim")
            result, proposed = evaluate_claim(
                state,
                run_id=args.run_id,
                exact_base_sha=args.exact_base_sha,
                lease_minutes=control["control_issue"]["lease_minutes"],
            )
        else:
            result, proposed = evaluate_release(
                state, run_id=args.run_id, final_result=args.final_result
            )
        if proposed is None:
            print(json.dumps(result, sort_keys=True))
            return 3
        _write_issue(repo, issue_number, replace_control_issue_state(body, proposed))
        stable = _stable_confirm(
            repo,
            issue_number,
            proposed,
            control["control_issue"]["claim_settle_seconds"],
        )
        if not stable:
            print(json.dumps(machine_result("LOCK_UNCERTAIN", issue=issue_number), sort_keys=True))
            return 4
        code = "LEASE_ACQUIRED" if args.command == "claim" else "LEASE_RELEASED"
        print(json.dumps(machine_result(code, issue=issue_number, run_id=args.run_id), sort_keys=True))
        return 0
    except (AutomationContractError, OSError, json.JSONDecodeError) as exc:
        print(json.dumps(machine_result("LOCK_UNCERTAIN", error=str(exc)), sort_keys=True))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
