#!/usr/bin/env python3
"""Produce a read-only tracked-file and remote-branch forensic snapshot.

The snapshot contains repository metadata only. It never reads owner-local ignored
files, scan payloads, credentials or private paths outside the checkout.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any, Iterable

ROOT = Path(__file__).resolve().parents[2]
INVENTORY_PATH = ROOT / "docs" / "PROJECT_INVENTORY.json"


def _run_git(arguments: list[str], *, timeout: int = 60) -> str:
    completed = subprocess.run(
        ["git", *arguments],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip() or "unknown git failure"
        raise RuntimeError(f"git {' '.join(arguments)} failed: {detail}")
    return completed.stdout


def _load_inventory() -> dict[str, Any]:
    value = json.loads(INVENTORY_PATH.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("project inventory root must be an object")
    return value


def _classify_path(path: str) -> str:
    if path.startswith(("src/", "include/")):
        return "upstream-engine-core"
    if path.startswith(("test/", "benchmark/", "shared/", "data/", "extern/")):
        return "upstream-support"
    if path in {"CMakeLists.txt", "LICENSE", "SECURITY.md"} or path.startswith("cmake/"):
        return "upstream-build-and-policy"
    if path.startswith("samples/host/") or path in {
        "samples/main.cpp",
        "samples/sample.cpp",
        "samples/sample.h",
        "samples/CMakeLists.txt",
    }:
        return "shared-native-host"
    if path.startswith("samples/sample_") and path != "samples/sample_jozz_vehicle_lab.cpp":
        return "upstream-samples"
    if path.startswith(("samples/jozz_vehicle_", "samples/validation/")):
        return "vehicle-physics-and-rig"
    if path.startswith(("assets/contracts/", "assets/vehicle_presets/", "assets/source/")):
        return "vehicle-assets-and-contracts"
    if path.startswith("samples/jozz_scan_preview_"):
        return "scan-source-geometry-preview-v1"
    if path.startswith(("tools/scan_pipeline/", "tests/scan_pipeline/", "docs/scan_import/")):
        return "scan-pipeline"
    if path.startswith("docs/PHOTOGRAMMETRY_"):
        return "scan-historical-evidence"
    if path == ".github/workflows/p1-scan-inspector.yml":
        return "scan-ci"
    if path.startswith((".automation/", "tools/automation/", "tests/automation/")):
        return "automation-and-governance"
    if path in {
        ".github/workflows/automation-foundation.yml",
        ".github/PULL_REQUEST_TEMPLATE/automation.md",
    }:
        return "automation-and-governance"
    if path.startswith(("tools/project/", "tests/project/")):
        return "repository-governance"
    if path in {
        "AGENTS.md",
        "AI_PROJECT_MEMORY.md",
        "CONTRIBUTING.md",
        "README.md",
        ".github/workflows/repository-governance.yml",
        ".github/PULL_REQUEST_TEMPLATE/manual.md",
        "docs/README.md",
        "docs/REPOSITORY_STRUCTURE_PL.md",
        "docs/PROJECT_OPERATING_PLAN_PL.md",
        "docs/PROJECT_CHARTER_PL.md",
        "docs/PROJECT_REFOUNDATION_AUDIT_2026_07_22_PL.md",
        "docs/PROJECT_FORENSIC_INVENTORY_2026_07_22_PL.md",
        "docs/PROJECT_INVENTORY.json",
    }:
        return "repository-governance"
    if path in {"README_FOR_AGENTS.md", "docs/CURRENT_STATE_INDEX_PL.md", "docs/TECH_DEBT_PL.md"}:
        return "vehicle-documentation"
    if path.startswith(("docs/SUBSYSTEM_", "docs/M7_", "docs/M8_", "docs/M9_")):
        return "vehicle-documentation"
    if path == "docs/CHECKPOINTS_PL.md" or path.startswith(("docs/archive/", "docs/adr/")):
        return "historical-documentation"
    if path.startswith("docs/"):
        return "unclassified-documentation"
    if path.startswith("assets/"):
        return "unclassified-assets"
    if path.startswith(".github/"):
        return "unclassified-github"
    if path.startswith("tools/"):
        return "unclassified-tools"
    if path.startswith("tests/"):
        return "unclassified-tests"
    return "unclassified-root-or-other"


def _tracked_paths() -> list[str]:
    output = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        timeout=60,
    )
    if output.returncode != 0:
        detail = output.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"git ls-files failed: {detail}")
    return sorted(
        item.decode("utf-8", errors="strict")
        for item in output.stdout.split(b"\0")
        if item
    )


def _parse_remote_refs(output: str, prefix: str) -> list[dict[str, str]]:
    records: list[dict[str, str]] = []
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        parts = line.split("\t")
        if len(parts) != 2:
            raise ValueError(f"unexpected ls-remote line: {line!r}")
        sha, ref = parts
        if not ref.startswith(prefix):
            continue
        name = ref[len(prefix) :]
        records.append({"name": name, "sha": sha})
    return sorted(records, key=lambda record: record["name"])


def _remote_heads(remote: str) -> list[dict[str, str]]:
    return _parse_remote_refs(_run_git(["ls-remote", "--heads", remote]), "refs/heads/")


def _remote_tags(remote: str) -> list[dict[str, str]]:
    records = _parse_remote_refs(_run_git(["ls-remote", "--tags", remote]), "refs/tags/")
    return [record for record in records if not record["name"].endswith("^{}")]


def _lineage_by_branch(inventory: dict[str, Any]) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    lineage = inventory.get("pullRequestLineage", [])
    if not isinstance(lineage, list):
        return result
    for record in lineage:
        if not isinstance(record, dict):
            continue
        branch = record.get("branch")
        if not isinstance(branch, str):
            continue
        existing = result.get(branch)
        if existing is None:
            result[branch] = {
                "prs": [record.get("pr")],
                "roles": [record.get("role")],
                "retention": record.get("retention"),
            }
        else:
            existing["prs"].append(record.get("pr"))
            existing["roles"].append(record.get("role"))
            if existing.get("retention") != record.get("retention"):
                existing["retention"] = "MIXED_REVIEW_REQUIRED"
    return result


def _annotate_branches(
    branches: list[dict[str, str]], inventory: dict[str, Any]
) -> list[dict[str, Any]]:
    lineage = _lineage_by_branch(inventory)
    reduction = inventory.get("branchReduction", {})
    preferred = set(reduction.get("preferredFinalBranches", [])) if isinstance(reduction, dict) else set()
    annotated: list[dict[str, Any]] = []
    for branch in branches:
        name = branch["name"]
        record: dict[str, Any] = dict(branch)
        record["preferredKeep"] = name in preferred or name in {"main", "jozz-vehicle-sandbox-m0"}
        if name in lineage:
            record.update(lineage[name])
        elif name == inventory.get("snapshot", {}).get("auditBranch"):
            record["retention"] = "DELETE_AFTER_INTEGRATION"
            record["roles"] = ["current re-foundation review branch"]
            record["prs"] = [inventory.get("snapshot", {}).get("refoundationPr")]
        elif name == inventory.get("snapshot", {}).get("authoritativeProductBranch"):
            record["retention"] = "KEEP_ACTIVE"
            record["roles"] = ["current authoritative product integration"]
            record["prs"] = [inventory.get("snapshot", {}).get("activeProductPr")]
        elif name in {"main", "jozz-vehicle-sandbox-m0"}:
            record["retention"] = "KEEP_BASELINE"
            record["roles"] = ["durable baseline"]
            record["prs"] = []
        else:
            record["retention"] = "UNCLASSIFIED_REMOTE_BRANCH"
            record["roles"] = []
            record["prs"] = []
        annotated.append(record)
    return annotated


def build_snapshot(remote: str, require_remote: bool) -> dict[str, Any]:
    inventory = _load_inventory()
    tracked = _tracked_paths()
    groups: dict[str, list[str]] = {}
    for path in tracked:
        groups.setdefault(_classify_path(path), []).append(path)

    remote_error: str | None = None
    branches: list[dict[str, str]] = []
    tags: list[dict[str, str]] = []
    try:
        branches = _remote_heads(remote)
        tags = _remote_tags(remote)
    except Exception as exc:
        remote_error = str(exc)
        if require_remote:
            raise

    reduction = inventory.get("branchReduction", {})
    hard_max = reduction.get("ownerTargetMaximum") if isinstance(reduction, dict) else None
    preferred_count = reduction.get("preferredFinalCount") if isinstance(reduction, dict) else None
    annotated_branches = _annotate_branches(branches, inventory)
    unclassified_branches = [
        record["name"]
        for record in annotated_branches
        if record.get("retention") == "UNCLASSIFIED_REMOTE_BRANCH"
    ]
    unclassified_paths = {
        group: paths
        for group, paths in groups.items()
        if group.startswith("unclassified-")
    }

    return {
        "schemaVersion": 1,
        "repository": "Jozzpoly/Box3d_FunProject",
        "generatedFrom": {
            "head": _run_git(["rev-parse", "HEAD"]).strip(),
            "githubSha": os.environ.get("GITHUB_SHA"),
            "githubHeadRef": os.environ.get("GITHUB_HEAD_REF"),
            "githubBaseRef": os.environ.get("GITHUB_BASE_REF"),
        },
        "trackedTree": {
            "fileCount": len(tracked),
            "groupCounts": {group: len(paths) for group, paths in sorted(groups.items())},
            "groups": {group: paths for group, paths in sorted(groups.items())},
            "unclassifiedPaths": unclassified_paths,
            "coverageComplete": not unclassified_paths,
        },
        "remoteRefs": {
            "remote": remote,
            "enumerationSucceeded": remote_error is None,
            "error": remote_error,
            "branchCount": len(branches),
            "hardMaximum": hard_max,
            "preferredFinalCount": preferred_count,
            "overHardMaximum": isinstance(hard_max, int) and len(branches) > hard_max,
            "branches": annotated_branches,
            "unclassifiedBranches": unclassified_branches,
            "tagCount": len(tags),
            "tags": tags,
            "deletionAuthorized": False,
        },
        "privacy": {
            "ignoredFilesRead": False,
            "ownerLocalPathsRead": False,
            "scanPayloadsRead": False,
            "credentialsStored": False,
        },
    }


def _write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def parse_args(argv: Iterable[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--remote", default="origin")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-remote", action="store_true")
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        snapshot = build_snapshot(args.remote, args.require_remote)
        _write_json(args.output, snapshot)
    except Exception as exc:
        print(f"REPOSITORY_FORENSIC_SNAPSHOT_FAIL: {exc}", file=sys.stderr)
        return 1

    remote = snapshot["remoteRefs"]
    tree = snapshot["trackedTree"]
    print("REPOSITORY_FORENSIC_SNAPSHOT_PASS")
    print(f"tracked_files={tree['fileCount']} coverage_complete={str(tree['coverageComplete']).lower()}")
    print(
        "remote_branches="
        f"{remote['branchCount']} hard_max={remote['hardMaximum']} "
        f"preferred={remote['preferredFinalCount']} over_max={str(remote['overHardMaximum']).lower()}"
    )
    print(f"unclassified_remote_branches={len(remote['unclassifiedBranches'])}")
    print(f"output={args.output.as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
