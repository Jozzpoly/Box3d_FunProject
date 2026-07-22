# Rozgrzewka pod edytor rigu — completion record

**Pierwotna data:** 2026-07-11  
**Status obecny:** `COMPLETED_RESEARCH_WARMUP / HISTORICAL_EVIDENCE`  
**Nie jest:** aktywnym trackiem ani zgodą na budowę pełnego edytora.

Kanoniczne przyszłe wymagania zostały skondensowane w:

```text
docs/EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md
```

Accepted vehicle state po finalizacji:

```text
README_FOR_AGENTS.md
docs/CURRENT_STATE_INDEX_PL.md
docs/PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md
```

## 1. Cel historycznej rozgrzewki

Zamiast projektować pełny edytor „na sucho”, nowy model
`OneSided_Steering_Suspension_Rig` został związany z żywymi ciałami jezdnego M6.
Celem było jednocześnie:

- zobaczyć nowy rig na realnym pojeździe;
- odkryć, jakich bindingów potrzebuje przyszły edytor;
- nie zmienić accepted physics;
- traktować render i owner test jako bramkę.

Owner decision D1 została rozstrzygnięta:

```text
przód = nowy rig kierowniczy
tył   = dotychczasowy mount
```

## 2. Wynik G0/G1

Zrealizowano:

- audyt bieżącego języka rigowania;
- import nowego modelu na przednią oś M6;
- przypisanie części do realnych ciał;
- rozdział `WheelCenter` i `ChassisMount_b`;
- mirror L/P;
- render-only toggle podczas warm-up;
- owner test skrętu A/D;
- potwierdzenie, że visual work nie zmieniło validator numbers.

Najważniejsza lekcja:

> Rodzic części ma znaczenie kinematyczne. Nie jest kosmetycznym folderem.

Realny M6 nie ma uproszczonego „carriera” z benchu M9. Części wymagają wyboru spośród
rzeczywistych ciał: chassis, knuckle, upper/lower arm, wheel i rack.

## 3. Wynik G3

Zrealizowano:

- drążek od środka realnego racka do knuckla;
- lewy i prawy drążek łączące się w centrum zgodnie z decyzją Jozza;
- dumper między górnym socketem chassis a dolnym socketem niesionym przez ramię;
- owner render review;
- zachowanie niezmienionej fizyki.

Rozgrzewka ujawniła co najmniej cztery typy bindingu:

```text
rigid-to-one-parent
stretch-between-two-anchors
anchor-at-sub-position-on-another-body
socket-carried-by-a-stretch-part-pose
```

## 4. Finalizacja do accepted vehicle

Po warm-up powstał osobny plan finalizacji. Zakończył on:

- domyślny przedni rig kierowniczy;
- domyślną ramę rurową;
- persistence identity/offset/modelu;
- presety;
- opt-in ukrywanie/pokazywanie bryły kolizyjnej chassis.

Warm-up nie jest już alternatywną aktywną ścieżką implementacji.

## 5. Jawny pozostały dług

Dynamiczny rozdział części jest semantycznie poprawny, ale wizualnie może być „zbyt
osobny”: sąsiednie części rozchodzą się przy skręcie za mocno.

To kandydat do pierwszego praktycznego testu przyszłego edytora:

- pivot per część;
- parent binding;
- local binding transform;
- authored socket correction.

Nie naprawiać przez sklejanie części ani zmianę accepted physics.

## 6. Co pozostało opcjonalne lub przyszłe

```text
G4 cardan integration             OPTIONAL / NOT A BLOCKER
full rig editor                   PARKED / OWNER_DECISION_REQUIRED
in-game model importer            PARKED / separate architecture
Blockbench collision source       PARKED / PHYSICS_CHANGE
single-source socket/binding data KNOWN_DEBT
```

O8 — model importowany jako źródło kolizji — jest wymaganiem ownera dla przyszłego
narzędzia, ale nie został zalegalizowany przez warm-up. Wymaga osobnej reprezentacji,
walidacji hull/mesh, limitów Box3D, podglądu i owner gate’u.

## 7. Lifecycle

Nie uruchamiaj ponownie G0/G1/G3. Każdy przyszły krok edytora musi zacząć się od:

```text
explicit owner campaign activation
→ boundary with JES
→ exact accepted vehicle baseline
→ bounded editor/importer brief
→ no physics change unless separately authorized
```
