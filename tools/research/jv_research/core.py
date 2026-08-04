"""Experiment contracts, repository proposal fingerprint and planning output."""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SPEC_DIR = ROOT / "tools" / "research" / "experiments"
SPEC_SCHEMA = "jv-experiment/v1"
RUN_SCHEMA = "jv-run/v1"
ID_RE = re.compile(r"^[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)+$")
SAFE_NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]*$")
STATES = {"planned", "blocked", "ready", "complete"}
TERMINAL_CASE_STATES = {"PASSED", "FAILED", "SKIPPED"}
DECISION_STATUSES = {"PROVISIONAL", "SUPPORTED", "STRONGLY_SUPPORTED", "REFUTED", "INCONCLUSIVE"}
PROMOTABLE_STATUSES = {"SUPPORTED", "STRONGLY_SUPPORTED"}

class ExperimentError(RuntimeError):
    pass


@dataclass(frozen=True)
class GitSnapshot:
    head: str
    index_tree: str
    proposal_token: str
    branch: str


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def canonical_json(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def atomic_write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(text, encoding="utf-8", newline="\n")
    temp.replace(path)


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ExperimentError(f"nie znaleziono pliku: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ExperimentError(f"{path}: niepoprawny JSON: {exc}") from exc


def run_git(*args: str, capture: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *args],
        cwd=str(ROOT),
        capture_output=capture,
        text=True,
        encoding="utf-8",
        check=False,
    )


def proposal_complete() -> tuple[bool, str]:
    conflicts = run_git("diff", "--name-only", "--diff-filter=U")
    if conflicts.returncode != 0:
        return False, conflicts.stderr.strip() or "nie można sprawdzić konfliktów"
    if conflicts.stdout.strip():
        return False, "nierozwiązane konflikty: " + ", ".join(conflicts.stdout.splitlines())

    unstaged = subprocess.run(["git", "diff", "--quiet", "--"], cwd=str(ROOT), check=False)
    if unstaged.returncode not in (0, 1):
        return False, "nie można sprawdzić zmian unstaged"
    if unstaged.returncode == 1:
        return False, "istnieją zmiany tracked poza indeksem; stage'uj kompletną propozycję"

    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "-z"],
        cwd=str(ROOT), capture_output=True, check=False,
    )
    if untracked.returncode != 0:
        return False, "nie można sprawdzić plików untracked"
    paths = [raw.decode("utf-8", errors="replace") for raw in untracked.stdout.split(b"\0") if raw]
    if paths:
        preview = ", ".join(paths[:8]) + (" …" if len(paths) > 8 else "")
        return False, "nieignorowane pliki untracked nie należą do propozycji: " + preview
    return True, "worktree odpowiada staged proposal (lub jest czysty)"


def git_snapshot() -> GitSnapshot:
    values: dict[str, str] = {}
    for name, args in {
        "head": ("rev-parse", "HEAD"),
        "index_tree": ("write-tree",),
        "branch": ("branch", "--show-current"),
    }.items():
        result = run_git(*args)
        if result.returncode != 0:
            raise ExperimentError(result.stderr.strip() or f"git {' '.join(args)} nie powiódł się")
        values[name] = result.stdout.strip()
    return GitSnapshot(
        head=values["head"],
        index_tree=values["index_tree"],
        proposal_token=f"{values['head']}:{values['index_tree']}",
        branch=values["branch"],
    )


def require_type(value: Any, expected: type, label: str, errors: list[str]) -> bool:
    if not isinstance(value, expected):
        errors.append(f"{label}: oczekiwano {expected.__name__}, jest {type(value).__name__}")
        return False
    return True


def validate_spec(spec: Any, source: str = "spec") -> list[str]:
    errors: list[str] = []
    if not require_type(spec, dict, source, errors):
        return errors

    if spec.get("schema") != SPEC_SCHEMA:
        errors.append(f"schema: oczekiwano {SPEC_SCHEMA!r}, jest {spec.get('schema')!r}")

    experiment_id = spec.get("id")
    if not isinstance(experiment_id, str) or not ID_RE.fullmatch(experiment_id):
        errors.append("id: wymagany format np. 'WHEEL-SOFT-03'")

    state = spec.get("state")
    if state not in STATES:
        errors.append(f"state: dozwolone {sorted(STATES)}, jest {state!r}")

    experiment_deps = spec.get("depends_on_experiments")
    if not isinstance(experiment_deps, list) or not all(isinstance(item, str) and ID_RE.fullmatch(item) for item in experiment_deps):
        errors.append("depends_on_experiments: wymagana lista poprawnych identyfikatorów eksperymentów")
    elif experiment_id in experiment_deps:
        errors.append("depends_on_experiments: eksperyment nie może zależeć od siebie")

    order = spec.get("order")
    if not isinstance(order, int) or order < 0:
        errors.append("order: wymagana nieujemna liczba całkowita określająca kolejność programu")

    for field in ("title", "question", "hypothesis", "primary_factor"):
        if not isinstance(spec.get(field), str) or not spec[field].strip():
            errors.append(f"{field}: wymagany niepusty tekst")

    locked = spec.get("locked_factors")
    if not require_type(locked, dict, "locked_factors", errors) or not locked:
        errors.append("locked_factors: wymagany niepusty obiekt zamrożonych czynników")

    for field in ("confounds", "metrics", "promotion_rules", "manual_gates"):
        value = spec.get(field)
        if not require_type(value, list, field, errors) or not value:
            errors.append(f"{field}: wymagana niepusta lista")

    variants = spec.get("variants")
    baseline = spec.get("baseline_variant")
    primary = spec.get("primary_factor")
    variant_ids: set[str] = set()
    if require_type(variants, list, "variants", errors):
        if len(variants) < 2:
            errors.append("variants: wymagane co najmniej baseline i jeden kandydat")
        for index, variant in enumerate(variants):
            label = f"variants[{index}]"
            if not require_type(variant, dict, label, errors):
                continue
            variant_id = variant.get("id")
            if not isinstance(variant_id, str) or not SAFE_NAME_RE.fullmatch(variant_id):
                errors.append(f"{label}.id: wymagany bezpieczny identyfikator [A-Za-z0-9_.-]")
            elif variant_id in variant_ids:
                errors.append(f"{label}.id: duplikat {variant_id!r}")
            else:
                variant_ids.add(variant_id)
            params = variant.get("parameters")
            if require_type(params, dict, f"{label}.parameters", errors):
                keys = set(params)
                if isinstance(primary, str) and keys != {primary}:
                    errors.append(
                        f"{label}.parameters: eksperyment może zmieniać tylko primary_factor {primary!r}; "
                        f"otrzymano {sorted(keys)}"
                    )
        if baseline not in variant_ids:
            errors.append(f"baseline_variant: {baseline!r} nie istnieje w variants")

    levels = spec.get("levels")
    level_ids: list[str] = []
    if require_type(levels, list, "levels", errors):
        if not levels:
            errors.append("levels: wymagana co najmniej jedna warstwa rigu")
        for index, level in enumerate(levels):
            label = f"levels[{index}]"
            if not require_type(level, dict, label, errors):
                continue
            level_id = level.get("id")
            if not isinstance(level_id, str) or not SAFE_NAME_RE.fullmatch(level_id):
                errors.append(f"{label}.id: wymagany bezpieczny identyfikator [A-Za-z0-9_.-]")
                continue
            if level_id in level_ids:
                errors.append(f"{label}.id: duplikat {level_id!r}")
            level_ids.append(level_id)
            for field in ("rig", "purpose", "entry_gate", "exit_gate"):
                if not isinstance(level.get(field), str) or not level[field].strip():
                    errors.append(f"{label}.{field}: wymagany niepusty tekst")
            deps = level.get("depends_on", [])
            if not isinstance(deps, list) or not all(isinstance(item, str) for item in deps):
                errors.append(f"{label}.depends_on: wymagana lista identyfikatorów")
            else:
                unknown = [item for item in deps if item not in level_ids]
                if unknown:
                    errors.append(f"{label}.depends_on: zależności muszą wskazywać wcześniejsze poziomy: {unknown}")

    blockers = spec.get("blockers", [])
    if not isinstance(blockers, list) or not all(isinstance(item, str) and item.strip() for item in blockers):
        errors.append("blockers: wymagana lista niepustych tekstów")
    if state in {"planned", "blocked"} and not blockers:
        errors.append(f"blockers: stan {state!r} wymaga jawnej listy blokad")
    if state == "ready" and blockers:
        errors.append("blockers: eksperyment ready nie może mieć aktywnych blokad")

    execution = spec.get("execution")
    if state == "ready":
        if not require_type(execution, dict, "execution", errors):
            pass
        else:
            command = execution.get("command")
            if not isinstance(command, list) or not command or not all(isinstance(item, str) for item in command):
                errors.append("execution.command: wymagana niepusta lista argumentów, bez shell stringa")
            timeout = execution.get("timeout_seconds", 300)
            if not isinstance(timeout, int) or timeout <= 0:
                errors.append("execution.timeout_seconds: wymagana dodatnia liczba całkowita")
            artifacts = execution.get("expected_artifacts", [])
            if not isinstance(artifacts, list) or not all(isinstance(item, str) for item in artifacts):
                errors.append("execution.expected_artifacts: wymagana lista ścieżek względnych")
            else:
                for item in artifacts:
                    artifact = Path(item)
                    if artifact.is_absolute() or ".." in artifact.parts or item in {"", "."}:
                        errors.append(f"execution.expected_artifacts: niedozwolona ścieżka {item!r}")

    return errors


def load_spec(path: Path) -> dict[str, Any]:
    spec = load_json(path)
    errors = validate_spec(spec, str(path))
    if errors:
        raise ExperimentError("spec odrzucony:\n  - " + "\n  - ".join(errors))
    return spec


def variant_map(spec: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {variant["id"]: variant for variant in spec["variants"]}


def level_map(spec: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {level["id"]: level for level in spec["levels"]}


def format_value(value: Any) -> str:
    if isinstance(value, (dict, list)):
        return json.dumps(value, ensure_ascii=False, sort_keys=True)
    return str(value)


def print_plan(spec: dict[str, Any]) -> None:
    print(f"{spec['id']} — {spec['title']}")
    print(f"Stan: {spec['state']}")
    print(f"Pytanie: {spec['question']}")
    print(f"Hipoteza: {spec['hypothesis']}")
    print(f"Zmienna główna: {spec['primary_factor']}")
    print(f"Baseline: {spec['baseline_variant']}")
    print("\nWarianty:")
    for variant in spec["variants"]:
        value = variant["parameters"][spec["primary_factor"]]
        marker = " [BASELINE]" if variant["id"] == spec["baseline_variant"] else ""
        print(f"  - {variant['id']}: {format_value(value)}{marker}")
    print("\nZamrożone czynniki:")
    for key, value in spec["locked_factors"].items():
        print(f"  - {key}: {format_value(value)}")
    print("\nDrabina awansu:")
    for level in spec["levels"]:
        deps = ", ".join(level.get("depends_on", [])) or "brak"
        print(f"  - {level['id']} / {level['rig']} (zależności: {deps})")
        print(f"      wejście: {level['entry_gate']}")
        print(f"      wyjście: {level['exit_gate']}")
    if spec.get("blockers"):
        print("\nAktywne blokady:")
        for blocker in spec["blockers"]:
            print(f"  - {blocker}")
    print("\nMetryki:")
    for metric in spec["metrics"]:
        print(f"  - {metric}")
    print("\nSystem nie nadaje automatycznie statusu SUPPORTED; generuje materiał do decyzji.")
