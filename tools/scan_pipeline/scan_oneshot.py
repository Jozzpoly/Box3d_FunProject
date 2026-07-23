#!/usr/bin/env python3
"""One command: raw scan dataset -> verified private preview -> native window.

This is a thin *orchestrator* over the existing, unchanged pipeline tools:

    scan_dataset_inspect.py        (1) inspect the raw GLB/PLY dataset
    scan_source_frame_contract.py  (2) derive the ENU->lab frame contract
    scan_owner_gate.py             (3) build + independently verify one bundle,
                                       produce the owner-gate receipt
    scan_real_terrain_flow.py      (4) build the preview pack and launch samples

It exists so that importing a scan is a single call instead of a multi-step
ceremony spread across many agent turns. No new importer, no geometry code here.

Privacy stays intact:
  * every artifact lives under the git-ignored build/ tree,
  * nothing is ever uploaded anywhere (PRIVATE_LOCAL_ONLY),
  * the shareable privacy-review file is still built and, without
    --reviewed-privacy, the run STOPS so the owner can inspect it. The
    P1B_BUNDLE_PASS receipt (required to launch) is only written once the owner
    passes --reviewed-privacy. That flag is the human owner's decision.

Typical use (7-tile ENU/metres scan, e.g. Bentley ContextCapture output):

    # first run: build up to the privacy review, then stop and print the file
    python tools/scan_pipeline/scan_oneshot.py --dataset ..\\_private_scan_local

    # after eyeballing the shareable file, launch the native preview in one go
    python tools/scan_pipeline/scan_oneshot.py --dataset ..\\_private_scan_local --reviewed-privacy

Re-running is safe/idempotent: the frame contract is overwritten (--force), the
bundle is content-addressed (same input -> same bundle), the receipt is rewritten.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Sequence

_BUNDLE_LINE = re.compile(r"bundle=(?P<bundle>.*?) receipt=")

MODULE_DIR = Path(__file__).resolve().parent
REPO_ROOT = MODULE_DIR.parent.parent
DEFAULT_PIPELINE_ROOT = REPO_ROOT / "build" / "scan_pipeline"
PYTHON = sys.executable


class OneShotError(RuntimeError):
    pass


def _tool(name: str) -> str:
    return str(MODULE_DIR / name)


def _run(description: str, argv: Sequence[str]) -> str:
    """Run one pipeline tool, echo its output, and return its stdout."""
    print(f"\n[oneshot] === {description} ===", flush=True)
    result = subprocess.run([PYTHON, *argv], capture_output=True, text=True)
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)
    sys.stdout.flush()
    if result.returncode != 0:
        raise OneShotError(f"step failed ({result.returncode}): {description}")
    return result.stdout or ""


def _parse_bundle_path(owner_gate_stdout: str) -> Path:
    """Extract the exact bundle path this run built, from owner-gate output."""
    match = _BUNDLE_LINE.search(owner_gate_stdout)
    if not match:
        raise OneShotError("could not parse bundle path from owner-gate output")
    return Path(match.group("bundle").strip())


def _persist_pack_env(active_preview: Path) -> None:
    """Point a persistent JOZZ_SCAN_PREVIEW_PACK at the freshly built pack so any
    samples.exe (double-clicked, launched from an IDE, any CWD) finds the terrain,
    not only ones started through this pipeline."""
    try:
        document = json.loads(active_preview.read_text(encoding="utf-8"))
        pack = str(Path(document["previewPath"]).resolve())
    except (OSError, ValueError, KeyError) as exc:
        print(f"[oneshot] WARN: could not read active preview to pin env: {exc}", file=sys.stderr)
        return
    os.environ["JOZZ_SCAN_PREVIEW_PACK"] = pack
    if os.name == "nt":
        # setx writes the persistent User-scope variable (new processes inherit it)
        result = subprocess.run(["setx", "JOZZ_SCAN_PREVIEW_PACK", pack],
                                capture_output=True, text=True)
        if result.returncode != 0:
            print(f"[oneshot] WARN: setx failed: {result.stderr.strip()}", file=sys.stderr)
            return
    print(f"[oneshot] pinned JOZZ_SCAN_PREVIEW_PACK -> {pack}")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--dataset",
        required=True,
        type=Path,
        help="directory holding the raw scan (GLB + PLY trees, searched recursively)",
    )
    parser.add_argument("--pipeline-root", type=Path, default=DEFAULT_PIPELINE_ROOT)
    parser.add_argument("--expected-glb", type=int, default=7)
    parser.add_argument("--expected-ply", type=int, default=7)
    parser.add_argument("--dataset-name", default="scan-dataset")

    # Frame contract: defaults describe an ENU / real-metres / Z-up source
    # (East=+X, North=+Y, Up=+Z) mapped onto the lab's default axes. Override
    # only for non-ENU sources.
    parser.add_argument("--source-units-per-meter", type=float, default=1.0)
    parser.add_argument("--source-right", default="+X")
    parser.add_argument("--source-forward", default="+Y")
    parser.add_argument("--source-up", default="+Z")
    parser.add_argument(
        "--local-origin-source",
        nargs=3,
        type=float,
        metavar=("X", "Y", "Z"),
        default=[0.0, 0.0, 0.0],
    )
    parser.add_argument("--mirror-approved", action="store_true")

    # Per-scan leveling: correct a tilted mip-tile export so the ground is
    # horizontal for the drive test. Each scan can differ; default is no tilt.
    parser.add_argument("--level-x-deg", type=float, default=0.0)
    parser.add_argument("--level-z-deg", type=float, default=0.0)

    parser.add_argument(
        "--reviewed-privacy",
        action="store_true",
        help="owner asserts the shareable review file is privacy-clean; "
        "unlocks the P1B_BUNDLE_PASS receipt and the launch",
    )
    parser.add_argument(
        "--no-launch",
        action="store_true",
        help="build the preview pack but do not open the native window",
    )
    args = parser.parse_args(argv)

    dataset = args.dataset.resolve()
    if not dataset.is_dir():
        print(f"[oneshot] ERROR: dataset is not a directory: {dataset}", file=sys.stderr)
        return 2

    pipe = args.pipeline_root.resolve()
    inspection_dir = pipe / "inspection"
    inspection = inspection_dir / "inspection.json"
    contract = pipe / "frame_contract.local.json"
    bundles_root = pipe / "bundles"
    receipt = pipe / "p1b_owner_gate_receipt.local.json"

    try:
        # (1) inspect the raw dataset
        _run(
            "inspect dataset",
            [
                _tool("scan_dataset_inspect.py"),
                "--input", str(dataset),
                "--output", str(inspection_dir),
                "--name", args.dataset_name,
                "--expected-glb", str(args.expected_glb),
                "--expected-ply", str(args.expected_ply),
            ],
        )

        # (2) derive + confirm the ENU->lab frame contract
        frame_argv = [
            _tool("scan_source_frame_contract.py"),
            "create",
            "--source-units-per-meter", str(args.source_units_per_meter),
            "--source-right", args.source_right,
            "--source-forward", args.source_forward,
            "--source-up", args.source_up,
            "--local-origin-source", *[str(v) for v in args.local_origin_source],
            "--output", str(contract),
            "--confirmed",
            "--force",
        ]
        if args.mirror_approved:
            frame_argv.append("--mirror-approved")
        _run("create frame contract (ENU / real metres, confirmed)", frame_argv)

        # (3) owner-gate: build + independently verify the bundle, write receipt
        gate_argv = [
            _tool("scan_owner_gate.py"),
            "finalize",
            "--inspection", str(inspection),
            "--frame-contract", str(contract),
            "--output-root", str(bundles_root),
            "--expected-glb", str(args.expected_glb),
            "--expected-ply", str(args.expected_ply),
        ]
        if args.reviewed_privacy:
            gate_argv.append("--acknowledge-shareable-privacy-review")
        gate_stdout = _run("owner-gate finalize (build + verify bundle)", gate_argv)

        if not args.reviewed_privacy:
            print(
                "\n[oneshot] PRIVACY GATE REACHED\n"
                "[oneshot]   The bundle is built and the shareable review file is\n"
                "[oneshot]   listed above (REVIEW_ONLY_THIS_FILE). Inspect it, then\n"
                "[oneshot]   rerun the SAME command with --reviewed-privacy to launch.",
                flush=True,
            )
            return 0

        # (4) build the exact preview pack and launch the native host.
        # Use the exact bundle THIS run built (parsed from owner-gate output),
        # not "whatever single bundle exists" -- so re-importing further scans
        # onto the same machine never picks up an unrelated bundle.
        bundle = _parse_bundle_path(gate_stdout)
        flow_argv = [
            _tool("scan_real_terrain_flow.py"),
            "--pipeline-root", str(pipe),
            "continue",
            "--bundle", str(bundle),
            "--owner-gate-receipt", str(receipt),
            "--source-root", str(dataset),
            "--level-x-deg", str(args.level_x_deg),
            "--level-z-deg", str(args.level_z_deg),
        ]
        if not args.no_launch:
            flow_argv.append("--launch")
        _run("build preview pack" + ("" if args.no_launch else " + launch native window"), flow_argv)

        if not args.no_launch:
            # Pin the persistent pointer so a double-clicked samples.exe (started
            # outside this pipeline, from any CWD) shows the scan just imported
            # rather than an older one. Best-effort: warns but never aborts here.
            _persist_pack_env(pipe / "ACTIVE_PREVIEW.json")

        print(
            "\n[oneshot] DONE — private preview pack ready"
            + ("." if args.no_launch else "; native window launched.")
            + "\n[oneshot] In Box3D: sample browser -> 'Jozz Vehicle' -> 'P2A Scan Source Preview'.",
            flush=True,
        )
        return 0
    except OneShotError as exc:
        print(f"\n[oneshot] ABORTED: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
