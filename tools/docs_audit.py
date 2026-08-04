#!/usr/bin/env python3
"""Small, deterministic documentation gate for JV.

The goal is not prose linting. It catches the failure classes that repeatedly
made this repository hard to continue: multiple current front doors, references
to moved plans, missing local links, and historical rules leaking back into the
active instructions.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
ARCHIVE = DOCS / "archive"
FINDINGS = DOCS / "KOLA_FINDINGS.json"
FINDINGS_DOC = DOCS / "KOLA_01_DOWODY_PL.md"
FINDINGS_BEGIN = "<!-- FINDINGS_CATALOG:BEGIN -->"
FINDINGS_END = "<!-- FINDINGS_CATALOG:END -->"

REQUIRED = [
    ROOT / "README_FOR_AGENTS.md",
    ROOT / "JOZZ_VEHICLE_README_PL.md",
    DOCS / "JV_DOCS_INDEX_PL.md",
    DOCS / "CURRENT_STATE_INDEX_PL.md",
    DOCS / "CHECKPOINTS_PL.md",
    DOCS / "TECH_DEBT_PL.md",
    DOCS / "JV_JES_HERITAGE_PL.md",
    DOCS / "JV_RESEARCH_OS_PL.md",
    DOCS / "MAPA_INDEX_PL.md",
    DOCS / "ASSET_CONTRACT_PL.md",
    DOCS / "SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md",
    DOCS / "SUBSYSTEM_UI_PRESETS_PL.md",
    DOCS / "KOLA_00_INDEX_PL.md",
    DOCS / "KOLA_01_DOWODY_PL.md",
    DOCS / "KOLA_02_ARCHITEKTURA_PL.md",
    DOCS / "KOLA_03_POLITYKA_BOX3D_PL.md",
    DOCS / "KOLA_04_PETLA_BADAWCZA_PL.md",
    DOCS / "KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md",
    FINDINGS,
    DOCS / "JOZZ_CORE_PATCHES.json",
    ARCHIVE / "README_PL.md",
    ARCHIVE / "consolidated_2026-08" / "README_PL.md",
]

ACTIVE_PROJECT_DOCS = {
    "ASSET_CONTRACT_PL.md",
    "CHECKPOINTS_PL.md",
    "CURRENT_STATE_INDEX_PL.md",
    "HOTKEY_AUDIT_PL.md",  # forbidden below; kept here only for a clear error if restored
    "JV_DOCS_INDEX_PL.md",
    "JV_JES_HERITAGE_PL.md",
    "JV_RESEARCH_OS_PL.md",
    "KOLA_00_INDEX_PL.md",
    "KOLA_01_DOWODY_PL.md",
    "KOLA_02_ARCHITEKTURA_PL.md",
    "KOLA_03_POLITYKA_BOX3D_PL.md",
    "KOLA_04_PETLA_BADAWCZA_PL.md",
    "KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md",
    "MAPA_INDEX_PL.md",
    "SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md",
    "SUBSYSTEM_UI_PRESETS_PL.md",
    "TECH_DEBT_PL.md",
}

# Source -> tokens that must be routed explicitly from that source. These are
# intentionally few and structural. They prevent a valid document from becoming
# practically undiscoverable in the next conversation.
REQUIRED_ROUTES = {
    ROOT / "README_FOR_AGENTS.md": [
        "docs/CURRENT_STATE_INDEX_PL.md",
        "docs/JV_DOCS_INDEX_PL.md",
    ],
    ROOT / "JOZZ_VEHICLE_README_PL.md": [
        "docs/CURRENT_STATE_INDEX_PL.md",
        "docs/KOLA_00_INDEX_PL.md",
    ],
    DOCS / "JV_DOCS_INDEX_PL.md": [
        "CURRENT_STATE_INDEX_PL.md",
        "CHECKPOINTS_PL.md",
        "TECH_DEBT_PL.md",
        "MAPA_INDEX_PL.md",
        "JV_JES_HERITAGE_PL.md",
        "JV_RESEARCH_OS_PL.md",
        "ASSET_CONTRACT_PL.md",
        "SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md",
        "SUBSYSTEM_UI_PRESETS_PL.md",
        "KOLA_00_INDEX_PL.md",
        "KOLA_01_DOWODY_PL.md",
        "KOLA_02_ARCHITEKTURA_PL.md",
        "KOLA_03_POLITYKA_BOX3D_PL.md",
        "KOLA_04_PETLA_BADAWCZA_PL.md",
        "KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md",
        "JV_RESEARCH_OS_PL.md",
        "KOLA_FINDINGS.json",
        "JOZZ_CORE_PATCHES.json",
    ],
    DOCS / "CURRENT_STATE_INDEX_PL.md": [
        "KOLA_00_INDEX_PL.md",
        "TECH_DEBT_PL.md",
        "MAPA_INDEX_PL.md",
        "JV_RESEARCH_OS_PL.md",
    ],
    DOCS / "KOLA_00_INDEX_PL.md": [
        "KOLA_02_ARCHITEKTURA_PL.md",
        "KOLA_04_PETLA_BADAWCZA_PL.md",
        "KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md",
        "KOLA_FINDINGS.json",
    ],
    DOCS / "KOLA_02_ARCHITEKTURA_PL.md": ["KOLA_03_POLITYKA_BOX3D_PL.md"],
    DOCS / "KOLA_04_PETLA_BADAWCZA_PL.md": ["KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md"],
    DOCS / "KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md": ["JV_RESEARCH_OS_PL.md"],
    DOCS / "JV_RESEARCH_OS_PL.md": ["tools/jv_lab.py", "tools/research/experiments/WHEEL-SOFT-03.json"],
    DOCS / "SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md": ["ASSET_CONTRACT_PL.md"],
}


CURRENT_AUTHORITY = [p for p in REQUIRED if p.suffix == ".md" and p != ARCHIVE / "README_PL.md"]
FORBIDDEN_CURRENT = {
    "jozz-vehicle-sandbox-m0": "stara gałąź nie może być bieżącą instrukcją",
    "engine core is untouched": "rdzeń JV nie jest już nietykalny",
    "rdzeń silnika pozostaje nietknięty": "rdzeń JV nie jest już nietykalny",
    "docs/jes_pakiet_dla_sola_2026_07_15": "pakiet JES został zarchiwizowany",
}

# These files were deliberately moved out of the active documentation root.
MOVED_BASENAMES = {
    "PROJECT_DIRECTION_PL.md",
    "M6_SUSPENSION_RIG_FOUNDATION_PL.md",
    "M7_REAL_FORCES_FOUNDATION_PL.md",
    "M8_SUSPENSION_RIG_REPAIR_PLAN_PL.md",
    "AUDIT_PHYSICS_STEERING_2026_07_08_PL.md",
    "AUDIT_WERYFIKACJA_P1_P6_2026_07_09_PL.md",
    "PLAN_STABILNOSC_PROWADZENIE_PL.md",
    "PLAN_PORZADKI_FUNDAMENT_2026_07_09_PL.md",
    "PLAN_WIELKI_REFACTOR_2026_07_09_PL.md",
    "PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md",
    "PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md",
    "MAPA_ETAP_1_FUNDAMENT_TERENU_PL.md",
    "MAPA_ETAP_2_PRZESZKODY_I_POLIGONY_PL.md",
    "PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md",
    "KOLA_PRZEKAZANIE_KOLIDER_KOLA_PL.md",
    "ASSET_CONTRACT_RUNTIME_V1_PL.md",
    "ASSET_CONTRACT_V2_DRAFT_PL.md",
    "HOTKEY_AUDIT_PL.md",
    "SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md",
    "KOLA_05_PROTOKOL_STENDU_V21_PL.md",
}

LINK_RE = re.compile(r"(?<!!)\[[^\]]*\]\(([^)]+)\)")
BACKTICK_PATH_RE = re.compile(r"`((?:docs|tools|samples|src|include|assets)/[^`\n]+)`")
HISTORICAL_ROOT_RE = re.compile(
    r"^(?:PLAN_|AUDIT_|AUDYT_|FINALIZACJA_|M[0-9]+_|ODZYSK_UTRACONYCH_)"
)
TEXT_SUFFIXES = {".md", ".c", ".cpp", ".h", ".py", ".ps1", ".txt"}

# Curated cross-file contracts whose names have drifted before. Keep the list
# short: it is a tripwire, not an attempt to parse C++.
CODE_DOC_TERMS = [
    ("[ARCADE]", ROOT / "samples/jozz_vehicle_m6_rig_lab_ui_tabs.cpp", DOCS / "SUBSYSTEM_UI_PRESETS_PL.md"),
    ("rackFrictionBase", ROOT / "samples/jozz_vehicle_m6_suspension_rig.h", DOCS / "SUBSYSTEM_UI_PRESETS_PL.md"),
    ("rackFrictionLoadCoeff", ROOT / "samples/jozz_vehicle_m6_suspension_rig.h", DOCS / "SUBSYSTEM_UI_PRESETS_PL.md"),
    ("LoadJozzVehicleM6PresetConfig", ROOT / "samples/jozz_vehicle_m6_config_io.h", DOCS / "SUBSYSTEM_UI_PRESETS_PL.md"),
    ("assets/vehicle_spawns.txt", ROOT / "samples/jozz_vehicle_m6_rig_lab_internal.h", DOCS / "SUBSYSTEM_UI_PRESETS_PL.md"),
    ("DrawTelescopingDamper", ROOT / "samples/jozz_vehicle_visual_mesh_draw.cpp", DOCS / "SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md"),
    ("LoadJozzVehicleAssetContract", ROOT / "samples/jozz_vehicle_asset_contract.cpp", DOCS / "ASSET_CONTRACT_PL.md"),
    ("physicsAuthority", ROOT / "samples/jozz_vehicle_asset_contract.cpp", DOCS / "ASSET_CONTRACT_PL.md"),
    ("B3_SPECULATIVE_DISTANCE", ROOT / "src/wheel_shape.c", DOCS / "KOLA_02_ARCHITEKTURA_PL.md"),
]


def rel(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def tracked_paths() -> list[Path]:
    result = subprocess.run(
        ["git", "-C", str(ROOT), "ls-files", "-z"],
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        message = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(message or "git ls-files failed")
    return [
        ROOT / raw.decode("utf-8", errors="strict")
        for raw in result.stdout.split(b"\0")
        if raw
    ]


def active_text_files() -> list[Path]:
    out: list[Path] = []
    for path in tracked_paths():
        if path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        if ARCHIVE in path.parents:
            continue
        if path.is_file():
            out.append(path)
    return out


def resolve_link(source: Path, raw: str) -> Path | None:
    target = raw.strip().split(maxsplit=1)[0].strip("<>")
    if not target or target.startswith(("http://", "https://", "mailto:", "#")):
        return None
    target = unquote(target.split("#", 1)[0])
    if not target:
        return None
    return (source.parent / target).resolve()



def finding_sort_key(finding_id: str) -> tuple[str, int, str]:
    match = re.fullmatch(r"([A-Za-z]+)-(\d+)", finding_id)
    if match is None:
        return (finding_id, 0, finding_id)
    return (match.group(1), int(match.group(2)), finding_id)


def render_findings_catalog() -> str:
    data = json.loads(FINDINGS.read_text("utf-8"))
    findings = data.get("findings")
    if not isinstance(findings, dict):
        raise ValueError("KOLA_FINDINGS.json: pole 'findings' musi być obiektem")

    lines = [
        FINDINGS_BEGIN,
        "",
        "> Ta sekcja jest generowana z `KOLA_FINDINGS.json`. Status i treść",
        "> zmieniają się w rejestrze, a następnie odtwarza poleceniem",
        "> `python tools/docs_audit.py --fix-findings`.",
        "",
    ]
    for finding_id in sorted(findings, key=finding_sort_key):
        entry = findings[finding_id]
        if not isinstance(entry, dict):
            raise ValueError(f"KOLA_FINDINGS.json: {finding_id} nie jest obiektem")
        status = str(entry.get("status", "BRAK_STATUSU")).strip()
        claim = str(entry.get("claim", "BRAK_OPISU")).strip().replace("\n", " ")
        lines.append(f"- **{finding_id}** · `{status}` — {claim}")
    lines.extend(["", FINDINGS_END])
    return "\n".join(lines)


def replace_findings_catalog(text: str, catalog: str) -> str:
    begin = text.find(FINDINGS_BEGIN)
    end = text.find(FINDINGS_END)
    if begin == -1 and end == -1:
        suffix = "" if text.endswith("\n") else "\n"
        return (
            text
            + suffix
            + "\n## 9. Katalog rejestru findingów (generowany)\n\n"
            + catalog
            + "\n"
        )
    if begin == -1 or end == -1 or end < begin:
        raise ValueError("KOLA_01: uszkodzone markery katalogu findingów")
    end += len(FINDINGS_END)
    return text[:begin] + catalog + text[end:]


def check_findings_catalog(errors: list[str]) -> None:
    if not FINDINGS.is_file() or not FINDINGS_DOC.is_file():
        return
    try:
        expected = render_findings_catalog()
        text = FINDINGS_DOC.read_text("utf-8")
        begin = text.find(FINDINGS_BEGIN)
        end = text.find(FINDINGS_END)
        if begin == -1 or end == -1 or end < begin:
            errors.append(
                "REJESTR: brak kompletnego generowanego katalogu findingów w "
                f"{rel(FINDINGS_DOC)}"
            )
            return
        end += len(FINDINGS_END)
        actual = text[begin:end].replace("\r\n", "\n")
        if actual != expected:
            errors.append(
                "REJESTR: katalog findingów jest nieaktualny; uruchom "
                "python tools/docs_audit.py --fix-findings"
            )
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        errors.append(f"REJESTR: {exc}")


def fix_findings_catalog() -> None:
    catalog = render_findings_catalog()
    text = FINDINGS_DOC.read_text("utf-8")
    updated = replace_findings_catalog(text, catalog)
    FINDINGS_DOC.write_text(updated, encoding="utf-8", newline="\n")

def check() -> list[str]:
    errors: list[str] = []

    for path in REQUIRED:
        if not path.is_file():
            errors.append(f"BRAK: {rel(path)}")

    for source, tokens in REQUIRED_ROUTES.items():
        if not source.is_file():
            continue
        source_text = source.read_text("utf-8", errors="replace")
        for token in tokens:
            if token not in source_text:
                errors.append(f"BRAK TRASY: {rel(source)} -> {token}")

    # Uppercase/project-owned Markdown at docs root must be represented by the
    # curated active set. This catches a new plan/report that silently becomes
    # a second front door. Explicitly retired names get a stronger message.
    retired_active_names = {
        "ASSET_CONTRACT_RUNTIME_V1_PL.md",
        "ASSET_CONTRACT_V2_DRAFT_PL.md",
        "HOTKEY_AUDIT_PL.md",
        "SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md",
        "KOLA_05_PROTOKOL_STENDU_V21_PL.md",
    }
    for path in DOCS.glob("*.md"):
        project_like = path.name.upper() == path.name or path.name.startswith((
            "ASSET_", "CHECKPOINTS_", "CURRENT_", "HOTKEY_", "JV_", "KOLA_",
            "MAPA_", "SUBSYSTEM_", "SUSPENSION_", "TECH_",
        ))
        if not project_like:
            continue
        if path.name in retired_active_names:
            errors.append(f"SCALONY DOKUMENT WRÓCIŁ DO DOCS/: {path.name}")
        elif path.name not in ACTIVE_PROJECT_DOCS:
            errors.append(f"OSIEROCONY AKTYWNY DOKUMENT: {path.name} (dodaj do mapy albo archiwizuj)")

    for path in CURRENT_AUTHORITY:
        if not path.is_file():
            continue
        text = path.read_text("utf-8", errors="replace").lower()
        for term, reason in FORBIDDEN_CURRENT.items():
            if term.lower() in text:
                errors.append(f"STALE: {rel(path)} zawiera '{term}' ({reason})")

    for path in DOCS.glob("*.md"):
        if HISTORICAL_ROOT_RE.match(path.name):
            errors.append(f"ARCHIWIZUJ: historyczny plik nadal w docs/: {path.name}")

    for path in active_text_files():
        text = path.read_text("utf-8", errors="replace")

        # A moved historical basename must carry its archive path. This covers
        # source-code comments too; checking only literal ``docs/<name>`` let
        # bare stale references survive after a move. The audit's own registry
        # is data, not a repository reference.
        if path.resolve() != Path(__file__).resolve():
            for basename in MOVED_BASENAMES:
                for line_number, line in enumerate(text.splitlines(), start=1):
                    if basename not in line:
                        continue
                    if "archive/" in line or "docs/archive/" in line:
                        continue
                    errors.append(
                        f"STARE ODNIESIENIE: {rel(path)}:{line_number} -> {basename} "
                        "(podaj jawna sciezke archiwalna)"
                    )

        if path.suffix.lower() == ".md" and (DOCS in path.parents or path.name in {"README_FOR_AGENTS.md", "JOZZ_VEHICLE_README_PL.md"}):
            for match in LINK_RE.finditer(text):
                resolved = resolve_link(path, match.group(1))
                if resolved is not None and not resolved.exists():
                    errors.append(
                        f"MARTWY LINK: {rel(path)} -> {match.group(1)}"
                    )

            for match in BACKTICK_PATH_RE.finditer(text):
                token = match.group(1)
                # Skip documented glob/brace notation and command arguments.
                if any(ch in token for ch in "*{}<>") or " " in token:
                    continue
                token = re.sub(r":[0-9]+(?:[-–][0-9]+)?$", "", token)
                candidate = (ROOT / token).resolve()
                if not candidate.exists():
                    errors.append(f"MARTWA ŚCIEŻKA: {rel(path)} -> {token}")

    for subdir in [
        ARCHIVE / "vehicle_legacy_2026-07",
        ARCHIVE / "map_scan_2026-07",
        ARCHIVE / "wheels",
        ARCHIVE / "jes_foundation_2026-07-15",
        ARCHIVE / "ledgers",
        ARCHIVE / "consolidated_2026-08",
    ]:
        if not subdir.is_dir():
            errors.append(f"BRAK KATALOGU ARCHIWUM: {rel(subdir)}")
            continue
        for path in subdir.glob("*.md"):
            first = path.read_text("utf-8", errors="replace")[:160]
            if not first.startswith("> **ARCHIWUM"):
                errors.append(f"BRAK BANNERA ARCHIWUM: {rel(path)}")

    for term, code_path, doc_path in CODE_DOC_TERMS:
        if not code_path.is_file():
            errors.append(f"DRIFT: brak pliku kodu {rel(code_path)} dla '{term}'")
            continue
        if not doc_path.is_file():
            errors.append(f"DRIFT: brak dokumentu {rel(doc_path)} dla '{term}'")
            continue
        in_code = term in code_path.read_text("utf-8", errors="replace")
        in_doc = term in doc_path.read_text("utf-8", errors="replace")
        if not in_code:
            errors.append(
                f"DRIFT: nieaktualny straznik '{term}' — brak w {rel(code_path)}"
            )
        elif not in_doc:
            errors.append(
                f"DRIFT: '{term}' jest w {rel(code_path)}, ale brak w {rel(doc_path)}"
            )

    check_findings_catalog(errors)
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fix-findings",
        action="store_true",
        help="odtwórz generowany katalog findingów w KOLA_01",
    )
    args = parser.parse_args()
    if args.fix_findings:
        fix_findings_catalog()

    errors = check()
    if errors:
        print(f"docs-audit: FAIL ({len(errors)})")
        for error in errors:
            print(f"- {error}")
        return 1
    print("docs-audit: OK — authority, routing, archive and code-doc contracts are coherent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
