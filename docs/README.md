# Dokumentacja Box3d_FunProject — indeks

Ten plik jest mapą dokumentacji. Nie jest current state, roadmapą ani zgodą na
implementację lub usuwanie branchy.

## 1. Zawsze zacznij tutaj

```text
AGENTS.md
→ GitHub Control Issue #11
→ AI_PROJECT_MEMORY.md
→ właściwy docs/*/CURRENT_STATE.md
→ aktywny PR i remote head
→ docs/PROJECT_OPERATING_PLAN_PL.md
→ docs/PROJECT_CHARTER_PL.md
```

Podczas re-foundation dodatkowo przeczytaj:

```text
docs/PROJECT_REFOUNDATION_AUDIT_2026_07_22_PL.md
docs/PROJECT_INVENTORY.json
docs/PROJECT_FORENSIC_INVENTORY_2026_07_22_PL.md
```

Exact mutable SHA pochodzi wyłącznie z Control Issue. Charter pomaga interpretować
cel produktu, ale nie nadpisuje policy, current state ani evidence. Inventory opisuje
audytowany stan i luki; sam nie przesuwa authority.

## 2. Globalne źródła prawdy

| Plik | Rola | Kiedy aktualizować |
|---|---|---|
| `AGENTS.md` | globalna polityka i routing agentów | tylko przy zmianie governance |
| `.automation/CONTROL.yaml` | ścisły control contract | owner-directed A3 |
| `AI_PROJECT_MEMORY.md` | krótki router aktywnej kampanii i gate'ów | campaign/authority/gates |
| `docs/PROJECT_OPERATING_PLAN_PL.md` | jedna krytyczna ścieżka i workflow | stage/workflow/strategy |
| `docs/PROJECT_CHARTER_PL.md` | trwała dusza, wizja i zasady produktu | tylko przy realnej zmianie intencji ownera |
| `docs/PROJECT_INVENTORY.json` | machine-readable domeny, lineage, retention i jawne luki | podczas kontrolowanego re-foundation/integration |
| `CONTRIBUTING.md` | manualny branch/PR/evidence workflow | przy zmianie procesu pracy |
| `docs/REPOSITORY_STRUCTURE_PL.md` | ownership katalogów i walidacja | przy zmianie architektury repo |

Charter ani inventory nie przechowują mutable exact head jako własnej authority.

## 3. Aktywne current-state documents

### Scan import

- `docs/scan_import/CURRENT_STATE.md` — jedyny current state domeny scan;
- `docs/scan_import/00_START_HERE.md` — aktualny router domeny scan;
- `docs/scan_import/ARCHITECTURE.md` — trwałe kontrakty evidence/import;
- `docs/scan_import/P2A_SOURCE_VISUAL_PREVIEW.md` — zamknięty geometry-only preview v1;
- `docs/scan_import/STATUS.md` — krótki pointer, nie drugi current state.

Najwyższy current scan capability:

```text
TERRAIN_VISIBLE_PASS
```

Następny gate:

```text
TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ dopiero potem collision research
```

### Vehicle

- `README_FOR_AGENTS.md` — zaakceptowane reguły domeny pojazdu;
- `docs/CURRENT_STATE_INDEX_PL.md` — szczegółowy vehicle milestone ledger;
- `docs/TECH_DEBT_PL.md` — vehicle-only debt registry, nie globalna polityka;
- `docs/SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` — rig/damper/mount;
- `docs/SUBSYSTEM_UI_PRESETS_PL.md` — UI/presets/persistence.

Vehicle docs nie aktywują kampanii samodzielnie.

## 4. Milestone evidence

Pełny redacted checkpoint pierwszego realnego native preview:

```text
docs/scan_import/TERRAIN_VISIBLE_PASS_2026_07_22_PL.md
```

Dokument zapisuje:

- wykonane lokalne bramki;
- realny seven-tile native load;
- owner visual acceptance;
- same-revision restart;
- zaakceptowane source limitations;
- jawne non-capabilities, szczególnie brak finalnego scale/collision/drive pass.

Prywatne ścieżki, lokalizacja, raw data i source hashes nie należą do milestone doc.

## 5. Project re-foundation

Metoda i fazy owner-directed audytu całego projektu:

```text
docs/PROJECT_REFOUNDATION_AUDIT_2026_07_22_PL.md
```

Aktualny forensic snapshot:

```text
docs/PROJECT_INVENTORY.json
docs/PROJECT_FORENSIC_INVENTORY_2026_07_22_PL.md
```

Inventory obejmuje:

- Git/PR lineage oraz branch retention classes;
- upstream/shared-host/vehicle/synthetic-world domains;
- scan evidence, geometry preview, textures, scale i collision boundaries;
- future Blender/world authoring;
- automation/governance;
- privacy oraz granicę z JES;
- elementy `UNREVIEWED`, których nie wolno ukryć pod „później”.

Status pozostaje `FORENSIC_INVENTORY_IN_PROGRESS`. To nie jest deklaracja, że każdy
plik i commit został już ręcznie przeczytany.

## 6. Branch reduction

Owner wymaga po cleanupie:

```text
hard maximum: 5 branches
preferred:    3 branches
```

Preferowany model:

```text
main
jozz-vehicle-sandbox-m0
ONE_CURRENT_INTEGRATED_PROJECT_BRANCH
```

Szczegółowy provisional delete/tag plan znajduje się w forensic report i JSON
inventory. Przed jakimkolwiek delete obowiązuje:

```powershell
git ls-remote --heads origin
```

oraz per-branch ancestry/content proof. Divergent heads wymagają tagu albo innego
jawnego reachable ref. `deletionAuthorized=false` pozostaje obowiązujące do osobnej
owner review. Nie usuwaj current authority ani otwartego review branch.

## 7. Roadmapa i decyzje

- `docs/PROJECT_OPERATING_PLAN_PL.md` — jedyna globalna roadmapa operacyjna;
- `docs/PROJECT_CHARTER_PL.md` — trwały kompas, nie roadmapa;
- `docs/adr/**` — trwałe decyzje architektoniczne;
- `docs/TECH_DEBT_PL.md` — vehicle debt i accepted limits;
- `docs/CHECKPOINTS_PL.md` — historyczny vehicle ledger handoffów.

Nie twórz drugiej globalnej roadmapy. Lokalny campaign brief może powstać tylko dla
jednego jasno ograniczonego etapu i musi wskazywać lifecycle.

## 8. Materiały historyczne i eksperymentalne

Dokumenty milestone, audyty i plany z datą są kontekstem historycznym, chyba że indeks
jawnie oznacza je jako aktywny audit/brief.

`docs/archive/**` przechowuje starsze handoffy i raporty, których nie należy czytać
przed current state.

Szczególnie ważne historyczne scan evidence:

- `docs/PHOTOGRAMMETRY_ROADMAP_EXPERIMENT_REPORT_2026_07_15_PL.md` — offline
  geometry/ground/chunk/texture experiments;
- `docs/PHOTOGRAMMETRY_WORLD_IMPORT_PL.md` — wcześniejsza architektura i zmieniona
  kolejność prac;
- P1/P1B checkpointy — evidence lineage, nie current task queue;
- PR #7 / Issue #14 — divergent surface evidence, nie accepted surface.

Wcześniejsze texture experiments są materiałem do odzyskania. Nie są automatycznie
finalnym contractem dla obecnego seven-tile packa.

## 9. Reguły aktualizacji

Aktualizuj dokument tylko wtedy, gdy jego odpowiedzialność naprawdę się zmieniła:

- owner philosophy/product intent → `PROJECT_CHARTER_PL.md`;
- campaign/authority/gates → `AI_PROJECT_MEMORY.md`;
- stan domeny/evidence boundary → matching `CURRENT_STATE.md`;
- workflow/roadmap stage → `PROJECT_OPERATING_PLAN_PL.md`;
- accepted vehicle rules → `README_FOR_AGENTS.md`;
- vehicle debt/accepted limits → `TECH_DEBT_PL.md`;
- subsystem contract → właściwy `SUBSYSTEM_*` lub ADR;
- realny milestone → checkpoint/report;
- pełna reorganizacja → re-foundation audit + machine-readable inventory.

Rutynowy CI PASS, niezmieniony gate i cykliczny raport nie wymagają commitu.

## 10. Nazewnictwo statusów

Status opisuje najwyższy poziom faktycznie udowodniony:

```text
PASS_CODE_AND_CI
PASS_OWNER_PRIVATE_EVIDENCE
REAL_PREVIEW_PIPELINE_CODE_READY
REAL_PREVIEW_PACK_READY / VISUAL_REVIEW_PENDING
TERRAIN_VISIBLE_PASS
TEXTURED_SOURCE_PREVIEW_READY
WORLD_SCALE_VALIDATED
COLLISION_PROJECTION_READY
FIRST_REAL_SCAN_DRIVE_PASS
OWNER_FUN_VERDICT
```

Nie promuj capability na podstawie nazwy pliku, planu, kompilacji albo intentu.
`TERRAIN_VISIBLE_PASS` nie oznacza automatycznie `WORLD_SCALE_VALIDATED`.

## 11. Warstwy prawdy świata

Dokumentacja musi zachować rozdział:

```text
PRIVATE_SOURCE_EVIDENCE
SOURCE_GEOMETRY_PREVIEW
SOURCE_TEXTURE_PREVIEW
AUTHORED_WORLD_ASSETS
RENDER_DERIVATIVES
PHYSICS_SURFACE
SURFACE_MATERIAL_MAP
GAMEPLAY_SEMANTICS
WORLD_COMPOSITION_AND_STREAMING
```

Dokument jednej warstwy nie może przyznać capability innej bez jawnej promocji i
właściwego evidence.

## 12. Prywatność dokumentacji

W żadnym dokumencie, Issue ani PR nie umieszczaj:

- prywatnych ścieżek ownera;
- współrzędnych i lokalizacji;
- raw scan data ani original texture payloads;
- prywatnych source hashes i receipts;
- credentials.

Dokumentuj logical IDs, publiczne kontrakty, redacted wyniki i granice capability.

## 13. Walidacja driftu

```powershell
python tools/automation/validate_control.py
python tools/project/repository_audit.py
python tools/project/refoundation_inventory_audit.py
python -m unittest discover -s tests/project -p "test_*.py"
```

Audit sprawdza routing, wymagane pliki, spójność active branch/PR/statusu, workflowy
CI, obecność charteru i teksturowanego gate'u przed scale/collision.

Inventory audit dodatkowo sprawdza:

- komplet wymaganych domen i PR lineage;
- target 3 / hard max 5 branchy;
- brak pre-autoryzacji delete;
- exact preservation-tag targets dla divergent heads;
- brak powrotu starego P1B current status;
- brak direct-push authority drift w vehicle debt registry.
