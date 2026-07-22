# AI Project Memory — Box3d_FunProject

## Rola tego pliku

Ten plik jest globalnym routerem projektu. Wskazuje aktywną kampanię, jej dokument
stanu, branch integracyjny, aktywny PR, bieżące bramki i najbliższą granicę
produktu.

Nie jest:

- szczegółową dokumentacją subsystemu;
- historią wszystkich milestone'ów;
- kolejką pracy;
- źródłem mutable lease lub exact current SHA;
- zgodą na implementację albo merge.

## Kolejność autorytetu

1. GitHub Issue #11 `[AUTOMATION CONTROL] Box3d_FunProject recurring agent`;
2. `AGENTS.md` i `.automation/CONTROL.yaml`;
3. ten plik;
4. właściwy `docs/*/CURRENT_STATE.md`;
5. aktywny PR kampanii i jego remote head;
6. `docs/PROJECT_OPERATING_PLAN_PL.md`;
7. `README_FOR_AGENTS.md` jako podręcznik domeny pojazdu;
8. checkpointy, tech debt, subsystem docs, kod i testy.

Exact mutable `authoritative_head` zawsze odczytuj z Issue #11. Nie kopiuj go jako
rzekomo trwałego current SHA do kolejnych dokumentów: każdy nowy commit natychmiast
uczyniłby taki zapis nieaktualnym.

Historyczny checkpoint, stary roadmap ani zamknięty stacked PR nie wybiera pracy
samodzielnie.

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

PR #13 jest jedyną bieżącą powierzchnią integracyjną kampanii P0–R1B. Zachowuje
liniową historię wcześniejszych etapów bez rebase'u, squashowania ani force-pusha.

Historyczne PR-y #1–#5, #8 i #9 są zastąpione przez #13. Ich branche i commity
pozostają zachowane. PR #7 jest odrębną, rozbieżną linią surface-evidence; została
zaparkowana, nie odrzucona, w Issue #14 i nie należy do bieżącego preview path.

## Aktualny uczciwy stan produktu

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

Potwierdzone:

- real 7 GLB + 7 PLY inspection istnieje jako owner-private evidence;
- owner-confirmed source-frame contract istnieje prywatnie;
- real P1B bundle i privacy receipt istnieją prywatnie;
- R1B source resolution i owner-flow przeszły hosted contracts;
- native sample build przeszedł na integrowanej linii;
- recurring-agent foundation jest zintegrowany bez zmiany produktu;
- stack ma jedną konsolidacyjną powierzchnię review w PR #13.

Nieudowodnione:

- real preview pack z prywatnego zestawu siedmiu kafli;
- native runtime load dokładnie tego packa;
- owner visual review;
- same-revision restart;
- `TERRAIN_VISIBLE_PASS`.

Prywatne ścieżki, współrzędne, hashe źródeł i surowe skany pozostają poza
GitHubem i publicznymi logami.

## Najbliższa realna granica produktu

Następna realna akcja to jedno owner-local uruchomienie wspieranego runnera:

```text
run_real_terrain_flow.ps1
```

Runner ma odnaleźć dokładny historyczny bundle/receipt, rozwiązać siedem par
GLB/PLY, zbudować i zweryfikować preview pack, wybrać go i zatrzymać się na
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

CI, kompilacja, wygenerowany pack lub uruchomiony proces nie są visual proof.

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

W `PLAN_ONLY` operator może audytować authority, PR-y, CI, gate'y, lease i pustą
kolejkę. Może proponować ograniczony następny krok, ale nie tworzy product branch,
commitu ani PR-a i nie przełącza się na `IMPLEMENT_SAFE`.

Pełny workflow i roadmapa:

```text
docs/PROJECT_OPERATING_PLAN_PL.md
```

## Inne domeny projektu

### Vehicle sandbox

M7/M8 pozostaje zaakceptowanym stabilnym baseline'em, nie aktywną kampanią.
Podręcznik domeny: `README_FOR_AGENTS.md`.

### Map

Odrzucony sześciopasmowy Etap 2 i plan central-campus pozostają wstrzymane, dopóki
owner nie wybierze mapy jako osobnej kampanii.

### Surface evidence / collision

Dokładny parking state znajduje się w Issue #14. Nie reaktywuj go przed
`TERRAIN_VISIBLE_PASS` i osobną decyzją ownera. Accepted surface, collision i
drive readiness nie są częścią R1B.

## Hard capability boundaries

- `src/` i `include/` Box3D są poza autonomicznym zakresem;
- accepted physics, feel, UX i defaults wymagają decyzji ownera;
- A3/A4 nigdy nie są autonomiczne;
- visual/private evidence nie może być wyprowadzone z CI;
- bez zgody ownera nie ma merge, auto-merge, force-push, rebase, retarget ani
  zamykania PR-ów;
- nie zapisuj bezpośrednio na `main`, `jozz-vehicle-sandbox-m0` ani aktywny branch;
- nie używaj `Git_Diff_Patcher_Bridge`;
- recurring run nie zmienia własnego control plane.

## Branch i handoff rule

Manualna praca startuje z exact remote head na nowym izolowanym branchu, używa
małych logicznych commitów i kończy w draft PR. Dokumentację aktualizuj tylko,
gdy stan naprawdę się przesunął albo naprawiono konflikt autorytetu.

Po każdej owner-approved integracji Issue #11 musi wskazywać wynikowy branch i
pełny SHA, zanim scheduler uzna nowy stan za authoritative.