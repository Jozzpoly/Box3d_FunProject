#!/usr/bin/env python3
"""Run the owner-local P1B bundle gate with fail-fast checks.

The command can discover repeated real inspection reports, but it auto-selects
one only when all passing candidates are byte-identical. Finalization requires
an explicitly owner-confirmed source-frame contract, builds the immutable
bundle, invokes the independent verifier as a separate process and writes a
private local receipt bound to that exact bundle revision. Occupancy/P2 remain
blocked until the owner also acknowledges manual review of the shareable JSON.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Sequence
import uuid

MODULE_DIR = Path(__file__).resolve().parent
INSPECTION_SCHEMA = "jozz.scan-dataset-inspection"
MIN_INSPECTION_SCHEMA_VERSION = 3
RECEIPT_SCHEMA = "jozz.scan-p1b-owner-gate-receipt"
RECEIPT_SCHEMA_VERSION = 2
_SAFE_KEY = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")
_SHA256 = re.compile(r"^[0-9a-f]{64}$")
_REVISION = re.compile(r"^sha256:[0-9a-f]{64}$")
_BUNDLE_PATH = re.compile(r"\bpath=(.*?)\s+bundle_sha256=")


class OwnerGateError(ValueError):
    pass


def _strict_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise OwnerGateError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _validate_json(value: Any, path: str = "root") -> None:
    if value is None or isinstance(value, (str, bool, int)):
        return
    if isinstance(value, float):
        if not math.isfinite(value):
            raise OwnerGateError(f"{path} contains a non-finite float")
        return
    if isinstance(value, list):
        for index, child in enumerate(value):
            _validate_json(child, f"{path}[{index}]")
        return
    if isinstance(value, dict):
        for key, child in value.items():
            if not isinstance(key, str):
                raise OwnerGateError(f"{path} contains a non-string key")
            _validate_json(child, f"{path}.{key}")
        return
    raise OwnerGateError(f"{path} contains non-JSON value {type(value).__name__}")


def load_json_strict(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(
            Path(path).read_text(encoding="utf-8"),
            object_pairs_hook=_strict_object,
            parse_constant=lambda token: (_ for _ in ()).throw(
                OwnerGateError(f"non-standard JSON constant: {token}")
            ),
        )
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise OwnerGateError(f"cannot read JSON from {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise OwnerGateError(f"{path} must contain a JSON object")
    _validate_json(value)
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _nonnegative_int(value: Any, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise OwnerGateError(f"{field} must be a non-negative integer")
    return value


def _sha256(value: Any, field: str) -> str:
    if not isinstance(value, str) or not _SHA256.fullmatch(value):
        raise OwnerGateError(f"{field} must be lowercase SHA-256")
    return value


def _revision(value: Any) -> str:
    if not isinstance(value, str) or not _REVISION.fullmatch(value):
        raise OwnerGateError("sourceRevisionId must be sha256:<hex>")
    return value


def inspection_candidate(path: Path) -> dict[str, Any]:
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise OwnerGateError(f"inspection must be a real file: {path}")
    report = load_json_strict(path)
    if report.get("schema") != INSPECTION_SCHEMA:
        raise OwnerGateError(f"unexpected inspection schema in {path}")
    version = _nonnegative_int(report.get("schemaVersion"), "schemaVersion")
    if version < MIN_INSPECTION_SCHEMA_VERSION:
        raise OwnerGateError(f"inspection schema is too old in {path}")
    status = report.get("datasetStatus")
    if status not in {"compatible", "compatible-review", "incompatible"}:
        raise OwnerGateError(f"invalid datasetStatus in {path}")
    automatic = report.get("automaticEvidenceGate")
    if not isinstance(automatic, dict) or not isinstance(
        automatic.get("passed"), bool
    ):
        raise OwnerGateError(
            f"automaticEvidenceGate.passed must be boolean in {path}"
        )
    totals = report.get("totals")
    pairs = report.get("pairs")
    if not isinstance(totals, dict) or not isinstance(pairs, list):
        raise OwnerGateError(f"inspection totals/pairs are malformed in {path}")
    return {
        "path": path,
        "sha256": sha256_file(path),
        "schemaVersion": version,
        "datasetStatus": status,
        "automaticEvidenceGatePassed": automatic["passed"],
        "glbFiles": _nonnegative_int(
            totals.get("glbFiles"), "totals.glbFiles"
        ),
        "plyFiles": _nonnegative_int(
            totals.get("plyFiles"), "totals.plyFiles"
        ),
        "pairCount": len(pairs),
    }


def discover_candidates(root: Path) -> list[dict[str, Any]]:
    root = Path(root)
    if not root.is_dir() or root.is_symlink():
        raise OwnerGateError(f"inspection root must be a real directory: {root}")
    paths = sorted(
        (
            path
            for path in root.rglob("inspection.json")
            if path.is_file() and not path.is_symlink()
        ),
        key=lambda path: path.as_posix().lower(),
    )
    if not paths:
        raise OwnerGateError(f"no inspection.json files found under {root}")
    candidates: list[dict[str, Any]] = []
    errors: list[str] = []
    for path in paths:
        try:
            candidates.append(inspection_candidate(path))
        except OwnerGateError as exc:
            errors.append(str(exc))
    if errors:
        raise OwnerGateError(f"invalid inspection candidate: {errors[0]}")
    if not candidates:
        raise OwnerGateError("no valid inspection candidates")
    return candidates


def select_identical_passing_candidate(
    root: Path,
    *,
    expected_glb: int,
    expected_ply: int,
) -> tuple[dict[str, Any], int]:
    candidates = discover_candidates(root)
    passing = [
        candidate
        for candidate in candidates
        if candidate["automaticEvidenceGatePassed"]
        and candidate["datasetStatus"] != "incompatible"
        and candidate["glbFiles"] == expected_glb
        and candidate["plyFiles"] == expected_ply
        and candidate["pairCount"] == expected_glb == expected_ply
    ]
    if not passing:
        raise OwnerGateError(
            f"no passing {expected_glb}+{expected_ply} inspection with complete pair coverage"
        )
    hashes = {candidate["sha256"] for candidate in passing}
    if len(hashes) != 1:
        raise OwnerGateError(
            "multiple passing inspection reports differ byte-for-byte; use --inspection after review"
        )
    passing.sort(key=lambda candidate: candidate["path"].as_posix().lower())
    return passing[0], len(passing)


def _run(command: list[str], label: str) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise OwnerGateError(
            f"{label} failed with exit code {result.returncode}: {detail}"
        )
    return result


def parse_bundle_path(stdout: str) -> Path:
    match = _BUNDLE_PATH.search(stdout)
    if not match:
        raise OwnerGateError(
            "bundle command did not report a parseable output path"
        )
    return Path(match.group(1))


def read_verified_bundle_binding(bundle: Path) -> dict[str, str]:
    complete = Path(bundle) / "COMPLETE.json"
    if not complete.is_file() or complete.is_symlink():
        raise OwnerGateError("verified bundle COMPLETE.json is missing or linked")
    manifest = load_json_strict(complete)
    if (
        manifest.get("schema") != "jozz.scan-import-bundle"
        or manifest.get("status") != "COMPLETE"
    ):
        raise OwnerGateError("verified bundle manifest boundary is invalid")
    return {
        "bundleContentSha256": _sha256(
            manifest.get("bundleContentSha256"), "bundleContentSha256"
        ),
        "sourceRevisionId": _revision(manifest.get("sourceRevisionId")),
    }


def build_receipt(
    *,
    candidate: dict[str, Any],
    identical_copy_count: int,
    privacy_review_acknowledged: bool,
    bundle_content_sha256: str,
    source_revision_id: str,
) -> dict[str, Any]:
    return {
        "schema": RECEIPT_SCHEMA,
        "schemaVersion": RECEIPT_SCHEMA_VERSION,
        "status": (
            "P1B_BUNDLE_PASS"
            if privacy_review_acknowledged
            else "P1B_TECHNICAL_PASS_PRIVACY_REVIEW_REQUIRED"
        ),
        "privacyClass": "PRIVATE_LOCAL_ONLY",
        "inspection": {
            "schemaVersion": candidate["schemaVersion"],
            "datasetStatus": candidate["datasetStatus"],
            "automaticEvidenceGatePassed": candidate[
                "automaticEvidenceGatePassed"
            ],
            "glbFiles": candidate["glbFiles"],
            "plyFiles": candidate["plyFiles"],
            "pairCount": candidate["pairCount"],
            "byteIdenticalCopyCount": identical_copy_count,
        },
        "sourceFrameConfirmed": True,
        "bundle": {
            "internalVerificationPassed": True,
            "independentVerificationPassed": True,
            "bundleContentSha256": _sha256(
                bundle_content_sha256, "bundleContentSha256"
            ),
            "sourceRevisionId": _revision(source_revision_id),
        },
        "privacyReview": {
            "status": (
                "ACKNOWLEDGED" if privacy_review_acknowledged else "PENDING"
            ),
            "reviewTargetRelativePath": "shareable/inspection.shareable.json",
        },
        "privacy": {
            "sourceCoordinatesIncluded": False,
            "sourceBoundsIncluded": False,
            "sourceNamesIncluded": False,
            "sourcePathsIncluded": False,
            "sourceFileHashesIncluded": False,
            "bundleFingerprintIncluded": True,
        },
    }


def write_json_atomic(path: Path, value: dict[str, Any]) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = (
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    ).encode("utf-8")
    temporary = path.with_name(
        f".{path.name}.tmp-{os.getpid()}-{uuid.uuid4().hex}"
    )
    try:
        with temporary.open("xb") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def _select(args: argparse.Namespace) -> tuple[dict[str, Any], int]:
    if args.inspection is not None:
        candidate = inspection_candidate(args.inspection)
        if not candidate["automaticEvidenceGatePassed"]:
            raise OwnerGateError(
                "explicit inspection did not pass the automatic evidence gate"
            )
        if candidate["datasetStatus"] == "incompatible":
            raise OwnerGateError("explicit inspection is spatially incompatible")
        if (
            candidate["glbFiles"] != args.expected_glb
            or candidate["plyFiles"] != args.expected_ply
        ):
            raise OwnerGateError(
                "explicit inspection source counts do not match expectations"
            )
        if (
            candidate["pairCount"] != args.expected_glb
            or candidate["pairCount"] != args.expected_ply
        ):
            raise OwnerGateError(
                "explicit inspection pair coverage is incomplete"
            )
        return candidate, 1
    return select_identical_passing_candidate(
        args.inspection_root,
        expected_glb=args.expected_glb,
        expected_ply=args.expected_ply,
    )


def _inspect(args: argparse.Namespace) -> int:
    candidate, copies = _select(args)
    print(
        "scan_owner_gate: INSPECTION_READY | "
        f"path={candidate['path']} copies={copies} "
        f"glb={candidate['glbFiles']} ply={candidate['plyFiles']} "
        f"pairs={candidate['pairCount']} status={candidate['datasetStatus']}"
    )
    return 0


def _finalize(args: argparse.Namespace) -> int:
    candidate, copies = _select(args)
    key = args.package_key
    if not _SAFE_KEY.fullmatch(key):
        raise OwnerGateError(f"package key must match {_SAFE_KEY.pattern}")

    _run(
        [
            sys.executable,
            str(MODULE_DIR / "scan_source_frame_contract.py"),
            "validate",
            str(args.frame_contract),
            "--require-confirmed",
        ],
        "source-frame validation",
    )

    output_root = Path(args.output_root)
    bundle_result = _run(
        [
            sys.executable,
            str(MODULE_DIR / "scan_import_bundle.py"),
            "--inspection",
            str(candidate["path"]),
            "--frame-contract",
            str(args.frame_contract),
            "--output-root",
            str(output_root),
            "--package-id",
            f"scan/{key}",
            "--proposal-id",
            f"proposal/{key}/revision-1",
            "--bundle-label",
            key,
            "--require-inspection-pass",
            "--require-frame-confirmed",
        ],
        "bundle publication",
    )
    bundle = parse_bundle_path(bundle_result.stdout)
    _run(
        [
            sys.executable,
            str(MODULE_DIR / "scan_import_bundle_verify.py"),
            str(bundle),
        ],
        "independent bundle verification",
    )
    binding = read_verified_bundle_binding(bundle)

    shareable = bundle / "shareable" / "inspection.shareable.json"
    if not shareable.is_file() or shareable.is_symlink():
        raise OwnerGateError(
            "verified bundle does not contain a real shareable review file"
        )

    receipt = build_receipt(
        candidate=candidate,
        identical_copy_count=copies,
        privacy_review_acknowledged=args.acknowledge_shareable_privacy_review,
        bundle_content_sha256=binding["bundleContentSha256"],
        source_revision_id=binding["sourceRevisionId"],
    )
    receipt_path = (
        args.receipt
        or output_root.parent / "p1b_owner_gate_receipt.local.json"
    )
    write_json_atomic(receipt_path, receipt)

    if args.acknowledge_shareable_privacy_review:
        print(
            "scan_owner_gate: P1B_BUNDLE_PASS | "
            f"bundle={bundle} receipt={receipt_path}"
        )
    else:
        print(
            "scan_owner_gate: TECHNICAL_PASS / PRIVACY_REVIEW_REQUIRED | "
            f"bundle={bundle} receipt={receipt_path}"
        )
        print(f"scan_owner_gate: REVIEW_ONLY_THIS_FILE | {shareable}")
        print(
            "scan_owner_gate: NEXT | after manual review rerun the same command "
            "with --acknowledge-shareable-privacy-review"
        )
    return 0


def _add_inspection_selection(parser: argparse.ArgumentParser) -> None:
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--inspection", type=Path)
    selection.add_argument("--inspection-root", type=Path)
    parser.add_argument("--expected-glb", type=int, default=7)
    parser.add_argument("--expected-ply", type=int, default=7)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    inspect = subparsers.add_parser(
        "inspect", help="discover and compare real inspection reports"
    )
    _add_inspection_selection(inspect)
    inspect.set_defaults(handler=_inspect)

    finalize = subparsers.add_parser(
        "finalize",
        help="build, independently verify and receipt one real bundle",
    )
    _add_inspection_selection(finalize)
    finalize.add_argument("--frame-contract", required=True, type=Path)
    finalize.add_argument(
        "--output-root",
        type=Path,
        default=Path("build/scan_pipeline/bundles"),
    )
    finalize.add_argument("--receipt", type=Path)
    finalize.add_argument("--package-key", default="photogrammetry-primary")
    finalize.add_argument(
        "--acknowledge-shareable-privacy-review", action="store_true"
    )
    finalize.set_defaults(handler=_finalize)

    args = parser.parse_args(argv)
    try:
        return int(args.handler(args))
    except (OSError, OwnerGateError) as exc:
        print(f"scan_owner_gate: ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
