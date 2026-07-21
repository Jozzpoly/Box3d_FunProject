#!/usr/bin/env python3
"""Run and record the P0 Windows baseline for photogrammetry import V2.

This tool is intentionally conservative. It refuses to run the project gate when:
- the checkout is not derived from the expected experiment branch;
- the current branch is main;
- the working tree is dirty;
- the raw archive is outside local_assets/scans;
- the archive is not ignored by Git;
- raw point-cloud/archive files are already tracked.

The generated report is local (under build/, which is ignored) and deliberately
contains only repository-relative paths. It never records the Windows username,
home directory, or georeferencing metadata from the scan.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import locale
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from typing import Any, Iterable, Sequence

FORMAT_ID = "jozz-photogrammetry-p0-baseline-v1"
DEFAULT_BASE_BRANCH = "Photogrametry_Import_experiment"
DEFAULT_BASE_COMMIT = "f1c4919e501721749084210aea9b571e96b69bed"
DEFAULT_OUTPUT_ROOT = Path("build/scan_pipeline/p0_baseline")
RAW_PATTERNS = ("*.rar", "*.ply", "*.las", "*.laz")


class BaselineError(RuntimeError):
    """A hard P0 guard or execution failure."""


def _run(
    args: Sequence[str],
    *,
    cwd: Path,
    check: bool = True,
    capture: bool = True,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        list(args),
        cwd=cwd,
        text=True,
        encoding=locale.getpreferredencoding(False) or "utf-8",
        errors="replace",
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
        check=False,
    )
    if check and result.returncode != 0:
        output = (result.stdout or "").strip()
        raise BaselineError(f"command failed ({result.returncode}): {' '.join(args)}\n{output}")
    return result


def _git(repo_root: Path, *args: str, check: bool = True) -> str:
    result = _run(("git", *args), cwd=repo_root, check=check)
    return (result.stdout or "").strip()


def sha256_file(path: Path, chunk_size: int = 1024 * 1024) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(chunk_size), b""):
            digest.update(chunk)
    return digest.hexdigest()


def ensure_scan_path(repo_root: Path, scan_archive: Path) -> tuple[Path, str]:
    root = repo_root.resolve()
    allowed_root = (root / "local_assets" / "scans").resolve()
    archive = scan_archive if scan_archive.is_absolute() else root / scan_archive
    archive = archive.resolve(strict=True)
    try:
        relative_to_allowed = archive.relative_to(allowed_root)
    except ValueError as exc:
        raise BaselineError(
            "raw scan must be stored under local_assets/scans before running P0"
        ) from exc
    if not archive.is_file():
        raise BaselineError(f"scan archive is not a file: {archive.name}")
    repo_relative = (Path("local_assets") / "scans" / relative_to_allowed).as_posix()
    return archive, repo_relative


def tracked_raw_files(repo_root: Path) -> list[str]:
    result: list[str] = []
    for pattern in RAW_PATTERNS:
        output = _git(repo_root, "ls-files", "--", pattern)
        result.extend(line for line in output.splitlines() if line)
    return sorted(set(result))


def sanitize_summary_line(lines: Iterable[str]) -> str | None:
    candidates = [line.strip() for line in lines if line.strip().startswith("BRAMKA:")]
    return candidates[-1] if candidates else None


def assert_report_private(report: dict[str, Any], repo_root: Path) -> None:
    serialized = json.dumps(report, ensure_ascii=False, sort_keys=True)
    forbidden = {
        str(repo_root.resolve()),
        str(Path.home().resolve()),
        os.environ.get("USERNAME", ""),
        os.environ.get("USERPROFILE", ""),
    }
    for value in forbidden:
        if value and value in serialized:
            raise BaselineError("generated report contains a forbidden absolute/user path")


def _first_line(command: Sequence[str], repo_root: Path) -> str:
    result = _run(command, cwd=repo_root, check=False)
    output = (result.stdout or "").splitlines()
    return output[0].strip() if output else "unavailable"


def _find_powershell() -> str:
    for candidate in ("pwsh", "powershell.exe", "powershell"):
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    raise BaselineError("PowerShell executable was not found")


def _file_stamp(path: Path) -> tuple[int, int] | None:
    if not path.is_file():
        return None
    stat = path.stat()
    return stat.st_mtime_ns, stat.st_size


def _copy_if_updated(
    source: Path, destination: Path, *, previous_stamp: tuple[int, int] | None
) -> dict[str, Any] | None:
    current_stamp = _file_stamp(source)
    if current_stamp is None or current_stamp == previous_stamp:
        return None
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return {
        "file": destination.name,
        "sizeBytes": destination.stat().st_size,
        "sha256": sha256_file(destination),
    }


def _write_reports(output_dir: Path, report: dict[str, Any], repo_root: Path) -> None:
    assert_report_private(report, repo_root)
    json_path = output_dir / "p0_baseline.json"
    md_path = output_dir / "p0_baseline.md"
    json_path.write_text(
        json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    gate = report["gate"]
    scan = report["scanArchive"]
    git = report["git"]
    artifacts = report["artifacts"]
    lines = [
        "# Photogrammetry Import V2 — P0 baseline",
        "",
        f"- Status: **{gate['status']}**",
        f"- Branch: `{report['repository']['branch']}`",
        f"- Commit: `{report['repository']['commitSha']}`",
        f"- Base branch: `{report['repository']['expectedBaseBranch']}`",
        f"- Build preset: `{report['build']['preset']}`",
        f"- Scan archive: `{scan['relativePath']}`",
        f"- Scan SHA-256: `{scan['sha256']}`",
        f"- Scan size: `{scan['sizeBytes']}` bytes",
        f"- Gate duration: `{gate['durationSeconds']:.3f}` s",
        f"- Gate summary: `{gate['summaryLine'] or 'missing'}`",
        f"- Clean before: `{git['cleanBefore']}`",
        f"- Clean after: `{git['cleanAfter']}`",
        "",
        "## Local artifacts",
        "",
    ]
    if artifacts:
        for key, value in sorted(artifacts.items()):
            if value is not None:
                lines.append(
                    f"- {key}: `{value['file']}` — {value['sizeBytes']} bytes, SHA-256 `{value['sha256']}`"
                )
    else:
        lines.append("- none")
    lines.extend(
        [
            "",
            "## Privacy",
            "",
            "The report contains repository-relative paths only. Raw scan data, absolute local paths, usernames, and georeferencing metadata are not copied into this report.",
            "",
        ]
    )
    md_path.write_text("\n".join(lines), encoding="utf-8")


def run_baseline(args: argparse.Namespace) -> int:
    if sys.platform != "win32":
        raise BaselineError("P0 project gate must be executed on Windows")

    repo_root = Path(
        _run(("git", "rev-parse", "--show-toplevel"), cwd=Path.cwd(), check=True).stdout.strip()
    ).resolve()
    scan_path, scan_relative = ensure_scan_path(repo_root, args.scan_archive)

    branch = _git(repo_root, "branch", "--show-current")
    if not branch:
        raise BaselineError("detached HEAD is not allowed for P0")
    if branch == "main":
        raise BaselineError("P0 must not run on main")

    base_commit = _run(
        ("git", "rev-parse", "--verify", f"{args.base_commit}^{{commit}}"),
        cwd=repo_root,
        check=False,
    )
    if base_commit.returncode != 0:
        raise BaselineError(
            f"expected base commit is not available locally: {args.base_commit}"
        )
    ancestor = _run(
        ("git", "merge-base", "--is-ancestor", args.base_commit, "HEAD"),
        cwd=repo_root,
        check=False,
    )
    if ancestor.returncode != 0:
        raise BaselineError(
            f"current branch is not derived from {args.base_branch} at {args.base_commit}"
        )

    commit_sha = _git(repo_root, "rev-parse", "HEAD")
    dirty_before = _git(repo_root, "status", "--porcelain", "--untracked-files=all")
    if dirty_before:
        raise BaselineError("working tree must be clean before P0 baseline")

    raw_tracked = tracked_raw_files(repo_root)
    if raw_tracked:
        raise BaselineError("raw scan formats are already tracked: " + ", ".join(raw_tracked))

    ignored = _run(
        ("git", "check-ignore", "-q", "--", scan_relative),
        cwd=repo_root,
        check=False,
        capture=False,
    )
    if ignored.returncode != 0:
        raise BaselineError(f"scan archive is not ignored by Git: {scan_relative}")

    archive_sha = sha256_file(scan_path)
    if args.expected_sha256 and archive_sha.lower() != args.expected_sha256.lower():
        raise BaselineError(
            f"scan SHA-256 mismatch: expected {args.expected_sha256.lower()}, got {archive_sha.lower()}"
        )

    timestamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_root = args.output_root if args.output_root.is_absolute() else repo_root / args.output_root
    output_dir = output_root / f"{commit_sha[:12]}_{timestamp}"
    output_dir.mkdir(parents=True, exist_ok=False)

    gate_log = output_dir / "gate_output.txt"
    gate_script = repo_root / "tools" / "gate.ps1"
    powershell = _find_powershell()
    gate_command = (
        powershell,
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(gate_script),
        "-SaveBaseline",
    )

    validator_source = repo_root / "build" / "gate_baseline.txt"
    quad_source = repo_root / "build" / "gate_baseline_shots" / "quad.png"
    validator_before = _file_stamp(validator_source)
    quad_before = _file_stamp(quad_source)

    started = time.perf_counter()
    gate_result = _run(gate_command, cwd=repo_root, check=False)
    duration = time.perf_counter() - started
    gate_output = gate_result.stdout or ""
    gate_log.write_text(gate_output, encoding="utf-8")

    artifacts: dict[str, Any] = {
        "gateOutput": {
            "file": gate_log.name,
            "sizeBytes": gate_log.stat().st_size,
            "sha256": sha256_file(gate_log),
        },
        "validatorBaseline": _copy_if_updated(
            validator_source,
            output_dir / "validator_baseline.txt",
            previous_stamp=validator_before,
        ),
        "m6Quad": _copy_if_updated(
            quad_source,
            output_dir / "m6_baseline_quad.png",
            previous_stamp=quad_before,
        ),
    }

    dirty_after = _git(repo_root, "status", "--porcelain", "--untracked-files=all")
    report: dict[str, Any] = {
        "format": FORMAT_ID,
        "createdUtc": dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00", "Z"),
        "repository": {
            "branch": branch,
            "commitSha": commit_sha,
            "expectedBaseBranch": args.base_branch,
            "expectedBaseCommit": args.base_commit,
        },
        "build": {
            "preset": "windows-debug",
            "gitVersion": _first_line(("git", "--version"), repo_root),
            "cmakeVersion": _first_line(("cmake", "--version"), repo_root),
            "pythonVersion": sys.version.split()[0],
            "powershellExecutable": Path(powershell).name,
        },
        "scanArchive": {
            "relativePath": scan_relative,
            "sizeBytes": scan_path.stat().st_size,
            "sha256": archive_sha,
        },
        "git": {
            "cleanBefore": not bool(dirty_before),
            "cleanAfter": not bool(dirty_after),
            "trackedRawFiles": raw_tracked,
        },
        "gate": {
            "status": "PASS" if gate_result.returncode == 0 else "FAIL",
            "exitCode": gate_result.returncode,
            "durationSeconds": round(duration, 6),
            "summaryLine": sanitize_summary_line(gate_output.splitlines()),
        },
        "artifacts": artifacts,
        "privacy": {
            "absolutePathsStored": False,
            "georeferencingCopied": False,
            "rawDataCopied": False,
        },
    }

    _write_reports(output_dir, report, repo_root)

    print(f"P0 report: {output_dir.relative_to(repo_root).as_posix()}")
    if gate_result.returncode != 0:
        print("P0: FAIL — P1 remains blocked", file=sys.stderr)
        return 1
    if dirty_after:
        print("P0: FAIL — gate left tracked/untracked repository changes", file=sys.stderr)
        return 1
    print("P0: PASS — baseline captured; P1 may start")
    return 0


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scan-archive", type=Path, required=True)
    parser.add_argument("--expected-sha256")
    parser.add_argument("--base-branch", default=DEFAULT_BASE_BRANCH)
    parser.add_argument("--base-commit", default=DEFAULT_BASE_COMMIT)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    try:
        return run_baseline(parse_args(argv))
    except (BaselineError, OSError) as exc:
        print(f"P0: FAIL — {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
