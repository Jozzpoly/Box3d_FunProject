#!/usr/bin/env python3
"""Validate a proposed diff against one exact work-item scope."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
from typing import Any, Mapping, Sequence

from common import (
    AutomationContractError,
    load_control,
    load_json_document,
    machine_result,
    normalize_repo_path,
    path_matches,
    run_command,
    validate_work_item,
)

THRESHOLD_LINE = re.compile(
    r"^[+-](?![+-])(?=.*\b(?:threshold|tolerance|epsilon|limit|max|min|probe|assert)\b)(?=.*\d)",
    re.IGNORECASE,
)


def evaluate_scope(
    control: Mapping[str, Any],
    work_item: Mapping[str, Any],
    *,
    changed_files: Sequence[str],
    changed_lines: int,
    patch_text: str,
    branch_name: str,
    run_id: str,
) -> dict[str, Any]:
    item = validate_work_item(dict(work_item))
    violations: list[str] = []
    normalized = [normalize_repo_path(path) for path in changed_files]
    expected_branch = control["branch"]["format"].format(
        work_item_id=item["id"], run_id=run_id
    )
    if branch_name != expected_branch:
        violations.append("BRANCH_POLICY_VIOLATION")
    if branch_name in control["branch"]["owner_branches_forbidden"]:
        violations.append("OWNER_BRANCH_FORBIDDEN")
    for path in normalized:
        if path_matches(path, control["scope"]["always_forbidden_paths"]):
            violations.append(f"ALWAYS_FORBIDDEN:{path}")
        if path_matches(path, item["forbidden_paths"]):
            violations.append(f"WORK_ITEM_FORBIDDEN:{path}")
        if not path_matches(path, item["allowed_paths"]):
            violations.append(f"OUTSIDE_ALLOWED_PATHS:{path}")
        if path_matches(path, control["scope"]["protected_control_paths"]):
            violations.append(f"SELF_POLICY_MODIFICATION:{path}")
    max_files = min(
        item["maximum_scope"]["max_files"],
        control["scope"]["maximum_default_files"],
    )
    max_lines = min(
        item["maximum_scope"]["max_changed_lines"],
        control["scope"]["maximum_default_changed_lines"],
    )
    if len(normalized) > max_files:
        violations.append(f"TOO_MANY_FILES:{len(normalized)}>{max_files}")
    if changed_lines > max_lines:
        violations.append(f"TOO_MANY_CHANGED_LINES:{changed_lines}>{max_lines}")
    sensitive = any(
        path_matches(path, control["scope"]["threshold_sensitive_patterns"])
        for path in normalized
    )
    if sensitive and any(THRESHOLD_LINE.search(line) for line in patch_text.splitlines()):
        violations.append("THRESHOLD_CHANGE_STOP")
    if not normalized:
        return machine_result("NO_MATERIAL_CHANGE", work_item_id=item["id"])
    if violations:
        return machine_result(
            "POLICY_CONFLICT",
            work_item_id=item["id"],
            violations=sorted(set(violations)),
            changed_files=normalized,
            changed_lines=changed_lines,
        )
    return machine_result(
        "SCOPE_VALID",
        work_item_id=item["id"],
        branch=branch_name,
        changed_files=normalized,
        changed_lines=changed_lines,
        max_files=max_files,
        max_changed_lines=max_lines,
    )


def _git_diff(repo: Path, base: str, head: str) -> tuple[list[str], int, str]:
    names = run_command(["git", "diff", "--name-only", f"{base}...{head}"], repo)
    if names.returncode != 0:
        raise AutomationContractError(f"git diff names failed: {names.stderr}")
    numstat = run_command(["git", "diff", "--numstat", f"{base}...{head}"], repo)
    if numstat.returncode != 0:
        raise AutomationContractError(f"git diff numstat failed: {numstat.stderr}")
    patch = run_command(["git", "diff", "--unified=0", f"{base}...{head}"], repo)
    if patch.returncode != 0:
        raise AutomationContractError(f"git diff patch failed: {patch.stderr}")
    files = [line for line in names.stdout.splitlines() if line.strip()]
    changed_lines = 0
    for line in numstat.stdout.splitlines():
        parts = line.split("\t")
        if len(parts) >= 2:
            for value in parts[:2]:
                if value.isdigit():
                    changed_lines += int(value)
    return files, changed_lines, patch.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--work-item", type=Path, required=True)
    parser.add_argument("--base", required=True)
    parser.add_argument("--head", default="HEAD")
    parser.add_argument("--branch", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--changed-files-fixture", type=Path)
    parser.add_argument("--patch-fixture", type=Path)
    parser.add_argument("--changed-lines", type=int)
    args = parser.parse_args()
    repo = args.repo.resolve()
    try:
        control = load_control(repo / ".automation/CONTROL.yaml")
        item = validate_work_item(load_json_document(args.work_item))
        if args.changed_files_fixture:
            files_doc = load_json_document(args.changed_files_fixture)
            if not isinstance(files_doc, list) or not all(isinstance(x, str) for x in files_doc):
                raise AutomationContractError("changed-files fixture must be a string list")
            files = files_doc
            patch = args.patch_fixture.read_text(encoding="utf-8") if args.patch_fixture else ""
            lines = args.changed_lines if args.changed_lines is not None else 0
        else:
            files, lines, patch = _git_diff(repo, args.base, args.head)
        result = evaluate_scope(
            control,
            item,
            changed_files=files,
            changed_lines=lines,
            patch_text=patch,
            branch_name=args.branch,
            run_id=args.run_id,
        )
        print(json.dumps(result, sort_keys=True))
        return 0 if result["result"] in ("SCOPE_VALID", "NO_MATERIAL_CHANGE") else 3
    except (AutomationContractError, OSError) as exc:
        print(json.dumps(machine_result("POLICY_CONFLICT", error=str(exc)), sort_keys=True))
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
