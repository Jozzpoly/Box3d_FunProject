#!/usr/bin/env python3
"""Lancuch dowodowy programu KOLA: raw evidence -> summary -> widok w dokumencie.

Powod istnienia (2026-07-29): tabela CPU w KOLA_01 §7.2 zawierala dziesiec liczb,
z ktorych ZADNA nie wystepowala w zachowanym surowym przebiegu. Liczby byly
przepisane recznie. Roznice 1-5% wygladaly wiarygodnie i przetrwaly dwa dni.

Wersja v1 tego narzedzia zamykala tylko ogniwo `summary -> dokument`. Ogniwo
`raw -> summary` pozostawalo forgowalne: recznie zmienione summary.json bylo
renderowane do dokumentu i przechodzilo `check` z kodem 0, a dokument nosil
przy tym pieczatke `sha256 ...` wskazujaca na surowy log o innej tresci.

Model danych (JP-01):

    raw          jest ZRODLEM danych pomiarowych i jest niezmienny
    summary      jest artefaktem POCHODNYM (cache parsowania)
    dokument     jest widokiem POCHODNYM

`check` nie ufa zadnemu ogniwu posredniemu - parsuje surowe logi od nowa
i porownuje semantycznie w dol lancucha.

Komendy:
    register   jawnie rejestruje run w RAW_MANIFEST.json (sha256 + tryb)
    extract    raw -> summary.json            (atomowo, wszystko albo nic)
    render     summary -> bloki w dokumentach (transakcyjnie, wszystko albo nic)
    check      weryfikuje caly lancuch        (kod != 0 przy dowolnym zerwaniu)

Uzycie (zawsze z roota repo):
    python tools/evidence/evidence.py check
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE_DIR = ROOT / "tools" / "jozz_wheel_bench" / "evidence"
SUMMARY_NAME = "summary.json"
MANIFEST_NAME = "RAW_MANIFEST.json"
FINDINGS = ROOT / "docs" / "KOLA_FINDINGS.json"
DOCS_GLOB = "KOLA_*.md"


class EvidenceError(Exception):
    """Blad lancucha dowodowego. Zawsze glosny, nigdy normalizowany do zera."""


# ---------------------------------------------------------------------------
# Schematy
# ---------------------------------------------------------------------------
# Schemat kazdej parsowanej tabeli jest ZADEKLAROWANY, nie zgadywany z naglowka.
# Powod: naglowek sekcji D ma wielowyrazowe nazwy kolumn ('n=0 ms'), wiec
# liczenie tokenow naglowka mapowalo kolumny po cichu zle.
#
#   marker       linia otwierajaca sekcje; musi wystapic DOKLADNIE raz
#   table_index  ktora tabela wewnatrz sekcji (E ma dwie: 1900 N i 432 N)
#   name_col     czy pierwsza kolumna to nazwa wariantu (moze zawierac spacje)
#   numeric      nazwy kolumn liczbowych, od lewej do prawej
#   text_tail    kolumny tekstowe na koncu wiersza
#   expect_names dokladny zbior nazw wariantow (kolejnosc tez jest sprawdzana)
#   expect_rows  dokladna liczba wierszy dla tabel bez kolumny nazwy
#
# Przebieg v1 (2026-07-25) NIE ma schematu: jego format rozni sie od v2, a ciche
# zmapowanie go na schemat v2 dawaloby przeklamania. Jest `raw-only`: zachowany
# i hashowany, nieparsowany. To nie jest blad.
_ENVELOPES = ["sphere", "cylinder-32", "phased union-4", "prism-Nmax", "tire profile"]
_CPU_COLS = ["n=0 ms", "n=1 ms", "n=2 ms", "n=4 ms", "n=8 ms", "us/wheel", "spread%"]

RUN_SCHEMAS = {
    "2026_07_27_v2": {
        "A.prism_budget": dict(
            marker="=== A) HULL BUDGET: straight prism", table_index=0, name_col=False,
            numeric=["sides", "pts_in", "verts", "halfedg", "faces", "ripple_mm", "meanR_m"],
            expect_rows=9),
        "C.mass": dict(
            marker="=== C) MASS / INERTIA CONFOUND", table_index=0, name_col=True,
            numeric=["mass_kg", "I_spin", "I_trans", "I_sp/mr2"], text_tail=["note"],
            expect_names=_ENVELOPES),
        # Surowy log nazywa te kolumny `loadedPts` / `loaded_%`. Nazwa jest zbyt
        # mocna: totalNormalImpulse > 0 znaczy "punkt otrzymal impuls podczas
        # podkrokow", co obejmuje kontakt spekulatywny. Mapowanie pozycja ->
        # nazwa semantyczna jest tutaj jawne i swiadome.
        "E.1900N": dict(
            marker="=== E) ROLL QUALITY", table_index=0, name_col=True,
            numeric=["vy_rms", "impulse_active_pts", "churn_%", "impulse_active_%",
                     "penet_mm", "ydrop_mm", "vx_end"],
            expect_names=_ENVELOPES),
        "E.432N": dict(
            marker="=== E) ROLL QUALITY", table_index=1, name_col=True,
            numeric=["vy_rms", "impulse_active_pts", "churn_%", "impulse_active_%", "penet_mm"],
            expect_names=_ENVELOPES),
        "D.box": dict(
            marker="=== D) CPU COST - MARGINAL, box", table_index=0, name_col=True,
            numeric=list(_CPU_COLS), expect_names=_ENVELOPES),
        "D.mesh": dict(
            marker="=== D) CPU COST - MARGINAL, MESH", table_index=0, name_col=True,
            numeric=list(_CPU_COLS), expect_names=_ENVELOPES),
    },
}

BLOCK_RE = re.compile(
    r"(?P<open><!-- EVIDENCE:BEGIN run=(?P<run>[\w.\-]+) id=(?P<id>[\w.\-]+) -->\n)"
    r"(?P<body>.*?)"
    r"(?P<close><!-- EVIDENCE:END -->)",
    re.DOTALL,
)
FINDING_RE = re.compile(r"\b([FP]-\d{2})\b")
NUMBER_RE = re.compile(r"[-+]?\d*\.?\d+%?\Z")
BAD_NUMBER_RE = re.compile(r"\A[-+]?(nan|inf|infinity|1\.#inf|1\.#qnan)", re.IGNORECASE)


# ---------------------------------------------------------------------------
# I/O - jawne, deterministyczne, zachowujace zakonczenia linii
# ---------------------------------------------------------------------------

def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_document(path: Path) -> tuple[str, str]:
    """Zwraca (tresc znormalizowana do \\n, dominujace zakonczenie linii).

    Korpus ma MIESZANE zakonczenia (KOLA_01 = CRLF, KOLA_02 = LF), a
    `.gitattributes` ma `* text=auto`. Jednolity zapis przepisalby caly plik
    i utopil realna zmiane w szumie, dlatego styl kazdego pliku jest wykrywany
    i odtwarzany osobno.
    """
    raw = path.read_bytes()
    crlf = raw.count(b"\r\n")
    lf = raw.count(b"\n") - crlf
    newline = "\r\n" if crlf > lf else "\n"
    return raw.decode("utf-8").replace("\r\n", "\n"), newline


def atomic_write(path: Path, text: str, newline: str = "\n") -> None:
    """Zapis wszystko-albo-nic: plik tymczasowy w TYM SAMYM katalogu + os.replace.

    os.replace jest atomowe takze na Windows (MoveFileEx z REPLACE_EXISTING).
    """
    data = text.replace("\n", newline).encode("utf-8")
    fd, tmp = tempfile.mkstemp(dir=str(path.parent), prefix=f".{path.name}.", suffix=".tmp")
    try:
        with os.fdopen(fd, "wb") as fh:
            fh.write(data)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, path)
    except BaseException:
        Path(tmp).unlink(missing_ok=True)
        raise


def git(*args: str) -> tuple[int, str]:
    try:
        p = subprocess.run(["git", *args], cwd=str(ROOT), capture_output=True, text=True)
        return p.returncode, p.stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return -1, ""


# ---------------------------------------------------------------------------
# Rdzen parsowania - JEDNA implementacja uzywana przez extract, check i testy
# ---------------------------------------------------------------------------

def _is_number(tok: str) -> bool:
    if BAD_NUMBER_RE.match(tok):
        return False
    return bool(NUMBER_RE.match(tok))


def _parse_row(toks: list[str], spec: dict, nnum: int, ntail: int) -> dict | None:
    """Dopasowuje wiersz danych. Zwraca None, gdy linia nie jest wierszem tej tabeli.

    Kolumny liczbowe znajdowane sa skanem OD LEWEJ, a nie na stalej pozycji od
    prawej. Pozycja od prawej zakladala, ze ogon tekstowy ma dokladnie `ntail`
    tokenow - a kolumna `note` w tabeli C ma dwa ('1 shape(s)'), przez co nazwa
    wariantu wchlaniala pierwsza liczbe i cala tabela byla przesunieta o jedna
    kolumne. Blad byl CICHY: wartosci nadal wygladaly jak liczby.

    ntail == 0  -> liczby musza konczyc wiersz (brak nieprzetworzonego ogona)
    ntail >= 1  -> reszta wiersza to jedna kolumna wolnego tekstu
    """
    if not spec["name_col"]:
        if len(toks) != nnum or not all(_is_number(t) for t in toks):
            return None
        return {"name": "", "values": list(toks)}

    for i in range(1, len(toks) - nnum + 1):
        span = toks[i:i + nnum]
        if not all(_is_number(t) for t in span):
            continue
        tail = toks[i + nnum:]
        if ntail == 0:
            if tail:
                continue  # nieprzetworzony ogon - to nie jest wiersz tej tabeli
            return {"name": " ".join(toks[:i]), "values": list(span)}
        if not tail:
            continue
        return {"name": " ".join(toks[:i]), "values": list(span) + [" ".join(tail)]}
    return None


def parse_section(lines: list[str], spec: dict, table_id: str) -> dict:
    """Parsuje jedna zadeklarowana tabele. Zawodzi GLOSNO, nigdy po cichu."""
    marker = spec["marker"]
    hits = [i for i, ln in enumerate(lines) if ln.startswith(marker)]
    if len(hits) != 1:
        raise EvidenceError(
            f"{table_id}: marker {marker!r} wystapil {len(hits)}x, oczekiwano dokladnie 1")

    start = hits[0]
    end = next((j for j in range(start + 1, len(lines)) if lines[j].startswith("===")), len(lines))
    body = lines[start + 1:end]

    nnum = len(spec["numeric"])
    ntail = len(spec.get("text_tail", []))
    expect_names = spec.get("expect_names")

    blocks: list[list[dict]] = []
    current: list[dict] = []
    rejected: list[str] = []
    for raw_line in body:
        stripped = re.sub(r"\([^)]*\)\s*$", "", raw_line).rstrip()
        toks = stripped.split()
        row = _parse_row(toks, spec, nnum, ntail) if toks else None
        if row is not None:
            current.append(row)
            continue
        # Linia, ktora ZACZYNA sie od oczekiwanego wariantu, a nie parsuje sie,
        # jest zapamietywana jako diagnostyka. Sekcja E zawiera dwie tabele
        # o roznej liczbie kolumn, wiec sam brak dopasowania nie jest bledem -
        # bledem jest dopiero niezgodnosc wybranego bloku z `expect_names`.
        if expect_names and any(stripped.startswith(nm) for nm in expect_names):
            rejected.append(raw_line.strip())
        if current:
            blocks.append(current)
            current = []
    if current:
        blocks.append(current)

    idx = spec["table_index"]
    if len(blocks) <= idx:
        raise EvidenceError(
            f"{table_id}: sekcja ma {len(blocks)} tabel, oczekiwano indeksu {idx}")
    rows = blocks[idx]

    if expect_names is not None:
        got = [r["name"] for r in rows]
        if got != list(expect_names):
            hint = f"; odrzucone wiersze wariantow: {rejected}" if rejected else ""
            raise EvidenceError(
                f"{table_id}: warianty {got} != oczekiwane {list(expect_names)}{hint}")
    if "expect_rows" in spec and len(rows) != spec["expect_rows"]:
        raise EvidenceError(
            f"{table_id}: {len(rows)} wierszy, oczekiwano {spec['expect_rows']}")
    if not rows:
        raise EvidenceError(f"{table_id}: brak wierszy danych")

    columns = ([spec.get("name_label", "envelope")] if spec["name_col"] else []) \
        + list(spec["numeric"]) + list(spec.get("text_tail", []))
    return {"columns": columns, "rows": rows}


def parse_provenance(lines: list[str]) -> dict:
    prov = {}
    for line in lines[:12]:
        if ":" not in line or line.startswith("==="):
            continue
        key, _, val = line.partition(":")
        key = key.strip()
        if key and not key.startswith("#"):
            prov[key] = val.strip()
    return prov


def parse_run(path: Path, schema: dict | None) -> dict:
    """Parsuje jeden surowy przebieg. `schema=None` -> tryb raw-only."""
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    tables = {}
    if schema is not None:
        for table_id, spec in schema.items():
            tables[table_id] = parse_section(lines, spec, table_id)
    return {
        "source_file": path.name,
        "source_sha256": sha256_file(path),
        "source_bytes": path.stat().st_size,
        "provenance": parse_provenance(lines),
        "tables": tables,
    }


def build_summary(evidence_dir: Path, schemas: dict) -> dict:
    """Buduje KOMPLETNE summary w pamieci. Kazdy blad przerywa - brak polowicznych wynikow."""
    runs = {}
    for raw in sorted(evidence_dir.glob("run_*.txt")):
        run_id = raw.stem[len("run_"):]
        runs[run_id] = parse_run(raw, schemas.get(run_id))
    if not runs:
        raise EvidenceError(f"brak surowych przebiegow w {evidence_dir}")
    return {"schema": 1, "generator": "tools/evidence/evidence.py", "runs": runs}


def dump_summary(summary: dict) -> str:
    return json.dumps(summary, indent=2, ensure_ascii=False) + "\n"


# ---------------------------------------------------------------------------
# Manifest niezmiennosci surowych przebiegow
# ---------------------------------------------------------------------------

def load_manifest(evidence_dir: Path) -> dict | None:
    path = evidence_dir / MANIFEST_NAME
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def verify_manifest(evidence_dir: Path, schemas: dict, manifest: dict) -> list[str]:
    """Surowy przebieg zarejestrowany pod danym run ID nie moze sie po cichu zmienic."""
    failures = []
    for run_id, entry in sorted(manifest.get("runs", {}).items()):
        raw = evidence_dir / entry["file"]
        if not raw.exists():
            failures.append(f"raw: {entry['file']} zarejestrowany, ale nie istnieje")
            continue
        actual = sha256_file(raw)
        if actual != entry["sha256"]:
            failures.append(
                f"raw: {entry['file']} ZMIENIONY od rejestracji "
                f"(manifest {entry['sha256'][:16]}, na dysku {actual[:16]}) "
                f"- historyczny artefakt zrodlowy jest niezmienny; "
                f"nowy przebieg wymaga 'register' pod nowym run ID")
        expected_mode = "parsed" if run_id in schemas else "raw-only"
        if entry.get("mode") != expected_mode:
            failures.append(
                f"raw: {run_id} ma tryb {entry.get('mode')!r}, schemat mowi {expected_mode!r}")
        rc, out = git("status", "--porcelain", "--", str(raw.relative_to(ROOT)).replace("\\", "/"))
        if rc == 0 and out.startswith("??"):
            failures.append(f"raw: {entry['file']} zarejestrowany, ale NIESLEDZONY przez git")
    for raw in sorted(evidence_dir.glob("run_*.txt")):
        run_id = raw.stem[len("run_"):]
        if run_id not in manifest.get("runs", {}):
            failures.append(f"raw: {raw.name} nie jest zarejestrowany (uzyj 'register')")
    return failures


# ---------------------------------------------------------------------------
# Renderowanie
# ---------------------------------------------------------------------------

def render_table(summary: dict, run_id: str, table_id: str) -> str:
    run = summary["runs"].get(run_id)
    if run is None:
        raise EvidenceError(f"nieznany run {run_id!r}")
    table = run["tables"].get(table_id)
    if table is None:
        raise EvidenceError(f"run {run_id!r} nie zawiera tabeli {table_id!r}")
    out = ["| " + " | ".join(table["columns"]) + " |",
           "|" + "|".join(["---"] * len(table["columns"])) + "|"]
    for row in table["rows"]:
        out.append("| " + " | ".join([row["name"]] + row["values"]) + " |")
    out.append("")
    out.append(f"*zrodlo: `{run['source_file']}` sha256 `{run['source_sha256'][:16]}` "
               f"tabela `{table_id}` - wygenerowane, nie przepisywac recznie*")
    return "\n".join(out) + "\n"


def render_text(text: str, summary: dict, label: str) -> str:
    """Zwraca tresc dokumentu z odswiezonymi blokami. Kazdy blad przerywa."""
    errors: list[str] = []

    def repl(m: re.Match) -> str:
        try:
            body = render_table(summary, m.group("run"), m.group("id"))
        except EvidenceError as exc:
            errors.append(f"{label}: blok {m.group('id')} - {exc}")
            return m.group(0)
        return m.group("open") + body + m.group("close")

    new = BLOCK_RE.sub(repl, text)
    if errors:
        raise EvidenceError("; ".join(errors))
    return new


# ---------------------------------------------------------------------------
# Komendy
# ---------------------------------------------------------------------------

def docs_of(root: Path) -> list[Path]:
    return sorted((root / "docs").glob(DOCS_GLOB))


def cmd_register(evidence_dir: Path = EVIDENCE_DIR, schemas: dict = RUN_SCHEMAS) -> int:
    """Jawna rejestracja surowych przebiegow. Jedyny sposob na zmiane manifestu."""
    manifest = load_manifest(evidence_dir) or {"schema": 1, "runs": {}}
    changed = []
    for raw in sorted(evidence_dir.glob("run_*.txt")):
        run_id = raw.stem[len("run_"):]
        digest = sha256_file(raw)
        prev = manifest["runs"].get(run_id)
        if prev and prev["sha256"] == digest:
            continue
        if prev:
            print(f"  UWAGA {run_id}: nadpisuje rejestracje "
                  f"({prev['sha256'][:16]} -> {digest[:16]})")
        manifest["runs"][run_id] = {
            "file": raw.name,
            "sha256": digest,
            "bytes": raw.stat().st_size,
            "mode": "parsed" if run_id in schemas else "raw-only",
        }
        changed.append(run_id)
    manifest["runs"] = dict(sorted(manifest["runs"].items()))
    atomic_write(evidence_dir / MANIFEST_NAME,
                 json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    for run_id in changed:
        print(f"  zarejestrowano {run_id} [{manifest['runs'][run_id]['mode']}]")
    print(f"-> {MANIFEST_NAME}: {len(manifest['runs'])} przebiegow")
    return 0


def cmd_extract(evidence_dir: Path = EVIDENCE_DIR, schemas: dict = RUN_SCHEMAS) -> int:
    """Parsuje WSZYSTKO, waliduje WSZYSTKO, dopiero potem zapisuje - atomowo."""
    try:
        summary = build_summary(evidence_dir, schemas)
    except EvidenceError as exc:
        print(f"  BLAD {exc}")
        print("  summary.json NIE zostal zmieniony")
        return 1
    atomic_write(evidence_dir / SUMMARY_NAME, dump_summary(summary))
    for run_id, run in summary["runs"].items():
        mode = f"{len(run['tables'])} tabel" if run["tables"] else "raw-only"
        print(f"  {run['source_file']}: {mode}, sha256 {run['source_sha256'][:12]}")
    print(f"-> {SUMMARY_NAME}")
    return 0


def cmd_render(root: Path = ROOT, evidence_dir: Path = EVIDENCE_DIR,
               schemas: dict = RUN_SCHEMAS) -> int:
    """Buduje tresc WSZYSTKICH dokumentow w pamieci, waliduje, dopiero potem zapisuje."""
    try:
        summary = build_summary(evidence_dir, schemas)
    except EvidenceError as exc:
        print(f"  BLAD {exc}")
        print("  zaden dokument NIE zostal zmieniony")
        return 1

    pending: list[tuple[Path, str, str]] = []
    try:
        for doc in docs_of(root):
            text, newline = read_document(doc)
            new = render_text(text, summary, doc.name)
            if new != text:
                pending.append((doc, new, newline))
    except EvidenceError as exc:
        print(f"  BLAD {exc}")
        print("  zaden dokument NIE zostal zmieniony (transakcja wycofana przed zapisem)")
        return 1

    for doc, new, newline in pending:
        atomic_write(doc, new, newline)
        print(f"  zaktualizowano {doc.name}")
    print(f"-> {len(pending)} plikow zaktualizowanych")
    return 0


def cmd_check(root: Path = ROOT, evidence_dir: Path = EVIDENCE_DIR,
              schemas: dict = RUN_SCHEMAS) -> int:
    """Nie ufa zadnemu ogniwu posredniemu: parsuje raw od nowa i schodzi w dol."""
    failures: list[str] = []
    warnings: list[str] = []

    # 1. niezmiennosc surowych przebiegow
    manifest = load_manifest(evidence_dir)
    if manifest is None:
        failures.append(f"brak {MANIFEST_NAME} - uruchom 'register'")
    else:
        failures.extend(verify_manifest(evidence_dir, schemas, manifest))

    # 2. swieze parsowanie raw (JEDNA implementacja, ta sama co w extract)
    fresh = None
    try:
        fresh = build_summary(evidence_dir, schemas)
    except EvidenceError as exc:
        failures.append(f"raw nie parsuje sie: {exc}")

    # 3. summary.json == swieze parsowanie (zamyka forgowalne ogniwo posrednie)
    summary_path = evidence_dir / SUMMARY_NAME
    stored = None
    if not summary_path.exists():
        failures.append(f"brak {SUMMARY_NAME} - uruchom 'extract'")
    else:
        try:
            stored = json.loads(summary_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            failures.append(f"{SUMMARY_NAME} nie jest poprawnym JSON: {exc}")
    if fresh is not None and stored is not None and stored != fresh:
        for run_id, run in fresh["runs"].items():
            s_run = stored.get("runs", {}).get(run_id)
            if s_run is None:
                failures.append(f"summary: brak przebiegu {run_id}")
                continue
            for key in ("source_sha256", "provenance"):
                if s_run.get(key) != run[key]:
                    failures.append(f"summary: {run_id}.{key} != swieze parsowanie raw")
            for tid, table in run["tables"].items():
                if s_run.get("tables", {}).get(tid) != table:
                    failures.append(
                        f"summary: {run_id}#{tid} ROZNI SIE od surowego logu "
                        f"(podmiana ogniwa posredniego)")
            for tid in s_run.get("tables", {}):
                if tid not in run["tables"]:
                    failures.append(f"summary: {run_id}#{tid} nie istnieje w surowym logu")
        if not failures:
            failures.append("summary: rozni sie od swiezego parsowania raw (struktura)")

    # 4. bloki EVIDENCE w dokumentach == render ze SWIEZEGO parsowania
    if fresh is not None:
        for doc in docs_of(root):
            text, _ = read_document(doc)
            try:
                if render_text(text, fresh, doc.name) != text:
                    failures.append(
                        f"{doc.name}: blok EVIDENCE rozjechal sie z surowym logiem "
                        f"(liczby przepisane recznie?) - uruchom render")
            except EvidenceError as exc:
                failures.append(str(exc))

    # 5. rejestr findingow
    if not FINDINGS.exists():
        warnings.append(f"brak {FINDINGS.name} - rejestr statusow nie jest sprawdzany")
    else:
        ledger = json.loads(FINDINGS.read_text(encoding="utf-8"))
        known = set(ledger["findings"])
        used: set[str] = set()
        for doc in docs_of(root):
            used |= set(FINDING_RE.findall(read_document(doc)[0]))
        for fid in sorted(used - known):
            failures.append(f"rejestr: {fid} uzyte w dokumentach, brak wpisu w {FINDINGS.name}")
        for fid in sorted(known - used):
            warnings.append(f"rejestr: {fid} w rejestrze, nieuzywane w zadnym dokumencie")
        for fid, entry in ledger["findings"].items():
            if entry.get("status") not in ledger["statuses"]:
                failures.append(f"rejestr: {fid} ma status {entry.get('status')!r} spoza slownika")
            for ref in entry.get("evidence", []):
                if ref == "none":
                    continue
                run_id, _, table_id = ref.partition("#")
                src = fresh or stored or {"runs": {}}
                if table_id not in src.get("runs", {}).get(run_id, {}).get("tables", {}):
                    failures.append(f"rejestr: {fid} wskazuje na nieistniejacy dowod {ref!r}")

    # 6. tabele danych poza blokiem EVIDENCE (>=3 kolejne wiersze po >=3 liczby)
    kola01 = root / "docs" / "KOLA_01_DOWODY_PL.md"
    if kola01.exists():
        masked = BLOCK_RE.sub(lambda m: "\n" * m.group(0).count("\n"), read_document(kola01)[0])
        run_start, run_len = None, 0
        for i, line in enumerate(masked.splitlines() + [""], 1):
            if line.lstrip().startswith("|") and len(re.findall(r"\d+\.\d+", line)) >= 3:
                run_start = run_start or i
                run_len += 1
                continue
            if run_len >= 3:
                warnings.append(f"KOLA_01:{run_start}: tabela danych ({run_len} wierszy) "
                                f"poza blokiem EVIDENCE")
            run_start, run_len = None, 0

    for w in warnings:
        print(f"  OSTRZEZENIE {w}")
    for f in failures:
        print(f"  BLAD {f}")
    if failures:
        print(f"\nCHECK FAILED: {len(failures)} bledow, {len(warnings)} ostrzezen")
        return 1
    print(f"\nCHECK OK ({len(warnings)} ostrzezen)")
    print("UWAGA: dowodzi wylacznie integralnosci obslugi danych w zakresie testowanym")
    print("       przez narzedzie. NIE dowodzi, ze findings sa merytorycznie prawdziwe.")
    return 0


def main(argv: list[str]) -> int:
    cmds = {"register": cmd_register, "extract": cmd_extract,
            "render": cmd_render, "check": cmd_check}
    if len(argv) != 2 or argv[1] not in cmds:
        print(f"uzycie: python {Path(__file__).name} {{{'|'.join(cmds)}}}", file=sys.stderr)
        return 2
    try:
        return cmds[argv[1]]()
    except EvidenceError as exc:
        print(f"  BLAD {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
