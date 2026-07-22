# AI Project Memory — Box3d_FunProject

## Rola tego pliku

Ten plik jest globalnym routerem bieżącego projektu. Wskazuje aktywną kampanię,
matching current state, produktowy branch/PR, gate'y, równoległe warstwy governance
i najbliższą realną granicę.

Nie jest:

- twardą polityką — ta zaczyna się w `AGENTS.md` i `.automation/CONTROL.yaml`;
- szczegółową dokumentacją subsystemu;
- historią milestone'ów;
- work-item queue;
- źródłem mutable lease lub exact current SHA;
- zgodą na implementację albo merge.

## Policy i kolejność faktów

Najpierw odczytaj twardą politykę:

1. `AGENTS.md`;
2. `.automation/CONTROL.yaml`;
3. `.automation/POLICY.md`.

Następnie rozwiąż mutable state i fakty:

1. GitHub Issue #11 `[AUTOMATION CONTROL] Box3d_FunProject recurring agent`;
2. ten plik;
3. właściwy `docs/*/CURRENT_STATE.md`;
4. aktywny PR kampanii i jego remote head;
5. `docs/PROJECT_OPERATING_PLAN_PL.md`;
6. właściwy podręcznik domeny, np. `README_FOR_AGENTS.md` dla pojazdu;
7. checkpointy, tech debt, subsystem docs, kod i testy.

Exact mutable `authoritative_head` zawsze odczytuj z Issue #11. Nie kopiuj go jako
trwałego current SHA do wersjonowanego dokumentu. Historyczny checkpoint, stary
roadmap ani zamknięty PR nie wybiera pracy.

## Aktywna kampania produktu

```text
campaign:             scan-terrain-r1b
goal:                 real seven-tile native render-only preview
state document:       docs/scan_import/CURRENT_STATE.md
authoritative branch: agent/scan-terrain-r1b-consolidated-integration
active draft PR:      #13
exact current head:   GitHub Control Issue #11
integration base:     jozz-vehicle-sandbox-m0
```

PR #13 jest jedyną bieżącą powierzchnią **produktowej integracji** P0–R1B. Zachowuje
liniową historię wcześniejszych etapów bez rebase'u, squashowania ani force-pusha.

Historyczne PR-y #1–#5, #8 i #9 są zastąpione przez #13, ale ich branche i commity
pozostają zachowane. PR #7 jest rozbieżną linią surface-evidence, zaparkowaną w
Issue #14 i wyłączoną z bieżącego preview path.

## Warstwa gotowości repozytorium

```text
repository readiness PR: #15
branch:                  agent/r1b-repository-readiness-polish-v1
base:                    active product integration PR #13
state:                   DRAFT_UNDER_REVIEW
product capability:      UNCHANGED
```

PR #15 porządkuje globalną politykę, front door repo, contribution workflow,
dokumentację, ownership map, mechaniczny authority audit i routing CI.

Nie jest jeszcze authoritative i nie zmienia Issue #11. Recurring scheduler nadal
czyta zwalidowany head PR #13, dopóki governance layer nie zostanie osobno
zreviewowany i owner-approved zintegrowany. To zapobiega kierowaniu automatu na
niezatwierdzoną zmianę własnej polityki.

## Aktualny uczciwy stan produktu

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

Potwierdzone:

- real 7 GLB + 7 PLY inspection istnieje jako owner-private evidence;
- owner-confirmed source-frame contract istnieje prywatnie;
- real P1B bundle i privacy receipt istnieją prywatnie;
- R1B source resolution i owner flow przeszły hosted contracts;
- native sample build przeszedł na integrowanej linii;
- recurring-agent foundation jest zintegrowany bez zmiany produktu;
- stack ma jedną produktową powierzchnię review w PR #13;
- repository-readiness layer jest odseparowany w draft PR #15.

Nieudowodnione:

- real preview pack z prywatnego zestawu siedmiu kafli;
- native runtime load dokładnie tego packa;
- owner visual review;
- same-revision restart;
- `TERRAIN_VISIBLE_PASS`.

Prywatne ścieżki, współrzędne, hashe źródeł i surowe skany pozostają poza
GitHubem i publicznymi logami.

## Najbliższa realna granica produktu

Następna realna akcja to jedno owner-local uruchomienie:

```text
run_real_terrain_flow.ps1
```

Runner ma odnaleźć exact historical bundle/receipt, rozwiązać siedem par GLB/PLY,
zbudować i niezależnie zweryfikować preview pack, wybrać go i zatrzymać się na
visual review.

Owner podaje wyłącznie nieunikniony prywatny source root oraz wykonuje rzeczywistą
decyzję wizualną. Nie jest ręcznym orkiestratorem technicznej sekwencji.

Oczekiwany stan po poprawnym prywatnym runie:

```text
REAL_PREVIEW_PACK_READY / VISUAL_REVIEW_PENDING
```

Tylko jawna ocena orientacji, skali, osi, mirror state, coverage, seams i
same-revision restart może ustanowić:

```text
TERRAIN_VISIBLE_PASS
```

CI, kompilacja, wygenerowany pack, governance polish lub uruchomiony proces nie są
visual proof.

## Current gates

```text
owner gate:    explicit decision at visual review
private gate:  OWNER_LOCAL_REAL_SCAN_RUN_REQUIRED
visual gate:   REAL_TERRAIN_PREVIEW_REVIEW_PENDING
```

Nie czyść ani nie reinterpretuj gate'ów tylko po to, aby wykazać aktywność.

## Recurring operator

```text
control issue: #11
enabled:       true
mode:          PLAN_ONLY
cadence:       every 6 hours, Europe/Warsaw
active lease:  read from Issue #11
```

W `PLAN_ONLY` operator może audytować authority, PR-y, CI, gate'y, lease i kolejkę.
Może proponować ograniczony następny krok, ale nie tworzy product branch, commitu
ani PR-a i nie przełącza się na `IMPLEMENT_SAFE`.

PR #15 jest manualną owner-directed zmianą A3. Recurring agent może ją zauważyć i
raportować, ale nie może sam wdrażać, modyfikować ani czynić authoritative.

Pełny workflow i roadmapa:

```text
docs/PROJECT_OPERATING_PLAN_PL.md
```

Indeks i ownership repo:

```text
docs/README.md
docs/REPOSITORY_STRUCTURE_PL.md
```

## Inne domeny projektu

### Vehicle sandbox

M7/M8 pozostaje zaakceptowanym stabilnym baseline'em, nie aktywną kampanią.
Podręcznik domeny: `README_FOR_AGENTS.md`.

### Map

Odrzucony sześciopasmowy Etap 2 i central-campus pozostają wstrzymane, dopóki owner
nie wybierze mapy jako osobnej kampanii.

### Surface evidence / collision

Dokładny parking state znajduje się w Issue #14. Nie reaktywuj go przed
`TERRAIN_VISIBLE_PASS` i osobną decyzją ownera. Accepted surface, collision i drive
readiness nie są częścią R1B.

## Hard capability boundaries

- `src/` i `include/` Box3D są poza autonomicznym zakresem;
- accepted physics, feel, UX i defaults wymagają decyzji ownera;
- A3/A4 nigdy nie są autonomiczne;
- visual/private evidence nie może być wyprowadzone z CI;
- bez zgody ownera nie ma merge, auto-merge, force-push, rebase, retarget, zamykania
  PR-ów ani branch deletion;
- nie zapisuj bezpośrednio na `main`, `jozz-vehicle-sandbox-m0` ani aktywny branch;
- nie używaj `Git_Diff_Patcher_Bridge`;
- recurring run nie zmienia własnego control plane.

## Branch i handoff rule

Manualna praca startuje z exact remote head na nowym izolowanym branchu, ma jeden
czytelny zakres i kończy w draft PR. Dokumentację aktualizuj tylko, gdy stan naprawdę
się przesunął albo naprawiono realny konflikt autorytetu.

Po owner-approved integracji Issue #11 musi wskazywać wynikowy branch i pełny SHA,
zanim scheduler uzna nowe pliki za authoritative. Nie aktualizuj Issue na head
niezreviewowanego governance PR tylko po to, aby scheduler zobaczył go wcześniej.
