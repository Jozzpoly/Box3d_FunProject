#!/usr/bin/env python3
"""Walidator eksperymentu Q2A (staly dystans, utrzymywana predkosc).

Sprawdza spojnosc pakietu i weryfikuje hashe wobec plikow na dysku. NIE orzeka,
ktory wariant jest lepszy - to nie narzedzie do wnioskowania.

    python tools/jozz_wheel_bench/validate_q2a.py <katalog_q2a> [--exe <wheel_bench.exe>]
"""

from __future__ import annotations

import csv
import hashlib
import json
import math
import sys
from pathlib import Path

REQUIRED_SAMPLE_COLS = [
    "variant", "rep", "phase", "step", "time_s", "distance_m", "target_speed_m_s",
    "actual_speed_m_s", "speed_error_m_s", "drive_force_N", "drive_power_W",
    "controller_saturated", "omega_spin_rad_s", "reference_rim_speed_m_s",
    "reference_slip_speed_m_s", "cumulative_revolutions", "position_y_m",
    "velocity_y_m_s", "kinetic_translation_J", "kinetic_rotation_J",
    "kinetic_total_J", "potential_gravity_J",
]

LOCKED = ("target_speed_m_s",)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(root: Path, exe: Path | None) -> int:
    fail: list[str] = []
    warn: list[str] = []

    man_path, s_path, u_path = (root / "q2a_manifest.json", root / "q2a_samples.csv",
                               root / "q2a_summary.csv")
    for p in (man_path, s_path, u_path):
        if not p.exists():
            print(f"  BLAD brak {p.name}")
            return 1
    man = json.loads(man_path.read_text(encoding="utf-8"))
    samples = list(csv.DictReader(s_path.open(encoding="utf-8")))
    summary = list(csv.DictReader(u_path.open(encoding="utf-8")))

    # --- provenance: hashe musza opisywac pliki, ktore sa na dysku
    prov = man["provenance"]
    # Zrodlo lezy obok TEGO pliku, nie obok katalogu wynikow - katalog wynikow
    # jest w scratchu i nie zawiera zrodel.
    src = Path(__file__).resolve().parent / "wheel_bench.c"
    for label, path, key in (("source", src, "source_sha256"),):
        if path.exists():
            got = sha256(path)
            if got != prov[key]:
                warn.append(f"{label}: sha w manifescie {prov[key][:16]} != plik na dysku {got[:16]} "
                            f"(zrodlo moglo sie zmienic po przebiegu)")
        else:
            warn.append(f"{label}: {path} nie istnieje - nie moge zweryfikowac sha")
    if exe is not None:
        if not exe.exists():
            fail.append(f"exe: {exe} nie istnieje")
        else:
            got = sha256(exe)
            if got != prov["exe_sha256"]:
                fail.append(f"exe: sha w manifescie {prov['exe_sha256'][:16]} != plik {got[:16]}")
    else:
        warn.append("exe: nie podano --exe, sha pliku wykonywalnego niesprawdzone")
    for key in ("git_sha", "git_dirty", "command_line", "box3d_lib_sha256"):
        if not prov.get(key):
            fail.append(f"provenance: brak {key}")
    if man.get("registered_in_raw_manifest") is not False:
        fail.append("manifest: registered_in_raw_manifest musi byc false w tej fazie")

    # --- kolumny probek
    missing = [c for c in REQUIRED_SAMPLE_COLS if c not in (samples[0] if samples else {})]
    if missing:
        fail.append(f"probki: brakujace kolumny {missing}")

    # --- brak NaN/Inf, monotonicznosc czasu i dystansu w kazdym przebiegu
    groups: dict[tuple[str, str, str], list[dict]] = {}
    for r in samples:
        groups.setdefault((r["variant"], r["rep"], r["phase"]), []).append(r)
    for key, rows in sorted(groups.items()):
        tag = "/".join(key)
        for r in rows:
            for c in REQUIRED_SAMPLE_COLS[3:]:
                v = float(r[c])
                if not math.isfinite(v):
                    fail.append(f"{tag}: {c} = {r[c]}")
        for c in ("time_s", "distance_m"):
            vals = [float(r[c]) for r in rows]
            if any(b < a - 1e-12 for a, b in zip(vals, vals[1:])):
                fail.append(f"{tag}: {c} nie jest monotoniczne")

    # --- ta sama konfiguracja dla obu wariantow
    ctrl, phys = man["controller"], man["physics"]
    variants = {r["variant"] for r in summary}
    if variants != {"sphere", "prism-Nmax"}:
        fail.append(f"warianty {sorted(variants)} != ['prism-Nmax', 'sphere']")
    for col in LOCKED:
        vals = {r[col] for r in samples}
        if len(vals) != 1:
            fail.append(f"{col} rozni sie miedzy probkami: {sorted(vals)}")
    if abs(float(next(iter({r['target_speed_m_s'] for r in samples})))
           - ctrl["target_speed_m_s"]) > 1e-9:
        fail.append("target w probkach != target w manifescie")
    if abs(phys["effective_normal_load_N"]
           - (phys["external_downforce_N"] + phys["gravity_load_N"])) > 1e-6:
        fail.append("obciazenie: efektywne != docisk + grawitacja")
    if phys["rolling_resistance_each_material"] != 0.0:
        fail.append("rollingResistance musi byc jawnie 0")

    # --- statusy i bramki
    for r in summary:
        tag = f"{r['variant']}/rep{r['rep']}"
        if r["status"] not in ("qualified", "failed_to_qualify"):
            fail.append(f"{tag}: nieznany status {r['status']!r}")
        if r["status"] == "failed_to_qualify":
            if r["gate_mean_err"] != "n/a":
                fail.append(f"{tag}: bramki musza byc n/a przy failed_to_qualify")
            if int(r["measure_steps"]) != 0:
                fail.append(f"{tag}: failed_to_qualify z niezerowym oknem")
        else:
            d = float(r["distance_m"])
            if d < man["qualification"]["measure_distance_m"] - 1e-9:
                fail.append(f"{tag}: dystans {d} < zadeklarowany")

    # --- SPOJNOSC CALKOWANIA W PIONIE: to nie ozdoba, to warunek sensownosci
    #     pionowych czlonow bilansu pracy.
    for r in summary:
        if r["status"] != "qualified":
            continue
        vyi, dy = float(r["vy_integral_m"]), float(r["delta_y_m"])
        if abs(vyi - dy) > 1e-3:
            warn.append(
                f"{r['variant']}/rep{r['rep']}: calka v_y*dt = {vyi:+.4f} m, a delta_y = {dy:+.2e} m "
                f"-> pionowe czlony (W_downforce, W_gravity) i energy_residual SA BEZ WARTOSCI "
                f"w tym buildzie; praca napedu liczona z v_x pozostaje wazna")

    # --- determinizm powtorzen
    for variant in sorted(variants):
        reps = [r for r in summary if r["variant"] == variant and r["stage"] == "B_locked"]
        if len(reps) < 3:
            fail.append(f"{variant}: {len(reps)} powtorzen etapu B, wymagane >= 3")
        keys = ["status", "measure_steps", "distance_m", "err_rms_m_s", "W_drive_signed_J"]
        distinct = {tuple(r[k] for k in keys) for r in reps}
        if len(distinct) > 1:
            warn.append(f"{variant}: powtorzenia NIE sa identyczne, rozrzut: {sorted(distinct)}")

    for w in warn:
        print(f"  OSTRZEZENIE {w}")
    for f in fail:
        print(f"  BLAD {f}")
    if fail:
        print(f"\nQ2A INVALID: {len(fail)} bledow, {len(warn)} ostrzezen")
        return 1
    print(f"\nQ2A OK: {len(samples)} probek, {len(summary)} przebiegow, {len(warn)} ostrzezen")
    print("UWAGA: sprawdzona spojnosc pakietu i provenance. NIE jest to ocena eksperymentu")
    print("       ani orzeczenie, ktory wariant jest lepszy.")
    return 0


if __name__ == "__main__":
    args = sys.argv[1:]
    exe = None
    if "--exe" in args:
        i = args.index("--exe")
        exe = Path(args[i + 1])
        del args[i:i + 2]
    if len(args) != 1:
        print(f"uzycie: python {Path(__file__).name} <katalog_q2a> [--exe <wheel_bench.exe>]",
              file=sys.stderr)
        sys.exit(2)
    sys.exit(main(Path(args[0]), exe))
