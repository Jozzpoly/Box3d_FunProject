# Recurring Agent Policy

## 1. Cel i granice

Ta infrastruktura przygotowuje repozytorium do cyklicznej pętli agentowej.
Nie jest zgodą na autonomiczny rozwój produktu. Harmonogram nie jest częścią tej
zmiany, a domyślny tryb pozostaje `PLAN_ONLY`.

Control plane obejmuje:

- `.automation/CONTROL.yaml`;
- trwały GitHub Issue `[AUTOMATION CONTROL] Box3d_FunProject recurring agent`;
- `.automation/WORK_ITEMS.json` i schema work itemów;
- `AGENTS.md`, ten dokument i `RUNTIME_PROMPT.md`;
- `tools/automation/**` oraz dedykowany workflow CI.

Work plane obejmuje wyłącznie branch i draft PR jednego zatwierdzonego work itemu.
Control plane i work plane nie mogą być zmieniane w tym samym autonomicznym runie.

## 2. Model zagrożeń

System zakłada możliwość:

- równoległego uruchomienia dwóch agentów;
- pozostawienia porzuconego lub stale locka;
- przesunięcia branch head podczas pracy;
- równoległej pracy właściciela lub innego agenta;
- nieaktualnych albo sprzecznych dokumentów;
- próby podniesienia własnych uprawnień;
- stworzenia przez agenta zadania i natychmiastowego wdrożenia go;
- ukrycia zmiany zachowania jako refaktoru;
- rozluźnienia testu lub progu w celu uzyskania PASS;
- przypadkowego zapisania prywatnych danych w logu, commicie albo PR;
- wymyślenia zastępczej pracy, gdy bezpiecznej pracy nie ma.

Zasada nadrzędna: niepewność obniża zdolność działania. Nigdy jej nie podnosi.

## 3. Hierarchia autorytetu

Dokładna kolejność jest zakodowana w `CONTROL.yaml`:

1. control issue — owner-controlled enable/mode/campaign/branch/head/gates/lease;
2. `AI_PROJECT_MEMORY.md` — globalna mapa aktualnej kampanii;
3. domenowy `docs/*/CURRENT_STATE.md` — bieżący stan tej kampanii;
4. aktualny PR kampanii — exact branch/head i realne zależności;
5. `README_FOR_AGENTS.md` — reguły globalne produktu i istniejące bramki;
6. `docs/CHECKPOINTS_PL.md` — najnowszy ledger, ale nie task queue;
7. `docs/TECH_DEBT_PL.md` — ryzyka i świadomie odłożona praca;
8. subsystem docs;
9. kod i testy.

Dokument historyczny nie może sam aktywować kampanii, work itemu ani podnieść
statusu. Przy konflikcie agent raportuje `POLICY_CONFLICT` lub `OWNER_GATE`.

## 4. Poziomy autonomii

- `DISABLED`: brak runów poza inspekcją stanu kontrolnego.
- `READ_ONLY`: A0 — odczyt, audyt, raport bez zapisu.
- `PLAN_ONLY`: A0 oraz plan/propozycja A1/A2; zero implementacji.
- `IMPLEMENT_SAFE`: A0, A1 i wyłącznie owner-approved `AGENT_READY` klasy A2.

Tylko właściciel może zmienić tryb na wyższy. Agent nie może modyfikować ani
pliku `CONTROL.yaml`, ani pola `mode` w control issue. Rozbieżność między nimi to
`POLICY_CONFLICT` i zero implementacji.

## 5. Klasy ryzyka

### A0 — odczyt, audyt, raport

Bez zmian w repo. Dozwolone w `READ_ONLY`, `PLAN_ONLY` i `IMPLEMENT_SAFE`.

### A1 — dokumentacja, testy, diagnostyka, drift check

Niskie ryzyko, bez zmiany zaakceptowanego zachowania. Może zostać wykonane tylko
w `IMPLEMENT_SAFE`, jeżeli work item i allowed paths jawnie na to pozwalają.

### A2 — ograniczona implementacja

Mały, izolowany zakres, jednoznaczne acceptance criteria, dokładny base SHA,
required tests i owner-approved status `AGENT_READY`. Tylko `IMPLEMENT_SAFE`.

### A3 — decyzja właściciela lub zmiana produktu/polityki

Obejmuje fizykę, feel, UX, domyślne wartości, realistic-vs-arcade, progi testów,
rozluźnianie walidacji, zmianę control plane, merge i zamykanie PR-ów. Nigdy nie
jest implementowane autonomicznie.

### A4 — zabronione/prywatne/ręczne

Prywatne skany i ścieżki, visual acceptance, ręczny runtime, Box3D `src/` i
`include/`, dane uwierzytelniające oraz operacje, których nie da się wiarygodnie
zwalidować. Zawsze STOP.

## 6. Kolejka pracy

`.automation/WORK_ITEMS.json` jest kontrolowaną kolejką. Każdy item musi przejść
ścisłą walidację i zawierać wszystkie pola ze schematu.

Stany:

- `PROPOSED`: pomysł do oceny, nieimplementowalny;
- `AGENT_READY`: owner-approved A2, jedyny implementowalny stan;
- `ACTIVE`: przypisany do potwierdzonego lease/runu;
- `BLOCKED`, `OWNER_NEEDED`, `VISUAL_REVIEW`: STOP;
- `DONE`, `REJECTED`: niewybieralne.

Agent nie może w tym samym runie dopisać work itemu, ustawić `AGENT_READY` i go
wykonać. Zmiana kolejki jest zmianą control plane klasy A3 i wymaga osobnego
brancha, draft PR oraz akceptacji właściciela.

Selekcja jest deterministyczna: zero lub jeden item, najpierw najniższe `priority`,
potem `id`. Dependencies muszą być `DONE`; konflikt z aktywnym itemem blokuje.

## 7. Lock/lease w control issue

Repo przechowuje politykę, ale zmienny lock znajduje się w jednym GitHub Issue.
Commitowany `LOCK.json` nie jest źródłem wyłączności.

Claim przed implementacją:

1. odczytaj dokładnie jeden otwarty control issue;
2. sprawdź `enabled`, `mode`, active campaign, authoritative branch/head i gates;
3. sprawdź brak aktywnego albo stale lease;
4. sprawdź brak innego automation PR;
5. zapisz własny `run_id`, exact base SHA, start i expiration;
6. ponownie odczytaj issue;
7. odczekaj configured settle interval i odczytaj drugi raz;
8. przed utworzeniem brancha i bezpośrednio przed pierwszym zapisem ponownie
   sprawdź, czy claim nadal należy do tego runu i base SHA się nie przesunął.

GitHub Issue edit nie daje atomowego compare-and-swap. Dlatego:

- niepotwierdzony claim => `LOCK_UNCERTAIN / NO_IMPLEMENTATION`;
- dwa widoczne aktywne claimy => `LOCK_UNCERTAIN`;
- unexpired lease innego runu => `ACTIVE_AGENT_DETECTED`;
- expired lease => `LOCK_UNAVAILABLE`; automat nie przejmuje go sam;
- właściciel ręcznie rozstrzyga stale lease po sprawdzeniu brancha/PR/logów.

Lease jest zwalniany tylko przez run, którego `active_run_id` nadal widnieje w
Issue. Przerwany run pozostawia diagnozowalny run ID, czas, branch i PR.

## 8. Branch i PR policy

Każdy work item zaczyna z ponownie sprawdzonego exact SHA i używa:

```text
automation/<work-item-id>/<run-id>
```

Zakazane:

- push do `main`, `jozz-vehicle-sandbox-m0`, aktywnego brancha kampanii lub cudzego brancha;
- aktualizacja istniejącego produktu PR bez jawnego przypisania;
- rebase cudzej historii, force-push, retargetowanie, merge, auto-merge;
- zamykanie PR-ów lub issue’ów produktu.

Jeden run tworzy najwyżej jeden draft PR. PR zawiera Run ID, Work item ID, mode,
risk, exact base SHA, allowed/changed files, acceptance criteria, tests, unavailable
tests, gates, limitations i uzasadnienie bezpieczeństwa.

Jeżeli base się przesunął, automat nie rebase’uje. Kończy `BASE_MOVED`.

## 9. Scope validation

`validate_scope.py` porównuje diff z itemem i odrzuca:

- pliki poza `allowed_paths`;
- `forbidden_paths` oraz globalne `src/**` i `include/**`;
- przekroczenie max files/changed lines;
- zmianę chronionego control plane;
- zmianę progów/tolerancji/limitów na threshold-sensitive paths;
- branch niespełniający formatu runu.

Mechaniczna walidacja nie potrafi dowieść semantycznego związku każdej linii z
zadaniem. Dlatego mały zakres, exact allowed paths, acceptance criteria i review
pozostają obowiązkowe. Przy wątpliwości wynik to `POLICY_CONFLICT`.

## 10. Testy i capability

Automat używa istniejących bramek, zwłaszcza `tools/gate.ps1`; nie kopiuje ich
logiki. Na pełnym Windows uruchamia właściwą gate.

W cloud/Linux uruchamia wyłącznie testy rzeczywiście dostępne. Brak Windows,
native runtime lub obrazu jest raportowany jako:

```text
PARTIAL_CLOUD_VALIDATION
LOCAL_WINDOWS_GATE_PENDING
```

Zielone CI nie zastępuje visual gate ani prywatnego owner flow.

Zmiana testu, progu, tolerancji lub acceptance criteria w celu uzyskania PASS jest
A3 i STOP. Test niedostępny nie jest testem zaliczonym.

## 11. No-op i STOP semantics

Poprawne końcowe wyniki bez implementacji:

- `NO_MATERIAL_CHANGE`
- `NO_SAFE_WORK`
- `ACTIVE_AGENT_DETECTED`
- `LOCK_UNAVAILABLE`
- `LOCK_UNCERTAIN`
- `OWNER_GATE`
- `VISUAL_GATE`
- `PRIVATE_DATA_REQUIRED`
- `CI_PENDING`
- `BASE_MOVED`
- `TASK_TOO_LARGE`
- `POLICY_CONFLICT`

Żaden z nich nie uruchamia zastępczego refaktoru, cleanupu ani funkcji.

## 12. Raportowanie

Stan runu trafia do control issue. Szczegółowy raport roboczy trafia do
`build/automation/latest-report.json`, który jest lokalny i niecommitowany.
Dokumentacja projektu jest aktualizowana tylko, gdy realny stan kampanii się
zmienił. Cykliczny brak zmian nie generuje commitów.

Prywatne ścieżki, współrzędne, hashe prywatnych źródeł i surowe dane nie trafiają
do Issue, PR, CI ani commitu. Raport używa redakcji i identyfikatorów logicznych.

## 13. Recovery

### Przerwany run

Sprawdź control issue, branch `automation/...`, draft PR i ostatni commit. Nie
kontynuuj automatycznie na cudzym branchu. Właściciel może zamknąć/odrzucić run,
wyczyścić lease lub jawnie utworzyć nowy work item recovery.

### Stale lock

Automat raportuje `LOCK_UNAVAILABLE`. Właściciel sprawdza, czy proces/branch/PR
nadal istnieje, po czym ręcznie czyści lease albo przedłuża go. Brak automatycznego takeover.

### Zmiana aktywnej kampanii

Właściciel aktualizuje control issue: campaign, authoritative branch/head i gates.
Następny run porównuje to z `AI_PROJECT_MEMORY.md` oraz domenowym current state.
Rozbieżność blokuje implementację do czasu synchronizacji dokumentów.

### Wyłączenie

Właściciel ustawia `enabled=false` i/lub `mode=DISABLED` w control issue oraz
oddzielnie zatwierdza odpowiadającą zmianę `CONTROL.yaml`, jeżeli ma być trwała.
Każda rozbieżność fail-closed.

## 14. Samomodyfikacja

Automat może wykryć wadę workflow i opisać ją w raporcie. Nie może w tym samym
runie zmieniać własnej polityki, locka, risk classification, cadence, scope,
test requirements ani merge policy.

Control-plane change wymaga osobnego work itemu A3, osobnego brancha, osobnego
draft PR i jawnej akceptacji właściciela. Produkt i reguły oceny produktu nie
mogą być zmieniane razem.
