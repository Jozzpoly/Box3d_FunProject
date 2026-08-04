# Koła i opony — protokół eksperymentu

Status: bieżący protokół dla `WHEEL-RIGID-01`, `WHEEL-SEAM-02` i
`WHEEL-SOFT-03`. Historyczny protokół stendu v2.1 oraz rigi Q0–Q4 zachowano w
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

## 6. `WHEEL-SEAM-02`

Scena minimalna: dwa współpłaszczyznowe trójkąty ze wspólną krawędzią oraz
wariant z małą zmianą normalnej.

Mierzymy:

- brak klatki bez kontaktu podczas przejścia przez szew;
- ciągłość normalnej i impulsu;
- poprawne face/edge/vertex feature IDs;
- brak podwójnego impulsu od dwóch współpłaszczyznowych trójkątów;
- powtórzenie w obu kierunkach i przy kilku fazach obrotu.

Dopiero po tej bramce wyniki mapy mogą walidować model opony.

## 7. `WHEEL-SOFT-03`

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

## 8. Status wyniku

- `PROVISIONAL` — świeży przebieg, poprawny technicznie;
- `SUPPORTED` — powtórzony i przeniesiony co najmniej poziom wyżej;
- `STRONGLY_SUPPORTED` — mechanizm widoczny w telemetrii i odporny na kontrolę
  przeciwną;
- `REFUTED` — hipoteza obalona;
- `INCONCLUSIVE` — confound lub brak mocy pomiaru.

Status findingu żyje w `KOLA_FINDINGS.json`. Proza nie może nadawać mu innego
statusu.

## 9. Manualna sesja

Chwyt, wymuszenie impulsu, zmiana suwaka podczas okna pomiarowego lub ręczne
przejęcie pojazdu oznaczają sesję eksploracyjną. Jest cenna i może wygenerować
nową hipotezę, ale nie zastępuje deterministycznego przebiegu headless.

Feeling, obraz i zachowanie graniczne odbiera Jozz. Instrument ma zachować
konfigurację i log, a nie automatycznie ogłaszać „dobrze jeździ”.

## 10. Bramka publikacji

Przed wpisaniem wyniku do aktywnej dokumentacji:

1. surowe dane i manifest istnieją;
2. kontrola A/B różni się jedną główną zmienną;
3. log nie zawiera ostrzeżeń ani non-finite;
4. wynik przeszedł audyt przeciwny;
5. tabela jest generowana z logu, nie przepisana ręcznie;
6. `python tools/jv_gate.py wheel` przechodzi;
7. ograniczenia rigu są napisane obok werdyktu.
