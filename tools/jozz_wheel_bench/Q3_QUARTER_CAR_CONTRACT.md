# Kontrakt rigu Q3 — QUARTER CAR

**Napisany PRZED pierwszym przebiegiem.** To jest jedyny powód, dla którego
liczby z Q2A cokolwiek znaczą (`Q2A_CONSTANT_SPEED_BOX_CONTRACT.md`), i ten sam
powód obowiązuje tutaj. Każda liczba w tym pliku, zmieniona po pierwszym
przebiegu, zmienia **eksperyment**, a nie widok — i łamie blokadę zachowania.

Właściciel specyfikacji szczebla: `KOLA_05` §1.2. Plan etapu: `KOLA_04` §4.

---

## 1. Na jakie pytanie ten rig odpowiada

**Jak koło zamontowane do piasty i zawieszenia, pod masą resorowaną i pod
momentem napędowym, zachowuje się na zadanym profilu drogi.**

Czego ten rig **nie** rozstrzyga: feelu (to `V3`, wyłącznie Jozz), transferu do
pełnego pojazdu (to `Q4`), ani prawa opony (to `W3` — tutaj kontakt jest nadal
stockowym Coulombem).

## 2. Odstępstwo od drabiny, jawnie

`KOLA_05` w pierwotnej wersji mówił o Q3 **„brak sterowania, brak napędu"**.
Decyzja właściciela z 2026-07-31: **rig jest napędzany momentem**, bo pytanie
produktowe o oponę brzmi także „co robi przyczepność pod momentem", a nie tylko
„jak udary transferują". Wersja bierna (moment zerowy) **zostaje jako kontrola**
i jest osobnym przebiegiem, nie osobnym rigiem.

## 3. Protokół domyślny: STAŁA PRĘDKOŚĆ, mierzony moment

To jest najważniejsza decyzja tego kontraktu i jest odwrotna do odruchu.

Odruch mówi: „napędzany momentem" = zadaj moment, patrz co się dzieje. Ale
`KOLA_05` §1.1 opisuje dokładnie ten błąd w stendzie v2: **warianty zwalniają
w różnym tempie, więc porównujemy koła w różnych stanach ruchu**, a wszystkie
metryki jakości toczenia są przez to zanieczyszczone. Rig ze stałym momentem
odtworzyłby ten confound co do joty — każdy kandydat jechałby z inną prędkością.

Dlatego **domyślnie regulator utrzymuje prędkość 13,0 m/s** (ta sama co Q2A),
a **wielkością mierzoną jest moment potrzebny do jej utrzymania**. Konsekwencja
jest mocna: Q3 różni się od Q2A **dokładnie jedną rzeczą** — obecnością
zawieszenia i masy resorowanej. Dopiero to czyni porównanie `Q2→Q3` regułą
transferu (`KOLA_05` §1), a nie zestawieniem dwóch niezależnych eksperymentów.

Tryb stałego momentu istnieje (`drive_mode torque`), ale **nie jest domyślny**
i jego wyniki nie są porównywalne między kandydatami bez bramki prędkości.

## 4. Konstrukcja

```
NADWOZIE (masa resorowana)  150 kg
  b3MotionLocks: linearZ, angularX, angularY, angularZ  ZABLOKOWANE
                 linearX (jazda) i linearY (skok) SWOBODNE
  Powod blokad NIE jest kosmetyczny:
    - angularZ swobodne => nadwozie wspolredukuje spinMass wiezu
      (src/wheel_joint.c:473-475) i ZJADA czesc momentu napedowego;
    - linearZ/angularX/angularY swobodne => rig przestaje byc quarter-carem,
      bo dopuszcza ruch, ktorego zaden narożnik pojazdu nie ma w izolacji.

KOLO (masa nieresorowana)   44 kg, R = 0.5141 m, W = 0.4375 m
  I_spin  0.70 mR^2   I_trans 0.55 mR^2   - ZAMROZONE, jak w Q2A
  obwiednie: sphere | prism-N | torus-N   (patrz aneks ponizej)

WIEZ  b3CreateWheelJoint(nadwozie, kolo)
  os zawieszenia: frameA.x (src/wheel_joint.c:459) ustawiona PIONOWO
  os obrotu:      frameB.z (src/wheel_joint.c:473) ustawiona na os kola
  sterowanie skretem: WYLACZONE (enableSteering = false)
```

**Masa nieresorowana jest ZAMROŻONA między kandydatami** (reguła twarda 1
`KOLA_00`). Kandydaci o różnej masie to `U-23` i są otwierani dopiero po `Q3-1` —
z powodu opisanego w §5.

### 4.2 Filtr nadwozia (poprawka 2026-08-02, zachowanie niezmienione)

Nadwozie ma z niczym nie kolidować, ale ma być **widoczne** i ma **dawać się
chwycić**. To trzy różne testy i pierwotne rozwiązanie — `maskBits = 0` —
zaspokajało dwa z nich, milcząc o trzecim:

```
rysowanie      b3World_Draw filtruje po categoryBits
kolizja        (A.kat & B.maska) I (B.kat & A.maska)      src/shape.h:145
promien myszy  ten SAM predykat, z domyslnym filtrem zapytania
```

Zerowa maska przewraca **drugą połowę predykatu zapytania**, więc
`b3World_CastRayClosest` nigdy nie trafiał w masę resorowaną — `Ctrl+LPM` nie
miał czego złapać poza kołem. Teraz nadwozie ma własną kategorię (bit 63),
a grunt i progi mają ją wyciętą z masek; z kołem chroni je `collideConnected =
false` więzu. Kolizji nadal nie ma i **przebieg jest bajtowo ten sam** —
`--qc-compare` daje 30 wierszy identycznych z `evidence/run_q3_compare_2026_07_31.txt`
we wszystkich kolumnach poza kosztem CPU.

### 4.1 Aneks: trzecia obwiednia `torus-N` (2026-07-31, decyzja właściciela)

Ten punkt brzmiał pierwotnie „obwiednie: sphere | prism-N — **żadnych nowych w
tym etapie**". Właściciel uchylił to ograniczenie wprost: etap ma skończyć się
działającym **nowym systemem koła**, a nie samym przyrządem.

```
torus-N   pierscien N kapsul o osi rownoleglej do osi kola,
          rozstawionych po okregu o promieniu (wheelR - crownR)
          crownR   promien przekroju korony = promien kapsuly
          biežnia  plaska, szerokosci wheelW - 2*crownR
          barki    zaokraglone promieniem crownR
```

Powód jest wyprowadzony ze **zmierzonych** faktów, nie z estetyki:

1. `F-01`: `b3CreateHull` nie przyjmuje więcej niż ~42 ścianki. „Zagęszczaj
   dalej" **nie jest** drogą wyjścia dla pryzmatu; kapsuł można dać 64 albo 96.
2. Pryzmat ma **ostre krawędzie** — normalna kontaktu skacze na wierzchołku.
   Pierścień kapsuł nie ma w kierunku toczenia ani jednej krawędzi.
3. Masa i bezwładność zostają **zamrożone**, więc obwiednia jest jedyną zmienną —
   dokładnie tak jak w porównaniach Q2A.

Konstrukcja jest odrzucana, gdy pierścień jest **nieszczelny**
(`crownR < ringR·sin(π/N)`) — wtedy obwiednia ma dziury i koło wpada między
kapsuły. Rig woli odmówić budowy niż zbudować coś, co wygląda jak koło, a toczy
się jak grzechotka (`JozzRig_MinTorusSegments`).

Wyniki: `F-21` (torus wygrywa), `F-22` (wygrywa **mimo większego** tętnienia
promienia), `F-23` (cena: 13× CPU).

### 4.3 Profil poprzeczny korony (2026-08-03, rozszerzenie `torus-N`)

Pierwsza wersja `torus-N` miała bieżnię **zawsze płaską**, a `crownR` służył
naraz za promień barku i za jedyne pokrętło kształtu przekroju — cały zakres
opon, od twardej drogowej po miękką terenową, miał do dyspozycji jedną liczbę,
i to taką, która zwężała bieżnię przy każdej próbie zaokrąglenia barku.

Dwa nowe pola konstrukcji (`crown_rows`, `crown_drop` w `.qc` i `.rig`):

```
crownRows  ile rzedow kapsul lezy obok siebie w poprzek biezni  (1..9)
crownDrop  o ile promien BARKU jest mniejszy od promienia srodka biezni

  a       = W/2 - crownR            polowa plaskiej biezni
  sag(y)  = Rc - sqrt(Rc^2 - y^2),  Rc = (a^2 + drop^2) / (2*drop)

  granice pasm sa ROWNE PO ZWISIE, nie po szerokosci:
      krawedz(t) = sqrt(2*Rc*s - s^2),  s = drop*|t|,  t = -1 + 2j/M
  os rzedu j lezy na promieniu (wheelR - crownR) - sag(srodek pasma) + lift
  lift    = najmniejszy sag wsrod rzedow  (patrz punkt 4 nizej)
```

Podział równy po **zwisie**, a nie po szerokości, jest poprawką z 2026-08-03 i
jest ZMIERZONY: błąd schodka bierze się z tego, o ile promień zmienia się wewnątrz
jednego pasma, a łuk korony jest przy barku kilkanaście razy bardziej stromy niż
na środku bieżni. Równe pasma po szerokości marnowały rozdzielczość tam, gdzie
profil jest płaski. Przy 9 rzędach i zwisie 80 mm odchyłka spadła **18,1 → 9,0 mm
przy identycznym koszcie CPU**, a przebieg fizyczny przestał skakać: strata idzie
teraz monotonicznie 973,8 → 916,9 → 879,3 → 869,9 W zamiast 788,9 → 895,6 → 845,5
→ 878,5 W.

Trzy rzeczy, które trzymają ten dodatek w ryzach:

1. **`crownRows == 1` daje DOKŁADNIE bryłę sprzed zmiany**, bit w bit: przy
   jednym paśmie środek pasma wypada na `y = 0`, zwis wynosi zero, a kolejność
   tworzenia kształtów jest ta sama. Sprawdzone: `--qc-compare` daje 30 wierszy
   identycznych z `evidence/run_q3_compare_2026_07_31.txt` we wszystkich
   kolumnach poza kosztem CPU, a blokada zachowania Q2A jest zielona.
2. **Odcisk konfiguracji milczy o koronie, dopóki jej nie ma** — `rows=`/`drop=`
   dopisują się dopiero przy `crownRows > 1`. Bez tego wzorzec zachowania
   zapalałby się na czerwono dla `sphere` i `prism`, gdzie nic się nie zmieniło.
3. **Cena jest wprost proporcjonalna i jawna**: kształtów = `N * rzedy`. To jest
   wynik, a nie usterka — łuku nie da się złożyć z kapsuł o osi równoległej do
   osi koła, bo każda daje w przekroju odcinek prosty.

4. **Szczyt jest normalizowany do zamówionego promienia.** Przy PARZYSTEJ liczbie
   rzędów żadne pasmo nie leży na środku bieżni, więc najwyższy pasek siedzi już
   na zwisie `sag(a/M)` i koło wychodziło **cicho mniejsze** niż `wheelR` — przy
   dwóch rzędach i zwisie 20 mm o 4,9 mm, czyli 1% promienia. Przemiatanie po
   liczbie rzędów mieszało wtedy wierność profilu z rozmiarem opony. Przy `M`
   nieparzystym i przy `M = 1` przesunięcie wychodzi dokładnie 0.0.
5. **Zakres jest twardy, nie przycinany.** `crown_rows` poza `1..9` albo zwis
   większy niż połowa bieżni to konstrukcja **nieprzedstawialna**, odrzucana
   z konkretnym komunikatem. Wcześniej była po cichu przycinana, a odcisk
   konfiguracji drukował dalej wartość ZAMÓWIONĄ — plik `.qc` z `crown_rows 20`
   budował dziewięć rzędów i podpisywał przebieg jako dwadzieścia.

Nowa liczba w oknie: **odchyłka od profilu** — największa różnica między
obwiednią zbudowaną a zamówionym łukiem. Jest to miara jakości REPREZENTACJI,
odpowiadająca na pytanie „na ile ta bryła jest oponą", którego nie rozstrzyga
żadna z dotychczasowych metryk. Rośnie ze zwisem, maleje z liczbą rzędów.
Próbki leżą w ŚRODKACH przedziałów: na samej krawędzi barku obwiednia jest
nieciągła i próbka postawiona dokładnie tam porównywała „krawędź" z „pustką",
dając odchyłkę 464 mm na kole o promieniu 514 mm.

Stend: `--qc-rows M`, `--qc-drop m`, `--qc-sweep-param rows|drop`.

### 4.3.1 Odcisk obwiedni — dlaczego przemiatanie musi mówić, czy zmieniło kształt

`JozzRig_EnvelopeSignature` liczy odcisk **powierzchni** obwiedni (nie listy
kapsuł, nie konfiguracji). Dwie konstrukcje o równym odcisku są dla drogi
nieodróżnialne, choć mogą mieć różną liczbę kształtów i różny koszt.

Powód istnienia jest zmierzony i kosztował jeden błędny wniosek (`F-26`,
wycofane): przemiatanie po liczbie rzędów wykonane przy **zwisie 0** przemiatało
tę samą bryłę — wszystkie rzędy leżą wtedy na jednym promieniu, więc ich unia to
dokładnie ta sama kapsuła co przy jednym rzędzie. Tabela pokazywała pięć coraz
droższych punktów o różnych liczbach i wyglądała jak pomiar kształtu.

Stend drukuje odcisk przy każdym punkcie, oznacza `= TA SAMA BRYLA` i po tabeli
pisze, ile RÓŻNYCH brył naprawdę przez nią przeszło. Ta sama kolumna stoi
w tabeli okna.

**`F-27`, konsekwencja dla całego programu:** przy identycznej obwiedni sama
liczba kształtów przesuwa stratę o **5,3%** (775,0 → 816,1 W, monotonicznie
z liczbą nośnych punktów kontaktu 2,84 → 18,55). To jest podłoga szumu każdego
porównania między konstrukcjami o istotnie różnej liczbie punktów kontaktu —
a takie właśnie będzie porównanie opony sztywnej z odkształcalną.

### 4.4 Skok dynamiczny (2026-08-03, nowa kolumna obok starej)

`travel_rms` jest liczony od zera więzu, więc zawiera w sobie ugięcie statyczne —
przy 13 500 N/m i 150 kg to 111 mm, na tle których praca zawieszenia rzędu kilku
milimetrów po prostu ginie. Kolumna „skok rms" pokazywała przez to niemal tę samą
liczbę dla KAŻDEGO kandydata, czyli nie mierzyła niczego, co się porównuje.

Zostaje niezmieniona, bo opisuje ją cała dotychczasowa tabela. Obok stoi
`travel_dyn_rms` — odchylenie od średniej okna, czyli ile zawieszenie naprawdę
pracuje.

## 5. Zawieszenie podawane FIZYCZNIE, nie w hercach

```
sztywnosc przy kole   13 500 N/m     (docelowa czestotliwosc resoru ~1.51 Hz
                                      przy 150 kg, ugiecie statyczne ~111 mm)
tlumienie             zeta = 0.35
skok                  bump  0.09 m OD PUNKTU STATYCZNEGO
                      droop 0.11 m OD PUNKTU STATYCZNEGO
```

**Skok podawany od statyki, nie od zera** — poprawka wymuszona przez samokontrolę
rigu przy pierwszej budowie. Pierwsza wersja miała „skok 0,10 m" liczony od zera,
a samo ugięcie statyczne wynosi 0,1111 m: rig stałby na zderzaku **już na
postoju**. Konstrukcja odmówiła budowy i wypisała obie liczby. To jest ten sam
mechanizm, który w `Q3-1` złapał zerowaną masę nadwozia.

Rig startuje **już ugięty** o ugięcie statyczne. Bez tego każdy przebieg zaczyna
się od 2–3 s dzwonienia zawieszenia, które trzeba albo przeczekać, albo — gorzej —
wpuścić do okna pomiarowego.

**`suspensionHertz` NIE jest polem tego kontraktu — jest wielkością wyliczaną.**
Powód, z implementacji (`KOLA_05` §1.3): sztywność podąża za masą efektywną
więzu, czyli masą **zredukowaną obu ciał** (150 + 44 kg → 34,02 kg), a nie za
masą resorowaną. Ta sama liczba herców na dwóch kandydatach o różnej masie
nieresorowanej to **dwie różne sprężyny**.

Mapa `N/m ↔ hertz` jest **mierzona sondą `Q3-1`, nie wyprowadzana**. Wzór
`k ≈ m_red·ω²` jest przybliżeniem granicy sztywnej, a schemat `bias`/`massScale`
(`src/solver.h:264`) nie jest liniową sprężyną. Przebieg zapisuje **oba**:
wartość zadaną w N/m i wartość osiągniętą — razem z resztą kalibracji.

`dt` i liczba podkroków są częścią **przyrządu**, nie badanego obiektu, i muszą
być identyczne dla wszystkich porównywanych kandydatów: `b3MakeSoft` bierze `h`
podkroku, więc **liczba podkroków zmienia także zawieszenie** (`U-25`).

## 6. Droga

```
flat    plaska plyta                      KONTROLA
cleat   pojedynczy prog, wysokosc h       odpowiedz na pojedynczy udar
comb    grzebien progow, rozstaw s        wymuszenie okresowe

obstacle_half_z   polowa szerokosci progu; 0 = CALA szerokosc plyty
obstacle_z        srodek progu w poprzek
```

Bez wymuszenia Q3 na płaskiej płycie mierzy **fasety wielokąta**, a nie transfer
udaru — czyli odpowiada na pytanie Q1/Q2, nie na swoje. Płaski przebieg zostaje
jako kontrola i jako punkt odniesienia dla `Q2→Q3`.

### 6.1 Przekrój drogi (2026-08-03, `F-28`)

Do tej wersji każdy próg był pudłem o połowie szerokości równej połowie płyty,
czyli droga Q3 była **jednorodna w poprzek**. To, a nie fizyka, było powodem,
dla którego profil poprzeczny opony nie dawał się na tym stanowisku zmierzyć:
sztywna obwiednia na przeszkodzie rozciągniętej przez całą szerokość dotyka
wyłącznie wierzchołkiem korony.

`obstacle_half_z <= 0` znaczy „cała szerokość", czyli **dokładnie dotychczasowa
droga** — cały zarejestrowany materiał Q3 zostaje ważny, a odcisk konfiguracji
milczy o tych polach, dopóki próg jest pełnej szerokości.

Zmierzone (`F-28`): ta sama bryła (`torus-64`, 5 rzędów, zwis 0,08 m), grzebień
kamieni 30 mm o połowie szerokości 40 mm przesuwanych w bok z 0 na 0,20 m —
`a_rms` **1,548 → 0,079 m/s²**. Zmienia się wyłącznie położenie kamienia pod
bieżnią.

Stend: `--qc-obstacle-h`, `--qc-obstacle-half-z`, `--qc-obstacle-z`,
`--qc-sweep-param stone_w|stone_z`.

Każdy przebieg w wersji `PUDŁO` i `MESH` (`KOLA_05` §1.2, linia „grunt").

## 7. Co jest mierzone

| wielkość | jednostka | dlaczego jest w kontrakcie |
|---|---|---|
| `drive_torque_mean` | N·m | odpowiednik `tow_force_mean` z Q2; podstawa porównania |
| `loss_power` | W | `drive_torque_mean × omega`; addytywna, przenoszalna |
| `sprung_accel_rms` | m/s² | **pytanie Q3**: ile udaru dochodzi do pojazdu |
| `airborne_fraction` | — | udział kroków bez impulsu normalnego (patrz §8) |
| `travel_rms`, `travel_min/max` | m | czy zawieszenie pracuje, czy stoi na zderzakach |
| `limit_hits` | — | ile kroków na ograniczniku skoku |
| `contact_churn` | — | ta sama definicja co Q2A, dla porównywalności |
| `slip_ratio_mean` | — | `(omega·R − v)/v`; jedyna metryka nowa wobec Q2 |
| `loaded_points` | — | punkty, które solver uznał za nośne (`totalNormalImpulse > 0`) |
| `loaded_manifolds` | — | ile **par kształtów** niesie; tyle jest kotwic tarcia (`P-11`, `P-18`) |
| `points_eff` | — | `(Σp)²/Σp²` — ile punktów **naprawdę** niesie (§7.1) |
| `share_max`, `share_max_peak` | — | udział największego punktu: średni po oknie i najgorszy krok |

### 7.1 Rozkład nacisku, nie tylko jego suma (2026-08-03, `F-31`)

Do 2026-08-03 telemetria kontaktu czytała `totalNormalImpulse` **osobno dla
każdego punktu** i zostawiała z tego licznik i sumę. Rozkład szedł na podłogę
przy każdym kroku, a `loaded_points` sam w sobie **wprowadza w błąd**: kontakt
spekulatywny o impulsie rzędu 1e-6 N·s liczy się do niego tak samo jak punkt
niosący ćwierć pojazdu.

`points_eff` to *participation ratio*: równe **1**, gdy jeden punkt niesie
wszystko, i **k**, gdy k punktów niesie po równo. Kontrola miary: `sphere` na
płycie ma z geometrii dokładnie jeden punkt i daje `points_eff` 1,00 przy
`share_max` 100,0% — jeżeli kiedykolwiek przestanie, miara jest zepsuta.

**`share_max` i `points_eff` liczy się WYŁĄCZNIE po krokach w kontakcie.**
W powietrzu te wielkości nie istnieją; uśrednione z zerami dawałyby koło
skaczące jako koło o ładnie rozłożonym nacisku, czyli dokładnie na odwrót.
`loaded_points` i `loaded_manifolds` idą po wszystkich krokach, tak jak dotąd —
te dwie rodziny mają różne dzielniki i nie wolno ich mieszać.

`share_max_peak` stoi obok średniej, bo degeneracja bywa **chwilowa**: koło może
przez większość okna stać na kilku punktach i degenerować się dokładnie tam,
gdzie to boli — na krawędzi progu.

## 8. Co UNIEWAŻNIA przebieg

0. **Rozrzut między powtórzeniami** — patrz §8.1. Komórka porównania, której
   którekolwiek powtórzenie jest nieważne, jest nieważna w całości.
1. **`airborne_fraction` > 0,10** — koło w powietrzu przez ponad 10% kroków.
   Wtedy porównanie mierzy zawieszenie i geometrię profilu drogi, a nie oponę.
   `P-17` mówi wprost, że kontrakt Q2A jedzie **2,76×** powyżej prędkości
   ciągłego styku, więc to nie jest ryzyko teoretyczne — to stan domyślny.
2. **Regulator w saturacji > 5% kroków w oknie pomiarowym** — mierzymy limit
   regulatora, nie koło. Ta sama zasada co w Q2A.
3. **Sesja oznaczona `EXPLORATION`** — jakakolwiek ingerencja w fizykę
   (chwyt myszy, wstrzelone ciało, hot-swap kandydata w oknie).
4. **Różne `dt` lub różna liczba podkroków** między porównywanymi przebiegami.

Unieważniony przebieg **nie jest kasowany** (reguła twarda 6) — jest oznaczany.

### 8.1 Powtórzenia: dlaczego jeden przebieg to za mało (`F-24`)

Podniesienie punktu mocowania nadwozia o 0,35 m — wielkość **geometrycznie
neutralna** dla tego więzu (oś pionowa, oba zaczepy na osi, zerowe ramię) —
przerzuciło `airborne_fraction` dla `torus-32` przy 13 m/s z 6,0% na 10,8%, czyli
**przez próg ważności**. To nie jest błąd: koło skaczące po fasetach jest układem
chaotycznym i inne zaokrąglenie zmiennoprzecinkowe rozjeżdża trajektorie.

Dlatego `--qc-compare` **powtarza każdą komórkę trzykrotnie** z przesuniętym
punktem startu (0 / 13,7 / 27,1 mm) i podaje **połowę rozrzutu** obok średniej.
Przesuwamy punkt startu, bo to jedyna zmiana, która na płaskiej płycie nie dotyka
ani koła, ani zawieszenia, ani drogi.

Przy 4 m/s rozrzut jest zerowy do trzech miejsc. Problem dotyczy **wyłącznie
reżimu, w którym koło traci ciągły styk** — czyli tego, w którym Q3 i tak nie ma
prawa rozstrzygać o obwiedni (§6).

## 9. Czego ten rig świadomie NIE modeluje

Bocznego prowadzenia (brak skrętu i brak siły bocznej), przenoszenia obciążenia
między narożnikami, podatności opony (kontakt jest sztywny — to `R8`), ani
prawa opony (`W3`). Wynik z Q3 **nie awansuje na prawo** bez `Q4`
(`KOLA_05` §1, reguła transferu).

---

## 10. Kalibracja — ZMIERZONA przez `Q3-1` (2026-07-31)

Regeneracja: `wheel_bench.exe --qc-probe evidence/qc_probe_2026_07_31.txt`

```
hertz dla 13 500 N/m przy 150+44 kg, 4 podkroki   3.17041 Hz
osiagnieta sztywnosc                              13 500.0 N/m  (blad -0.000 %)
ugiecie statyczne                                 0.1111 m
f_n zmierzone (uklad 2-DOF, tlumione)             1.4770 Hz
wrazliwosc k na liczbe podkrokow                  BRAK: 1/2/4/8 daja k identyczne
                                                  do 5 cyfr; f_n zmienia sie 1.9%
alfa = tau / I ?                                  I_spin - potwierdzone kontrola
                                                  b3Body_ApplyTorque (iloraz 1.013)
konwencja znaku translacji wiezu                  DODATNIA = sciskanie
                                                  (nadwozie blizej kola)
```

### 10.1 Zmiana konstrukcji wymuszona pomiarem: napęd

**Silnik więzu (`maxSpinTorque`) NIE jest źródłem momentu o zadanej wartości.**
Zmierzone: przy nasyceniu dostarcza **2,018×** zadanego momentu (4 podkroki;
1,996× przy 1 podkroku, 2,022× przy 8). Zależność od `tau` jest idealnie
liniowa — 100, 500 i 1000 N·m dają ten sam iloraz — a bezwładność obrotowa
nadwozia **nie wpływa na wynik w ogóle** (identycznie do trzech miejsc przy
obrocie zablokowanym i swobodnym).

Kontrola rozstrzygnęła, gdzie leży rozbieżność: `b3Body_ApplyTorque` przyłożony
wprost do koła daje `α/(τ/I_spin) = 1,013`, więc **masa i bezwładność koła są
poprawne**, a przeskalowanie siedzi w ścieżce silnika więzu.

**Dlatego Q3 napędza koło przez `b3Body_ApplyTorque`, a silnik więzu zostaje
wyłączony.** Trzy powody:

1. „Napędzany momentem" ma znaczyć N·m, a nie 2,018 × N·m.
2. Współczynnik zależy od liczby podkroków (1,3% rozrzutu), więc oparcie na nim
   wiązałoby wynik z ustawieniem **przyrządu**, nie badanego obiektu.
3. Q3 staje się strukturalnie równoległy do Q2A: oba rigi przykładają jawne
   uogólnione wymuszenie i oba mają regulator PI. To jest warunek, żeby
   porównanie `Q2→Q3` mierzyło zawieszenie, a nie różnicę ścieżek napędu.

Mechanizm samego przeskalowania **nie jest ustalony** (podejrzenie: impuls
silnika aplikowany w więcej niż jednym przebiegu solvera). Efekt jest zmierzony,
mechanizm pozostaje hipotezą — te dwie rzeczy mają w tym programie osobne
statusy (`P-05`).

---

## 11. Wynik pierwszego porównania (2026-07-31)

Regeneracja: `wheel_bench.exe --qc-compare <plik.csv>`
Surowy przebieg: `evidence/run_q3_compare_2026_07_31.txt` (sha256 `c086d7d91831`)

**Reżim rozstrzygający, 4,0 m/s, płaska płyta** — obwiednia jest tu widzialna dla
solvera (~0,7 elementu na krok) i nic nie jest w powietrzu:

| kandydat | tętn. mm | moment N·m | strata W | a_rms m/s² | churn % | ms/krok |
|---|---|---|---|---|---|---|
| sphere (kontrola) | 0,000 | −0,00 | 0,0 | 0,001 | 0,0 | 0,007 |
| prism-42 (dziś) | 1,438 | 82,09 | 638,7 | 0,332 | 89,7 | 0,007 |
| prism-32 | 2,476 | 115,99 | 902,8 | 0,531 | 70,1 | 0,007 |
| **torus-32** | 3,896 | 67,22 | 523,1 | 0,177 | 8,6 | 0,036 |
| **torus-64** | 0,973 | **54,72** | **425,7** | **0,055** | **0,0** | 0,094 |

Rozrzut z trzech powtórzeń: 0,00–0,01 we wszystkich komórkach.

**Reżim kontraktowy, 13,0 m/s** — struktura obwiedni jest poniżej rozdzielczości
czasowej solvera (~2,2 elementu na krok). `torus-64` trzyma ciągły styk (0,0%
w powietrzu wobec 5,8% pryzmatu) i ma o 69% niższe `a_rms`, ale **wyższą** moc
strat. Uczciwe zastrzeżenie: koło 5,8% kroków w powietrzu nie stawia w tym czasie
oporu, więc niższa zmierzona strata pryzmatu częściowo mierzy krótszy czas styku.

**Grzebień przy 13 m/s: wszystkie przebiegi NIEWAŻNE** (60–66% w powietrzu, sfera
też). To wynik o drodze, nie o kole — reguła §8 zadziałała.

---

## 12. Czego to porównanie NIE rozstrzyga

`torus-N` wygrywa **w Q3, w jednym rigu, na dwóch prędkościach, bez prawa opony
i bez prowadzenia bocznego**. Reguła transferu (`KOLA_05` §1) mówi wprost, że to
nie awansuje na prawo produktu bez `Q4`. Trzy konkretne rzeczy pozostają
niezbadane:

1. **Boczne** — pierścień kapsuł ma płaską bieżnię szerokości 37,5 mm przy
   `crownR` 0,20. Przy stockowym Coulombie to nie ma znaczenia (tarcie nie zależy
   od powierzchni), ale przy prawie opony (`W3`) i przy pochyleniu koła — będzie.
2. **Koszt w pojeździe** — `F-23` mierzy jedno koło na płycie, nie cztery koła
   w broadphase razem z nadwoziem i terenem.
3. **Feel** — wyłącznie Jozz, w oknie `Quarter Car Scope` (reguła twarda 5).

---

## 12.1 Przemiatanie parametru (dodane 2026-07-31, druga sesja)

Trzeci tryb obok pojedynczego przebiegu i macierzy kandydatów:

```
wheel_bench --qc-sweep <plik.csv>
            --qc-sweep-param segments|crown|rows|drop|spring|speed|obstacle|stone_w|stone_z|substeps
            --qc-sweep-from A --qc-sweep-to B [--qc-sweep-steps N] [+ opcje --qc-* jako baza]
```

**`substeps` nie jest ustawieniem wydajności, tylko parametrem eksperymentu**
(dodane 2026-08-03) — i dlatego stoi na tej liście obok geometrii. Ostrzeżenie
`U-20` obowiązuje: przemiatanie po podkrokach rusza **dwie rzeczy naraz**, bo
`F-16` wiąże efektywny `contactHertz` z `0.125·inv_h`. Wolno z niego czytać to,
co nie zależy od twardości kontaktu (`points_eff`, `share_max`), albo **różnicę
między dwiema konstrukcjami przy tej samej liczbie podkroków** — bo wtedy
`contactHertz` jest dla obu wspólny. Tak właśnie zmierzone jest `F-32`.

Ten sam protokół co §3 i §8: rozgrzewka 120, okno 600, **3 powtórzenia na punkt**,
ten sam werdykt ważności. Różnica jest tylko taka, że zmienną jest jedno pole
konstrukcji, a nie tożsamość kandydata. To samo jest w oknie (zakładka `Pomiar`),
tą samą funkcją `JozzQc_MeasureCell` — sprawdzone: obie strony dają te same liczby.

Punkt, którego nie da się zbudować, **nie przerywa przemiatania i nie znika**:
trafia do tabeli z `valid=0` i powodem. „Tutaj konstrukcja się kończy" jest
wynikiem, a nie awarią.

Po co osobny tryb, skoro macierz już istnieje: macierz odpowiada na pytanie
*który kandydat lepszy*. Przemiatanie odpowiada na *co ten parametr właściwie
robi* — i od razu obaliło jedno założenie zapisane w kodzie (`F-25`, promień
korony).

---

## 13. Czego ta kalibracja NIE mówi

`f_n` i tłumienie zmierzone na **sygnale skoku zawieszenia w układzie 2-DOF**
(masa resorowana + nieresorowana na sztywności kontaktu), a nie na czystym
oscylatorze. Zmierzone tłumienie 0,204 przy zadanym `zeta` 0,35 **nie dowodzi**,
że `suspensionDampingRatio` jest przeskalowany — może to być własność układu
dwustopniowego albo mojego estymatora dekrementu. Sztywność statyczna jest wolna
od tego zastrzeżenia, bo mierzy się ją w równowadze. Osobne pytanie: `U-27`.
