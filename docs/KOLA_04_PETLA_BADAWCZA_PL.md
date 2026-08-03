# Pętla badawcza kół i opon — wersja rekurencyjna

Data przebudowy: 2026-07-27 (zastępuje pętlę I0–I6 z 2026-07-25)
Korekta po audycie zewnętrznym: 2026-07-28
Podstawa: decyzje właściciela O-1…O-5 z 2026-07-27 + dowody `KOLA_01` §7
(statusy §7 są `PROVISIONAL` do czasu odbioru stendu v2.1 — `KOLA_05`)

---

## 0. Po co ta pętla istnieje i czym jest jej produkt

Właściciel rozstrzygnął dwie rzeczy, które zmieniają charakter całego programu:

> **O-1/O-2 (`OWNER DECISION`):** wolno badać kilka pomysłów równolegle i **nie
> trzeba finalnie zostać przy jednym**. Cały obecny projekt to poligon przed
> większym (JES). **Dokumentacja porażek jest warta tyle samo co sukcesów.**

> **O-5 (`OWNER DECISION`):** kontynuujemy etap badawczy **bez wyboru
> zwycięzcy**. Hybryda i jeden backend pozostają otwarte.

Poniższa hierarchia produktu jest **propozycją metodyczną agenta**
(`RESEARCH PROGRAM PROPOSAL`), a nie literalną decyzją właściciela. O-1/O-2
mówią, że wolno prowadzić wiele gałęzi i że porażki są cenne — nie rozstrzygają
same z siebie kolejności `PRAWA > PRZYRZĄD > KANDYDACI`. To samo dotyczy
kształtu `WheelState` (N-05) i wszystkich kontraktów w `KOLA_02`.

Z tym zastrzeżeniem: **produktem** tego programu nie jest „koło", tylko trzy
rzeczy, w tej kolejności ważności:

```
1. PRAWA  - zdania o mechanizmach, ktore przenosza sie do JES i pozostana
            prawdziwe niezaleznie od tego, co wybierzemy
2. PRZYRZAD - stend, ktory potrafi obalic pomysl w godzine zamiast w miesiac
3. KANDYDACI - rodziny z uczciwie zmierzonym sufitem, awansowane LUB odrzucone
```

Rodzina odrzucona pomiarem jest **wynikiem pozytywnym**. Nie kasujemy jej —
przenosimy do rejestru §6 razem z prawem, które ją obaliło, i z kryterium,
które ją przywróci.

### 0.1 Bramka przeglądu artefaktów (`GATE-A`, obowiązuje od 2026-07-27)

Powtarzający się błąd tego programu, nazwany w audycie zewnętrznym i przyjęty:

```
ciekawy wynik -> nazwanie go prawem -> zamkniecie rodziny ->
przebudowa dokumentacji -> zapis do pamieci -> DOPIERO POTEM pelny audyt
```

Od teraz każdy świeży wynik przez pierwszą rundę trafia wyłącznie do
`PROVISIONAL FINDINGS`. Przed zmianą statusu w `KOLA_00`/`KOLA_04` albo zapisem
do pamięci projektu musi przejść sześć kroków:

```
1. artefakty (kod + surowy wydruk + manifest)
2. niezalezna kontrola (przeczytanie kodu, nie tylko wydruku)
3. jawna lista confoundow
4. kontrkandydat, ktory tlumaczylby to samo
5. test transferu o poziom wyzej
6. dopiero status
```

Słów `PRAWO`, `ROZSTRZYGNIĘTE`, `RODZINA ODRZUCONA` nie wolno użyć bez
transferu na poziom V2.

---

## 1. Dlaczego pętla jest rekurencyjna, a nie liniowa

Roadmapa zakłada, że wiadomo, co jest krokiem następnym. Tu nie wiadomo —
i dwa razy pod rząd okazało się, że **odpowiedź z niższego poziomu unieważniła
pytanie z wyższego**:

- pomiar kosztu CPU (poziom 5) obalił hipotezę o barierze wydajności (poziom 2);
- prawo straty energii wielokąta (poziom 2) wykreśliło całą rodzinę
  reprezentacji (poziom 3), której szukaliśmy przez dwie iteracje.

Dlatego pętla ma **poziomy** i **regułę powrotu**, a nie etapy.

### 1.1 Poziomy

```
L0  NIEZMIENNIKI     Co musi byc prawda niezaleznie od rozwiazania.
                     Zrodla: decyzje wlasciciela, fakty o silniku.

L1  ZJAWISKA         Co fizycznie dzieje sie na styku opony, na czym nam zalezy.
                     Jednostka: obserwowalne zachowanie, nie mechanizm.

L2  PRAWA            Mechanizmy i ich zakresy. Zdania typu "X skaluje sie jak Y,
                     w zakresie Z, obalane przez W".

L3  REPREZENTACJE    Konkretna struktura danych / bryla / wiez, ktora realizuje
                     mechanizm.

L4  INTEGRACJA       Co Box3D musi naprawde zmienic, zeby to uniesc.

L5  DOWODY           Pomiar, ktory rozstrzyga. Cztery poziomy walidacji (§3).
```

### 1.2 Reguła powrotu (to jest cała rekurencja)

> Każdy wynik na poziomie `Ln` musi zostać **skonfrontowany z poziomem `Ln-1`
> i `Ln-2`**, zanim pójdziemy w dół. Jeśli wynik zmienia zdanie na wyższym
> poziomie, **wracamy tam i przepisujemy je**, nawet jeśli kosztuje to całą
> iterację.

Trzy pytania kończące każdą iterację:

```
R-a  Czy ten wynik uniewaznia ktores ZJAWISKO albo NIEZMIENNIK?
R-b  Czy ten wynik jest PRAWEM (przenosi sie), czy tylko wlasnoscia
     jednego prototypu / jednego manifoldu / jednej sceny?
R-c  Czy istnieje tansze pytanie, ktore powinno bylo pasc pierwsze?
```

`R-c` jest najważniejsze i najczęściej pomijane. Dwa razy odpowiedź brzmiała
„tak": koszt CPU trzeba było zmierzyć przed budową drogiego wariantu, a prawo
wielokąta wyprowadzić przed poszukiwaniem idealnego N.

---

## 2. Stan poziomów na 2026-07-27

### L0 — Niezmienniki

```
N-01  Ta sama podstawowa geometria kola odpowiada za kontakt z gruntem, skala,
      sciana, obiektem ruchomym i innym pojazdem. Kategorie moga filtrowac
      wyjatki, nie moga przelaczac natury opony.        [OWNER CONSTRAINT]
N-02  PERMANENTNIE ODRZUCONE (O-3): przelaczanie NATURY collidera opony
      wedlug kategorii powierzchni. Kolo ma dzialac na nierozroznionym
      meshu. NIE jest odrzucone: tagowanie skanu jako takie - tagi moga
      kiedys sluzyc materialom, semantyce, optymalizacji, gameplayowi
      i analizie.                                       [OWNER DECISION]
N-03  Felga i opona beda osobnymi assetami; wiele felg x wiele opon.
N-04  Koszt mierzony PER KOLO. Architektura nie zaklada jednego auta.
N-05  WheelState musi od poczatku umiec reprezentowac cisnienie, zuzycie,
      temperature, proste uszkodzenie, oddzielenie opony od felgi,
      zmiane parametrow w runtime - nawet jesli backend tego nie uzywa.
N-06  Trzy swiaty opon sa rownowazne: drift/przegrzewanie, zwykla jazda,
      ciezki offroad wtulajacy sie w kamienie.
N-07  Werdykt o feelu nalezy wylacznie do wlasciciela.
N-08  Kazdy patch w src/ musi miec WYLACZNIK i przejsc ZDO-S (identycznosc
      semantyczna przy funkcji OFF). To jest niezmiennik.
      ZDO-B (identycznosc BITOWA) NIE jest jeszcze niezmiennikiem - zalezy
      od otwartej decyzji D-CORE-02. Sprzecznosc z poprzednia wersja
      tego pliku usunieta 2026-07-27.
N-09  Masa i bezwladnosc kola musza byc JAWNE, nigdy pochodna objetosci
      collidera.                                        [z P-02]
```

### L1 — Zjawiska (czego chcemy od styku opony)

Lista jest celowo pisana jako **obserwowalne zachowanie**, nie jako mechanizm.
Kolumna „mierzalne" mówi, czy w ogóle umiemy to dziś zobaczyć.

```
Z-01  kolo toczy sie bez strat i bez drgan na plaskim         mierzalne  TAK
Z-02  kolo ma poprawna szerokosc - nie zahacza bokiem         mierzalne  TAK
Z-03  kolo reaguje na pochylenie: styk przechodzi
      korona -> bark -> bok                                   mierzalne  TAK
Z-04  styk jest POWIERZCHNIA, nie punktem                     mierzalne  czesciowo
Z-05  footprint zmienia sie z cisnieniem i obciazeniem        mierzalne  NIE
Z-06  opona wtula sie lokalnie w kamien / krawedz             mierzalne  NIE
Z-07  przyczepnosc zalezy od poslizgu wzdluznego i katowego   mierzalne  NIE
Z-08  przyczepnosc zalezy od obciazenia nieliniowo            mierzalne  NIE
Z-09  opona ma pamiec: relaksacja, temperatura, zuzycie       mierzalne  NIE
Z-10  kolo lezace bokiem / pojazd przewrocony zachowuje sie
      geometrycznie sensownie                                 mierzalne  TAK
Z-11  felga moze zaczac kontaktowac (uszkodzenie, debeading)  mierzalne  NIE
Z-12  koszt jest liniowy w liczbie kol                        mierzalne  TAK
```

**Sześć z dwunastu zjawisk jest dziś niemierzalnych.** To jest ważniejsza
informacja niż ranking kandydatów: nie umiemy jeszcze zobaczyć większości tego,
o co nam chodzi. Rozbudowa przyrządu (L5) ma pierwszeństwo przed wyborem bryły.

### L2 — Prawa (stan na dziś)

```
P-01  Limit formatu hulla: 6N polkrawedzi <= 255 -> pryzmat <= 42 scianki,
      profil o 4 pierscieniach <= 18 scianek.              ENGINE FACT (F-01)
P-02  Masa z objetosci collidera zmienia sie 1.58-1.87x miedzy obwiedniami.
      Kazde porownanie ksztaltow bez zamrozonej masy jest niewazne. BENCH FACT (F-02)
P-03  Churn tozsamosci kontaktu rosnie liniowo z liczba scianek,
      amplituda tarki maleje jak 1/N^2. Nie ma dobrego N.
      PROVISIONAL - wspolsprawca silnie poparty, nie dowiedziony
      jako mechanizm dominujacy. Wymaga stalej predkosci
      i normalizacji na metr/obrot.                         (F-10)
P-04  Strata energii toczacego sie wielokata ~ 4pi^2 (mR^2/I_c) / N na obrot,
      I_c = I_cm + mR^2. Dla N=32 to ~2.25% energii na wierzcholek
      i ~51% na obrot. Dla 1% trzeba N~2300. Limit formatu to 42.
      MODEL FACT (idealny rimless wheel) + BENCH WARNING.
      OTWARTE: transfer na stala predkosc, quarter-car, pojazd
      i inne procedury kontaktu.                            (F-11)
P-05  Globalny sweep stockowego contactHertz nie naprawia fasetowanych
      hulli w tescie constant-downforce: podatnosc dziala przez
      penetracje, a faseta na plaskim gruncie penetruje ~0.1 mm.
      OBALONY SUWAK, NIE CALA RODZINA PODATNOSCI.           (F-12)
P-06  Kazda bryla obrotowa ma krzywizne obwodu dokladnie R; kazdy hull ma
      w tym miejscu narozek.                                MATH FACT (F-13)
P-07  Elipsoida: rho korony = (W/2)^2 / R. Dla naszego kola 0.093 m, czyli
      ostrzej niz opona motocyklowa. Zero swobodnych parametrow profilu.
                                                            MATH FACT (F-13)
P-08  Rodzina revolved Lame nie ma czlonka o koronie posredniej: dla p=2
      rho=0.093, dla kazdego p>2 krzywizna w apeksie = 0.
      Przejscie nieciagle.                                  MATH FACT (F-13)
P-09  Plaska korona powoduje, ze pochylone kolo robi sie WIEKSZE
      (prism-42: 0.514 -> 0.558 przy 20 stopniach), bo skraj barku lezy
      dalej od osi niz srodek korony.                       MATH FACT (F-13)
      "Realna opona odwrotnie" - WYCOFANE, brak zrodla.
P-10  Asymetria kierunkow: czestotliwosc przejsc miedzy cechami wokol
      obwodu wynosi N*v/(2 pi R) ~ 129 Hz przy N=32 i 13 m/s, a w poprzek
      biezni jest ograniczona tempem zmiany cambera (rzad jednostek Hz).
      Roznica ~2 rzedy wielkosci.
      STRONG DESIGN INFERENCE (F-14). Wymaga pomiaru przejsc
      korona-bark-bok w orientacjach ekstremalnych.
P-11  Tarcie w Box3D jest CENTRALNE na manifold - jeden `frictionImpulse`
      niezaleznie od liczby punktow.                        ENGINE FACT
P-12  `b3_compoundShape` nie ma zarejestrowanej pary z mesh ani height.
                                                            ENGINE FACT
P-13  Telemetria manifoldu (featureId, persisted, impulsy) jest dostepna
      publicznym API. Zaden patch core nie jest do tego potrzebny.
                                                            ENGINE FACT
```

### L3 — Reprezentacje: statusy

```
sfera                  KONTROLA. Toczy sie idealnie (vx_end 12.93/13.0),
                       ale +295 mm szerokosci; wysokosc jazdy identyczna
                       przy kazdym camberze (izotropia), wiec geometria
                       nie odroznia korony, barku i boku.
hull fasetowy          BARDZO PODEJRZANY jako powierzchnia toczna.
                       NIE zamkniety: P-04 to model idealny + ostrzezenie
                       ze stendu o znanych wadach (KOLA_01 §7.9).
                       Bramka: rig Q2 (stala predkosc) w KOLA_05.
                       Nadal dopuszczalny dla BOKU i barku.
elipsoida              KANDYDAT WASKI. Sufit zmierzony (P-07). Zero parametrow.
                       Reaguje na camber geometrycznie, ale nie daje
                       footprintu, camber thrust ani pneumatic trail.
swept-disk             KANDYDAT. Plaska korona (pelna szerokosc 277 mm),
                       sterowany bark, ale P-09 i skok wsparcia 140 mm.
revolved Lame          KANDYDAT Z LUKA (P-08).
revolved convex profile OTWARTA HIPOTEZA KONSTRUKCYJNA (P-10).
                       Nie sprawdzono manifoldu ani niczego z L4.
                       Do porownania w R3 wchodza tez: luki odcinkowe,
                       splajn scisle wypukly, probkowane wsparcie,
                       zaokraglony wielokat, ogolny support-mapped convex.
podatnosc kontaktu     Globalny contactHertz OBALONY jako zamiennik
                       geometrii (P-05). Rodzina podatnosci NIE obalona:
                       wlasny wiez opony, elastyczny pierscien, model
                       enveloping i struktura pozostaja otwarte.
wiele shape'ow         OTWARTE. Koszt zmierzony (union-4 = 6.07x sfery na meshu).
struktura miekka       OTWARTE. Kompletny koszt wciaz nieznany.
model aplikacyjny      OTWARTE, niedoceniony. Nie narusza N-01, jesli collider
                       zostaje i probki tylko dokladaja wiezy.
prawo opony            ORTOGONALNE do wszystkich powyzszych. Dziala na kazdym
                       backendzie. Obsluguje Z-07..Z-09.
```

### L4 — Integracja: nic nie ruszamy

Zero wpisów w ledgerze `KOLA_03` §5.1. Warunki C1–C4 niespełnione dla każdego
kandydata. Reguła: **żaden wpis, dopóki nie zostanie nazwane konkretne brakujące
pole albo zdarzenie publicznego API**, a dla nowej bryły — dopóki nie istnieje
osobny dowód wykonalności manifoldu (§3, poziom V1b).

---

## 3. Poziomy walidacji

```
V0  MATEMATYKA        Poza silnikiem, bez fizyki. Profil, wsparcie, krzywizna,
                      pozy ekstremalne. Godziny. Obala tanio.
V1  STEND             tools/jozz_wheel_bench. Mechanizm w izolacji. Godziny.
V1v OKNO WIZUALNE     samples "Jozz Wheel / Wheel Scope". TEN SAM rig co V1,
                      widziany i sterowany na zywo. Minuty. NIE akceptuje
                      i nie odrzuca - dostarcza obserwacje i hipotezy oraz
                      pozwala wlascicielowi wplywac na kierunek badan PRZED
                      wyborem kandydata. Wspolnota fizyki z V1 jest
                      dowodzona bajtowo: check_visual_equivalence.py.
V1b MANIFOLD LAB      Osobny dowod wykonalnosci kontaktu dla nowej bryly:
                      shape-plaszczyzna, -trojkat, -krawedz, -hull, przejscia
                      miedzy trojkatami, featureId, persisted, warm start,
                      plaska korona i maly bark, replay/determinizm.
                      WARUNEK KONIECZNY przed jakimkolwiek patchem core.
V2  SONDA PRODUKTOWA  Headless, pelny pojazd, walidator z ROOTA repo.
                      Sprawdza TRANSFER: czy wynik ze stendu dotyczy auta.
V3  PRZEJAZD JOZZA    Jedyny poziom, ktory AKCEPTUJE. Werdykt o feelu.
```

Reguły:

- **V1 blokuje wyłącznie na crashu, niestabilności i złamanym niezmienniku.**
  Nie blokuje na jednej słabej metryce — to była wada poprzedniej wersji pętli.
- **Nowy istotny mechanizm fizyczny nie przechodzi kilku iteracji bez
  odpowiednika w V1v.** Wprowadzone 2026-07-30 po audycie kolejności prac:
  drabina prowadziła V0 → V1 → V1b → V2 → V3, więc pierwszy obraz dla
  właściciela wypadał **po** bramce do patcha core i po integracji z pojazdem.
  Program powstał z obserwacji V3-owej (kolizja sferyczna na skałach), a plan
  stawiał ten poziom na końcu. V1v jest tani (minuty) i nie zastępuje V3:
  V3 nadal jest jedynym poziomem, który AKCEPTUJE.
- **Sesja z ingerencją w fizykę nie jest przebiegiem dowodowym.** V1v rozdziela
  to praktycznie: `OBSERVATION` (bez ingerencji) i `EXPLORATION` (po ingerencji,
  flaga lepka, widoczna na ekranie i w zapisie obserwacji). Droga do faktu:
  obserwacja → jawna hipoteza → poprawka kontraktu **przed** przebiegiem →
  zamrożony eksperyment headless.
- **Dowód skaluje się z nieodwracalnością.** Zmiana w `samples/` — V1+V2.
  Zmiana w `src/` — V0+V1+V1b+V2+V3+próba aktualizacji upstreamu.
- **Każdy pomiar podaje zakres sceny.** Bez zakresu wynik nie wchodzi do `KOLA_01`.
- **Confound musi być nazwany w tym samym akapicie co liczba** (przykład: F-09,
  gdzie `vy_rms` jest nieporównywalne między wierszami, bo warianty fasetowe
  się zatrzymują, a nieruchome koło ma z definicji niską agitację).

---

## 4. Iteracje

Iteracje **nie są etapami roadmapy**. Są wejściami do pętli. Kolejność jest
propozycją; reguła powrotu (§1.2) może ją przestawić w każdej chwili.
Zgodnie z O-1 **kilka gałęzi wolno prowadzić równolegle**.

### R0 — RESET — `OPEN FOR CORRECTION`, **nie zamknięta**

Przebudowa instrumentu i przepisanie pytania od zjawisk, nie od brył.
Produkt: stend v2, `KOLA_01` §7, hipotezy P-03…P-10.

Zamknięcie R0 z 2026-07-27 **cofnięte** po audycie zewnętrznym: instrument ma
dwanaście udokumentowanych wad (`KOLA_01` §7.9), a dwie z nich (ukryty rozbieg,
błędna nazwa rigu) zanieczyszczają wszystkie metryki jakości toczenia.
**R0 zamknie się dopiero wraz z odbiorem stendu v2.1** (`KOLA_05`).

### R0.5 — KALIBRACJA INSTRUMENTU (`KOLA_05`) — bramka do wszystkiego dalej

Rigi Q0–Q4, słownik metryk, manifest dowodowy, rejestr confoundów.
**Dopóki nie działa, żaden wynik nie awansuje na prawo.** Protokół jest
zaprojektowany i opisany **przed** uruchomieniem — to warunek, żeby wynik dało
się odróżnić od artefaktu narzędzia.

Stan szczebli: **Q2A zbudowany i zamknięty bramkami** (2026-07-31 — konfiguracja
jako plik, blokada zachowania, ekwiwalencja okno/stend). Następny szczebel: Q3.

#### Etap `Q3 — Quarter Car Lab` — plan wykonawczy (przyjęty 2026-07-31)

**Decyzje właściciela z tej sesji.** Nazwa: `Q3`, wewnątrz istniejącej drabiny —
propozycja „W2" odrzucona, bo `W2` jest już warstwą architektury (`KOLA_02` §4)
i jedna nazwa nie może wskazywać na warstwę kontraktu i na stend naraz.
Domknięcie etapu: **rig zwalidowany na znanych obwiedniach** (sfera,
pryzmat-N); nowy kandydat ląduje w etapie następnym, na zaufanym już rigu.

**Decyzja agenta (reguła twarda 1 — zamrożona masa).** W tym etapie masa
nieresorowana jest **zamrożona** między kandydatami, więc porównujemy geometrię.
Kandydaci o różnej masie to `U-23`, otwierane dopiero po `Q3-1`.

Architektura: kandydat wychodzi z rigu i staje się osobnym bytem, wspólnym dla
obu stendów — to mechaniczna podstawa reguły transferu (`KOLA_05` §1):

```
JozzWheelSpec         KANDYDAT: felga + opona, geometria, masa, bezwladnosc, material
   |                  jeden plik opisuje kandydata dla OBU rigow
   +-- JozzRigConfig  Q2A: swobodne kolo, sila w srodku masy          [istnieje]
   +-- JozzQcConfig   Q3:  piasta + zawieszenie + masa resorowana     [nowe]
                           + moment + profil drogi
```

| # | Krój | Produkt | Bramka domykająca |
|---|---|---|---|
| `Q3-0` | kontrakt przed pierwszym przebiegiem | `Q3_QUARTER_CAR_CONTRACT.md`: masy, sprężyna w N/m, tłumienie, skok, profil drogi, protokół momentu, co unieważnia przebieg | dokument istnieje **przed** kodem rigu |
| `Q3-1` | **sonda silnika** | pomiar mapy `hertz ↔ N/m`, wrażliwość na podkroki, weryfikacja osi obrotu przez `τ/α` (`KOLA_05` §1.3) | nowe wpisy w `KOLA_FINDINGS.json` (numery nadawane **razem z pomiarem**, nie z góry) + surowe logi w `evidence/` |
| ~~`Q3-2`~~ | ~~kandydat jako osobny byt~~ | **SKREŚLONY 2026-07-31** — wydzielenie `JozzWheelSpec` było architekturą na zapas. Q3 woła istniejące `JozzRig_BuildEnvelopeEx`/`FreezeMassEx`, które już biorą jawne parametry. Wspólna maszyneria serializacji powstanie **w chwili**, gdy pojawi się druga konfiguracja — nie wcześniej | — |
| `Q3-3` | rig Q3 headless | `jozz_qc_rig.c/.h`, `--qc-trace`, `--qc-compare` | **ZAMKNIĘTY 2026-07-31**: samokontrola konstrukcji odmawia pracy, gdy zbudowany układ ≠ zamówiony |
| `Q3-4` | okno `Quarter Car Scope` | `samples/sample_jozz_quarter_car.cpp` na TYM SAMYM rigu, wybór kandydata i drogi, okno pomiarowe na żywo | **ZAMKNIĘTY 2026-07-31**: obejrzane na zrzucie, nie zadeklarowane |
| `Q3-5` | wzorzec zachowania | golden dla Q3, wpis w `check_all.py` | **OTWARTY** — dopiero po akceptacji rigu przez właściciela w oknie (reguła pracy 3) |
| `Q3-6` | pierwsze porównanie | macierz kandydat × droga × prędkość, 3 powtórzenia na komórkę | **ZAMKNIĘTY 2026-07-31**: `F-21`…`F-24`, surowy przebieg `evidence/run_q3_compare_2026_07_31.txt` |
| `Q3-7` | **nowa obwiednia `torus-N`** | pierścień N kapsuł jako trzeci wariant `JozzRigVariant`, dostępny także w Q2A | **ZAMKNIĘTY 2026-07-31**: aneks §4.1 kontraktu, decyzja właściciela uchyliła zakaz nowych obwiedni w tym etapie |

**Stan na 2026-07-31: `Q3-0` i `Q3-1` ZAMKNIĘTE.** Kontrakt:
`tools/jozz_wheel_bench/Q3_QUARTER_CAR_CONTRACT.md`. Sonda: `--qc-probe`,
surowy przebieg `evidence/run_qc_probe_2026_07_31.txt`. Sonda **zmieniła projekt
rigu w dwóch miejscach** — dokładnie po to szła pierwsza:

1. **Napęd przez `b3Body_ApplyTorque`, nie przez silnik więzu** (`F-20`: silnik
   daje 2,018× zadanego momentu, ze współczynnikiem zależnym od podkroków).
2. **Protokół domyślny to stała prędkość z mierzonym momentem**, nie stały
   moment — inaczej kandydaci jadą z różnymi prędkościami i wraca dokładnie ten
   confound, który `KOLA_05` §1.1 opisuje jako główną wadę stendu v2.

Trzy rzeczy ustawione **wbrew odruchowi**, każda z powodem:

1. **Sonda silnika przed rigiem.** `Q3-1` może zmienić projekt `Q3-3`. Budowa
   najpierw oznacza przebudowę po pomiarze — albo przemilczenie pomiaru.
2. **Wzorzec zachowania na końcu.** Blokada założona na rig, którego nikt nie
   oglądał, zamraża błąd i nadaje mu status faktu. Q2A dostał wzorzec po
   akceptacji i tak samo ma być tutaj (reguła pracy 3, `KOLA_00`).
3. **Okno służy feelowi, stend liczbom.** Hot-swap A/B jest konieczny, bo pamięć
   feelu jest krótka — ale przebudowa ciała w trakcie przebiegu **znaczy sesję
   jako A/B, nie jako pomiar**. Pomiary idą przez stend, jeden kandydat na
   przebieg. Maszyneria oznaczania zaburzeń już to potrafi (V1v, §3).

### Stan na koniec 2026-07-31: etap zamknięty poza `Q3-5`

Właściciel uchylił ograniczenie „żadnych nowych obwiedni w tym etapie" i polecił
skończyć **działającym nowym systemem koła**. Rig, okno i nowa obwiednia powstały
w jednej sesji; ryzyko z reguły twardej 3 (nowy rig + nowy kandydat naraz)
zostało spłacone tym, że **sfera i pryzmat są w tej samej tabeli** — gdyby rig
był dziwny, byłyby dziwne razem z torusem.

Zmierzone: `F-21` (torus-64 bije prism-42 o 33% straty i 83% `a_rms` przy 4 m/s),
`F-22` (**wygrywa mimo o 57% większego tętnienia promienia** — decydują ostre
krawędzie, nie amplituda), `F-23` (cena: 13× CPU), `F-24` (przy 13 m/s pojedynczy
przebieg nie jest powtarzalny na poziomie progu ważności — stąd 3 powtórzenia na
komórkę).

Dwie rzeczy złapane przez konstrukcję, nie przez test:

1. **Skok zawieszenia liczony od zera** — samokontrola odmówiła budowy, bo samo
   ugięcie statyczne (111 mm) przekraczało zamówiony skok 100 mm. Kontrakt §5
   podawał tylko sztywność; teraz podaje bump/droop **od punktu statycznego**.
2. **Niewidoczna masa resorowana** — `categoryBits = 0` wycinało nadwozie także
   z `b3World_Draw`, więc okno pokazywało koło wiszące w powietrzu. Zobaczone na
   zrzucie. `maskBits = 0` w zupełności wystarcza, żeby nic nie kolidowało.

#### Rewizja 2026-07-31 (druga sesja): okno staje się warsztatem, `Q3-8`

| krok | co | wynik | stan |
|---|---|---|---|
| `Q3-8` | **warsztat w oknie** | protokół pomiaru wspólny, konstrukcja `.qc`, strojenie na żywo, przemiatanie | **ZAMKNIĘTY 2026-07-31** |

Właściciel po pierwszej jeździe: kierunek dobry, ale okno „bardzo surowe
i niewygodne, część kontrolek nie działa poprawnie, zbyt mały wpływ na rig".
Diagnoza po przeglądzie kodu i po zrzutach: to nie był brak suwaków, tylko
**cztery różne blokady eksperymentowania**.

1. **Każde pokrętło przebudowywało rig**, a przebudowa startuje 80 m przed
   punktem pomiaru — więc po każdym ruchu suwaka trzeba było odczekać 2 s
   dojazdu (przy spowolnieniu 1:8 — pół minuty). Teraz sprężyna, tłumienie,
   skok, tryb napędu i prędkość zadana idą **na żywo** przez `JozzQc_Set*`,
   a to, co musi przebudować, **od razu wykonuje rozgrzewkę w jednej klatce**.
2. **Okno nie umiało zmierzyć.** Protokół (rozgrzewka, okno, 3 powtórzenia,
   lista kandydatów) siedział w `wheel_bench.c`, więc jedyną drogą do liczby
   bench-grade z okna byłoby napisanie **drugiej kopii protokołu**. Przeniesiony
   do `jozz_qc_rig.c`; oba frontendy wołają `JozzQc_MeasureCell`. Sprawdzone:
   okno i stend dają te same liczby (churn 89,7 / 70,1 / 8,6 / 0,0).
3. **Nie dało się odłożyć przypadku.** Q2A miało `.rig`, Q3 nie miało nic.
   Format `.qc` opisuje cały narożnik, ma ten sam potrójny strażnik i tę samą
   bramkę; `--qc-config` w stendzie zamyka pętlę okno → stend. Sprawdzone:
   plik i flagi dają przebieg identyczny co do bajtu.
4. **Dało się zbudować ślepy zaułek.** Za mały promień korony (nieszczelny
   pierścień) albo `torus-64` → pryzmat (limit hulla) kończyły się napisem
   „rig nie powstał" bez drogi powrotnej. Teraz konstrukcja jest **wciskana
   w zakres budowalny z jawnym komunikatem**, a nieudana budowa zostawia
   przycisk powrotu do ostatniej działającej.

Rozszerzenie workflow: **przemiatanie jednego parametru** — w oknie i w stendzie
(`--qc-sweep --qc-sweep-param segments|crown|spring|speed|obstacle`). A/B dwóch
konstrukcji odpowiada „która lepsza"; przemiatanie odpowiada „co ten parametr
właściwie robi", a od tego zaczyna się każdy nowy system koła.

**`F-25` — pierwszy wynik, który dał ten warsztat, i obala mój własny zapis.**
W kodzie (`jozz_wheel_rig.h`, pole `crownR`) stało napisane, że promień korony
handluje gładkość za szerokość płaskiej bieżni. Przemiatanie 0,04 → 0,19 m dla
`torus-64` przy 4 m/s pokazuje **monotoniczną poprawę wszystkiego**: strata
795,2 → 457,1 W (−43%), churn 26,9% → 0,0%, `sprung_accel_rms` −34%, przy
zwężeniu płaskiej bieżni z 358 do 58 mm. Rozrzut zerowy w każdym z 6 punktów.
Cena jest, ale gdzie indziej: **CPU rośnie 1,7×** (0,048 → 0,082 ms/krok), bo
większe kapsuły dają więcej punktów styku. Dlaczego to nie zamyka sprawy: Q3 na
płaskiej płycie **nie stawia kołu siły bocznej ani pochylenia**, więc nie może
zmierzyć, po co płaska bieżnia istnieje. Kompromis, jeśli istnieje, leży poza
tym szczeblem — i to jest pytanie dla `W3`, nie dla Q3.

Poza zakresem etapu, świadomie: prawo opony (`W3`) i prowadzenie boczne,
rozszerzanie `JozzRigConfig` o pola zawieszenia (martwe pola w Q2A i zmiana
nagłówka `# config` we wzorcach bez zmiany fizyki) oraz `jozz_vehicle_m6_
suspension_rig.cpp` (1404 linie maszynerii pojazdu wpiętej w assety, które
`KOLA_02` §3 nazywa blokerem; bierzemy stamtąd liczby i lekcję, nie kod).

### R1 — WIDZIALNOŚĆ (najwyższy priorytet po R0.5) — **pierwszy punkt ZAMKNIĘTY 2026-08-03**

**Pytanie:** sześć z dwunastu zjawisk (Z-04…Z-09, Z-11) jest dziś niemierzalnych.
Ile z nich da się zobaczyć bez dotykania `src/`?

Zakres:
- ~~rozkład nacisku po punktach manifoldu w czasie → czy Z-04 jest w ogóle
  widoczny~~ — **ZROBIONE, `F-31` + `P-18`.** Dane były w `b3ContactData` od
  początku i były wyrzucane po zsumowaniu. Odpowiedź: **odcisku nie ma** —
  1–3 punkty efektywne, rekord rodziny ~5 przy 576 kształtach i 32 podkrokach.
  Liczba kształtów i liczba podkroków działają multiplikatywnie i obie są
  konieczne; sama geometria nie wystarcza.
- ile manifoldów i punktów nośnych daje kontakt z **meshem** (nie z pudłem) —
  bo teren ze skanu jest meshem, a P-11 mówi, że każdy manifold ma własną
  kotwicę tarcia; to może być cichy sterownik feelu. **NADAL OTWARTE i po
  `F-31` ważniejsze niż było**: cała rodzina została zmierzona na gruncie
  pudełkowym, a produkt jeździ po meshu;
- czy `b3ShapeCast` wzdłuż profilu opony da tanią miarę „co jest pod kołem",
  jako *przyrząd pomiarowy*, jeszcze nie jako model. **OTWARTE.**

Falsyfikator R1 (**zawężony po krytyce**): jeżeli rozkład nacisku po punktach
jest degeneracyjny (1 punkt niesie > 90% obciążenia), wniosek brzmi
**„obecna para bryła + procedura manifoldu nie dostarcza użytecznego rozkładu
nacisku"** — a nie „Z-04 jest nieosiągalny żadną bryłą sztywną". Wynik zależy
od bryły, generatora manifoldu, gruntu i algorytmu redukcji punktów; inna para
może rozłożyć impulsy inaczej.

**Rozstrzygnięcie falsyfikatora (2026-08-03).** Próg 90% przekracza tylko
`torus-32` przy 13 m/s (`max` 92,2%). Reszta rodziny mieści się w 39–77%, czyli
**formalnie falsyfikator nie zapalił się** — ale liczba, którą zapala, była
źle dobrana. Prawdziwa granica nie leży na „jednym punkcie", tylko na **skali**:
39% na najgorszym punkcie przy pięciu punktach efektywnych to nadal nie jest
plama kontaktu, tylko pięć gwoździ. Falsyfikator zostaje w zapisie taki, jaki
był (reguła twarda 6), a wniosek formułujemy na `nios`, nie na `max%`.

### R1b — JAK KOŁO JEDZIE (przyrząd, którego nie było) — 2026-08-03

**Skąd się wzięło.** Właściciel przejechał się autem na nowych obwiedniach
i zgłosił, że **wszystkie szarpią i podskakują przy prędkości**, najgorzej
w drifcie, a **sfera jeździ gładko** — ma za to wady, dla których cały program
powstał. Program nie mógł tego zobaczyć **w zasadzie**, bo mierzył co innego:

| przyrząd | co mierzy | czego nie mierzy |
|---|---|---|
| stend Q3 | jedno koło, płyta, tempo spaceru | osi skrętu, nadwozia, prędkości, poślizgu |
| macierz stresu walidatora | drżenie **na postoju** po skrypcie znęcania | drżenia **w trakcie jazdy** |

Czyli: jedyna wielkość, która decyduje o tym, czy koło nadaje się do jazdy,
nie miała liczby. To nie jest pomyłka w wyniku — to **dziura w metodzie**,
i wszystkie werdykty wydane w warsztacie są o tyle podejrzane, o ile
opierały się na założeniu, że stend przewiduje jazdę.

**Przyrząd:** `samples/validation/jozz_probes_ride.cpp`, sonda diagnostyczna
w walidatorze (wzorem `straight-pull diagnosis` — drukuje, nie bramkuje).
Idealnie płaska płyta, więc **koło idealnie okrągłe musi pokazać zero**;
prędkość trzymana regulatorem, żeby porównywać przy tej samej prędkości,
a nie przy prędkości maksymalnej każdego kandydata. Masa zamrożona dla
wszystkich obwiedni (przy okazji spłacone `U-32`).

**Wyniki:** `F-35` (dzisiejsze koło jest w jeździe fizycznie sferą — boczny
walec nigdy nie dotyka gruntu), `F-36` (każda obwiednia inna niż sfera szarpie
25–40×, i **nie zależy to od liczby kształtów**: 1 walec ≈ 64 kapsuły ≫ sfera),
`F-37` (podkroki nie leczą — pogarszają).

**Co to znaczy dla programu.** `F-31` powiedział, że sztywna bryła wielokształtna
nie daje odcisku. `F-36` mówi, że **ta sama rodzina nie potrafi się też toczyć**.
Rodzina nie dostarcza więc żadnej z dwóch rzeczy, dla których była budowana —
i nie jest to kwestia budżetu, bo ani kształty (`F-36`), ani podkroki (`F-37`)
nie kupują poprawy. Otwarte po tym: `U-33` (czy silnik w ogóle ma bryłę
obrotowo symetryczną szerszą niż punkt), `U-34` (mesh vs płyta), `U-35`
(wybór kierunku — decyzja właściciela).

### R2 — ATLAS ZJAWISK TRZECH ŚWIATÓW

**Pytanie:** czym różnią się mierzalnie drift, zwykła jazda i offroad skalny?

Nie „jakiej opony potrzebują", tylko: jakie **wielkości obserwowalne** muszą
istnieć w każdym z nich i które z nich dzielą. Produkt: tabela zjawisko × świat
z kolumną „czy jeden backend może to unieść".

To jest iteracja, która **rozstrzyga pytanie o hybrydę** — dziś odpowiadamy na
nie intuicją.

### R3 — LABORATORIUM PROFILU (V0, rozszerzenie)

Rozbudowa eksperymentu G ze stendu:
- profil jako **wypukły wielokąt 2D obrotowy** (P-10) — pełna parametryzacja;
- 5–6 realnych profili opon, nie tylko obecny asset offroad;
- sweep rozmiarów i proporcji (R/W od 1.2 do 4);
- porównanie wsparcia i normalnych z gęstym meshem referencyjnym;
- pozy: pion, camber, jedno koło, bok, ściana, krawędź, dwie powierzchnie,
  pojazd przewrócony;
- **kontrkandydaci obowiązkowo**: jeden prostszy (swept-disk), jeden ogólniejszy
  (dowolne wsparcie).

Falsyfikator R3: jeżeli profil wielokątny obrotowy nie potrafi odtworzyć korony
o `rho` 0.5–1.5 m przy jednoczesnym barku 10–25 mm, rodzina traci główną
przewagę nad swept-diskiem i schodzi do remisu.

### R4 — PRAWO OPONY BEZ GEOMETRII (gałąź równoległa)

Ortogonalna do całej reszty. Sprawdza, ile z Z-07…Z-09 da się dostać na
**dzisiejszej sferze**, bez zmiany bryły.

**Zakres dopuszczalnych wniosków ze sfery** (zawężony po krytyce): elipsa
tarcia, krzywe poślizgu, wrażliwość na obciążenie, termika jako model stanu —
i tylko na płaskim gruncie. **Nie wolno** z tego rigu wnioskować o camberze,
kontakcie barku, boku, konformowaniu w offroadzie ani o przewróconym pojeździe.

Teza „trzy światy żyją głównie w prawie opony" jest trafna dla driftu i zwykłej
jazdy. **Ciężki offroad zależy fundamentalnie od geometrii, podatności, kontaktu
na krawędziach i rozkładu nacisku** — nie wpisywać tego jako ogólnego prawa.

Falsyfikator R4: jeżeli centralne tarcie na manifold (P-11) nie pozwala
odtworzyć elipsy tarcia i wrażliwości na obciążenie, to `B3X-TIREFRAME-001`
staje się pierwszym realnym kandydatem do core — i to **przed** jakąkolwiek nową
bryłą.

### R5 — MANIFOLD LAB (V1b) — bramka do core

Uruchamiana **tylko** dla kandydata, który przeszedł R3. Bez tego dowodu żaden
patch core nie powstaje.

### R6 — KOSZT FORKA NA PRAWDZIWYM DIFFIE

Dopiero po R5: realny diff dla danych i API, dispatchu, masy/AABB, generacji
kontaktu, zapytań/castów/CCD, snapshotów/nagrań, debug draw, testów, próby
aktualizacji upstreamu. **Liczba `case` nie jest kosztem.**

### R7 — KONFRONTACJA (V3)

Przejazd właściciela. Jedyny poziom akceptujący.

### R8 — GAŁĄŹ PODATNA / ENVELOPING (równoległa, otwarta)

Gałąź, której globalny `contactHertz` **nie** zamknął. Do porównania:
własny więz normalny opony, pierścień sztywno-elastyczny, model obejmujący
nierówność (*enveloping road*), minimalny miękki pierścień, model plamy
kontaktu, pełna struktura. Literatura modelowania opon (elastyczne pierścienie,
rozkład nacisku, walidacja quarter-car / over-cleat) jest **źródłem mechanizmów
do porównania, nie gotowym systemem do skopiowania**.

### R∞ — DECYZJA UPSTREAM (niezależna od geometrii, starzeje się)

`D-CORE-03`. Nie zależy od żadnej z powyższych i z każdym tygodniem drożeje.

---

## 5. Rejestr pytań otwartych

```
U-01  OTWARTE ZAWEZONE. Churn jest silnie poparty jako wspolsprawca.
      Brakuje korelacji czasowej przy STALEJ predkosci.            -> Q2
U-02  OTWARTE ZAWEZONE. Test constant-downforce wykonany.
      Quarter-car i pelny pojazd nierozstrzygniete.                -> Q3/Q4
U-03  WSTEPNE. Koszt krancowy zmierzony, ale kazdy wariant byl
      w innym stanie dynamicznym i przy innym allowFastRotation.   -> Q1c
U-04  OTWARTE ZAWEZONE. Globalny contactHertz negatywny.
      Podatnosc wlasciwa oponie nietknieta.                        -> R8
U-17  Kto jest WLASCICIELEM oporu toczenia dla nowej bryly i czy
      nie liczymy go podwojnie z prawem opony. Cztery osobne
      pytania: API / solver / geometria (promien efektywny na
      koronie, barku, boku) / prawo opony.                         -> Q2
U-18  Czy metryki na METR i na OBROT odwracaja ranking F-09.       -> Q2
U-19  Ile wynosi vx i omega na POCZATKU okna pomiarowego.          -> Q0/Q1
U-05  Ile kosztuje KOMPLETNA struktura miekka (ciala+jointy+cisnienie+substepy).
U-06  Czy pasy w poprzek walcza ze soba jak phased-union.
U-07  Czy kontakt liniowy ma stabilna tozsamosc cech i warm start na trojkatach.
U-08  Realny diff nowej bryly w tym konkretnym kodzie.
U-09  WYCOFANE (dotyczylo rodziny "podatnosc zamiast geometrii").
U-10  Czy trzy swiaty moga dzielic jeden backend.                 -> R2
U-11  Kontrolowane porownanie stend vs pojazd.                    -> R1/V2
U-12  WYCOFANE. Role skanu odrzucone permanentnie (N-02).
U-13  ZAMKNIETE 2026-08-03 -> F-31 (i P-18). Zmierzone: caly ciezar
      stoi na 1-3 punktach efektywnych, rekord rodziny ~5 przy
      576 ksztaltach i 32 podkrokach (1.59 ms/krok na JEDNO kolo).
      Prog degeneracji falsyfikatora R1 przekracza tylko torus-32
      przy 13 m/s (max 92.2%). Odpowiedz brzmi wiec: TA rodzina
      obwiedni nie daje odcisku po zadnej dostepnej cenie - a nie
      "zadna bryla sztywna go nie da".                            -> R8
U-29  Czy sufit ~5 punktow efektywnych jest wlasnoscia RODZINY
      (pierscien kapsul o osi rownoleglej do osi kola), czy
      procedury manifoldu. Kapsula lezaca na plaszczyznie daje
      styk LINIOWY, wiec do czterech punktow na pare - czyli
      teoretyczny sufit dla 576 ksztaltow to setki punktow, a
      dostajemy piec. Rozstrzygnie to inna rodzina o tej samej
      liczbie ksztaltow (np. pas malych sfer albo pudelek).       -> R1/R3
U-30  ZAMKNIETE 2026-08-03 -> F-34 (koszt) + F-37 (skutek).
      Pytanie brzmialo "ile podkrokow kosztuje odcisk W POJEZDZIE"
      i zakladalo, ze podkroki sa waluta, za ktora kupuje sie
      jakosc. NIE SA - dla tej rodziny obwiedni. Sweep 4/8/16/32
      przy 16 m/s: sfera poprawia sie monotonicznie
      (0.061 -> 0.004), opona-32 POGARSZA sie (2.233 -> 3.233).
      Cena byla znana (F-34: 0.403 ms/krok), ale nie ma czego za
      nia kupic. Monotoniczna sfera jest kontrola miary.
U-31  Czym nastroic kierownice pod styk o PRAWDZIWEJ szerokosci
      (F-33). Kandydaci: wyprzedzenie, promien zataczania,
      tlumienie ukladu, tarcie przekladni. To zmienia prowadzenie
      auta, wiec kolejnosc jest odwrotna niz zwykle: najpierw
      decyzja wlasciciela, potem pomiar.                         -> Q4/V3
U-32  SPLACONE 2026-08-03. CYLINDER i PHASED_UNION biora teraz
      referencje masy z jednorazowej sfery, tym samym wzorcem co
      TORUS (masa BRANA, nie wyprowadzana). Splacone po to, zeby
      wiersze walca i uniona w sondzie jazdy roznily sie
      KSZTALTEM, a nie masa - inaczej F-36 bylby porownaniem
      dwoch roznych aut.
      samples/jozz_vehicle_m6_suspension_rig.cpp
U-33  Czy w slowniku ksztaltow box3d istnieje bryla OBROTOWO
      SYMETRYCZNA wzgledem osi koła i szersza niz punkt. Sfera
      jest jedyna znana - i to ona jako jedyna toczy sie gladko
      (F-36). Kapsula o osi wzdluz osi koła wygladalaby idealnie
      (przekroj w plaszczyznie toczenia to okrag), ale jej czasze
      maja promien rowny promieniowi koła, wiec najwezsze takie
      kolo ma szerokosc 2R - dla R 0.51 m to 1.03 m zamiast
      0.44 m. Jesli odpowiedz brzmi NIE, to jest granica silnika,
      a nie kwestia strojenia, i przesadza o wyborze kierunku.  -> R5/R-inf
U-34  Czy szarpanie z F-36 zachowuje sie tak samo na SIATCE
      terenu. Caly przebieg jest na plaskiej plycie - celowo,
      zeby oddzielic kolo od gruntu - ale produkt jezdzi po
      meshu, gdzie dochodza krawedzie trojkatow. Nie zmieni to
      werdyktu o sferze (0.061 to podloga), moze zmienic
      dystanse miedzy kandydatami.                              -> R1
U-35  Ktory kierunek po F-36. Trzy rozlaczne: (a) sfera zostaje
      koliderem, a szerokosc/odcisk/flaczenie robi MODEL SIL,
      (b) opona jako cialo podatne, (c) latka do rdzenia box3d
      z prawdziwym koliderem obrotowym (D-CORE-01). To decyzja
      wlasciciela; moim zadaniem jest wycena kazdej z trzech,
      nie wybor.                                                 -> par. 8
U-14  Ile manifoldow daje kolo na MESHU (nie na pudle) i czy kazdy
      wnosi wlasna kotwice tarcia (P-11).                         -> R1
U-15  Czy prawo opony na dzisiejszej sferze obsluguje Z-07..Z-09.  -> R4
U-16  Czy profil wielokatny obrotowy potrafi korone rho 0.5-1.5 m
      przy barku 10-25 mm.                                        -> R3
U-20  Sweep po LICZBIE PODKROKOW przy stalym dt. F-16 mowi, ze
      podkroki zmieniaja twardosc kontaktu, wiec sweep mierzy
      DWIE rzeczy naraz i nie wolno go czytac jako "dokladnosc".
      Wymaga kontroli: powtorzyc przy contactHertz zamrozonym
      ponizej 0.125*inv_h dla NAJMNIEJSZEJ liczby podkrokow.      -> R0.5
U-21  Sweep po PREDKOSCI przy stalym dt, z jawnym przejsciem
      przez v_kryt (P-17) i przez faset/krok = 1 (F-15). Dwie
      granice, ktore moga sie mylic - trzeba je rozdzielic
      eksperymentem, w ktorym poruszamy tylko jedna.              -> R0.5
U-22  Sweep po LICZBIE SCIANEK N przy zamrozonej masie. Jedyny
      sposob odroznienia "wiecej scianek = gladziej" (F-03 mowi,
      ze falszywe) od efektu granicy fasetowania.                 -> R0.5
U-23  Czy kandydaci o ROZNEJ masie nieresorowanej daja sie w ogole
      uczciwie porownac. KOLA_05 1.3: ta sama liczba hertz to inna
      sprezyna przy innej masie. Otwierane dopiero po Q3-1, bo
      dopiero wtedy wiadomo, czy pytanie ma sens.                  -> Q3
U-24  ZAMKNIETE 2026-07-31 -> F-18. Mapa jest DOKLADNA, nie
      przyblizona: k = m_red*(2pi*f)^2 co do 5 cyfr. Zadane
      13500 N/m -> hertz 3.17041 -> osiagniete 13500.0 N/m.
U-25  ZAMKNIETE 2026-07-31 -> F-19. Wynik NEGATYWNY: sztywnosc
      statyczna nie zalezy od podkrokow (1/2/4/8 identyczne do
      5 cyfr), f_n zmienia sie o 1.9%. Zawieszenie NIE jest
      miejscem wrazliwym na podkroki - wrazliwy jest kontakt.
U-26  ZAMKNIETE 2026-07-31. Ramki ustawione poprawnie: kontrola
      b3Body_ApplyTorque daje alfa/(tau/I_spin) = 1.013 przy
      czystosci osi 1.0000. Przy okazji wyszlo F-20: silnik
      wiezu daje 2.018x zadanego momentu.
U-27  Czy suspensionDampingRatio jest przeskalowany, czy zmierzone
      0.204 przy zadanych 0.35 to wlasnosc ukladu 2-DOF (masa
      resorowana + nieresorowana na sztywnosci kontaktu) albo mojego
      estymatora dekrementu. Sztywnosc statyczna jest wolna od tego
      zastrzezenia, bo mierzy sie ja w rownowadze.                 -> Q3
U-28  DLUG NARZEDZIOWY: sonda Q3-1 jest zarejestrowana jako
      raw-only, wiec F-18/F-19/F-20 nie moga wskazac dowodu
      maszynowo (odwolanie run#tabela wymaga schematu tabel
      w evidence.py). Liczby w kontrakcie Q3 par.10 sa dzis
      PRZEPISANE, a regula 9 chce generowanych.                    -> narzedzia
```

**Uwaga do `U-20`…`U-22` (2026-07-31).** Te trzy sweepy zostały już raz
wykonane diagnostycznie, w scratchpadowej uprzęży, której **w repo nie ma**.
Liczby nie są cytowane nigdzie i nie stają się dowodem — nie da się ich
zregenerować z tego drzewa, więc reguła 9 (`KOLA_00`) je wyklucza. Zachowana
jest natomiast obserwacja jakościowa warta sprawdzenia: **podnoszenie
`contactHertz` powyżej 30 Hz nie zmieniało wyniku ani o cyfrę, obniżanie
zmieniało** — co jest dokładnie tym, co przewiduje `F-16`.

Od 2026-07-31 każdy z tych sweepów to pętla po plikach `.rig` i wywołaniach
`wheel_bench --rig-trace ... --rig-config ...`, więc powtórzenie ich **pod
bramkami** jest tanie. To jest właściwa droga; stare liczby zostają porzucone.

## 6. Rejestr rodzin: odrzucone i zawieszone (produkt, nie porażka)

Kolumna „poziom" mówi, jak mocny jest wpis: `ZAMKNIĘTE` = decyzja właściciela
albo fakt o silniku; `ZAWIESZONE` = wynik pomiarowy o znanym, wąskim zakresie,
który wraca do gry po transferze V2.

| Rodzina | Poziom | Odrzucona przez | Zakres | Co ją przywróci |
|---|---|---|---|---|
| split envelope (sfera+cylinder na maskach) | ZAMKNIĘTE | `OWNER DECISION` + N-01 | produktowo, architektonicznie | nic — to niezmiennik |
| przełączanie **natury** collidera wg kategorii powierzchni | ZAMKNIĘTE | `OWNER DECISION` O-3 | permanentnie | nic (ale tagi do materiałów/gameplayu nie są tym samym) |
| hull fasetowy jako **powierzchnia toczna** | **ZAWIESZONE** | P-04 (model) + stend v2 (ostrzeżenie) | hulle `b3CreateHull`, N ≤ 42, jedno wolne koło pod stałym dociskiem, grunt pudełkowy | wynik z rigu Q2: fasetowany obwód o mocy strat porównywalnej z bryłą gładką; albo inna procedura kontaktu (convex margin, wygładzone normalne, własny manifold) |
| globalny `contactHertz` jako **zamiennik geometrii** | **ZAWIESZONE** | P-05 | płaski grunt pudełkowy, hulle 32/42, hertz ≥ 6 Hz, zmienna prędkość | mechanizm podatności działający na wymuszenie kinematyczne, nie na penetrację |
| `b3_compoundShape` na terenie ze skanu | ZAMKNIĘTE | P-12 | teren mesh, bez patcha core | rejestracja pary compound×mesh |
| `B3X-CONTACTOBS-001` (patch telemetrii) | ZAMKNIĘTE | P-13 | — | nazwanie brakującego pola publicznego API |

**Nie wolno jeszcze zakładać** (lista przyjęta z audytu): że hull jako każda
możliwa powierzchnia toczna jest martwy; że 2300 ścianek to uniwersalne
wymaganie solvera; że churn jest jedyną przyczyną; że globalny `contactHertz`
obala podatność opony; że 6–10 punktów profilu wystarczy; że wielokąt obrotowy
będzie tani; że koncept jest globalnie nowatorski; że bryła sztywna da
footprint; że prawo opony na sferze rozstrzyga offroad; że jakakolwiek rodzina
albo hybryda już wygrała.

## 7. Rozliczenie z materiałami zewnętrznymi

Pakiet metodyczny („Proving Ground v1.2") i **cztery** dokumenty „Second Brain"
(ostatni: pełny audyt stendu v2 z 2026-07-27) są **źródłem metody i krytyki, nie
planem**. Audyt v2 został w całości zweryfikowany wobec kodu i przyjęty; jego
rozliczenie punkt po punkcie jest w odpowiedzi z 2026-07-28 oraz w `KOLA_01`
§7.9. Przyjęte z nich w całości:

- bramka `GATE-A` przed zmianą statusu (§0.1);
- rozdzielenie decyzji właściciela od propozycji programu badawczego;
- rigi Q0–Q4 jako drabina transferu (`KOLA_05`);
- normalizacja metryk na metr i na obrót;

- rozdzielenie support mappingu od manifoldu jako osobnych problemów;
- „liczba `case` nie jest kosztem, funkcja wsparcia nie jest jeszcze bryłą";
- niezmiennik N-01 (jedna geometria wobec każdej powierzchni i orientacji);
- pętla kontrolna dla każdego atrakcyjnego pomysłu (wcielona w §1.2);
- wszystkie zarzuty metodologiczne wobec stendu v1 (wcielone w stend v2).

Odrzucone lub skorygowane po konfrontacji z kodem i pomiarem — patrz odpowiedź
z 2026-07-27, sekcja walidacji.

## 8. Decyzje właściciela

```
ROZSTRZYGNIETE 2026-07-27
  O-1  wolno badac kilka pomyslow rownolegle; nie trzeba wybierac jednego
  O-2  poligon przed JES; porazki dokumentujemy na rowni z sukcesami
  O-3  tagowanie rol skanu odrzucone permanentnie
  O-4  naprawa przyrzadu NIE jest decyzja wlasciciela - to obowiazek agenta
  O-5  etap badawczy bez wyboru zwyciezcy; hybryda i jeden backend otwarte

ROZSTRZYGNIETE 2026-08-03
  O-6  KIERUNEK: wlasny kolider kola W RDZENIU box3d (droga "C").
       Wlasciciel: "opcja C jest opcja o ktorej zdecydowalem bardzo dawno
       ze sie jej trzymamy, pozwolilem edytowac box3d, zastanawialem sie
       czemu ignorujesz to". Zgoda byla udzielona 2026-07-24 i zapisana
       doslownie w naglowku KOLA_03. BLAD BYL MOJ: zamienilem udzielona
       zgode na wymyslone przez siebie pytanie D-CORE-01, wpisalem do
       KOLA_00 zdanie "nic nie rusza src/ przed D-CORE-01" i przez
       dziesiec dni egzekwowalem wlasna bramke, badajac rodziny obwiedni
       skladane ze stockowych ksztaltow. To ten sam wzorzec, co
       feedback-listen-literally-when-repeated.
  D-CORE-01  ROZSTRZYGNIETE. Domyka sie wlasnym warunkiem wyjatku z
       KOLA_03 par. 8 (C1: zdolnosc dowiedzenie nieosiagalna inaczej).
       Dowod: F-31 + F-36.

OTWARTE
  D-CORE-02  Zero-Delta-Off jako warunek konieczny
  D-CORE-03  aktualizacja do upstream/main teraz  <- starzeje sie
```
