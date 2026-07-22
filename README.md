# Jozz Vehicle / Box3d_FunProject

Eksperymentalny sandbox inżynieryjny Jozza zbudowany na upstreamowym silniku
[Box3D](https://github.com/erincatto/box3d). Projekt łączy fizyczne pojazdy,
autorskie modele glTF/Blockbench, narzędzia diagnostyczne oraz kontrolowany pipeline
importu rzeczywistych skanów terenu.

> **To repozytorium nie jest zwykłym mirrorem upstream Box3D.**
>
> GitHubowy default branch `main` zachowuje starszą linię upstream/projektu i nie
> jest automatycznie bieżącym branchem pracy. Aktualną kampanię, branch i exact head
> zawsze odczytuj z GitHub Control Issue wskazanego w `AGENTS.md`.

## Start tutaj

### Dla człowieka

1. [`AI_PROJECT_MEMORY.md`](AI_PROJECT_MEMORY.md) — co jest aktualnie aktywne;
2. [`docs/PROJECT_OPERATING_PLAN_PL.md`](docs/PROJECT_OPERATING_PLAN_PL.md) — workflow
   i roadmapa;
3. [`docs/README.md`](docs/README.md) — indeks dokumentacji;
4. [`CONTRIBUTING.md`](CONTRIBUTING.md) — bezpieczny sposób pracy i tworzenia PR;
5. [`docs/REPOSITORY_STRUCTURE_PL.md`](docs/REPOSITORY_STRUCTURE_PL.md) — mapa kodu,
   testów, narzędzi i źródeł prawdy.

### Dla agenta AI

Zacznij od [`AGENTS.md`](AGENTS.md). Następnie odczytaj Control Issue,
`.automation/CONTROL.yaml`, pamięć projektu i current state aktywnej domeny.

## Aktualny uczciwy kierunek

Bieżąca kampania projektu jest wybierana poza tym README, aby wersjonowany tekst nie
udawał mutable control plane. W chwili przygotowania tej struktury najbliższą
realną granicą jest prywatny, siedmiokaflowy, natywny preview skanu terenu.

Najwyższy potwierdzony poziom capability i nierozwiązane gate'y znajdują się w:

```text
AI_PROJECT_MEMORY.md
docs/scan_import/CURRENT_STATE.md
GitHub Control Issue
```

CI, kompilacja i wygenerowany asset nie stanowią visual acceptance ani prywatnego
dowodu ownera.

## Główne domeny

### Vehicle sandbox

Natywny Windows/C++ sandbox pojazdów z:

- fizycznym zawieszeniem wielobryłowym;
- back-drivable steering rack;
- napędem i hamowaniem momentem;
- autorskimi modelami glTF;
- presetami, persistence i debug overlays;
- headless validatorami i screenshot toolingiem.

Reguły tej domeny: [`README_FOR_AGENTS.md`](README_FOR_AGENTS.md).

### Scan terrain pipeline

Prywatnościowo bezpieczny pipeline obejmujący:

- inspekcję par GLB/PLY;
- source-frame i source-package contracts;
- prywatne/shareable evidence bundles;
- owner-local privacy gate;
- exact render-only preview pack;
- niezależną weryfikację;
- native preview lab;
- resumable source resolution i owner flow.

Stan domeny: [`docs/scan_import/CURRENT_STATE.md`](docs/scan_import/CURRENT_STATE.md).

### Recurring repository operator

Fail-closed pętla agentowa działa według `.automation/**`. Aktualny tryb i enable
pochodzą z Control Issue. Agent nie może sam podnieść autonomii, zmienić polityki,
zmergować PR ani ominąć owner/private/visual gate'ów.

## Budowanie i walidacja

Docelowe środowisko projektu to Windows + PowerShell.

### Pełna istniejąca bramka projektu

```powershell
.\tools\gate.ps1
```

### Governance i control plane

```powershell
python tools/automation/validate_control.py
python tools/project/repository_audit.py
python -m unittest discover -s tests/automation -p "test_*.py"
python -m unittest discover -s tests/project -p "test_*.py"
```

### Scan contracts

```powershell
python tools/scan_pipeline/run_p1_contracts.py
```

### Natywny preview / sample build

```powershell
cmake --preset windows
cmake --build --preset windows-debug --target samples
```

Pełne wymagania i ograniczenia znajdują się w dokumentacji domenowej. Nie traktuj
samego `PASS` procesu jako dowodu poprawności wizualnej lub fizycznej.

## Zasady bezpieczeństwa repozytorium

- `src/` i `include/` pozostają upstreamowym core Box3D i nie są zwykłym obszarem
  pracy projektu;
- nie zapisuj bezpośrednio na `main`, baseline ani aktywny branch kampanii;
- każda manualna zmiana zaczyna się z exact remote SHA na nowym branchu;
- bez jawnej zgody nie ma merge, force-push, rebase, retarget ani branch deletion;
- prywatne skany, ścieżki, współrzędne, hashe źródeł i credentials nie trafiają do
  Git, PR, Issue ani publicznego CI;
- visual, feel, UX, defaults i owner acceptance są bramkami ludzkimi;
- stary roadmap lub zamknięty PR nie aktywuje pracy.

## Branch model

```text
main
└─ zachowana linia upstream / historyczny default

jozz-vehicle-sandbox-m0
└─ stabilny baseline domeny pojazdu

<authoritative branch from Control Issue>
└─ aktywna kampania i jej draft PR

agent/... / automation/...
└─ izolowane reviewable zmiany
```

Dokładne mutable branche i SHAs nie są hardkodowane w tym README.

## Dokumentacja

Repo zawiera dużo wartościowych raportów historycznych. Ich kompletność nie oznacza
aktualności. Używaj indeksu [`docs/README.md`](docs/README.md), który rozdziela:

- current authority;
- aktywną dokumentację domenową;
- decyzje architektoniczne;
- checkpointy i tech debt;
- materiały historyczne i archive.

## Upstream Box3D

Projekt bazuje na Box3D autorstwa Erin Catto — przenośnym silniku fizyki 3D w C17
z natywnymi sample'ami C++20. Upstream:

- repository: `erincatto/box3d`;
- strona i dokumentacja: `box2d.org` / upstream `docs/`;
- licencja: MIT.

Jozz-specific kod, assety, narzędzia i dokumentacja są rozwijane w tym forku, przy
zachowaniu wyraźnej granicy od upstreamowego engine core.

## Licencja

Upstream Box3D używa licencji MIT. Zobacz [`LICENSE`](LICENSE) oraz nagłówki
poszczególnych plików.
