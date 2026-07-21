from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

MODULE_PATH = (
    Path(__file__).parents[2]
    / "tools"
    / "scan_pipeline"
    / "scan_source_frame_contract.py"
)
spec = importlib.util.spec_from_file_location(
    "scan_source_frame_contract_tested",
    MODULE_PATH,
)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


def inspection(
    *,
    minimum: list[float] | None = None,
    maximum: list[float] | None = None,
    passed: bool = True,
    status: str = "compatible-review",
) -> dict[str, object]:
    return {
        "schema": "jozz.scan-dataset-inspection",
        "schemaVersion": 3,
        "datasetStatus": status,
        "automaticEvidenceGate": {"passed": passed},
        "globalBounds": {
            "min": minimum or [1000.0, 2000.0, 3000.0],
            "max": maximum or [1100.0, 2200.0, 3400.0],
        },
    }


def propose(
    report: dict[str, object] | None = None,
) -> dict[str, object]:
    return module.propose_contract_from_inspection(
        inspection=report or inspection(),
        source_units_per_meter=1.0,
        source_axis_roles={
            "right": "+X",
            "forward": "+Y",
            "up": "+Z",
        },
        lab_axis_roles={
            "right": "+X",
            "forward": "-Z",
            "up": "+Y",
        },
    )


def propose_command(
    report_path: Path,
    output_path: Path,
) -> list[str]:
    return [
        sys.executable,
        str(MODULE_PATH),
        "propose-from-inspection",
        "--inspection",
        str(report_path),
        "--output",
        str(output_path),
        "--origin-policy",
        "global-bounds-center",
        "--source-units-per-meter",
        "1",
        "--source-right",
        "+X",
        "--source-forward",
        "+Y",
        "--source-up",
        "+Z",
    ]


def confirm_command(
    proposal_path: Path,
    output_path: Path,
    expected_sha256: str,
) -> list[str]:
    return [
        sys.executable,
        str(MODULE_PATH),
        "confirm",
        "--proposal",
        str(proposal_path),
        "--expected-sha256",
        expected_sha256,
        "--output",
        str(output_path),
    ]


class ScanSourceFrameContractTests(unittest.TestCase):
    def test_build_derives_expected_matrix_without_confirming(self) -> None:
        contract = module.build_contract(
            source_units_per_meter=2.0,
            source_axis_roles={
                "right": "+X",
                "forward": "+Y",
                "up": "+Z",
            },
            lab_axis_roles={
                "right": "+X",
                "forward": "-Z",
                "up": "+Y",
            },
            local_origin_source=[100.0, 200.0, 300.0],
            confirmed=False,
            mirror_approved=False,
        )
        self.assertFalse(contract["confirmed"])
        self.assertEqual(
            contract["sourceToLab"]["axisMatrix"],
            [[1, 0, 0], [0, 0, 1], [0, -1, 0]],
        )
        self.assertEqual(
            contract["sourceToLab"]["orientationChange"],
            "preserve",
        )
        self.assertEqual(contract["sourceToLab"]["determinant"], 1)
        self.assertEqual(contract["sourceFrame"]["handedness"], "right")

    def test_mirror_requires_explicit_approval(self) -> None:
        with self.assertRaises(module.SourceFrameCliError):
            module.build_contract(
                source_units_per_meter=1.0,
                source_axis_roles={
                    "right": "+X",
                    "forward": "+Y",
                    "up": "+Z",
                },
                lab_axis_roles={
                    "right": "+X",
                    "forward": "+Z",
                    "up": "+Y",
                },
                local_origin_source=[0.0, 0.0, 0.0],
                confirmed=False,
                mirror_approved=False,
            )

    def test_duplicate_axis_roles_are_rejected(self) -> None:
        with self.assertRaises(module.SourceFrameCliError):
            module.build_contract(
                source_units_per_meter=1.0,
                source_axis_roles={
                    "right": "+X",
                    "forward": "+Y",
                    "up": "+Y",
                },
                lab_axis_roles={
                    "right": "+X",
                    "forward": "-Z",
                    "up": "+Y",
                },
                local_origin_source=[0.0, 0.0, 0.0],
                confirmed=False,
                mirror_approved=False,
            )

    def test_writer_refuses_silent_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "frame.json"
            module.write_contract(path, {"a": 1}, force=False)
            with self.assertRaises(module.SourceFrameCliError):
                module.write_contract(path, {"a": 2}, force=False)
            module.write_contract(path, {"a": 2}, force=True)
            self.assertIn('"a": 2', path.read_text(encoding="utf-8"))

    def test_proposal_uses_global_bounds_center_and_stays_unconfirmed(self) -> None:
        contract = propose()
        self.assertFalse(contract["confirmed"])
        self.assertEqual(
            contract["sourceToLab"]["localOriginSource"],
            [1050.0, 2100.0, 3200.0],
        )
        self.assertEqual(contract["sourceToLab"]["determinant"], 1)
        self.assertEqual(
            contract["sourceToLab"]["orientationChange"],
            "preserve",
        )
        self.assertFalse(contract["sourceToLab"]["mirrorApproved"])

    def test_reversed_or_nonfinite_bounds_are_rejected(self) -> None:
        with self.assertRaises(module.SourceFrameCliError):
            propose(
                inspection(
                    minimum=[2.0, 0.0, 0.0],
                    maximum=[1.0, 1.0, 1.0],
                )
            )
        with self.assertRaises(module.SourceFrameCliError):
            propose(
                inspection(
                    minimum=[0.0, float("nan"), 0.0],
                    maximum=[1.0, 1.0, 1.0],
                )
            )
        with self.assertRaises(module.SourceFrameCliError):
            propose(
                inspection(
                    minimum=[0.0, 0.0, 0.0],
                    maximum=[1.0, float("inf"), 1.0],
                )
            )

    def test_failed_or_incompatible_inspection_cannot_propose(self) -> None:
        with self.assertRaises(module.SourceFrameCliError):
            propose(inspection(passed=False))
        with self.assertRaises(module.SourceFrameCliError):
            propose(inspection(status="incompatible"))
        with self.assertRaises(module.SourceFrameCliError):
            propose(inspection(status="unknown"))

    def test_proposal_cli_does_not_print_private_origin_or_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report_path = root / "very-private-inspection-name.json"
            output_path = root / "private-proposal-name.json"
            report = inspection(
                minimum=[1234.125, 5678.25, 9012.5],
                maximum=[1236.125, 5682.25, 9018.5],
            )
            report_path.write_text(json.dumps(report), encoding="utf-8")
            result = subprocess.run(
                propose_command(report_path, output_path),
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            combined = result.stdout + result.stderr
            self.assertNotIn(str(report_path), combined)
            self.assertNotIn(str(output_path), combined)
            for private_value in (
                "1234.125",
                "5678.25",
                "9012.5",
                "1235.125",
                "5680.25",
                "9015.5",
            ):
                self.assertNotIn(private_value, combined)
            self.assertIn(
                "origin_policy=GLOBAL_BOUNDS_CENTER",
                result.stdout,
            )
            self.assertIn("proposal_sha256=", result.stdout)
            self.assertIn("units_per_meter=1.0", result.stdout)

    def test_proposal_read_failure_does_not_print_private_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report_path = root / "secret-broken-inspection.json"
            output_path = root / "proposal.json"
            report_path.write_text("{broken", encoding="utf-8")
            result = subprocess.run(
                propose_command(report_path, output_path),
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            self.assertNotIn(str(report_path), result.stderr)
            self.assertNotIn(str(output_path), result.stderr)
            self.assertIn(
                "cannot read private inspection JSON",
                result.stderr,
            )

    def test_proposal_overwrite_failure_does_not_print_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report_path = root / "private-inspection.json"
            output_path = root / "private-proposal.json"
            report_path.write_text(
                json.dumps(inspection()),
                encoding="utf-8",
            )
            output_path.write_text("occupied", encoding="utf-8")
            result = subprocess.run(
                propose_command(report_path, output_path),
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            combined = result.stdout + result.stderr
            self.assertNotIn(str(report_path), combined)
            self.assertNotIn(str(output_path), combined)
            self.assertIn("refusing to overwrite", result.stderr)

    def test_confirmation_requires_exact_hash_and_changes_only_confirmed(self) -> None:
        proposal = propose()
        proposal_before = copy.deepcopy(proposal)
        proposal_hash = module.contract_sha256(proposal)
        confirmed = module.confirm_contract(
            proposal,
            expected_sha256=proposal_hash,
        )
        self.assertFalse(proposal["confirmed"])
        self.assertEqual(proposal, proposal_before)
        self.assertTrue(confirmed["confirmed"])
        comparison = copy.deepcopy(confirmed)
        comparison["confirmed"] = False
        self.assertEqual(comparison, proposal_before)

    def test_confirmation_rejects_noncanonical_proposal_shape(self) -> None:
        extra = propose()
        extra["unexpected"] = "field"
        with self.assertRaises(module.SourceFrameCliError):
            module.confirm_contract(
                extra,
                expected_sha256=module.contract_sha256(extra),
            )

        missing = propose()
        missing.pop("confirmed")
        with self.assertRaises(module.SourceFrameCliError):
            module.confirm_contract(
                missing,
                expected_sha256=module.contract_sha256(missing),
            )

    def test_wrong_or_stale_hash_cannot_confirm(self) -> None:
        proposal = propose()
        with self.assertRaises(module.SourceFrameCliError):
            module.confirm_contract(
                proposal,
                expected_sha256="0" * 64,
            )

        old_hash = module.contract_sha256(proposal)
        proposal["sourceFrame"]["unitsPerMeter"] = 2.0
        with self.assertRaises(module.SourceFrameCliError):
            module.confirm_contract(
                proposal,
                expected_sha256=old_hash,
            )

    def test_confirm_cli_refuses_overwrite_without_printing_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            proposal_path = root / "private-proposal.json"
            output_path = root / "private-confirmed.json"
            proposal_document = propose()
            module.write_contract(
                proposal_path,
                proposal_document,
                force=False,
            )
            proposal_hash = module.contract_sha256(proposal_document)
            command = confirm_command(
                proposal_path,
                output_path,
                proposal_hash,
            )
            first = subprocess.run(
                command,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(first.returncode, 0, first.stderr)
            self.assertNotIn(
                str(proposal_path),
                first.stdout + first.stderr,
            )
            self.assertNotIn(
                str(output_path),
                first.stdout + first.stderr,
            )
            self.assertIn("confirmed_sha256=", first.stdout)

            second = subprocess.run(
                command,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(second.returncode, 2)
            combined = second.stdout + second.stderr
            self.assertNotIn(str(proposal_path), combined)
            self.assertNotIn(str(output_path), combined)
            self.assertIn("refusing to overwrite", second.stderr)

    def test_confirm_read_failure_does_not_print_private_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            proposal_path = root / "secret-broken-proposal.json"
            output_path = root / "private-confirmed.json"
            proposal_path.write_text("{broken", encoding="utf-8")
            result = subprocess.run(
                confirm_command(
                    proposal_path,
                    output_path,
                    "0" * 64,
                ),
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(result.returncode, 2)
            combined = result.stdout + result.stderr
            self.assertNotIn(str(proposal_path), combined)
            self.assertNotIn(str(output_path), combined)
            self.assertIn(
                "cannot read private source-frame proposal JSON",
                result.stderr,
            )

    def test_owner_safe_confirmation_refuses_mirror_even_when_preapproved(self) -> None:
        mirror = module.build_contract(
            source_units_per_meter=1.0,
            source_axis_roles={
                "right": "+X",
                "forward": "+Y",
                "up": "+Z",
            },
            lab_axis_roles={
                "right": "+X",
                "forward": "+Z",
                "up": "+Y",
            },
            local_origin_source=[0.0, 0.0, 0.0],
            confirmed=False,
            mirror_approved=True,
        )
        with self.assertRaises(module.SourceFrameCliError):
            module.confirm_contract(
                mirror,
                expected_sha256=module.contract_sha256(mirror),
            )


if __name__ == "__main__":
    unittest.main()
