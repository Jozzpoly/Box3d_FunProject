#!/usr/bin/env python3
"""Jedno wejscie do wszystkich bramek programu kol.

Powod istnienia: bramki byly rozsypane po katalogu i trzeba bylo WIEDZIEC, ktore
istnieja i w jakiej kolejnosci maja sens. Wiedza plemienna nie jest bramka -
znika razem z sesja. Tutaj jest lista, ktora sie nie chowa.

Kolejnosc nie jest przypadkowa: od najtanszej i najbardziej podstawowej do
najdrozszej. Pierwsza czerwona bramka zwykle tlumaczy wszystkie nastepne.

  1. format konstrukcji  - czy plik .rig opisuje CALA tozsamosc przebiegu
  2. blokada zachowania  - czy rig daje TEN SAM przebieg co wzorzec
  3. ekwiwalencja        - czy okno pokazuje TE SAMA symulacje co stend
                           (w tym: czy ten sam PLIK konstrukcji daje po obu
                           stronach identyczny przebieg)
  4. integralnosc danych - czy findings i raw sa spojne

Czego ten skrypt NIE robi: nie buduje. Budowanie jest swiadoma czynnoscia i
bramka ma sprawdzac to, co faktycznie lezy na dysku. Bramka 1 sama odmowi
pracy, jesli binarka jest starsza od zrodel.

    python tools/jozz_wheel_bench/check_all.py
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent

GATES = [
    ("format konstrukcji", [str(HERE / "wheel_bench.exe"), "--rig-config-check"]),
    ("blokada zachowania rigu", [sys.executable, str(HERE / "check_behaviour_lock.py")]),
    ("ekwiwalencja visual/headless", [sys.executable, str(HERE / "check_visual_equivalence.py")]),
    ("integralnosc findings i raw", [sys.executable, str(REPO / "tools" / "evidence" / "evidence.py"), "check"]),
]


def main() -> int:
    results = []
    for name, cmd in GATES:
        print(f"\n===== {name} " + "=" * max(0, 52 - len(name)))
        rc = subprocess.run(cmd, cwd=str(REPO)).returncode
        results.append((name, rc))

    print("\n" + "=" * 60)
    worst = 0
    for name, rc in results:
        print(f"  {'OK    ' if rc == 0 else 'BLAD  '} {name}")
        worst = max(worst, rc)

    if worst == 0:
        print("\nWSZYSTKIE BRAMKI ZIELONE.")
        print("UWAGA: to znaczy `nic sie nie zmienilo i dane sa spojne`.")
        print("       NIE znaczy `fizyka jest poprawna`. Tego zadna z tych")
        print("       bramek nie sprawdza i sprawdzic nie moze.")
    else:
        print("\nCO NAJMNIEJ JEDNA BRAMKA CZERWONA - patrz wyzej.")
    return worst


if __name__ == "__main__":
    sys.exit(main())
