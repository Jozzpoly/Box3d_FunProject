# Jozz Vehicle — README PL

JV to natywne laboratorium pojazdów rozwijane na forku Box3D. Projekt służy do
budowania i sprawdzania realnie działających mechanizmów: zawieszenia, układu
kierowniczego, mapy, importu skanów oraz przede wszystkim nowych reprezentacji
koła i opony.

## Gdzie jesteśmy

Baseline programu koła `jozz-scan-terrain-f0` @ `5b92e9c` zawiera własny typ
`b3Wheel`: obrotowo symetryczną bryłę z profilem bieżni, dedykowanymi kontaktami,
obsługą mesha, raycastem i debug draw. To jest wartościowy fundament, ale jeszcze
nie pełny model opony. Obecny manifold może poszerzać „ślad” przez aktywowanie
punktów znajdujących się w dystansie spekulacyjnym; trzeba oddzielić ten efekt od
prawdziwej podatności.

Najbliższy program pracy:

```text
sztywny manifold bez sztucznego odcisku
→ poprawność na szwach i krawędziach terenu
→ A/B lokalnej podatności przy identycznej topologii
→ decyzja, czy potrzebna jest opona strukturalna
```

Pełny stan: `docs/CURRENT_STATE_INDEX_PL.md`.
Program koła: `docs/KOLA_00_INDEX_PL.md`.

## JV a JES

JES jest osobnym, młodszym projektem. JV ma dostarczać mu zweryfikowane
zdolności, dowody, porażki i metody badawcze — nie cały kod ani historyczną
strukturę repo. Macierz dziedzictwa: `docs/JV_JES_HERITAGE_PL.md`.

## Dokumentacja

- `README_FOR_AGENTS.md` — start dla agenta;
- `docs/JV_DOCS_INDEX_PL.md` — mapa dokumentacji;
- `docs/CHECKPOINTS_PL.md` — najnowsze zmiany;
- `docs/TECH_DEBT_PL.md` — otwarte ryzyka;
- `docs/archive/` — dawne plany i raporty, zachowane jako historia.

Główny `README.md` pozostaje upstreamowym README Box3D. To celowe: JV jest
forkiem silnika i nie udaje, że upstreamowa dokumentacja API przestała istnieć.
