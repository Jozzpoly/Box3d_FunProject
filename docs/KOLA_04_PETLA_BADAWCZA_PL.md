# Pętla badawcza kół i opon — kontrakt bieżący

Status: obowiązujący proces pracy od 2026-08-04.
Bieżący stan techniczny: `CURRENT_STATE_INDEX_PL.md`.
Historia iteracji R0–R8 i dawny rejestr pytań:
`archive/wheels/KOLA_04_PETLA_BADAWCZA_LEGACY_2026-07-27_2026-08-03_PL.md`.

## 1. Produkt pętli

Format manifestu, metryk i statusów: `KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md`.

Celem nie jest jak najszybsze wybranie jednego kształtu koła. Produktem są:

1. prawa o mechanizmach, które mają wartość także dla JES;
2. przyrządy potrafiące tanio obalić zły pomysł;
3. kandydaci z jawnym zakresem działania i kosztem;
4. udokumentowane porażki, które chronią przed ponownym wejściem w ślepą uliczkę.

Decyzje właściciela pozostają nadrzędne:

- wolno badać kilka gałęzi równolegle i nie trzeba ogłaszać jednego zwycięzcy;
- negatywny wynik jest pełnoprawnym rezultatem;
- kierunkiem dopuszczonym i wybranym jest własna zdolność koła w rdzeniu Box3D;
- werdykt o prowadzeniu i feelingu należy wyłącznie do Jozza;
- nie przełączamy natury opony zależnie od kategorii podłoża.

## 2. Jedna rekurencja

Każdy cykl ma osiem małych kroków. Nie przechodzimy dalej, dopóki artefakty
bieżącego kroku nie są czytelne i odtwarzalne.

### K0 — odtwórz stan

- zapisz SHA, gałąź, czystość drzewa i użyte narzędzia;
- przeczytaj `CURRENT_STATE_INDEX_PL.md`, `TECH_DEBT_PL.md` i odpowiedni kontrakt;
- rozdziel fakty od wyników raportowanych, ale jeszcze niezreprodukowanych;
- potwierdź, że poprzednia bramka nadal jest zielona.

**Wyjście:** krótki snapshot oraz jedna lista niepewności.

### K1 — wybierz najmniejszą niepewność o największej wartości

Priorytet ma pytanie, które może unieważnić najwięcej dalszej pracy przy
najmniejszym koszcie. Nie wybieramy zadania dlatego, że jest efektowne.

Dla każdej niepewności zapisujemy:

- zdanie, które próbujemy obalić;
- najtańszy falsyfikator;
- alternatywne wyjaśnienie;
- poziom transferu potrzebny do awansu wyniku.

**Wyjście:** jedno pytanie główne. Pozostałe trafiają do `TECH_DEBT_PL.md` albo
rejestru równoległej gałęzi.

### K2 — zamroź eksperyment

Eksperyment musi mieć jedną zmienną główną. Jawnie zamrażamy co najmniej:

- geometrię i liczbę punktów kontaktu, o ile nie są badaną zmienną;
- masę i bezwładność;
- tarcie, rolling resistance i tangent velocity;
- solver, `dt`, podkroki i warm start;
- warunki początkowe, obciążenie, prędkość i drogę;
- sposób redukcji oraz próg akceptacji danych.

**Wyjście:** kontrakt A/B i lista confoundów przed uruchomieniem kodu.

### K3 — wykonaj najmniejszą odwracalną zmianę

- najpierw sonda lub tryb diagnostyczny, potem przebudowa architektury;
- nie dodawaj drugiego mechanizmu „przy okazji”;
- patch rdzenia musi mieć wpis w `JOZZ_CORE_PATCHES.json` i przejść
  `tools/jozz_core_delta.py`;
- ścieżka OFF zachowuje semantykę stockowego Box3D zgodnie z `KOLA_03`;
- nie buduj wielu stockowych colliderów, jeżeli pytanie dotyczy jednego kontaktu.

**Wyjście:** mały diff możliwy do cofnięcia bez migracji projektu.

### K4 — przejdź właściwy poziom walidacji

| Poziom | Cel | Co może rozstrzygnąć |
|---|---|---|
| V0 | matematyka i geometria poza światem | wykonalność, support, granice |
| V1 | izolowany stend | mechanizm lokalny i koszt |
| V1v | to samo doświadczenie w oknie | obserwacje i hipotezy Jozza |
| V1b | manifold lab | punkty, normalne, `featureId`, persistence, szwy |
| V2 | pełny pojazd headless | transfer do zawieszenia, kierownicy i mapy |
| V3 | przejazd Jozza | akceptacja feelingu |

Zmiana w `samples/` zwykle wymaga co najmniej V1 i V2. Zmiana kontaktu w
`src/` wymaga V0, V1b i V2; przed uznaniem kierunku także V3.

**Wyjście:** surowy artefakt, manifest, parametry przebiegu i wynik bramki.

### K5 — wykonaj audyt przeciwny

Przed awansem wyniku odpowiadamy:

1. Czy wynik można wyjaśnić inną liczbą punktów, masą, twardością lub fazą?
2. Czy miara nie nagradza zatrzymania koła albo utraty kontaktu?
3. Czy wynik przeżył zmianę płaszczyzna → mesh albo stend → pojazd?
4. Czy identyfikatory cech i warm start są stabilne?
5. Czy kontrola negatywna daje oczekiwane zero lub baseline?
6. Czy koszt podano per koło i dla właściwego builda?

**Wyjście:** lista prób obalenia oraz wynik każdej z nich.

### K6 — nadaj uczciwy status

- `OBSERVATION` — pojedyncza obserwacja, bez roszczenia mechanizmu;
- `PROVISIONAL` — świeży wynik z kompletem artefaktów;
- `SUPPORTED` — powtórzony i odporny na główne confoundy;
- `TRANSFERRED` — utrzymał się co najmniej poziom wyżej;
- `MODEL FACT` — wynika z kodu lub matematyki, z jawnym zakresem;
- `OWNER DECISION` — decyzja Jozza, nie wniosek z telemetrii;
- `REJECTED IN SCOPE` — obalony wyłącznie w zapisanym zakresie.

Nie używamy słów „prawo”, „rozstrzygnięte” ani „rodzina odrzucona” bez
artefaktów i transferu odpowiedniego do tezy.

**Wyjście:** aktualizacja `KOLA_FINDINGS.json`, dokumentu dowodowego oraz
`CHECKPOINTS_PL.md`. Nie kopiujemy tej samej narracji do pięciu plików.

### K7 — wybierz kierunek rekurencji

Po każdym cyklu wybieramy dokładnie jedną akcję:

- **w górę** — wynik podważa zjawisko, niezmiennik albo pytanie;
- **w dół** — mechanizm jest wystarczająco mocny, by projektować reprezentację;
- **poziom wyżej walidacji** — trzeba sprawdzić transfer;
- **równolegle** — niezależna gałąź może odpowiedzieć taniej;
- **archiwizuj** — gałąź jest zamknięta w swoim zakresie;
- **stop** — kolejna iteracja nie zmniejszy istotnej niepewności.

Pytanie kontrolne brzmi: **jaki tańszy eksperyment powinien był poprzedzić ten,
który właśnie wykonaliśmy?** Odpowiedź ulepsza następny cykl.

## 3. Bieżąca kolejka rekurencji

### WHEEL-RIGID-01 — rygorystyczny baseline manifoldu — ZAMKNIĘTY

Cel: oddzielić prawdziwy support sztywnej bryły od sztucznego „odcisku”
wywołanego wybieraniem wszystkich punktów profilu wewnątrz
`B3_SPECULATIVE_DISTANCE`.

A/B:

- A — obecny manifold z commita `5b92e9c`;
- B — strict support: jeden wierzchołek albo dwa końce rzeczywistego segmentu
  wspierającego; speculative distance decyduje o istnieniu kontaktu, nie o jego
  szerokości.

Zamrożone: profil, crown, masa, tarcie, world softness, podkroki, prędkość,
zawieszenie i warunki startowe.

Wynik: strict support jest wdrożony i testowany. Dwa pełne przebiegi produktu
były bajtowo identyczne i zielone. Rozdzielona telemetria pokazała dla każdego
crown `1,00 all/kolo` oraz `1,00 nios/kolo`; 3 mm nadal poprawiło zakręt
(`0,571`→`0,436 m/s²`), ale pogorszyło prostą
(`0,053`→`0,061 m/s²`). Wniosek: confound liczby punktów usunięty; crown pozostaje
zmienną geometryczną, nie podatnością.

### WHEEL-SEAM-02A — trójkąty i mesh — ZAMKNIĘTY

Finite triangle fallback rozróżnia face/edge/vertex i odrzuca tylną stronę.
Obciążony płaski szew przechodzi bez luki, z dokładnie jednym constraintem, w
obu kierunkach i trzech fazach. Wariant `~1,15°` przechodzi w obu kierunkach i
dwóch fazach; wheel-only normalna najgłębszego manifoldu usunęła zależny od
fazy skok impulsu `+36,4%`. Pełny walidator produktu nie zmienił ani bajtu.

### WHEEL-HULL-02B — krawędzie i narożniki hulla — AKTYWNY

Następna niewiadoma jest odrębna: obecny wheel–hull wybiera ścianę, lecz używa
jej jak nieskończonej płaszczyzny i nie ma kompletnego edge/axis SAT. Celem jest
clipping do polygonu ściany, brak phantom contacts i ciągłość face→edge→vertex
na rzeczywistych przeszkodach. Softness pozostaje zamrożona.

### WHEEL-SOFT-03 — lokalna podatność A/B

Ten sam profil, te same punkty i identyfikatory kontaktu. Jedyną zmienną jest
lokalna normalna softness koła. Najpierw wartości względne wobec world
`contactHertz`; parametry „realnej opony” dopiero po wyznaczeniu docelowej
sztywności pionowej.

### WHEEL-STRUCT-04 — decyzja o strukturze

Dopiero wynik WHEEL-SOFT-03 odpowiada, czy potrzebna jest lokalna struktura
bieżni/ringu. Jeżeli tak, analityczna powierzchnia pozostaje broad-phase i
zewnętrzną obwiednią, a stan odkształcenia nie może być zbiorem niezależnych
stockowych colliderów.

## 4. Bramka zakończenia iteracji

Cykl jest zamknięty tylko wtedy, gdy:

- diff odpowiada jednemu pytaniu;
- artefakty i komenda odtworzenia istnieją;
- wszystkie obowiązujące bramki są zielone;
- wynik ma zapisany zakres i confoundy;
- dokumentacja bieżąca nie przeczy kodowi;
- historia została zarchiwizowana zamiast pozostać drugim źródłem prawdy;
- następna niepewność jest mniejsza i lepiej zdefiniowana niż poprzednia.

Jeżeli którykolwiek punkt jest fałszywy, wracamy do najwcześniejszego kroku,
który go spowodował — nie dokładamy kolejnej warstwy poprawek na wierzchu.
