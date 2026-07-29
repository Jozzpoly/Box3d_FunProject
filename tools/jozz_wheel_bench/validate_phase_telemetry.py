#!/usr/bin/env python3
"""Walidator telemetrii granic faz (JP-02).

Sprawdza KSZTALT i wewnetrzna spojnosc pliku telemetrii. NIE orzeka, czy
eksperyment jest sensowny ani czy ktorykolwiek wariant jest lepszy - to nie
jest narzedzie do wnioskowania.

    python tools/jozz_wheel_bench/validate_phase_telemetry.py <plik.csv>
"""

from __future__ import annotations

import csv
import math
import sys
from pathlib import Path

WHEEL_R = 0.5141
V0 = 13.0
ENVELOPES = ["sphere", "cylinder-32", "phased union-4", "prism-Nmax", "tire profile"]
LOAD_CASES = ["corner_1900N", "v1_432N"]
BOUNDARIES = ["INITIAL", "MEASURE_START", "MEASURE_END"]
EXPECTED_STEP = {"INITIAL": 0, "MEASURE_START": 120, "MEASURE_END": 360}
EXPECTED_TIME = {"INITIAL": 0.0, "MEASURE_START": 2.0, "MEASURE_END": 6.0}

NUMERIC = [
    "time_s", "position_x", "position_y", "position_z",
    "linear_velocity_x", "linear_velocity_y", "linear_velocity_z",
    "angular_velocity_x", "angular_velocity_y", "angular_velocity_z",
    "wheel_axle_world_x", "wheel_axle_world_y", "wheel_axle_world_z",
    "omega_spin_rad_s", "rim_surface_speed_m_s", "longitudinal_slip_speed_m_s",
    "translational_kinetic_energy_J", "rotational_kinetic_energy_J", "total_kinetic_energy_J",
    "cumulative_x_displacement_m", "cumulative_path_length_m",
    "cumulative_spin_angle_rad", "cumulative_revolutions",
    "nominal_load_N", "external_downforce_N", "gravity_load_N", "effective_static_load_N",
    "dt_s",
]


def main(path: Path) -> int:
    fail: list[str] = []
    rows = list(csv.DictReader(path.open(encoding="utf-8")))

    # 1 + 10: dokladnie 5 wariantow x 2 obciazenia x 3 granice, nic wiecej.
    # Nadmiarowy rekord oznaczalby telemetrie z sekcji D albo F.
    if len(rows) != 30:
        fail.append(f"1/10: {len(rows)} rekordow, oczekiwano 30 (nadmiar = telemetria z D albo F)")
    got_env, got_load = {r["envelope"] for r in rows}, {r["load_case"] for r in rows}
    if got_env != set(ENVELOPES):
        fail.append(f"1: warianty {sorted(got_env)} != {ENVELOPES}")
    if got_load != set(LOAD_CASES):
        fail.append(f"10: load_case {sorted(got_load)} != {LOAD_CASES}")

    # 2: klucz unikalny
    keys = [(r["load_case"], r["envelope"], r["boundary"]) for r in rows]
    if len(set(keys)) != len(keys):
        dup = [k for k in set(keys) if keys.count(k) > 1]
        fail.append(f"2: zduplikowane klucze {dup}")

    for r in rows:
        tag = f"{r['load_case']}/{r['envelope']}/{r['boundary']}"

        # 3 + 4: kroki i czasy
        if r["boundary"] not in BOUNDARIES:
            fail.append(f"3: {tag}: nieznana granica")
            continue
        if int(r["global_step"]) != EXPECTED_STEP[r["boundary"]]:
            fail.append(f"3: {tag}: krok {r['global_step']} != {EXPECTED_STEP[r['boundary']]}")
        if abs(float(r["time_s"]) - EXPECTED_TIME[r["boundary"]]) > 1e-9:
            fail.append(f"4: {tag}: czas {r['time_s']} != {EXPECTED_TIME[r['boundary']]}")

        # 5: wszystko skonczone
        for col in NUMERIC:
            v = float(r[col])
            if not math.isfinite(v):
                fail.append(f"5: {tag}: {col} = {r[col]}")

        # 6: energia nieujemna poza tolerancja numeryczna
        for col in ("translational_kinetic_energy_J", "rotational_kinetic_energy_J",
                    "total_kinetic_energy_J"):
            if float(r[col]) < -1e-9:
                fail.append(f"6: {tag}: {col} = {r[col]} < 0")

        # 8: warunki poczatkowe zgodne z zadanymi.
        # Tolerancja 1e-4, nie 0: `cylinder-32` wchodzi w okno z vx = 12.9999981,
        # bo srodek masy jego hulla nie lezy dokladnie w origin ciala we float32,
        # a `b3Body_SetMassData` (src/body.c:1834-1842) przesuwa srodek masy BEZ
        # korekty predkosci liniowej. To wlasnosc istniejacego stendu, nie
        # instrumentacji - JP-02 ja REJESTRUJE, nie naprawia. Kazda odchylka jest
        # drukowana ponizej, wiec zluzowana tolerancja niczego nie chowa.
        if r["boundary"] == "INITIAL":
            if abs(float(r["linear_velocity_x"]) - V0) > 1e-4:
                fail.append(f"8: {tag}: vx = {r['linear_velocity_x']}, zadane {V0}")
            if abs(float(r["omega_spin_rad_s"]) + V0 / WHEEL_R) > 1e-3:
                fail.append(f"8: {tag}: omega_spin = {r['omega_spin_rad_s']}, zadane {-V0/WHEEL_R:.5f}")
            if abs(float(r["longitudinal_slip_speed_m_s"])) > 1e-3:
                fail.append(f"8: {tag}: slip = {r['longitudinal_slip_speed_m_s']}, oczekiwano ~0")

    # 7: droga i bezwzgledna liczba obrotow nie cofaja sie miedzy granicami
    for lc in LOAD_CASES:
        for env in ENVELOPES:
            seq = [next((r for r in rows if (r["load_case"], r["envelope"], r["boundary"]) == (lc, env, b)), None)
                   for b in BOUNDARIES]
            if any(s is None for s in seq):
                fail.append(f"7: brak kompletu granic dla {lc}/{env}")
                continue
            for col in ("cumulative_path_length_m", "cumulative_revolutions"):
                vals = [float(s[col]) for s in seq]
                if not (vals[0] <= vals[1] + 1e-9 <= vals[2] + 1e-9):
                    fail.append(f"7: {lc}/{env}: {col} cofa sie: {vals}")

            # 9: MEASURE_START jest stanem po dokladnie 120 krokach, a okno pomiarowe
            #    trwa dokladnie 240 krokow. Kontrola strukturalna - nie dowodzi sama
            #    z siebie, ze petla pomiarowa czyta ten sam obiekt (to gwarantuje kod).
            d = int(seq[2]["global_step"]) - int(seq[1]["global_step"])
            if d != 240:
                fail.append(f"9: {lc}/{env}: okno pomiarowe {d} krokow, oczekiwano 240")

    # Jawny raport odchylek warunku poczatkowego - zeby tolerancja z punktu 8
    # nie ukrywala niczego przed czytajacym.
    dev = [(f"{r['load_case']}/{r['envelope']}", float(r["linear_velocity_x"]) - V0)
           for r in rows if r["boundary"] == "INITIAL"]
    nonzero = [(k, d) for k, d in dev if d != 0.0]
    if nonzero:
        print("  OBSERWACJA odchylka vx w INITIAL od zadanych 13.0 m/s:")
        for k, d in nonzero:
            print(f"    {k:30} {d:+.3e} m/s")
    else:
        print("  OBSERWACJA wszystkie warianty startuja z dokladnie 13.0 m/s")

    for f in fail:
        print(f"  BLAD {f}")
    if fail:
        print(f"\nTELEMETRY INVALID: {len(fail)} bledow")
        return 1
    print(f"TELEMETRY OK: {len(rows)} rekordow, {len(set(keys))} unikalnych kluczy")
    print("UWAGA: sprawdzony ksztalt i spojnosc pliku. NIE jest to ocena eksperymentu.")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"uzycie: python {Path(__file__).name} <plik.csv>", file=sys.stderr)
        sys.exit(2)
    sys.exit(main(Path(sys.argv[1])))
