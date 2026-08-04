# Current State Index — Jozz Vehicle

Data odświeżenia: 2026-08-04
Snapshot bazowy: `jozz-scan-terrain-f0` @ `5b92e9c`
Status: bieżące źródło prawdy; historia w `archive/` i Git.

## 1. Najważniejszy stan

JV przeszedł od strojenia stockowych prymitywów do własnej bryły koła w Box3D.
`b3Wheel` jest powierzchnią obrotową z profilem poprzecznym do 8 punktów,
zaokrągloną `cornerRadius`. Profil jest sortowany, deduplikowany i redukowany do
górnej otoczki wypukłej. Kod zawiera dedykowane kontakty z płaszczyzną, hullem,
trójkątem, kapsułą i sferą, obsługę mesha/heightfieldu, AABB, masę, raycast oraz
debug draw.

Ta implementacja jest fundamentem, nie końcowym modelem opony.

## 2. Co zostało zmierzone

W repo zapisano wyniki, według których nowy kolider na płaskiej drodze toczy się
znacznie spokojniej niż walec i wielokształtne pierścienie. Sweep wysklepienia
bieżni raportuje m.in. poprawę drgań w zakręcie dla 3 mm crown drop. Te liczby są
**zapisanym wynikiem eksperymentów projektu**, nie zostały ponownie odtworzone w
trakcie porządków dokumentacji.

Krytyczne zastrzeżenie: kod manifoldu płaszczyzny wybiera wszystkie wierzchołki
profilu mieszczące się w `B3_SPECULATIVE_DISTANCE`. Zatem wzrost liczby punktów
z obciążeniem może być skutkiem próbkowania i dystansu spekulacyjnego, a nie
odkształcenia opony. Obecne dane nie dowodzą jeszcze fizycznego „rosnącego śladu”.

## 3. Najbliższy program badawczy

### Etap A — rygorystyczny baseline sztywny

Dla ciągłego, odcinkowo-liniowego profilu:

- unikalny support vertex → jeden kontakt;
- support segment równoległy do płaszczyzny → dwa końce segmentu;
- dystans spekulacyjny decyduje o istnieniu kontaktu, nie o szerokości śladu;
- feature ID identyfikuje rzeczywistą cechę supportu i pozostaje stabilny przy
  obrocie koła.

Sonda ma raportować liczbę punktów, trwałość, rozkład impulsów, `a_rms` i wykrywać
sztuczny wzrost 1→3 punktów wskutek samego overlapu.

### Etap B — poprawność terenu

- test przejazdu przez szew dwóch współpłaszczyznowych trójkątów;
- fallback krawędź/wierzchołek dla kontaktu wheel–triangle;
- brak znikania kontaktu na granicy barycentrycznej;
- później pełne ograniczenie wheel–hull do face polygon oraz osie edge/axis.

### Etap C — A/B podatności

Ta sama geometria, te same feature IDs, punkty manifoldu, masa, tarcie,
zawieszenie i podkroki:

- A: baseline kontaktu bez dodatkowej podatności opony;
- B: lokalny override Hertz/tłumienia dla kontaktu wheel–ground.

Mierzyć kompresję, normal impulse/force, energię, drgania, travel i trwałość
kontaktu. Najpierw quarter-car, potem pełny pojazd na płycie, w zakręcie i na
meshu. Dopiero ten test odpowie, czy lokalna podatność daje wartość niezależną od
biasu topologii.

### Etap D — decyzja o oponie strukturalnej

Jeżeli C daje czysty efekt, rozważyć lokalne stany radialne/osiowe bieżni z
couplingiem sąsiadów, pozostawiając analityczną obwiednię jako broad phase i
powierzchnię zapytań. Nie wracać do wielu niezależnych stockowych colliderów.

## 4. Otwarte ograniczenia implementacji

- wheel–plane obecnie miesza support i speculative candidates;
- wheel–triangle filtruje punkty testem barycentrycznym bez pełnego fallbacku
  krawędzi/wierzchołka;
- wheel–hull opiera się głównie na normalnych ścian i płaszczyźnie, bez pełnego
  clippingu do wielokąta ściany i kompletnego SAT;
- masa koła jest przybliżeniem walca obwiedniowego; pojazd zamraża ją osobno;
- raycast jest konserwatywny, nie jest dokładnym przecięciem profilu;
- generic shape-cast/overlap używa konserwatywnego proxy, nie pełnej geometrii;
- świat ma globalną miękkość kontaktu, brak jeszcze lokalnego parametru opony.

Pełny rejestr: `TECH_DEBT_PL.md`.

## 5. Pozostałe subsystemy JV

- wielociałowe zawieszenie i kierownica pozostają działającym laboratorium
  pojazdu, ale nie są aktualnym przedmiotem refaktoru;
- centralny kampus mapy, offroad, import skanu i spawny per fragment są w kodzie;
  manualny odbiór mapy pozostaje otwarty;
- UI/presety oraz rig wizualny mają własne dokumenty subsystemowe;
- stare milestone M0–M9 i zamknięte plany przeniesiono do `archive/`.

## 6. JV jako dziedzictwo JES

Do JES promujemy zdolności i wiedzę: protokoły eksperymentów, jawne confoundy,
Zero-Delta-Off, rozdział authoring/visual/physics/contact, negatywne wyniki i
minimalne reproduktory. Nie promujemy automatycznie hosta sampli, nazw milestone,
całego configu M6 ani mechanizmów związanych tylko z historycznym układem JV.
Szczegóły: `JV_JES_HERITAGE_PL.md`.

## 7. Minimalna bramka następnego commita

- nowy test manifold topology;
- przejście istniejących testów koła;
- brak zmiany masy, tarcia i geometrii w A/B;
- `python tools/docs_audit.py`;
- `python tools/repo_hygiene.py`;
- `python tools/jozz_core_delta.py` przy delcie `src/`/`include/`;
- `git diff --check`;
- wpis w `CHECKPOINTS_PL.md` i aktualizacja długu.
