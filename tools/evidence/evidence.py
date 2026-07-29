#!/usr/bin/env python3
"""Lancuch dowodowy programu KOLA: raw log -> summary.json -> tabela w dokumencie.

Powod istnienia (2026-07-29): tabela CPU w KOLA_01 §7.2 zawierala dziesiec liczb,
z ktorych ZADNA nie wystepowala w zachowanym surowym przebiegu. Liczby byly
przepisane z kontekstu, nie odczytane z pliku. Roznice 1-5% wygladaly wiarygodnie
i dlatego nikt ich nie zauwazyl przez dwa dni.

Ten skrypt sprawia, ze taki blad jest niemozliwy strukturalnie, a nie tylko
zabroniony regulaminem:

  extract  raw log -> summary.json (z SHA-256 zrodla)
  render   summary.json -> tabele wstrzykniete do blokow EVIDENCE w dokumentach
  check    weryfikuje, ze bloki w dokumentach sa identyczne z regeneracja,
           ze kazde F-xx/P-xx ma dokladnie jeden aktualny status w rejestrze,
           i ze zadna tabela liczbowa w sekcji dowodowej nie stoi poza blokiem

Uzycie (zawsze z roota repo):
  python tools/evidence/evidence.py extract
  python tools/evidence/evidence.py render
  python tools/evidence/evidence.py check
"""

import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE_DIR = ROOT / "tools" / "jozz_wheel_bench" / "evidence"
SUMMARY = EVIDENCE_DIR / "summary.json"
FINDINGS = ROOT / "docs" / "KOLA_FINDINGS.json"
DOCS = sorted((ROOT / "docs").glob("KOLA_*.md"))

# Schemat kazdej parsowanej tabeli jest ZADEKLAROWANY, nie zgadywany z naglowka.
# Powod: naglowek sekcji D ma wielowyrazowe nazwy kolumn ('n=0 ms'), wiec liczenie
# tokenow naglowka mapowalo kolumny po cichu zle. Deklaracja sprawia, ze zmiana
# formatu wydruku stendu wywala extract z bledem zamiast produkowac zla tabele.
#
# marker      : linia otwierajaca sekcje w surowym logu
# table_index : ktora tabela wewnatrz sekcji (E ma dwie: 1900 N i 432 N)
# name_col    : czy pierwsza kolumna to nazwa wariantu (moze zawierac spacje)
# numeric     : nazwy kolumn liczbowych, od lewej do prawej
# text_tail   : kolumny tekstowe na koncu wiersza
#
# Rejestrujemy schematy tylko dla przebiegow, ktore realnie parsujemy. Przebieg
# v1 (2026-07-25) jest SUPERSEDED - zostaje zahaszowany i zachowany jako surowy
# artefakt, ale nie jest parsowany, zeby nie mapowac go po cichu na schemat v2.
RUN_SCHEMAS = {
    "2026_07_27_v2": {
        "A.prism_budget": dict(
            marker="=== A) HULL BUDGET: straight prism", table_index=0, name_col=False,
            numeric=["sides", "pts_in", "verts", "halfedg", "faces", "ripple_mm", "meanR_m"]),
        "C.mass": dict(
            marker="=== C) MASS / INERTIA CONFOUND", table_index=0, name_col=True,
            numeric=["mass_kg", "I_spin", "I_trans", "I_sp/mr2"], text_tail=["note"]),
        "E.1900N": dict(
            marker="=== E) ROLL QUALITY", table_index=0, name_col=True,
            numeric=["vy_rms", "impulse_active_pts", "churn_%", "impulse_active_%",
                     "penet_mm", "ydrop_mm", "vx_end"]),
        "E.432N": dict(
            marker="=== E) ROLL QUALITY", table_index=1, name_col=True,
            numeric=["vy_rms", "impulse_active_pts", "churn_%", "impulse_active_%", "penet_mm"]),
        "D.box": dict(
            marker="=== D) CPU COST - MARGINAL, box", table_index=0, name_col=True,
            numeric=["n=0 ms", "n=1 ms", "n=2 ms", "n=4 ms", "n=8 ms", "us/wheel", "spread%"]),
        "D.mesh": dict(
            marker="=== D) CPU COST - MARGINAL, MESH", table_index=0, name_col=True,
            numeric=["n=0 ms", "n=1 ms", "n=2 ms", "n=4 ms", "n=8 ms", "us/wheel", "spread%"]),
    },
}

BLOCK_RE = re.compile(
    r"(?P<open><!-- EVIDENCE:BEGIN run=(?P<run>[\w.\-]+) id=(?P<id>[\w.\-]+) -->\n)"
    r"(?P<body>.*?)"
    r"(?P<close><!-- EVIDENCE:END -->)",
    re.DOTALL,
)

FINDING_RE = re.compile(r"\b([FP]-\d{2})\b")
NUMERIC_ROW_RE = re.compile(r"^\|.*\d+\.\d+.*\|\s*$")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


# --------------------------------------------------------------------------
# extract
# --------------------------------------------------------------------------

def parse_header(lines):
    """Naglowek stendu: '  klucz : wartosc'."""
    prov = {}
    for line in lines[:12]:
        if ":" not in line or line.startswith("==="):
            continue
        key, _, val = line.partition(":")
        key = key.strip()
        if key and not key.startswith("#"):
            prov[key] = val.strip()
    return prov


def is_number(tok: str) -> bool:
    return bool(re.fullmatch(r"[-+]?\d*\.?\d+%?", tok))


def parse_rows(body, spec):
    """Wyciaga wiersze pasujace do zadeklarowanego schematu.

    Nazwa wariantu moze zawierac spacje ('phased union-4'), wiec kolumny liczbowe
    czytamy OD PRAWEJ. Trailing '(1.00x sphere)' jest odcinany jako komentarz.
    Zwraca liste blokow wierszy - tyle, ile tabel o tym schemacie jest w sekcji.
    """
    nnum = len(spec["numeric"])
    ntail = len(spec.get("text_tail", []))
    blocks, current = [], []
    for raw in body:
        stripped = re.sub(r"\([^)]*\)\s*$", "", raw).rstrip()
        toks = stripped.split()
        need = nnum + ntail + (1 if spec["name_col"] else 0)
        ok = False
        if len(toks) >= need:
            nums = toks[len(toks) - ntail - nnum: len(toks) - ntail] if ntail else toks[-nnum:]
            ok = all(is_number(t) for t in nums)
            if ok and not spec["name_col"]:
                ok = len(toks) == nnum
        if ok:
            tail = toks[len(toks) - ntail:] if ntail else []
            name = " ".join(toks[: len(toks) - ntail - nnum]) if spec["name_col"] else ""
            current.append({"name": name, "values": list(nums) + ([" ".join(tail)] if ntail else [])})
        elif current:
            blocks.append(current)
            current = []
    if current:
        blocks.append(current)
    return blocks


def find_table(lines, spec):
    marker = spec["marker"]
    hits = [i for i, l in enumerate(lines) if l.startswith(marker)]
    if not hits:
        return None, f"nie znaleziono sekcji '{marker}'"
    start = hits[0]
    end = len(lines)
    for j in range(start + 1, len(lines)):
        if lines[j].startswith("==="):
            end = j
            break
    blocks = parse_rows(lines[start + 1:end], spec)
    idx = spec["table_index"]
    if len(blocks) <= idx:
        return None, f"sekcja '{marker}' ma {len(blocks)} tabel, oczekiwano indeksu {idx}"
    columns = ([spec.get("name_label", "envelope")] if spec["name_col"] else []) \
        + spec["numeric"] + spec.get("text_tail", [])
    return {"columns": columns, "rows": blocks[idx]}, None


def cmd_extract():
    runs = {}
    problems = []
    for raw in sorted(EVIDENCE_DIR.glob("run_*.txt")):
        text = raw.read_text(encoding="utf-8-sig")
        lines = text.splitlines()
        run_id = raw.stem.replace("run_", "")
        schema = RUN_SCHEMAS.get(run_id)
        tables = {}
        if schema is None:
            print(f"  {raw.name}: brak zadeklarowanego schematu -> zachowany jako surowy artefakt")
        else:
            for tid, spec in schema.items():
                t, err = find_table(lines, spec)
                if err:
                    problems.append(f"{raw.name} / {tid}: {err}")
                else:
                    tables[tid] = t
        runs[run_id] = {
            "source_file": raw.name,
            "source_sha256": sha256(raw),
            "source_bytes": raw.stat().st_size,
            "provenance": parse_header(lines),
            "tables": tables,
        }
        if schema is not None:
            print(f"  {raw.name}: {len(tables)}/{len(schema)} tabel, sha256 {sha256(raw)[:12]}")
    payload = {
        "schema": 1,
        "generator": "tools/evidence/evidence.py",
        "runs": runs,
    }
    SUMMARY.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"-> {SUMMARY.relative_to(ROOT)}")
    for p in problems:
        print(f"  BLAD {p}")
    return 1 if problems else 0


# --------------------------------------------------------------------------
# render
# --------------------------------------------------------------------------

def load_summary():
    if not SUMMARY.exists():
        sys.exit("BRAK summary.json - uruchom najpierw: python tools/evidence/evidence.py extract")
    return json.loads(SUMMARY.read_text(encoding="utf-8"))


def render_table(summary, run_id, table_id):
    run = summary["runs"].get(run_id)
    if run is None:
        return None, f"nieznany run '{run_id}'"
    table = run["tables"].get(table_id)
    if table is None:
        return None, f"run '{run_id}' nie zawiera tabeli '{table_id}'"
    cols = table["columns"]
    out = ["| " + " | ".join(cols) + " |",
           "|" + "|".join(["---"] * len(cols)) + "|"]
    for row in table["rows"]:
        out.append("| " + " | ".join([row["name"]] + row["values"]) + " |")
    out.append("")
    out.append(f"*zrodlo: `{run['source_file']}` sha256 `{run['source_sha256'][:16]}` "
               f"tabela `{table_id}` - wygenerowane, nie przepisywac recznie*")
    return "\n".join(out) + "\n", None


def rebuild_blocks(text, summary, path_label, errors):
    def repl(m):
        body, err = render_table(summary, m.group("run"), m.group("id"))
        if err:
            errors.append(f"{path_label}: blok {m.group('id')} - {err}")
            return m.group(0)
        return m.group("open") + body + m.group("close")
    return BLOCK_RE.sub(repl, text)


def cmd_render():
    summary = load_summary()
    errors = []
    changed = 0
    for doc in DOCS:
        text = doc.read_text(encoding="utf-8")
        new = rebuild_blocks(text, summary, doc.name, errors)
        if new != text:
            doc.write_text(new, encoding="utf-8")
            changed += 1
            print(f"  zaktualizowano {doc.name}")
    for e in errors:
        print(f"  BLAD {e}")
    print(f"-> {changed} plikow zaktualizowanych")
    return 1 if errors else 0


# --------------------------------------------------------------------------
# check
# --------------------------------------------------------------------------

def cmd_check():
    summary = load_summary()
    failures = []
    warnings = []

    # 1. surowe logi nie zmienily sie od czasu ekstrakcji
    for run_id, run in summary["runs"].items():
        raw = EVIDENCE_DIR / run["source_file"]
        if not raw.exists():
            failures.append(f"evidence: brak pliku {run['source_file']} opisanego w summary.json")
        elif sha256(raw) != run["source_sha256"]:
            failures.append(f"evidence: {run['source_file']} zmieniony po ekstrakcji - uruchom extract")

    # 2. kazdy blok EVIDENCE w dokumencie == regeneracja z summary.json
    for doc in DOCS:
        text = doc.read_text(encoding="utf-8")
        rebuilt = rebuild_blocks(text, summary, doc.name, failures)
        if rebuilt != text:
            failures.append(f"{doc.name}: blok EVIDENCE rozjechal sie z summary.json "
                            f"(liczby przepisane recznie?) - uruchom render")

    # 3. rejestr findingow: kazde F-xx/P-xx uzyte w dokumentach ma jeden aktualny status
    if not FINDINGS.exists():
        failures.append("brak docs/KOLA_FINDINGS.json - rejestr statusow jest obowiazkowy")
    else:
        ledger = json.loads(FINDINGS.read_text(encoding="utf-8"))
        known = set(ledger["findings"].keys())
        used = set()
        for doc in DOCS:
            used |= set(FINDING_RE.findall(doc.read_text(encoding="utf-8")))
        for fid in sorted(used - known):
            failures.append(f"rejestr: {fid} uzyte w dokumentach, brak wpisu w KOLA_FINDINGS.json")
        for fid in sorted(known - used):
            warnings.append(f"rejestr: {fid} w rejestrze, nieuzywane w zadnym dokumencie")
        for fid, entry in ledger["findings"].items():
            for ref in entry.get("evidence", []):
                if ref == "none":
                    continue
                run_id, _, table_id = ref.partition("#")
                run = summary["runs"].get(run_id)
                if run is None or table_id not in run.get("tables", {}):
                    failures.append(f"rejestr: {fid} wskazuje na nieistniejacy dowod '{ref}'")
            if entry.get("status") not in ledger["statuses"]:
                failures.append(f"rejestr: {fid} ma status '{entry.get('status')}' spoza slownika")

    # 4. tabele DANYCH poza blokiem EVIDENCE
    # Heurystyka celuje w tabele pomiarowe, nie w tabele prozy z pojedyncza liczba
    # w tekscie (rejestr wad W-xx cytuje numery linii i wartosci jak 9.81).
    # Kryterium: >=3 kolejne wiersze, kazdy z >=3 liczbami dziesietnymi.
    kola01 = ROOT / "docs" / "KOLA_01_DOWODY_PL.md"
    if kola01.exists():
        text = kola01.read_text(encoding="utf-8")
        masked = BLOCK_RE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
        run_start, run_len = None, 0
        for i, line in enumerate(masked.splitlines() + [""], 1):
            dense = (line.lstrip().startswith("|")
                     and len(re.findall(r"\d+\.\d+", line)) >= 3)
            if dense:
                run_start = run_start or i
                run_len += 1
                continue
            if run_len >= 3:
                warnings.append(f"KOLA_01:{run_start}: tabela danych ({run_len} wierszy) "
                                f"poza blokiem EVIDENCE - liczby nie sa zwiazane z surowym logiem")
            run_start, run_len = None, 0

    for w in warnings:
        print(f"  OSTRZEZENIE {w}")
    for f in failures:
        print(f"  BLAD {f}")
    if failures:
        print(f"\nCHECK FAILED: {len(failures)} bledow, {len(warnings)} ostrzezen")
        return 1
    print(f"\nCHECK OK ({len(warnings)} ostrzezen)")
    return 0


def main():
    cmds = {"extract": cmd_extract, "render": cmd_render, "check": cmd_check}
    if len(sys.argv) != 2 or sys.argv[1] not in cmds:
        sys.exit(f"uzycie: python {Path(__file__).name} {{extract|render|check}}")
    sys.exit(cmds[sys.argv[1]]())


if __name__ == "__main__":
    main()
