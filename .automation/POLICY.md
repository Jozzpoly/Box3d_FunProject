# Recurring Agent Policy

## 1. Cel i granice

Ta infrastruktura ogranicza cyklicznego agenta repozytorium. Nie jest zgodą na
samodzielny rozwój produktu.

Stan harmonogramu, `enabled`, tryb, kampania, exact branch/head, gate'y i lease są
zmienne i pochodzą z GitHub Control Issue. Repozytorium celowo nie zawiera triggera
GitHub Actions `schedule:`; zewnętrzny scheduler uruchamia prompt, a repozytorium
weryfikuje każdy przebieg fail-closed.

Control plane obejmuje:

- `AGENTS.md` i `.automation/CONTROL.yaml`;
- tę politykę, `RUNTIME_PROMPT.md`, kolejkę i schemat work itemów;
- jedno trwałe GitHub Control Issue;
- `tools/automation/**`, `tests/automation/**` i workflow safety;
- pliki governance wymienione w `scope.protected_control_paths`.

Work plane obejmuje wyłącznie nowy branch i draft PR jednego wcześniej
zatwierdzonego work itemu. Control plane i work plane nie mogą być zmieniane w tym
samym autonomicznym runie.

## 2. Model zagrożeń

System zakłada możliwość:

- równoległego uruchomienia dwóch agentów;
- porzuconego lub stale locka;
- przesunięcia authoritative head podczas pracy;
- równoległej pracy właściciela lub innego agenta;
- nieaktualnych albo sprzecznych dokumentów;
- pomylenia upstream `main` z aktywną linią projektu;
- próby podniesienia własnych uprawnień;
- stworzenia zadania i natychmiastowego wdrożenia go przez ten sam run;
- ukrycia zmiany zachowania jako refaktoru lub cleanupu;
- rozluźnienia testu, progu albo acceptance criteria dla uzyskania PASS;
- przypadkowego zapisania prywatnych danych w logu, commicie, Issue lub PR;
- wymyślenia zastępczej pracy, gdy kolejka jest pusta lub zablokowana;
- driftu między Control Issue, dokumentacją, PR-em i workflowami CI.

Zasada nadrzędna: niepewność obniża zdolność działania. Nigdy jej nie podnosi.

## 3. Polityka a mutable state

### Twarda polityka

`AGENTS.md`, `.automation/CONTROL.yaml` i ta polityka ograniczają wszystkie runy.
Control Issue nie może zalegalizować operacji zabronionej w repozytorium.

### Zmienny stan

Control Issue jest najwyższym źródłem dla:

- `enabled` i `mode` w granicach polityki;
- aktywnej kampanii;
- authoritative branch i pełnego head SHA;
- owner/visual/private gate'ów;
- active lease, branch i PR runu;
- ostatniego wyniku operacyjnego.

Jeżeli Issue przeczy polityce, wynik to `POLICY_CONFLICT / NO_IMPLEMENTATION`.

## 4. Hierarchia faktów projektu

Po załadowaniu twardej polityki agent rozwiązuje fakty w kolejności zakodowanej w
`CONTROL.yaml`:

1. Control Issue — mutable campaign/branch/head/gates/lease;
2. `AI_PROJECT_MEMORY.md` — router aktywnej kampanii;
3. domenowy `docs/*/CURRENT_STATE.md` — uczciwy stan tej kampanii;
4. aktywny PR kampanii — realna topologia i remote head;
5. właściwy podręcznik domeny, np. `README_FOR_AGENTS.md` dla pojazdu;
6. checkpoint ledger;
7. technical debt;
8. subsystem docs;
9. kod i testy.

`docs/PROJECT_OPERATING_PLAN_PL.md` opisuje workflow i roadmapę, ale nie może
nadpisać policy, mutable state ani evidence boundary. Historyczny dokument lub
zamknięty PR nigdy sam nie aktywuje pracy.

## 5. Poziomy autonomii

- `DISABLED`: brak pracy poza sprawdzeniem control state.
- `READ_ONLY`: A0 — odczyt, audyt i raport bez zapisu produktu.
- `PLAN_ONLY`: A0 oraz plan/propozycja; zero implementacji.
- `IMPLEMENT_SAFE`: A0, A1 oraz wyłącznie owner-approved `AGENT_READY` klasy A2.

Tylko właściciel może podnieść tryb. Agent nie zmienia własnego `CONTROL.yaml`,
mode, risk modelu, scope ani merge policy.

## 6. Klasy ryzyka

### A0 — odczyt, audyt, raport

Bez zmian produktu. Dozwolone we wszystkich aktywnych trybach.

### A1 — dokumentacja, testy i diagnostyka

Niskie ryzyko i brak zmiany zaakceptowanego zachowania. Autonomicznie wykonywalne
dopiero w `IMPLEMENT_SAFE` i wyłącznie przy jawnym allowed scope.

### A2 — ograniczona implementacja

Mały izolowany zakres, exact base SHA, jednoznaczne acceptance criteria, required
tests oraz wcześniej ustawiony przez właściciela status `AGENT_READY`.

### A3 — decyzja właściciela albo zmiana produktu/polityki

Obejmuje fizykę, feel, UX, defaults, realistic-vs-arcade, progi testów, control
plane, workflowy, merge, retarget i zamykanie PR-ów. Nigdy autonomicznie.

### A4 — prywatne, wizualne, ręczne lub zabronione

Prywatne skany i ścieżki, visual acceptance, owner-local runtime, credentials,
Box3D `src/` i `include/` oraz operacje niewiarygodnie walidowalne. Zawsze STOP.

## 7. Kolejka pracy

`.automation/WORK_ITEMS.json` jest jedyną wersjonowaną kolejką recurring agenta.
Każdy item musi przejść ścisły schema validation.

- `PROPOSED`: pomysł, niewykonywalny;
- `AGENT_READY`: owner-approved A2, jedyny implementowalny stan;
- `ACTIVE`: przypisany do potwierdzonego lease/runu;
- `BLOCKED`, `OWNER_NEEDED`, `VISUAL_REVIEW`: STOP;
- `DONE`, `REJECTED`: niewybieralne.

Run nie może dopisać itemu, wypromować go i wykonać w tej samej iteracji. Zmiana
kolejki jest control-plane A3.

## 8. Lock/lease

Mutable lock istnieje wyłącznie w Control Issue. Commitowany plik nie zapewnia
wyłączności.

Przed implementacją agent:

1. odczytuje dokładnie jedno otwarte Control Issue;
2. sprawdza identity/schema, `enabled`, mode, campaign, authoritative branch/head i
   gate'y;
3. sprawdza brak aktywnego lub stale lease;
4. sprawdza brak drugiego automation PR i nakładającej się pracy;
5. zapisuje run ID, start i expiration;
6. odczytuje Issue ponownie;
7. czeka configured settle interval i odczytuje ponownie;
8. rewaliduje lease oraz exact remote head przed branchem i przed pierwszym zapisem.

GitHub Issue edit nie jest atomowym compare-and-swap. Dlatego każdy niepewny claim
kończy się bez implementacji. Stale lease nie jest automatycznie przejmowany.

## 9. Branch i PR policy

Automatyczny work item używa:

```text
automation/<work-item-id>/<run-id>
```

Manualna owner-directed praca również używa nowego izolowanego brancha z dokładnego
remote SHA. Nigdy nie zakładaj, że `main` jest aktywną linią; odczytaj Control Issue.

Zakazane bez osobnej jawnej zgody właściciela:

- push do `main`, `jozz-vehicle-sandbox-m0`, aktywnego lub cudzego brancha;
- aktualizacja istniejącego product PR przez recurring run;
- rebase, force-push, retarget, merge i auto-merge;
- zamykanie produktu PR/Issue;
- branch deletion lub rewrite history.

Jeden run tworzy najwyżej jeden draft PR. PR podaje exact base, scope, kryteria,
testy, niedostępne capability, gate'y i non-actions.

## 10. Scope validation

`validate_scope.py` odrzuca:

- pliki poza allowed paths;
- forbidden paths i globalne `src/**` / `include/**`;
- przekroczenie max files lub changed lines;
- zmianę chronionego governance/control plane;
- zmianę progów, tolerancji lub limitów na threshold-sensitive paths;
- zły branch runu.

Mechaniczna walidacja nie dowodzi semantycznego związku każdej linii z zadaniem.
Mały zakres, testy, exact allowed paths i review pozostają obowiązkowe.

## 11. Testy i capability

Agent używa istniejących bramek, przede wszystkim `tools/gate.ps1`; nie kopiuje ich
logiki. Cloud uruchamia tylko rzeczywiście dostępne testy. Brak Windows/native/
render/private capability jest jawnie raportowany, nigdy zaliczany.

Zielone CI nie zastępuje visual gate, prywatnego owner flow ani owner acceptance.
Zmiana testu lub progu dla uzyskania PASS jest A3 i STOP.

## 12. No-op i STOP semantics

Poprawne bezpieczne wyniki bez implementacji:

`NO_MATERIAL_CHANGE`, `NO_SAFE_WORK`, `ACTIVE_AGENT_DETECTED`, `LOCK_UNAVAILABLE`,
`LOCK_UNCERTAIN`, `OWNER_GATE`, `VISUAL_GATE`, `PRIVATE_DATA_REQUIRED`, `CI_PENDING`,
`BASE_MOVED`, `TASK_TOO_LARGE`, `POLICY_CONFLICT`.

Żaden z nich nie uruchamia zastępczego refaktoru, cleanupu ani funkcji.

## 13. Raportowanie i prywatność

Stan runu trafia do Control Issue. Szczegółowy raport roboczy może istnieć pod
`build/automation/` i nie jest commitowany. Dokumentacja projektu zmienia się tylko
przy realnym przesunięciu stanu albo naprawie konfliktu autorytetu.

Prywatne ścieżki, współrzędne, hashe prywatnych źródeł, credentials i raw data nie
trafiają do Issue, PR, CI ani Git.

## 14. Recovery

- Przerwany run pozostawia diagnozowalny run ID, branch/PR i lease state.
- Automat nie kontynuuje na cudzym branchu.
- Stale lease rozstrzyga ręcznie właściciel po sprawdzeniu runu, brancha i PR.
- Po zmianie kampanii właściciel aktualizuje Issue, a osobna manualna zmiana
  synchronizuje wersjonowane mapy projektu.
- Rozbieżność między Issue a dokumentami blokuje implementację, nie jest naprawiana
  przez recurring run.

## 15. Samomodyfikacja

Recurring agent może wykryć wadę governance i ją opisać. Nie może zmieniać własnej
polityki, narzędzi, workflowów, cadence, locka, risk classification, scope, test
requirements ani merge policy.

Naprawa governance wymaga osobnej owner-directed pracy A3, osobnego brancha i draft
PR. Produkt i reguły oceny produktu nie mogą być zmieniane w tej samej paczce.
