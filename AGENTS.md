# AGENTS.md — globalna mapa operacyjna

Ten plik jest **globalnym wejściem dla agentów pracujących nad repozytorium**.
Nie jest kolejką zadań ani zgodą na zmianę produktu. Szczegółowy threat model i
reguły recurring agenta znajdują się w [`.automation/POLICY.md`](.automation/POLICY.md).

## 1. Dwa rodzaje autorytetu

Najpierw rozdziel politykę od zmiennego stanu:

1. `AGENTS.md` i `.automation/CONTROL.yaml` określają twardą politykę. Control Issue
   nie może zalegalizować merge'a, force-pusha, samomodyfikacji ani pracy A3/A4.
2. GitHub Issue `[AUTOMATION CONTROL] Box3d_FunProject recurring agent` przechowuje
   zmienny stan mieszczący się w tej polityce: enable/mode, aktywną kampanię,
   authoritative branch/head, gate'y i lease.

Jeżeli mutable Issue przeczy twardej polityce, wynik to `POLICY_CONFLICT` i zero
implementacji.

## 2. Kolejność odczytu

1. `AGENTS.md` oraz `.automation/CONTROL.yaml` — twarde ograniczenia.
2. GitHub Control Issue — dokładny mutable branch/head, tryb, gate'y i lease.
3. `AI_PROJECT_MEMORY.md` — router bieżącej kampanii.
4. właściwy `docs/*/CURRENT_STATE.md` — uczciwy stan domeny.
5. aktualny PR kampanii i jego remote head.
6. `docs/PROJECT_OPERATING_PLAN_PL.md` — workflow i roadmapa.
7. podręcznik właściwej domeny, np. `README_FOR_AGENTS.md` dla pojazdu.
8. checkpointy, tech debt, subsystem docs, kod i testy.

Historyczny checkpoint, zamknięty PR ani stary roadmap nigdy sam nie wybiera pracy.

## 3. Aktualna organizacja repozytorium

- mapa katalogów i odpowiedzialności: `docs/REPOSITORY_STRUCTURE_PL.md`;
- indeks dokumentacji: `docs/README.md`;
- zasady manualnych zmian i PR-ów: `CONTRIBUTING.md`;
- aktywna kampania i stan: `AI_PROJECT_MEMORY.md` oraz domenowy `CURRENT_STATE.md`;
- globalna roadmapa operacyjna: `docs/PROJECT_OPERATING_PLAN_PL.md`;
- pojazd: `README_FOR_AGENTS.md` — **podręcznik domeny, nie globalna polityka**.

`main` jest zachowaną linią upstream. Nie zakładaj, że domyślny branch GitHuba jest
bieżącym branchem projektu. Dokładny branch zawsze pochodzi z Control Issue.

## 4. Minimalny preflight

```powershell
python tools/automation/validate_control.py
python tools/project/repository_audit.py
python tools/automation/preflight.py
python -m unittest discover -s tests/automation -p "test_*.py"
python -m unittest discover -s tests/project -p "test_*.py"
```

Pełna bramka produktu pozostaje bez zmian:

```powershell
.\tools\gate.ps1
```

Automatyzacja może ją orkiestracyjnie uruchomić, ale nie może reimplementować ani
rozluźniać jej logiki.

## 5. Twarde reguły pracy

- Domyślny tryb recurring agenta to `PLAN_ONLY`; agent nie podnosi uprawnień.
- Bez potwierdzonego lease nie ma implementacji.
- Przy `LOCK_UNCERTAIN`, pending CI lub zmianie exact base SHA: zero zapisu.
- Jeden przebieg wybiera zero albo jeden istniejący work item.
- Implementować wolno tylko owner-approved `AGENT_READY` klasy A2 i tylko w
  `IMPLEMENT_SAFE`.
- A3 i A4 zawsze kończą się STOP.
- Jeden work item = nowy branch `automation/<work-item-id>/<run-id>`.
- Jeden run = najwyżej jeden draft PR; bez merge, rebase, force-push, retargetowania
  i zamykania cudzych PR-ów.
- Manualna praca również zaczyna się na nowym izolowanym branchu z exact remote SHA.
- Nie zapisuj bezpośrednio na `main`, `jozz-vehicle-sandbox-m0` ani aktywnym branchu.
- Raporty cykliczne trafiają do Control Issue lub `build/automation/`, nie do Git.
- Brak bezpiecznej pracy jest poprawnym wynikiem; nie wymyślaj zastępczego cleanupu.

## 6. STOP gates

Natychmiast zatrzymaj implementację przy: decyzji właściciela, visual review,
prywatnych danych, zmianie testowych progów, zaakceptowanego zachowania/defaultów,
fizyki/UX, Box3D `src/` lub `include/`, konflikcie polityki, aktywnym agencie,
niepewnym locku, przesuniętym base SHA, pending CI albo zbyt dużym zadaniu.

## 7. Ochrona governance i control plane

Automatyczny run nie może zmieniać plików wymienionych w
`scope.protected_control_paths` z `.automation/CONTROL.yaml`. Obejmuje to politykę,
workflowy, narzędzia governance, szablony PR i główne mapy repozytorium.

Wadę governance wolno wykryć i opisać. Jej naprawa wymaga osobnej, jawnie
owner-directed zmiany A3 — nigdy samomodyfikacji recurring runu.

## 8. Szczegóły recurring agenta

- policy i threat model: `.automation/POLICY.md`;
- prompt runtime: `.automation/RUNTIME_PROMPT.md`;
- kolejka: `.automation/WORK_ITEMS.json`;
- schema work itemu: `.automation/WORK_ITEM_SCHEMA.json`;
- template automatycznego PR: `.github/PULL_REQUEST_TEMPLATE/automation.md`;
- template manualnego PR: `.github/PULL_REQUEST_TEMPLATE/manual.md`.
