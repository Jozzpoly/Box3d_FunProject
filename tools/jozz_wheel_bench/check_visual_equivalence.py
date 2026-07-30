#!/usr/bin/env python3
"""Test ekwiwalencji headless <-> visual dla wspolnego rigu kola.

Pytanie, na ktore odpowiada: czy okno wizualne pokazuje TE SAMA symulacje,
ktora stend mierzy bez okna. Nie "podobna", nie "w granicach tolerancji".

Tolerancja jest zadeklarowana PRZED testem i wynosi ZERO: oba frontendy
linkuja te same funkcje z jozz_wheel_rig.c, wolaja je w tej samej kolejnosci
i formatuja stan TA SAMA funkcja JozzRig_DigestLine. Przy takiej architekturze
kazda roznica bajtowa jest defektem, a nie szumem numerycznym. Gdyby test
dopuszczal epsilon, przepuscilby wlasnie te bledy, ktore ma lapac.

Sprawdzane osobno:
  1. renderer WYLACZONY vs WLACZONY - headless kontra visual, oba warianty
  2. tempo rysowania - visual 1:1 kontra visual 1:3 (slow motion pomija
     klatki, nie skraca kroku)
  3. kamera i overlay - kamera sledzaca + overlay kontra kamera nieruchoma,
     overlay off, UI off, cienie i GTAO off

    python tools/jozz_wheel_bench/check_visual_equivalence.py [--out <katalog>]
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
BENCH = HERE / "wheel_bench.exe"
SAMPLES = REPO / "build" / "bin" / "Debug" / "samples.exe"
STEPS = 600
SAMPLE_NAME = "Wheel Scope"


def run_headless(out: Path, variant: str, steps: int) -> None:
    subprocess.run([str(BENCH), "--rig-trace", str(out), "--rig-trace-variant", variant,
                    "--rig-trace-steps", str(steps)], check=True, capture_output=True)


def run_visual(out: Path, variant: str, steps: int, divider: int = 1, plain: bool = False) -> None:
    env = dict(os.environ)
    env["JOZZ_RIG_TRACE"] = str(out)
    env["JOZZ_RIG_VARIANT"] = variant
    env["JOZZ_RIG_TRACE_STEPS"] = str(steps)
    env["JOZZ_RIG_TRACE_DIVIDER"] = str(divider)
    if plain:
        env["JOZZ_RIG_TRACE_VIEW_PLAIN"] = "1"
    # Zapas klatek: przy dzielniku N jeden krok fizyki potrzebuje N klatek.
    frames = steps * divider + 60
    subprocess.run([str(SAMPLES), "--sample-name", SAMPLE_NAME, "--frames", str(frames)],
                   check=True, capture_output=True, env=env, cwd=str(REPO))


def body(path: Path) -> bytes:
    """Odcisk bez linii naglowkowej `#`, ktora niesie tylko opis przebiegu."""
    return b"".join(l for l in path.read_bytes().splitlines(keepends=True)
                    if not l.startswith(b"#"))


def compare(label: str, a: Path, b: Path, failures: list[str]) -> None:
    if not a.exists() or not b.exists():
        failures.append(f"{label}: brak pliku ({a.name} / {b.name})")
        print(f"  BLAD  {label}: brak pliku")
        return
    x, y = body(a), body(b)
    if x == y:
        n = x.count(b"\n")
        print(f"  OK    {label}: {n} krokow IDENTYCZNYCH bajtowo")
        return
    xl, yl = x.splitlines(), y.splitlines()
    first = next((i for i, (p, q) in enumerate(zip(xl, yl)) if p != q), min(len(xl), len(yl)))
    failures.append(f"{label}: pierwsza roznica w kroku {first + 1}")
    print(f"  BLAD  {label}: pierwsza roznica w kroku {first + 1} "
          f"({len(xl)} vs {len(yl)} linii)")
    for src, line in (("headless/A", xl[first:first + 1]), ("visual/B", yl[first:first + 1])):
        if line:
            print(f"        {src}: {line[0].decode(errors='replace')}")


def main(out: Path) -> int:
    for exe in (BENCH, SAMPLES):
        if not exe.exists():
            print(f"BRAK {exe} - zbuduj najpierw stend i target samples", file=sys.stderr)
            return 2
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)

    failures: list[str] = []

    print("1) renderer wylaczony (headless) kontra wlaczony (visual)")
    for variant in ("sphere", "prism-Nmax"):
        h, v = out / f"headless_{variant}.csv", out / f"visual_{variant}.csv"
        run_headless(h, variant, STEPS)
        run_visual(v, variant, STEPS)
        compare(f"{variant:11} headless == visual", h, v, failures)

    print("2) tempo rysowania: visual 1:1 kontra visual 1:3")
    slow = out / "visual_sphere_div3.csv"
    run_visual(slow, "sphere", 200, divider=3)
    ref200 = out / "headless_sphere_200.csv"
    run_headless(ref200, "sphere", 200)
    compare("sphere      1:1 == 1:3", ref200, slow, failures)

    print("3) kamera i overlay: sledzaca+overlay kontra nieruchoma+bez overlaya")
    plain = out / "visual_sphere_plain.csv"
    run_visual(plain, "sphere", 200, plain=True)
    compare("sphere      pelny widok == goly widok", ref200, plain, failures)

    print()
    if failures:
        print(f"EQUIVALENCE FAILED: {len(failures)} roznic")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("EQUIVALENCE OK: headless i visual to ta sama symulacja, bajt w bajt.")
    print("UWAGA: dowodzi wspolnoty fizyki i niezaleznosci od renderera.")
    print("       NIE jest to ocena, czy ruch wyglada jak wlasciwe kolo.")
    return 0


if __name__ == "__main__":
    args = sys.argv[1:]
    target = HERE / "equivalence_out"
    if "--out" in args:
        target = Path(args[args.index("--out") + 1])
    sys.exit(main(target.resolve()))
