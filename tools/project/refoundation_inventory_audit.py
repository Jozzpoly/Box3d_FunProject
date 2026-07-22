#!/usr/bin/env python3
"""Fail-closed checks for the project re-foundation forensic inventory."""
from __future__ import annotations

import json
from pathlib import Path
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
VALID_RETENTION = {
    "KEEP_ACTIVE",
    "DELETE_AFTER_VERIFICATION",
    "PRESERVE_BY_TAG_OR_COMMIT",
    "DELETE_AFTER_INTEGRATION",
}


def _read_text(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def _load_inventory(root: Path) -> dict[str, Any]:
    value = json.loads(_read_text(root, INVENTORY_PATH))
    if not isinstance(value, dict):
        raise ValueError("inventory root must be an object")
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


def audit_refoundation_inventory(root: Path = ROOT) -> list[str]:
    root = Path(root)
    errors: list[str] = []

    for relative in (INVENTORY_PATH, REPORT_PATH, SCAN_START_PATH, TECH_DEBT_PATH):
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
    expected_snapshot = {
        "highestProvenCapability": "TERRAIN_VISIBLE_PASS",
        "nextProductGate": "TEXTURED_SOURCE_PREVIEW",
        "authoritativeProductBranch": "agent/scan-terrain-r1b-consolidated-integration",
        "authoritativeHeadSource": "GitHub Control Issue #11",
    }
    for field, expected in expected_snapshot.items():
        if snapshot.get(field) != expected:
            errors.append(f"INVENTORY_SNAPSHOT_DRIFT:{field}:{snapshot.get(field)}")

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
        missing_fields = sorted(DOMAIN_FIELDS - set(domain))
        if missing_fields:
            errors.append(
                f"INVENTORY_DOMAIN_FIELDS_MISSING:{domain_id}:" + ",".join(missing_fields)
            )
        if domain.get("status") not in REQUIRED_STATUS_VALUES:
            errors.append(f"INVENTORY_DOMAIN_STATUS_INVALID:{domain_id}:{domain.get('status')}")
    duplicates = sorted({value for value in domain_ids if domain_ids.count(value) > 1})
    if duplicates:
        errors.append("INVENTORY_DUPLICATE_DOMAIN_IDS:" + ",".join(duplicates))
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
        if not isinstance(pr, int):
            errors.append("INVENTORY_PR_NUMBER_INVALID")
            continue
        seen_prs.add(pr)
        if not isinstance(record.get("branch"), str) or not record["branch"]:
            errors.append(f"INVENTORY_PR_BRANCH_MISSING:{pr}")
        if record.get("retention") not in VALID_RETENTION:
            errors.append(f"INVENTORY_PR_RETENTION_INVALID:{pr}:{record.get('retention')}")
    missing_prs = sorted(REQUIRED_PRS - seen_prs)
    if missing_prs:
        errors.append("INVENTORY_PR_LINEAGE_INCOMPLETE:" + ",".join(map(str, missing_prs)))

    reduction = inventory.get("branchReduction")
    if not isinstance(reduction, dict):
        errors.append("INVENTORY_BRANCH_REDUCTION_MISSING")
        reduction = {}
    if reduction.get("ownerTargetMaximum") != 5:
        errors.append("BRANCH_HARD_MAXIMUM_DRIFT")
    if reduction.get("preferredFinalCount") != 3:
        errors.append("BRANCH_PREFERRED_COUNT_DRIFT")
    if reduction.get("preferredFinalBranches") != [
        "main",
        "jozz-vehicle-sandbox-m0",
        "ONE_CURRENT_INTEGRATED_PROJECT_BRANCH",
    ]:
        errors.append("BRANCH_PREFERRED_SET_DRIFT")
    if reduction.get("requiredRemoteEnumeration") != "git ls-remote --heads origin":
        errors.append("BRANCH_REMOTE_ENUMERATION_MISSING")
    if reduction.get("remotePresence") != "UNVERIFIED":
        errors.append("BRANCH_REMOTE_PRESENCE_OVERCLAIM")
    if reduction.get("deletionAuthorized") is not False:
        errors.append("BRANCH_DELETION_AUTHORITY_OVERCLAIM")

    found_tags: dict[str, object] = {}
    tags = reduction.get("provisionalPreservationTags")
    if isinstance(tags, list):
        for record in tags:
            if isinstance(record, dict) and isinstance(record.get("name"), str):
                found_tags[record["name"]] = record.get("target")
    for name, expected in REQUIRED_TAG_TARGETS.items():
        actual = found_tags.get(name)
        if actual != expected:
            errors.append(f"BRANCH_PRESERVATION_TAG_DRIFT:{name}:{actual}")
        if actual is not None and not _full_sha(actual):
            errors.append(f"BRANCH_PRESERVATION_TAG_SHA_INVALID:{name}")

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
        "P1B_OWNER_GATE_HARDENING",
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

    report = _read_text(root, REPORT_PATH)
    _require_markers(
        report,
        (
            "hard maximum after cleanup: 5 branches",
            "preferred final state:       3 branches",
            "git ls-remote --heads origin",
            "Divergent heads — tag przed usunięciem brancha",
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
    print("branch_target=3 hard_max=5 deletion_authorized=false")
    print("next_gate=TEXTURED_SOURCE_PREVIEW")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
