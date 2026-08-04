# Koła i opony — protokół eksperymentu

Status: bieżący protokół dla `WHEEL-RIGID-01`, `WHEEL-SEAM-02A`,
`WHEEL-HULL-02B` i `WHEEL-SOFT-03`. Historyczny protokół stendu v2.1 oraz rigi Q0–Q4 zachowano w
`archive/consolidated_2026-08/KOLA_05_PROTOKOL_STENDU_V21_PL.md`.

## 1. Minimalny kontrakt paczki

Każdy eksperyment ma identyfikator i katalog:

```text
experiments/<ID>/
  manifest.json
  result_summary.md
  raw/
```

`manifest.json` zapisuje:

- pełny commit SHA, platformę, konfigurację builda i krok czasu;
- badaną hipotezę oraz jeden parametr główny;
- wartości zamrożone;
- opis rigu, drogi, prędkości i sterowania;
- listę surowych logów z SHA-256;
- metryki i warunki PASS / FAIL / INCONCLUSIVE;
- informację o manualnej ingerencji.

Brak manifestu lub surowego logu oznacza obserwację, nie dowód.

## 2. Drabina dowodu

| Poziom | Rola |
|---|---|
| Q0 | test geometrii/supportu bez dynamiki |
| Q1 | pojedyncze koło na kontrolowanej powierzchni |
| Q2 | quarter-car, izolacja masy niesprężonej i zawieszenia |
| Q3 | pełny pojazd na płycie i w zakręcie |
| Q4 | mapa/mesh, przeszkody i manualny feeling Jozza |

Wynik nie awansuje automatycznie. Przechodzi przynajmniej jeden poziom wyżej albo
pozostaje opisany jako ograniczony do danego rigu.

## 3. Zamrożone zmienne

Domyślnie zamrażamy:

```text
profil i cornerRadius
masa + inertia
friction + rolling resistance
suspension geometry + tuning
solver substeps i timestep
sterowanie, prędkość, droga, seed
warunki startowe i rozgrzewkę
```

Zmiana liczby próbek profilu jest zmianą geometrii i możliwej topologii. Nie
wolno jednocześnie interpretować jej jako testu podatności.

## 4. Metryki obowiązkowe

### Kontakt

- manifold count i point count per krok;
- feature IDs i procent `persisted`;
- separation/penetration;
- normal impulse per punkt i suma;
- centroid/rozpiętość punktów wzdłuż szerokości;
- liczba zgubionych kontaktów i czas bez kontaktu.

### Dynamika

- `a_rms` i peak nadwozia w osi pionowej;
- RMS/peak travel zawieszenia;
- prędkość liniowa i kątowa koła;
- energia lub praca tracona na jednostkę drogi;
- dryf kierunku, yaw i odpowiedź kierownicy tam, gdzie dotyczą.

### Stabilność numeryczna

- wartości nie-finite;
- maksymalna penetracja;
- zależność wyniku od podkroków;
- deterministyczność powtórzeń i rozrzut serii.

## 5. `WHEEL-RIGID-01`

Hipoteza: strict support manifold usuwa sztuczny wzrost liczby punktów bez
pogorszenia ciągłości toczenia.

Kontrole:

- płaski profil: prawdziwy support segment → 2 punkty;
- wypukły profil: unikalny support → 1 punkt;
- zwiększony overlap nie może sam tworzyć 1→3 punktów;
- obrót koła nie zmienia feature ID tego samego support feature;
- porównanie obecnego i strict trybu przy tej samej geometrii.

Akceptacja nie zależy od tego, który wariant daje niższe drgania. Najpierw ma
być poprawny geometrycznie i stabilny; wynik jazdy opisujemy osobno.

## 6. `WHEEL-SEAM-02A` — triangle/mesh — ZAMKNIĘTY

Scena minimalna obejmuje finite edge, finite vertex, tylną stronę trójkąta, dwa
współpłaszczyznowe trójkąty oraz załamanie `0,02 m / 1,00 m` (`~1,15°`).

Bramka płaskiego, obciążonego szwu:

- brak kroku bez kontaktu;
- dokładnie 1 solver point, bez podwójnego impulsu;
- suma impulsu w oknie `±5%`;
- stały `featureId`, dokładnie jeden handoff `triangleIndex`;
- najwyżej jeden reset `persisted` podczas handoffu;
- oba kierunki i fazy `0,00 / 0,73 / 1,61 rad`.

Bramka łagodnego załamania:

- brak kroku bez kontaktu i `1–2` punkty dla dwóch różnych normalnych;
- iloczyn kolejnych normalnych `> 0,995`;
- suma impulsu po rozgrzewce w oknie `±20%`;
- oba kierunki i fazy `0,00 / 1,17 rad`.

Próg `20%` zapisano po pomiarze kontrolnym `+9% / +13,5%`, ale przed poprawką
normalnej klastra; pre-fix anomalia `+36,4%` była więc czerwona, nie została
zaakceptowana przez dostrojenie progu. Surowy przebieg i komendy są w
`tools/jozz_wheel_bench/evidence/run_wheel_seam_02a_2026_08_04.txt`.

## 7. `WHEEL-HULL-02B` — ZAMKNIĘTY

Scena minimalna: skończona ściana hulla, przejście przez jej krawędź, narożnik
oraz kontrola negatywna, w której support nieskończonej płaszczyzny leży poza
polygonem i nie wolno utworzyć kontaktu.

Bramka wymaga:

- clippingu face manifold do polygonu ściany;
- istotnych osi face, edge/axis i ich deterministycznego wyboru;
- poprawnych face/edge/vertex feature IDs i warm startu;
- braku phantom contact oraz luki przy prawdziwym przejściu;
- obu kierunków, kilku faz koła i obciążonego testu impulsu.

Softness, profil, masa, tarcie i parametry solvera pozostają zamrożone.

Wynik: finite face clipping, konserwatywny face-prism fast path oraz feature
walk z numerycznym szukaniem osi edge/vertex. Obciążone przejście face→edge→vertex
przechodzi w obu kierunkach i dwóch fazach; finalny audit 2000 boxów i 60
nieortogonalnych hullów pozostaje w granicy 3 mm względem 8192 kierunków.
Surowe komendy, pomiary kosztu i ograniczenia są w
`tools/jozz_wheel_bench/evidence/run_wheel_hull_02b_2026_08_04.txt`.

## 8. `WHEEL-SOFT-03`

A i B używają identycznej geometrii i manifoldu.

```text
A = bazowa odpowiedź kontaktu świata
B = lokalny override wheel–ground Hertz + damping
```

Sweep zaczyna się od wartości względnych wobec świata, np. `1.0x`, `0.5x`,
`0.25x`, bez udawania jeszcze realnej sztywności konkretnej opony. Później
parametry można wyprowadzać z docelowej stiffness i efektywnej masy.

Werdykt wymaga Q2, następnie Q3 na prostej i w zakręcie, a na końcu Q4 na meshu.
Mierzymy dodatkowo kompresję, siłę/impuls normalny, stratę energii i wpływ na
sterowanie.

## 8.1 Wykonywalny kontrakt i JV Research OS

Aktywny eksperyment ma maszynową specyfikację w
`tools/research/experiments/` i jest prowadzony przez `python tools/jv_lab.py`.
Kontrakt blokuje drugą zmienną główną, zapisuje `HEAD:index-tree`, wymaga
artefaktów niezależnie od kodu wyjścia procesu i nie pozwala wznowić runu na
zmienionej proposal.

Awans Q2→Q3→Q4 wymaga zapieczętowanego parent runu oraz jawnej decyzji
`SUPPORTED`/`STRONGLY_SUPPORTED`; zielony proces nie nadaje statusu wiedzy.
Pełny kontrakt infrastruktury: `JV_RESEARCH_OS_PL.md`. Bieżący plan 03:
`tools/research/experiments/WHEEL-SOFT-03.json`.

## 9. Status wyniku

- `PROVISIONAL` — świeży przebieg, poprawny technicznie;
- `SUPPORTED` — powtórzony i przeniesiony co najmniej poziom wyżej;
- `STRONGLY_SUPPORTED` — mechanizm widoczny w telemetrii i odporny na kontrolę
  przeciwną;
- `REFUTED` — hipoteza obalona;
- `INCONCLUSIVE` — confound lub brak mocy pomiaru.

Status findingu żyje w `KOLA_FINDINGS.json`. Proza nie może nadawać mu innego
statusu.

## 10. Manualna sesja

Chwyt, wymuszenie impulsu, zmiana suwaka podczas okna pomiarowego lub ręczne
przejęcie pojazdu oznaczają sesję eksploracyjną. Jest cenna i może wygenerować
nową hipotezę, ale nie zastępuje deterministycznego przebiegu headless.

Feeling, obraz i zachowanie graniczne odbiera Jozz. Instrument ma zachować
konfigurację i log, a nie automatycznie ogłaszać „dobrze jeździ”.

## 11. Bramka publikacji

Przed wpisaniem wyniku do aktywnej dokumentacji:

1. surowe dane i manifest istnieją;
2. kontrola A/B różni się jedną główną zmienną;
3. log nie zawiera ostrzeżeń ani non-finite;
4. wynik przeszedł audyt przeciwny;
5. tabela jest generowana z logu, nie przepisana ręcznie;
6. `python tools/jv_gate.py wheel` przechodzi;
7. ograniczenia rigu są napisane obok werdyktu.
