#!/usr/bin/env python3
"""Audit JV modifications to Box3D core against a checked-in manifest.

The comparison includes committed, staged, unstaged and untracked files under
the declared core scope. This tool proves ownership and bounded diff coverage;
it does not prove semantic equivalence or physical correctness.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "JOZZ_CORE_PATCHES.json"


def run_git(*args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(ROOT), *args], text=True, encoding="utf-8"
    ).strip()


def fail(message: str) -> int:
    print(f"core-delta: FAIL — {message}", file=sys.stderr)
    return 1


def valid_repo_path(raw: str, scopes: tuple[str, ...]) -> bool:
    path = PurePosixPath(raw)
    return (
        bool(raw)
        and not path.is_absolute()
        and ".." not in path.parts
        and any(path == PurePosixPath(scope) or PurePosixPath(scope) in path.parents for scope in scopes)
    )


def main() -> int:
    try:
        data = json.loads(MANIFEST.read_text("utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return fail(f"cannot read {MANIFEST.relative_to(ROOT)}: {exc}")

    if data.get("schema") != 1:
        return fail("unsupported manifest schema")
    base = data.get("comparison_base")
    upstream = data.get("upstream_reference")
    patches = data.get("patches")
    raw_scopes = data.get("scope")
    if not isinstance(base, str) or not isinstance(upstream, str):
        return fail("manifest needs comparison_base and upstream_reference")
    if not isinstance(patches, list) or not isinstance(raw_scopes, list) or not raw_scopes:
        return fail("manifest needs non-empty scope[] and patches[]")
    if not all(isinstance(scope, str) and scope and "/" not in scope.rstrip("/") for scope in raw_scopes):
        return fail("scope entries must be top-level repository directories")
    scopes = tuple(scope.rstrip("/") for scope in raw_scopes)

    try:
        run_git("cat-file", "-e", f"{base}^{{commit}}")
        run_git("cat-file", "-e", f"{upstream}^{{commit}}")
        subprocess.check_call(
            ["git", "-C", str(ROOT), "merge-base", "--is-ancestor", base, "HEAD"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        changed_raw = run_git("diff", "--name-only", base, "--", *scopes)
        numstat_raw = run_git("diff", "--numstat", base, "--", *scopes)
        untracked_raw = run_git(
            "ls-files", "--others", "--exclude-standard", "--", *scopes
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        return fail(f"git query failed or comparison_base is not an ancestor of HEAD: {exc}")

    untracked = {line for line in untracked_raw.splitlines() if line}
    changed = {line for line in changed_raw.splitlines() if line} | untracked
    owners: dict[str, str] = {}
    ids: set[str] = set()
    for patch in patches:
        if not isinstance(patch, dict):
            return fail("every patch entry must be an object")
        patch_id = patch.get("patch_id")
        files = patch.get("files")
        if not isinstance(patch_id, str) or not patch_id:
            return fail("patch without patch_id")
        if patch_id in ids:
            return fail(f"duplicate patch_id {patch_id}")
        ids.add(patch_id)
        if not isinstance(files, list) or not files:
            return fail(f"{patch_id} has no files")
        for item in files:
            if not isinstance(item, str) or not valid_repo_path(item, scopes):
                return fail(f"{patch_id} contains invalid/out-of-scope path: {item!r}")
            if item in owners:
                return fail(f"{item} owned by both {owners[item]} and {patch_id}")
            owners[item] = patch_id
            if not (ROOT / item).is_file():
                return fail(f"manifest file missing: {item}")
        for required in (
            "class",
            "status",
            "question",
            "owner",
            "activation_boundary",
            "zero_delta_contract",
            "upstream_update_dry_run",
        ):
            if not patch.get(required):
                return fail(f"{patch_id} missing {required}")
        owner = patch["owner"]
        if not isinstance(owner, str) or not (ROOT / owner).is_file():
            return fail(f"{patch_id} owner document missing: {owner!r}")

    unowned = sorted(changed - owners.keys())
    stale = sorted(owners.keys() - changed)
    if unowned:
        return fail("unowned core files: " + ", ".join(unowned))
    if stale:
        return fail("manifest lists unchanged files: " + ", ".join(stale))

    added = deleted = 0
    for line in numstat_raw.splitlines():
        if not line:
            continue
        a, d, _ = line.split("\t", 2)
        if a.isdigit():
            added += int(a)
        if d.isdigit():
            deleted += int(d)
    for relative in untracked:
        try:
            added += len((ROOT / relative).read_bytes().splitlines())
        except OSError as exc:
            return fail(f"cannot count untracked core file {relative}: {exc}")

    print(
        "core-delta: OK — "
        f"{len(patches)} patch(es), {len(changed)} file(s), +{added}/-{deleted}, "
        f"base {base[:7]}"
    )
    for patch in patches:
        owned_count = sum(1 for owner in owners.values() if owner == patch["patch_id"])
        print(
            f"- {patch['patch_id']} [{patch['class']}, {patch['status']}]: "
            f"{owned_count} file(s)"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
