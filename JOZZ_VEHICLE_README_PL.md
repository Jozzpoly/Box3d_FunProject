# Jozz Vehicle — README PL

JV to natywne laboratorium pojazdów rozwijane na forku Box3D. Projekt służy do
budowania i sprawdzania realnie działających mechanizmów: zawieszenia, układu
kierowniczego, mapy, importu skanów oraz przede wszystkim nowych reprezentacji
koła i opony.

## Gdzie jesteśmy

Baseline programu koła `jozz-scan-terrain-f0` @ `5b92e9c` zawiera własny typ
`b3Wheel`: obrotowo symetryczną bryłę z profilem bieżni, dedykowanymi kontaktami,
obsługą mesha, raycastem i debug draw. `WHEEL-RIGID-01`, `WHEEL-SEAM-02A` oraz
`WHEEL-HULL-02B` zamknęły sztywną topologię supportu i kontrolowane przejścia po
trójkątach, ścianach, krawędziach i narożnikach. To jest wartościowy fundament,
ale jeszcze nie model podatnej opony.

Najbliższy program pracy:

```text
sztywny manifold bez sztucznego odcisku — DONE
→ szwy triangle/mesh oraz finite hull face/edge/vertex — DONE
→ A/B lokalnej podatności przy identycznej topologii — NEXT
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

## Lokalny checkpoint

```text
python tools/jv_gate.py quick
```

Pełna mapa profili i dokumentów: `README_FOR_AGENTS.md`.
