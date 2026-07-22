# Finalizacja nadwozia i przedniego rigu — completion record

**Pierwotna data planu:** 2026-07-11  
**Status obecny:** `COMPLETED / HISTORICAL_EVIDENCE`  
**Owner:** Jozz  
**Nie jest:** aktywnym trackiem, task queue ani zgodą na zmianę accepted vehicle.

Szczegółowe historyczne instrukcje implementacyjne pozostają w:

```text
FINALIZACJA_ETAP_1_MODEL_I_UI_PL.md
FINALIZACJA_ETAP_2_PERSYSTENCJA_PL.md
FINALIZACJA_ETAP_3_STAN_ZWALIDOWANY_PL.md
```

Aktualny accepted vehicle contract znajduje się w:

```text
README_FOR_AGENTS.md
docs/CURRENT_STATE_INDEX_PL.md
docs/TECH_DEBT_PL.md
```

## 1. Co właściciel zamówił

Plan miał przeprowadzić wizualne nadwozie i przedni rig kierowniczy z poziomu
eksperymentu do domyślnego, spersystowanego stanu gry bez zmiany fizyki chassis.

Wymagania ownera:

- możliwość wyboru modelu nadwozia;
- dla nowych nadwozi tylko offset pozycji w UI, bez przypadkowego skalowania/rotacji;
- zachowanie taniej bryły kolizyjnej chassis;
- typ nadwozia, offset i model zawieszenia jako część tożsamości pojazdu/presetu;
- restart `R` nie może kasować tych ustawień;
- przedni rig i nadwozie mają przestać być ukrytym debug experimentem.

## 2. Ustanowiona granica danych

Rozstrzygnięto, że:

```text
vehicle identity/config
  bodyVisualModel
  bodyVisualOffset
  frontSuspensionVisualModel

view/debug session
  show body visual
  helper lines
  raw collision/debug visibility
  diagnostic tints
```

Wybrany model i jego ustawienie są częścią pojazdu. Samo chwilowe pokazywanie warstwy
pozostaje ustawieniem widoku i nie powinno przeciekać do presetów.

## 3. Wynik Etapu 1 — modele i UI

Zrealizowano:

- kurowany rejestr modeli nadwozia;
- wybór modelu w zakładce Nadwozie;
- live offset pozycji;
- jawne klucze modeli zamiast runtime handle’i;
- zachowanie osobnej, niezmienionej fizyki chassis;
- przedni visual rig jako wybieralny model.

## 4. Wynik Etapu 2 — persystencja

Zrealizowano:

- pola visual w `JozzVehicleM6Config`;
- zapis/odczyt session i presetów;
- kompatybilność z częściowymi presetami built-in;
- restart `R` zachowujący identity/offset;
- rozdział configu pojazdu od debug-session.

Kontrakt persistence jest opisany w `docs/SUBSYSTEM_UI_PRESETS_PL.md`.

## 5. Wynik Etapu 3 — accepted default i kolider

Owner decisions zostały wykonane i zapisane jako accepted state:

```text
bodyVisualModel              = rama_rurowa
frontSuspensionVisualModel   = rig_kierowniczy
```

Tania bryła kolizyjna chassis pozostaje fizyką, ale może być ukrywana w debug renderze
przez mały opt-in shared-host hook. Ukrycie wizualizacji nie usuwa shape’a ani kolizji.

Walidacja obejmowała build, validator, Box3D tests, smoke oraz owner render/runtime
review. Visual defaults nie powinny zmieniać wyników sond fizycznych.

## 6. Co nie zostało zalegalizowane przez ten plan

Plan nie przyznał zgody na:

- zmianę geometrii lub masy chassis;
- zmianę accepted suspension/steering physics;
- mesh collision nadwozia;
- automatyczne skalowanie nadwozia;
- pełny frame/rig editor;
- patchowanie Box3D `src/**` lub `include/**`;
- ponowne otwarcie D1/D2/D3 bez nowego owner feedbacku.

## 7. Pozostały jawny dług

Przy skręcie niektóre części przedniego visual rigu mogą rozchodzić się względem siebie
zbyt mocno. To problem pivotów/parent relationships i kandydat dla przyszłego edytora,
nie powód do sklejania części ani zmiany accepted physics.

Aktualny wpis:

```text
docs/TECH_DEBT_PL.md → V-12
```

## 8. Lifecycle

```text
plan umbrella                  COMPLETED
Etap 1/2/3 detailed documents HISTORICAL_EVIDENCE
accepted defaults             FOUNDATION_PRESERVE
future rig/frame editor        PARKED / OWNER_DECISION_REQUIRED
```

Nie rozpoczynaj ponownie tego planu. Nowa zmiana nadwozia/rigu wymaga nowego bounded
briefu z exact baseline, owner gate i pełną vehicle validation.
