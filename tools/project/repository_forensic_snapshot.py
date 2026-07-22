#!/usr/bin/env python3
"""Create a read-only tracked-tree and remote-ref forensic snapshot.

Only tracked repository metadata and remote ref names/SHAs are read. Ignored owner
files, private scan payloads, credentials and local paths outside the checkout are
never inspected.
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
BRANCH_PLAN_PATH = ROOT / "docs" / "BRANCH_RETENTION_PLAN_2026_07_22.json"
DYNAMIC_CURRENT_POLICY = "DYNAMIC_CURRENT_PROJECT_HEAD_UNTIL_INTEGRATION"

ROOT_BUILD_AND_HYGIENE = {
    ".clang-format",
    ".gitattributes",
    ".gitignore",
    "CMakeLists.txt",
    "CMakePresets.json",
    "LICENSE",
    "SECURITY.md",
    "build.sh",
    "build_vs2026.bat",
    "deploy_docs.sh",
}
SHARED_RENDERER_FILES = {
    "samples/earcut.h",
    "samples/jsmn.h",
    "samples/mesh_loader.cpp",
    "samples/mesh_loader.h",
    "samples/tiny_obj_loader.h",
}
VEHICLE_TOOL_FILES = {
    "tools/asset_audit.py",
    "tools/asset_contract_audit.py",
    "tools/doc_drift_check.ps1",
    "tools/gate.ps1",
    "tools/quad_shot.ps1",
}
UPSTREAM_DOC_FILES = {"docs/CMakeLists.txt", "docs/extra.css", "docs/layout.xml"}
UPSTREAM_GITHUB_FILES = {
    ".github/FUNDING.yml",
    ".github/issue_template.md",
    ".github/workflows/build.yml",
}
REPOSITORY_GOVERNANCE_FILES = {
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
    "docs/BRANCH_RETENTION_PLAN_2026_07_22.json",
    "docs/DOCUMENT_LIFECYCLE_2026_07_22.json",
}
VEHICLE_DOCUMENTATION_FILES = {
    "README_FOR_AGENTS.md",
    "JOZZ_VEHICLE_README_PL.md",
    "docs/CURRENT_STATE_INDEX_PL.md",
    "docs/TECH_DEBT_PL.md",
}


def _read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path.name} root must be an object")
    return value


def _load_inventory() -> dict[str, Any]:
    return _read_json(INVENTORY_PATH)


def _load_branch_plan() -> dict[str, Any]:
    return _read_json(BRANCH_PLAN_PATH)


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


def _is_upstream_doc(path: str) -> bool:
    if path in UPSTREAM_DOC_FILES or path.startswith("docs/images/"):
        return True
    if not path.startswith("docs/"):
        return False
    relative = path.removeprefix("docs/")
    return bool(relative and relative[0].islower())


def _classify_path(path: str) -> str:
    if path.startswith(("src/", "include/")):
        return "upstream-engine-core"
    if path.startswith(("test/", "benchmark/", "shared/", "data/", "extern/")):
        return "upstream-support"
    if path in ROOT_BUILD_AND_HYGIENE or path.startswith("cmake/"):
        return "repository-build-and-hygiene"

    if path.startswith("samples/host/") or path in {
        "samples/main.cpp",
        "samples/sample.cpp",
        "samples/sample.h",
        "samples/CMakeLists.txt",
    }:
        return "shared-native-host"
    if path.startswith(("samples/gfx/", "samples/shaders/")) or path in SHARED_RENDERER_FILES:
        return "shared-native-renderer"
    if path == "samples/sample_jozz_vehicle_lab.cpp":
        return "vehicle-registration"
    if path.startswith("samples/sample_"):
        return "upstream-samples"

    if path.startswith(("samples/jozz_vehicle_", "samples/validation/")):
        return "vehicle-physics-rig-and-world"
    if path.startswith(("assets/contracts/", "assets/vehicle_presets/", "assets/source/")):
        return "vehicle-assets-and-contracts"
    if path.startswith("assets/"):
        return "vehicle-asset-evidence"
    if path in VEHICLE_TOOL_FILES:
        return "vehicle-tooling-and-gates"

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
    if path in REPOSITORY_GOVERNANCE_FILES:
        return "repository-governance"

    if path == ".github/pull_request_template.md":
        return "legacy-conflicting-governance"
    if path in UPSTREAM_GITHUB_FILES:
        return "upstream-github-metadata-and-ci"
    if path.startswith(".github/"):
        return "repository-github-metadata"

    if path in VEHICLE_DOCUMENTATION_FILES:
        return "vehicle-documentation"
    if path.startswith(("docs/SUBSYSTEM_", "docs/M7_", "docs/M8_", "docs/M9_")):
        return "vehicle-documentation"
    if path == "docs/CHECKPOINTS_PL.md" or path.startswith(("docs/archive/", "docs/adr/")):
        return "historical-documentation"
    if _is_upstream_doc(path):
        return "upstream-documentation"
    if path.startswith("docs/"):
        return "vehicle-world-and-project-history"

    return "unclassified-root-or-other"


def _tracked_paths() -> list[str]:
    completed = subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        timeout=60,
    )
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"git ls-files failed: {detail}")
    return sorted(
        item.decode("utf-8", errors="strict")
        for item in completed.stdout.split(b"\0")
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
        if ref.startswith(prefix):
            records.append({"name": ref[len(prefix) :], "sha": sha})
    return sorted(records, key=lambda record: record["name"])


def _remote_heads(remote: str) -> list[dict[str, str]]:
    return _parse_remote_refs(_run_git(["ls-remote", "--heads", remote]), "refs/heads/")


def _remote_tags(remote: str) -> list[dict[str, str]]:
    records = _parse_remote_refs(_run_git(["ls-remote", "--tags", remote]), "refs/tags/")
    return [record for record in records if not record["name"].endswith("^{}")]


def _branch_plan_by_name() -> dict[str, dict[str, Any]]:
    records = _load_branch_plan().get("currentBranches", [])
    if not isinstance(records, list):
        return {}
    return {
        item["name"]: item
        for item in records
        if isinstance(item, dict) and isinstance(item.get("name"), str)
    }


def _annotate_branches(
    branches: list[dict[str, str]], inventory: dict[str, Any]
) -> list[dict[str, Any]]:
    exact_plan = _branch_plan_by_name()
    annotated: list[dict[str, Any]] = []
    for remote in branches:
        name = remote["name"]
        record: dict[str, Any] = dict(remote)
        planned = exact_plan.get(name)
        if planned is None:
            record.update(
                retention="UNCLASSIFIED_REMOTE_BRANCH",
                preferredKeep=False,
                plannedShaMatches=None,
                dynamicCurrentHead=False,
            )
            annotated.append(record)
            continue

        sha_policy = planned.get("shaPolicy")
        retention = planned.get("retention")
        record.update(
            retention=retention,
            role=planned.get("role"),
            shaPolicy=sha_policy,
            preferredKeep=retention in {"KEEP_BASELINE", "KEEP_ACTIVE"},
        )
        if sha_policy == DYNAMIC_CURRENT_POLICY:
            record["plannedShaMatches"] = None
            record["dynamicCurrentHead"] = True
        else:
            record["plannedShaMatches"] = planned.get("sha") == remote["sha"]
            record["dynamicCurrentHead"] = False
        annotated.append(record)
    return annotated


def build_snapshot(remote: str, require_remote: bool) -> dict[str, Any]:
    inventory = _load_inventory()
    branch_plan = _load_branch_plan()
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
    annotated = _annotate_branches(branches, inventory)
    unclassified_branches = [
        item["name"]
        for item in annotated
        if item.get("retention") == "UNCLASSIFIED_REMOTE_BRANCH"
    ]
    exact_mismatches = [
        item["name"] for item in annotated if item.get("plannedShaMatches") is False
    ]
    dynamic_current_branches = [
        item["name"] for item in annotated if item.get("dynamicCurrentHead") is True
    ]
    unclassified_paths = {
        group: paths
        for group, paths in groups.items()
        if group.startswith("unclassified-")
    }
    debt = branch_plan.get("retentionDebt", {})
    missing_tags = debt.get("missingTags", []) if isinstance(debt, dict) else []

    return {
        "schemaVersion": 3,
        "repository": "Jozzpoly/Box3d_FunProject",
        "generatedFrom": {
            "head": _run_git(["rev-parse", "HEAD"]).strip(),
            "githubSha": os.environ.get("GITHUB_SHA"),
            "githubHeadRef": os.environ.get("GITHUB_HEAD_REF"),
            "githubBaseRef": os.environ.get("GITHUB_BASE_REF"),
        },
        "trackedTree": {
            "fileCount": len(tracked),
            "groupCounts": {key: len(value) for key, value in sorted(groups.items())},
            "groups": {key: value for key, value in sorted(groups.items())},
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
            "branches": annotated,
            "unclassifiedBranches": unclassified_branches,
            "plannedShaMismatches": exact_mismatches,
            "dynamicCurrentBranches": dynamic_current_branches,
            "tagCount": len(tags),
            "tags": tags,
            "retentionTagDebtCount": len(missing_tags),
            "retentionTagDebt": missing_tags,
            "furtherDeletionAuthorized": False,
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
    temporary.write_text(
        json.dumps(value, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
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

    tree = snapshot["trackedTree"]
    refs = snapshot["remoteRefs"]
    print("REPOSITORY_FORENSIC_SNAPSHOT_PASS")
    print(
        f"tracked_files={tree['fileCount']} "
        f"coverage_complete={str(tree['coverageComplete']).lower()}"
    )
    print(
        f"remote_branches={refs['branchCount']} hard_max={refs['hardMaximum']} "
        f"preferred={refs['preferredFinalCount']} "
        f"over_max={str(refs['overHardMaximum']).lower()}"
    )
    print(f"unclassified_remote_branches={len(refs['unclassifiedBranches'])}")
    print(f"unexpected_sha_mismatches={len(refs['plannedShaMismatches'])}")
    print(f"dynamic_current_branches={len(refs['dynamicCurrentBranches'])}")
    print(f"retention_tag_debt={refs['retentionTagDebtCount']}")
    print(f"output={args.output.as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
