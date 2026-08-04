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
import os
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
        # These tests perform many atomic writes with fsync. On Linux, use the
        # RAM-backed temp filesystem when available so storage latency cannot
        # turn a logic regression suite into a random multi-minute stall.
        # Set JV_EVIDENCE_TEST_TMP to override; Windows keeps its normal temp.
        configured = os.environ.get("JV_EVIDENCE_TEST_TMP")
        if configured:
            temp_parent = Path(configured)
        elif os.name != "nt":
            temp_parent = Path("/dev/shm")
        else:
            temp_parent = None
        temp_dir = (
            str(temp_parent)
            if temp_parent is not None and temp_parent.is_dir() and os.access(temp_parent, os.W_OK)
            else None
        )
        self.root = Path(tempfile.mkdtemp(prefix="jp01_", dir=temp_dir))
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

    def reregister(self) -> None:
        """Rejestruje AKTUALNY stan raw, kasujac wczesniejszy manifest.

        Od JP-01.2 `extract` odmawia pracy na niezarejestrowanym raw, a
        `register` nie nadpisuje historii. Testy parsera musza wiec jawnie
        powiedziec "to jest log, ktory teraz mamy" - inaczej zatrzymywalaby je
        bramka manifestu i nigdy nie dochodzilyby do parsera. Odmowa nadpisania
        jest sprawdzana osobno (T15), tu nie jest przedmiotem testu.
        """
        (self.evidence / "RAW_MANIFEST.json").unlink()
        assert self.run("register").returncode == 0

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
        self.sb.reregister()
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
        self.sb.reregister()
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
        self.sb.reregister()
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
        self.sb.reregister()
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
                sb.reregister()
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

    # -- T13 (JP-01.1) -----------------------------------------------------
    def test_T13_golden_values_from_real_raw(self):
        """Golden test na PRAWDZIWYM logu v2.

        Suita JP-01 sprawdzala wylacznie kody wyjscia i niezmiennosc plikow.
        Blad przesuniecia kolumn w C.mass (mass_kg = 4.633 zamiast 43.83)
        przechodzil ja na zielono i zostal znaleziony okiem, nie testem.
        Ponizsze liczby sa odczytane z `run_2026_07_27_v2.txt`:

            envelope          mass_kg     I_spin    I_trans   I_sp/mr2   note
            sphere              43.83      4.633      4.633      0.400   1 shape(s)
        """
        self.sb.bootstrap()
        tables = json.loads(self.sb.summary.read_text(encoding="utf-8"))\
            ["runs"]["2026_07_27_v2"]["tables"]

        mass = tables["C.mass"]
        self.assertEqual(mass["columns"],
                         ["envelope", "mass_kg", "I_spin", "I_trans", "I_sp/mr2", "note"])
        self.assertEqual(mass["rows"], [
            {"name": "sphere", "values": ["43.83", "4.633", "4.633", "0.400", "1 shape"]},
            {"name": "cylinder-32", "values": ["27.79", "3.649", "2.268", "0.497", "1 shape"]},
            {"name": "phased union-4", "values": ["27.79", "3.649", "2.268", "0.497", "4 shape"]},
            {"name": "prism-Nmax", "values": ["27.87", "3.669", "2.279", "0.498", "1 shape"]},
            {"name": "tire profile", "values": ["23.47", "2.674", "1.657", "0.431", "1 shape"]},
        ])

        roll = tables["E.1900N"]
        self.assertEqual(roll["rows"][0], {
            "name": "sphere",
            "values": ["0.5060", "1.00", "0.0", "100.0%", "13.486", "0.057", "12.93"]})
        self.assertEqual(roll["rows"][1]["values"][0], "0.1481")

        prism = tables["A.prism_budget"]
        self.assertEqual(len(prism["rows"]), 9)
        self.assertEqual(prism["rows"][0]["values"],
                         ["8", "16", "16", "48", "10", "39.134", "0.5010"])
        self.assertEqual(prism["rows"][-1]["values"],
                         ["40", "80", "80", "240", "42", "1.585", "0.5136"])

    # -- T14 (JP-01.1) -----------------------------------------------------
    def test_T14_raw_shape_change_stops_extract(self):
        """Stend dokladajacy kolumne nie moze przejsc po cichu.

        Bez kotwicy naglowka dodatkowa kolumna liczbowa w tabeli z ogonem
        tekstowym wpadala do `note` (C.mass: note = '0.777 1 shape'), nazwy
        wariantow sie zgadzaly i `check` konczyl sie kodem 0.
        """
        self.sb.bootstrap()
        before = digest(self.sb.summary)
        lines = self.sb.raw_v2.read_text(encoding="utf-8-sig").splitlines()
        for i, ln in enumerate(lines):
            if ln.startswith("envelope") and "mass_kg" in ln:
                lines[i] = ln.replace("note", "extra   note")
            elif " 1 shape(s)" in ln or " 4 shape(s)" in ln:
                lines[i] = ln.replace(" 1 shape(s)", "   0.777   1 shape(s)") \
                             .replace(" 4 shape(s)", "   0.777   4 shape(s)")
        self.sb.raw_v2.write_text("\n".join(lines), encoding="utf-8")
        self.sb.reregister()
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1, res.stdout)
        self.assertIn("naglowek kolumn", res.stdout)
        self.assertEqual(digest(self.sb.summary), before)

    def test_T14c_extra_number_in_rows_only_stops_extract(self):
        """Wariant bez zmiany naglowka - kotwica naglowka go NIE lapie.

        Lapie go dopiero sztywna dlugosc ogona: dopasowanie przesuwa sie w prawo,
        nazwa wariantu staje sie 'sphere 43.83' i wychodzi na `expect_names`.
        """
        self.sb.bootstrap()
        before = digest(self.sb.summary)
        lines = self.sb.raw_v2.read_text(encoding="utf-8-sig").splitlines()
        for i, ln in enumerate(lines):
            if " shape(s)" in ln:
                lines[i] = ln.replace(" 1 shape(s)", "   0.777   1 shape(s)") \
                             .replace(" 4 shape(s)", "   0.777   4 shape(s)")
        self.sb.raw_v2.write_text("\n".join(lines), encoding="utf-8")
        self.sb.reregister()
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1, res.stdout)
        self.assertIn("C.mass", res.stdout)
        self.assertIn("!= oczekiwane", res.stdout)
        self.assertEqual(digest(self.sb.summary), before)

    def test_T14b_renamed_header_stops_extract(self):
        self.sb.bootstrap()
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig")
        self.sb.raw_v2.write_text(txt.replace("I_sp/mr2", "I_ratio"), encoding="utf-8")
        self.sb.reregister()
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1, res.stdout)
        self.assertIn("naglowek kolumn", res.stdout)

    # -- T15 (JP-01.1) -----------------------------------------------------
    def test_T15_register_refuses_to_overwrite(self):
        """Sciezka wybielenia: zmien raw -> register -> extract -> zielony check."""
        self.sb.bootstrap()
        manifest = self.sb.evidence / "RAW_MANIFEST.json"
        before = digest(manifest)
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig")
        self.sb.raw_v2.write_text(txt.replace("1.13", "1.15"), encoding="utf-8")

        res = self.sb.run("register")
        self.assertEqual(res.returncode, 1, "register nadpisal historyczna rejestracje")
        self.assertIn("nie nadpisuje historii", res.stdout)
        self.assertEqual(digest(manifest), before, "manifest zmieniony mimo odmowy")

        self.sb.run("extract")
        res2 = self.sb.run("check")
        self.assertNotEqual(res2.returncode, 0, "zmiana raw zostala wybielona")
        self.assertIn("ZMIENIONY od rejestracji", res2.stdout)

    def test_T15b_register_is_idempotent(self):
        self.sb.bootstrap()
        manifest = self.sb.evidence / "RAW_MANIFEST.json"
        before = digest(manifest)
        self.assertEqual(self.sb.run("register").returncode, 0)
        self.assertEqual(digest(manifest), before)

    # -- T16 (JP-01.1) -----------------------------------------------------
    def test_T16_manifest_entry_cannot_escape_evidence_dir(self):
        """Przekierowany wpis zostawial prawdziwy log bez zadnej weryfikacji."""
        self.sb.bootstrap()
        decoy = self.sb.root / "podrzucony.txt"
        decoy.write_text("nie jest surowym przebiegiem\n", encoding="utf-8")
        manifest = self.sb.evidence / "RAW_MANIFEST.json"
        data = json.loads(manifest.read_text(encoding="utf-8"))
        data["runs"]["2026_07_27_v2"]["file"] = "../../../podrzucony.txt"
        data["runs"]["2026_07_27_v2"]["sha256"] = digest(decoy)
        manifest.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        res = self.sb.run("check")
        self.assertNotEqual(res.returncode, 0, "manifest wskazujacy poza katalog przeszedl check")
        self.assertIn("wymagana gola nazwa", res.stdout)

    def test_T16b_manifest_entry_must_match_run_id(self):
        self.sb.bootstrap()
        manifest = self.sb.evidence / "RAW_MANIFEST.json"
        data = json.loads(manifest.read_text(encoding="utf-8"))
        data["runs"]["2026_07_27_v2"]["file"] = "run_2026_07_25_baseline.txt"
        manifest.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        res = self.sb.run("check")
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("oczekiwano", res.stdout)

    # -- T17..T19 (JP-01.2): manifest egzekwowany PRZED zapisem -------------
    def test_T17_modified_raw_blocks_extract(self):
        """Wykrycie po fakcie to nie egzekwowanie: zapis ma sie nie odbyc."""
        self.sb.bootstrap()
        before = digest(self.sb.summary)
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig")
        self.sb.raw_v2.write_text(txt.replace("1.13", "1.15"), encoding="utf-8")
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1, "extract zapisal summary ze zmienionego raw")
        self.assertIn("ZMIENIONY od rejestracji", res.stdout)
        self.assertEqual(digest(self.sb.summary), before)

    def test_T18_modified_raw_blocks_render(self):
        doc = self.sb.doc("KOLA_01_DOWODY_PL.md")
        self.sb.bootstrap()
        self.assertEqual(self.sb.run("render").returncode, 0)
        before = digest(doc)
        txt = self.sb.raw_v2.read_text(encoding="utf-8-sig")
        self.sb.raw_v2.write_text(txt.replace("1.13", "1.15"), encoding="utf-8")
        res = self.sb.run("render")
        self.assertEqual(res.returncode, 1, "render wpuscil zmieniony raw do dokumentu")
        self.assertIn("ZMIENIONY od rejestracji", res.stdout)
        self.assertIn("zaden dokument NIE zostal zmieniony", res.stdout)
        self.assertEqual(digest(doc), before)
        self.assertNotIn("1.15", doc.read_text(encoding="utf-8"))

    def test_T19_stale_summary_blocks_render(self):
        """Dokument nie moze dostac liczb, ktorych nie widzial zaden extract."""
        doc = self.sb.doc("KOLA_01_DOWODY_PL.md")
        self.sb.bootstrap()
        before = digest(doc)
        data = json.loads(self.sb.summary.read_text(encoding="utf-8"))
        data["runs"]["2026_07_27_v2"]["tables"]["D.box"]["rows"][0]["values"][-2] = "9.99"
        self.sb.summary.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                                   encoding="utf-8")
        res = self.sb.run("render")
        self.assertEqual(res.returncode, 1, "render zadzialal na nieaktualnym summary")
        self.assertIn("nieaktualne", res.stdout)
        self.assertEqual(digest(doc), before)

    # -- T20 (JP-01.2): przenosnosc hasha raw -------------------------------
    def test_T20_raw_hash_survives_any_eol_policy(self):
        """Czysty checkout musi dac te same bajty niezaleznie od polityki EOL.

        Przed `-text` w .gitattributes blob byl znormalizowany do LF, a manifest
        hashowal CRLF z Windowsowego working tree: na Linuksie `check` zglaszal
        manipulacje na plikach, ktorych nikt nie tknal.
        """
        if not (REPO / ".git").exists():
            self.skipTest("nie jest repozytorium git")
        manifest = json.loads((REAL_EVIDENCE / "RAW_MANIFEST.json").read_text(encoding="utf-8"))
        out = Path(tempfile.mkdtemp(prefix="jp012_eol_"))
        self.addCleanup(shutil.rmtree, out, True)
        rel = [f"tools/jozz_wheel_bench/evidence/{e['file']}" for e in manifest["runs"].values()]
        for policy in ("true", "false", "input"):
            with self.subTest(autocrlf=policy):
                dest = out / policy
                dest.mkdir()
                res = subprocess.run(
                    ["git", "-c", f"core.autocrlf={policy}", "checkout-index", "-f",
                     f"--prefix={dest.as_posix()}/", "--", *rel],
                    cwd=str(REPO), capture_output=True, text=True)
                self.assertEqual(res.returncode, 0, res.stderr)
                for run_id, entry in manifest["runs"].items():
                    got = digest(dest / "tools" / "jozz_wheel_bench" / "evidence" / entry["file"])
                    self.assertEqual(got, entry["sha256"],
                                     f"{entry['file']} ma inny hash przy autocrlf={policy}")

    # -- T21 (JP-01.2): renderowanie tabeli bez kolumny nazwy ---------------
    def test_T21_table_without_name_column_renders_square(self):
        """A.prism_budget: renderer doklejal pusta komorke i dawal 8 pod 7 kolumnami."""
        self.sb.bootstrap()
        sys.path.insert(0, str(self.sb.root / "tools" / "evidence"))
        try:
            for mod in ("evidence",):
                sys.modules.pop(mod, None)
            import evidence as ev
            summary = json.loads(self.sb.summary.read_text(encoding="utf-8"))
            for table_id in ("A.prism_budget", "C.mass", "E.1900N", "D.box"):
                with self.subTest(table=table_id):
                    lines = ev.render_table(summary, "2026_07_27_v2", table_id).splitlines()
                    ncols = len(lines[0].split("|")) - 2
                    for row in lines[2:]:
                        if not row.startswith("|"):
                            break
                        self.assertEqual(len(row.split("|")) - 2, ncols,
                                         f"{table_id}: {row}")
                    self.assertNotIn("|  |", lines[2], f"{table_id}: pusta komorka wiodaca")
        finally:
            sys.path.pop(0)
            sys.modules.pop("evidence", None)

    # -- T22..T24 (JP-01.2): walidacja manifestu ----------------------------
    def _corrupt_manifest(self, mutate) -> subprocess.CompletedProcess:
        self.sb.bootstrap()
        path = self.sb.evidence / "RAW_MANIFEST.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        mutate(data)
        path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        return self.sb.run("check")

    def test_T22_unsupported_schema_rejected(self):
        def mutate(d):
            d["schema"] = 99
        res = self._corrupt_manifest(mutate)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("schema", res.stdout)

    def test_T23_wrong_bytes_rejected(self):
        def mutate(d):
            d["runs"]["2026_07_27_v2"]["bytes"] = 1
        res = self._corrupt_manifest(mutate)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("bajtow, manifest mowi 1", res.stdout)

    def test_T23b_bad_field_types_rejected(self):
        for field, value, needle in (("sha256", "krotki", "sha256"),
                                     ("bytes", "duzo", "bytes"),
                                     ("mode", "wymyslony", "mode")):
            with self.subTest(field=field):
                sb = Sandbox()
                self.addCleanup(sb.cleanup)
                sb.bootstrap()
                path = sb.evidence / "RAW_MANIFEST.json"
                data = json.loads(path.read_text(encoding="utf-8"))
                data["runs"]["2026_07_27_v2"][field] = value
                path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
                res = sb.run("check")
                self.assertNotEqual(res.returncode, 0)
                self.assertIn(needle, res.stdout)

    def test_T24_absolute_path_rejected(self):
        def mutate(d):
            d["runs"]["2026_07_27_v2"]["file"] = "C:/Windows/run_x.txt"
        res = self._corrupt_manifest(mutate)
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("wymagana gola nazwa", res.stdout)

    def test_T24b_broken_manifest_json_is_reported_not_crash(self):
        self.sb.bootstrap()
        (self.sb.evidence / "RAW_MANIFEST.json").write_text("{ to nie jest json",
                                                            encoding="utf-8")
        res = self.sb.run("check")
        self.assertEqual(res.returncode, 1)
        self.assertIn("nie jest poprawnym JSON", res.stdout)
        self.assertNotIn("Traceback", res.stderr)

    # -- T25 (JP-01.3): register waliduje istniejacy manifest ---------------
    def _register_on_broken_manifest(self, mutate) -> tuple[subprocess.CompletedProcess, bool]:
        self.sb.bootstrap()
        path = self.sb.evidence / "RAW_MANIFEST.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        mutate(data)
        path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        before = digest(path)
        res = self.sb.run("register")
        return res, digest(path) == before

    def test_T25_register_rejects_unsupported_schema(self):
        """Wczesniej: rc 0 i manifest przepisany mimo schema 99."""
        res, unchanged = self._register_on_broken_manifest(lambda d: d.update(schema=99))
        self.assertEqual(res.returncode, 1)
        self.assertIn("schema", res.stdout)
        self.assertNotIn("Traceback", res.stderr)
        self.assertTrue(unchanged, "register zmienil niepoprawny manifest")

    def test_T25b_register_rejects_bad_runs_type(self):
        """Wczesniej: AttributeError: 'list' object has no attribute 'get'."""
        res, unchanged = self._register_on_broken_manifest(lambda d: d.update(runs=[]))
        self.assertEqual(res.returncode, 1)
        self.assertIn("runs", res.stdout)
        self.assertNotIn("Traceback", res.stderr)
        self.assertTrue(unchanged)

    def test_T25c_register_rejects_missing_entry_field(self):
        """Wczesniej: KeyError: 'sha256'."""
        res, unchanged = self._register_on_broken_manifest(
            lambda d: d["runs"]["2026_07_27_v2"].pop("sha256"))
        self.assertEqual(res.returncode, 1)
        self.assertIn("sha256", res.stdout)
        self.assertNotIn("Traceback", res.stderr)
        self.assertTrue(unchanged)

    def test_T25d_register_still_accepts_normal_raw(self):
        """Zaostrzenie nie moze zablokowac zwyklej, poprawnej rejestracji."""
        res = self.sb.run("register")
        self.assertEqual(res.returncode, 0, res.stdout + res.stderr)
        manifest = json.loads((self.sb.evidence / "RAW_MANIFEST.json").read_text(encoding="utf-8"))
        # The evidence set grows over time. The contract is not "exactly the two
        # runs that existed when this test was written"; it is "every tracked
        # run_*.txt in the fixture is registered, and nothing else is".
        expected_runs = sorted(
            path.stem.removeprefix("run_") for path in self.sb.evidence.glob("run_*.txt")
        )
        self.assertEqual(sorted(manifest["runs"]), expected_runs)
        self.assertEqual(self.sb.run("extract").returncode, 0)
        self.assertEqual(self.sb.run("check").returncode, 0)

    # -- T26 (JP-01.3): raw musi byc zwyklym plikiem w evidence -------------
    def test_T26_resolved_path_outside_evidence_is_rejected(self):
        """Czysta funkcja - testowalna takze tam, gdzie nie wolno tworzyc symlinkow."""
        sys.path.insert(0, str(TOOL_SRC.parent))
        try:
            from evidence import resolved_is_in_evidence_dir as inside
        finally:
            sys.path.pop(0)
        ev = Path("/repo/tools/jozz_wheel_bench/evidence").resolve()
        self.assertTrue(inside(ev, ev / "run_x.txt"))
        self.assertFalse(inside(ev, Path("/repo/gdzie_indziej/run_x.txt").resolve()),
                         "plik spoza katalogu uznany za wewnetrzny")
        self.assertFalse(inside(ev, ev / "podkatalog" / "run_x.txt"),
                         "plik w podkatalogu uznany za bezposredni")

    def test_T26b_symlinked_raw_is_rejected(self):
        outside = self.sb.root / "obcy_run.txt"
        outside.write_text("nie jest surowym przebiegiem\n", encoding="utf-8")
        link = self.sb.evidence / "run_2026_07_30_link.txt"
        try:
            link.symlink_to(outside)
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"system nie pozwala utworzyc dowiazania: {exc}")
        res = self.sb.run("register")
        self.assertEqual(res.returncode, 1, "symlink zostal zarejestrowany")
        self.assertIn("dowiazanie symboliczne", res.stdout)
        self.assertNotIn("Traceback", res.stderr)
        self.assertFalse((self.sb.evidence / "RAW_MANIFEST.json").exists(),
                         "manifest powstal mimo odrzucenia")

    def test_T26c_non_regular_file_is_rejected(self):
        """Wariant wykonalny bez uprawnien do symlinkow: katalog o nazwie run_*.txt.

        T26b bywa pominiete na Windows, a pominiety test nie dowodzi niczego -
        ten sprawdza te sama galaz `raw_file_problem` na kazdej platformie.
        """
        (self.sb.evidence / "run_2026_07_30_katalog.txt").mkdir()
        res = self.sb.run("register")
        self.assertEqual(res.returncode, 1)
        self.assertIn("nie jest zwyklym plikiem", res.stdout)
        self.assertNotIn("Traceback", res.stderr)
        self.assertFalse((self.sb.evidence / "RAW_MANIFEST.json").exists())

    # -- dodatkowe ---------------------------------------------------------
    def test_extra_missing_manifest_fails(self):
        """Bez manifestu nie wolno ani wyciagnac, ani wyrenderowac, ani przejsc check.

        Do JP-01.2 `extract` bez manifestu konczyl sie kodem 0 i zapisywal summary.
        """
        self.sb.doc("KOLA_01_DOWODY_PL.md")
        res = self.sb.run("extract")
        self.assertEqual(res.returncode, 1, "extract zapisal summary bez manifestu")
        self.assertIn("brak RAW_MANIFEST.json", res.stdout)
        self.assertFalse(self.sb.summary.exists(), "summary powstalo mimo braku manifestu")
        self.assertEqual(self.sb.run("render").returncode, 1)
        res = self.sb.run("check")
        self.assertNotEqual(res.returncode, 0)
        self.assertIn("brak RAW_MANIFEST.json", res.stdout)

    def test_extra_unknown_command_exit_2(self):
        self.assertEqual(self.sb.run("nieznana").returncode, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
