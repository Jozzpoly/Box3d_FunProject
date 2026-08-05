# Decision packet — WHEEL-SOFT-03

- Run: `20260805T100714313031Z-bf1264ed-b6de28ae`
- Proposal: `bf1264ed5a1f5b79ce9d865e61628af109cb94c9:6960840e6c199f025ca0ecaea4d0864037290cef`
- Poziom: `Q2`
- Wykonanie: **EXECUTED**
- Stan decyzji: **READY_FOR_ANALYSIS**
- Automatyczny werdykt fizyczny: **BRAK**

## Pytanie

Czy lokalna podatność normalnego kontaktu wheel-ground zmniejsza drgania i szczyty obciążenia bez zmiany geometrii, topologii manifoldu ani zachowania stycznego?

## Case'y

- `Q2--A_WORLD_1_00` — PASSED, rc=0, parametry={'wheel_contact_hertz_scale': 1.0}
- `Q2--B_LOCAL_0_25` — PASSED, rc=0, parametry={'wheel_contact_hertz_scale': 0.25}
- `Q2--B_LOCAL_0_50` — PASSED, rc=0, parametry={'wheel_contact_hertz_scale': 0.5}
- `Q2--B_LOCAL_0_75` — PASSED, rc=0, parametry={'wheel_contact_hertz_scale': 0.75}

## Dalej

Przeanalizować metryki i kontrole przeciwne. Status SUPPORTED/REFUTED nadaje osobna decyzja, nie runner.
