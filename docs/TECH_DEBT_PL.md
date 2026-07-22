# TECH_DEBT — Jozz Vehicle Box3D Native

**Rola:** rejestr znanego długu i świadomie odłożonej pracy domeny vehicle.  
**Nie jest:** globalną polityką agentów, current state projektu, task queue ani zgodą
na cleanup/implementację.

Globalny routing i zasady Git pochodzą z:

```text
AGENTS.md
→ GitHub Control Issue #11
→ AI_PROJECT_MEMORY.md
→ właściwy current state i aktywny PR
```

Szczegółowe historyczne śledztwa pozostają w historii Git, checkpointach i dokumentach
subsystemowych. Ten plik utrzymuje tylko nadal istotne konsekwencje.

## 0. Korekta dawnych zasad operacyjnych

Historyczny wpis z 2026-07-08 mówił, że agenci mogą bezpośrednio commitować i pushować
na `jozz-vehicle-sandbox-m0` po zielonej bramce. Ta reguła została **zastąpiona** przez
obecną politykę repozytorium:

```text
exact remote SHA
→ nowy izolowany branch
→ bounded change
→ pełna właściwa walidacja
→ draft PR
→ owner review
```

Nie zapisuj bezpośrednio na `main`, `jozz-vehicle-sandbox-m0` ani aktywnym branchu
kampanii. `README_FOR_AGENTS.md` jest manualem domeny vehicle, a nie globalnym front
doorem. Globalnym wejściem jest `AGENTS.md`.

To sprostowanie nie usuwa historycznego faktu, że M7/M8 zostały w 2026-07-08 uratowane
z tygodnia niezacommitowanej pracy. Zmienia wyłącznie obowiązującą procedurę przyszłych
zmian.

## Legenda

```text
OPEN              realny dług wymagający osobnej decyzji lub kampanii
ACCEPTED_LIMIT     znane ograniczenie zaakceptowane przez Jozza
WATCH              obserwować; nie refaktoryzować bez konkretnego powodu
RESOLVED           zamknięte; pozostaje jako lekcja/regression guard
FUTURE_SCOPE       roadmapa, nie bug
```

## V-01 · Dokumentacja i authority drift

**Status:** `RESOLVED_WITH_GUARDS`

Historycznie vehicle docs spóźniały się o kamień, a `README_FOR_AGENTS.md` i
`CURRENT_STATE_INDEX_PL.md` konkurowały o rolę front door. Obecny model rozdziela:

- `AGENTS.md` — globalna polityka;
- `AI_PROJECT_MEMORY.md` — router aktywnej kampanii;
- `README_FOR_AGENTS.md` — zaakceptowane reguły vehicle;
- `CURRENT_STATE_INDEX_PL.md` — szczegółowy vehicle milestone ledger;
- ten plik — debt registry.

**Guard:** `tools/project/repository_audit.py` oraz project tests. Nie twórz drugiej
globalnej roadmapy ani drugiego current state.

## V-02 · UI/presety/persistence miały brakujący kontrakt

**Status:** `RESOLVED`

System sesji, presetów i debug-session jest opisany w:

```text
docs/SUBSYSTEM_UI_PRESETS_PL.md
```

Przebudowa UI, stabilne ImGui IDs, zapis sesji i presetów nie są już zależne od
prywatnej pamięci jednego agenta.

## V-03 · Pozostałe env-hooki i rusztowania diagnostyczne

**Status:** `WATCH`

Stabilne narzędzia `JOZZ_M6_*` są częścią owner/debug workflow. Jednorazowe sondy,
takie jak historyczne `JOZZ_M6_DIRTY_AT_FRAME` i `JOZZ_M6_TEST_RESET_MODAL`, mogą
pozostać nieszkodliwe, ale nie wolno ich mylić z product API.

**Reactivation trigger:** konkretna potrzeba cleanupu lub konflikt przy następnym
rozszerzeniu labu. Wymagana osobna mała zmiana, build, validator i smoke.

## V-04 · Droop i over-center obecnej geometrii kierownicy

**Status:** `ACCEPTED_LIMIT`

Agresywne opadanie wahaczy powyżej około 16° może przeprowadzić trapez kierowniczy
przez niebezpieczny obszar over-center. Zweryfikowany bezpieczny sufit obecnej
geometrii to około 15°; UI klamruje zakres z ostrzeżeniem.

Pełniejsza poza wymaga współprojektowania:

- `steeringArmBack`;
- `ackermannFraction`;
- bump-steer lift;
- rack stroke/dead-point fences.

**Guard:** nie ufać samemu `OK`; czytać drukowane kąty i uruchamiać właściwe sondy po
każdej zmianie. Szczegóły: `docs/M8_SUSPENSION_RIG_REPAIR_PLAN_PL.md`.

## V-05 · Pasek „Zastosuj” może zniknąć poza viewportem

**Status:** `OPEN_LOW_UX`

Przy niskim ekranie i rozwiniętych sekcjach cały prawy panel scrolluje się, więc
przycisk Apply może znaleźć się poza widokiem.

**Kandydat:** scrollowalny `BeginChild` dla treści, z presetami u góry i paskiem Apply
utrzymanym poza scrollem.

**Warunek:** osobna owner-reviewed zmiana UI. Nie łączyć z texture/scan campaign.

## V-06 · Duże translation units

**Status:** `RESOLVED_FOR_ACCEPTED_SCOPE`

Seria R3–R5 rozdzieliła:

- `jozz_vehicle_m6_rig_lab.cpp` na odpowiedzialności main/UI/persistence/visual;
- `jozz_vehicle_visual_mesh.cpp` na loader i draw;
- world-free geometrię z `suspension_rig` do `jozz_vehicle_m6_geometry.*`.

Zmiany były move-only i porównane validator/render evidence. Nie rozpoczynaj kolejnego
refaktoru wyłącznie dla estetyki. Duży, ale spójny loader może pozostać duży.

## V-07 · Hands-off steering na postoju

**Status:** `RESOLVED_FALSE_BUG`

Dawna narracja o geometrycznym „zakleszczeniu” po bocznym uderzeniu na stojącym aucie
była błędna. Bez ruchu nie istnieje caster-derived siła centrująca. Koło i rack mogą
pozostać przesunięte przez tarcie; jazda przywraca centrowanie.

Naprawiono sondę: uderzenie → jazda → asercja centrowania w ruchu. Opcjonalne
`rackCenteringHertz` pozostaje opt-in arcade assist, domyślnie `0`.

Nie przywracaj programowego self-align jako domyślnego zachowania.

## V-08 · Tarcie kinetyczne racka i twarde lądowanie

**Status:** `ACCEPTED_GUARDED_DEFAULT`

Tarcie kinetyczne racka poniżej około 200 N destabilizowało sondę lądowania 3.5 m:
camber i znos przekraczały zdrową granicę. Default około 200 N pozostaje chroniony.

**Guard:** każda przyszła redukcja rack friction musi uruchomić nie tylko steering
impulse, ale też `RunM7LandingIntegrityProbe` 3.5 m.

## V-09 · Bump-stopy distance jointa są sztywne

**Status:** `ACCEPTED_ENGINE_LIMIT`

Box3D nie wystawia miękkiego limitu dla `b3DistanceJoint`; `SetSpringForceRange`
ogranicza sprężynę główną, nie tworzy progresywnego bump-stop. Obecne stress tests nie
wykazały blockera.

**Możliwa przyszłość:** drugi krótki spring/distance joint jako emulowany bump-stop.
Nie patchować `src/` bez osobnej owner decision.

## V-10 · Delikatna wędrówka kierunku na wprost

**Status:** `ACCEPTED_LIMIT`

Po usunięciu systematycznego pullu pojazd nadal może lekko wędrować w obie strony,
ponieważ niemal wolny rack reaguje na perturbacje. Jozz zaakceptował aktualny feel.

**Guard:** straight-pull probe utrzymuje ograniczenia heading/drift. Naturalna poprawa
może przyjść wraz z przyszłym tire model/pneumatic trail, nie przez sztuczne defaultowe
centrowanie.

## V-11 · Solver contact settings a restart/persistence

**Status:** `OPEN_PHYSICS_STARTUP`

`m_contactHertz`, `m_contactDamping` i `m_contactSpeed` są sample state, a nie pełnym
vehicle configiem. Poprawne zapisanie wymaga również jawnego zastosowania wartości
podczas startu; obecnie `ApplyContactTuning()` jest wywoływane przy zmianie UI.

To nie jest zwykły persistence cleanup — może zmienić fizykę startową.

**Warunek:** osobny plan, baseline numbers, full gate i owner feel review.

## V-12 · Wizualne rozjeżdżanie części steering rigu

**Status:** `OPEN_LOW_VISUAL / EDITOR_CANDIDATE`

Podział `WheelCenter` / `ChassisMount_b` jest semantycznie poprawny, ale przy skręcie
sąsiadujące części mogą wizualnie rozchodzić się za mocno. Fizyka pozostaje poprawna;
problem dotyczy pivotów, parent relationships i visual skinning/placement.

**Plan:** użyć jako wczesny praktyczny przypadek przyszłego edytora rigu. Nie naprawiać
przez sklejanie części ani zmianę accepted physics. Kontekst:
`docs/EDYTOR_RIGU_WYMAGANIA_I_AUDYT_PL.md`.

## V-13 · Planned vehicle scope, nie bieżące bugi

**Status:** `FUTURE_SCOPE`

Świadomie poza aktualną kampanią:

- soft tire/deformacja opony;
- pełniejszy drivetrain, dyferencjały i engine braking;
- tire slip/load sensitivity/pneumatic trail;
- import authored hardpointów z markerów assetu;
- dwa boczne dampery;
- dalszy rig/world editor workflow.

Kolejność i aktywacja wynikają z przyszłej owner decision, nie z samej obecności tego
wpisu.

## Zasada reaktywacji dowolnego długu vehicle

Przed rozpoczęciem:

1. potwierdź, że aktywna kampania pozwala dotknąć vehicle;
2. zacznij z exact remote SHA na nowym izolowanym branchu;
3. odtwórz problem lub zapisz baseline;
4. zdefiniuj minimalny zakres i owner gate;
5. uruchom `tools/gate.ps1` oraz właściwe sondy/render/drive evidence;
6. otwórz draft PR; bez direct push, merge, rebase i force-push.
