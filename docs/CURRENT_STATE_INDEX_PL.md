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

`WHEEL-SEAM-02A` zamknął kontrolowany zakres triangle/mesh (`F-40`, `F-41`).
Kontakt trójkąta jest jednostronny, zachowuje face support, a poza skończoną
ścianą przechodzi na rzeczywistą krawędź lub wierzchołek. Na obciążonym płaskim
szwie solver utrzymuje dokładnie jeden punkt bez luki w obu kierunkach i przy
trzech fazach obrotu. Na załamaniu `~1,15°` suma impulsu pozostaje w przyjętym
oknie `20%`; wheel-only wybór normalnej najgłębszego manifoldu usunął zmierzony
przed poprawką, zależny od fazy skok `+36,4%`. Dwa świeże przebiegi pełnego
walidatora produktu pozostały bajtowo identyczne (`19 + 2`, `OK`).


`WHEEL-HULL-02B` zamknął kontrolowany zakres skończonych hullów (`F-42`, `F-43`).
Face manifold jest clipowany do polygonu; zwykła szeroka ściana ma konserwatywny
certyfikat całej projekcji koła, a przypadki brzegowe przechodzą deterministyczny
feature walk z osobnymi face/edge/vertex IDs. Obciążone przejście
face→edge→vertex przechodzi w obu kierunkach i dwóch fazach. Audit obejmuje 2000
orientowanych boxów oraz 60 nieortogonalnych hullów względem 8192 kierunków
referencyjnych; dopuszczalny błąd osi wynosi 3 mm. Pełny validator produktu
pozostał zielony (`19 + 2`, `OK`). Search edge/vertex jest numeryczny, nie jest
formalnym dowodem analitycznie dokładnego SAT.

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

### Etap B1 — trójkąty i mesh — ZAMKNIĘTY (`WHEEL-SEAM-02A`)

- face/edge/vertex są rozróżniane przez feature IDs;
- finite edge/vertex fallback usuwa lukę po odrzuceniu barycentrycznym;
- płaski szew nie dubluje constraintu ani impulsu;
- mała zmiana normalnej jest sprawdzona w obu kierunkach i kilku fazach;
- przejście `triangleIndex` może dać jeden jawny reset `persisted`, ale bez luki
  kontaktu, zmiany feature ID ani churnu po obu stronach.

### Etap B2 — hully i narożniki — ZAMKNIĘTY (`WHEEL-HULL-02B`)

- face manifold jest ograniczony do polygonu ściany;
- pełna projekcja koła certyfikuje tani fast path tylko dla rzeczywistej ściany;
- feature walk dociera do odległych krawędzi i wierzchołków normal fan;
- osobne feature IDs przeżywają spin, a obciążone face→edge→vertex nie ma luki;
- numeryczny search jest walidowany względem gęstego globalnego odniesienia,
  lecz pozostaje kontrolowanym przybliżeniem z progiem 3 mm.

### Etap C — A/B podatności — AKTYWNY NASTĘPNY KROK (`WHEEL-SOFT-03`)

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

- testy triangle/mesh obejmują kontrolowane płaskie i łagodnie załamane szwy,
  lecz nie dowodzą poprawności dowolnej siatki ani bardzo ostrych cech;
- wheel–hull używa numerycznego searchu w stożkach normalnych krawędzi i
  wierzchołków; audit ogranicza znany błąd do 3 mm, ale nie jest formalnym dowodem
  globalnej optymalności, a ciężka ścieżka narożnika kosztuje około 12 us/call;
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

- wheel–hull face polygon clipping i test negatywny phantom corner;
- edge/axis separation oraz przejście face→edge→vertex bez znikania kontaktu;
- przejście 16 istniejących testów koła i pełnego walidatora pojazdu;
- brak zmiany masy, tarcia, profilu i softness w tej paczce;
- `python tools/docs_audit.py`;
- `python tools/repo_hygiene.py`;
- `python tools/jozz_core_delta.py` przy delcie `src/`/`include/`;
- `git diff --check`;
- wpis w `CHECKPOINTS_PL.md` i aktualizacja długu.
