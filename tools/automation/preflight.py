#!/usr/bin/env python3
"""Read-only recurring-agent preflight.

The command never edits the repository, issue, branch or PR. If remote state or
lease state cannot be confirmed, implementation is denied.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
from typing import Any, Mapping, Sequence

from common import (
    AutomationContractError,
    extract_control_issue_state,
    load_control,
    load_json_document,
    machine_result,
    parse_utc,
    run_command,
    utc_now,
    validate_control_issue_state,
)


def evaluate_preflight(
    control: Mapping[str, Any],
    state: Mapping[str, Any],
    *,
    worktree_clean: bool,
    current_branch: str,
    current_head: str,
    remote_authoritative_head: str | None,
    open_automation_prs: Sequence[str],
    now: datetime | None = None,
    remote_available: bool = True,
) -> dict[str, Any]:
    now = now or utc_now()
    mode = control["mode"]
    if state["mode"] != mode:
        return machine_result(
            "POLICY_CONFLICT",
            reason="CONTROL_FILE_AND_ISSUE_MODE_DIFFER",
            file_mode=mode,
            issue_mode=state["mode"],
            implementation_allowed=False,
        )
    if not state["enabled"] or mode == "DISABLED":
        return machine_result("NO_SAFE_WORK", reason="AUTOMATION_DISABLED", implementation_allowed=False)
    if not worktree_clean:
        return machine_result("ACTIVE_AGENT_DETECTED", reason="WORKTREE_DIRTY", implementation_allowed=False)
    if not remote_available or remote_authoritative_head is None:
        return machine_result("LOCK_UNCERTAIN", reason="REMOTE_STATE_UNAVAILABLE", implementation_allowed=False)
    if remote_authoritative_head != state["authoritative_head"]:
        return machine_result(
            "BASE_MOVED",
            expected=state["authoritative_head"],
            observed=remote_authoritative_head,
            implementation_allowed=False,
        )
    if len(open_automation_prs) >= control["execution"]["max_open_automation_prs"]:
        return machine_result(
            "ACTIVE_AGENT_DETECTED",
            reason="AUTOMATION_PR_ALREADY_OPEN",
            open_automation_prs=list(open_automation_prs),
            implementation_allowed=False,
        )
    if current_branch in control["branch"]["owner_branches_forbidden"]:
        return machine_result(
            "POLICY_CONFLICT",
            reason="RUNNING_ON_OWNER_PROTECTED_BRANCH",
            current_branch=current_branch,
            implementation_allowed=False,
        )
    active_run = state["active_run_id"]
    if active_run is not None:
        expires = parse_utc(state["lease_expires_at"])
        if expires is None:
            return machine_result("LOCK_UNCERTAIN", reason="LEASE_INCOMPLETE", implementation_allowed=False)
        if expires > now:
            return machine_result(
                "ACTIVE_AGENT_DETECTED",
                active_run_id=active_run,
                lease_expires_at=state["lease_expires_at"],
                implementation_allowed=False,
            )
        return machine_result(
            "LOCK_UNAVAILABLE",
            reason="STALE_LEASE_REQUIRES_OWNER_REVIEW",
            stale_run_id=active_run,
            lease_expires_at=state["lease_expires_at"],
            implementation_allowed=False,
        )
    for field, result in (
        ("current_owner_gate", "OWNER_GATE"),
        ("current_visual_gate", "VISUAL_GATE"),
        ("current_private_gate", "PRIVATE_DATA_REQUIRED"),
    ):
        value = state[field]
        if value not in ("NONE", "CLEAR"):
            return machine_result(result, gate=value, implementation_allowed=False)
    implementation_allowed = mode == "IMPLEMENT_SAFE"
    return machine_result(
        "PREFLIGHT_OK" if implementation_allowed else "PREFLIGHT_OK_PLAN_ONLY",
        mode=mode,
        active_campaign=state["active_campaign"],
        authoritative_branch=state["authoritative_branch"],
        authoritative_head=state["authoritative_head"],
        current_branch=current_branch,
        current_head=current_head,
        implementation_allowed=implementation_allowed,
    )


def _discover_repo(start: Path) -> Path:
    result = run_command(["git", "rev-parse", "--show-toplevel"], start)
    if result.returncode != 0:
        raise AutomationContractError("not inside a Git worktree")
    return Path(result.stdout).resolve()


def _load_issue_with_gh(repo: Path, title: str) -> dict[str, Any]:
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
        raise AutomationContractError("GitHub control issue cannot be queried")
    values = json.loads(listed.stdout or "[]")
    exact = [item for item in values if item.get("title") == title]
    if len(exact) != 1:
        raise AutomationContractError("exactly one open automation control issue is required")
    viewed = run_command(
        ["gh", "issue", "view", str(exact[0]["number"]), "--json", "body,number,title"],
        repo,
    )
    if viewed.returncode != 0:
        raise AutomationContractError("automation control issue cannot be read")
    issue = json.loads(viewed.stdout)
    return extract_control_issue_state(issue["body"])


def _open_automation_prs(repo: Path) -> list[str]:
    result = run_command(
        ["gh", "pr", "list", "--state", "open", "--json", "number,headRefName,isDraft"],
        repo,
    )
    if result.returncode != 0:
        raise AutomationContractError("open PR state cannot be queried")
    values = json.loads(result.stdout or "[]")
    return [
        f"#{item['number']}:{item['headRefName']}"
        for item in values
        if str(item.get("headRefName", "")).startswith("automation/")
    ]


def _remote_head(repo: Path, branch: str) -> str | None:
    result = run_command(["git", "ls-remote", "origin", f"refs/heads/{branch}"], repo)
    if result.returncode != 0 or not result.stdout:
        return None
    return result.stdout.split()[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--control-issue-snapshot", type=Path)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--open-automation-pr", action="append", default=[])
    args = parser.parse_args()
    try:
        repo = _discover_repo(args.repo)
        control = load_control(repo / ".automation/CONTROL.yaml")
        status = run_command(["git", "status", "--porcelain=v1"], repo)
        branch = run_command(["git", "branch", "--show-current"], repo)
        head = run_command(["git", "rev-parse", "HEAD"], repo)
        if any(result.returncode != 0 for result in (status, branch, head)):
            raise AutomationContractError("local Git state cannot be read")
        remote_available = not args.offline
        if args.control_issue_snapshot:
            raw = load_json_document(args.control_issue_snapshot)
            state = validate_control_issue_state(raw)
        elif args.offline:
            raise AutomationContractError("offline preflight requires a control issue snapshot")
        else:
            fetch = run_command(["git", "fetch", "--prune", "origin"], repo, timeout=180)
            if fetch.returncode != 0:
                remote_available = False
            state = _load_issue_with_gh(repo, control["control_issue"]["title"])
        remote_head = (
            state["authoritative_head"]
            if args.offline
            else _remote_head(repo, state["authoritative_branch"])
        )
        prs = args.open_automation_pr or ([] if args.offline else _open_automation_prs(repo))
        result = evaluate_preflight(
            control,
            state,
            worktree_clean=not bool(status.stdout),
            current_branch=branch.stdout,
            current_head=head.stdout,
            remote_authoritative_head=remote_head,
            open_automation_prs=prs,
            remote_available=remote_available,
        )
        print(json.dumps(result, sort_keys=True))
        return 0 if result["result"].startswith("PREFLIGHT_OK") else 3
    except (AutomationContractError, OSError, json.JSONDecodeError) as exc:
        print(json.dumps(machine_result("LOCK_UNCERTAIN", error=str(exc), implementation_allowed=False), sort_keys=True))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
