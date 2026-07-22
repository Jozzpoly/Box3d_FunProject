#!/usr/bin/env python3
"""Fail-closed audit for repository authority, documentation and CI routing."""
from __future__ import annotations

from pathlib import Path
import re
import sys
from typing import Iterable

ROOT = Path(__file__).resolve().parents[2]
AUTOMATION_TOOLS = ROOT / "tools" / "automation"
if str(AUTOMATION_TOOLS) not in sys.path:
    sys.path.insert(0, str(AUTOMATION_TOOLS))

import common  # noqa: E402

ACTIVE_BRANCH = "agent/scan-terrain-r1b-consolidated-integration"
ACTIVE_PR = "#13"
CURRENT_STATUS = "TERRAIN_VISIBLE_PASS"
CONTROL_HEAD_AUTHORITY = "GitHub Control Issue #11"
LEGACY_WORKFLOW = ".github/workflows/p2a-source-visual-preview.yml"
CHARTER_FILE = "docs/PROJECT_CHARTER_PL.md"
REFOUNDATION_FILE = "docs/PROJECT_REFOUNDATION_AUDIT_2026_07_22_PL.md"
MILESTONE_FILE = "docs/scan_import/TERRAIN_VISIBLE_PASS_2026_07_22_PL.md"

REQUIRED_FILES = (
    "README.md",
    "CONTRIBUTING.md",
    "AGENTS.md",
    "AI_PROJECT_MEMORY.md",
    ".automation/CONTROL.yaml",
    ".automation/POLICY.md",
    ".automation/RUNTIME_PROMPT.md",
    ".automation/CONTROL_ISSUE_TEMPLATE.md",
    "README_FOR_AGENTS.md",
    "docs/README.md",
    "docs/REPOSITORY_STRUCTURE_PL.md",
    "docs/PROJECT_OPERATING_PLAN_PL.md",
    CHARTER_FILE,
    REFOUNDATION_FILE,
    "docs/scan_import/CURRENT_STATE.md",
    MILESTONE_FILE,
    ".github/PULL_REQUEST_TEMPLATE/manual.md",
    ".github/PULL_REQUEST_TEMPLATE/automation.md",
    ".github/workflows/automation-foundation.yml",
    ".github/workflows/p1-scan-inspector.yml",
    ".github/workflows/repository-governance.yml",
)

PROTECTED_GOVERNANCE_PATHS = {
    "README.md",
    "CONTRIBUTING.md",
    "AGENTS.md",
    ".automation/**",
    "tools/automation/**",
    "tools/project/**",
    "tests/automation/**",
    "tests/project/**",
    ".github/workflows/**",
    ".github/PULL_REQUEST_TEMPLATE/**",
    "docs/README.md",
    "docs/REPOSITORY_STRUCTURE_PL.md",
}


def read_text(root: Path, relative: str) -> str:
    return (root / relative).read_text(encoding="utf-8")


def extract(pattern: str, text: str, label: str, errors: list[str]) -> str | None:
    match = re.search(pattern, text, flags=re.MULTILINE)
    if match is None:
        errors.append(f"MISSING_FIELD:{label}")
        return None
    return match.group(1).strip()


def check_contains(text: str, needles: Iterable[str], label: str, errors: list[str]) -> None:
    for needle in needles:
        if needle not in text:
            errors.append(f"MISSING_MARKER:{label}:{needle}")


def normalize_head_authority(value: str) -> str:
    """Normalize prose-only wording without weakening the authority identity."""
    normalized = value.strip()
    if normalized.lower().startswith("read from "):
        normalized = normalized[len("read from ") :].strip()
    return normalized


def check_texture_before_collision(text: str, label: str, errors: list[str]) -> None:
    """Require the owner-selected product ordering in each current router.

    This is intentionally a prose/order tripwire, not a product implementation gate.
    It prevents documentation from silently moving collision ahead of textured visual
    context and the vehicle scale-reference scene.
    """
    markers = (
        "TEXTURED_SOURCE_PREVIEW",
        "VEHICLE_SCALE_REFERENCE_SCENE",
        "COLLISION",
    )
    positions: list[int] = []
    for marker in markers:
        position = text.find(marker)
        if position < 0:
            errors.append(f"MISSING_PRODUCT_ORDER_MARKER:{label}:{marker}")
            return
        positions.append(position)
    if not positions[0] < positions[1] < positions[2]:
        errors.append(f"PRODUCT_ORDER_DRIFT:{label}:TEXTURE_SCALE_COLLISION")


def audit_repository(root: Path = ROOT) -> list[str]:
    root = Path(root)
    errors: list[str] = []

    for relative in REQUIRED_FILES:
        if not (root / relative).is_file():
            errors.append(f"MISSING_REQUIRED_FILE:{relative}")

    if errors:
        return errors

    try:
        control = common.load_control(root / ".automation" / "CONTROL.yaml")
    except Exception as exc:  # the strict validator provides the useful detail
        return [f"CONTROL_INVALID:{exc}"]

    if control["authority"]["global_agent_rules"] != "AGENTS.md":
        errors.append("GLOBAL_RULES_NOT_AGENTS")

    protected = set(control["scope"]["protected_control_paths"])
    missing_protected = sorted(PROTECTED_GOVERNANCE_PATHS - protected)
    if missing_protected:
        errors.append("PROTECTED_GOVERNANCE_INCOMPLETE:" + ",".join(missing_protected))

    agents = read_text(root, "AGENTS.md")
    vehicle_manual = read_text(root, "README_FOR_AGENTS.md")
    policy = read_text(root, ".automation/POLICY.md")
    runtime = read_text(root, ".automation/RUNTIME_PROMPT.md")
    root_readme = read_text(root, "README.md")
    contributing = read_text(root, "CONTRIBUTING.md")
    docs_index = read_text(root, "docs/README.md")
    structure = read_text(root, "docs/REPOSITORY_STRUCTURE_PL.md")
    memory = read_text(root, "AI_PROJECT_MEMORY.md")
    current = read_text(root, "docs/scan_import/CURRENT_STATE.md")
    operating = read_text(root, "docs/PROJECT_OPERATING_PLAN_PL.md")
    charter = read_text(root, CHARTER_FILE)
    refoundation = read_text(root, REFOUNDATION_FILE)
    milestone = read_text(root, MILESTONE_FILE)
    issue_template = read_text(root, ".automation/CONTROL_ISSUE_TEMPLATE.md")

    check_contains(
        agents,
        ("globalnym wejściem", "docs/REPOSITORY_STRUCTURE_PL.md", "repository_audit.py"),
        "AGENTS",
        errors,
    )
    check_contains(
        vehicle_manual,
        ("vehicle-domain manual", "This is not the global project front door"),
        "VEHICLE_MANUAL",
        errors,
    )
    if "README_FOR_AGENTS.md` — reguły globalne" in policy:
        errors.append("VEHICLE_MANUAL_PROMOTED_TO_GLOBAL_POLICY")
    if "README_FOR_AGENTS.md`\n6. checkpoint" in runtime:
        errors.append("RUNTIME_OLD_AUTHORITY_ORDER")

    check_contains(
        root_readme,
        ("Jozz Vehicle / Box3d_FunProject", "GitHub Control Issue", "Upstream Box3D"),
        "ROOT_README",
        errors,
    )
    check_contains(
        contributing,
        ("exact remote SHA", ".github/PULL_REQUEST_TEMPLATE/manual.md", "Definition of Done"),
        "CONTRIBUTING",
        errors,
    )
    check_contains(
        docs_index,
        (
            "Globalne źródła prawdy",
            "Aktywne current-state documents",
            "Walidacja driftu",
            "PROJECT_CHARTER_PL.md",
            "PROJECT_REFOUNDATION_AUDIT_2026_07_22_PL.md",
        ),
        "DOCS_INDEX",
        errors,
    )
    check_contains(
        structure,
        ("Upstream Box3D core", "Scan terrain pipeline", "Repository governance"),
        "REPOSITORY_STRUCTURE",
        errors,
    )
    check_contains(
        charter,
        (
            "Drążymy skałę kropla po kropli",
            "Kontekst wizualny przed promocją do fizyki",
            "TEXTURED_SOURCE_PREVIEW",
            "Warstwy prawdy świata",
        ),
        "PROJECT_CHARTER",
        errors,
    )
    check_contains(
        refoundation,
        (
            "PHASE_0_STARTED",
            "TEXTURED_SOURCE_PREVIEW",
            "forensic inventory",
            "nie jest jeszcze zakończona",
        ),
        "REFOUNDATION_AUDIT",
        errors,
    )
    check_contains(
        milestone,
        (
            "TERRAIN_VISIBLE_PASS",
            "SAME_REVISION_RESTART_PASS",
            "WORLD_SCALE_VALIDATED = false",
            "TEXTURED_PREVIEW_REQUIRED_BEFORE_SCALE_OR_COLLISION",
        ),
        "TERRAIN_VISIBLE_MILESTONE",
        errors,
    )

    branch_values = {
        "memory": extract(r"authoritative branch:\s*([^\n]+)", memory, "memory.branch", errors),
        "current": extract(r"\*\*Authoritative branch:\*\* `([^`]+)`", current, "current.branch", errors),
        "operating": extract(r"authoritative branch:\s*([^\n]+)", operating, "operating.branch", errors),
    }
    for source, value in branch_values.items():
        if value is not None and value != ACTIVE_BRANCH:
            errors.append(f"ACTIVE_BRANCH_DRIFT:{source}:{value}")

    pr_values = {
        "memory": extract(r"active draft PR:\s*(#[0-9]+)", memory, "memory.pr", errors),
        "current": extract(r"\*\*Active draft PR:\*\* (#[0-9]+)", current, "current.pr", errors),
        "operating": extract(r"active campaign PR:\s*(#[0-9]+)", operating, "operating.pr", errors),
    }
    for source, value in pr_values.items():
        if value is not None and value != ACTIVE_PR:
            errors.append(f"ACTIVE_PR_DRIFT:{source}:{value}")

    head_values = {
        "memory": extract(r"exact current head:\s*([^\n]+)", memory, "memory.head", errors),
        "current": extract(r"\*\*Exact current head:\*\* ([^\n]+)", current, "current.head", errors),
        "operating": extract(r"exact head:\s*([^\n]+)", operating, "operating.head", errors),
    }
    for source, value in head_values.items():
        if value is not None and normalize_head_authority(value) != CONTROL_HEAD_AUTHORITY:
            errors.append(f"EXACT_HEAD_AUTHORITY_DRIFT:{source}:{value}")

    current_documents = (
        ("AI_PROJECT_MEMORY.md", memory),
        ("docs/scan_import/CURRENT_STATE.md", current),
        ("docs/PROJECT_OPERATING_PLAN_PL.md", operating),
    )
    for relative, text in current_documents:
        if CURRENT_STATUS not in text:
            errors.append(f"STATUS_DRIFT:{relative}")
        if "WORLD_SCALE_VALIDATED" not in text:
            errors.append(f"SCALE_BOUNDARY_MISSING:{relative}")
        check_texture_before_collision(text, relative, errors)

    if ACTIVE_BRANCH in issue_template or "3a0d63e" in issue_template:
        errors.append("CONTROL_TEMPLATE_CONTAINS_LIVE_OR_STALE_BRANCH")
    check_contains(
        issue_template,
        ("OWNER_SELECTED_BRANCH", "0000000000000000000000000000000000000000"),
        "CONTROL_ISSUE_TEMPLATE",
        errors,
    )

    automation_workflow = read_text(root, ".github/workflows/automation-foundation.yml")
    scan_workflow = read_text(root, ".github/workflows/p1-scan-inspector.yml")
    governance_workflow = read_text(root, ".github/workflows/repository-governance.yml")
    for label, workflow in (
        ("automation", automation_workflow),
        ("scan", scan_workflow),
        ("governance", governance_workflow),
    ):
        if ACTIVE_BRANCH not in workflow:
            errors.append(f"WORKFLOW_MISSING_ACTIVE_BRANCH:{label}")
        if "workflow_dispatch:" not in workflow:
            errors.append(f"WORKFLOW_MISSING_MANUAL_DISPATCH:{label}")

    if (root / LEGACY_WORKFLOW).exists():
        errors.append(f"LEGACY_WORKFLOW_STILL_PRESENT:{LEGACY_WORKFLOW}")

    for path in sorted((root / ".github" / "workflows").glob("*.yml")):
        if re.search(r"^\s*schedule\s*:", path.read_text(encoding="utf-8"), re.MULTILINE):
            errors.append(f"UNEXPECTED_GITHUB_SCHEDULE:{path.relative_to(root).as_posix()}")

    return errors


def main() -> int:
    errors = audit_repository(ROOT)
    if errors:
        print("REPOSITORY_AUDIT_FAIL")
        for error in errors:
            print(f"- {error}")
        return 1
    print("REPOSITORY_AUDIT_PASS")
    print(f"authority={ACTIVE_BRANCH} pr={ACTIVE_PR} status={CURRENT_STATUS}")
    print("next_gate=TEXTURED_SOURCE_PREVIEW_BEFORE_SCALE_AND_COLLISION")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
