# Contributing to Box3d_FunProject

Ten dokument opisuje manualną pracę ownera i agentów nad forkiem Jozza. Nie
zastępuje `AGENTS.md`, Control Issue ani dokumentacji aktywnej domeny.

## 1. Przed rozpoczęciem

1. Przeczytaj `AGENTS.md` i `.automation/CONTROL.yaml`.
2. Odczytaj GitHub Control Issue i zapisz authoritative branch oraz pełny head SHA.
3. Przeczytaj `AI_PROJECT_MEMORY.md` i właściwy `docs/*/CURRENT_STATE.md`.
4. Sprawdź aktywne PR-y, pending CI, gate'y i możliwe nakładanie zakresu.
5. Sklasyfikuj pracę jako A0–A4.

Nie rozpoczynaj implementacji, jeżeli prawdziwym acceptance jest prywatny run,
visual review, decyzja feel/UX/default, zmiana polityki lub Box3D core.

## 2. Branch

Manualna praca:

```text
agent/<campaign>-<short-scope>-vN
```

Recurring agent używa wyłącznie:

```text
automation/<work-item-id>/<run-id>
```

Branch musi powstać z exact remote SHA. Nie używaj lokalnego brancha bez ponownego
porównania z remote.

Zakazane bez jawnej zgody ownera:

- direct push do `main`, `jozz-vehicle-sandbox-m0` lub aktywnej kampanii;
- force-push, rebase cudzej historii i retarget;
- merge lub auto-merge;
- zamykanie cudzych PR-ów;
- branch deletion;
- przypadkowa zmiana `src/` albo `include/`.

## 3. Zakres

Jeden PR powinien mieć jeden czytelny cel. Przed zapisem określ:

- exact base SHA;
- dozwolone i zabronione ścieżki;
- acceptance criteria;
- wymagane testy;
- capability, której nie da się uruchomić;
- owner/visual/private gate'y;
- warunki STOP.

Jeżeli zadanie rozrosło się do kilku niezależnych subsystemów, rozbij je przed
implementacją. Nie ukrywaj zmiany zachowania jako cleanupu.

## 4. Commity

Commity powinny być logiczne i możliwe do osobnego zrozumienia. Preferowany format:

```text
<SCOPE-ID>: krótki opis wyniku
```

Dokumentacja opisuje wyłącznie stan udowodniony. Nie commituj cyklicznych raportów,
prywatnych rezultatów ani chwilowych build outputs.

## 5. Walidacja

Minimalna walidacja governance:

```powershell
python tools/automation/validate_control.py
python tools/project/repository_audit.py
python -m unittest discover -s tests/automation -p "test_*.py"
python -m unittest discover -s tests/project -p "test_*.py"
```

Scan pipeline:

```powershell
python tools/scan_pipeline/run_p1_contracts.py
```

Pełna bramka Windows:

```powershell
.\tools\gate.ps1
```

W PR podaj dokładnie, co wykonano i czego nie dało się wykonać. `NOT_RUN` i
`UNAVAILABLE` nie są PASS.

## 6. Evidence discipline

- Reproduce before fix.
- Build success nie jest render proof.
- Process exit nie jest owner acceptance.
- CI nie zna prywatnych danych.
- Test threshold nie może zostać rozluźniony dla uzyskania green.
- Zmiana validator output, screenshotu, defaultu lub feel nie jest move-only.

## 7. Prywatność

Nie umieszczaj w GitHubie:

- prywatnych ścieżek i nazw katalogów ownera;
- współrzędnych i prywatnego georeferencing;
- hashy prywatnych źródeł, jeśli mogą identyfikować zestaw;
- raw GLB/PLY, receipts private-only lub resumable state;
- credentials i tokenów.

Używaj logicznych ID i redakcji. Prywatne outputy pozostają pod ignorowanym
`build/`.

## 8. Pull request

Manualny PR korzysta z `.github/PULL_REQUEST_TEMPLATE/manual.md`. Musi zawierać:

- purpose i non-goals;
- base branch/SHA oraz head branch/SHA;
- changed paths i diff budget;
- testy, evidence i ograniczenia;
- owner/visual/private gates;
- migration/rollback;
- listę potwierdzonych non-actions.

PR pozostaje draftem, dopóki wymagane testy i review nie są uczciwie opisane. Draft
nie jest zgodą na merge.

## 9. Dokumentacja

Używaj `docs/README.md` jako indeksu. Nowy długi dokument powstaje tylko dla:

- current state nowej domeny;
- decyzji architektonicznej;
- realnego milestone'u;
- trwałego subsystem contract;
- jawnie potrzebnego planu kampanii.

Nie twórz kolejnego roadmapu, jeżeli istniejący `PROJECT_OPERATING_PLAN_PL.md` może
zostać jednoznacznie zaktualizowany.

## 10. Definition of Done

Zmiana jest gotowa do review, gdy:

- diff mieści się w zadeklarowanym zakresie;
- acceptance criteria są spełnione bez rozluźnienia bramek;
- wszystkie dostępne testy przeszły;
- niedostępne capability są jawne;
- prywatność została sprawdzona;
- dokumenty mówią wyłącznie prawdę;
- PR jest draftem i nie wykonano merge/rebase/force-push/retarget.

## Upstream contributions

Ten fork pozostaje oparty na `erincatto/box3d`. Zmiany przeznaczone dla upstreamu
mogą podlegać innym zasadom, CLA i branch modelowi upstream repository. Nie mieszaj
workflowu tego forka z workflowem upstream bez osobnej decyzji ownera.
