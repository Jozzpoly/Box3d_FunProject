# Wielki refactor przed edytorem rigu — completion record

**Pierwotna data:** 2026-07-09  
**Status obecny:** `R0_TO_R5_COMPLETED / HISTORICAL_EVIDENCE`  
**Nie jest:** aktywną sekwencją refaktoru ani zgodą na R6/R7.

Aktualne reguły pracy i accepted vehicle authority:

```text
AGENTS.md
README_FOR_AGENTS.md
docs/CURRENT_STATE_INDEX_PL.md
docs/TECH_DEBT_PL.md
```

## 1. Cel zakończonej serii

Seria miała spłacić największe długi strukturalne bez poruszenia accepted physics lub
feelu. Główna zasada była mocniejsza niż zwykły zielony test:

```text
behavior-preserving refactor
→ validator output byte-identical
→ fixed render evidence identical where applicable
→ no opportunistic cleanup
```

Nie projektowano spekulacyjnych interfejsów „pod przyszły edytor”. Wyciągano tylko
odpowiedzialności o realnej wartości.

## 2. R0 — baseline-diff safety net

**Status:** `COMPLETED / FOUNDATION_PRESERVE`

`tools/gate.ps1` otrzymał tryby zapisania i porównania baseline’u. Walidator jest
porównywany po odfiltrowaniu niestabilnych linii czasowych, a visual stages mogą używać
ustalonych zrzutów.

Najważniejszy dowód: subtelna zmiana defaultu, która nadal przechodziła luźne asercje,
została wykryta przez różnicę liczbową baseline’u. Zasada pozostaje aktualna dla
behavior-preserving vehicle refactors.

## 3. R1 — podział walidatora

**Status:** `COMPLETED`

Monolityczny validator został rozdzielony na:

```text
validation/jozz_validation_helpers.*
validation/jozz_probes_m5_m6.cpp
validation/jozz_probes_m7.cpp
validation/jozz_probes_steering.cpp
validation/jozz_probes_config.cpp
jozz_vehicle_validation.cpp jako mały harness/rejestr
```

Wynik sond pozostał identyczny. Rejestr sond i wspólne pliki są częścią accepted
validation foundation.

## 4. R2 — deskryptory pól konfiguracji

**Status:** `COMPLETED`

Ręcznie zdublowane ścieżki zapisu/odczytu pól zostały zastąpione uporządkowanymi
deskryptorami zachowującymi kolejność JSON i typ właściciela. Celem było ograniczenie
ryzyka, że nowe pole zostanie dopisane tylko do jednej strony persistence.

Nie jest to jeszcze generyczny schema system całego produktu ani finalny format JES.

## 5. R3 — podział M6 rig lab

**Status:** `COMPLETED`

Klasa i odpowiedzialności zostały rozdzielone na wewnętrzny header oraz pliki dla:

- głównego lifecycle/input;
- UI tabs;
- persistence;
- mount/steering visuals.

Przenosiny były behavior-preserving i zweryfikowane baseline/render comparison.

## 6. R4 — podział visual mesh

**Status:** `COMPLETED`

Rozdzielono:

```text
jozz_vehicle_visual_mesh_loader.cpp
jozz_vehicle_visual_mesh_draw.cpp
```

Publiczny kontrakt pozostał stabilny. Loader może pozostać duży, jeżeli nadal jest
spójny; kolejne cięcie wyłącznie dla liczby linii nie jest zadaniem.

## 7. R5 — ekstrakcja czystej geometrii

**Status:** `COMPLETED_WITH_BOUNDED_SCOPE`

Do `jozz_vehicle_m6_geometry.*` przeniesiono sześć publicznych world-free funkcji,
które mają realną wartość poza builderem ciał/jointów. Trzy wewnętrzne helpery
pozostały w implementacji fizyki, ponieważ ich wyciągnięcie zwiększałoby zależności i
blast radius bez wartości kontraktowej.

`geometry.h` nie został sztucznie usamodzielniony przez migrację wszystkich structów.
To była świadoma korekta zakresu, nie niedokończony etap.

## 8. R6 — katalogi i rename churn

**Status:** `OPTIONAL_AESTHETIC / NOT_ACTIVE`

Przeniesienie płaskich `samples/jozz_vehicle_*` do podkatalogów mogłoby poprawić
nawigację, ale kosztuje duży rename/include/history churn i nie daje capability.

Nie uruchamiać bez konkretnego bólu nawigacyjnego oraz osobnej owner decision. Pełny
forensic inventory rozwiązuje dziś znaczną część problemu orientacji bez rename’ów.

## 9. R7 — solver contact przy starcie

**Status:** `OPEN_PHYSICS_CHANGE / OWNER_DECISION_REQUIRED`

Aplikowanie contact tuning przy starcie oraz jego persistence nie jest refaktorem.
Zmienia początkowe zachowanie świata, ponieważ obecnie część wartości jest stosowana
dopiero po interakcji z UI.

Aktualny wpis:

```text
docs/TECH_DEBT_PL.md → V-11
```

Wymaga osobnego briefu, baseline numbers, pełnego gate’u i owner feel review. Nie wolno
przemycić R7 pod nazwą „dokończenie wielkiego refactoru”.

## 10. Trwałe lekcje

- move-only znaczy bez rename’ów i ulepszeń przy okazji;
- kolejność inicjalizacji/ciał/jointów może wpływać na solver;
- zielone progi nie zastępują porównania rzeczywistych liczb;
- render-only zmiana wymaga obejrzanego renderu;
- refactor accepted code potrzebuje konkretnego celu, nie estetycznego impulsu;
- każdy nowy refactor zaczyna się na izolowanym branchu z exact remote SHA i kończy
  draft PR-em — bez direct push na baseline.

## 11. Lifecycle

```text
R0–R5  COMPLETED / FOUNDATION_PRESERVE
R6     OPTIONAL_AESTHETIC / OWNER_DECISION_REQUIRED
R7     OPEN_PHYSICS_CHANGE / OWNER_DECISION_REQUIRED
```

Nie wykonuj ponownie R0–R5 i nie traktuj R6/R7 jako naturalnej kolejki po re-foundation.
