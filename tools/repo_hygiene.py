#!/usr/bin/env python3
"""Deterministic repository-hygiene gate for JV.

This checks the proposed Git-tracked tree, not ignored local build products.
It catches the classes of repository pollution that are expensive to discover
later on Windows: generated artifacts committed by accident, case-insensitive
path collisions, unsafe Windows names, unexplained empty files, mixed line
endings, duplicate blobs and unexpectedly large files.
"""
from __future__ import annotations

import hashlib
import subprocess
import sys
import unicodedata
from collections import defaultdict
from pathlib import Path, PurePosixPath

MAX_TRACKED_FILE_BYTES = 8 * 1024 * 1024

FORBIDDEN_COMPONENTS = {
    "__pycache__",
    ".cache",
    ".fetchcontent-cache",
    ".mypy_cache",
    ".pytest_cache",
    ".ruff_cache",
    ".vs",
    ".vscode-test",
    "build",
    "cmakefiles",
    "coverage",
    "dist",
    "node_modules",
    "out",
    "temp",
    "tmp",
}
FORBIDDEN_BASENAMES = {
    ".ds_store",
    "desktop.ini",
    "thumbs.db",
}
FORBIDDEN_SUFFIXES = {
    ".bak",
    ".ilk",
    ".log",
    ".old",
    ".orig",
    ".pdb",
    ".pyc",
    ".pyo",
    ".rej",
    ".swo",
    ".swp",
    ".tmp",
}
EMPTY_ALLOWLIST = {
    "samples/shaders/.gitkeep",
}
DUPLICATE_ALLOWLIST: set[frozenset[str]] = set()
TEXT_SUFFIXES = {
    ".bat",
    ".c",
    ".cmake",
    ".cpp",
    ".h",
    ".json",
    ".md",
    ".ps1",
    ".py",
    ".sh",
    ".txt",
    ".yaml",
    ".yml",
}
WINDOWS_RESERVED_NAMES = {
    "aux",
    "con",
    "nul",
    "prn",
    *(f"com{i}" for i in range(1, 10)),
    *(f"lpt{i}" for i in range(1, 10)),
}
WINDOWS_INVALID_CHARS = set('<>:"\\|?*')


def git_output(root: Path, *args: str) -> bytes:
    result = subprocess.run(
        ["git", *args], cwd=root, capture_output=True, check=False
    )
    if result.returncode != 0:
        message = result.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(message or f"git {' '.join(args)} failed")
    return result.stdout


def repository_root() -> Path:
    output = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    if output.returncode != 0:
        raise RuntimeError("uruchom narzędzie wewnątrz repozytorium Git")
    return Path(output.stdout.strip()).resolve()


def tracked_paths(root: Path) -> list[str]:
    raw = git_output(root, "ls-files", "-z", "--cached")
    return sorted(
        entry.decode("utf-8", errors="strict")
        for entry in raw.split(b"\0")
        if entry
    )


def tracked_modes(root: Path) -> dict[str, str]:
    raw = git_output(root, "ls-files", "-s", "-z")
    modes: dict[str, str] = {}
    for entry in raw.split(b"\0"):
        if not entry:
            continue
        metadata, path = entry.split(b"\t", 1)
        mode = metadata.split(b" ", 1)[0].decode("ascii")
        modes[path.decode("utf-8", errors="strict")] = mode
    return modes


def windows_key(path: str) -> str:
    return unicodedata.normalize("NFC", path).casefold()


def check_path(path: str, errors: list[str]) -> None:
    pure = PurePosixPath(path)
    lower_parts = [part.casefold() for part in pure.parts]

    forbidden = sorted(set(lower_parts) & FORBIDDEN_COMPONENTS)
    if forbidden:
        errors.append(
            f"ARTEFAKT: {path} zawiera zakazany katalog '{forbidden[0]}'"
        )

    name_lower = pure.name.casefold()
    if name_lower in FORBIDDEN_BASENAMES:
        errors.append(f"ARTEFAKT: {path} ma zakazaną nazwę pliku")
    if pure.suffix.casefold() in FORBIDDEN_SUFFIXES or pure.name.endswith("~"):
        errors.append(f"ARTEFAKT: {path} ma podejrzane rozszerzenie/nazwę")

    for segment in pure.parts:
        if segment.endswith((" ", ".")):
            errors.append(
                f"WINDOWS: segment '{segment}' w {path} kończy się spacją lub kropką"
            )
        if any(char in WINDOWS_INVALID_CHARS or ord(char) < 32 for char in segment):
            errors.append(f"WINDOWS: niedozwolony znak w segmencie '{segment}' ({path})")
        stem = segment.split(".", 1)[0].casefold()
        if stem in WINDOWS_RESERVED_NAMES:
            errors.append(f"WINDOWS: zarezerwowana nazwa '{segment}' w {path}")


def has_mixed_line_endings(data: bytes) -> bool:
    crlf = data.count(b"\r\n")
    lone_lf = data.count(b"\n") - crlf
    lone_cr = data.count(b"\r") - crlf
    return lone_cr > 0 or (crlf > 0 and lone_lf > 0)


def check(root: Path) -> list[str]:
    errors: list[str] = []
    paths = tracked_paths(root)
    modes = tracked_modes(root)

    by_windows_name: dict[str, list[str]] = defaultdict(list)
    by_blob: dict[tuple[int, str], list[str]] = defaultdict(list)

    for relative in paths:
        check_path(relative, errors)
        by_windows_name[windows_key(relative)].append(relative)

        mode = modes.get(relative, "")
        if mode == "120000":
            errors.append(
                f"SYMLINK: {relative} jest symlinkiem; JV ma być przenośne na Windows"
            )

        path = root / relative
        if not path.is_file():
            errors.append(f"BRAK: śledzony plik nie istnieje w worktree: {relative}")
            continue

        data = path.read_bytes()
        size = len(data)
        if size == 0 and relative not in EMPTY_ALLOWLIST:
            errors.append(f"PUSTY: {relative} nie ma jawnego wyjątku")
        if size > MAX_TRACKED_FILE_BYTES:
            errors.append(
                f"DUŻY BLOB: {relative} ma {size / (1024 * 1024):.2f} MiB "
                f"(limit {MAX_TRACKED_FILE_BYTES / (1024 * 1024):.0f} MiB)"
            )
        if size > 0:
            digest = hashlib.sha256(data).hexdigest()
            by_blob[(size, digest)].append(relative)
        if path.suffix.casefold() in TEXT_SUFFIXES and has_mixed_line_endings(data):
            errors.append(f"EOL: {relative} miesza CRLF/LF albo zawiera samotne CR")

    for group in by_windows_name.values():
        if len(group) > 1:
            errors.append(
                "WINDOWS CASE COLLISION: " + ", ".join(sorted(group))
            )

    for group in by_blob.values():
        if len(group) < 2:
            continue
        key = frozenset(group)
        if key not in DUPLICATE_ALLOWLIST:
            errors.append("DUPLIKAT: identyczne pliki: " + ", ".join(sorted(group)))

    return sorted(set(errors))


def main() -> int:
    try:
        root = repository_root()
        errors = check(root)
    except (OSError, RuntimeError, UnicodeDecodeError, ValueError) as exc:
        print(f"repo-hygiene: BŁĄD — {exc}", file=sys.stderr)
        return 2

    if errors:
        print(f"repo-hygiene: FAIL — {len(errors)} problem(ów)", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    count = len(tracked_paths(root))
    print(
        f"repo-hygiene: OK — {count} tracked files; no artifacts, collisions, "
        "duplicates or unexplained blobs"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
