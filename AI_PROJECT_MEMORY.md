# AI Project Memory — Box3d_FunProject

## Rola tego pliku

Ten plik jest globalnym routerem aktualnego stanu projektu. Wskazuje aktywną
kampanię, jej domenowy `CURRENT_STATE.md`, aktywny branch/PR i najważniejsze STOP
gates. Nie jest szczegółową dokumentacją subsystemu, historią milestone'ów ani
kolejką zadań automatyzacji.

Hierarchia dla przyszłych agentów:

1. właścicielski GitHub Issue `[AUTOMATION CONTROL] Box3d_FunProject recurring agent`;
2. ten plik jako wybór kampanii;
3. domenowy `docs/*/CURRENT_STATE.md`;
4. aktualny PR kampanii i jego exact head SHA;
5. `README_FOR_AGENTS.md` jako globalne reguły produktu;
6. checkpointy, tech debt, subsystem docs, kod i testy.

Historyczne checkpointy nie mogą samodzielnie wybrać aktywnego zadania.

## Aktywna kampania produktu

Cel: pokazać siedem rzeczywistych kafli fotogrametrycznych GLB w natywnym,
render-only preview i uzyskać uczciwy owner visual review orientacji, skali, osi
up, mirror state, coverage i seams.

- domenowy stan: `docs/scan_import/CURRENT_STATE.md`;
- aktywny branch: `agent/r1b-source-resolution-owner-integration`;
- aktywny draft PR: #9;
- exact authoritative head: `3a0d63e700108155886e1e00df7293f9c3d52db7`;
- base kampanii: exact PR #5 head `f20357ba10618ddecfdd2e274e93917fe508a983`;
- PR #7 pozostaje zamrożony do exact visual proof.

## Aktualny stan prywatny zgłoszony przez właściciela

- real 7 GLB + 7 PLY inspection: passed;
- real owner-confirmed source frame: passed;
- real P1B bundle and privacy acknowledgement: passed;
- real preview pack: not yet produced;
- native load and visual proof: not yet performed.

Prywatne ścieżki, współrzędne, hashe źródeł i surowe skany pozostają poza
GitHubem i publicznymi logami.

## Najbliższa realna granica produktu

Kod R1B rozwiązuje wcześniejszy blocker filesystem-layout i ma zieloną hosted
walidację. Pozostaje nieautomatyzowalny owner-local run na prywatnych danych,
natywne uruchomienie oraz visual review. Uczciwy status maksymalny przed tymi
bramkami to:

```text
REAL_PREVIEW_PIPELINE_CODE_READY
```

Nigdy nie wyprowadzaj `TERRAIN_VISIBLE_PASS` z samego CI, kompilacji, bundle'a,
preview packa ani uruchomionego executable.

## Równoległa kampania infrastrukturalna

Przygotowanie przyszłej cyklicznej pętli agentowej odbywa się wyłącznie na:

- branch: `agent/autonomous-loop-foundation-v1`;
- draft PR: #10;
- control issue: #11;
- szczegółowa polityka: `.automation/POLICY.md`;
- maszynowy kontrakt: `.automation/CONTROL.yaml`;
- operacyjna mapa: `AGENTS.md`.

Ta kampania nie zmienia produktu. Domyślny tryb pozostaje `PLAN_ONLY`, control
issue ma `enabled=false`, a żaden harmonogram nie został utworzony.

## Reguła właścicielskiego workflow skanów

Właściciel nie jest manualnym orkiestratorem technicznej sekwencji. R1B zapisuje
bindingi, hashe i ścieżki prywatnie przez `scan_real_terrain_flow.py`, a wspieranym
wejściem jest `run_real_terrain_flow.ps1`. Właściciel wykonuje tylko niezbędne
wskazanie source root przy pierwszym runie i rzeczywistą ocenę wizualną.

## Hard capability boundaries

- accepted surface, collision i drive readiness nie są częścią R1B;
- A3/A4 nie są autonomicznie implementowane;
- merge, zamykanie PR-ów, force-push i retargetowanie wymagają właściciela;
- `src/` i `include/` Box3D pozostają poza autonomicznym zakresem;
- przyszły agent nie może zmieniać własnego control plane w tym samym runie.

## Branch rules

- nigdy nie merge'uj ani nie zamykaj PR bez zgody właściciela;
- nigdy nie force-pushuj ani nie przepisuj istniejącej historii;
- nie zapisuj bezpośrednio na `main`, `jozz-vehicle-sandbox-m0` ani aktywny branch kampanii;
- nie używaj Git_Diff_Patcher_Bridge;
- operacje repozytorium wykonuj przez bezpieczne, jawne operacje GitHub.
