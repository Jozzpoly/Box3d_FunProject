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
STEPS = 600
SAMPLE_NAME = "Wheel Scope"

# Kazda binarka ma WLASNA liste zrodel. Wspolna lista wygladala oszczedniej, ale
# kazala przebudowac stend po zmianie w pliku sample'a, ktorego stend nie
# kompiluje - a bramka zadajaca pracy bez powodu uczy przebudowywac odruchowo,
# czyli dokladnie odwrotnie, niz powinna.
RIG = (HERE / "jozz_wheel_rig.c", HERE / "jozz_wheel_rig.h")
BENCH_SOURCES = RIG + (HERE / "wheel_bench.c",)
SAMPLES_SOURCES = RIG + (REPO / "samples" / "sample_jozz_wheel_scope.cpp",
                         REPO / "samples" / "sample.cpp", REPO / "samples" / "sample.h")


def pick_samples() -> Path | None:
    """Najnowsza istniejaca binarka samples.

    Sciezka byla wczesniej przypieta na sztywno do `Debug`. Skutek zlapany
    2026-07-30: przy pracy na buildzie Release test przez cala sesje porownywal
    stend ze SKOMPILOWANYM POPRZEDNIEGO DNIA oknem i swiecil na zielono, nie
    dotykajac ani razu zmienionego kodu. Bramka, ktora testuje nieaktualna
    binarke, jest gorsza od braku bramki, bo daje falszywa pewnosc.
    """
    found = [p for p in (REPO / "build" / "bin" / c / "samples.exe" for c in ("Release", "Debug"))
             if p.exists()]
    return max(found, key=lambda p: p.stat().st_mtime) if found else None


def check_freshness(samples: Path) -> list[str]:
    stale = []
    for binary, sources in ((samples, SAMPLES_SOURCES), (BENCH, BENCH_SOURCES)):
        if not binary.exists():
            continue
        newer = [s.name for s in sources if s.exists() and s.stat().st_mtime > binary.stat().st_mtime]
        if newer:
            stale.append(f"{binary.relative_to(REPO)} starsza niz: {', '.join(newer)}")
    return stale


def run_headless(out: Path, variant: str | None, steps: int, config: Path | None = None) -> None:
    cmd = [str(BENCH), "--rig-trace", str(out), "--rig-trace-steps", str(steps)]
    cmd += ["--rig-config", str(config)] if config else ["--rig-trace-variant", variant or "sphere"]
    subprocess.run(cmd, check=True, capture_output=True)


def run_visual(out: Path, variant: str | None, steps: int, divider: int = 1, plain: bool = False,
               config: Path | None = None) -> None:
    env = dict(os.environ)
    # Sweep-owe nadpisania z otoczenia zmienilyby konfiguracje pod testem.
    for k in ("JOZZ_RIG_SIDES", "JOZZ_RIG_START_V", "JOZZ_RIG_RECORD", "JOZZ_RIG_CONFIG"):
        env.pop(k, None)
    env["JOZZ_RIG_TRACE"] = str(out)
    env["JOZZ_RIG_TRACE_STEPS"] = str(steps)
    env["JOZZ_RIG_TRACE_DIVIDER"] = str(divider)
    if config:
        env["JOZZ_RIG_CONFIG"] = str(config)
    else:
        env["JOZZ_RIG_VARIANT"] = variant or "sphere"
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
    global SAMPLES
    SAMPLES = pick_samples()
    if SAMPLES is None or not BENCH.exists():
        print("BRAK binarki - zbuduj stend i target samples", file=sys.stderr)
        return 2

    stale = check_freshness(SAMPLES)
    if stale:
        print("BLAD: binarka starsza niz zrodla - test dotyczylby nieaktualnego kodu:",
              file=sys.stderr)
        for s in stale:
            print(f"  - {s}", file=sys.stderr)
        return 2
    print(f"binarka okna: {SAMPLES.relative_to(REPO)}")

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

    print("4) konstrukcja z polki: ten sam PLIK w oknie i w stendzie")
    # Plik pisany tutaj RECZNIE, nie wygenerowany przez stend. To celowe: gdyby
    # test uzywal pliku zapisanego przez to samo narzedzie, ktore go potem czyta,
    # dowodzilby tylko wewnetrznej zgodnosci. Recznie napisany plik sprawdza, czy
    # format jest naprawde otwarty - czyli czy agent albo Owner moga go stworzyc.
    # Plik jest NIEPELNY (brak wiekszosci kluczy) i NIEDOMYSLNY (inny wariant,
    # inne N, inna predkosc, inne obciazenie) - obie te cechy sa czescia testu.
    rig = out / "z_polki.rig"
    rig.write_text(
        "format 1\n"
        "# test: konstrukcja napisana recznie, celowo niepelna i niedomyslna\n"
        "variant prism-Nmax\n"
        "prism_sides 17\n"
        "start_speed 3\n"
        "target_speed 3\n"
        "load_n 1200\n",
        encoding="ascii")
    hc, vc = out / "headless_konstrukcja.csv", out / "visual_konstrukcja.csv"
    run_headless(hc, None, 300, config=rig)
    run_visual(vc, None, 300, config=rig)
    compare("konstrukcja headless == visual", hc, vc, failures)
    # Bez tego test przeszedlby take wtedy, gdyby OBA frontendy zignorowaly plik
    # i uruchomily konfiguracje domyslna - identycznie i bezuzytecznie.
    ref_default = out / "headless_sphere.csv"
    if ref_default.exists() and body(hc)[:200] == body(ref_default)[:200]:
        failures.append("konstrukcja: plik zostal ZIGNOROWANY (przebieg jak domyslny)")
        print("  BLAD  konstrukcja: plik zostal zignorowany - przebieg jak domyslny")
    else:
        print("  OK    konstrukcja: plik naprawde zmienil przebieg (rozny od domyslnego)")

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
