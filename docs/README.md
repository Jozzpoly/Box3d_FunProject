# Dokumentacja Box3d_FunProject — indeks

Ten plik jest mapą dokumentacji. Nie jest current state, roadmapą ani zgodą na
implementację.

## 1. Zawsze zacznij tutaj

```text
AGENTS.md
→ GitHub Control Issue
→ AI_PROJECT_MEMORY.md
→ właściwy docs/*/CURRENT_STATE.md
→ aktywny PR i remote head
→ docs/PROJECT_OPERATING_PLAN_PL.md
```

Exact mutable SHA pochodzi wyłącznie z Control Issue.

## 2. Globalne źródła prawdy

| Plik | Rola | Kiedy aktualizować |
|---|---|---|
| `AGENTS.md` | globalna polityka i routing agentów | tylko przy zmianie governance |
| `.automation/CONTROL.yaml` | ścisły control contract | owner-directed A3 |
| `AI_PROJECT_MEMORY.md` | router aktywnej kampanii | campaign/authority/gates |
| `docs/PROJECT_OPERATING_PLAN_PL.md` | workflow i roadmapa krytycznej ścieżki | stage/workflow/strategy |
| `CONTRIBUTING.md` | manualny branch/PR/evidence workflow | przy zmianie procesu pracy |
| `docs/REPOSITORY_STRUCTURE_PL.md` | ownership katalogów i walidacja | przy zmianie architektury repo |

## 3. Aktywne current-state documents

### Scan import

- `docs/scan_import/CURRENT_STATE.md` — jedyny current state aktywnej kampanii scan;
- `docs/scan_import/00_START_HERE.md` — wejście do architektury scan;
- `docs/scan_import/ARCHITECTURE.md` — trwałe kontrakty domeny;
- `docs/scan_import/P2A_SOURCE_VISUAL_PREVIEW.md` — exact render-only preview;
- `docs/scan_import/STATUS.md` — krótki pointer, nie drugi current state.

### Vehicle

- `README_FOR_AGENTS.md` — zaakceptowane reguły domeny pojazdu;
- `docs/CURRENT_STATE_INDEX_PL.md` — szczegółowy ledger milestone'ów pojazdu;
- `docs/SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` — rig/damper/mount;
- `docs/SUBSYSTEM_UI_PRESETS_PL.md` — UI/presets/persistence.

Vehicle docs nie aktywują kampanii samodzielnie.

## 4. Roadmapa i decyzje

- `docs/PROJECT_OPERATING_PLAN_PL.md` — jedyna globalna roadmapa operacyjna;
- `docs/adr/**` — trwałe decyzje architektoniczne;
- `docs/TECH_DEBT_PL.md` — znane ryzyka i odłożona praca;
- `docs/CHECKPOINTS_PL.md` — historyczny ledger handoffów.

Nie twórz drugiej globalnej roadmapy. Aktualizuj istniejący plan albo utwórz
lokalny plan kampanii tylko wtedy, gdy ma wyraźny scope i lifecycle.

## 5. Materiały historyczne

Dokumenty milestone, audyty i plany z datą są kontekstem historycznym. Mogą zawierać
wartościowe reasoning i evidence, ale nie są task queue.

`docs/archive/**` przechowuje starsze handoffy i raporty, których nie należy czytać
przed current state. Historyczne scan/photogrammetry raporty pozostają w `docs/`,
ponieważ nadal dokumentują lineage; ich rolę określa ten indeks.

## 6. Reguły aktualizacji

Aktualizuj dokument tylko wtedy, gdy jego własna odpowiedzialność naprawdę się
zmieniła:

- campaign/authority/gates → `AI_PROJECT_MEMORY.md`;
- stan domeny/evidence boundary → matching `CURRENT_STATE.md`;
- workflow/roadmap stage → `PROJECT_OPERATING_PLAN_PL.md`;
- accepted vehicle rules → `README_FOR_AGENTS.md`;
- subsystem contract → właściwy `SUBSYSTEM_*` lub ADR;
- historyczny milestone → checkpoint/report.

Rutynowy CI PASS, niezmieniony gate i cykliczny raport nie wymagają commitu.

## 7. Nazewnictwo statusów

Status musi opisywać najwyższy poziom faktycznie udowodniony. Przykładowe granice:

```text
PASS_CODE_AND_CI
PASS_OWNER_PRIVATE_EVIDENCE
REAL_PREVIEW_PIPELINE_CODE_READY
REAL_PREVIEW_PACK_READY / VISUAL_REVIEW_PENDING
TERRAIN_VISIBLE_PASS
```

Nie promuj capability na podstawie nazwy pliku, planu, kompilacji albo intentu.

## 8. Prywatność dokumentacji

W żadnym dokumencie, Issue ani PR nie umieszczaj:

- prywatnych ścieżek ownera;
- współrzędnych i lokalizacji;
- raw scan data;
- prywatnych source hashes i receipts;
- credentials.

Dokumentuj logical IDs, publiczne kontrakty, wyniki redacted i granice capability.

## 9. Walidacja driftu

```powershell
python tools/project/repository_audit.py
python -m unittest discover -s tests/project -p "test_*.py"
```

Audit sprawdza routing, wymagane pliki, spójność aktywnego brancha/PR/statusu,
workflowy CI oraz brak ponownego uznania vehicle manual za globalną politykę.
