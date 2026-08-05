#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from jv_research import core as jv  # noqa: E402
from jv_research import runner  # noqa: E402
from jv_research import cli as lab_cli  # noqa: E402
from adapters import wheel_soft_q2  # noqa: E402


class ExperimentSystemTests(unittest.TestCase):
    def good_spec(self, *, state: str = "ready", script: Path | None = None) -> dict:
        blockers = [] if state == "ready" else ["test blocker"]
        spec = {
            "schema": jv.SPEC_SCHEMA,
            "id": "TEST-EXPERIMENT-00",
            "order": 1,
            "depends_on_experiments": [],
            "title": "Test experiment",
            "state": state,
            "question": "Does the runner preserve one variable?",
            "hypothesis": "The candidate differs only in scale.",
            "primary_factor": "scale",
            "baseline_variant": "A",
            "variants": [
                {"id": "A", "parameters": {"scale": 1.0}},
                {"id": "B", "parameters": {"scale": 0.5}},
            ],
            "locked_factors": {"geometry": "fixed"},
            "confounds": ["changing another factor"],
            "levels": [
                {
                    "id": "Q2",
                    "rig": "test rig",
                    "purpose": "exercise runner",
                    "depends_on": [],
                    "entry_gate": "clean proposal",
                    "exit_gate": "artifacts exist",
                },
                {
                    "id": "Q3",
                    "rig": "transfer rig",
                    "purpose": "enforce promotion",
                    "depends_on": ["Q2"],
                    "entry_gate": "approved Q2",
                    "exit_gate": "transfer artifacts exist",
                }
            ],
            "metrics": ["scale echo"],
            "promotion_rules": ["baseline required"],
            "manual_gates": ["human reviews packet"],
            "blockers": blockers,
        }
        if state == "ready":
            assert script is not None
            spec["execution"] = {
                "command": [
                    "{python}",
                    str(script),
                    "{case_dir}",
                    "{param:scale}",
                ],
                "timeout_seconds": 30,
                "expected_artifacts": ["metrics.json"],
            }
        return spec

    def write_helper(self, root: Path, *, create_artifact: bool = True, exit_code: int = 0) -> Path:
        helper = root / "helper.py"
        helper.write_text(
            "from pathlib import Path\n"
            "import json, sys\n"
            "case_dir = Path(sys.argv[1])\n"
            "scale = float(sys.argv[2])\n"
            + ("(case_dir / 'metrics.json').write_text(json.dumps({'scale': scale}) + '\\n', encoding='utf-8')\n" if create_artifact else "")
            + f"raise SystemExit({exit_code})\n",
            encoding="utf-8",
        )
        return helper

    def snapshot(self, token: str = "head:tree") -> jv.GitSnapshot:
        head, tree = token.split(":")
        return jv.GitSnapshot(head=head, index_tree=tree, proposal_token=token, branch="test")

    def test_repository_specs_are_valid_and_ordered(self) -> None:
        paths = lab_cli.spec_paths()
        self.assertGreaterEqual(len(paths), 2)
        specs = [jv.load_spec(path) for path in paths]
        self.assertEqual([spec["id"] for spec in specs[:2]], ["WHEEL-SOFT-03", "VEHICLE-FLEET-STRESS-04"])
        self.assertLess(specs[0]["order"], specs[1]["order"])

    def test_rejects_more_than_one_primary_parameter(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            helper = self.write_helper(Path(tmp))
            spec = self.good_spec(script=helper)
            spec["variants"][1]["parameters"]["damping"] = 0.7
            errors = jv.validate_spec(spec)
            self.assertTrue(any("może zmieniać tylko primary_factor" in error for error in errors))

    def test_blocked_experiment_cannot_create_run(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            spec = self.good_spec(state="blocked")
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with self.assertRaisesRegex(jv.ExperimentError, "run zablokowany"):
                runner.create_run(path, spec, "Q2", None, root / "runs")

    def test_ready_run_executes_baseline_and_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot()):
                run_dir = runner.create_run(path, spec, "Q2", None, root / "runs")
                rc = runner.execute_cases(run_dir)
            self.assertEqual(rc, 0)
            status = json.loads((run_dir / "status.json").read_text(encoding="utf-8"))
            self.assertEqual(status["state"], "EXECUTED")
            self.assertEqual(set(status["cases"].values()), {"PASSED"})
            for case_dir in (run_dir / "cases").iterdir():
                self.assertTrue((case_dir / "metrics.json").is_file())
                result = json.loads((case_dir / "result.json").read_text(encoding="utf-8"))
                self.assertTrue(result["success"])
                self.assertTrue(result["artifacts"][0]["sha256"])

    def test_resume_rejects_changed_proposal(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot("head:tree")):
                run_dir = runner.create_run(path, spec, "Q2", None, root / "runs")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot("head:other")):
                with self.assertRaisesRegex(jv.ExperimentError, "innej propozycji"):
                    runner.execute_cases(run_dir)

    def test_missing_artifact_is_failure_even_with_zero_exit_code(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root, create_artifact=False)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot()):
                run_dir = runner.create_run(path, spec, "Q2", None, root / "runs")
                rc = runner.execute_cases(run_dir)
            self.assertEqual(rc, 1)
            status = json.loads((run_dir / "status.json").read_text(encoding="utf-8"))
            self.assertEqual(status["state"], "EXECUTED_WITH_FAILURES")

    def test_blocked_cli_is_controlled_failure_without_traceback(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            spec = self.good_spec(state="blocked")
            path = root / "blocked.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(HERE.parent / "jv_lab.py"), "start", str(path), "--level", "Q2"],
                cwd=str(HERE.parents[1]), capture_output=True, text=True, encoding="utf-8", check=False,
            )
        self.assertEqual(result.returncode, 2)
        self.assertIn("run zablokowany", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_wheel_soft_adapter_forwards_one_case_and_validates_metrics(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            case_dir = root / "case"
            case_dir.mkdir()
            fake = root / "jv_wheel_soft_q2"
            fake.write_text(
                "#!/usr/bin/env python3\n"
                "from pathlib import Path\n"
                "import json, sys\n"
                "args = dict(zip(sys.argv[1::2], sys.argv[2::2]))\n"
                "case_dir = Path(args['--case-dir'])\n"
                "(case_dir / 'trace.csv').write_text('header\\n', encoding='utf-8')\n"
                "(case_dir / 'metrics.json').write_text(json.dumps({"
                "'schema':'jv-wheel-soft-q2/v1',"
                "'variant':args['--variant'],"
                "'wheel_contact_hertz_scale':float(args['--hertz-scale']),"
                "'finite':True,"
                "'manifold':{'topology_drift_steps':0},"
                "'rig':{},'softness':{},'equilibrium':{},"
                "'static_window':{'effective_contact_stiffness_n_per_m':1.0},"
                "'response_window':{},'performance':{}"
                "}) + '\\n', encoding='utf-8')\n",
                encoding="utf-8",
            )
            fake.chmod(0o755)
            rc = wheel_soft_q2.main([
                "--binary", str(fake),
                "--case-dir", str(case_dir),
                "--variant", "B_LOCAL_0_50",
                "--hertz-scale", "0.5",
            ])
            self.assertEqual(rc, 0)
            metrics = json.loads((case_dir / "metrics.json").read_text(encoding="utf-8"))
            self.assertEqual(metrics["schema"], "jv-wheel-soft-q2/v1")
            self.assertEqual(metrics["variant"], "B_LOCAL_0_50")
            self.assertEqual(metrics["wheel_contact_hertz_scale"], 0.5)

    def test_wheel_soft_adapter_rejects_semantically_invalid_metrics(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            case_dir = root / "case"
            case_dir.mkdir()
            fake = root / "jv_wheel_soft_q2"
            fake.write_text(
                "#!/usr/bin/env python3\n"
                "from pathlib import Path\n"
                "import json, sys\n"
                "args = dict(zip(sys.argv[1::2], sys.argv[2::2]))\n"
                "case_dir = Path(args['--case-dir'])\n"
                "(case_dir / 'trace.csv').write_text('header\\n', encoding='utf-8')\n"
                "(case_dir / 'metrics.json').write_text("
                "json.dumps({'schema':'jv-wheel-soft-q2/v1','finite':False}) + '\\n', encoding='utf-8')\n",
                encoding="utf-8",
            )
            fake.chmod(0o755)
            rc = wheel_soft_q2.main([
                "--binary", str(fake),
                "--case-dir", str(case_dir),
                "--variant", "B_LOCAL_0_50",
                "--hertz-scale", "0.5",
            ])
            self.assertNotEqual(rc, 0)

    def test_wheel_soft_repository_spec_declares_q2_execution_contract(self) -> None:
        spec = jv.load_spec(HERE / "experiments" / "WHEEL-SOFT-03.json")
        self.assertEqual(spec["state"], "ready")
        self.assertEqual(spec["blockers"], [])
        self.assertEqual(spec["execution"]["expected_artifacts"], ["metrics.json", "trace.csv"])
        command = spec["execution"]["command"]
        self.assertIn("{case_dir}", command)
        self.assertIn("{variant}", command)
        self.assertIn("{param:wheel_contact_hertz_scale}", command)

    def test_higher_level_requires_sealed_promotable_parent(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot()):
                q2 = runner.create_run(path, spec, "Q2", None, root / "runs")
                self.assertEqual(runner.execute_cases(q2), 0)
                runner.seal_run(q2)
                with self.assertRaisesRegex(jv.ExperimentError, "wymaga zatwierdzonych parent runów"):
                    runner.create_run(path, spec, "Q3", None, root / "runs")
                first_decision = runner.record_decision(q2, "PROVISIONAL", "tester", "needs more evidence")
                with self.assertRaisesRegex(jv.ExperimentError, "awans wymaga"):
                    runner.create_run(path, spec, "Q3", None, root / "runs", [q2])
                second_decision = runner.record_decision(q2, "SUPPORTED", "tester", "mechanism repeated")
                self.assertNotEqual(first_decision, second_decision)
                decision = json.loads((q2 / "human_decision.json").read_text(encoding="utf-8"))
                self.assertIsNotNone(decision["supersedes_sha256"])
                self.assertEqual(len(list((q2 / "decisions").glob("*.json"))), 2)
                with self.assertRaisesRegex(jv.ExperimentError, "nie wolno ponownie sealować"):
                    runner.seal_run(q2)
                q3 = runner.create_run(path, spec, "Q3", None, root / "runs", [q2])
            lock = json.loads((q3 / "run.lock.json").read_text(encoding="utf-8"))
            self.assertEqual(lock["parent_runs"][0]["level"], "Q2")
            self.assertEqual(lock["parent_runs"][0]["decision_status"], "SUPPORTED")

    def test_seal_rejects_mutated_artifact(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot()):
                run_dir = runner.create_run(path, spec, "Q2", None, root / "runs")
                self.assertEqual(runner.execute_cases(run_dir), 0)
            artifact = next((run_dir / "cases").glob("*/metrics.json"))
            artifact.write_text('{"scale": 999}\n', encoding="utf-8")
            with self.assertRaisesRegex(jv.ExperimentError, "artefakt zmieniony"):
                runner.seal_run(run_dir)

    def test_seal_never_claims_physical_verdict(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot()):
                run_dir = runner.create_run(path, spec, "Q2", None, root / "runs")
                self.assertEqual(runner.execute_cases(run_dir), 0)
            packet_path = runner.seal_run(run_dir)
            packet = json.loads(packet_path.read_text(encoding="utf-8"))
            self.assertIsNone(packet["automatic_verdict"])
            self.assertEqual(packet["decision_state"], "READY_FOR_ANALYSIS")
            self.assertIn("Brak automatycznego werdyktu", packet["note"])


    def test_publish_copies_curated_sealed_run_and_refuses_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot()):
                run_dir = runner.create_run(path, spec, "Q2", None, root / "runs")
                self.assertEqual(runner.execute_cases(run_dir), 0)
            runner.seal_run(run_dir)
            decision_history = runner.record_decision(run_dir, "INCONCLUSIVE", "tester", "needs road input")

            published = runner.publish_run(run_dir, root / "published")
            manifest = json.loads((published / "PUBLISH_MANIFEST.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["schema"], "jv-published-run/v1")
            self.assertEqual(manifest["decision_status"], "INCONCLUSIVE")
            self.assertEqual(manifest["spec_snapshot"], spec)
            self.assertEqual(manifest["current_decision_path"], f"decisions/{decision_history.name}")
            self.assertTrue((published / manifest["current_decision_path"]).is_file())
            self.assertFalse((published / "spec.snapshot.json").exists())
            self.assertFalse((published / "human_decision.json").exists())
            self.assertTrue(list((published / "cases").glob("*/metrics.json")))
            self.assertFalse(list((published / "cases").glob("*/stdout.log")))
            self.assertFalse(list((published / "cases").glob("*/stderr.log")))
            self.assertEqual(len(manifest["omitted_empty_files"]), 4)
            with self.assertRaisesRegex(jv.ExperimentError, "nie zostanie nadpisana"):
                runner.publish_run(run_dir, root / "published")

    def test_publish_rejects_artifact_mutated_after_seal(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot()):
                run_dir = runner.create_run(path, spec, "Q2", None, root / "runs")
                self.assertEqual(runner.execute_cases(run_dir), 0)
            runner.seal_run(run_dir)
            runner.record_decision(run_dir, "INCONCLUSIVE", "tester", "needs more evidence")
            artifact = next((run_dir / "cases").glob("*/metrics.json"))
            artifact.write_text('{"scale": 999}\n', encoding="utf-8")
            with self.assertRaisesRegex(jv.ExperimentError, "artefakt zmieniony"):
                runner.publish_run(run_dir, root / "published")

    def test_publish_requires_explicit_decision(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            helper = self.write_helper(root)
            spec = self.good_spec(script=helper)
            path = root / "spec.json"
            path.write_text(json.dumps(spec), encoding="utf-8")
            with mock.patch.object(runner, "proposal_complete", return_value=(True, "ok")), \
                 mock.patch.object(runner, "git_snapshot", return_value=self.snapshot()):
                run_dir = runner.create_run(path, spec, "Q2", None, root / "runs")
                self.assertEqual(runner.execute_cases(run_dir), 0)
            runner.seal_run(run_dir)
            with self.assertRaisesRegex(jv.ExperimentError, "jawnej decyzji"):
                runner.publish_run(run_dir, root / "published")



if __name__ == "__main__":
    unittest.main(verbosity=2)
