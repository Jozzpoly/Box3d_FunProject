#!/usr/bin/env python3
"""Run the evidence regression suite in small isolated process shards.

The suite creates many temporary repositories and sandbox trees. A single
unittest process accumulates enough resources to become slow or appear hung in
constrained environments. Shards keep failures attributable and make the run
resumable without weakening the test set.
"""
from __future__ import annotations

import argparse
import ast
import os
import re
import signal
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
TEST_FILE = HERE / "test_evidence.py"
TEST_CLASS = "EvidenceChainTest"


def discover_methods() -> list[str]:
    tree = ast.parse(TEST_FILE.read_text(encoding="utf-8"), filename=str(TEST_FILE))
    methods: list[str] = []
    for node in tree.body:
        if isinstance(node, ast.ClassDef) and node.name == TEST_CLASS:
            for child in node.body:
                if isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef)) and child.name.startswith("test_"):
                    methods.append(child.name)
    if not methods:
        raise RuntimeError(f"no tests found in {TEST_FILE}")
    return methods


def test_number(name: str) -> int | None:
    match = re.match(r"test_T(\d+)", name)
    return int(match.group(1)) if match else None


def shards(methods: list[str]) -> list[tuple[str, str, list[str]]]:
    # Keep each resumable shard small enough for interactive/agent runners.
    # Variant names (T4b, T14c, etc.) stay with their numbered family.
    definitions = [
        ("a", "T1-T3", lambda n: n is not None and 1 <= n <= 3),
        ("b", "T4-T5", lambda n: n is not None and 4 <= n <= 5),
        ("c", "T6-T9", lambda n: n is not None and 6 <= n <= 9),
        ("d", "T10-T11", lambda n: n is not None and 10 <= n <= 11),
        ("e", "T12-T14", lambda n: n is not None and 12 <= n <= 14),
        ("f", "T15-T17", lambda n: n is not None and 15 <= n <= 17),
        ("g", "T18-T20", lambda n: n is not None and 18 <= n <= 20),
        ("h", "T21-T23", lambda n: n is not None and 21 <= n <= 23),
        ("i", "T24", lambda n: n == 24),
        ("j", "T25", lambda n: n == 25),
        ("k", "T26+ and extras", lambda n: n is None or n >= 26),
    ]
    output: list[tuple[str, str, list[str]]] = []
    for key, label, predicate in definitions:
        selected = [method for method in methods if predicate(test_number(method))]
        output.append((key, label, selected))
    return output


def terminate_process_tree(process: subprocess.Popen) -> None:
    """Stop the whole shard process tree, not only the unittest parent."""
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    else:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass

    try:
        process.wait(timeout=5)
        return
    except subprocess.TimeoutExpired:
        pass

    if os.name == "nt":
        process.kill()
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        # A process stuck in an uninterruptible OS wait must not wedge the gate.
        # It may remain for the OS to reap, but the caller still receives TIMEOUT.
        pass


def run_command(command: list[str], cwd: Path, timeout_seconds: int) -> int:
    kwargs = {
        "cwd": str(cwd),
        "start_new_session": os.name != "nt",
    }
    if os.name == "nt":
        kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP

    process = subprocess.Popen(command, **kwargs)
    try:
        return process.wait(timeout=timeout_seconds)
    except subprocess.TimeoutExpired:
        terminate_process_tree(process)
        return 124


def run_shard(key: str, label: str, methods: list[str], timeout_seconds: int) -> int:
    if not methods:
        print(f"evidence-regression: shard {key} ({label}) is empty", file=sys.stderr)
        return 2

    print(
        f"\n===== evidence shard {key}: {label} ({len(methods)} tests, one fresh process each) =====",
        flush=True,
    )
    for index, method in enumerate(methods, start=1):
        selector = f"{TEST_CLASS}.{method}"
        # The runner already prints the exact method name. Quiet unittest output
        # avoids repeating long docstrings and keeps long gates stream-friendly.
        command = [sys.executable, str(TEST_FILE), "-q", selector]
        print(f"--- {key}.{index}/{len(methods)} {method} ---", flush=True)
        rc = run_command(command, HERE.parents[1], timeout_seconds)
        if rc == 124:
            print(
                f"evidence-regression: TIMEOUT — {method} exceeded {timeout_seconds}s; process tree stopped",
                file=sys.stderr,
            )
            return rc
        if rc != 0:
            print(
                f"evidence-regression: FAIL — {method} returned {rc}",
                file=sys.stderr,
            )
            return rc
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="show shards without running them")
    parser.add_argument(
        "--timeout-seconds",
        type=int,
        default=300,
        help="hard timeout for one isolated test process; prevents a wedged child from blocking the gate",
    )
    parser.add_argument(
        "--shard",
        choices=("a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "all"),
        default="all",
        help="run one resumable shard or the complete suite",
    )
    args = parser.parse_args()

    if args.timeout_seconds < 1:
        print("evidence-regression: ERROR — --timeout-seconds must be positive", file=sys.stderr)
        return 2

    try:
        discovered = discover_methods()
    except (OSError, SyntaxError, RuntimeError) as exc:
        print(f"evidence-regression: ERROR — {exc}", file=sys.stderr)
        return 2

    partition = shards(discovered)
    if args.list:
        for key, label, methods in partition:
            print(f"{key}: {label} (one process/test, timeout {args.timeout_seconds}s): {', '.join(methods)}")
        return 0

    selected = partition if args.shard == "all" else [item for item in partition if item[0] == args.shard]
    for key, label, methods in selected:
        rc = run_shard(key, label, methods, args.timeout_seconds)
        if rc != 0:
            print(f"evidence-regression: FAIL — shard {key} ({label}) returned {rc}", file=sys.stderr)
            return rc

    count = sum(len(methods) for _, _, methods in selected)
    print(f"\nevidence-regression: OK — {count} tests in {count} fresh process(es), {len(selected)} shard(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
