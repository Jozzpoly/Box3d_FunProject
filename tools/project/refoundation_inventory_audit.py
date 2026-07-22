#!/usr/bin/env python3
"""Fail-closed checks for the project re-foundation forensic inventory."""
from __future__ import annotations

import json
from pathlib import Path
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
INVENTORY_PATH = "docs/PROJECT_INVENTORY.json"
REPORT_PATH = "docs/PROJECT_FORENSIC_INVENTORY_2026_07_22_PL.md"
SCAN_START_PATH = "docs/scan_import/00_START_HERE.md"
TECH_DEBT_PATH = "docs/TECH_DEBT_PL.md"

REQUIRED_DOMAIN_IDS = {
    "upstream-engine-core",
    "shared-native-host",
    "vehicle-physics-and-rig",
    "synthetic-engineering-world",
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

REQUIRED_PRS = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 13, 15, 16}
REQUIRED_TAG_TARGETS = {
    "milestone/terrain-visible-2026-07-22": "33099413bf8f44adbe1d635f9e10bdf2d0b5c321",
    "evidence/surface-foundation-pr7": "9aacc752f331d0d47c4c9c3f6fe82c63466f592c",
    "evidence/owner-flow-pr8": "a36e3d2f4c76f35d138a7e8b0aa11f7889e69e90",
}


def _read_text(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def _load_inventory(root: Path) -> dict[str, Any]:
    path = root / INVENTORY_PATH
    with path.open("r", encoding="utf-8") as handle:
        value = json.load(handle)
    if not isinstance(value, dict):
        raise ValueError("inventory root must be an object")
    return value


def _is_full_sha(value: object) -> bool:
    if not isinstance(value, str) or len(value) != 40:
        return False
    return all(character in "0123456789abcdef" for character in value)


def audit_refoundation_inventory(root: Path = ROOT) -> list[str]:
    root = Path(root)
    errors: list[str] = []

    for relative in (
        INVENTORY_PATH,
        REPORT_PATH,
        SCAN_START_PATH,
        TECH_DEBT_PATH,
    ):
        if not (root / relative).is_file():
            errors.append(f"MISSING_REQUIRED_FILE:{relative}")
    if errors:
        return errors

    try:
        inventory = _load_inventory(root)
    except Exception as exc:
        return [f"INVENTORY_INVALID_JSON:{exc}"]

    if inventory.get("schemaVersion") != 1:
        errors.append("INVENTORY_SCHEMA_VERSION")

    snapshot = inventory.get("snapshot")
    if not isinstance(snapshot, dict):
        errors.append("INVENTORY_SNAPSHOT_MISSING")
        snapshot = {}
    if snapshot.get("highestProvenCapability") != "TERRAIN_VISIBLE_PASS":
        errors.append("INVENTORY_CAPABILITY_DRIFT")
    if snapshot.get("nextProductGate") != "TEXTURED_SOURCE_PREVIEW":
        errors.append("INVENTORY_NEXT_GATE_DRIFT")
    if snapshot.get("authoritativeProductBranch") != "agent/scan-terrain-r1b-consolidated-integration":
        errors.append("INVENTORY_AUTHORITY_BRANCH_DRIFT")
    if snapshot.get("authoritativeHeadSource") != "GitHub Control Issue #11":
        errors.append("INVENTORY_HEAD_AUTHORITY_DRIFT")

    status_values = inventory.get("statusVocabulary")
    if not isinstance(status_values, list):
        errors.append("INVENTORY_STATUS_VOCABULARY_MISSING")
    else:
        missing = sorted(REQUIRED_STATUS_VALUES - set(status_values))
        if missing:
            errors.append("INVENTORY_STATUS_VALUES_MISSING:" + ",".join(missing))

    domains = inventory.get("domains")
    if not isinstance(domains, list):
        errors.append("INVENTORY_DOMAINS_MISSING")
        domains = []
    domain_ids: list[str] = []
    for index, domain in enumerate(domains):
        if not isinstance(domain, dict):
            errors.append(f"INVENTORY_DOMAIN_NOT_OBJECT:{index}")
            continue
        domain_id = domain.get("id")
        if not isinstance(domain_id, str) or not domain_id:
            errors.append(f"INVENTORY_DOMAIN_ID_MISSING:{index}")
            continue
        domain_ids.append(domain_id)
        for field in (
            "paths",
            "responsibility",
            "authority",
            "status",
            "owner",
            "canonicalValidation",
            "privacyRisk",
            "notes",
        ):
            if field not in domain:
                errors.append(f"INVENTORY_DOMAIN_FIELD_MISSING:{domain_id}:{field}")
        if domain.get("status") not in REQUIRED_STATUS_VALUES:
            errors.append(f"INVENTORY_DOMAIN_STATUS_INVALID:{domain_id}:{domain.get('status')}")
    duplicate_ids = sorted({value for value in domain_ids if domain_ids.count(value) > 1})
    if duplicate_ids:
        errors.append("INVENTORY_DUPLICATE_DOMAIN_IDS:" + ",".join(duplicate_ids))
    missing_domains = sorted(REQUIRED_DOMAIN_IDS - set(domain_ids))
    if missing_domains:
        errors.append("INVENTORY_REQUIRED_DOMAINS_MISSING:" + ",".join(missing_domains))

    lineage = inventory.get("pullRequestLineage")
    if not isinstance(lineage, list):
        errors.append("INVENTORY_PR_LINEAGE_MISSING")
        lineage = []
    seen_prs: set[int] = set()
    for record in lineage:
        if not isinstance(record, dict):
            errors.append("INVENTORY_PR_RECORD_NOT_OBJECT")
            continue
        pr = record.get("pr")
        branch = record.get("branch")
        retention = record.get("retention")
        if not isinstance(pr, int):
            errors.append("INVENTORY_PR_NUMBER_INVALID")
            continue
        seen_prs.add(pr)
        if not isinstance(branch, str) or not branch:
            errors.append(f"INVENTORY_PR_BRANCH_MISSING:{pr}")
        if retention not in {
            "KEEP_ACTIVE",
            "DELETE_AFTER_VERIFICATION",
            "PRESERVE_BY_TAG_OR_COMMIT",
            "DELETE_AFTER_INTEGRATION",
        }:
            errors.append(f"INVENTORY_PR_RETENTION_INVALID:{pr}:{retention}")
    missing_prs = sorted(REQUIRED_PRS - seen_prs)
    if missing_prs:
        errors.append("INVENTORY_PR_LINEAGE_INCOMPLETE:" + ",".join(map(str, missing_prs)))

    branch_reduction = inventory.get("branchReduction")
    if not isinstance(branch_reduction, dict):
        errors.append("INVENTORY_BRANCH_REDUCTION_MISSING")
        branch_reduction = {}
    if branch_reduction.get("ownerTargetMaximum") != 5:
        errors.append("BRANCH_HARD_MAXIMUM_DRIFT")
    if branch_reduction.get("preferredFinalCount") != 3:
        errors.append("BRANCH_PREFERRED_COUNT_DRIFT")
    preferred = branch_reduction.get("preferredFinalBranches")
    if preferred != ["main", "jozz-vehicle-sandbox-m0", "ONE_CURRENT_INTEGRATED_PROJECT_BRANCH"]:
        errors.append("BRANCH_PREFERRED_SET_DRIFT")
    if branch_reduction.get("requiredRemoteEnumeration") != "git ls-remote --heads origin":
        errors.append("BRANCH_REMOTE_ENUMERATION_MISSING")
    if branch_reduction.get("remotePresence") != "UNVERIFIED":
        errors.append("BRANCH_REMOTE_PRESENCE_OVERCLAIM")
    if branch_reduction.get("deletionAuthorized") is not False:
        errors.append("BRANCH_DELETION_AUTHORITY_OVERCLAIM")

    tags = branch_reduction.get("provisionalPreservationTags")
    found_tags: dict[str, str] = {}
    if isinstance(tags, list):
        for record in tags:
            if isinstance(record, dict) and isinstance(record.get("name"), str):
                found_tags[record["name"]] = record.get("target")
    for name, expected_target in REQUIRED_TAG_TARGETS.items():
        actual = found_tags.get(name)
        if actual != expected_target:
            errors.append(f"BRANCH_PRESERVATION_TAG_DRIFT:{name}:{actual}")
        if actual is not None and not _is_full_sha(actual):
            errors.append(f"BRANCH_PRESERVATION_TAG_SHA_INVALID:{name}")

    scan_start = _read_text(root, SCAN_START_PATH)
    for marker in (
        "TERRAIN_VISIBLE_PASS",
        "TEXTURED_SOURCE_PREVIEW",
        "VEHICLE_SCALE_REFERENCE_SCENE",
        "WORLD_SCALE_VALIDATED = false",
        "current start here",
    ):
        if marker not in scan_start:
            errors.append(f"SCAN_START_MARKER_MISSING:{marker}")
    for stale in (
        "P1B_OWNER_GATE_HARDENING",
        "Brakuje realnego owner-confirmed source frame",
        "Następne po tej bramce",
    ):
        if stale in scan_start:
            errors.append(f"SCAN_START_STALE_IMPERATIVE:{stale}")

    tech_debt = _read_text(root, TECH_DEBT_PATH)
    for marker in (
        "AGENTS.md",
        "isolated branch",
        "draft PR",
        "vehicle",
    ):
        if marker not in tech_debt:
            errors.append(f"TECH_DEBT_AUTHORITY_MARKER_MISSING:{marker}")
    forbidden_tech_debt = (
        "agenci odtąd samodzielnie commitują i pushują\nna `jozz-vehicle-sandbox-m0`",
        "`README_FOR_AGENTS.md` jest teraz repo-widocznym,\naktualnym, wspólnym źródłem prawdy",
    )
    for stale in forbidden_tech_debt:
        if stale in tech_debt:
            errors.append("TECH_DEBT_STALE_OPERATIONAL_RULE")

    report = _read_text(root, REPORT_PATH)
    for marker in (
        "hard maximum after cleanup: 5 branches",
        "preferred final state:       3 branches",
        "git ls-remote --heads origin",
        "Divergent heads — tag przed usunięciem brancha",
        "TEXTURED_SOURCE_PREVIEW",
    ):
        if marker not in report:
            errors.append(f"FORENSIC_REPORT_MARKER_MISSING:{marker}")

    return errors


def main() -> int:
    errors = audit_refoundation_inventory(ROOT)
    if errors:
        print("REFOUNDATION_INVENTORY_AUDIT_FAIL")
        for error in errors:
            print(f"- {error}")
        return 1
    print("REFOUNDATION_INVENTORY_AUDIT_PASS")
    print("branch_target=3 hard_max=5 deletion_authorized=false")
    print("next_gate=TEXTURED_SOURCE_PREVIEW")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
