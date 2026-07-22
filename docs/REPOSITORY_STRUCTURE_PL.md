# Struktura repozytorium i ownership — Box3d_FunProject

Ten dokument opisuje odpowiedzialność katalogów, granice zmian i właściwe bramki.
Nie wybiera aktywnej kampanii i nie zastępuje current state.

## 1. Warstwy repozytorium

```text
upstream engine core
→ Jozz native samples and shared host hooks
→ vehicle domain
→ scan pipeline
→ automation/governance
→ documentation and evidence
```

Każda warstwa ma inny poziom ryzyka. Wspólna lokalizacja w jednym repo nie oznacza
wspólnej authority.

## 2. Upstream Box3D core

```text
src/**
include/**
```

Ownership: upstream Box3D.

Reguły:

- poza zwykłym zakresem projektu i całkowicie poza autonomicznym scope;
- zmiana wymaga osobnej owner decision, analizy upstream impact i pełnej walidacji;
- nie używaj core patcha jako wygodnego obejścia problemu w sample/tooling;
- przypadkowe dotknięcie tych ścieżek jest STOP.

## 3. Native sample host i wspólne hooki

```text
samples/host/**
samples/main.cpp
samples/sample.*
samples/CMakeLists.txt
```

Ownership: shared native host.

Dozwolone są tylko małe addytywne i opt-in hooki, które nie zmieniają zachowania
niepowiązanych sample'i. Wymagają pełnego Windows build/gate i, przy zmianie
wizualnej, realnego render evidence.

## 4. Vehicle domain

```text
samples/jozz_vehicle_*
samples/validation/**
assets/contracts/**
assets/vehicle_presets/**
assets/source/**
README_FOR_AGENTS.md
docs/CURRENT_STATE_INDEX_PL.md
docs/SUBSYSTEM_*
```

Ownership: Jozz vehicle sandbox.

Główne bramki:

- `README_FOR_AGENTS.md`;
- `tools/gate.ps1`;
- headless validator output;
- screenshot/render evidence;
- owner drive/feel decision, gdy dotyczy.

Nie mieszaj refaktoru vehicle z aktywną kampanią scan bez jawnej zmiany campaign.

## 5. Scan terrain pipeline

```text
tools/scan_pipeline/**
tests/scan_pipeline/**
samples/jozz_scan_preview_*
docs/scan_import/**
.github/workflows/p1-scan-inspector.yml
```

Ownership: scan import/evidence/preview campaign.

Zasady:

- source evidence nie staje się automatycznie accepted world ani collision truth;
- private outputs pozostają pod ignorowanym `build/`;
- builder i verifier są osobnymi authority;
- real source pack, native load i visual acceptance mają oddzielne statusy;
- `run_p1_contracts.py` jest canonical dependency-free contract runnerem;
- native preview zmiany wymagają Windows sample build i render review.

## 6. Automation control plane

```text
.automation/**
tools/automation/**
tests/automation/**
.github/workflows/automation-foundation.yml
.github/PULL_REQUEST_TEMPLATE/automation.md
AGENTS.md
```

Ownership: owner-directed repository governance.

Recurring run nie może modyfikować tej warstwy. Zmiana jest A3, wymaga osobnego
manualnego brancha, draft PR i jawnej akceptacji ownera.

Control Issue przechowuje mutable state, ale nie może nadpisać twardych zakazów z
repozytorium.

## 7. Repository governance

```text
README.md
CONTRIBUTING.md
docs/README.md
docs/REPOSITORY_STRUCTURE_PL.md
tools/project/**
tests/project/**
.github/workflows/repository-governance.yml
.github/PULL_REQUEST_TEMPLATE/manual.md
```

Ownership: manual owner-directed governance.

Ta warstwa utrzymuje routing, spójność dokumentacji, workflow i review discipline.
Nie może zmieniać produktu w tej samej paczce.

## 8. Dokumentacja

### Current authority

```text
AI_PROJECT_MEMORY.md
docs/*/CURRENT_STATE.md
docs/PROJECT_OPERATING_PLAN_PL.md
```

### Domain manuals and contracts

```text
README_FOR_AGENTS.md
docs/scan_import/ARCHITECTURE.md
docs/SUBSYSTEM_*
docs/adr/**
```

### History and deferred work

```text
docs/CHECKPOINTS_PL.md
docs/TECH_DEBT_PL.md
docs/archive/**
dated audit/plan/report files
```

Aktualność wynika z roli dokumentu i authority order, nie z długości ani daty
ostatniej edycji.

## 9. Tests and gates map

| Zakres | Minimalna bramka | Dodatkowa capability |
|---|---|---|
| governance | `repository_audit.py`, `tests/project` | review authority |
| automation | `validate_control.py`, `tests/automation`, scenarios | Issue/lease integration |
| scan Python | `run_p1_contracts.py` | stdlib/NumPy matrix |
| scan native preview | scan contracts + samples build | real render/private pack |
| vehicle | `tools/gate.ps1` | numbers/screenshots/drive review |
| shared host | full project gate | regression across unrelated samples |
| Box3D core | STOP / owner plan | upstream-grade full validation |

Test niedostępny nie jest zaliczony. CI nie zastępuje private/visual/owner evidence.

## 10. Branch topology

- `main`: zachowana linia upstream/historyczny default;
- `jozz-vehicle-sandbox-m0`: stabilny vehicle baseline;
- branch z Control Issue: aktywna kampania;
- `agent/**`: manualne izolowane prace;
- `automation/**`: implementacyjne runy recurring agenta;
- zamknięte branche historyczne mogą pozostać jako evidence lineage.

Nie usuwaj ani nie przepisuj historycznych branchy tylko dla estetyki. Najpierw
udokumentuj zastąpienie, parking albo migrację.

## 11. Zasada jednego zakresu

W jednym PR nie łącz:

- produktu i control plane;
- scan oraz vehicle, jeżeli jedno nie jest bezpośrednią zależnością drugiego;
- workflow governance i zmian progów produktu;
- private evidence i publicznej dokumentacji;
- visual acceptance i automatycznego claimu PASS.

Jeżeli review wymaga dwóch różnych authority, zakres prawdopodobnie powinien zostać
rozdzielony.

## 12. Kiedy aktualizować tę mapę

Tylko gdy:

- powstaje nowa trwała domena lub katalog;
- zmienia się ownership ścieżek;
- zmienia się canonical test/gate;
- control plane lub branch model ma nową architekturę.

Nie aktualizuj jej dla pojedynczego feature'a albo rutynowego CI result.
