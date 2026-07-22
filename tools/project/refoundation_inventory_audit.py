#!/usr/bin/env python3
"""Fail-closed checks for the current project inventory and branch topology."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
INVENTORY_PATH = "docs/PROJECT_INVENTORY.json"
BRANCH_PLAN_PATH = "docs/BRANCH_RETENTION_PLAN_2026_07_22.json"
REPORT_PATH = "docs/PROJECT_FORENSIC_INVENTORY_2026_07_22_PL.md"
SCAN_START_PATH = "docs/scan_import/00_START_HERE.md"
TECH_DEBT_PATH = "docs/TECH_DEBT_PL.md"
COMPATIBILITY_README_PATH = "JOZZ_VEHICLE_README_PL.md"
PROJECT_DIRECTION_PATH = "docs/PROJECT_DIRECTION_PL.md"
LEGACY_PR_TEMPLATE = ".github/pull_request_template.md"

CURRENT_PROJECT_BRANCH = "agent/project-refoundation-audit-v1"
INTEGRATION_BASE = "jozz-vehicle-sandbox-m0"
CURRENT_PR = 17
CURRENT_REMOTE_BRANCHES = {
    "main",
    INTEGRATION_BASE,
    CURRENT_PROJECT_BRANCH,
}
REQUIRED_TAG_TARGETS = {
    "milestone/terrain-visible-2026-07-22": "33099413bf8f44adbe1d635f9e10bdf2d0b5c321",
    "evidence/surface-foundation-pr7": "9aacc752f331d0d47c4c9c3f6fe82c63466f592c",
    "evidence/owner-flow-pr8": "a36e3d2f4c76f35d138a7e8b0aa11f7889e69e90",
}
REQUIRED_DOMAIN_IDS = {
    "upstream-engine-core",
    "upstream-support-and-ci",
    "shared-native-host-and-renderer",
    "vehicle-physics-and-rig",
    "synthetic-engineering-world",
    "vehicle-assets-tooling-and-evidence",
    "scan-inspection-and-evidence",
    "scan-source-geometry-preview-v1",
    "scan-textured-source-preview",
    "vehicle-scale-reference-scene",
    "surface-and-collision-research",
    "future-world-authoring",
    "automation-and-governance",
    "jes-boundary",
}
REQUIRED_STATUS_VALUES = {
    "ACTIVE_NEXT",
    "FOUNDATION_PRESERVE",
    "PARKED_WITH_REACTIVATION_GATE",
    "HISTORICAL_EVIDENCE",
    "SUPERSEDED",
    "KNOWN_DEBT",
    "EXPERIMENTAL_NOT_AUTHORITY",
    "OWNER_DECISION_REQUIRED",
    "REMOVE_AFTER_VERIFICATION",
    "UNREVIEWED",
}
REQUIRED_PRS = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 15, 16, 17}
DOMAIN_FIELDS = {
    "paths",
    "responsibility",
    "authority",
    "status",
    "owner",
    "canonicalValidation",
    "privacyRisk",
    "notes",
}


def _read_text(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def _load_json(root: Path, relative: str) -> dict[str, Any]:
    value = json.loads(_read_text(root, relative))
    if not isinstance(value, dict):
        raise ValueError(f"{relative} root must be an object")
    return value


def _full_sha(value: object) -> bool:
    return (
        isinstance(value, str)
        and len(value) == 40
        and all(character in "0123456789abcdef" for character in value)
    )


def _require_markers(text: str, markers: tuple[str, ...], label: str, errors: list[str]) -> None:
    for marker in markers:
        if marker not in text:
            errors.append(f"{label}_MARKER_MISSING:{marker}")


def _audit_domains(inventory: dict[str, Any], errors: list[str]) -> None:
    vocabulary = inventory.get("statusVocabulary")
    if not isinstance(vocabulary, list):
        errors.append("INVENTORY_STATUS_VOCABULARY_MISSING")
    else:
        missing = sorted(REQUIRED_STATUS_VALUES - set(vocabulary))
        if missing:
            errors.append("INVENTORY_STATUS_VALUES_MISSING:" + ",".join(missing))

    domains = inventory.get("domains")
    if not isinstance(domains, list):
        errors.append("INVENTORY_DOMAINS_MISSING")
        return

    ids: list[str] = []
    for index, domain in enumerate(domains):
        if not isinstance(domain, dict):
            errors.append(f"INVENTORY_DOMAIN_NOT_OBJECT:{index}")
            continue
        domain_id = domain.get("id")
        if not isinstance(domain_id, str) or not domain_id:
            errors.append(f"INVENTORY_DOMAIN_ID_MISSING:{index}")
            continue
        ids.append(domain_id)
        missing_fields = sorted(DOMAIN_FIELDS - set(domain))
        if missing_fields:
            errors.append(
                f"INVENTORY_DOMAIN_FIELDS_MISSING:{domain_id}:" + ",".join(missing_fields)
            )
        if domain.get("status") not in REQUIRED_STATUS_VALUES:
            errors.append(f"INVENTORY_DOMAIN_STATUS_INVALID:{domain_id}:{domain.get('status')}")

    duplicates = sorted({value for value in ids if ids.count(value) > 1})
    if duplicates:
        errors.append("INVENTORY_DUPLICATE_DOMAIN_IDS:" + ",".join(duplicates))
    missing_domains = sorted(REQUIRED_DOMAIN_IDS - set(ids))
    if missing_domains:
        errors.append("INVENTORY_REQUIRED_DOMAINS_MISSING:" + ",".join(missing_domains))


def _audit_lineage(inventory: dict[str, Any], errors: list[str]) -> None:
    lineage = inventory.get("pullRequestLineage")
    if not isinstance(lineage, list):
        errors.append("INVENTORY_PR_LINEAGE_MISSING")
        return

    seen: set[int] = set()
    for record in lineage:
        if not isinstance(record, dict):
            errors.append("INVENTORY_PR_RECORD_NOT_OBJECT")
            continue
        pr = record.get("pr")
        if not isinstance(pr, int):
            errors.append("INVENTORY_PR_NUMBER_INVALID")
            continue
        seen.add(pr)
        if not isinstance(record.get("role"), str) or not record["role"]:
            errors.append(f"INVENTORY_PR_ROLE_MISSING:{pr}")
        if not isinstance(record.get("status"), str) or not record["status"]:
            errors.append(f"INVENTORY_PR_STATUS_MISSING:{pr}")

    missing_prs = sorted(REQUIRED_PRS - seen)
    if missing_prs:
        errors.append("INVENTORY_PR_LINEAGE_INCOMPLETE:" + ",".join(map(str, missing_prs)))


def _tag_map(records: object) -> dict[str, object]:
    result: dict[str, object] = {}
    if not isinstance(records, list):
        return result
    for record in records:
        if isinstance(record, dict) and isinstance(record.get("name"), str):
            result[record["name"]] = record.get("target")
    return result


def _audit_branch_reduction(inventory: dict[str, Any], errors: list[str]) -> None:
    reduction = inventory.get("branchReduction")
    if not isinstance(reduction, dict):
        errors.append("INVENTORY_BRANCH_REDUCTION_MISSING")
        return

    expected = {
        "ownerTargetMaximum": 5,
        "preferredFinalCount": 3,
        "currentBranchCount": 3,
        "cleanupStatus": "COMPLETED_TO_PREFERRED_COUNT_WITH_RETENTION_DEBT",
        "retentionTagDebtCount": 3,
        "furtherDeletionAuthorized": False,
        "exactPlan": BRANCH_PLAN_PATH,
    }
    for field, value in expected.items():
        if reduction.get(field) != value:
            errors.append(f"BRANCH_REDUCTION_DRIFT:{field}:{reduction.get(field)}")

    branches = reduction.get("currentBranches")
    if not isinstance(branches, list) or set(branches) != CURRENT_REMOTE_BRANCHES:
        errors.append(f"BRANCH_REDUCTION_CURRENT_SET_DRIFT:{branches}")


def _audit_branch_plan(plan: dict[str, Any], errors: list[str]) -> None:
    if plan.get("schemaVersion") != 3:
        errors.append("BRANCH_PLAN_SCHEMA_VERSION")
    if plan.get("cleanupStatus") != "COMPLETED_TO_PREFERRED_COUNT_WITH_RETENTION_DEBT":
        errors.append("BRANCH_PLAN_CLEANUP_STATUS_DRIFT")

    policy = plan.get("ownerPolicy")
    if not isinstance(policy, dict):
        errors.append("BRANCH_PLAN_OWNER_POLICY_MISSING")
    else:
        expected_policy = {
            "hardMaximumBranches": 5,
            "preferredBranchCount": 3,
            "currentBranchCount": 3,
            "furtherDeletionAuthorized": False,
            "forcePushAuthorized": False,
            "historyRewriteAuthorized": False,
            "bulkDeleteAuthorized": False,
        }
        for field, value in expected_policy.items():
            if policy.get(field) != value:
                errors.append(f"BRANCH_PLAN_POLICY_DRIFT:{field}:{policy.get(field)}")

    current = plan.get("currentBranches")
    if not isinstance(current, list):
        errors.append("BRANCH_PLAN_CURRENT_BRANCHES_MISSING")
        current = []

    names: list[str] = []
    for record in current:
        if not isinstance(record, dict):
            errors.append("BRANCH_PLAN_CURRENT_RECORD_NOT_OBJECT")
            continue
        name = record.get("name")
        if not isinstance(name, str):
            errors.append("BRANCH_PLAN_CURRENT_NAME_INVALID")
            continue
        names.append(name)
        if name == CURRENT_PROJECT_BRANCH:
            if record.get("shaPolicy") != "DYNAMIC_CURRENT_PROJECT_HEAD_UNTIL_INTEGRATION":
                errors.append("BRANCH_PLAN_CURRENT_HEAD_POLICY_DRIFT")
            if record.get("retention") != "KEEP_ACTIVE":
                errors.append("BRANCH_PLAN_CURRENT_RETENTION_DRIFT")
        else:
            if record.get("shaPolicy") != "EXACT_REMOTE_SHA":
                errors.append(f"BRANCH_PLAN_BASELINE_SHA_POLICY_DRIFT:{name}")
            if not _full_sha(record.get("sha")):
                errors.append(f"BRANCH_PLAN_BASELINE_SHA_INVALID:{name}")
            if record.get("retention") != "KEEP_BASELINE":
                errors.append(f"BRANCH_PLAN_BASELINE_RETENTION_DRIFT:{name}")

    if set(names) != CURRENT_REMOTE_BRANCHES or len(names) != 3:
        errors.append("BRANCH_PLAN_CURRENT_SET_DRIFT:" + ",".join(names))

    debt = plan.get("retentionDebt")
    if not isinstance(debt, dict):
        errors.append("BRANCH_PLAN_RETENTION_DEBT_MISSING")
    else:
        if debt.get("status") != "OPEN_NON_BLOCKING_FOR_CURRENT_BRANCH_WORK":
            errors.append("BRANCH_PLAN_RETENTION_DEBT_STATUS_DRIFT")
        tags = _tag_map(debt.get("missingTags"))
        for name, target in REQUIRED_TAG_TARGETS.items():
            if tags.get(name) != target:
                errors.append(f"BRANCH_PLAN_RETENTION_TAG_DRIFT:{name}:{tags.get(name)}")

    final_state = plan.get("finalState")
    if not isinstance(final_state, dict):
        errors.append("BRANCH_PLAN_FINAL_STATE_MISSING")
    else:
        expected_final = {
            "branchCount": 3,
            "satisfiesHardMaximum": True,
            "satisfiesPreferredCount": True,
            "branchCleanupComplete": True,
            "retentionTaggingComplete": False,
        }
        for field, value in expected_final.items():
            if final_state.get(field) != value:
                errors.append(f"BRANCH_PLAN_FINAL_STATE_DRIFT:{field}:{final_state.get(field)}")


def audit_refoundation_inventory(root: Path = ROOT) -> list[str]:
    root = Path(root)
    errors: list[str] = []
    required_files = (
        INVENTORY_PATH,
        BRANCH_PLAN_PATH,
        REPORT_PATH,
        SCAN_START_PATH,
        TECH_DEBT_PATH,
        COMPATIBILITY_README_PATH,
        PROJECT_DIRECTION_PATH,
    )
    for relative in required_files:
        if not (root / relative).is_file():
            errors.append(f"MISSING_REQUIRED_FILE:{relative}")
    if (root / LEGACY_PR_TEMPLATE).exists():
        errors.append(f"LEGACY_CONFLICTING_PR_TEMPLATE_PRESENT:{LEGACY_PR_TEMPLATE}")
    if errors:
        return errors

    try:
        inventory = _load_json(root, INVENTORY_PATH)
        branch_plan = _load_json(root, BRANCH_PLAN_PATH)
    except Exception as exc:
        return [f"REFOUNDATION_JSON_INVALID:{exc}"]

    if inventory.get("schemaVersion") != 3:
        errors.append("INVENTORY_SCHEMA_VERSION")
    snapshot = inventory.get("snapshot")
    if not isinstance(snapshot, dict):
        errors.append("INVENTORY_SNAPSHOT_MISSING")
        snapshot = {}

    expected_snapshot = {
        "authoritativeProductBranch": CURRENT_PROJECT_BRANCH,
        "authoritativeHeadSource": "GitHub Control Issue #11",
        "activeProductPr": CURRENT_PR,
        "integrationBase": INTEGRATION_BASE,
        "highestProvenCapability": "TERRAIN_VISIBLE_PASS",
        "nextProductGate": "TEXTURED_SOURCE_PREVIEW",
        "status": "F3_THREE_BRANCH_TOPOLOGY_RECONCILED_PENDING_INTEGRATION_REVIEW",
    }
    for field, value in expected_snapshot.items():
        if snapshot.get(field) != value:
            errors.append(f"INVENTORY_SNAPSHOT_DRIFT:{field}:{snapshot.get(field)}")

    refs = snapshot.get("remoteRefState")
    if not isinstance(refs, dict):
        errors.append("INVENTORY_REMOTE_REF_STATE_MISSING")
    else:
        if refs.get("branchCount") != 3 or refs.get("tagCount") != 0:
            errors.append("INVENTORY_REMOTE_REF_COUNT_DRIFT")
        if set(refs.get("branches", [])) != CURRENT_REMOTE_BRANCHES:
            errors.append(f"INVENTORY_REMOTE_BRANCH_SET_DRIFT:{refs.get('branches')}")
        if refs.get("coverageComplete") is not True:
            errors.append("INVENTORY_REMOTE_REF_COVERAGE_INCOMPLETE")
        if refs.get("retentionTagDebtCount") != 3:
            errors.append("INVENTORY_RETENTION_TAG_DEBT_DRIFT")
        if refs.get("exactPlan") != BRANCH_PLAN_PATH:
            errors.append("INVENTORY_EXACT_BRANCH_PLAN_DRIFT")

    _audit_domains(inventory, errors)
    _audit_lineage(inventory, errors)
    _audit_branch_reduction(inventory, errors)
    _audit_branch_plan(branch_plan, errors)

    scan_start = _read_text(root, SCAN_START_PATH)
    _require_markers(
        scan_start,
        (
            "TERRAIN_VISIBLE_PASS",
            "TEXTURED_SOURCE_PREVIEW",
            "VEHICLE_SCALE_REFERENCE_SCENE",
            "WORLD_SCALE_VALIDATED = false",
            "current start here",
        ),
        "SCAN_START",
        errors,
    )
    for stale in (
        "**Status:** `P1A_REAL_INSPECTION_LOCAL_PASS / P1B_OWNER_GATE_HARDENING`",
        "Brakuje realnego owner-confirmed source frame",
        "Następne po tej bramce",
    ):
        if stale in scan_start:
            errors.append(f"SCAN_START_STALE_IMPERATIVE:{stale}")

    tech_debt = _read_text(root, TECH_DEBT_PATH)
    _require_markers(
        tech_debt,
        ("AGENTS.md", "izolowany branch", "draft PR", "vehicle"),
        "TECH_DEBT_AUTHORITY",
        errors,
    )
    for stale in (
        "agenci odtąd samodzielnie commitują i pushują\nna `jozz-vehicle-sandbox-m0`",
        "`README_FOR_AGENTS.md` jest teraz repo-widocznym,\naktualnym, wspólnym źródłem prawdy",
    ):
        if stale in tech_debt:
            errors.append("TECH_DEBT_STALE_OPERATIONAL_RULE")

    compatibility = _read_text(root, COMPATIBILITY_README_PATH)
    _require_markers(
        compatibility,
        ("SUPERSEDED_AS_FRONT_DOOR", "AGENTS.md", "README_FOR_AGENTS.md"),
        "VEHICLE_COMPATIBILITY_POINTER",
        errors,
    )
    direction = _read_text(root, PROJECT_DIRECTION_PATH)
    _require_markers(
        direction,
        ("HISTORICAL_EVIDENCE", "SUPERSEDED_AS_CURRENT_ROADMAP", "PROJECT_CHARTER_PL.md"),
        "PROJECT_DIRECTION_HISTORY",
        errors,
    )
    report = _read_text(root, REPORT_PATH)
    _require_markers(
        report,
        (
            "3 branches",
            "retention tag debt",
            "agent/project-refoundation-audit-v1",
            "TEXTURED_SOURCE_PREVIEW",
        ),
        "FORENSIC_REPORT",
        errors,
    )
    return errors


def main() -> int:
    errors = audit_refoundation_inventory(ROOT)
    if errors:
        print("REFOUNDATION_INVENTORY_AUDIT_FAIL")
        for error in errors:
            print(f"- {error}")
        return 1
    print("REFOUNDATION_INVENTORY_AUDIT_PASS")
    print("remote_branches=3 remote_tags=0 retention_tag_debt=3")
    print("branch_target=3 hard_max=5 further_deletion_authorized=false")
    print("authority=agent/project-refoundation-audit-v1 pr=#17")
    print("next_gate=TEXTURED_SOURCE_PREVIEW")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
