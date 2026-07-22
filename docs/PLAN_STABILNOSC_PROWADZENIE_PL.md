# Stabilność zawieszenia i prowadzenia P1–P6 — completion record

**Status:** `COMPLETED_AND_OWNER_ACCEPTED / HISTORICAL_EVIDENCE`  
**Nie jest:** aktywnym planem, listą zadań ani aktualnym workflowem Git.

Current vehicle authority:

```text
README_FOR_AGENTS.md
docs/CURRENT_STATE_INDEX_PL.md
docs/TECH_DEBT_PL.md
```

Globalne zasady pracy pochodzą z `AGENTS.md` i `CONTRIBUTING.md`.

## 1. Cel zakończonej kampanii

Kampania P1–P6 usunęła łamanie układu kierowniczego, uporządkowała pozycję pojazdu,
poprawiła prowadzenie oraz rozdzieliła realne błędy od poprawnego zachowania fizyki.

Trwała metoda:

```text
reprodukcja albo sonda przed zmianą
→ minimalna poprawka
→ analiza rzeczywistych liczb
→ pełny gate
→ owner drive/feel review
```

## 2. Zamknięte wyniki

### Rack i steering fences

- `rackTravel` jest przeliczany po zmianie geometrii;
- diagnostyka wykrywa stale limit;
- granice skrętu wynikają z konfiguracji zamiast z nadmiernie szerokiego hardcode’u;
- owner potwierdził brak dawnego zrywania skrętu podczas ekstremalnej jazdy.

### Poza i prześwit

- przód i tył mogą być strojone świadomie;
- późniejszy M8 rozwinął fundament o droop, preload i accepted visual rig;
- visual mount nie jest automatycznie physics frame.

### Powrót kierownicy i tarcie racka

Dawna hipoteza geometrycznego „zakleszczenia” na postoju została obalona. Bez ruchu
nie ma caster-derived siły centrującej, więc koło może pozostać skręcone przez tarcie.
Jazda przywraca centrowanie.

Sonda została poprawiona do sekwencji:

```text
uderzenie → jazda → centrowanie w ruchu
```

Sztuczne defaultowe self-align pozostaje usunięte. Opcjonalny arcade assist jest
wyłączony domyślnie.

### Sanity i regresje

Zachowano sondy dla:

- landing integrity;
- steering fences;
- straight pull;
- config sanitize i determinism;
- preset behavior.

## 3. Owner verdict

Jozz zaakceptował:

- poprawione prowadzenie;
- brak wcześniejszego zrywania skrętu;
- delikatną, dwukierunkową wędrówkę racka;
- brak automatycznego centrowania na postoju jako zachowanie realistyczne.

## 4. Rzeczy poza P1–P6

Nadal jawne, ale wymagające osobnych kampanii:

```text
droop / over-center geometry
rack friction vs hard landing guard
rigid bump-stops
contact tuning startup persistence
soft tire i bogatszy tire model
visual steering-rig pivot separation
```

Authority: `docs/TECH_DEBT_PL.md`.

## 5. Trwałe reguły

- czytaj drukowane liczby, nie tylko końcowe `OK`;
- sprawdzaj, czy sonda mierzy zgłoszone zjawisko;
- nie luzuj progu, aby ukryć problem;
- zmiana prowadzenia wymaga owner feel review;
- nie używaj `src/**` lub `include/**` jako wygodnego obejścia;
- nowa praca zaczyna się na izolowanym branchu z exact remote SHA i kończy draft PR-em.

## 6. Lifecycle

```text
P1–P6          COMPLETED
owner feel     ACCEPTED
probe lessons  FOUNDATION_PRESERVE
future tuning  OWNER-GATED SEPARATE CAMPAIGN
```

Nie wykonuj ponownie historycznych etapów ani nie kopiuj ich dawnych poleceń jako
bieżącego workflowu.
