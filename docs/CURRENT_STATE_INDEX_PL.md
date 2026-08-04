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

`WHEEL-RIGID-01` został odtworzony lokalnie i zamknięty testem czerwony→zielony (`F-38`).
Manifold płaszczyzny raportuje teraz rzeczywisty support sztywnego profilu: jeden
wierzchołek albo dwa końce równoległego segmentu. `B3_SPECULATIVE_DISTANCE`
decyduje wyłącznie o istnieniu kontaktu, nie o szerokości śladu. Sztywne koło
nie udaje już deformacji przez aktywowanie pobliskich próbek.

Pełny walidator pojazdu przeszedł dwukrotnie i dał bajtowo identyczne wyjście:
19 sond + 2 sondy mapy, `OK`. Rozszerzona telemetria rozdzieliła wszystkie
punkty manifoldu od punktów faktycznie niosących impuls; w sweepie crown każdy
wariant miał stale `1,00 all/kolo` i `1,00 nios/kolo`. Mimo usunięcia confoundu
speculative candidates 3 mm nadal obniżyło `a_rms` w
zakręcie z `0,571` do `0,436 m/s²`; na prostej pogorszyło wynik z `0,053` do
`0,061 m/s²`. Jest to odtworzony efekt geometrii w tym jednym deterministycznym
protokole (`F-39`), nie dowód podatności ani uniwersalnie lepszej opony.

## 3. Najbliższy program badawczy

Front door programu: `KOLA_00_INDEX_PL.md`.

### Etap A — rygorystyczny baseline sztywny — ZAMKNIĘTY (`WHEEL-RIGID-01`)

Dla ciągłego, odcinkowo-liniowego profilu:

- unikalny support vertex → jeden kontakt;
- support segment równoległy do płaszczyzny → dwa końce segmentu;
- dystans spekulacyjny decyduje o istnieniu kontaktu, nie o szerokości śladu;
- feature ID identyfikuje rzeczywistą cechę supportu i pozostaje stabilny przy
  obrocie koła.

Testy sprawdzają topologię przy różnym overlapie, camber, limit pojemności,
granicę speculative distance oraz trwałość `featureId` podczas rzeczywistego
obrotu dynamicznego koła. Następne rozszerzenie telemetryczne należy wykonywać
już w ramach seam/soft, bez ponownego otwierania topologii plane.

### Etap B — poprawność terenu — AKTYWNY NASTĘPNY KROK (`WHEEL-SEAM-02`)

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
- centralny kampus mapy, offroad, import skanu i spawny per fragment są w kodzie (`MAPA_INDEX_PL.md`);
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

- seam probe dwóch współpłaszczyznowych trójkątów;
- test face→edge→vertex bez znikania kontaktu;
- przejście istniejących testów koła i pełnego walidatora pojazdu;
- brak zmiany masy, tarcia i geometrii w A/B;
- `python tools/docs_audit.py`;
- `python tools/repo_hygiene.py`;
- `python tools/jozz_core_delta.py` przy delcie `src/`/`include/`;
- `git diff --check`;
- wpis w `CHECKPOINTS_PL.md` i aktualizacja długu.
