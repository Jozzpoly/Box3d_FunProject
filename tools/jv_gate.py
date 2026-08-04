#!/usr/bin/env python3
"""One local entry point for JV quality profiles.

This orchestrates existing gates; it does not reinterpret their results. Each
profile is an ordered superset of the previous one and stops at the first
failure so the earliest violated contract stays visible.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Gate:
    name: str
    command: tuple[str, ...]


def py(relative: str, *args: str) -> tuple[str, ...]:
    return (sys.executable, str(ROOT / relative), *args)


QUICK = (
    Gate("documentation authority and routing", py("tools/docs_audit.py")),
    Gate("repository hygiene", py("tools/repo_hygiene.py")),
    Gate("owned Box3D core delta", py("tools/jozz_core_delta.py")),
    Gate("evidence integrity", py("tools/evidence/evidence.py", "check")),
    Gate("worktree whitespace", ("git", "diff", "--check")),
    Gate("index whitespace", ("git", "diff", "--cached", "--check")),
)

DEEP = QUICK + (
    Gate("hygiene tool regressions", py("tools/test_hygiene.py")),
    Gate("evidence regressions shard a (T1-T3)", py("tools/evidence/run_regression_tests.py", "--shard", "a")),
    Gate("evidence regressions shard b (T4-T5)", py("tools/evidence/run_regression_tests.py", "--shard", "b")),
    Gate("evidence regressions shard c (T6-T9)", py("tools/evidence/run_regression_tests.py", "--shard", "c")),
    Gate("evidence regressions shard d (T10-T11)", py("tools/evidence/run_regression_tests.py", "--shard", "d")),
    Gate("evidence regressions shard e (T12-T14)", py("tools/evidence/run_regression_tests.py", "--shard", "e")),
    Gate("evidence regressions shard f (T15-T17)", py("tools/evidence/run_regression_tests.py", "--shard", "f")),
    Gate("evidence regressions shard g (T18-T20)", py("tools/evidence/run_regression_tests.py", "--shard", "g")),
    Gate("evidence regressions shard h (T21-T23)", py("tools/evidence/run_regression_tests.py", "--shard", "h")),
    Gate("evidence regressions shard i (T24)", py("tools/evidence/run_regression_tests.py", "--shard", "i")),
    Gate("evidence regressions shard j (T25)", py("tools/evidence/run_regression_tests.py", "--shard", "j")),
    Gate("evidence regressions shard k (T26+)", py("tools/evidence/run_regression_tests.py", "--shard", "k")),
)

WHEEL = DEEP + (
    Gate("Wheel Scope instrument gates", py("tools/jozz_wheel_bench/check_all.py")),
)

PROFILES = {
    "quick": QUICK,
    "deep": DEEP,
    "wheel": WHEEL,
}


def full_profile() -> tuple[Gate, ...]:
    shell = shutil.which("pwsh") or shutil.which("powershell")
    if os.name != "nt" and shell is None:
        return WHEEL + (
            Gate(
                "Windows product gate (requires Windows/PowerShell)",
                (sys.executable, "-c", "import sys; print('full profile requires Windows product build', file=sys.stderr); sys.exit(2)"),
            ),
        )
    if shell is None:
        shell = "powershell"
    return WHEEL + (
        Gate("Windows product gate", (shell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools/gate.ps1"))),
    )



def proposal_complete() -> tuple[bool, str]:
    conflicts = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=U"],
        cwd=str(ROOT), capture_output=True, text=True, encoding="utf-8", check=False,
    )
    if conflicts.returncode != 0:
        return False, conflicts.stderr.strip() or "cannot inspect merge conflicts"
    if conflicts.stdout.strip():
        return False, "unresolved conflicts: " + ", ".join(conflicts.stdout.splitlines())

    unstaged = subprocess.run(
        ["git", "diff", "--quiet", "--"], cwd=str(ROOT), check=False
    )
    if unstaged.returncode not in (0, 1):
        return False, "cannot inspect unstaged changes"
    if unstaged.returncode == 1:
        return False, "unstaged tracked changes exist; stage the complete proposal or restore it before a checkpoint"

    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard", "-z"],
        cwd=str(ROOT), capture_output=True, check=False,
    )
    if untracked.returncode != 0:
        return False, "cannot inspect untracked files"
    paths = [raw.decode("utf-8", errors="replace") for raw in untracked.stdout.split(b"\0") if raw]
    if paths:
        preview = ", ".join(paths[:8]) + (" …" if len(paths) > 8 else "")
        return False, "untracked non-ignored files are outside the proposal: " + preview

    return True, "worktree matches the staged proposal (or is clean)"

def run_gate(gate: Gate) -> int:
    print(f"\n===== {gate.name} =====", flush=True)
    print("$ " + " ".join(gate.command), flush=True)
    try:
        result = subprocess.run(gate.command, cwd=str(ROOT))
    except OSError as exc:
        print(f"jv-gate: ERROR — cannot run {gate.name}: {exc}", file=sys.stderr)
        return 2
    if result.returncode != 0:
        print(f"jv-gate: FAIL — {gate.name} returned {result.returncode}", file=sys.stderr)
    return result.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", nargs="?", choices=("quick", "deep", "wheel", "full"), default="quick")
    parser.add_argument("--list", action="store_true", help="show gates in the selected profile")
    parser.add_argument("--start-at", type=int, default=1, help="resume at this 1-based gate number after proposal check")
    args = parser.parse_args()

    gates = full_profile() if args.profile == "full" else PROFILES[args.profile]
    if args.list:
        print("0. proposal completeness: no conflicts, unstaged tracked files or untracked non-ignored files")
        for index, gate in enumerate(gates, start=1):
            print(f"{index}. {gate.name}: {' '.join(gate.command)}")
        return 0

    complete, message = proposal_complete()
    print("===== proposal completeness =====")
    if not complete:
        print(f"jv-gate: FAIL — {message}", file=sys.stderr)
        return 1
    print(f"proposal: OK — {message}")

    if args.start_at < 1 or args.start_at > len(gates):
        print(f"jv-gate: FAIL — --start-at must be in 1..{len(gates)}", file=sys.stderr)
        return 2

    print(f"JV gate profile: {args.profile} ({len(gates)} gates), starting at {args.start_at}")
    for index, gate in enumerate(gates, start=1):
        if index < args.start_at:
            print(f"SKIP {index}. {gate.name}")
            continue
        print(f"GATE {index}/{len(gates)}", flush=True)
        rc = run_gate(gate)
        if rc != 0:
            print(f"Resume unchanged proposal with: python tools/jv_gate.py {args.profile} --start-at {index}", file=sys.stderr)
            return rc

    executed = len(gates) - args.start_at + 1
    print(f"\njv-gate: OK — profile {args.profile} passed ({executed} executed gates)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
