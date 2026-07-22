#!/usr/bin/env python3
"""Fail-closed audit for project-authored document lifecycle classification."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = "docs/DOCUMENT_LIFECYCLE_2026_07_22.json"

EXPECTED_PROJECT_DOCUMENTS = {
    "docs/ASSET_CONTRACT_RUNTIME_V1_PL.md",
    "docs/ASSET_CONTRACT_V2_DRAFT_PL.md",
    "docs/AUDIT_PHYSICS_STEERING_2026_07_08_PL.md",
    "docs/AUDIT_WERYFIKACJA_P1_P6_2026_07_09_PL.md",
    "docs/BOX3D_JOINT_SAMPLES_STUDY_PL.md",
    "docs/EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md",
    "docs/FINALIZACJA_ETAP_1_MODEL_I_UI_PL.md",
    "docs/FINALIZACJA_ETAP_2_PERSYSTENCJA_PL.md",
    "docs/FINALIZACJA_ETAP_3_STAN_ZWALIDOWANY_PL.md",
    "docs/HOTKEY_AUDIT_PL.md",
    "docs/M6_SUSPENSION_RIG_FOUNDATION_PL.md",
    "docs/MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md",
    "docs/MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md",
    "docs/MAPA_ETAP_3_TOR_I_DRIFT_PL.md",
    "docs/MAPA_ETAP_4_PLAC_FIZYKI_PL.md",
    "docs/MAPA_ETAP_5_SPAWNER_I_STRESS_PL.md",
    "docs/MAPA_ETAP_6_NAWIGACJA_POMIAR_POLISH_PL.md",
    "docs/PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md",
    "docs/PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md",
    "docs/PLAN_PORZADKI_FUNDAMENT_2026_07_09_PL.md",
    "docs/PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md",
    "docs/PLAN_STABILNOSC_PROWADZENIE_PL.md",
    "docs/PLAN_WIELKI_REFACTOR_2026_07_09_PL.md",
    "docs/PROJECT_DIRECTION_PL.md",
    "docs/SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md",
}
EXPECTED_CURRENT_DOCUMENTS = {
    "AGENTS.md",
    "AI_PROJECT_MEMORY.md",
    "docs/PROJECT_OPERATING_PLAN_PL.md",
    "docs/PROJECT_CHARTER_PL.md",
    "README_FOR_AGENTS.md",
    "docs/CURRENT_STATE_INDEX_PL.md",
    "docs/TECH_DEBT_PL.md",
    "docs/SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md",
    "docs/SUBSYSTEM_UI_PRESETS_PL.md",
    "docs/HOTKEY_AUDIT_PL.md",
    "docs/scan_import/00_START_HERE.md",
    "docs/scan_import/CURRENT_STATE.md",
    "docs/scan_import/ARCHITECTURE.md",
    "docs/scan_import/P2A_SOURCE_VISUAL_PREVIEW.md",
}
VALID_STATUSES = {
    "CURRENT_AUTHORITY",
    "FOUNDATION_PRESERVE",
    "COMPLETED_RECORD",
    "PARKED_WITH_REACTIVATION_GATE",
    "BLOCKED_BY_PREDECESSOR",
    "HISTORICAL_EVIDENCE",
    "HISTORICAL_FOUNDATION",
    "DRAFT_NOT_AUTHORITY",
    "SUPERSEDED_AS_FRONT_DOOR",
}
REQUIRED_MARKERS = {
    "docs/PLAN_STABILNOSC_PROWADZENIE_PL.md": (
        "COMPLETED_AND_OWNER_ACCEPTED",
        "Nie jest:",
        "izolowanym branchu",
    ),
    "docs/PLAN_WIELKI_REFACTOR_2026_07_09_PL.md": (
        "R0_TO_R5_COMPLETED",
        "R6",
        "OPEN_PHYSICS_CHANGE",
    ),
    "docs/PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md": (
        "COMPLETED",
        "Nie jest:",
    ),
    "docs/PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md": (
        "COMPLETED_RESEARCH_WARMUP",
        "Nie jest:",
    ),
    "docs/EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md": (
        "PARKED_DESIGN_AUTHORITY",
        "OWNER_DECISION_REQUIRED",
    ),
    "docs/PROJECT_DIRECTION_PL.md": (
        "HISTORICAL_EVIDENCE",
        "SUPERSEDED_AS_CURRENT_ROADMAP",
    ),
    "docs/HOTKEY_AUDIT_PL.md": (
        "CURRENT_CONFLICT_AVOIDANCE_CONTRACT",
        "P2A Source Visual Preview",
    ),
}


def _load_manifest(root: Path) -> dict:
    value = json.loads((root / MANIFEST_PATH).read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("document lifecycle manifest root must be an object")
    return value


def _records_by_path(value: object, errors: list[str], label: str) -> dict[str, dict]:
    if not isinstance(value, list):
        errors.append(f"{label}_MISSING")
        return {}
    result: dict[str, dict] = {}
    for index, record in enumerate(value):
        if not isinstance(record, dict):
            errors.append(f"{label}_RECORD_NOT_OBJECT:{index}")
            continue
        path = record.get("path")
        if not isinstance(path, str) or not path:
            errors.append(f"{label}_PATH_INVALID:{index}")
            continue
        if path in result:
            errors.append(f"{label}_DUPLICATE_PATH:{path}")
        result[path] = record
    return result


def audit_document_lifecycle(root: Path = ROOT) -> list[str]:
    root = Path(root)
    errors: list[str] = []
    manifest_file = root / MANIFEST_PATH
    if not manifest_file.is_file():
        return [f"MISSING_REQUIRED_FILE:{MANIFEST_PATH}"]

    try:
        manifest = _load_manifest(root)
    except Exception as exc:
        return [f"DOCUMENT_LIFECYCLE_INVALID_JSON:{exc}"]

    if manifest.get("schemaVersion") != 1:
        errors.append("DOCUMENT_LIFECYCLE_SCHEMA_VERSION")
    if "No document in this manifest selects current work by itself" not in str(
        manifest.get("authorityRule", "")
    ):
        errors.append("DOCUMENT_LIFECYCLE_AUTHORITY_RULE_MISSING")

    current = _records_by_path(manifest.get("currentDocuments"), errors, "CURRENT_DOCUMENTS")
    project = _records_by_path(
        manifest.get("projectAuthoredDocuments"), errors, "PROJECT_DOCUMENTS"
    )

    missing_current = sorted(EXPECTED_CURRENT_DOCUMENTS - set(current))
    extra_current = sorted(set(current) - EXPECTED_CURRENT_DOCUMENTS)
    if missing_current:
        errors.append("CURRENT_DOCUMENTS_MISSING:" + ",".join(missing_current))
    if extra_current:
        errors.append("CURRENT_DOCUMENTS_UNEXPECTED:" + ",".join(extra_current))

    missing_project = sorted(EXPECTED_PROJECT_DOCUMENTS - set(project))
    extra_project = sorted(set(project) - EXPECTED_PROJECT_DOCUMENTS)
    if missing_project:
        errors.append("PROJECT_DOCUMENTS_MISSING:" + ",".join(missing_project))
    if extra_project:
        errors.append("PROJECT_DOCUMENTS_UNEXPECTED:" + ",".join(extra_project))

    for path, record in {**current, **project}.items():
        if record.get("status") not in VALID_STATUSES:
            errors.append(f"DOCUMENT_STATUS_INVALID:{path}:{record.get('status')}")
        if not (root / path).is_file():
            errors.append(f"CLASSIFIED_DOCUMENT_NOT_TRACKED:{path}")

    for path, record in project.items():
        if record.get("mayActivateWork") is not False:
            errors.append(f"HISTORICAL_DOCUMENT_CAN_ACTIVATE_WORK:{path}")
        for field in ("domain", "role", "supersededBy", "reactivationGate"):
            if field not in record:
                errors.append(f"DOCUMENT_FIELD_MISSING:{path}:{field}")

    coverage = manifest.get("coverage")
    if not isinstance(coverage, dict):
        errors.append("DOCUMENT_COVERAGE_MISSING")
    else:
        if coverage.get("projectAuthoredDocumentCount") != 25:
            errors.append(
                f"DOCUMENT_COUNT_DRIFT:{coverage.get('projectAuthoredDocumentCount')}"
            )
        if coverage.get("allTrackedProjectAuthoredDocumentsClassified") is not True:
            errors.append("DOCUMENT_COVERAGE_NOT_COMPLETE")
        if coverage.get("unclassifiedProjectAuthoredDocuments") != []:
            errors.append("DOCUMENT_UNCLASSIFIED_LIST_NOT_EMPTY")
        if coverage.get("mayActivateWorkCount") != 0:
            errors.append("DOCUMENT_ACTIVATION_COUNT_NOT_ZERO")

    for path, markers in REQUIRED_MARKERS.items():
        text = (root / path).read_text(encoding="utf-8")
        for marker in markers:
            if marker not in text:
                errors.append(f"DOCUMENT_MARKER_MISSING:{path}:{marker}")

    return errors


def main() -> int:
    errors = audit_document_lifecycle(ROOT)
    if errors:
        print("DOCUMENT_LIFECYCLE_AUDIT_FAIL")
        for error in errors:
            print(f"- {error}")
        return 1
    print("DOCUMENT_LIFECYCLE_AUDIT_PASS")
    print("project_documents=25 classified=25 may_activate_work=0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
