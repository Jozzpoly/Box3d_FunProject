# Project direction — historyczny origin record Jozz Vehicle

**Pierwotna data:** 2026-07-03  
**Status obecny:** `HISTORICAL_EVIDENCE / SUPERSEDED_AS_CURRENT_ROADMAP`  
**Nie jest:** current state, active direction, task queue ani zgodą na rozpoczęcie M3/M4.

Aktualna trwała wizja projektu znajduje się w:

```text
docs/PROJECT_CHARTER_PL.md
```

Aktualna kolejność pracy i exact authority znajdują się w:

```text
AGENTS.md
→ GitHub Control Issue #11
→ AI_PROJECT_MEMORY.md
→ właściwy current state i aktywny PR
→ docs/PROJECT_OPERATING_PLAN_PL.md
```

## 1. Dlaczego ten dokument pozostaje

Ten zapis dokumentuje moment, w którym projekt przestał być abstrakcyjnym planem
„pełnej gry” i zaczął rosnąć przez małe fizyczne laboratoria w istniejącym Box3D
sample host. To ważna część duszy projektu, ale jego milestone’y M2.5/M3A/M3B zostały
wielokrotnie przekroczone.

## 2. Historyczna decyzja: sample host jako pragmatyczne laboratorium

Pierwotny plan zakładał szybkie stworzenie osobnego executable. Praktyka M1/M2/M2.5
pokazała, że istniejący sample host dawał od razu:

- natywne okno;
- kamerę i input;
- ImGui;
- debug draw;
- integrację CMake;
- szybki dostęp do prawdziwej fizyki Box3D.

Decyzja brzmiała:

```text
najpierw udowodnić fizykę i feel w produktywnym laboratorium
zamiast zaczynać od budowania całej infrastruktury gry
```

Ta zasada pozostaje aktualna. Nie oznacza jednak, że sample host jest finalną
architekturą produktu albo automatycznie przechodzi do JES.

## 3. Historyczna decyzja ownera o authoringu modeli

Jozz świadomie nie blokował pierwszych etapów na idealnej orientacji modeli w
Blockbenchu. Najpierw modele miały stać się widoczne w grze, a dopiero potem authoring
miał zostać poprawiony na podstawie realnego feedbacku.

Trwałe konsekwencje:

- tymczasowa korekta renderowa per asset może istnieć jawnie;
- finalne authored axes nie powinny być zgadywane bez render review;
- magiczne obroty nie mogą rozlewać się po kodzie fizyki;
- visual transform nie staje się automatycznie physics frame.

## 4. Trwały rozdział warstw vehicle

```text
Authoring asset  → glTF + sidecar metadata
Game visual      → render mesh + visual rig + material data
Physics prefab   → bodies + shapes + joints + tuning parameters
```

Jeden glTF nie jest automatycznie jednocześnie:

- finalnym render meshem;
- kolizją;
- proceduralnym rigem;
- physics prefabem;
- gameplayowym kontraktem montażu.

Ta lekcja później stała się częścią szerszego rozdziału warstw prawdy świata w
`PROJECT_CHARTER_PL.md`.

## 5. Trwały rest-anchor model

Historyczny M2.5 ustalił zasadę, której nie wolno cofać:

```text
Frame A = rest wheel-center anchor na chassis
Frame B = centrum koła
spring rest = translation 0
rest drop = jawna decyzja konfiguracji
```

Wizualny damper/chassis mount nie jest automatycznie frame’em `b3WheelJoint`.
Historyczny model M2.3 był błędny.

## 6. Co wydarzyło się później

Po tym dokumencie projekt osiągnął między innymi:

- asset-derived visuals i kontrakty;
- pierwszy jeżdżący pojazd;
- M7 honest real-forces suspension/steering foundation;
- M8 rig, poza, persistence, presety i owner UI;
- syntetyczny engineering world;
- realny scan inspection/evidence pipeline;
- seven-tile native geometry preview i `TERRAIN_VISIBLE_PASS`;
- repository governance oraz project re-foundation.

Dlatego dawna kolejność M3A → M3B → visual rig → full vehicle assembly jest wyłącznie
historycznym śladem rozwoju, nie aktualną roadmapą.

## 7. Niezmienna zasada decyzyjna

Jedno zdanie z pierwotnego dokumentu pozostaje w pełni aktualne:

> Gdy szybki efekt wizualny konkuruje z zachowaniem wiarygodnego fundamentu, najpierw
> chronimy fundament — ale nie używamy tej zasady jako wymówki przed zbudowaniem małego,
> grywalnego proofu.

Obecnym małym proofem jest teksturowany realny teren, zaakceptowany samochód jako
referencja skali, następnie ograniczona kolizja i pierwszy owner fun verdict.
