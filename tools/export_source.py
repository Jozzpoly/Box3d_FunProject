#!/usr/bin/env python3
"""Create a deterministic JV source package from the committed HEAD tree.

The package is read from Git objects, not from the working directory. Local
builds, ignored files and uncommitted edits therefore cannot leak into it or be
mislabelled as the exported commit.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PREFIX = "box3d/"


def git_text(*args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(ROOT), *args], text=True, encoding="utf-8"
    ).strip()


def git_bytes(*args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def read_head_tree() -> list[tuple[str, str, str, str]]:
    """Return (path, mode, type, object-id) records for HEAD."""
    raw = git_bytes("ls-tree", "-r", "-z", "--full-tree", "HEAD")
    records: list[tuple[str, str, str, str]] = []
    for record in raw.split(b"\0"):
        if not record:
            continue
        metadata, raw_path = record.split(b"\t", 1)
        mode, object_type, object_id = metadata.decode("ascii").split(" ", 2)
        path = raw_path.decode("utf-8")
        records.append((path, mode, object_type, object_id))
    records.sort(key=lambda item: item[0])
    return records


def zip_info(name: str, timestamp: tuple[int, int, int, int, int, int], mode: int) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, timestamp)
    info.create_system = 3  # Unix permission bits in external_attr
    info.external_attr = (mode & 0xFFFF) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "output",
        nargs="?",
        type=Path,
        help="output zip; default: ../jv-source-<short-sha>.zip",
    )
    args = parser.parse_args()

    try:
        sha = git_text("rev-parse", "HEAD")
        commit_epoch = int(git_text("show", "-s", "--format=%ct", "HEAD"))
        records = read_head_tree()
        dirty = bool(git_text("status", "--porcelain=v1", "--untracked-files=all"))
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"export-source: git failed: {exc}", file=sys.stderr)
        return 2

    unsupported = [path for path, _mode, object_type, _oid in records if object_type != "blob"]
    if unsupported:
        print(
            "export-source: unsupported non-blob entries: " + ", ".join(unsupported),
            file=sys.stderr,
        )
        return 2

    output = (args.output or ROOT.parent / f"jv-source-{sha[:7]}.zip").resolve()
    if output == ROOT or ROOT in output.parents:
        print("export-source: output must be outside the repository", file=sys.stderr)
        return 2
    output.parent.mkdir(parents=True, exist_ok=True)

    commit_time = datetime.fromtimestamp(commit_epoch, timezone.utc)
    # ZIP cannot represent years before 1980 and stores seconds in two-second units.
    zip_year = max(1980, commit_time.year)
    timestamp = (
        zip_year,
        commit_time.month,
        commit_time.day,
        commit_time.hour,
        commit_time.minute,
        commit_time.second - commit_time.second % 2,
    )
    manifest = {
        "project": "Jozz Vehicle",
        "commit": sha,
        "source_commit_utc": commit_time.isoformat(),
        "tracked_files": len(records),
        "rule": "bytes read from committed HEAD Git blobs; no worktree/build/cache/.git/session data",
    }

    try:
        with zipfile.ZipFile(
            output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
        ) as archive:
            for relative, mode_text, _object_type, object_id in records:
                payload = git_bytes("cat-file", "blob", object_id)
                mode = int(mode_text, 8)
                archive.writestr(
                    zip_info(PREFIX + relative, timestamp, mode),
                    payload,
                    compress_type=zipfile.ZIP_DEFLATED,
                    compresslevel=9,
                )
            manifest_bytes = (
                json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
            ).encode("utf-8")
            archive.writestr(
                zip_info(PREFIX + "SOURCE_PACKAGE_MANIFEST.json", timestamp, 0o100644),
                manifest_bytes,
                compress_type=zipfile.ZIP_DEFLATED,
                compresslevel=9,
            )
    except (OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as exc:
        print(f"export-source: write failed: {exc}", file=sys.stderr)
        return 1

    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    print(
        f"export-source: OK — committed HEAD {sha[:12]}, {len(records)} files -> {output}"
    )
    print(f"export-source: SHA-256 {digest}")
    if dirty:
        print(
            "export-source: NOTE — worktree is dirty; uncommitted changes were intentionally excluded"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
