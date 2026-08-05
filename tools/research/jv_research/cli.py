"""Command-line front door for JV Research OS."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .core import (
    DECISION_STATUSES, DEFAULT_SPEC_DIR, ExperimentError, load_json, load_spec, print_plan,
)
from .runner import (
    DEFAULT_RUN_ROOT, create_run, execute_cases, print_status, publish_run, record_decision, seal_run,
)

def spec_paths() -> list[Path]:
    paths = list(DEFAULT_SPEC_DIR.glob("*.json"))
    def key(path: Path) -> tuple[int, str]:
        try:
            value = load_json(path)
            order = value.get("order") if isinstance(value, dict) else None
            return (order if isinstance(order, int) else 2**31 - 1, path.name)
        except ExperimentError:
            return (2**31 - 1, path.name)
    return sorted(paths, key=key)


def load_repository_specs() -> list[tuple[Path, dict[str, Any]]]:
    items = [(path, load_spec(path)) for path in spec_paths()]
    ids = {spec["id"] for _, spec in items}
    errors: list[str] = []
    order = {spec["id"]: spec["order"] for _, spec in items}
    for path, spec in items:
        for dependency in spec["depends_on_experiments"]:
            if dependency not in ids:
                errors.append(f"{path.name}: brak zależnego eksperymentu {dependency}")
            elif order[dependency] >= spec["order"]:
                errors.append(f"{path.name}: zależność {dependency} musi mieć mniejsze order")
    if errors:
        raise ExperimentError("repozytorium eksperymentów jest niespójne:\n  - " + "\n  - ".join(errors))
    return items


def cmd_list() -> int:
    for _, spec in load_repository_specs():
        blockers = len(spec.get("blockers", []))
        deps = ",".join(spec["depends_on_experiments"]) or "-"
        print(f"{spec['id']:<26} {spec['state']:<8} blockers={blockers} deps={deps:<20} {spec['title']}")
    return 0


def cmd_next() -> int:
    items = load_repository_specs()
    states = {spec["id"]: spec["state"] for _, spec in items}
    incomplete = []
    for _, spec in items:
        if spec["state"] == "complete":
            continue
        incomplete.append(spec)
        if all(states.get(dep) == "complete" for dep in spec["depends_on_experiments"]):
            print_plan(spec)
            return 0
    if incomplete:
        blocked = "; ".join(
            f"{spec['id']} czeka na {spec['depends_on_experiments']}" for spec in incomplete
        )
        raise ExperimentError("brak eksperymentu z domkniętymi zależnościami: " + blocked)
    print("Wszystkie zarejestrowane eksperymenty mają stan complete.")
    return 0

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("list", help="lista zarejestrowanych eksperymentów")
    sub.add_parser("next", help="najbliższy niezamknięty eksperyment")

    validate = sub.add_parser("validate", help="waliduj kontrakt eksperymentu")
    validate.add_argument("spec", type=Path)

    plan = sub.add_parser("plan", help="pokaż zamrożony plan eksperymentu")
    plan.add_argument("spec", type=Path)

    start = sub.add_parser("start", help="utwórz run i wykonaj case'y")
    start.add_argument("spec", type=Path)
    start.add_argument("--level", required=True)
    start.add_argument("--variant", action="append", dest="variants")
    start.add_argument("--run-root", type=Path, default=DEFAULT_RUN_ROOT)
    start.add_argument("--parent-run", action="append", type=Path, dest="parent_runs")
    start.add_argument("--create-only", action="store_true")

    resume = sub.add_parser("resume", help="wznów ten sam immutable run")
    resume.add_argument("run_dir", type=Path)
    resume.add_argument("--retry-failed", action="store_true")

    status = sub.add_parser("status", help="pokaż stan runu")
    status.add_argument("run_dir", type=Path)

    seal = sub.add_parser("seal", help="zamknij wykonanie w decision packet bez automatycznego werdyktu")
    seal.add_argument("run_dir", type=Path)

    decide = sub.add_parser("decide", help="zapisz jawną ludzką decyzję dla zapieczętowanego runu")
    decide.add_argument("run_dir", type=Path)
    decide.add_argument("--status", required=True, choices=sorted(DECISION_STATUSES))
    decide.add_argument("--decided-by", required=True)
    decide.add_argument("--note", required=True)

    publish = sub.add_parser("publish", help="opublikuj zapieczętowany run jako kuratorowane evidence")
    publish.add_argument("run_dir", type=Path)
    publish.add_argument("--destination-root", type=Path)

    args = parser.parse_args(argv)
    try:
        if args.command == "list":
            return cmd_list()
        if args.command == "next":
            return cmd_next()
        if args.command == "validate":
            spec = load_spec(args.spec.resolve())
            print(f"OK {spec['id']} — kontrakt poprawny")
            return 0
        if args.command == "plan":
            print_plan(load_spec(args.spec.resolve()))
            return 0
        if args.command == "start":
            spec_path = args.spec.resolve()
            spec = load_spec(spec_path)
            run_dir = create_run(
                spec_path, spec, args.level, args.variants, args.run_root.resolve(), args.parent_runs
            )
            print(f"Run utworzony: {run_dir}")
            if args.create_only:
                return 0
            return execute_cases(run_dir)
        if args.command == "resume":
            return execute_cases(args.run_dir.resolve(), retry_failed=args.retry_failed)
        if args.command == "status":
            print_status(args.run_dir.resolve())
            return 0
        if args.command == "seal":
            packet = seal_run(args.run_dir.resolve())
            print(f"Decision packet: {packet}")
            return 0
        if args.command == "publish":
            destination = publish_run(
                args.run_dir.resolve(),
                args.destination_root.resolve() if args.destination_root else None,
            )
            print(f"Published evidence: {destination}")
            return 0
        if args.command == "decide":
            decision = record_decision(
                args.run_dir.resolve(), args.status, args.decided_by, args.note
            )
            print(f"Human decision: {decision}")
            return 0
    except ExperimentError as exc:
        print(f"jv-lab: FAIL — {exc}", file=sys.stderr)
        return 2
    return 2
