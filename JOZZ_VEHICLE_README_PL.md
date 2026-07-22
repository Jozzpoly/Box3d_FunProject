# Jozz Vehicle — historyczny compatibility pointer

**Status:** `SUPERSEDED_AS_FRONT_DOOR / LINK_COMPATIBILITY_ONLY`  
**Nie jest:** current state, globalną polityką, roadmapą ani instrukcją wyboru brancha.

Ten plik powstał 2026-07-05, gdy root `README.md` nadal był upstreamowym README Box3D,
a pojazd znajdował się w okolicy M2.5/M5. Od tego czasu projekt przeszedł przez M7/M8,
syntetyczny engineering world, realny scan pipeline oraz repository governance.

Aktualna kolejność odczytu:

```text
AGENTS.md
→ GitHub Control Issue #11
→ AI_PROJECT_MEMORY.md
→ właściwy current state i aktywny PR
→ docs/PROJECT_OPERATING_PLAN_PL.md
→ docs/PROJECT_CHARTER_PL.md
```

Dla domeny vehicle:

```text
README_FOR_AGENTS.md
→ docs/CURRENT_STATE_INDEX_PL.md
→ docs/TECH_DEBT_PL.md
→ właściwe docs/SUBSYSTEM_* i ADR
```

Trwałe zasady zachowane z pierwotnego dokumentu:

- projekt jest natywnym sandboxem pojazdów Jozza na fundamencie Box3D;
- praca postępuje małymi, falsyfikowalnymi bramkami;
- `src/**` i `include/**` nie są powierzchnią wygodnych obejść;
- authored asset, visual asset i physics prefab pozostają rozdzielone;
- `b3WheelJoint` używa jawnego rest-anchor modelu:

```text
Frame A = rest wheel-center anchor na chassis
Frame B = centrum koła
spring rest = translation 0
```

Nie wracaj do historycznego modelu M2.3, w którym wizualny mount amortyzatora stawał
się frame’em fizyki.

Aktualne komendy, sample, accepted physics oraz owner gates znajdują się wyłącznie w
`README_FOR_AGENTS.md` i bieżących dokumentach authority. Historyczna pełna treść tego
pliku pozostaje osiągalna w Git.
