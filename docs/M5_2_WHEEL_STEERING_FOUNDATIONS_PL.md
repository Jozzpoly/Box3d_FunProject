# M5.2 Wheel & Steering Foundations — raport

Data: 2026-07-05
Branch: `jozz-vehicle-sandbox-m0`
Status: zaimplementowane, walidacja headless + boot smoke zielone; czeka na re-test Jozza
Commity: `8c4818f` (konwencja skrętu + tie rod + Ackermann + telemetria), `f59a412` (UI/wykresy)

Cel (z feedbacku Jozza po ~30 min jazdy): zanim powstanie pełny rig zawieszenia —
zlikwidować niestabilność kół, doprowadzić prowadzenie do porządku, dać maksymalną
kontrolę przez UI w grze, podejść "poważnie, profesjonalnie i nowatorsko".

## 1. A/D odwrócone — prawdziwa przyczyna (i rozliczenie dwóch błędnych diagnoz)

Historia w trzech aktach, zapisana świadomie, bo proces jest tu równie ważny co wynik:

1. **M5**: komentarz w module deklarował "left is +Z" i na tej podstawie skręt
   dostał negację znaku. Playtest: A/D odwrócone.
2. **M5.1**: uznałem, że znak "wychodzi poprawnie" i obwiniłem kamerę patrzącą
   od przodu (lustrzane lewo/prawo). Re-test: nadal odwrócone. Kamera była
   niewinna (choć chase-cam zostaje, bo jest po prostu lepszy).
3. **M5.2**: rachunek na papierze, raz na zawsze:

```text
forward = +X, up = +Y
right   = forward x up = (+X) x (+Y) = +Z
LEWO    = -Z
dodatni kąt skrętu obraca +X w stronę -Z wokół osi +Y  =>  dodatni kąt = SKRĘT W LEWO
```

Czyli: pierwotny znak BEZ negacji był dobry, a błędna była notatka "left is +Z",
która lustrzanie odwróciła też nazwy narożników (FRONT_LEFT siedział po prawej!).
Naprawione: negacja usunięta, offsety narożników odlustrzone, konwencja wpisana
na górze `jozz_vehicle_m5_vehicle.h` jako jedyne źródło prawdy.

**Lekcja procesowa:** smoke test sprawdzał tylko |wartość| kąta i |wielkość|
zmiany headingu — więc dwukrotnie "przechodził" przy złym znaku. Teraz asercje
są PODPISANE: `steer=+1` musi dać dodatnie kąty kół i ujemną deltę headingu
(atan2(f.z, f.x) maleje przy skręcie ku -Z). Ta klasa błędu już się nie prześlizgnie.

## 2. Niestabilne koła przy prędkości — mechanizm znaleziony i zmierzony

Obserwacja Jozza: koła podskakują/tracą kontakt przy prędkości, najgorzej na
odciążonym przodzie; sub-steps nic nie zmieniają. Ostatnie wyklucza konwergencję
solvera. Prawdziwy mechanizm: **koło-cylinder jest fizycznie 32-kątem** (twardy
limit silnika: `b3CreateCylinder` asercja `sides <= 32`). Przy promieniu 0.514 m
ripple promienia = r·(1-cos(π/32)) ≈ **2.5 mm co ściankę** — tarka wbudowana w
samo koło. Przy 13 m/s ścianki uderzają ~127 razy/s.

Sonda pomiarowa (`RunM5WheelSmoothnessProbe`, w walidatorze, cruise na pełnym gazie,
okno 3 s, mierzony kontakt obu przednich kół i RMS ich prędkości pionowej):

```text
kształt              kontakt przodu   RMS vy przodu   top speed
cylinder 12 ścianek       5%            1.542 m/s       11.9 m/s
cylinder 32 ścianek      31%            1.170 m/s       13.1 m/s
sfera (gładka)          100%            0.413 m/s       13.2 m/s
```

Monotonia 12→32→sfera potwierdza mechanizm bez wątpliwości. **Cylinder-32
(dotychczasowy default) = przednie koła w powietrzu ~70% czasu przy prędkości.**

Decyzja: **default = sfera** (jak w stockowym samplu Driving). Cylinder zostaje
w UI (combo + suwak ścianek 3..32) jako narzędzie eksperymentalne — ustaw 8-12
ścianek, żeby ZOBACZYĆ mechanizm w akcji. Efekty uboczne sfery, uczciwie:

```text
+ kontakt 100%, agitacja pionowa 3x mniejsza
+ dryf toru prostego spadł z ~1-2.5 m do 0.02 m na 61 m (facety powodowały też dryf!)
+ skręt na postoju trafia dokładnie w targety (32.0/26.3 deg vs 31.1/25.5 na cylindrze)
+ skuteczność skrętu w ruchu ~2x lepsza (delta headingu -2.86 vs -1.49 rad)
- kontakt punktowy (brak płaskiego bieżnika)
- sfera wystaje bocznie ~0.29 m poza wizualną oponę (kolizje bokiem z propsami
  będą "za wczesne"; do zaakceptowania w labie, do rozwiązania w Etapie 2 poniżej)
```

Werdykt ws. modyfikacji silnika (polityka Jozza: najlepiej nie, chyba że duży zysk):
**nie modyfikujemy**. Jedyny kandydat — podniesienie limitu 32 ścianek — dałby
cylinder-64 z ripple 0.6 mm (nadal niezerowe), podczas gdy sfera daje zero bez
żadnego kosztu merge z upstreamem. Temat wraca dopiero przy Etapie 3 miękkiej opony.

## 3. Sprzężenie skrętu — wirtualny drążek kierowniczy + Ackermann

Obserwacja Jozza: koła skręcają niezależnie; zablokowane jedno nie powstrzymuje
drugiego — "zepsuty układ kierowniczy". Trafna intuicja: każde koło miało własne
serwo. Fizycznego drążka nie zbudujemy bez ciał zwrotnic (przyjdą z multi-body
suspension), więc M5.2 daje kinematyczne przybliżenie:

```text
LINKED (default): komenda dla koła jest przycinana do
  [kąt_faktyczny_partnera + offset_Ackermanna ± tolerancja_drążka]
  => zablokowane koło trzyma partnera przy sobie, jak sztywny drążek
INDEPENDENT: stare zachowanie, do porównań
```

Do tego geometria Ackermanna (wewnętrzne koło skręca ciaśniej):

```text
R = rozstaw_osi / tan(|kąt_rack|)
wewnętrzne = atan(rozstaw / (R - rozstaw_kół/2))
zewnętrzne = atan(rozstaw / (R + rozstaw_kół/2))
przy pełnym skręcie: 32.0° / 26.3° (zmierzone = wyliczone, funkcja
GetJozzVehicleM5SteeringTargets jest współdzielona przez runtime i walidator)
```

Opcjonalnie: speed-sensitive steering (asysta zwężająca kąt z prędkością,
default OFF — zostawiamy "raw" zgodnie z duchem labu; suwaki taperu w UI).
To częściowa odpowiedź na "ślizganie przy skręcie przy prędkości" — fizycznie
duży kąt przy 13 m/s ŻĄDA poślizgu; asysta pozwala porównać oba światy.

## 4. Nowe pokrętła i telemetria (maksymalny wpływ przez UI — wprost z feedbacku)

```text
Steering:  linkage combo, Ackermann, tolerancja drążka, speed-taper (3 suwaki)
Wheels:    tire friction (live), kształt koła (Apply), ścianki cylindra (Apply),
           rolling resistance (Apply)
Contact:   world-level contact hertz/damping/push speed (live) + reset 30/10/3
           (miękkość kontaktu = pierwszy przedsionek "miękkiej opony")
Suspension:front/rear axle scale (walka z lekkim przodem przy gazie)
Structural:CG drop (obniżenie środka masy bez ruszania zawieszenia,
           b3MakeOffsetBoxHull; default 0.15 m)
Telemetry: kontakt per koło (HUD + panel), obciążenie per koło [N]
           (b3Joint_GetConstraintForce rzutowane na oś up), dwa wykresy ImPlot
           (rolling 10 s): skok zawieszenia x4 i obciążenie x4 — widać gołym
           okiem odciążanie przodu przy gazie i pracę na tarce
```

## 5. Roadmapa "miękkiej opony" (marzenie Jozza — analiza potencjału i ryzyka)

Etapami, od zera ryzyka do dużego; każdy etap ma wartość sam w sobie:

```text
Etap 0 (JEST, od dziś): contact tuning w UI. Niższy contact hertz = opona
  "wgryza się" miękko w grunt. Pseudo-ciśnienie bez żadnej zmiany architektury.

Etap 1 (po rigu zawieszenia, zero ryzyka fizyki): WIZUALNE zgniatanie opony.
  Telemetria suspensionLoad już istnieje; mapować obciążenie na skalowanie
  segmentów opony przy gruncie. Modele Jozza są z części przygotowanych do
  skalowania - to jest dokładnie ten plan. Fizyka zostaje sztywna (separacja
  visual/physics z PROJECT_DIRECTION zachowana).

Etap 2 (średnie ryzyko, bez modyfikacji silnika): koło jako klaster sfer
  (multi-sphere envelope) - kilka sfer w poprzek szerokości daje pseudo-patch,
  naprawia też boczne wystawanie pojedynczej sfery. Koszt: więcej kontaktów,
  tuning masy. Prototyp = jedna funkcja tworząca shape'y.

Etap 3 (duże ryzyko, TO byłaby modyfikacja silnika): prawdziwa deformacja /
  model ciśnienia w kontakcie. Wchodzić tylko jeśli Etapy 0-2 nie wystarczą;
  wymaga jawnego oznaczenia w dokumentacji per polityka projektu.
```

## 6. Czego świadomie nie zrobiono

```text
- brak zmian w silniku Box3D (zbadane i odrzucone z uzasadnieniem, sekcja 2)
- corner lab M2 nietknięty
- "ślizganie przy skręcie" nie jest "naprawione" na siłę - część tego zjawiska
  to poprawna fizyka; dostałeś narzędzia (friction, taper, telemetria), żeby
  oddzielić "nie podoba mi się" od "niefizyczne", zanim coś wymusimy
```

## 7. Walidacja

```text
jozz_vehicle_validation.exe: OK (nowe asercje: znak skrętu, Ackermann
  wewnętrzne>zewnętrzne, sprzężenie drążka, sonda 3 kształtów kół)
test.exe: All Box3D tests passed
samples.exe --sample 96/95 --frames 300/240: 0 sokol errors
```

## 8. Checklist re-testu dla Jozza

```text
1. A/D bez "Invert steering" - wreszcie poprawne? (checkbox zostaje jako preferencja)
2. Prędkość przelotowa na sferach - podskakiwanie zniknęło? (HUD: contact Y/Y/Y/Y)
3. Wheels -> Cylinder 12 ścianek + Apply - poczuj tarkę, to był Twój bug
4. Skręt przy zablokowanym kole (najedź kołem na props) - drugie koło ma się
   zatrzymać przy partnerze (Linked) vs skręcać dalej (Independent)
5. Wykresy telemetrii przy gazie - przód się odciąża (load spada) - to jest to
   "lżejsze przednie zawieszenie", teraz widzialne liczbowo
6. Contact tuning: zejdź contact hertz do ~10 - miękkie, "oponiaste" lądowania
7. Stress testy: CG drop, axle scales, friction - szukaj granic modelu
```

## 9. Werdykt Jozza po re-teście (2026-07-05)

```text
A/D: naprawione.
Niestabilne koła: naprawione (sfery).
Sprzężenie skrętu (tie rod): działa dobrze.
Miękka opona: świadomie odłożona na później, roadmapa w sekcji 5 zostaje aktualna.
```

Fundamenty pojazdu (kierunki, koła, skręt, kontakt, telemetria) uznane za
wystarczające na tym etapie. M5.2 zamknięte. Następny krok: pełny rig
zawieszenia wielowahaczowego (wahacze/damper/cardan jako ciała fizyczne, nie
tylko visual-only) — to była pierwotna wizja projektu z `PROJECT_DIRECTION_PL.md`,
teraz odblokowana przez solidny fundament koła/steeringu/napędu. M4C
(proceduralny damper/cardan visual na jeżdżących narożnikach M5) jest naturalną
bramką pomostową do tej pracy.
