#!/usr/bin/env python3
"""Blokada zachowania rigu: czy przebieg domyslnej konfiguracji jest DALEJ ten sam.

Pytanie, na ktore odpowiada: czy zmiana w kodzie rigu, w box3d albo w toolchainie
przesunela trajektorie kola. Nie "czy testy przechodza" - czy TA SAMA konfiguracja
daje TEN SAM przebieg, krok po kroku, bajt w bajt.

Dlaczego akurat tak:

Projekt mial dotad wylacznie bramki SPOJNOSCI (czy dane same ze soba sie zgadzaja)
i bramke EKWIWALENCJI (czy okno pokazuje to samo co stend). Zadna z nich nie potrafi
zauwazyc, ze fizyka zmienila sie w OBU miejscach naraz - a wlasnie taka zmiana jest
najgrozniejsza, bo wyglada jak zdrowy zielony wynik. Ta bramka jest jedynym miejscem,
gdzie zapisane jest, jak rig ma sie zachowywac.

Wzorzec odniesienia (`golden/`) zostal wygenerowany i sprawdzony wobec stendu
zbudowanego ze zrodel z commita `f9576c3` - jest wiec zapisem ZAAKCEPTOWANEGO
zachowania, a nie zdjeciem przypadkowego stanu drzewa roboczego.

Czerwony wynik NIE znaczy "zepsute". Znaczy: zmienil sie eksperyment. Jesli zmiana
byla zamierzona, wzorzec aktualizuje sie SWIADOMIE (--update) i ta aktualizacja jest
osobna decyzja, widoczna w diffie.

    python tools/jozz_wheel_bench/check_behaviour_lock.py
    python tools/jozz_wheel_bench/check_behaviour_lock.py --update   # po SWIADOMEJ zmianie
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
BENCH = HERE / "wheel_bench.exe"
GOLDEN = HERE / "golden"
VARIANTS = ("sphere", "prism-Nmax")
STEPS = 600

# Zrodla, ktore moga zmienic przebieg. Jesli ktorekolwiek jest nowsze od binarki,
# porownanie dotyczy nieaktualnego kodu - czyli jest cicha nieprawda.
SOURCES = (HERE / "jozz_wheel_rig.c", HERE / "jozz_wheel_rig.h", HERE / "wheel_bench.c")


def fail(msg: str) -> int:
    print(f"BLAD: {msg}")
    return 1


def check_freshness() -> list[str]:
    if not BENCH.exists():
        return [f"brak {BENCH.name} - zbuduj: tools/jozz_wheel_bench/build.bat"]
    stale = [s.name for s in SOURCES if s.exists() and s.stat().st_mtime > BENCH.stat().st_mtime]
    if stale:
        return [f"{', '.join(stale)} nowsze niz wheel_bench.exe - przebuduj przed sprawdzeniem"]
    return []


def trace(out_dir: Path, variant: str) -> Path:
    out = out_dir / f"rig_trace_{variant}.csv"
    subprocess.run([str(BENCH), "--rig-trace", str(out), "--rig-trace-variant", variant,
                    "--rig-trace-steps", str(STEPS)], check=True, capture_output=True)
    return out


def split(path: Path) -> tuple[list[str], list[str]]:
    """Naglowek (linie `#` i wiersz kolumn) osobno od trajektorii.

    Rozdzial jest istotny: zmiana OPISU przebiegu i zmiana samego przebiegu to
    dwa zupelnie rozne zdarzenia. Wspolne porownanie zglaszalo je tak samo -
    `rozna liczba linii` - czyli najlagodniejsza mozliwa zmiana wygladala jak
    najgrozniejsza, dokladnie wtedy, gdy czytajacy jest zaniepokojony.
    """
    lines = path.read_text(encoding="ascii", errors="replace").splitlines()
    is_head = lambda l: l.startswith("#") or l.startswith("step,")
    return [l for l in lines if is_head(l)], [l for l in lines if not is_head(l)]


def first_diff(a: list[str], b: list[str], label: str) -> str | None:
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return (f"{label} - pierwsza roznica w pozycji {i + 1}\n"
                    f"      wzorzec: {x[:150]}\n"
                    f"      teraz  : {y[:150]}")
    if len(a) != len(b):
        return f"{label} - rozna liczba linii: wzorzec {len(a)}, teraz {len(b)}"
    return None


def compare(golden: Path, fresh: Path) -> tuple[bool, str]:
    """Zwraca (zgodne, opis). Opis nazywa RODZAJ zmiany, nie tylko jej fakt."""
    if golden.read_bytes() == fresh.read_bytes():
        return True, ""

    gh, gb = split(golden)
    fh, fb = split(fresh)

    body = first_diff(gb, fb, f"TRAJEKTORIA (krok {'?'})")
    if body:
        # Numer kroku jest wprost w pierwszej kolumnie, wiec nie zgadujemy.
        for i, (x, y) in enumerate(zip(gb, fb)):
            if x != y:
                body = body.replace("krok ?", f"krok {x.split(',')[0]}")
                break
        return False, "FIZYKA SIE ZMIENILA. " + body

    head = first_diff(gh, fh, "KONTRAKT (naglowek)")
    if head:
        return False, ("tylko OPIS przebiegu, trajektoria bez zmian co do bitu.\n      " + head)

    return False, "pliki roznia sie bajtowo, ale nie liniowo (koniec linii?)"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--update", action="store_true",
                    help="nadpisz wzorzec biezacym zachowaniem (SWIADOMA zmiana eksperymentu)")
    args = ap.parse_args()

    problems = check_freshness()
    if problems:
        return fail(problems[0])

    GOLDEN.mkdir(exist_ok=True)
    tmp = Path(tempfile.mkdtemp(prefix="jozz_lock_"))
    try:
        bad = 0
        physics = 0
        for variant in VARIANTS:
            fresh = trace(tmp, variant)
            gold = GOLDEN / f"rig_trace_{variant}.csv"

            if args.update:
                shutil.copyfile(fresh, gold)
                print(f"  ZAKTUALIZOWANO  {variant:<11} -> {gold.relative_to(REPO)}")
                continue

            if not gold.exists():
                print(f"  BRAK WZORCA     {variant:<11} ({gold.relative_to(REPO)})")
                bad += 1
                continue

            same, detail = compare(gold, fresh)
            if same:
                print(f"  OK              {variant:<11} {STEPS} krokow IDENTYCZNYCH bajtowo")
            else:
                print(f"  ZMIANA          {variant:<11} {detail}")
                bad += 1
                physics += detail.startswith("FIZYKA")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print()
    if args.update:
        print("WZORZEC ZAKTUALIZOWANY. Sprawdz diff - to jest zmiana eksperymentu,")
        print("a nie zmiana testu. Powinna byc opisana w commicie.")
        return 0
    if bad:
        if physics:
            print("BLOKADA ZACHOWANIA NARUSZONA: domyslna konfiguracja daje inny PRZEBIEG.")
            print("To jest zmiana fizyki. Zanim siegniesz po --update, ustal przyczyne:")
            print("wzorzec pochodzi z zaakceptowanego zachowania, wiec rozjazd znaczy, ze")
            print("eksperyment jest inny niz ten, ktory opisuja dokumenty.")
        else:
            print("BLOKADA ZACHOWANIA NARUSZONA: zmienil sie OPIS przebiegu.")
            print("Trajektoria jest identyczna co do bitu - fizyka stoi w miejscu.")
            print("Jesli nowy naglowek jest zamierzony, --update i opisz go w commicie.")
        return 1

    print("BLOKADA ZACHOWANIA OK: domyslna konfiguracja daje ten sam przebieg co wzorzec.")
    print("UWAGA: dowodzi wylacznie NIEZMIENNOSCI wzgledem wzorca. NIE dowodzi, ze")
    print("       przebieg jest fizycznie poprawny ani ze zbiega przy mniejszym dt.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
