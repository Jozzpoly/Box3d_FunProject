> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# Project direction — Jozz Vehicle Box3D Native

Date: 2026-07-03  
Status: active direction, corrected after M2.5 sample-host path

## Werdykt

Startujemy nową grę/sandbox pojazdów na fundamencie Box3D, ale robimy to małymi, twardymi krokami. Nie zaczynamy od pełnego buildera, pełnego renderera glTF ani fizycznie dokładnego zawieszenia wielowahaczowego.

Najpierw ma powstać **Jozz Vehicle Lab**: miejsce do testowania fizyki narożnika zawieszenia, koła, później asset-derived dimensions i visual-only attachment modeli.

## Korekta po M2.5: sample host zamiast osobnego executable na teraz

Pierwotny kierunek zakładał szybkie stworzenie osobnego `jozz_vehicle_lab` executable.

Po praktycznej pracy M1/M2/M2.5 aktualna rzeczywistość jest inna i lepsza dla tego etapu:

```text
Jozz Vehicle Lab działa teraz w istniejącym Box3D samples host.
```

To nie jest cofnięcie celu projektu. To pragmatyczny krok: sample host już daje okno, kamerę, ImGui, debug draw, input i integrację CMake. Dzięki temu praca mogła skupić się na właściwym problemie: czy `b3WheelJoint` daje sensowny fundament dla jednego narożnika zawieszenia.

Osobny executable może wrócić później, ale nie jest obecnie blokadą. Nie wolno zaczynać od nowa tylko dlatego, że starszy dokument mówił o własnym executable jako najbliższym kroku.

## Feedback Jozza z 2026-07-03

Jozz przyjął, że orientację modeli w Blockbenchu dostosuje później, dopiero kiedy modele będą widoczne w grze. To jest ważna decyzja projektowa.

Konsekwencja:

- nie blokujemy M0/M1/M2 na finalnym ustawianiu osi w Blockbenchu;
- importer/kontrakt assetu musi pozwalać na tymczasowe korekty orientacji per asset;
- finalna orientacja authoringowa stanie się osobną pracą po uruchomieniu podglądu modeli w grze;
- kod fizyki nie może mieć porozrzucanych magicznych obrotów per model.

## Aktualny milestone baseline

Aktualny baseline to:

```text
Jozz Vehicle / Lab M2 Primitive Corner
Panel: Jozz Vehicle Lab M2.5
```

M2.5 potwierdza:

- primitive one-corner wheel-joint lab;
- centered wheel pivot;
- poprawny rest-anchor model dla `b3WheelJoint`;
- rest drop;
- rebound/compression travel;
- collision on/off;
- live root stress mover przez slider oraz Q/E;
- rozdział pending structural setup od runtime live root controls.

## Kierunek techniczny

Trzy warstwy muszą pozostać rozdzielone:

```text
Authoring asset  -> glTF + sidecar metadata
Game asset       -> render mesh + visual rig + material data
Physics prefab   -> bodies + shapes + joints + tuning parameters
```

Nie wolno traktować jednego glTF jako naraz:

- finalnego render mesha;
- fizycznej kolizji;
- rigu proceduralnego;
- prefab systemu;
- gameplayowego kontraktu montażu.

## Najważniejsze ryzyka

### 1. Zbyt szybkie wejście w renderer glTF

Renderer modeli jest potrzebny, ale może łatwo zjeść kilka dni zanim powstanie jakikolwiek feel jazdy. Dlatego aktualny lab fizyczny nadal używa primitive renderingu/debug draw.

### 2. Zbyt skomplikowane zawieszenie za wcześnie

Multi-body suspension brzmi kusząco, bo pasuje do modelu, ale na start grozi niestabilnością, trudnym tuningiem i brakiem szybkiego feedbacku. V0 powinno użyć jednego wheel jointa na narożnik.

### 3. Zgadywanie po nazwach node'ów

Aktualne glTF-y mają duplicate root/node names. Importer musi używać node index/path/parent chain i composed transforms, a nazwa jest tylko semantycznym hintem.

### 4. Skala i orientacja

Skala musi być jawna w sidecar `.asset.json`. Orientacja authoringowa pozostaje tymczasowo "not final", ale runtime/game-space musi mieć jedną spójną konwencję.

### 5. Powrót do błędnego modelu M2.3

Nie wolno traktować wizualnego chassis/damper mountu jako body-A frame dla `b3WheelJoint`.

Aktualna reguła:

```text
Frame A = rest wheel-center anchor na chassis
Frame B = centrum koła
spring rest = translation 0
```

## Aktualna rekomendowana kolejność

1. Seed branch z dokumentami, assetami, kontraktami i audytem. — zrobione.
2. Minimalny native lab w Box3D sample host. — zrobione przez M1/M2.
3. Primitive physics smoke test. — zrobione przez M1.
4. Single wheel-corner physics lab. — M2.5 zwalidowane ręcznie.
5. Foundation Grounding Phase. — aktualnie wykonywane.
6. M3A: asset-derived primitive dimensions.
7. M3B: pierwszy visual-only glTF wheel mesh attachment.
8. Visual rig wheel/suspension/damper/cardan.
9. Dopiero potem pełny vehicle assembly.

## Aktualna zasada decyzyjna

Jeżeli następny krok wymaga wyboru między szybkim efektem wizualnym a zachowaniem stabilnego baseline'u fizyki, wybierz baseline.

Najpierw projekt ma być trudny do zepsucia, dopiero potem efektowny.