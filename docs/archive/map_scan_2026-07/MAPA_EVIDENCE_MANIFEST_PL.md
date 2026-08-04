> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# Mapa — manifest dowodów

Status: aktywny rejestr. Najnowsze wpisy u góry.
Źródło statusu produktu: `PLAN_PRZEBUDOWA_MAPY_2026_07_11_PL.md`.

Ten plik przechowuje odtwarzalne dowody WP. Obraz bez komendy/kamery/hasza ma
status `SESSION_ONLY` i nie zamyka bramki.

## RECOVERY-2026-07-24 — odzysk WIP + zapis na main (WP-00 zamknięty)

```text
evidence_id:   RECOVERY-2026-07-24
wp_id:         WP-00 CLOSED (decyzja Jozza „zapisujemy aktualny stan na maina")
git_head:      main <- fast-forward z jozz-scan-terrain-f0
worktree:      CLEAN po commicie
product_status: E2R central campus ZINTEGROWANY (bez toru E3); tor E3 = pliki uśpione na dysku
snapshot_wip:  56c04c1 @ origin/jozz-map-wip-snapshot-2026-07-24
gate:          build 3/3 OK; walidator 18 sond + 2 sondy mapy OK; test PASS; boot-smoke 0 err
detail:        docs/ODZYSK_UTRACONYCH_ZMIAN_2026_07_24_PL.md
```

## AUDIT-2026-07-13 — wejściowy eksperyment E2R/E3

```text
evidence_id: AUDIT-2026-07-13
wp_id: PRE-WP-00
git_head: 445db88
worktree: DIRTY; E2R/E3 mixed; dokładny snapshot jeszcze nie istnieje
product_status: E2R RECOVERY_REQUIRED; current E3 REJECTED_EXPERIMENT
limitation: brak interaktywnego feel-testu Jozza
```

### Build i test po wymuszonym clean

Standardowy gate początkowo zwrócił zielono ze starym obiektem validatora.
Później raportował `build test: OK`, mimo że po clean brakowało `test.exe`.
Dlatego końcowy dowód uzyskano jawnie:

```powershell
cmd /c "set PATH=& cmake --build --preset windows-debug --target clean"
cmd /c "set PATH=& cmake --build --preset windows-debug --target samples jozz_vehicle_validation test"
.\build\bin\Debug\jozz_vehicle_validation.exe
.\build\bin\Debug\test.exe
.\build\bin\Debug\samples.exe --sample-name "M5 First Drivable" --frames 300
.\build\bin\Debug\samples.exe --sample-name "M6 Suspension Rig Lab" --frames 300
```

Wynik:

- `samples.exe` 2026-07-13 03:16:26 — build OK;
- `jozz_vehicle_validation.exe` 03:16:43 — exit 0, validator OK;
- `test.exe` 03:16:45 — `All Box3D tests passed`;
- M5 300 klatek — 0 błędów sokol;
- M6 300 klatek — 0 błędów sokol;
- validator nadal raportuje błędny tekst `18 probes + 2 map probes`, choć
  wywołuje trzy sondy mapy;
- validator sprawdza specs/layout, nie realne buildery kampusu i toru.

### Obrazy audytu

| Plik | Status | Obserwacja |
|---|---|---|
| `build/map_audit_20260713_center_top.png` | SESSION_ONLY | grid C zachowany; duże footprinty/overlay dominują |
| `build/map_audit_20260713_center_3q.png` | SESSION_ONLY | bumpery czytają się jak cienkie linie, skały jak drobny scatter |
| `build/map_audit_20260713_rocks_close.png` | SESSION_ONLY | zagęszczenie nie dowodzi linii ani braku zakleszczeń |
| `build/map_audit_20260713_track_top.png` | SESSION_ONLY | trzy pełne pętle tworzą nakładający się układ |
| `build/map_audit_20260713_track_profile.png` | SESSION_ONLY | widoczne warstwy/kliny, brak prześwitu produktu |
| `build/map_audit_20260713_drive_bumpers_n.png` | SESSION_ONLY | run zakończony, nie jest feel-testem w obie strony |
| `build/map_audit_20260713_drive_rocks_e.png` | SESSION_ONLY | run zakończony, brak macierzy kątów najazdu |
| `build/map_audit_20260713_drive_main_straight.png` | SESSION_ONLY | stabilny fragment nie dowodzi pełnego okrążenia |
| `build/map_audit_20260713_drive_red_articulation_reverse.png` | SESSION_ONLY | nie dowodzi przejazdu przez błędny profil |

Teleporty obserwowane w sesji:

- N bumper: `(-55,38)`, 420 klatek;
- E rock: `(18,0)`, 300 klatek;
- main straight: `(-120,150)`, 420 klatek;
- red articulation reverse: `(100,91.7)`, 120 klatek.

Brakuje literalnego formatu kamer, pełnych komend env, seedów i logów tych
czterech przejazdów. WP-01 ma je ustanowić od nowa; nie wolno rekonstruować ich
z samych nazw PNG i nazywać odtwarzalnymi.

## Szablon następnego wpisu

```text
evidence_id:
wp_id:
state:
git_hash_or_candidate_hash:
sample_name:
command:
seed:
teleport:
camera:
frames:
png_path:
pre_change_metric:
post_change_metric:
expected_observation:
actual_observation:
automatic_gate:
drive_gate:
jozz_gate:
known_limitation:
```
