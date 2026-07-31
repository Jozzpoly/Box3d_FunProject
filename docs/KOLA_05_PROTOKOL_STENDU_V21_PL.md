# Protokół stendu v2.1 — rigi Q0–Q4, słownik metryk, manifest dowodowy

Data: 2026-07-28
Podstawa: `KOLA_01` §7.9 (dwanaście wad stendu v2) + audyt zewnętrzny 2026-07-27
Status: **PROTOKÓŁ ZAPROJEKTOWANY, NIEURUCHOMIONY**

> **Reguła tego dokumentu.** Protokół powstaje i jest opisany **przed** pierwszym
> przebiegiem. Powód jest bolesny i zmierzony: stend v2 dał liczby, które przez
> jedną dobę uchodziły za prawa, a okazały się w połowie własnością narzędzia
> (ukryty rozbieg, mylna nazwa rigu, niespójne flagi). Kiedy protokół jest
> spisany wcześniej, wynik da się odróżnić od artefaktu. Kiedy nie jest —
> nie da się.

---

## 1. Drabina rigów Q0–Q4 — po co w ogóle pięć

Stend v2 miał **jeden** rig i nazwał go „realnym narożnikiem". To był wolne koło
pod prasą. Pięć rigów istnieje po to, żeby **transfer był mierzony, a nie
zakładany**: wynik z Q1 nie awansuje na prawo, dopóki nie przejdzie przez Q3.

```
Q0  FREE WHEEL              samo kolo, tylko wlasna masa (440 N = 44*10)
                            pytanie: co robi SAMA geometria?

Q1  CONSTANT DOWNFORCE      to co dzis, ale nazwane poprawnie
                            pytanie: jak geometria reaguje na duzy nacisk
                                     przy MALEJ bezwladnosci poziomej?

Q2  TOWED AT CONSTANT SPEED kolo ciagniete dokladnie 13 m/s   <- NAJWAZNIEJSZY
                            pytanie: ile MOCY zjada geometria?

Q3  QUARTER-CAR             masa resorowana + jawne zawieszenie
                            pytanie: jak udary transferuja do pojazdu?

Q4  FULL VEHICLE COAST-DOWN ten sam wariant w calym aucie, bez napedu
                            pytanie: czy ranking ze stendu dotyczy produktu?
```

**Reguła transferu:** *jeżeli wynik z Q1 nie powtarza się w Q3/Q4, nie awansuje
z Q1.* Rodzinę wolno zamknąć dopiero przy zgodności **Q2–Q4**.

### 1.1 Dlaczego Q2 jest najważniejszy i idzie pierwszy

Wszystkie metryki jakości toczenia ze stendu v2 są zanieczyszczone tym, że
warianty **zwalniają w różnym tempie** — porównujemy koła w różnych stanach
ruchu. Q2 usuwa ten confound u źródła: sterownik utrzymuje 13 m/s, a mierzoną
wielkością jest **siła / moc potrzebna do utrzymania tej prędkości**.

To zamienia miękkie „vy_rms 0.148 vs 0.247" na twarde „X watów strat na koło".
Moc strat jest wielkością fizyczną, porównywalną, addytywną i przenoszalną do
JES — a `vy_rms` przy zmiennej prędkości nie jest niczym takim.

Q2 rozstrzyga naraz: U-01 (churn na metr), U-17 (właściciel oporu toczenia),
U-18 (czy normalizacja odwraca ranking) i **kryterium obalenia P-04**.

### 1.2 Specyfikacja rigów

```
WSPOLNE DLA WSZYSTKICH
  R = 0.5141 m, W = 0.4375 m, masa 44 kg, I_spin 0.70 mR^2, I_tr 0.55 mR^2
  krok 1/60 s, 4 podkroki, 1 watek, sleep OFF
  masa i bezwladnosc ZAWSZE jawne (N-09) - nigdy z objetosci collidera
  grunt: obie wersje kazdego przebiegu - PUDLO i MESH
         (problem produktowy to mesh; pudlo zostaje jako kontrola)

Q0  sila zewnetrzna: brak. Nacisk = m*g = 44 * 10 = 440 N.
Q1  sila zewnetrzna: stala pionowa, dobrana do NACISKU 1900 N
      poprawka: F = 1900 - m * g_swiata, gdzie g_swiata = 10 (src/types.c:16)
      NIE 9.81. Blad v2 dawal 1908 N.
Q2  regulator predkosci: sila pozioma PD utrzymujaca vx = 13.0 m/s
      docisk jak Q1
      MIERZYMY: srednia sila ciagnaca, jej odchylenie, moc = F * v
      regulator musi byc opisany jawnie (wzmocnienia, limit, pasmo)
      i ten sam dla wszystkich wariantow - inaczej mierzymy regulator
Q3  masa resorowana 150 kg na jawnym wiezie pionowym
      ZMIENIONE 2026-07-31 (decyzja wlasciciela): rig JEST napedzany momentem.
        Wersja bez napedu zostaje jako KONTROLA, nie jako jedyny tryb.
        Powod zmiany: bez napedu Q3 odpowiada wylacznie na "jak udary
        transferuja", a pytanie produktowe o opone brzmi takze "co robi
        przyczepnosc pod momentem".
      sprezyna i tlumienie podawane FIZYCZNIE przy kole (N/m, N*s/m);
        `suspensionHertz` jest WYLICZANY, nigdy wpisywany - patrz 1.3
      profil drogi jest polem konfiguracji: plasko (kontrola) / prog o
        zadanej wysokosci / grzebien progow o zadanym rozstawie.
        Bez wymuszenia Q3 na plaskim pudle mierzy fasety wielokata,
        nie transfer udaru.
      nadwozie usztywnione JAWNIE; sposob usztywnienia jest czescia
        kontraktu, nie detalem sceny (patrz 1.3, spinMass)
Q4  jozz_vehicle_validation.exe z ROOTA repo, sonda coast-down
```

### 1.3 `suspensionHertz` nie jest częstotliwością resorowania

**Ostrzeżenie o confoundzie, nie ciekawostka.** Cytowane z implementacji
(reguła 11 `KOLA_00`: nigdy z komentarza nagłówka):

```c
// src/wheel_joint.c:463-465
float k = base->invMassA + base->invMassB
        + b3Dot( rAn, b3MulMV( base->invIA, rAn ) )
        + b3Dot( rBn, b3MulMV( base->invIB, rBn ) );
joint->suspensionMass = k > 0.0f ? 1.0f / k : 0.0f;

// src/wheel_joint.c:468  (b3MakeSoft -> src/solver.h:264)
joint->suspensionSoftness = b3MakeSoft( suspensionHertz, dampingRatio, context->h );

// src/wheel_joint.c:684
float impulse = -massScale * joint->suspensionMass * ( cdot + bias ) - ...
```

Sztywność podąża za **masą efektywną więzu**, czyli masą zredukowaną obu ciał
wzdłuż osi zawieszenia — nie za masą resorowaną:

| masa resorowana + nieresorowana | masa efektywna więzu |
|---|---|
| 150 kg + 44 kg | **34,02 kg** |
| 150 kg + 30 kg | **25,00 kg** |

**Rozliczenie z pomiarem (`Q3-1`, 2026-07-31).** Powyższe powstało z *czytania*
kodu. Sonda `wheel_bench --qc-probe` zmierzyła każdy z trzech wniosków — dwa
potwierdziła, **trzeci obaliła**:

1. ✅ **POTWIERDZONE, dokładnie.** Zmierzona sztywność zgadza się z
   `m_red·(2πf)²` do 5 cyfr znaczących w całym zakresie `f` ∈ [0,5; 6] Hz, a od
   `m_sprung·(2πf)²` odbiega o stały czynnik 4,409 (= 150/34,02). Rzeczywista
   częstotliwość resorowania = `hertz · √(m_red/m_sprung)`, czyli dla tych mas
   **`hertz`/2,10**. Ustawienie „6 Hz" daje resor 2,86 Hz.
   → `F-18`
2. ❌ **OBALONE dla sztywności statycznej.** Podkroki 1/2/4/8 dają sztywność
   **identyczną do 5 cyfr** (3021,9 N/m przy `hertz` 1,5); `f_n` zmienia się
   o 1,9%. To jest **wynik negatywny i produkt**: zawieszenie *nie* jest
   miejscem wrażliwym na podkroki — wrażliwy jest kontakt (`F-16`).
   → `F-19`
3. ❌ **OBALONE.** Bezwładność obrotowa nadwozia **nie wpływa na napęd w ogóle**
   (wynik identyczny do trzech miejsc przy obrocie zablokowanym i swobodnym).
   Powód: przy nasyceniu impuls silnika jest przycinany do `h·maxSpinTorque`,
   więc `spinMass` w ogóle nie wchodzi do wyniku. Zamiast tego wyszło coś
   ważniejszego: **silnik dostarcza 2,018× zadanego momentu**. → `F-20`

Blokady obrotu nadwozia zostają w kontrakcie Q3 mimo obalenia tego punktu —
uzasadnia je definicja quarter-cara, nie ten mechanizm.

Wbrew obawie, że schemat `bias`/`massScale` nie jest liniową sprężyną i mapa
`hertz ↔ N/m` będzie tylko przybliżeniem — **inwersja jest dokładna**. Punkt
pracy kontraktu Q3: zadane 13 500 N/m → `hertz` 3,17041 → osiągnięte
13 500,0 N/m, błąd **−0,000%**, ugięcie statyczne 111,1 mm.

Osobna pułapka tej samej klasy, **sprawdzona i czysta**: `JozzRig_FreezeMassEx`
kładzie `I_spin` na lokalnym Y (`jozz_wheel_rig.c:810`), a więz kręci wokół
Z ramki B (`wheel_joint.c:473`). Kontrola `b3Body_ApplyTorque` daje
`α/(τ/I_spin) = 1,013` przy czystości osi 1,0000 — ramki są ustawione poprawnie,
a masa i bezwładność koła są tym, co zamówiono.

**Uwaga metodyczna, warta więcej niż same liczby.** Pierwszy przebieg sondy dał
ugięcie ~7000× mniejsze od modelu i zerową różnicę między wariantami. Przyczyną
nie był silnik, tylko **kolejność wywołań w mojej własnej konstrukcji**:
`b3Body_SetMotionLocks` woła `b3UpdateBodyMassData`, gdy zmienia się status
`fixedRotation` (`src/body.c`), a ten przelicza masę **z kształtów** — nadwozie
bez shape'u traciło zadane 150 kg i stawało się nieruchomym sufitem. Poprawna
kolejność to `shape → blokady → masa`. Sonda ma od tego czasu sekcję 0, która
**odmawia pracy**, gdy zbudowany układ nie jest tym, który zamówiono. Gdyby jej
nie było, cała reszta tej sekcji byłaby pomiarem sufitu.

---

## 2. Manifest dowodowy — warunek, żeby przebieg był samopoświadczający

Dziś `build.bat` zapisuje sha commita, czystość `src/`, stempel `.lib` i wersję
kompilatora. To jest **sygnał, nie identyfikator zawartości**. Każdy przebieg
v2.1 generuje `evidence/<data>_<rig>/manifest.json`:

```
sha256 zrodla wheel_bench.c
sha256 wheel_bench.exe
sha256 box3d.lib
git HEAD + pelny git status --porcelain
kompilator + PELNE flagi kompilacji i linkowania
flagi CMake uzyte do zbudowania box3d.lib
OS, CPU, liczba rdzeni
znacznik czasu startu i konca
dokladna komenda uruchomienia
kod wyjscia procesu
seed kolejnosci wariantow
```

Do tego **surowe próbki**, nie tylko agregaty: `samples.csv` z jednym wierszem
na krok symulacji. Agregat bez surowych próbek nie da się ponownie
przeanalizować, a właśnie ponowna analiza wykryła połowę błędów v2.

---

## 3. Naprawa protokołu toczenia

### 3.1 Koniec z ukrytym rozbiegiem

v2: 120 kroków rozbiegu → 240 kroków pomiaru, raportowana jedna liczba na końcu.
v2.1 rozbija to na trzy jawne fazy i **raportuje stan między nimi**:

```
FAZA S  SETTLE      kolo bez ruchu poziomego, docisk narasta rampa 0.5 s,
                    czekamy na ustalenie penetracji.  <- usuwa udar startowy
FAZA W  WARMUP      opcjonalna, DOMYSLNIE ZERO krokow dla coast-down
FAZA M  MEASURE     okno pomiarowe
```

Raportujemy **stan na granicy każdej fazy**, nie tylko na końcu.

### 3.2 Sweep fazy początkowej wielokąta

Wielokąt o N podzielnym przez 4 może wystartować dokładnie na wierzchołku albo
dokładnie na ściance — i to zmienia wynik. v2 robił jeden deterministyczny
przebieg. v2.1 robi **8 przebiegów z fazą początkową 0…2π/N** i raportuje
medianę oraz rozrzut. Jeżeli rozrzut jest duży, to sam w sobie jest wynikiem.

### 3.3 Sweep zmiennych, które v2 zamroził bez uzasadnienia

```
obciazenie      440 / 950 / 1900 / 2800 N
tarcie          0.6 / 0.9 / 1.2
predkosc        5 / 13 / 25 m/s
substepy        2 / 4 / 8
grunt           pudlo / mesh regularny / mesh ze skanu
CCD             off / on
allowFastRotation   JEDNA wartosc we wszystkich rigach (W-4)
```

Nie wszystkie naraz — jedna zmienna główna na eksperyment (`KOLA_00` reguła 3).

---

## 4. Słownik metryk — jedna definicja, jedno miejsce

Połowa nieporozumień v2 wzięła się z metryk, których nazwa nie zgadzała się
z definicją (`flat b=`, „realny narożnik", `contact_%`). Od v2.1 metryka bez
wiersza w tym słowniku **nie może pojawić się w wydruku**.

| Metryka | Jednostka | Definicja operacyjna | Rig |
|---|---|---|---|
| `tow_force_mean` | N | średnia siła pozioma regulatora w oknie M | Q2 |
| `loss_power` | W | `tow_force_mean × v` | Q2 |
| `loss_per_meter` | J/m | `loss_power / v` | Q2 |
| `loss_per_rotation` | J/obr | `loss_per_meter × 2πR` | Q2 |
| `vx_at_M_start` | m/s | prędkość na granicy faz W→M | Q0–Q1 |
| `omega_at_M_start` | rad/s | prędkość kątowa tamże | Q0–Q1 |
| `distance_M` | m | droga przebyta w oknie M | wszystkie |
| `rotations_M` | — | `∫ω dt / 2π` w oknie M | wszystkie |
| `t_to_half_speed` | s | czas do spadku vx o połowę | Q0–Q1 |
| `t_to_stop` | s | czas do vx < 0.1 m/s | Q0–Q1 |
| `vy_rms` | m/s | RMS pionowej prędkości środka w oknie M **(porównywalne wyłącznie przy tej samej prędkości jazdy)** | wszystkie |
| `loaded_points` | szt. | punkty manifoldu z `totalNormalImpulse > 0` | wszystkie |
| `churn_per_meter` | 1/m | nowe punkty nośne na metr drogi | wszystkie |
| `churn_per_rotation` | 1/obr | nowe punkty nośne na obrót koła | wszystkie |
| `warm_start_retention` | % | udział impulsu przeniesionego z poprzedniego kroku | wszystkie |
| `impulse_spike_after_feature_switch` | N·s | skok impulsu w kroku po zmianie `featureId` | wszystkie |
| `penetration_deepest` | mm | najgłębsza separacja ujemna wśród punktów nośnych | wszystkie |
| `manifold_count` | szt. | liczba manifoldów na ciele koła | wszystkie |
| `contact_capacity_truncated` | bool | czy telemetria została obcięta (W-5) | wszystkie |
| `support_point_shift_0_to_1deg` | mm | przesunięcie punktu wsparcia przy 1° przechyłu — **NIE jest szerokością płaskiego pasa** | lab G |
| `flat_full_width` | mm | pełna szerokość płaskiego odcinka profilu (`2·hc`) | lab G |
| `apex_curvature` | m | promień krzywizny dokładnie w wierzchołku korony | lab G |
| `crown_drop` | mm | spadek promienia na zadanych odległościach od środka bieżnika | lab G |
| `marginal_cost_slope_0_8` | µs | nachylenie regresji kosztu z punktem n=0 | Q1c |
| `marginal_cost_slope_1_8` | µs | nachylenie **bez** punktu n=0 | Q1c |
| `cost_intercept`, `cost_r2`, `cost_residuals` | — | jakość dopasowania regresji | Q1c |

**Reguła:** każda metryka w wydruku ma obok **zakres sceny** i **rig**.
Bez tego nie wchodzi do `KOLA_01`.

---

## 5. Poprawki telemetrii i wydruków

```
W-5  b3Body_GetContactCapacity -> alokacja wlasciwej pojemnosci
     + flaga contact_capacity_truncated, glosno raportowana
W-6  drukowac totalImpulse i manifoldsAvg (dzis liczone i wyrzucane)
W-8  QueryPerformanceFrequency raz, do zmiennej statycznej
W-11 usunac falszywy wydruk "sphere: y_contact stays 0" (linia 969)
     -> "sfera: WYSOKOSC JAZDY stala przy kazdym camberze; lokalny punkt
         podparcia obraca sie razem z normalna. Geometria nie odroznia
         korony, barku i boku."
W-11 zmienic "flat b=" na "support_shift="; osobno drukowac flat_full_width
```

**Semantyka `totalNormalImpulse` do udokumentowania przed użyciem.**
Nazwaliśmy punkt z `totalNormalImpulse > 0` „nośnym". Zanim to zdanie awansuje
na definicję, trzeba przeczytać w `src/contact_solver.c`, czym dokładnie jest
to pole w tej wersji: sumą po podkrokach, impulsem maksymalnym, impulsem
rozwiązania końcowego — i czy może być dodatnie przy separacji spekulatywnej.
To jest godzina czytania kodu, która zabezpiecza wszystkie metryki kontaktu.

---

## 6. Atlas realnych profili — zamiast liczb bez źródła

`KOLA_01` §7.7 zawiera liczby „samochodowa korona 0.5–1.5 m", „motocyklowa
0.06–0.12 m", `I/mR² ≈ 0.55–0.80`. **Żadna nie ma w tym repo źródła.**
Zanim któraś stanie się wymaganiem:

```
1. wziac KONKRETNE assety Jozza (offroad, ktory juz istnieje, + kolejne)
2. zmierzyc ich przekroj z modelu: crown drop na kilku odlegloszciach
   od srodka biezni, promien barku, szerokosc plaskiego pasa
3. dolozyc kilka referencyjnych przekrojow innych klas opon ZE ZRODLEM
4. rozdzielic profil NIEOBCIAZONY i pozadany profil POD OBCIAZENIEM
5. zwalidowac wypuklosc; zaznaczyc wklesloszci, ktorych bryla wypukla
   nie uniesie - to jest lista wymagan dla F3, nie porazka F1
```

Dopiero atlas mówi, jakiej korony **naprawdę** potrzebujemy — i dopiero wtedy
`U-16` („czy wielokąt obrotowy potrafi tę koronę") ma sens.

---

## 7. Rejestr zmiennych i confoundów

Każdy przebieg wypisuje w nagłówku **wszystkie** poniższe, nawet niezmienione.
Zmienna, która nie jest wypisana, jest confoundem czekającym na swoją kolej.

```
GLOWNA        ta jedna, ktora zmieniamy
ZAMROZONE     masa, bezwladnosc, R, W, tarcie, krok, podkroki, watki,
              grawitacja, CCD, allowFastRotation, sleep, contactHertz,
              contactDampingRatio, contactSpeed, faza poczatkowa, seed
STAN          vx i omega na granicy KAZDEJ fazy
NIEKONTROLOWANE  turbo CPU, scheduling, kolejnosc wariantow
                 -> mitygacja: losowa kolejnosc + mediana z 7 + rozrzut
```

**Znane confoundy, które muszą zniknąć, a nie zostać opisane:**

| # | Confound | Usuwa go |
|---|---|---|
| C-1 | warianty w różnych stanach ruchu | Q2 (stała prędkość) |
| C-2 | ukryty rozbieg przed oknem | fazy S/W/M i raport granic (§3.1) |
| C-3 | metryki nienormalizowane na drogę | słownik §4 |
| C-4 | `allowFastRotation` różne między rigami | jedna wartość (§3.3) |
| C-5 | grunt pudełkowy zamiast meshu | obie wersje każdego przebiegu |
| C-6 | faza początkowa wielokąta | sweep 8 faz (§3.2) |
| C-7 | koszt aktywacji pierwszego ciała w regresji | slope 0–8 **i** 1–8 (§4) |

---

## 8. Kolejność wykonania i bramki

```
K-1  manifest dowodowy + slownik metryk + poprawki W-5/W-6/W-8/W-11
     BRAMKA: przebieg v2 powtorzony na nowym instrumencie musi dac
             te same liczby co 2026-07-27. Jesli nie - najpierw
             wyjasnic roznice, dopiero potem isc dalej.

K-2  rigi Q0 i Q1 z fazami S/W/M + pelna telemetria stanu
     PRODUKT: odpowiedz na U-19 (ile predkosci znika w rozbiegu)
     To jest ten pomiar, ktory rozstrzyga, ile z §7.3-§7.6 przezyje.

K-3  rig Q2 (staly ciag) + metryki na metr i na obrot
     PRODUKT: moc strat na kolo dla kazdego wariantu, na pudle i na meshu
     BRAMKA P-04: czy fasetowany obwod naprawde zjada moc?
     BRAMKA U-17: bilans energii przy rollingResistance OFF/ON

K-4  observer na MESHU (iteracja R1): rozklad naciskow, manifoldy,
     przejscia miedzy trojkatami, seams, pojemnosc kontaktow

K-5  rig Q3 (quarter-car)
     BRAMKA TRANSFERU: czy ranking z Q2 przezywa dolozenie masy resorowanej

K-6  atlas realnych profili (§6) - moze isc ROWNOLEGLE od poczatku
```

Iteracje R3 (laboratorium profilu) i R4 (prawo opony na sferze) wolno prowadzić
**równolegle** — O-1 na to pozwala i żadna z nich nie zależy od Q2.

---

## 9. Falsyfikatory — co obali każdą stronę sporu

```
OBALI "fasetowany obwod jest zly":
  w Q2 hull-42 wymaga mocy strat porownywalnej z bryla gladka
  (roznica ponizej rozrzutu miedzy fazami poczatkowymi)

OBALI "potrzebujemy nowej bryly":
  sfera lub elipsoida w Q3/Q4 daje charakterystyke, ktora Jozz
  akceptuje w V3, po dolozeniu samego prawa opony

OBALI "profil obrotowy jest obiecujacy":
  laboratorium manifoldu pokazuje, ze plaska korona oscyluje
  miedzy koncami odcinka wsparcia albo gubi featureId na mesh seam

OBALI "asymetria kierunkow (P-10)":
  pomiar przejsc korona-bark-bok w dachowaniu i klinowaniu pokazuje
  czestotliwosc porownywalna z obwodowa

OBALI "podatnosc nie pomaga":
  wlasny wiez normalny opony w Q2 obniza moc strat hulla
  do poziomu bryly gladkiej

OBALI "koszt na kolo jest liniowy":
  Q1c z 8 kolami na meshu ze skanu pokazuje R^2 ponizej 0.9
```

---

## 10. Czego ten protokół świadomie NIE robi

- nie wybiera bryły ani rodziny;
- nie dotyka `src/` ani `include/` — ani jednej linii;
- nie buduje nowego typu kształtu;
- nie przesądza, ile będzie backendów;
- nie wprowadza laboratorium manifoldu (V1b) — to osobna, późniejsza bramka;
- nie odpowiada na pytanie o feel. Na to odpowiada wyłącznie Jozz (V3).
