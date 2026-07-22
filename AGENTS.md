# AGENTS.md — mapa operacyjna

Ten plik jest krótkim wejściem dla agentów. Szczegółowe reguły automatyzacji są
w [`.automation/POLICY.md`](.automation/POLICY.md). Nie traktuj tego pliku jako
kolejki zadań ani zgody na zmianę produktu.

## Kolejność odczytu

1. `.automation/CONTROL.yaml` oraz GitHub Issue
   `[AUTOMATION CONTROL] Box3d_FunProject recurring agent`.
2. `AI_PROJECT_MEMORY.md` — wybór aktualnej kampanii i mapowanie do domeny.
3. właściwy `docs/*/CURRENT_STATE.md` — bieżący stan wybranej kampanii.
4. aktualny otwarty PR kampanii — branch, exact head SHA i zależności.
5. `README_FOR_AGENTS.md` — globalne reguły produktu i istniejące bramki.
6. `docs/CHECKPOINTS_PL.md` oraz `docs/TECH_DEBT_PL.md` — historia i ryzyka.
7. subsystem docs, kod i testy właściwe dla wybranego work itemu.

W razie konfliktu obowiązuje kolejność `authority.precedence` z
`.automation/CONTROL.yaml`. Historyczny checkpoint nigdy sam nie wybiera zadania.

## Komendy

```powershell
python tools/automation/validate_control.py
python tools/automation/preflight.py
python -m unittest discover -s tests/automation -p "test_*.py"
```

Pełna bramka produktu pozostaje bez zmian:

```powershell
.\tools\gate.ps1
```

Automatyzacja może ją orkiestracyjnie uruchomić, ale nie może reimplementować ani
rozluźniać jej logiki.

## Twarde reguły

- Domyślny tryb to `PLAN_ONLY`; agent nie podnosi własnych uprawnień.
- Nigdy nie pracuj bez potwierdzonego lease w control issue.
- Przy `LOCK_UNCERTAIN` lub zmianie base SHA: zero implementacji.
- Jeden przebieg wybiera zero albo jeden work item.
- Implementować wolno tylko `AGENT_READY`, A2, jawnie zatwierdzone przez właściciela.
- A3 i A4 zawsze kończą się STOP.
- Jeden work item = nowy branch `automation/<work-item-id>/<run-id>`.
- Jeden przebieg = najwyżej jeden draft PR; bez merge, rebase, force-push,
  retargetowania i zamykania innych PR-ów.
- Nie zapisuj bezpośrednio na `main`, `jozz-vehicle-sandbox-m0` ani aktywnym
  branchu kampanii/człowieka.
- Raporty cykliczne trafiają do control issue lub `build/automation/`, nie do Git.
- Brak bezpiecznej pracy jest poprawnym wynikiem; nie wymyślaj zastępczego refaktoru.

## STOP gates

Natychmiast zatrzymaj implementację przy: decyzji właściciela, visual review,
prywatnych danych, zmianie progów testów, zaakceptowanego zachowania/defaultów,
fizyki/UX, Box3D `src/` lub `include/`, konflikcie polityki, aktywnym agencie,
niepewnym locku, przesuniętym base SHA, pending CI albo zbyt dużym zadaniu.

## Ochrona control plane

Automatyczny run nie może zmieniać `AGENTS.md`, `.automation/**`,
`tools/automation/**`, workflow automatyzacji, klasyfikacji ryzyka, locka,
STOP gates, cadence ani merge policy. Może wyłącznie zaproponować osobny work
item klasy A3 do ręcznej akceptacji właściciela.

## Szczegóły

- polityka i threat model: `.automation/POLICY.md`
- gotowy prompt runtime: `.automation/RUNTIME_PROMPT.md`
- kolejka: `.automation/WORK_ITEMS.json`
- schema work itemu: `.automation/WORK_ITEM_SCHEMA.json`
- draft PR: `.github/PULL_REQUEST_TEMPLATE/automation.md`
