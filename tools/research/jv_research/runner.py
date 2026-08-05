"""Immutable run creation, execution, resume, sealing and human decisions."""
from __future__ import annotations

import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

from .core import (
    DECISION_STATUSES, PROMOTABLE_STATUSES, ROOT, RUN_SCHEMA, TERMINAL_CASE_STATES,
    ExperimentError, atomic_write, canonical_json, git_snapshot, level_map, load_json,
    proposal_complete, sha256_bytes, sha256_file, utc_now, validate_spec, variant_map,
)

DEFAULT_RUN_ROOT = ROOT / "build" / "research_runs"

def expand_arg(text: str, context: dict[str, str]) -> str:
    result = text
    for key, value in context.items():
        result = result.replace("{" + key + "}", value)
    unresolved = re.findall(r"\{[^{}]+\}", result)
    if unresolved:
        raise ExperimentError(f"nierozwiązane placeholdery w command: {unresolved}")
    return result


def case_context(
    spec: dict[str, Any], level: dict[str, Any], variant: dict[str, Any],
    run_dir: Path, case_dir: Path,
) -> dict[str, str]:
    context = {
        "python": sys.executable,
        "repo": str(ROOT),
        "run_dir": str(run_dir),
        "case_dir": str(case_dir),
        "experiment": spec["id"],
        "level": level["id"],
        "variant": variant["id"],
    }
    for key, value in variant["parameters"].items():
        context[f"param:{key}"] = str(value)
    return context


def selected_variants(spec: dict[str, Any], requested: Iterable[str] | None) -> list[dict[str, Any]]:
    variants = variant_map(spec)
    if not requested:
        return list(spec["variants"])
    result: list[dict[str, Any]] = []
    for variant_id in requested:
        try:
            result.append(variants[variant_id])
        except KeyError as exc:
            raise ExperimentError(f"nieznany wariant: {variant_id}") from exc
    baseline = spec["baseline_variant"]
    if baseline not in {item["id"] for item in result}:
        raise ExperimentError("każdy run porównawczy musi zawierać baseline_variant")
    return result


def validate_parent_runs(
    spec: dict[str, Any], level: dict[str, Any], parent_runs: Iterable[Path] | None
) -> list[dict[str, Any]]:
    required = set(level.get("depends_on", []))
    supplied = [path.resolve() for path in (parent_runs or [])]
    if not required and supplied:
        raise ExperimentError(f"poziom {level['id']} nie przyjmuje parent runów")
    records: dict[str, dict[str, Any]] = {}
    current_spec_hash = sha256_bytes(canonical_json(spec))
    for path in supplied:
        lock, parent_spec = load_run(path)
        decision_path = path / "human_decision.json"
        if not decision_path.is_file():
            raise ExperimentError(f"parent run nie ma jawnej decyzji: {path}")
        decision = load_json(decision_path)
        packet_path = path / "decision_packet.json"
        if not packet_path.is_file():
            raise ExperimentError(f"parent run nie ma decision packet: {path}")
        if decision.get("decision_packet_sha256") != sha256_file(packet_path):
            raise ExperimentError(f"parent {path} ma decyzję dla innej wersji decision packet")
        if decision.get("run_id") != lock.get("run_id") or decision.get("proposal_token") != lock.get("proposal_token"):
            raise ExperimentError(f"parent {path} ma decyzję niespójną z run.lock.json")
        if lock.get("experiment_id") != spec["id"]:
            raise ExperimentError(f"parent {path} należy do innego eksperymentu")
        if lock.get("spec_sha256") != current_spec_hash:
            raise ExperimentError(f"parent {path} używa innej wersji kontraktu eksperymentu")
        parent_level = lock.get("level")
        if parent_level not in required:
            raise ExperimentError(f"parent {path} ma poziom {parent_level!r}, niewymagany przez {level['id']}")
        if parent_level in records:
            raise ExperimentError(f"dwa parent runy dla poziomu {parent_level}")
        if decision.get("status") not in PROMOTABLE_STATUSES:
            raise ExperimentError(
                f"parent {path} ma decyzję {decision.get('status')!r}; awans wymaga {sorted(PROMOTABLE_STATUSES)}"
            )
        records[parent_level] = {
            "run_dir": str(path),
            "run_id": lock["run_id"],
            "level": parent_level,
            "decision_status": decision["status"],
            "decision_sha256": sha256_file(decision_path),
        }
    missing = required - set(records)
    if missing:
        raise ExperimentError(f"poziom {level['id']} wymaga zatwierdzonych parent runów: {sorted(missing)}")
    return [records[level_id] for level_id in level.get("depends_on", [])]


def create_run(
    spec_path: Path,
    spec: dict[str, Any],
    level_id: str,
    requested_variants: Iterable[str] | None,
    run_root: Path,
    parent_runs: Iterable[Path] | None = None,
) -> Path:
    if spec["state"] != "ready":
        blockers = "; ".join(spec.get("blockers", [])) or "brak executable contract"
        raise ExperimentError(f"{spec['id']} ma stan {spec['state']!r}; run zablokowany: {blockers}")

    levels = level_map(spec)
    if level_id not in levels:
        raise ExperimentError(f"nieznany poziom {level_id!r}; dostępne {sorted(levels)}")
    level = levels[level_id]
    parent_records = validate_parent_runs(spec, level, parent_runs)

    complete, message = proposal_complete()
    if not complete:
        raise ExperimentError(message)
    snapshot = git_snapshot()

    spec_bytes = canonical_json(spec)
    spec_hash = sha256_bytes(spec_bytes)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    run_id = f"{stamp}-{snapshot.head[:8]}-{spec_hash[:8]}"
    run_dir = run_root / spec["id"] / run_id
    if run_dir.exists():
        raise ExperimentError(f"run już istnieje: {run_dir}")
    run_dir.mkdir(parents=True)

    variants = selected_variants(spec, requested_variants)
    lock = {
        "schema": RUN_SCHEMA,
        "run_id": run_id,
        "experiment_id": spec["id"],
        "spec_source": str(spec_path.relative_to(ROOT)) if spec_path.is_relative_to(ROOT) else str(spec_path),
        "spec_sha256": spec_hash,
        "created_utc": utc_now(),
        "proposal_token": snapshot.proposal_token,
        "head": snapshot.head,
        "index_tree": snapshot.index_tree,
        "branch": snapshot.branch,
        "level": level_id,
        "variants": [item["id"] for item in variants],
        "parent_runs": parent_records,
    }
    atomic_write(run_dir / "spec.snapshot.json", json.dumps(spec, indent=2, ensure_ascii=False) + "\n")
    atomic_write(run_dir / "run.lock.json", json.dumps(lock, indent=2, ensure_ascii=False) + "\n")

    cases: list[dict[str, Any]] = []
    for variant in variants:
        case_id = f"{level_id}--{variant['id']}"
        case_dir = run_dir / "cases" / case_id
        case_dir.mkdir(parents=True)
        context = case_context(spec, level, variant, run_dir, case_dir)
        command = [expand_arg(arg, context) for arg in spec["execution"]["command"]]
        env = {
            key: expand_arg(str(value), context)
            for key, value in spec["execution"].get("env", {}).items()
        }
        case = {
            "case_id": case_id,
            "level": level_id,
            "variant": variant["id"],
            "parameters": variant["parameters"],
            "state": "PENDING",
            "attempts": 0,
            "command": command,
            "env": env,
            "expected_artifacts": spec["execution"].get("expected_artifacts", []),
        }
        cases.append(case)
        atomic_write(case_dir / "case.json", json.dumps(case, indent=2, ensure_ascii=False) + "\n")

    status = {
        "schema": RUN_SCHEMA,
        "run_id": run_id,
        "updated_utc": utc_now(),
        "state": "CREATED",
        "cases": {case["case_id"]: case["state"] for case in cases},
    }
    atomic_write(run_dir / "status.json", json.dumps(status, indent=2, ensure_ascii=False) + "\n")
    return run_dir


def load_run(run_dir: Path) -> tuple[dict[str, Any], dict[str, Any]]:
    lock = load_json(run_dir / "run.lock.json")
    spec = load_json(run_dir / "spec.snapshot.json")
    if lock.get("schema") != RUN_SCHEMA:
        raise ExperimentError(f"{run_dir}: nieobsługiwany run schema")
    errors = validate_spec(spec, str(run_dir / "spec.snapshot.json"))
    if errors:
        raise ExperimentError("snapshot spec jest niepoprawny:\n  - " + "\n  - ".join(errors))
    if sha256_bytes(canonical_json(spec)) != lock.get("spec_sha256"):
        raise ExperimentError("spec.snapshot.json nie odpowiada run.lock.json")
    return lock, spec


def verify_resume(lock: dict[str, Any]) -> None:
    complete, message = proposal_complete()
    if not complete:
        raise ExperimentError(message)
    snapshot = git_snapshot()
    if snapshot.proposal_token != lock["proposal_token"]:
        raise ExperimentError(
            "run należy do innej propozycji:\n"
            f"  zapisano: {lock['proposal_token']}\n"
            f"  obecnie:  {snapshot.proposal_token}"
        )


def read_case(case_path: Path) -> dict[str, Any]:
    case = load_json(case_path)
    if not isinstance(case, dict) or "case_id" not in case:
        raise ExperimentError(f"uszkodzony case: {case_path}")
    return case


def write_case(case_path: Path, case: dict[str, Any]) -> None:
    atomic_write(case_path, json.dumps(case, indent=2, ensure_ascii=False) + "\n")


def update_status(run_dir: Path) -> dict[str, Any]:
    states: dict[str, str] = {}
    for case_path in sorted((run_dir / "cases").glob("*/case.json")):
        case = read_case(case_path)
        states[case["case_id"]] = case["state"]
    if states and all(state == "PASSED" for state in states.values()):
        overall = "EXECUTED"
    elif states and all(state in TERMINAL_CASE_STATES for state in states.values()):
        overall = "EXECUTED_WITH_FAILURES"
    elif any(state == "RUNNING" for state in states.values()):
        overall = "RUNNING"
    else:
        overall = "INCOMPLETE"
    status = {
        "schema": RUN_SCHEMA,
        "run_id": run_dir.name,
        "updated_utc": utc_now(),
        "state": overall,
        "cases": states,
    }
    atomic_write(run_dir / "status.json", json.dumps(status, indent=2, ensure_ascii=False) + "\n")
    return status


def artifact_records(case_dir: Path, expected: list[str]) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for relative in expected:
        path = case_dir / relative
        if path.is_file():
            records.append({
                "path": relative,
                "exists": True,
                "bytes": path.stat().st_size,
                "sha256": sha256_file(path),
            })
        else:
            records.append({"path": relative, "exists": False})
    return records


def execute_cases(run_dir: Path, retry_failed: bool = False) -> int:
    lock, spec = load_run(run_dir)
    verify_resume(lock)
    timeout = spec["execution"].get("timeout_seconds", 300)
    any_failed = False

    for case_path in sorted((run_dir / "cases").glob("*/case.json")):
        case = read_case(case_path)
        state = case["state"]
        if state == "PASSED" or state == "SKIPPED":
            print(f"SKIP {case['case_id']} — {state}")
            continue
        if state == "FAILED" and not retry_failed:
            print(f"SKIP {case['case_id']} — FAILED (użyj --retry-failed)")
            any_failed = True
            continue

        case_dir = case_path.parent
        case["state"] = "RUNNING"
        case["attempts"] = int(case.get("attempts", 0)) + 1
        case["started_utc"] = utc_now()
        write_case(case_path, case)
        update_status(run_dir)

        print(f"RUN {case['case_id']}")
        print("$ " + " ".join(shlex.quote(arg) for arg in case["command"]))
        started = time.monotonic()
        env = os.environ.copy()
        env.update(case.get("env", {}))
        timed_out = False
        try:
            result = subprocess.run(
                case["command"],
                cwd=str(ROOT),
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=env,
                timeout=timeout,
                check=False,
            )
            returncode = result.returncode
            stdout = result.stdout
            stderr = result.stderr
        except subprocess.TimeoutExpired as exc:
            timed_out = True
            returncode = 124
            stdout = exc.stdout if isinstance(exc.stdout, str) else ""
            stderr = exc.stderr if isinstance(exc.stderr, str) else ""
            stderr += f"\nJV Research OS: TIMEOUT after {timeout}s\n"
        duration = time.monotonic() - started

        (case_dir / "stdout.log").write_text(stdout, encoding="utf-8", newline="\n")
        (case_dir / "stderr.log").write_text(stderr, encoding="utf-8", newline="\n")
        artifacts = artifact_records(case_dir, case.get("expected_artifacts", []))
        artifacts_ok = all(item["exists"] for item in artifacts)
        success = returncode == 0 and artifacts_ok and not timed_out

        result_record = {
            "schema": RUN_SCHEMA,
            "case_id": case["case_id"],
            "attempt": case["attempts"],
            "finished_utc": utc_now(),
            "duration_seconds": round(duration, 6),
            "returncode": returncode,
            "timed_out": timed_out,
            "artifacts": artifacts,
            "success": success,
        }
        atomic_write(case_dir / "result.json", json.dumps(result_record, indent=2, ensure_ascii=False) + "\n")
        case["state"] = "PASSED" if success else "FAILED"
        case["finished_utc"] = result_record["finished_utc"]
        case["last_returncode"] = returncode
        write_case(case_path, case)
        update_status(run_dir)
        if success:
            print(f"PASS {case['case_id']} ({duration:.3f}s)")
        else:
            any_failed = True
            missing = [item["path"] for item in artifacts if not item["exists"]]
            suffix = f"; brak artefaktów {missing}" if missing else ""
            print(f"FAIL {case['case_id']} rc={returncode}{suffix}", file=sys.stderr)

    status = update_status(run_dir)
    print(f"Run state: {status['state']}")
    return 1 if any_failed else 0


def print_status(run_dir: Path) -> None:
    lock, spec = load_run(run_dir)
    status = update_status(run_dir)
    print(f"Run: {lock['run_id']}")
    print(f"Eksperyment: {spec['id']} — {spec['title']}")
    print(f"Proposal token: {lock['proposal_token']}")
    print(f"Poziom: {lock['level']}")
    print(f"Stan: {status['state']}")
    for case_id, state in status["cases"].items():
        print(f"  - {case_id}: {state}")


def file_record(path: Path, relative_to: Path) -> dict[str, Any]:
    return {
        "path": str(path.relative_to(relative_to)).replace("\\", "/"),
        "bytes": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def verify_case_integrity(case_dir: Path, case: dict[str, Any], result: dict[str, Any]) -> list[dict[str, Any]]:
    for name in ("case.json", "result.json", "stdout.log", "stderr.log"):
        if not (case_dir / name).is_file():
            raise ExperimentError(f"{case['case_id']}: brak surowego pliku {name}")
    for artifact in result.get("artifacts", []):
        if not artifact.get("exists"):
            continue
        path = case_dir / artifact["path"]
        if not path.is_file():
            raise ExperimentError(f"{case['case_id']}: artefakt zniknął po wykonaniu: {artifact['path']}")
        if path.stat().st_size != artifact.get("bytes") or sha256_file(path) != artifact.get("sha256"):
            raise ExperimentError(f"{case['case_id']}: artefakt zmieniony po wykonaniu: {artifact['path']}")
    paths = [case_dir / "case.json", case_dir / "result.json", case_dir / "stdout.log", case_dir / "stderr.log"]
    paths.extend(case_dir / artifact["path"] for artifact in result.get("artifacts", []) if artifact.get("exists"))
    unique = sorted(set(paths), key=lambda item: str(item))
    return [file_record(path, case_dir) for path in unique]


def seal_run(run_dir: Path) -> Path:
    if (run_dir / "human_decision.json").exists():
        raise ExperimentError("run ma już ludzką decyzję; nie wolno ponownie sealować packetu")
    lock, spec = load_run(run_dir)
    status = update_status(run_dir)
    cases: list[dict[str, Any]] = []
    for case_path in sorted((run_dir / "cases").glob("*/case.json")):
        case = read_case(case_path)
        result_path = case_path.parent / "result.json"
        result = load_json(result_path) if result_path.exists() else None
        raw_files = verify_case_integrity(case_path.parent, case, result) if result is not None else []
        cases.append({
            "case_id": case["case_id"],
            "variant": case["variant"],
            "parameters": case["parameters"],
            "state": case["state"],
            "result": result,
            "raw_files": raw_files,
        })

    if not cases or not all(case["state"] in TERMINAL_CASE_STATES for case in cases):
        raise ExperimentError("nie można sealować niepełnego runu; wszystkie case'y muszą być terminalne")

    packet = {
        "schema": "jv-decision-packet/v1",
        "sealed_utc": utc_now(),
        "experiment_id": spec["id"],
        "run_id": lock["run_id"],
        "proposal_token": lock["proposal_token"],
        "level": lock["level"],
        "execution_state": status["state"],
        "automatic_verdict": None,
        "decision_state": "READY_FOR_ANALYSIS" if status["state"] == "EXECUTED" else "INCONCLUSIVE",
        "question": spec["question"],
        "hypothesis": spec["hypothesis"],
        "primary_factor": spec["primary_factor"],
        "locked_factors": spec["locked_factors"],
        "confounds": spec["confounds"],
        "metrics": spec["metrics"],
        "manual_gates": spec["manual_gates"],
        "cases": cases,
        "note": "Brak automatycznego werdyktu fizycznego. Packet zachowuje wykonanie do osobnej analizy i decyzji Jozza.",
    }
    packet_path = run_dir / "decision_packet.json"
    atomic_write(packet_path, json.dumps(packet, indent=2, ensure_ascii=False) + "\n")

    lines = [
        f"# Decision packet — {spec['id']}",
        "",
        f"- Run: `{lock['run_id']}`",
        f"- Proposal: `{lock['proposal_token']}`",
        f"- Poziom: `{lock['level']}`",
        f"- Wykonanie: **{status['state']}**",
        f"- Stan decyzji: **{packet['decision_state']}**",
        "- Automatyczny werdykt fizyczny: **BRAK**",
        "",
        "## Pytanie",
        "",
        spec["question"],
        "",
        "## Case'y",
        "",
    ]
    for case in cases:
        rc = case["result"]["returncode"] if case["result"] else "brak"
        lines.append(f"- `{case['case_id']}` — {case['state']}, rc={rc}, parametry={case['parameters']}")
    lines += [
        "",
        "## Dalej",
        "",
        "Przeanalizować metryki i kontrole przeciwne. Status SUPPORTED/REFUTED nadaje osobna decyzja, nie runner.",
        "",
    ]
    atomic_write(run_dir / "decision_packet.md", "\n".join(lines))
    return packet_path



def _safe_component(value: str, label: str) -> str:
    if not value or Path(value).name != value or value in {".", ".."}:
        raise ExperimentError(f"{label} ma niebezpieczną wartość: {value!r}")
    return value


def _verify_sealed_run_for_publish(
    run_dir: Path,
) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any], Path]:
    lock, spec = load_run(run_dir)
    packet_path = run_dir / "decision_packet.json"
    packet_md_path = run_dir / "decision_packet.md"
    decision_path = run_dir / "human_decision.json"
    if not packet_path.is_file() or not packet_md_path.is_file():
        raise ExperimentError("run nie ma kompletnego decision packet; najpierw wykonaj seal")
    if not decision_path.is_file():
        raise ExperimentError("publikacja wymaga jawnej decyzji człowieka")

    packet = load_json(packet_path)
    decision = load_json(decision_path)
    for label, value, expected in (
        ("packet run_id", packet.get("run_id"), lock.get("run_id")),
        ("packet experiment_id", packet.get("experiment_id"), lock.get("experiment_id")),
        ("packet proposal_token", packet.get("proposal_token"), lock.get("proposal_token")),
        ("decision run_id", decision.get("run_id"), lock.get("run_id")),
        ("decision experiment_id", decision.get("experiment_id"), lock.get("experiment_id")),
        ("decision proposal_token", decision.get("proposal_token"), lock.get("proposal_token")),
    ):
        if value != expected:
            raise ExperimentError(f"{label} jest niespójny z run.lock.json")
    if decision.get("decision_packet_sha256") != sha256_file(packet_path):
        raise ExperimentError("decyzja wskazuje inną wersję decision packet")

    packet_cases = {item.get("case_id"): item for item in packet.get("cases", [])}
    case_paths = sorted((run_dir / "cases").glob("*/case.json"))
    if not case_paths or set(packet_cases) != {path.parent.name for path in case_paths}:
        raise ExperimentError("decision packet nie odpowiada aktualnemu zestawowi case'ów")
    for case_path in case_paths:
        case = read_case(case_path)
        result_path = case_path.parent / "result.json"
        if not result_path.is_file():
            raise ExperimentError(f"{case['case_id']}: brak result.json")
        result = load_json(result_path)
        current = verify_case_integrity(case_path.parent, case, result)
        recorded = packet_cases[case["case_id"]].get("raw_files", [])
        if current != recorded:
            raise ExperimentError(f"{case['case_id']}: surowe pliki zmieniły się po seal")

    decision_hash = sha256_file(decision_path)
    history_matches = [
        path for path in sorted((run_dir / "decisions").glob("*.json"))
        if sha256_file(path) == decision_hash
    ]
    if len(history_matches) != 1:
        raise ExperimentError("bieżąca decyzja nie ma jednoznacznego wpisu w append-only history")
    return lock, spec, packet, decision, history_matches[0]


def publish_run(run_dir: Path, destination_root: Path | None = None) -> Path:
    run_dir = run_dir.resolve()
    lock, spec, packet, decision, current_history = _verify_sealed_run_for_publish(run_dir)
    experiment_id = _safe_component(str(lock["experiment_id"]), "experiment_id")
    run_id = _safe_component(str(lock["run_id"]), "run_id")
    root = (destination_root or (ROOT / "tools" / "research" / "evidence")).resolve()
    destination = root / experiment_id / run_id
    if destination.exists():
        raise ExperimentError(f"publikacja już istnieje i nie zostanie nadpisana: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temp_dir = Path(tempfile.mkdtemp(prefix=f".{run_id}.publish-", dir=destination.parent))

    copied: list[dict[str, Any]] = []
    omitted_empty: list[dict[str, Any]] = []

    def copy_verified(source: Path, published_relative: Path) -> None:
        if not source.is_file():
            raise ExperimentError(f"brak pliku publikacji: {source.relative_to(run_dir)}")
        target = temp_dir / published_relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        if source.stat().st_size != target.stat().st_size or sha256_file(source) != sha256_file(target):
            raise ExperimentError(f"kopiowanie zmieniło plik: {source.relative_to(run_dir)}")
        copied.append({
            "source_path": str(source.relative_to(run_dir)).replace("\\", "/"),
            "published_path": str(published_relative).replace("\\", "/"),
            "bytes": target.stat().st_size,
            "sha256": sha256_file(target),
        })

    try:
        for name in ("run.lock.json", "status.json", "decision_packet.json", "decision_packet.md"):
            copy_verified(run_dir / name, Path(name))
        for history in sorted((run_dir / "decisions").glob("*.json")):
            copy_verified(history, Path("decisions") / history.name)

        for case_path in sorted((run_dir / "cases").glob("*/case.json")):
            case_dir = case_path.parent
            case_id = _safe_component(case_dir.name, "case_id")
            result = load_json(case_dir / "result.json")
            copy_verified(case_path, Path("cases") / case_id / "case.json")
            copy_verified(case_dir / "result.json", Path("cases") / case_id / "result.json")
            for artifact in result.get("artifacts", []):
                if artifact.get("exists"):
                    relative = Path(artifact["path"])
                    if relative.is_absolute() or ".." in relative.parts:
                        raise ExperimentError(f"{case_id}: niebezpieczna ścieżka artefaktu {relative}")
                    copy_verified(case_dir / relative, Path("cases") / case_id / relative)
            for log_name in ("stdout.log", "stderr.log"):
                source = case_dir / log_name
                if source.stat().st_size == 0:
                    omitted_empty.append({
                        "source_path": str(source.relative_to(run_dir)).replace("\\", "/"),
                        "bytes": 0,
                        "sha256": sha256_file(source),
                        "reason": "empty_log",
                    })
                else:
                    copy_verified(
                        source,
                        Path("cases") / case_id / (Path(log_name).stem + ".txt"),
                    )

        manifest = {
            "schema": "jv-published-run/v1",
            "published_utc": utc_now(),
            "experiment_id": experiment_id,
            "run_id": run_id,
            "level": lock["level"],
            "proposal_token": lock["proposal_token"],
            "execution_state": packet["execution_state"],
            "decision_status": decision["status"],
            "decision_packet_sha256": sha256_file(run_dir / "decision_packet.json"),
            "current_decision_path": str(
                (Path("decisions") / current_history.name)
            ).replace("\\", "/"),
            "spec_sha256": lock["spec_sha256"],
            "spec_snapshot": spec,
            "files": sorted(copied, key=lambda item: item["published_path"]),
            "omitted_empty_files": sorted(omitted_empty, key=lambda item: item["source_path"]),
            "not_copied": [
                {"source_path": "spec.snapshot.json", "reason": "embedded_in_manifest"},
                {"source_path": "human_decision.json", "reason": "points_to_append_only_history"},
            ],
        }
        atomic_write(temp_dir / "PUBLISH_MANIFEST.json", json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")

        for record in copied:
            target = temp_dir / record["published_path"]
            if target.stat().st_size != record["bytes"] or sha256_file(target) != record["sha256"]:
                raise ExperimentError(f"weryfikacja publikacji nie przeszła: {record['published_path']}")
        os.replace(temp_dir, destination)
    except Exception:
        shutil.rmtree(temp_dir, ignore_errors=True)
        raise
    return destination

def decision_stamp() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")


def record_decision(run_dir: Path, status: str, decided_by: str, note: str) -> Path:
    if status not in DECISION_STATUSES:
        raise ExperimentError(f"nieznany status decyzji {status!r}")
    if not decided_by.strip() or not note.strip():
        raise ExperimentError("decyzja wymaga --decided-by i niepustego --note")
    lock, spec = load_run(run_dir)
    packet_path = run_dir / "decision_packet.json"
    if not packet_path.is_file():
        raise ExperimentError("najpierw wykonaj seal; decyzja musi wskazywać zamknięty decision packet")
    packet = load_json(packet_path)
    if packet.get("run_id") != lock["run_id"]:
        raise ExperimentError("decision packet nie odpowiada run.lock.json")

    current_path = run_dir / "human_decision.json"
    previous_hash = sha256_file(current_path) if current_path.is_file() else None
    decision = {
        "schema": "jv-human-decision/v1",
        "decided_utc": utc_now(),
        "experiment_id": spec["id"],
        "run_id": lock["run_id"],
        "level": lock["level"],
        "proposal_token": lock["proposal_token"],
        "decision_packet_sha256": sha256_file(packet_path),
        "status": status,
        "decided_by": decided_by.strip(),
        "note": note.strip(),
        "supersedes_sha256": previous_hash,
    }
    history = run_dir / "decisions" / f"{decision_stamp()}--{status}.json"
    atomic_write(history, json.dumps(decision, indent=2, ensure_ascii=False) + "\n")
    atomic_write(current_path, json.dumps(decision, indent=2, ensure_ascii=False) + "\n")
    return history
