#!/usr/bin/env python3
"""Testy regresyjne lancucha dowodowego (JP-01, T1-T12).

Uruchomienie z roota repo:
    python -m unittest discover -s tools/evidence -v

Zasady:
- kazdy test pracuje na WLASNEJ kopii w katalogu tymczasowym; produkcyjne
  KOLA_* i surowe logi nie sa dotykane;
- kody wyjscia sprawdzane przez bezposrednie `subprocess.run`, nigdy przez
  pipeline (w poprzedniej rundzie `| tail` zamaskowal kod wyjscia i przez
  chwile wygladalo, ze `extract` konczy sie sukcesem mimo bledu);
- kazdy test na sciezce bledu weryfikuje BAJTOWO, ze pliki docelowe sie nie
  zmienily - sam kod wyjscia nie dowodzi braku czesciowego zapisu.
"""

from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL_SRC = Path(__file__).resolve().parent / "evidence.py"
REPO = Path(__file__).resolve().parents[2]
REAL_EVIDENCE = REPO / "tools" / "jozz_wheel_bench" / "evidence"

DOC_TEMPLATE = """# Fixture

Tekst przed blokiem.

<!-- EVIDENCE:BEGIN run=2026_07_27_v2 id=D.box -->
<!-- EVIDENCE:END -->

Tekst po bloku. Finding F-08 jest tu wspomniany.
"""

SECOND_DOC_OK = """# Drugi fixture

<!-- EVIDENCE:BEGIN run=2026_07_27_v2 id=D.mesh -->
<!-- EVIDENCE:END -->
"""

SECOND_DOC_BAD = """# Drugi fixture

<!-- EVIDENCE:BEGIN run=2026_07_27_v2 id=NIE.ISTNIEJE -->
<!-- EVIDENCE:END -->
"""


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class Sandbox:
    """Izolowana kopia repo: narzedzie + surowe logi + fixtures dokumentow."""

    def __init__(self) -> None:
        self.root = Path(tempfile.mkdtemp(prefix="jp01_"))
        (self.root / "tools" / "evidence").mkdir(parents=True)
        self.evidence = self.root / "tools" / "jozz_wheel_bench" / "evidence"
        self.evidence.mkdir(parents=True)
        (self.root / "docs").mkdir()
        shutil.copy2(TOOL_SRC, self.root / "tools" / "evidence" / "evidence.py")
        for log in REAL_EVIDENCE.glob("run_*.txt"):
            shutil.copy2(log, self.evidence / log.name)
        self.tool = self.root / "tools" / "evidence" / "evidence.py"

    def doc(self, name: str, text: str = DOC_TEMPLATE) -> Path:
        p = self.root / "docs" / name
        p.write_text(text, encoding="utf-8")
        return p

    def run(self, cmd: str) -> subprocess.CompletedProcess:
        # bezposrednie wywolanie procesu - zaden pipeline nie maskuje kodu wyjscia
        return subprocess.run([sys.executable, str(self.tool), cmd],
                              capture_output=True, text=True, cwd=str(self.root))

    @property
    def summary(self) -> Path:
        return self.evidence / "summary.json"

    @property
    def raw_v2(self) -> Path:
        return self.evidence / "run_2026_07_27_v2.txt"

    def bootstrap(self) -> None:
        assert self.run("register").returncode == 0
        assert self.run("extract").returncode == 0

    def cleanup(self) -> None:
        shutil.rmtree(self.root, ignore_errors=True)


class EvidenceChainTest(unittest.TestCase):
    def setUp(self) -> None:
        self.sb = Sandbox()
        self.addCleanup(self.sb.cleanup)

    # -- T1 ----------------------------------------------------------------
    def test_T1_baseline_green(self):
        self.sb.doc("KOLA_01_DOWODY_PL.md")
        self.assertEqual(self.sb.run("register").returncode, 0)
        self.assertEqual(self.sb.run("extract").returncode, 0)
        self.assertEqual(self.sb.run("render").returncode, 0)
        res = self.sb.run("check")
        self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
        self.assertIn("CHECK OK", res.stdout)
        rendered = (self.sb.root / "docs" / "KOLA_01_DOWODY_PL.md").read_text(encoding="utf-8")
        self.assertIn("| sphere | 0.0002 |", rendered)
        self.assertIn("| 1.13 |", rendered)

    # -- T2 ----------------------------------------------------------------
    def test_T2_tampered_summary_detected(self):
        """Regresja glownej dziury v1: podmiana summary przy nietknietym raw."""
        doc = self.sb.doc("KOLA_01_DOWODY_PL.md")
        self.sb.bootstrap()
        self.sb.run("render")
        raw_before, doc_before = digest(self.sb.raw_v2), digest(doc)

        data = json.loads(self.sb.summary.read_text(encoding="utf-8"))
        data["runs"]["2026_07_27_v2"]["tables"]["D.box"]["rows"][0]["values"][-2] = "9.99"
        self.sb.summary.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                                   encoding="utf-8")

        res = self.sb.run("check")
        self.assertNotEqual(res.returncode, 0, "podmienione summary przeszlo check")
        self.assertIn("ROZNI SIE od surowego logu", res.stdout)
        self.assertEqual(digest(self.sb.raw_v2), raw_before)
        self.assertEqual(digest(doc), doc_before, "check zmodyfikowal dokument")

    # -- T3 ----------------------------------------------------------------
    def test_T3_broken_marker_leaves_summary_untouched(self):
        self.sb.bootstrap()
        before = digest(self.sb.summary)
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig")
        self.sb.raw_v2.write_text(
            txt.replace("=== D) CPU COST - MARGINAL, MESH ground ===", "=== ZEPSUTY ==="),
            encoding="utf-8")
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1)
        self.assertIn("oczekiwano dokladnie 1", res.stdout)
        self.assertEqual(digest(self.sb.summary), before, "summary zmienione mimo bledu")

    # -- T4 ----------------------------------------------------------------
    def test_T4_missing_variant_fails_loudly(self):
        self.sb.bootstrap()
        before = digest(self.sb.summary)
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig")
        lines = [ln for ln in txt.splitlines()
                 if not (ln.startswith("cylinder-32") and "1.67" in ln)]
        self.sb.raw_v2.write_text("\n".join(lines), encoding="utf-8")
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1)
        self.assertIn("!= oczekiwane", res.stdout)
        self.assertEqual(digest(self.sb.summary), before)

    def test_T4b_duplicate_variant_fails_loudly(self):
        self.sb.bootstrap()
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig").splitlines()
        idx = next(i for i, ln in enumerate(txt) if ln.startswith("sphere") and "1.13" in ln)
        txt.insert(idx + 1, txt[idx])
        self.sb.raw_v2.write_text("\n".join(txt), encoding="utf-8")
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1)
        self.assertIn("!= oczekiwane", res.stdout)

    # -- T5 ----------------------------------------------------------------
    def test_T5_wrong_column_count_fails_loudly(self):
        """Dodatkowa liczba nie moze po cichu przesunac mapowania kolumn."""
        self.sb.bootstrap()
        before = digest(self.sb.summary)
        lines = self.sb.raw_v2.read_text(encoding="utf-8-sig").splitlines()
        idx = next(i for i, ln in enumerate(lines) if ln.startswith("sphere") and "1.13" in ln)
        lines[idx] = lines[idx].replace("(1.00x sphere)", "") + "   0.0001"
        self.sb.raw_v2.write_text("\n".join(lines), encoding="utf-8")
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1, res.stdout)
        self.assertIn("!= oczekiwane", res.stdout)
        self.assertEqual(digest(self.sb.summary), before)

    # -- T6 ----------------------------------------------------------------
    def test_T6_nan_rejected(self):
        for token in ("NaN", "Inf", "-1.#IND"):
            with self.subTest(token=token):
                sb = Sandbox()
                self.addCleanup(sb.cleanup)
                sb.bootstrap()
                before = digest(sb.summary)
                lines = sb.raw_v2.read_text(encoding="utf-8-sig").splitlines()
                idx = next(i for i, ln in enumerate(lines)
                           if ln.startswith("sphere") and "1.13" in ln)
                lines[idx] = lines[idx].replace("1.13", token)
                sb.raw_v2.write_text("\n".join(lines), encoding="utf-8")
                res = sb.run("extract")
                self.assertEqual(res.returncode, 1, f"{token} zaakceptowany: {res.stdout}")
                self.assertIn("!= oczekiwane", res.stdout)
                self.assertNotIn(token, res.stdout.split("odrzucone")[0])
                self.assertEqual(digest(sb.summary), before)

    # -- T7 ----------------------------------------------------------------
    def test_T7_tampered_document_detected(self):
        doc = self.sb.doc("KOLA_01_DOWODY_PL.md")
        self.sb.bootstrap()
        self.sb.run("render")
        doc.write_text(doc.read_text(encoding="utf-8").replace("| 1.13 |", "| 1.14 |"),
                       encoding="utf-8")
        res = self.sb.run("check")
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("rozjechal sie z surowym logiem", res.stdout)

    # -- T8 ----------------------------------------------------------------
    def test_T8_render_is_transactional(self):
        """Blad w drugim dokumencie nie moze zostawic pierwszego zmienionego."""
        first = self.sb.doc("KOLA_01_DOWODY_PL.md")
        second = self.sb.doc("KOLA_09_BAD_PL.md", SECOND_DOC_BAD)
        self.sb.bootstrap()
        before_first, before_second = digest(first), digest(second)
        res = self.sb.run("render")
        self.assertEqual(res.returncode, 1, res.stdout)
        self.assertIn("zaden dokument NIE zostal zmieniony", res.stdout)
        self.assertEqual(digest(first), before_first, "pierwszy dokument zmieniony mimo bledu")
        self.assertEqual(digest(second), before_second)

    def test_T8b_render_multi_document_success(self):
        first = self.sb.doc("KOLA_01_DOWODY_PL.md")
        second = self.sb.doc("KOLA_09_OK_PL.md", SECOND_DOC_OK)
        self.sb.bootstrap()
        self.assertEqual(self.sb.run("render").returncode, 0)
        self.assertIn("1.13", first.read_text(encoding="utf-8"))
        self.assertIn("2.12", second.read_text(encoding="utf-8"))

    # -- T9 ----------------------------------------------------------------
    def test_T9_v1_stays_raw_only(self):
        self.sb.bootstrap()
        summary = json.loads(self.sb.summary.read_text(encoding="utf-8"))
        v1 = summary["runs"]["2026_07_25_baseline"]
        self.assertEqual(v1["tables"], {}, "v1 zostal sparsowany schematem v2")
        self.assertTrue(v1["source_sha256"])
        manifest = json.loads((self.sb.evidence / "RAW_MANIFEST.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["runs"]["2026_07_25_baseline"]["mode"], "raw-only")
        self.assertEqual(self.sb.run("check").returncode, 0, "raw-only potraktowane jako blad")

    # -- T10 ---------------------------------------------------------------
    def test_T10_modified_raw_under_existing_run_id(self):
        """Historyczny artefakt zrodlowy nie moze sie po cichu zmienic."""
        self.sb.bootstrap()
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig")
        self.sb.raw_v2.write_text(txt.replace("1.13", "1.15"), encoding="utf-8")
        res = self.sb.run("check")
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("ZMIENIONY od rejestracji", res.stdout)
        # ponowny extract nie moze "wybielic" zmiany
        self.sb.run("extract")
        res2 = self.sb.run("check")
        self.assertNotEqual(res2.returncode, 0, "ponowny extract ukryl zmiane raw")
        self.assertIn("ZMIENIONY od rejestracji", res2.stdout)

    def test_T10b_unregistered_raw_detected(self):
        self.sb.bootstrap()
        shutil.copy2(self.sb.raw_v2, self.sb.evidence / "run_2026_07_30_fake.txt")
        res = self.sb.run("check")
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("nie jest zarejestrowany", res.stdout)

    # -- T11 ---------------------------------------------------------------
    def test_T11_deterministic(self):
        doc = self.sb.doc("KOLA_01_DOWODY_PL.md")
        self.sb.bootstrap()
        self.sb.run("render")
        first_summary, first_doc = digest(self.sb.summary), digest(doc)
        self.sb.run("extract")
        self.sb.run("render")
        self.assertEqual(digest(self.sb.summary), first_summary, "extract niedeterministyczny")
        self.assertEqual(digest(doc), first_doc, "render niedeterministyczny")

    def test_T11b_line_endings_preserved(self):
        """Korpus ma MIESZANE zakonczenia; render nie moze przepisac calego pliku."""
        crlf = self.sb.root / "docs" / "KOLA_01_DOWODY_PL.md"
        crlf.write_bytes(DOC_TEMPLATE.replace("\n", "\r\n").encode("utf-8"))
        lf = self.sb.root / "docs" / "KOLA_09_LF_PL.md"
        lf.write_bytes(SECOND_DOC_OK.encode("utf-8"))
        self.sb.bootstrap()
        self.assertEqual(self.sb.run("render").returncode, 0)
        crlf_bytes, lf_bytes = crlf.read_bytes(), lf.read_bytes()
        self.assertEqual(crlf_bytes.count(b"\n"), crlf_bytes.count(b"\r\n"),
                         "plik CRLF stracil zakonczenia linii")
        self.assertNotIn(b"\r\n", lf_bytes, "plik LF dostal CRLF")
        self.assertEqual(self.sb.run("check").returncode, 0)

    # -- T12 ---------------------------------------------------------------
    def test_T12_no_partial_writes_on_any_error(self):
        doc = self.sb.doc("KOLA_01_DOWODY_PL.md")
        self.sb.bootstrap()
        self.sb.run("render")
        snapshot = {p: digest(p) for p in
                    [self.sb.summary, doc, self.sb.raw_v2,
                     self.sb.evidence / "RAW_MANIFEST.json"]}
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig")
        self.sb.raw_v2.write_text(txt.replace("=== C) MASS", "=== ZEPSUTE"), encoding="utf-8")
        for cmd in ("extract", "render", "check"):
            with self.subTest(cmd=cmd):
                self.assertEqual(self.sb.run(cmd).returncode, 1)
        for path, before in snapshot.items():
            if path == self.sb.raw_v2:
                continue
            self.assertEqual(digest(path), before, f"{path.name} zmieniony mimo bledu")

    # -- dodatkowe ---------------------------------------------------------
    def test_extra_missing_manifest_fails(self):
        self.sb.doc("KOLA_01_DOWODY_PL.md")
        self.assertEqual(self.sb.run("extract").returncode, 0)
        res = self.sb.run("check")
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("brak RAW_MANIFEST.json", res.stdout)

    def test_extra_unknown_command_exit_2(self):
        self.assertEqual(self.sb.run("nieznana").returncode, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
