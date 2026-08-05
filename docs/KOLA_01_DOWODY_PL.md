# Koła i opony — fundament wiedzy (zmierzony)

Data założenia: 2026-07-25 | Baseline pierwszej rundy: `959aefb`
Status: akumulowany korpus dowodów; kolejne sekcje mają własną datę i provenance.
Liczby mają pochodzić z zachowanych uruchomień, nie z pamięci ani ręcznego
przepisywania. Surowe wydruki: `tools/jozz_wheel_bench/evidence/`; aktualny status
findingów: `KOLA_FINDINGS.json`. Bieżący stan implementacji: `CURRENT_STATE_INDEX_PL.md`.

---

## 1. Baseline produktowy — działa

`build/bin/Debug/jozz_vehicle_validation.exe` (z roota repo): **OK**, 18 sond + 2 mapowe, exit 0.
Nie ma potrzeby eksperymentu `BASELINE_REPAIR`.

Wymiary koła pochodzą z markerów Blockbencha i przechodzą asercje:

```
wheelRadius = 0.5141 m   (Marker_TireRadiusOuter - Socket_WheelMount)
wheelWidth  = 0.4375 m   (Marker_TireWidthRight - Marker_TireWidthLeft)
skala       = 0.35 m / jednostkę Blockbencha
```

Sondy produktowe (pełny pojazd, na torze testowym):

```
cylinder 12 ścianek : kontakt przodu   5%   vy RMS 1.542 m/s   top 11.9 m/s
cylinder 32 ścianki : kontakt przodu  31%   vy RMS 1.170 m/s   top 13.1 m/s
sfera               : kontakt przodu 100%   vy RMS 0.413 m/s   top 13.2 m/s
envelope: cylinder 44% | phased-union-4 13% | split 100% | sfera 100%
```

## 2. Mapa zdolności (BA3 pakietu — odrobione)

| Zdolność | Stan | Dowód |
|---|---|---|
| walidator headless z sondami | `PRESENT` | `samples/validation/*`, 18 sond |
| warianty geometrii koła | `PRESENT` | 4 tryby envelope, `jozz_vehicle_m6_suspension_rig.cpp:72` |
| izolowany stend jednego koła | `PRESENT` (nowy) | `tools/jozz_wheel_bench/` |
| telemetria kontaktu / manifoldu | **`ABSENT`** | brak jakiegokolwiek podglądu manifoldu |
| bilans energii | **`ABSENT`** | — |
| teren twardy | `PRESENT` | mesh ze skanu + płyta |
| teren deformowalny | `ABSENT` | — |
| render gate / zrzuty | `PRESENT` | `samples.exe --screenshot` |
| modyfikacje Box3D | **`ZERO`** | patrz §4 |
| rysowanie per-część modelu | `PRESENT` | `JozzVehicleRiggedPart::DrawPart*` |

**Największa luka: nie widzimy kontaktu.** Wszystkie dotychczasowe wnioski o kołach
opierają się na *skutkach* (procent kontaktu, RMS pionowy), nigdy na *mechanizmie*
(ile punktów manifoldu, czy `featureId` jest ciągłe, czy warm start przeżywa).

## 3. Fakty o silniku Box3D (odczytane z kodu, nie z dokumentacji)

### 3.1 Model tarcia jest już „opono-podobny"

```c
typedef struct b3Manifold {
    b3ManifoldPoint points[4];  // do 4 punktów, każdy z normalImpulse + featureId
    b3Vec3 normal;
    float  twistImpulse;        // tarcie skrętne wokół normalnej
    b3Vec3 frictionImpulse;     // JEDNO centralne tarcie na cały manifold
    b3Vec3 rollingImpulse;      // opór toczenia
}
```

Box3D nie liczy tarcia per punkt — liczy jedno tarcie w centrum plamy styku,
ograniczone sumą impulsów normalnych, w bazie `tangent1 = b3Perp(normal)`,
czyli **arbitralnej**. Kod: `src/contact_solver.c:173`, `:514`, `:744`.

**Korekta wcześniejszej tezy (ważna):** `twistImpulse` **nie jest** momentem
samonastawnym opony. Jest oporem skrętnym styku. Prawdziwy moment samonastawny
ma dwa źródła: mechaniczny trail (geometria castera — to już działa w rigu M7)
oraz pneumatic trail (asymetryczny rozkład nacisku wzdłuż plamy styku). Przy
**jednym** centralnym punkcie tarcia pneumatic trail **nie może powstać** —
siła boczna działa dokładnie w centroidzie. To jest twarde ograniczenie modelu
i osobne pytanie badawcze (Q-RUB-03).

### 3.2 Dwie ścieżki solvera

`contact_solver.c` ma ścieżkę skalarną (`*_Mesh`) i **SIMD** (`*_Convex`,
`b3Vec3W`/`b3FloatW`) plus `*_Overflow`. Każda zmiana prawa tarcia musi być
napisana dwa razy, raz wektorowo. To jest najdroższy technicznie fragment
całego programu.

### 3.3 Powierzchnia dispatchu typu kształtu (zmierzona, nie oszacowana)

Dla oceny kosztu dodania nowego typu kształtu:

```
129   miejsc `case b3_*Shape` w core  (shape.c 82, compound.c 20,
                                       world_snapshot.c 12, sensor.c 6,
                                       physics_world.c 6, mesh_contact.c 3)
126   odwołań do typów kształtów w recording/replay/snapshot
 21   publicznych funkcji API wspominających konkretny typ
 15   par w tabeli kolizji (`contact.c:126`) -> nowy typ dodaje do 7 procedur
```

`b3ShapeType` jest **uporządkowany alfabetycznie** — wstawienie nowej wartości
w środku przesuwa indeksy i psuje format nagrań. To jest patch klasy INVASIVE.

### 3.4 Pułapki, które ugryzą przy zmianie kształtu

- `rollingResistance` — **UWAGA, KOMENTARZ SILNIKA JEST NIEAKTUALNY.**
  `types.h:407` twierdzi „*only used for spheres and capsules*", ale
  implementacja `src/contact.c:646-684` daje promień także hullowi:
  `sphere → radius`, `capsule → radius`, **`hull → 0.25 * hull->innerRadius`**,
  pozostałe → 0. Zejście ze sfery na hull **nie wyłącza** oporu toczenia —
  zmniejsza go po cichu około czterokrotnie. To jest gorsze niż wyłączenie,
  bo nie widać tego w telemetrii. Zapis `P-14`.
  *Historia tego błędu: skopiowałem komentarz nagłówka do tego dokumentu
  2026-07-25 bez przeczytania implementacji; zewnętrzny audyt 2026-07-29
  „potwierdził" go w swojej tabeli `ENGINE FACT`. Trzy poziomy przeglądu,
  zero odczytów `contact.c`. Dlatego pole `code:` w `KOLA_FINDINGS.json`
  wskazuje na plik **implementacji**, nigdy na nagłówek.*
- `b3DefaultShapeDef()` ustawia `categoryBits` na **wszystkie bity** (inaczej
  niż Box2D). Split envelope działa wyłącznie wtedy, gdy świat konsekwentnie
  taguje powierzchnie. Mapa ze skanu tego nie robi — patrz §5.2.
- Solver flaguje ciało jako „szybkie", gdy ruch na krok > połowy najmniejszego
  wymiaru shape'u → koło r=0.51 przy >15 m/s wpada w CCD i głodzi TOI.
  Świat labu ma `b3World_EnableContinuous(false)` — decyzja M7 §5.5.

## 4. Provenance forka — stan idealny

```
origin    https://github.com/Jozzpoly/Box3d_FunProject.git
upstream  https://github.com/erincatto/box3d.git
merge-base  29bf523          nasze commity od merge-base: 200
upstream/main d421e45        upstream przed nami: 10 commitów

git diff <merge-base>..HEAD -- src include   ->   PUSTE (zero linii)
```

**Silnik jest nietknięty.** Cała praca projektu żyje w `samples/`.
Wchodzimy w erę modyfikacji Box3D z czystej kartki.

10 commitów upstreamu przed nami dotyka dokładnie naszych ścieżek:

```
e961bfb Friction center weighted average      -> środek tarcia (nasz kanał opony)
ef8ef01 Ghost collision improvements          -> krawędzie mesha (teren ze skanu)
aaa795e Edge edge optimization                -> koszt hull vs teren
        triangle_manifold.c  297 linii, convex_manifold.c 268, contact_solver.c 112
```

**Zastrzeżenie (Second Brain, przyjęte):** brak konfliktu w merge'u nie oznacza
braku zmiany zachowania, a „ghost collision improvements" nie jest automatycznie
rozwiązaniem faset obwodu koła. Aktualizacja upstreamu jest **osobnym
eksperymentem utrzymaniowym** z pomiarem przed/po, związanym z dokładnym SHA.

---

## 5. Stend badawczy — wyniki (2026-07-25) — `SUPERSEDED`

> **CAŁA SEKCJA 5 JEST HISTORYCZNA. NIE UŻYWAĆ DO DECYZJI.**
> Etykiety `VERIFIED-MEASURED`, `TWARDA ŚCIANA` i `argument wydajnościowy jest
> MARTWY`, które padają niżej, pochodzą z przebiegu v1 i **nie są aktualnymi
> statusami**. Aktualny status każdego `F-xx` jest w `docs/KOLA_FINDINGS.json`
> i **tylko tam** — w szczególności `F-07` jest `SUPERSEDED` przez `F-08`.
> Sekcja zostaje w całości, bo wyniki negatywne są produktem (`KOLA_00` reguła 6),
> ale agent szukający `F-07` grepem trafi tu **przed** §7 i musi to zobaczyć.

Stend: `tools/jozz_wheel_bench/`, linkuje `box3d.lib` (Release), nie dotyka repo.
Koło R = 0.5141 m, W = 0.4375 m. Surowy wydruk w `evidence/`.

Surowy przebieg v1 (`run_2026_07_25_baseline.txt`) jest zachowany i zahaszowany,
ale **nie jest parsowany** przez `tools/evidence/evidence.py` — jego format
różni się od v2 i automatyczne mapowanie go na schemat v2 dawałoby ciche
przekłamania. Liczby w tej sekcji nie są związane z żadnym blokiem `EVIDENCE`.

### 5.1 Budżet hulla — TWARDA ŚCIANA

Limit silnika to **255 półkrawędzi** na hull (indeksy `uint8_t`, `src/hull.c:30`).
Pryzmat o N ściankach ma 6N półkrawędzi.

```
sides   verts  halfedg  faces   ripple_mm       <- ripple = pełne bicie promienia
   16      32       96     18       9.878          mierzone funkcją wsparcia
   24      48      144     26       4.398          (dokładnie to, co widzi grunt)
   32      64      192     34       2.476   <- dzisiejszy default
   40      80      240     42       1.585
   43+                                  b3CreateHull zwraca NULL
-> MAKSYMALNY UŻYTECZNY PRYZMAT = 42 ŚCIANKI
```

Profil z zaokrąglonym barkiem (rings = pierścienie wierzchołków w poprzek szerokości):

```
rings=2 (ostry bark)   -> max 42 ścianek   ripple 1.2 mm
rings=3                -> max 24 ścianki   ripple 4.4 mm
rings=4 (miękki bark)  -> max 18 ścianek   ripple 9.9 mm
rings=6                -> max 10 ścianek   ripple 25 mm
```

> **F-01 (VERIFIED-MEASURED).** W stockowym Box3D **nie da się** mieć jednocześnie
> gładkiej powierzchni toczenia i zaokrąglonego barku opony w jednym hullu.
> Budżet 255 półkrawędzi wymusza wybór: albo rozdzielczość obwodowa, albo profil.
> Zakres: hull convex, format hulla z indeksami uint8.
> Konsekwencja: **detal bieżnika z Blockbencha nigdy nie będzie geometrią fizyki.**
> Makroprofil niesie fizykę, voxelowy detal zostaje wizualny.

### 5.2 Masa i bezwładność — confound POTWIERDZONY z runtime

`b3Body_GetMassData` po zbudowaniu każdego envelope'u ścieżką produktową
(gęstość 77 kg/m³, ta sama co w produkcie):

```
envelope          mass_kg   I_spin   I_trans   I_spin/mr^2
sfera               43.83    4.633     4.633       0.400
cylinder-32         27.79    3.649     2.268       0.497
split (sfera+cyl)   43.83    4.633     4.633       0.400
phased union-4      27.79    3.649     2.268       0.497
pryzmat-42          27.87    3.669     2.279       0.498
profil opony-18     23.47    2.674     1.657       0.431
odniesienie: kula pełna 0.400 | realna opona+felga ~0.55-0.80
```

> **F-02 (VERIFIED-MEASURED).** Zmiana reprezentacji kolizji zmienia masę koła
> o **1.87×** (43.8 → 23.5 kg) i moment obrotowy o **1.73×**, bo masa pochodzi
> z objętości collidera. Każde dotychczasowe porównanie „sfera vs cylinder"
> mieszało geometrię z bezwładnością.
> **Dodatkowo:** dzisiejsze koło ma `I/mr² = 0.400`, czyli bezwładność **kuli
> pełnej**, a nie opony z masą przy obręczy (~0.55–0.80). Koło rozpędza się za
> łatwo, próg zerwania przyczepności jest przesunięty, żyroskop za słaby.
> **Wniosek operacyjny:** masa i bezwładność muszą pochodzić z `WheelSpec`
> i być ustawiane jawnie przez `b3Body_SetMassData` — ZANIM zaczniemy porównywać
> jakiekolwiek kształty.

### 5.3 Jakość toczenia przy ZAMROŻONEJ masie — trzy obalenia

Jedno koło, 44 kg, `I/mr²` = 0.70/0.55 identyczne dla wszystkich wariantów,
13 m/s, 4 s, płaski grunt (hull), tarcie 1.2, CCD off.

```
envelope         vy_rms   kontakt%   bicie środka   v końcowa (z 13.0)
sfera             0.125     100.0        0.05 mm       12.97
cylinder-32       0.500      70.4         176 mm        9.68
split             0.500      72.9         173 mm        9.68
phased union-4    0.189      98.8          18 mm        8.17
pryzmat-42        0.544      44.2         121 mm        9.77
profil opony-18   0.702      35.4         230 mm        6.64
```

> **F-03 (VERIFIED-MEASURED) — model „więcej ścianek = gładziej" JEST FAŁSZYWY.**
> Pryzmat-42 (bicie 1.59 mm) toczy się **gorzej** niż cylinder-32 (bicie 2.48 mm):
> kontakt 44% vs 70%. Wcześniejsza predykcja skalowania `1/N` została obalona
> własnym pomiarem. Dominującym mechanizmem nie jest amplituda bicia, tylko
> najprawdopodobniej **churn tożsamości kontaktu i utrata warm startu** —
> przy 42 ściankach i 13 m/s cecha kontaktu zmienia się ~169 razy/s przy solverze
> pracującym 240 razy/s. **To wymaga potwierdzenia telemetrią manifoldu (I1);
> dopóki jej nie ma, mechanizm pozostaje hipotezą, a obalenie modelu 1/N faktem.**

> **F-04 (ODRZUCONY PRODUKTOWO; MECHANIZM SKORYGOWANY 2026-07-26).**
> **Co zmierzono:** na gruncie **bez masek kategorii** (stend NIE odtwarzał
> logiki filtrów — obie bryły były aktywne) agregaty split i cylindra były
> nieodróżnialne (0.5001 vs 0.5004 vy RMS). Zakres: jedno wolne koło 44 kg,
> 13 m/s, płaski hull, 4 s.
> **Czego to NIE dowodzi:** że „split staje się cylindrem". Obie bryły mają ten
> sam promień zewnętrzny, więc kontaktują JEDNOCZEŚNIE; rozdziału mechanizmu
> nie wykonano.
> **KOREKTA mechanizmu odczucia Jozza (REPO FACT, ważniejsza):**
> `jozz_vehicle_scan_import.cpp:241` ustawia **wszystkim** wejściom skanu
> `role = JozzScanRole::Terrain`, a `:189` nadaje kategorię terenu wyłącznie
> roli Terrain. Cała mapa ze skanu — łącznie ze skałami i krawędziami — jest
> **jednym meshem otagowanym jako teren**. Na tej mapie aktywna jest więc
> WYŁĄCZNIE tocząca się sfera, a cylinder boczny nie włącza się nigdy.
> Jozz jeździ po skanie **na czystej sferze z wybrzuszeniem 0.295 m** — co
> dokładnie odpowiada jego opisowi („wyczuwam mocno sferyczną kolizję
> z terenem... ciągle zahacza"). To jest degeneracja **przeciwna** do tej,
> którą zmierzył stend. Oba są awariami tego samego projektu, ale mechanizm
> podany pierwotnie był błędny dla realnej sytuacji produktowej.
> **Status kierunku:** odrzucony produktowo decyzją właściciela (2026-07-25);
> mechanizm pozostaje OPEN.

> **F-05 (VERIFIED-MEASURED) — metryki się nie zgadzają, ranking zależy od metryki.**
> Najgładszy po sferze jest phased-union-4 (98.8% kontaktu), ale to on **traci
> najwięcej energii** (8.17 z 13.0 m/s). Sfera traci najmniej (12.97). Jakikolwiek
> werdykt oparty na jednej metryce byłby błędny. Wymagane minimum: gładkość
> + ciągłość kontaktu + zachowanie energii + koszt.

> **F-06 (OBSERWACJA, ostrzeżenie metodyczne) — ranking zależy od stendu.**
> Historyczna sonda produktowa (pełny pojazd) dała phased-union-4 = **13%**
> kontaktu, najgorszy wynik. Ten sam wariant na izolowanym stendzie daje **98.8%**,
> drugi najlepszy. Stend i pojazd dają **przeciwne** odpowiedzi.
> **Reguła: żaden wariant nie może zostać przyjęty ani odrzucony na podstawie
> samego stendu. Stend służy do izolacji mechanizmu; werdykt zapada na pojeździe.**

### 5.4 Koszt CPU — argument wydajnościowy jest MARTWY

4 koła, 600 kroków × 3 powtórzenia, QPC, best-of-3, 1 worker, Release.

```
                  grunt hull            grunt MESH
envelope        ms/krok  us/koło     ms/krok  us/koło
sfera            0.0059    1.48       0.0089    2.23
cylinder-32      0.0052    1.31       0.0089    2.23
split            0.0077    1.94       0.0132    3.30
phased union-4   0.0136    3.39       0.0292    7.31
pryzmat-42       0.0051    1.28       0.0095    2.37
profil opony-18  0.0050    1.25       0.0089    2.23
```

> **F-07 (VERIFIED-MEASURED).** Liczba ścianek **nie kosztuje nic** (pryzmat-42
> ≈ cylinder-32 ≈ sfera). Kosztuje **liczba shape'ów na kole**: split 1.5×,
> phased union 3.3×. Budżet: koło na terenie mesh to ~2.2 µs. Klatka 60 Hz ma
> 16 700 µs. Cztery koła = 9 µs = **0.05% klatki**.
> **Konsekwencja strategiczna:** wydajność geometrii kolizji nie jest i długo nie
> będzie ograniczeniem. Rodzina „miękki pierścień z wielu ciał" **nie jest
> wykluczona kosztem kolizji** — jej ryzyko leży w stabilności constraintów
> i kosztu solvera, nie w narrow-phase. Zakres: 1 worker, Release, płaski grunt,
> brak zawieszenia. Do powtórzenia na pojeździe i na terenie ze skanu.

### 5.5 Czego stend NIE dowodzi (uczciwe granice)

- brak zawieszenia, brak napędu, brak nadwozia → dynamika pionowa jest
  „naga"; wartości bezwzględne nie przenoszą się na auto;
- płaski grunt i regularny mesh; teren ze skanu ma inną statystykę trójkątów;
- jedno koło, jedna proporcja (R/W = 2.35); inne proporcje nie były testowane;
- jeden timestep (60 Hz / 4 substepy); brak sweepu;
- brak pomiaru manifoldu — mechanizm F-03 pozostaje hipotezą;
- faza początkowa koła nie była randomizowana (możliwy efekt szczególnego przypadku);
- crash stendu przy pierwszej wersji był błędem harnessu (grunt kończył się pod
  kołami), nie błędem Box3D — poprawione, ale warto pamiętać przy interpretacji
  starszych wydruków.

---

## 5.6 Wady protokołu pomiarowego (audyt własny, 2026-07-26)

Przed użyciem liczb z §5.4 i §5.3 do jakiejkolwiek decyzji trzeba wiedzieć, że:

```
1. wheel_bench.c ExperimentCost: komentarz mowi "best-of-3 medians / 2000 krokow",
   kod bierze MINIMUM z 3 x 600 krokow. Komentarz jest nieprawdziwy.
2. "rewind the wheels" miedzy repami to PUSTA petla (no-op) - kolejne repy mierza
   kolo o innej predkosci i na innym odcinku gruntu. Min-of-3 jest wiec minimum
   po ROZNYCH stanach fizycznych, nie po powtorzeniach tego samego stanu.
3. "us/kolo" = caly b3World_Step / 4. To NIE jest koszt krancowy kola - zawiera
   staly narzut swiata (broadphase, BVH gruntu, solver setup). Brak serii 0/1/2/4/8 kol,
   wiec nachylenia (prawdziwego kosztu na kolo) NIE zmierzono.
4. brak surowych czasow i rozrzutu - podano tylko jedna liczbe na wariant.
5. contact_% liczy kontakty z flaga b3_contactTouchingFlag (body.c:484). Przy
   kontaktach spekulatywnych punkt moze miec dodatnia separacje i zerowy impuls,
   wiec to jest contact_touching_%, NIE kontakt nosny. Metryka nosna to
   totalNormalImpulse > 0 - niezmierzona.
6. obciazenie = wlasny ciezar kola (44 kg ~ 432 N). Realny narożnik pojazdu to
   ~1900 N (M7 §1.2). Stend pracuje przy ~1/4 realnego nacisku, a ostrosc
   uderzen fasety jest zalezna od obciazenia.
7. vx_end mierzone po 1 s rozgrzewki - warianty wchodza w okno pomiarowe
   z ROZNYMI predkosciami, a opor toczenia zalezy od predkosci.
8. build.bat nie zapisuje SHA repo, stanu dirty, hasha biblioteki ani wersji
   kompilatora. box3d.lib zbudowano 2026-07-24 11:01, najnowszy plik w src/
   ma 2026-07-03, `git status -- src include` jest czysty -> biblioteka
   ODPOWIADA zrodlom, ale artefakt sam tego nie poswiadcza.
9. naglowek tego dokumentu ("working tree czysty @ 959aefb") opisuje snapshot
   WEJSCIOWY; dokumenty i stend powstaly pozniej i nie sa zacommitowane.
```

Konsekwencja: **wszystkie liczby z §5.4 mają status wstępny.** Porównania
względne w obrębie jednego przebiegu są użyteczne; wartości bezwzględne
„µs na koło" nie są.

## 6. Wniosek o zakresie ograniczonym do przebadanej rodziny

Zestawienie F-01, F-03, F-04 i F-07 daje jedno twarde stwierdzenie:

> **Wśród przetestowanych stockowych brył i hulli nie znaleziono reprezentacji
> łączącej gładkie toczenie, poprawną szerokość i sterowalny profil.**
>
> Zakres dowodu: bryły `b3_sphereShape` i `b3_hullShape`; hulle budowane przez
> `b3CreateHull` z dwóch generatorów (równomierny pryzmat, równomierna
> powierzchnia obrotowa z barkiem); jedno koło R/W = 2.35; jedno obciążenie;
> jeden timestep.
>
> **Czego ten wniosek NIE wyklucza:** kapsuł, compound shape'ów, wielu
> osobnych shape'ów rozłożonych w poprzek, nierównomiernych rozkładów
> wierzchołków, zmiękczenia kontaktu na poziomie świata, modeli
> aplikacyjnych (probe/tire law) i struktur wielociałowych.
>
> **Kryterium obalenia:** dowolna konfiguracja stockowa, która na tych samych
> scenach osiągnie ciągłość kontaktu i agitację pionową porównywalną ze sferą
> przy szerokości bryły ≤ szerokości opony.

To przekształca pytanie „czy warto modyfikować Box3D?" w pytanie badawcze:
**„czy któraś z niesprawdzonych rodzin osiąga to bez zmiany silnika, a jeśli
nie — która zmiana jest najmniejsza?"** Rodziny: `KOLA_02` §4 (statusy cofnięte
2026-07-26). Zasady wejścia w core: `KOLA_03`.

---

## 7. Stend v2 (2026-07-27) — ulepszony instrument, wyniki PROWIZORYCZNE

> **Status całej sekcji po audycie zewnętrznym z 2026-07-27 (`PROVISIONAL`).**
> Każdy wynik §7 został skonfrontowany z kodem stendu. Instrument v2 jest
> **istotnie lepszy od v1, ale nadal kalibrowany**: ma dwie wady protokołu,
> które zanieczyszczają wszystkie metryki jakości toczenia (§7.9). Żaden wynik
> §7 nie zamyka rodziny rozwiązań. Statusy podniesie dopiero stend v2.1
> (`KOLA_05`) i transfer na poziom V2.

Surowy przebieg: `tools/jozz_wheel_bench/evidence/run_2026_07_27_v2.txt`
Provenance w nagłówku wydruku: sha `959aefb`, core clean, `box3d.lib`
24.07.2026 11:01 / 2 769 746 B, MSVC 19.51.36248.

Provenance jest **sygnałem, nie identyfikatorem zawartości**: brak SHA-256
źródła / `.exe` / `.lib`, brak flag CMake, brak kodu wyjścia i komendy.
Status: `PROVENANCE IMPROVED, NOT SELF-CERTIFYING`. Naprawa w `KOLA_05` §2.

### 7.1 Co naprawiono w protokole

| Wada v1 (§5.6) | Naprawa v2 |
|---|---|
| „mediana z 2000" w komentarzu, min z 3×600 w kodzie | 7 przebiegów × 400 kroków, **mediana**, raportowany rozrzut |
| pusta pętla „rewind" | **każdy przebieg buduje świat od zera** |
| koszt = krok świata / 4 | seria **0/1/2/4/8 kół**, koszt = nachylenie najmniejszych kwadratów |
| `contact_%` = flaga touching | **punkty z `totalNormalImpulse > 0`** + osobno churn i penetracja |
| wolne koło 44 kg (432 N) | **stały docisk pionowy do ~1900 N** (`CONSTANT DOWNFORCE`, **nie** quarter-car), plus przebieg kontrolny przy 432 N |
| brak provenance | sha + dirty + stempel lib + wersja kompilatora w nagłówku |

Nowe laboratoria: **F** (sweep podatności kontaktu) i **G** (profil analityczny
bez silnika: krzywizna korony, obwodu i boku, przemieszczenie punktu styku
przy camberze 0→90°). Pełny zestaw eksperymentów v2 to **A–G**.

**Nazwa „realne obciążenie narożnika" była błędna i została wycofana.**
`RunRoll` tworzy jedno wolne koło o masie 44 kg i dokłada stałą siłę pionową
`1900 − 44·9.81 N`. Świat ma grawitację `−10 m/s²` (`src/types.c:16`), więc
rzeczywisty nacisk to ~**1908 N**. Układ ma nacisk odpowiadający ~194 kg, ale
**poziomą bezwładność 44 kg** — to prasa dociskająca lekkie koło, nie narożnik
pojazdu. Narożnik ma też pęd, zawieszenie i wymianę energii z masą resorowaną.

### 7.2 F-08 — koszt krańcowy koła (zastępuje F-07) — `PRELIMINARY`

> **ZERWANY ŁAŃCUCH DOWODOWY — naprawiony 2026-07-29.** Tabela, która stała
> w tym miejscu od 2026-07-27, zawierała dziesięć liczb, z których **żadna** nie
> występowała w zachowanym surowym przebiegu (`sfera 1.14` vs `1.13`, `mesh 2.01`
> vs `2.12`, `union-4 mesh 12.19` vs `12.24`, deklarowany „rozrzut 4–14%" vs
> realne maksimum **47.4%**). Różnice rzędu 1–5% wyglądały wiarygodnie i dlatego
> przetrwały dwa dni oraz jeden audyt. Nie wiem, czy pochodziły z przebiegu
> pośredniego, którego nie zachowano, czy z przepisania z kontekstu — i **to
> właśnie jest problem**: nie da się tego odróżnić po fakcie. Tabela poniżej jest
> od teraz **generowana** z surowego logu przez `tools/evidence/evidence.py`.
> Ręczna edycja wewnątrz bloku wywala `check`.

`BENCH FACT` (zakres: 1 wątek, płaski grunt, 60 Hz / 4 podkroki, koło R/W = 2.35,
docisk 1908 N, mediana z 7 świeżych światów; `us/wheel` = **nachylenie** regresji,
czyli koszt jednego dodatkowego koła):

Grunt pudełkowy:

<!-- EVIDENCE:BEGIN run=2026_07_27_v2 id=D.box -->
| envelope | n=0 ms | n=1 ms | n=2 ms | n=4 ms | n=8 ms | us/wheel | spread% |
|---|---|---|---|---|---|---|---|
| sphere | 0.0002 | 0.0038 | 0.0045 | 0.0059 | 0.0104 | 1.13 | 3.1% |
| cylinder-32 | 0.0002 | 0.0045 | 0.0058 | 0.0082 | 0.0147 | 1.67 | 3.9% |
| phased union-4 | 0.0002 | 0.0111 | 0.0134 | 0.0181 | 0.0346 | 3.88 | 5.8% |
| prism-Nmax | 0.0002 | 0.0059 | 0.0085 | 0.0130 | 0.0222 | 2.58 | 16.8% |
| tire profile | 0.0002 | 0.0039 | 0.0047 | 0.0062 | 0.0108 | 1.18 | 18.6% |

*zrodlo: `run_2026_07_27_v2.txt` sha256 `4427cbffaf22772a` tabela `D.box` - wygenerowane, nie przepisywac recznie*
<!-- EVIDENCE:END -->

Grunt MESH:

<!-- EVIDENCE:BEGIN run=2026_07_27_v2 id=D.mesh -->
| envelope | n=0 ms | n=1 ms | n=2 ms | n=4 ms | n=8 ms | us/wheel | spread% |
|---|---|---|---|---|---|---|---|
| sphere | 0.0002 | 0.0033 | 0.0052 | 0.0092 | 0.0176 | 2.12 | 17.2% |
| cylinder-32 | 0.0002 | 0.0053 | 0.0085 | 0.0162 | 0.0318 | 3.89 | 14.7% |
| phased union-4 | 0.0002 | 0.0141 | 0.0267 | 0.0505 | 0.0990 | 12.24 | 47.4% |
| prism-Nmax | 0.0002 | 0.0064 | 0.0107 | 0.0206 | 0.0403 | 4.95 | 6.3% |
| tire profile | 0.0002 | 0.0040 | 0.0064 | 0.0113 | 0.0215 | 2.60 | 4.1% |

*zrodlo: `run_2026_07_27_v2.txt` sha256 `4427cbffaf22772a` tabela `D.mesh` - wygenerowane, nie przepisywac recznie*
<!-- EVIDENCE:END -->

Kolumna n=0 (sam świat + grunt) = 0.0002 ms dla wszystkich.
**Skok n=0 → n=1 jest kosztem posiadania jakiegokolwiek dynamicznego ciała,
nie kosztem koła** — dlatego v1, dzieląc krok świata przez 4, zawyżał koszt
krańcowy około dwukrotnie. Kolumna `spread%` pokazuje, że rozrzut sięga
**47.4%** (`union-4` na meshu), więc różnice poniżej kilkunastu procent między
wariantami **nie są rozstrzygalne tym pomiarem**.

**Confoundy F-08, przez które to jest `PRELIMINARY`, nie „koszt na koło":**

1. **Różne stany dynamiczne.** `TimeFreshWorld` daje 60 kroków rozbiegu pod
   pełnym dociskiem, po czym mierzy 400 kroków. Sfera nadal jedzie, hulle są
   niemal nieruchome. Mierzymy koszt **każdego wariantu w jego własnym stanie**,
   nie koszt tych samych ruchów i tych samych kontaktów.
2. **`allowFastRotation` niespójne.** `RunRoll` ustawia `true` (linia 603),
   `TimeFreshWorld` zostawia domyślne (linia 436–443). Koszt i jakość toczenia
   nie przechodzą przez ten sam limit rotacji.
3. **Punkt n=0 w regresji.** Przejście „brak dynamicznego ciała → pierwszy
   body/contact/island" ma koszt stały, który nie należy do nachylenia na koło.
   Trzeba raportować slope 0–8 **i** 1–8, intercept, R², reszty.
4. **Krótkie okno.** 400 kroków to ~0.08 ms dla pustego świata; narzut timera
   i `QueryPerformanceFrequency` przy każdym `NowMs()` (linia 64) zanieczyszcza
   przede wszystkim baseline n=0.
5. **Stała kolejność wariantów**, brak przypięcia do rdzenia, brak kontroli turbo.

Zakres, który obroni się dziś: *w tym scenariuszu koszt na meshu skalował się
w przybliżeniu liniowo z liczbą kół, a `union-4` był wielokrotnie droższy od
pojedynczego shape'u*. **Nieudowodnione:** uniwersalny koszt na koło w produkcie,
koszt nowej bryły analitycznej, budżet miękkiej opony, koszt przy CCD.

### 7.3 F-09 — pod stałym dociskiem 1900 N ranking się ODWRACA

`BENCH FACT` (zakres: **jedno wolne koło pod stałym dociskiem**, v startowe
13 m/s, płaski grunt **pudełkowy** (nie mesh), masa 44 kg, I_spin 0.70 mR²,
tarcie gruntu 1.2, CCD off; **UWAGA na dwa confoundy opisane niżej**):

```
                 vy_rms   ptNosne   churn_%   penet_mm   vx_end
sfera            0.5060      1.00       0.0     13.486    12.93
cylinder-32      0.1481      2.23      37.7      0.097     0.08
phased union-4   0.0641      4.21      31.6      0.005     0.14
prism-42         0.2465      2.28      69.9      0.277     0.66
profil opony-18  0.0837      2.63       2.4      0.088     0.01
```

Przy 432 N (zbliżenie do warunków v1) sfera miała `vy_rms` 0.125 i była
**najlepsza**; przy 1900 N ma 0.506 i jest **najgorsza**. Zarazem jest jedyną
bryłą, która nie traci prędkości. To nie jest bezpośrednie powtórzenie v1:
v1 miał 1 s rozbiegu, v2 ma 2 s, a tabela 432 N nie drukuje `vx_end`.

**Confound 1 — `vy_rms` jest nieporównywalne między wierszami.** Wszystkie
warianty fasetowe **zatrzymują się**, a nieruchome koło ma z definicji niską
agitację pionową. Miarodajną kolumną tego przebiegu jest `vx_end`.

**Confound 2 — ukryty rozbieg, który zjada zjawisko.** Kod robi **120 kroków
(2 s) rozbiegu pod pełnym dociskiem**, a dopiero potem 240 kroków (4 s) pomiaru
(`wheel_bench.c:615` i `:622`). `vx_end` jest więc prędkością po **6 s**, nie po
4 s. Nie zapisano `vx` ani `omega` na **początku** okna pomiarowego, dystansu,
liczby obrotów ani czasu zatrzymania. Prosty model wielokąta (§7.5) przewiduje,
że hulle tracą większość prędkości **przed** rozpoczęciem pomiaru:

```
predkosc po 2 s rozbiegu wg modelu idealnego (uwzgledniajac,
ze zwalniajace kolo robi MNIEJ obrotow niz 8.05):
  N=18   ~1.6 m/s      N=32   ~3.0 m/s      N=42   ~3.5 m/s
```

Czyli okno mierzy głównie koło, które **już prawie stoi**. Wszystkie metryki
jakości toczenia z §7.3–§7.6 są tym zanieczyszczone. Naprawa: `KOLA_05` §3.

### 7.4 F-10 — churn tożsamości kontaktu jako mechanizm F-03 — `STRONGLY SUPPORTED`, nie rozstrzygnięty

`BENCH FACT` + `STRONG INFERENCE`. Churn (udział punktów nośnych, które nie
istniały w poprzednim kroku) porządkuje `vy_rms` niemal idealnie:

```
profil opony-18   N=18   churn  2.4%   vy_rms 0.084
cylinder-32       N=32   churn 37.7%   vy_rms 0.148
prism-42          N=42   churn 69.9%   vy_rms 0.247
sfera             gladka churn  0.0%   vy_rms 0.506 (ale patrz confound 7.3)
```

Przy 13 m/s i R = 0.514 koło robi 4.03 obr/s. Liczba przejść przez fasetę na
krok 60 Hz: N = 18 → 1.2 ; N = 32 → 2.15 ; N = 42 → 2.82. **Churn rośnie
liniowo z N.** Amplituda tarki maleje jak 1/N². To są dwa przeciwstawne
mechanizmy — dlatego „więcej ścianek = gładziej" było fałszywe (F-03) i dlatego
nie istnieje dobre N.

**Czego ten wynik NIE dowodzi.** Profil-18 ma churn 2.4%, ale też `vx_end` 0.01
— prawie nie jedzie, więc miał mało okazji do generowania nowych cech w oknie
pomiarowym. Korelacja churn ↔ `vy_rms` jest policzona na wariantach będących
w **różnych stanach ruchu**. Żeby churn awansował z „silnego współsprawcy" na
„mechanizm dominujący", potrzeba: churnu **na metr** i **na obrót**, korelacji
czasowej `zmiana featureId → skok impulsu → skok vy → strata energii`, oraz
stałej prędkości. Status: `CONTRIBUTING MECHANISM, STRONGLY SUPPORTED,
NOT PROVEN DOMINANT`.

### 7.5 F-11 — analityczna strata energii wielokąta — `MODEL FACT` + `BENCH WARNING`

Model idealnego koła bezobręczowego (*rimless wheel*). Przy przejściu przez
wierzchołek wielokąta moment pędu względem nowego punktu styku jest zachowany:

```
omega' / omega = (I_cm + m R^2 cos(2pi/N)) / (I_cm + m R^2)
```

Dla I_cm = 0.70 m R² (dokładnie tyle ustawia `FreezeMass`, linia 263) i N = 32:

```
omega'/omega           = 0.988697
strata OMEGA na wierzcholek  = 1.13%
strata ENERGII na wierzcholek = 2.25%   (kwadrat)
strata energii na pelny obrot = ~51.7%
```

> **Korekta wcześniejszej wersji tego dokumentu:** było „1.13% straty energii
> na wierzchołek". 1.13% dotyczy **prędkości kątowej**; strata energii to 2.25%.
> Liczba ~51% na obrót była policzona poprawnie (z kwadratu).

Strata na obrót skaluje się jak `4 pi^2 (m R^2 / I_c) / N`, gdzie
**`I_c = I_cm + m R^2` = moment bezwładności względem punktu styku** (u nas
1.70 mR²). Żeby zejść do 1% na obrót, trzeba **N ≈ 2300 ścianek**. Limit
formatu hulla to **42** (F-01).

**Założenia modelu, których stend nie spełnia:** idealnie sztywny regularny
wielokąt, czysty obrót wokół bieżącego wierzchołka, brak poślizgu przed
udarem, natychmiastowy niesprężysty transfer podparcia, **pojedynczy** punkt
kontaktu, brak miękkości solvera, idealna płaszczyzna. Box3D ma manifold
wielopunktowy, kontakt spekulatywny, soft-step, substepy, tarcie i penetrację.
Stend mierzy 2.23–2.28 punktu nośnego na krok, więc „pojedynczy punkt" nie
zachodzi.

**Niespójność wymagająca wyjaśnienia:** idealny stosunek energii przy udarze
**nie zależy od docisku**. Skoro 432 N i 1900 N dają dramatycznie inny wynik,
w stendzie uczestniczą także tarcie, poślizg, solver, sposób przykładania
docisku, penetracja i faza początkowa. Ten model **nie tłumaczy jeszcze
wszystkiego**.

> **Uczciwy zakres.** `MODEL FACT`: idealny sztywny wielokąt ma duże straty przy
> kolejnych zmianach punktu podparcia. `BENCH WARNING`: test constant-downforce
> pokazuje ekstremalną utratę prędkości hulli. **OTWARTE:** ile z tego przenosi
> się na stałą prędkość, quarter-car, pełny pojazd i inne procedury kontaktu
> (convex margin, wygładzone normalne, własny generator manifoldu).
> **Kryterium obalenia:** fasetowany obwód, który w rigu Q2 (stała prędkość)
> potrzebuje mocy strat porównywalnej z bryłą gładką.

Niezależny od tego modelu, czysto **kinematyczny** argument przeciw fasetowaniu
obwodu: tarka wysokości podparcia `R(1 − cos(pi/N))` wynosi 1.44 mm p-p przy
N = 42 i 4 obr/s wzbudza ją 168 razy na sekundę. To zachodzi niezależnie od
solvera — ale samo w sobie nie mówi, jak duża jest odpowiedź pojazdu.

### 7.6 F-12 — globalny `contactHertz` NIE pochłania tarki (moja hipoteza obalona)

> **KOREKTA 2026-07-29 — sweep ma sześć etykiet, ale PIĘĆ ustawień.**
> `src/physics_world.c:1114`: `contactHertz = min(zadany, 0.125 · inv_h)`,
> a `inv_h = subStepCount · inv_dt` = 4 · 60 = 240, więc sufit wynosi **30 Hz**.
> Kolumny `60Hz` i `30Hz` poniżej są **tym samym przebiegiem** — ich identyczność
> nie jest wynikiem fizycznym ani „prawdopodobnym clampem", tylko bezpośrednim
> skutkiem kodu (`P-15`). Realny zakres sweepu to **30 → 6 Hz**.
> Dodatkowo tarcie sceny wynosiło **0.849**, nie 1.2 (`P-16`).

`BENCH FACT` (zakres: sweep `contactHertz` **efektywnie 30/30/20/15/10/6 Hz**
przy domyślnym tłumieniu 10.0 i `contactSpeed` 3.0, obciążenie 1908 N,
tarcie efektywne 0.849):

```
                 60Hz     30Hz     20Hz     15Hz     10Hz      6Hz
cylinder-32     0.1481   0.1481   0.1638   0.1621   0.1579   0.1976   vy_rms
                  0.10     0.10     0.12     0.19     0.27     0.69   penet_mm
                  37.7     37.7     42.9     40.5     37.5     54.9   churn_%
sfera           0.5060   0.5060   0.3830   0.3615   0.3615   0.3220   vy_rms
                 13.49    13.49    12.97    15.22    21.85    25.52   penet_mm
```

Zmiękczanie kontaktu **pogarsza** cylinder i nie rusza churnu. Mechanizm jest
teraz jasny: podatność kontaktu działa **przez dopuszczenie penetracji**, a
płaska faseta leżąca na płaskim gruncie przenosi obciążenie przy penetracji
rzędu 0.1 mm — dźwignia nigdy się nie załącza. Tarka fasety jest **wymuszeniem
kinematycznym** (zmiana wysokości podparcia), a nie siłowym. Filtr działający na
penetrację nie filtruje wymuszenia kinematycznego.

Sfera reaguje zgodnie z oczekiwaniem (13.5 → 25.5 mm ugięcia), co potwierdza,
że mechanizm sam w sobie działa — po prostu nie na ten problem.

> **Obalone:** globalny sweep **stockowego `contactHertz`** jako prosty sposób
> naprawy fasetowanych hulli w teście constant-downforce (płaski grunt pudełkowy,
> hulle 32/42, ~1900 N, hertz ≥ 6 Hz, zmienna prędkość).
> **NIE obalone i wciąż otwarte:** podatność właściwa oponie, nieliniowa krzywa
> siła–ugięcie, sztywność zależna od ciśnienia, elastyczny pierścień, model
> obejmujący nierówność (*enveloping*), własny więz normalny opony,
> wielopunktowy więz opony, pełna struktura, hybrydy.
>
> Zdanie „penetracja **jest** tu ugięciem" było zbyt mocne i zostaje wycofane.
> Penetracja sztywnych brył jest stanem numerycznego więzu. Wolno jej użyć jako
> proxy ugięcia dopiero wtedy, gdy prawo siła–penetracja odpowiada oponie,
> stan jest kontrolowany i raportowany, geometria i CCD znoszą tę głębokość,
> a energia i histereza się zgadzają. Podobna liczba milimetrów nie czyni
> mechanizmu modelem karkasu.

### 7.7 F-13 — laboratorium profilu (czysta geometria, bez silnika)

`MATH FACT`. Wszystkie kandydatki znormalizowane do R = 0.5141 m, W = 0.4375 m.

**Kolumna „korona" wymaga poprawnej nazwy.** Wydruk `flat b=Nmm` NIE mierzy
szerokości płaskiego pasa. Mierzy **przesunięcie punktu wsparcia przy zmianie
normalnej o 1°** (`wheel_bench.c:894–908`). Poprawna nazwa metryki:
`support_point_shift_0_to_1deg`.

```
profil               korona: rho lub shift_0->1deg   obwod        polowa szer.
sfera                rho = 0.5141 m                  rho = 0.5141   0.5141 (+295 mm)
elipsoida            rho = 0.0931 m                  rho = 0.5141   0.2188
swept-disk r=80mm    plaska + shift 140 mm           rho = 0.5141   0.2188
revolved Lame p=4    prawie plaska + shift  43 mm    rho = 0.5141   0.2188
revolved Lame p=8    prawie plaska + shift 109 mm    rho = 0.5141   0.2188
prism-42 hull        plaska + shift 219 mm           NAROZNIK       0.2188
profil opony-18      plaska + shift  73 mm           NAROZNIK       0.2188
```

Pięć odczytów, których wcześniej nie mieliśmy:

1. **`rho` korony elipsoidy = 0.0931 m** — potwierdzenie liczbowe wzoru
   `(W/2)²/R`, zero swobodnych parametrów profilu.
   *(Porównanie z realnymi oponami — patrz uwaga o źródłach niżej.)*
2. **Każda bryła obrotowa ma `rho` obwodu dokładnie równe R**; każdy hull ma
   w tym miejscu **narożnik** (krzywizna zero). To jest F-11 wyrażone geometrią.
3. **Rodzina revolved Lamé nie ma członka o koronie z realnym `rho`.** Dla
   `p = 2` korona ma `rho = 0.093`; dla każdego `p > 2` krzywizna **w wierzchołku**
   spada do zera. Uwaga na nazwę: Lamé dla `p>2` **nie ma dosłownie płaskiego
   odcinka** — jest gładką krzywą o zerowej krzywiźnie dokładnie w apeksie
   i bardzo płaską wokół niego. Przejście jest nieciągłe: nie da się
   „podkręcić p", żeby dostać pośrednią koronę.
4. **Wysokość jazdy przy camberze rośnie** dla każdego profilu z płaską koroną:
   prism-42 0.5141 → 0.5579 przy 20°, swept-disk → 0.5354, Lamé p=8 → 0.5329.
   Płaska korona sprawia, że **pochylone koło robi się większe**, bo skraj barku
   leży dalej od osi niż środek korony. To jest solidna obserwacja geometryczna.
   Zdanie „realna opona zachowuje się odwrotnie" zostaje **wycofane jako
   niepodparte** — realna opona się deformuje, zmienia patch z obciążeniem
   i zależy od konstrukcji karkasu.
5. **Swept-disk: 140 mm to POŁOWA płaskiego pasa.** Dla `r = 80 mm` mamy
   `hc = W/2 − r = 138.75 mm`, więc pełna płaska korona ma **277.5 mm**.
   Ujednolicona konwencja dokumentów: podajemy `flat_full_width` i osobno
   `support_point_shift`. Nie mieszać.

**Sfera przy camberze — korekta wydruku.** Kod drukuje „`sphere: y_contact stays
0`" (linia 969). **To jest fałszywe** i przeczy jego własnej tabeli G.2
(0° → 0 mm, 2° → 17.9 mm, 90° → 514.1 mm). Prawdziwa własność sfery:
**wysokość jazdy pozostaje 0.5141 m przy każdej orientacji**, a lokalny punkt
podparcia obraca się razem z normalną. Problem sfery nie polega na nieruchomym
punkcie lokalnym, tylko na tym, że **geometria i wysokość nie odróżniają korony,
barku i boku**. Naprawa wydruku: `KOLA_05` §5.

> **Uwaga o źródłach (`NIEPODPARTE`).** Liczby „samochodowa korona 0.5–1.5 m",
> „motocyklowa 0.06–0.12 m", „slick = płaska" oraz `I/mR² ≈ 0.55–0.80` nie mają
> w tym repo źródła ani definicji pomiarowej i **nie są `MATH FACT`**. Do czasu
> zbudowania atlasu realnych profili (`KOLA_05` §6) traktować je jako roboczą
> intuicję, a nie jako wymaganie.

### 7.8 F-14 — asymetria kierunków (nowa rama problemu)

`STRONG INFERENCE`, wynika z zestawienia F-11 + F-13 + G.3.

Koło ma **dwa kierunki dyskretyzacji o skrajnie różnej cenie**:

```
WOKOL OBWODU        dyskretyzacja jest bardzo kosztowna
                    - strata energii 4pi^2 (mR^2/I_c) / N na obrot  (F-11)
                    - churn tozsamosci kontaktu rosnie liniowo z N   (F-10)
                    - nie istnieje N, ktore spelnia oba naraz

W POPRZEK BIEZNIKA  dyskretyzacja jest prawdopodobnie ZNACZNIE tansza
                    - kolo nigdy nie toczy sie w tym kierunku
                    - koszt to rozdzielczosc cambera: punkt styku
                      przeskakuje miedzy fasetami profilu przy pochyleniu
```

**Wielkość mierzalna, na której ta asymetria stoi — częstotliwość przejść
między cechami.** Wokół obwodu: `f = N · v / (2 pi R)`, czyli przy N = 32
i 13 m/s to **129 przejść na sekundę, bez przerwy, zawsze**. W poprzek: przejście
zachodzi tylko wtedy, gdy zmienia się camber, a jego tempo jest ograniczone
dynamiką zawieszenia — rząd jednostek Hz. Różnica jest rzędu **dwóch rzędów
wielkości**, i to jest twierdzenie, które wolno sprawdzić pomiarem.

**Zawężenie po krytyce (`niemal darmowa` było zbyt mocne).** Wizja właściciela
zakłada gwałtowne dachowania, lądowanie na boku, uderzenia kołem w ściany
i klinowanie w skałach. W tych scenach profil może zmieniać aktywną cechę
szybko i pod dużym obciążeniem. Poprawny zakres brzmi: *dyskretyzacja profilu
w poprzek jest prawdopodobnie dużo tańsza i łatwiejsza do kontrolowania niż
dyskretyzacja obwodu, ale nadal wymaga pomiaru przejść korona–bark–bok
w ekstremalnych orientacjach.*

Każda dotąd rozważana rodzina traktuje oba kierunki **tak samo**: hulle
dyskretyzują oba, a sfera / elipsoida / Lamé / swept-disk są analityczne w obu.
Nikt nie wykorzystał asymetrii.

Konsekwencja dla kandydatów: reprezentacja **analityczna wokół obwodu i
dyskretna w poprzek** ma funkcję wsparcia

```
h(d) = max_i ( a_i * |d_prostopadle| + b_i * (d . os) )     [ + r * |d| ]
```

gdzie `(a_i, b_i)` to wierzchołki wypukłego profilu 2D w półpłaszczyźnie
`(promień, pozycja wzdłuż osi)`. Rodzina zawiera jako przypadki szczególne:
cylinder (`k=2`, `r=0`), swept-disk (`k=2`, `r>0`), **wypukłą otoczkę profilu
toroidalnego** (`k=1`, `r>0`) oraz profil o koronie o dowolnym promieniu
(kilka wierzchołków na łuku korony).

> **Nie pisać „torus".** Prawdziwy torus ma dziurę i jest niewypukły. Funkcja
> wsparcia zbioru niewypukłego opisuje jego otoczkę wypukłą, więc `k=1` daje
> zewnętrzną otoczkę, a nie relację felga–opona.

**Koszt wsparcia to NIE koszt bryły.** Twierdzenie „6–10 iloczynów skalarnych
+ pierwiastek jest tańsze niż hull-42" dotyczy wyłącznie **pojedynczego
zapytania o funkcję wsparcia**. Box3D ma dla hulli SAT, cache, klipowanie cech,
generator manifoldu, ścieżki SIMD i wyspecjalizowany kontakt z meshem. Nowa
bryła support-mapped może wymagać GJK, procedury penetracji, własnego
generatora manifoldu, stabilnych `featureId`, nowych castów, CCD i nowej
ścieżki solvera. **Tania funkcja wsparcia może dać droższy całkowity kontakt.**
Status: `SUPPORT QUERY COST: PROMISING`, `WHOLE SHAPE COST: UNKNOWN`.

**To jest hipoteza konstrukcyjna, nie wybrany kierunek.** Nie sprawdzono:
manifoldu, tożsamości cech, warm startu, kontaktu z trójkątem, castów, CCD,
kosztu forka, ani właściciela oporu toczenia. Patrz `KOLA_04` — iteracje R3/R5.

Do porównania w R3 wchodzi **co najmniej dziewięć** reprezentacji profilu, nie
tylko wielokąt: swept-disk / Lamé / wypukły wielokąt obrotowy / zaokrąglony
wielokąt / łuki odcinkowe / ściśle wypukły splajn / próbkowana funkcja wsparcia
/ prosty wyspecjalizowany kształt koła / ogólny support-mapped convex.
Wielokąt + jeden globalny promień zaokrągla narożniki, ale **zachowuje płaskie
odcinki** — jeśli wymagamy płynnego kontaktu przy małym camberze, profil
ściśle wypukły albo jawnie wielopunktowy manifold może być ważniejszy niż
liczba wierzchołków.

### 7.9 Znane wady stendu v2 (wejście do v2.1)

Wykryte audytem zewnętrznym 2026-07-27 i **potwierdzone w kodzie**:

| # | Wada | Miejsce | Skutek |
|---|---|---|---|
| W-1 | 2 s rozbiegu przed 4 s pomiaru; brak stanu na starcie okna | `:615`, `:622` | wszystkie metryki toczenia liczone na kole, które już prawie stoi |
| W-2 | „realny narożnik" = wolne koło + prasa; pozioma bezwładność 44 kg | `:611–613` | brak transferu na pojazd |
| W-3 | grawitacja świata −10, siła liczona z 9.81 | `:613` vs `src/types.c:16` | nacisk 1908 N, nie 1900 N |
| W-4 | `allowFastRotation` w `RunRoll`, brak w `TimeFreshWorld` | `:603` vs `:436` | koszt i jakość mierzone przy innych limitach |
| W-5 | `static b3ContactData cd[128]` bez sprawdzenia pojemności | `:289` | ciche obcięcie telemetrii na gęstym meshu |
| W-6 | `totalImpulse` i `manifoldsAvg` liczone, ale niedrukowane | `:305`, `:653` | brak danych do rozdzielenia utraty kontaktu od skoku impulsu |
| W-7 | E i F używają **gruntu pudełkowego**, choć problem produktowy to mesh | `:593` | wynik nie dotyczy terenu ze skanu |
| W-8 | `QueryPerformanceFrequency` przy każdym `NowMs()` | `:64` | zanieczyszczenie baseline n=0 |
| W-9 | brak sweepu fazy początkowej wielokąta, tarcia, prędkości, substepów | — | pojedynczy przebieg deterministyczny myli się z prawem |
| W-10 | metryki nienormalizowane na metr i na obrót | — | warianty o różnym dystansie nieporównywalne |
| W-11 | `flat b=` i „`y_contact stays 0`" — błędne nazwy/twierdzenia w wydruku | `:907`, `:969` | dokumentacja odziedziczyła błąd |
| W-12 | brak manifestu dowodowego (SHA-256, flagi, kod wyjścia, surowe próbki) | `build.bat` | przebieg nie jest samopoświadczający |
| W-13 | sweep F ma **6 etykiet, ale 5 ustawień**: 60 Hz i 30 Hz to ten sam efektywny 30 Hz | `src/physics_world.c:1114` | „identyczne wyniki" opisane jako zjawisko, były skutkiem kodu (`P-15`) |
| W-14 | scena opisana jako „tarcie 1.2" ma tarcie efektywne **0.849** (`sqrt(1.2·0.6)`, koło zostało na domyślnym 0.6) | `src/physics_world.c:154` | wszystkie liczby toczenia dotyczą innego tarcia niż deklarowane (`P-16`) |
| W-15 | `rollingResistance` opisany jako „tylko sfera/kapsuła" za komentarzem nagłówka silnika; hull dostaje `0.25·innerRadius` | `src/contact.c:646–684` | opór toczenia zmienia się ~4× przy zmianie bryły, niewidocznie (`P-14`) |
| W-16 | tabela CPU w §7.2 nie odpowiadała **żadnemu** zachowanemu przebiegowi | §7.2 | zerwany łańcuch dowodowy — naprawiony blokiem generowanym 2026-07-29 |

W-13…W-16 znalezione 2026-07-29. **W-14 i W-15 nie są wadami stendu — są
wadami tego dokumentu**: opisywały silnik z jego komentarzy zamiast z jego kodu.

Protokół naprawy: `KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md`. **Do czasu jego
wykonania żaden wynik §7 nie awansuje na `PRAWO` ani nie zamyka rodziny.**

## 8. Rozdzielczość kontaktu i granica ciągłego styku (2026-07-31)

Trzy ustalenia z sesji pracy nad Wheel Scope. Dwa pierwsze są odczytem kodu
silnika, trzeci wyprowadzeniem. **Żadne z nich nie jest wynikiem pomiarowym**
i żadne nie zamyka pytania o geometrię.

### 8.1 F-15 — narrowphase wykonuje się RAZ na krok świata

`ENGINE FACT`. W `b3World_Step` wykrywanie kontaktu (`b3Collide`) stoi **poza**
pętlą podkroków — `src/physics_world.c:1125`. Pętla podkroków żyje wewnątrz
`b3Solve` (`:1133`) i **przesolwowuje ten sam manifold**, którego już nie
odświeża.

Konsekwencja dla programu kół: rozdzielczość kontaktu w czasie wynosi `dt`,
a nie `dt/podkroki`. Dla bryły fasetowanej sensowną miarą jest **liczba ścianek
mijających punkt styku w jednym kroku**, `|v|·dt / (2πR/N)`. Gdy przekracza 1,
między dwiema aktualizacjami manifoldu mija cała ścianka i **zwiększanie liczby
podkroków tego nie naprawia**. Wskaźnik jest liczony w oknie na żywo
(`JozzRig_FacetsPerStep`) i pokazywany na pasku reżimu.

### 8.2 F-16 — liczba podkroków zmienia TWARDOŚĆ kontaktu, nie tylko dokładność

`ENGINE FACT`. `src/physics_world.c:1114`:

```c
float contactHertz = b3MinFloat( world->contactHertz, 0.125f * context.inv_h );
```

gdzie `context.inv_h = subStepCount * inv_dt` (`:1101`), a domyślne
`def.contactHertz = 30.0f` (`src/types.c:20`).

Przy kontrakcie Q2A (`dt = 1/60`, 4 podkroki) `0.125·inv_h` daje dokładnie
30 Hz — ograniczenie wypada **na samej granicy** wartości domyślnej. Przy
2 podkrokach schodzi do 15 Hz, czyli kontakt staje się dwa razy bardziej
miękki. Podkroki nie są więc czystym pokrętłem dokładności: **zmieniają model
kontaktu**. Każde porównanie geometrii przy różnej liczbie podkroków jest z tego
powodu porównaniem dwóch różnych materiałów styku.

### 8.3 P-17 — prędkość graniczna ciągłego styku sztywnego wielokąta

`MODEL FACT`. Aby wielokąt obtoczył się wokół wierzchołka bez utraty kontaktu,
środek masy musi zostać ściągnięty po łuku o promieniu `R`, co wymaga
przyspieszenia dośrodkowego `v²/R`. Dostępne jest `a_eff = load/m`. Stąd

```
v_kryt = sqrt( (load/m) · R )
```

**Wielkość nie zależy od liczby ścianek.** Dla kontraktu Q2A
(`load = 1900 N`, `m = 44 kg`, `R = 0.5141 m`) wychodzi 4.71 m/s, przy celu
13 m/s — czyli **2.76× powyżej granicy**. Domyślny przebieg badawczy odbywa się
więc w reżimie, w którym sztywne koło z założenia nie utrzymuje ciągłego styku,
i przez cały czas trwania programu **nic tego nie sygnalizowało**.

Zakres: model swobodnego ciała z siłą przyłożoną w środku masy. Rig Q2A to
założenie **spełnia**. Koło na zawieszeniu go **nie spełnia** — `b3WheelJoint`
wnosi własną siłę pionową o własnej dynamice. `P-17` nie jest więc granicą
produktu, tylko granicą **tego rigu**, i tak ma być cytowane.

## 9. Katalog rejestru findingow (generowany)

<!-- FINDINGS_CATALOG:BEGIN -->

> Ta sekcja jest generowana z `KOLA_FINDINGS.json`. Status i treść
> zmieniają się w rejestrze, a następnie odtwarza poleceniem
> `python tools/docs_audit.py --fix-findings`.

- **F-01** · `ENGINE_FACT` — Format hulla: 6N polkrawedzi <= 255 -> pryzmat max 42 scianki, profil 4-pierscieniowy max 18.
- **F-02** · `ENGINE_FACT` — Masa i bezwladnosc pochodza z objetosci collidera, wiec zmieniaja sie 1.58-1.87x miedzy obwiedniami.
- **F-03** · `BENCH_PRELIMINARY` — Model 'wiecej scianek = gladziej' jest falszywy: prism-42 toczy sie gorzej niz cylinder-32.
- **F-04** · `OWNER_DECISION` — Split envelope wedlug kategorii powierzchni odpada produktowo.
- **F-05** · `BENCH_PRELIMINARY` — Metryki nie zgadzaja sie ze soba; ranking wariantow zalezy od wybranej metryki.
- **F-06** · `BENCH_PRELIMINARY` — Ranking reprezentacji zmienia sie miedzy stendem a pojazdem.
- **F-07** · `SUPERSEDED` — Liczba scianek nie kosztuje nic; kosztuje liczba shape'ow.
- **F-08** · `BENCH_PRELIMINARY` — Koszt kranowy jednego kola: sfera 1.13 us (box) / 2.12 us (mesh); najdrozszy phased union-4 3.88 / 12.24 us.
- **F-09** · `BENCH_PRELIMINARY` — Pod stalym dociskiem 1900 N ranking wariantow odwraca sie wzgledem 440 N.
- **F-10** · `MODEL_INFERENCE` — Churn tozsamosci kontaktu jest jednym z mechanizmow agitacji fasetowanego kola.
- **F-11** · `MODEL_FACT` — Strata energii toczacego sie wielokata ~ 4pi^2 (mR^2/I_c)/N na obrot, I_c = I_cm + mR^2. Dla N=32: 1.13% omegi i 2.25% energii na wierzcholek.
- **F-12** · `BENCH_PRELIMINARY` — Globalny sweep stockowego contactHertz nie naprawia fasetowanych hulli w tym rigu.
- **F-13** · `MATH_FACT` — Laboratorium profilu: krzywizna obwodu, camber, elipsoida, Lame, swept-disk - czysta geometria bez silnika.
- **F-14** · `DESIGN_INFERENCE` — Asymetria kierunkow: dyskretyzacja w poprzek biezni jest tansza niz wzdluz obwodu.
- **F-15** · `ENGINE_FACT` — b3Collide wykonuje sie RAZ na b3World_Step, poza petla podkrokow; podkroki dzialaja wewnatrz b3Solve i przesolwowuja ten sam manifold. Rozdzielczosc kontaktu w czasie to dt, nie dt/podkroki.
- **F-16** · `ENGINE_FACT` — contactHertz = min(world->contactHertz, 0.125*inv_h), gdzie inv_h = podkroki/dt. Przy domyslnym contactHertz=30, dt=1/60 i 4 podkrokach ograniczenie wypada dokladnie na 30 Hz; przy 2 podkrokach schodzi do 15 Hz. Liczba podkrokow zmienia TWARDOSC kontaktu, nie tylko dokladnosc solvera.
- **F-18** · `ENGINE_FACT` — b3WheelJoint: `suspensionHertz` jest czestotliwoscia wlasna MASY ZREDUKOWANEJ wiezu, nie masy resorowanej. Zmierzona sztywnosc = m_red*(2pi*f)^2 co do 5 cyfr znaczacych dla f w [0.5, 6] Hz. Rzeczywista czestotliwosc resorowania = hertz * sqrt(m_red/m_sprung); dla 150+44 kg to hertz/2.0998, czyli ustawienie 6 Hz daje resor 2.86 Hz.
- **F-19** · `ENGINE_FACT` — Sztywnosc statyczna zawieszenia jest NIEZALEZNA od liczby podkrokow: 1/2/4/8 podkrokow daje 3021.9 N/m identycznie do 5 cyfr. Czestotliwosc wlasna zmienia sie o 1.9% (0.689 -> 0.702 Hz).
- **F-20** · `BENCH_PRELIMINARY` — Silnik obrotu b3WheelJoint przy nasyceniu dostarcza 2.018x zadanego `maxSpinTorque` (4 podkroki; 1.996x przy 1, 2.022x przy 8). Zaleznosc od tau idealnie liniowa (100/500/1000 N*m daja ten sam iloraz). Bezwladnosc obrotowa nadwozia NIE wplywa na wynik.
- **F-21** · `BENCH_PRELIMINARY` — Nowa obwiednia `torus-N` (pierscien N kapsul o osi rownoleglej do osi kola) bije pryzmat w Q3 przy 4.0 m/s na plaskiej plycie. torus-64 vs prism-42 (dzisiejsze kolo produktu): moc strat 425.7 vs 638.7 W (-33.3%), sprung_accel_rms 0.055 vs 0.332 m/s2 (-83.4%), contact_churn 0.0% vs 89.7%. Polowa rozrzutu z 3 powtorzen: 0.00 dla obu kandydatow.
- **F-22** · `BENCH_PRELIMINARY` — Tetnienie promienia obwiedni NIE rzadzi jakoscia toczenia. Przy TEJ SAMEJ liczbie elementow (N=32) torus ma o 57% WIEKSZE tetnienie niz pryzmat (3.896 vs 2.476 mm), a mimo to o 42% mniejsza moc strat (523 vs 903 W), o 67% mniejsze sprung_accel_rms (0.177 vs 0.531) i o rzad wielkosci mniejszy churn (8.6% vs 70.1%). Roznica lezy w OSTRYCH KRAWEDZIACH, nie w amplitudzie odchylki promienia.
- **F-23** · `BENCH_PRELIMINARY` — Cena gladkosci jest w CPU, nie w pamieci ani w budzecie hulla: torus-64 kosztuje 0.094 ms na krok swiata wobec 0.007 ms dla pryzmatu (13x), torus-32 0.036 ms (5x). Pryzmat jest jednym ksztaltem, pierscien kapsul ma ich N.
- **F-24** · `BENCH_PRELIMINARY` — Przy 13 m/s pojedynczy przebieg Q3 nie jest powtarzalny na poziomie progu wazności. Zmiana wysokosci mocowania nadwozia o 0.35 m - geometrycznie NEUTRALNA dla tego wiezu (os pionowa, oba zaczepy na osi, zerowe ramie) - przerzucila `airborne_fraction` dla torus-32 z 6.0% na 10.8%, czyli przez prog 10% z par. 8 kontraktu.
- **F-25** · `BENCH_PRELIMINARY` — Promien korony torusa NIE jest kompromisem na plaskiej plycie. W zakresie 0.04-0.19 m (torus-64, 4.0 m/s) moc strat spada MONOTONICZNIE 795.2 -> 457.1 W (-43%), churn 26.9% -> 0.0%, sprung_accel_rms 0.076 -> 0.050 (-34%), a tetnienie promienia 8.031 -> 1.057 mm. Zwezenie plaskiej biezni z 358 do 58 mm nie kosztuje tu NIC mierzalnego. Cena jest gdzie indziej: koszt CPU rosnie 0.048 -> 0.082 ms/krok (1.7x), bo wieksze kapsuly daja wiecej punktow styku.
- **F-26** · `SUPERSEDED` — WYCOFANE 2026-08-03, zastapione przez F-27 i F-28. Twierdzenie brzmialo: "Q3 na plaskiej plycie NIE WIDZI profilu poprzecznego korony". Bylo oparte na przemiataniu liczby rzedow 1->9, ktore NIE ZMIENIALO BRYLY: przemiatanie ustawia wylacznie `crown_rows`, a zwis zostal wtedy domyslny, czyli ZEROWY - przy zwisie 0 wszystkie rzedy leza na jednym promieniu, wiec ich unia to dokladnie ta sama kapsula co przy jednym rzedzie. Mierzono piec razy te sama opone o rosnacym koszcie CPU. Opis findingu podawal przy tym "zwis 0.08 m", ktorego zaden z tych przebiegow nie mial - liczba zostala wziete z kontekstu pracy, a nie z naglowka przebiegu. Druga polowa (przemiatanie zwisu) byla prawdziwym przemiataniem ksztaltu, ale na drodze JEDNORODNEJ W POPRZEK, gdzie zaden profil poprzeczny nie ma jak zadzialac.
- **F-27** · `BENCH_PRELIMINARY` — Sama LICZBA KSZTALTOW przesuwa wynik Q3, przy IDENTYCZNEJ obwiedni. torus-64, crown_r 0.05, zwis 0, plaska plyta, 4 m/s, rzedy 1/3/5/7/9: wszystkie piec punktow ma ten sam odcisk obwiedni (5f587854) i to samo tetnienie 6.046 mm, a mimo to strata idzie MONOTONICZNIE 775.0 / 797.6 / 798.8 / 802.7 / 816.1 W (+5.3%) razem ze srednia liczba punktow kontaktu niosacych obciazenie 2.84 / 6.51 / 10.95 / 14.79 / 18.55. Koszt CPU 0.050 -> 1.176 ms/krok (24x). Rozrzut miedzy 3 powtorzeniami zerowy w kazdym punkcie.
- **F-28** · `BENCH_PRELIMINARY` — Q3 WIDZI profil poprzeczny opony, gdy droga przestaje byc jednorodna w poprzek. Ta sama bryla (odcisk 1ee68a41: torus-64, crown_r 0.05, 5 rzedow, zwis 0.08 m), grzebien waskich kamieni 30 mm o polowie szerokosci 40 mm, przesuwanych w bok z 0 na 0.20 m: a_rms 1.548 / 1.548 / 1.553 / 1.112 / 0.079 m/s2 i strata 974.0 / 977.9 / 968.8 / 916.9 / 789.4 W. Zmienia sie WYLACZNIE polozenie kamienia pod bieznia. Przy progu na cala szerokosc plyty (dotychczasowa droga Q3) profil poprzeczny nie ma jak zadzialac - i to bylo prawdziwa przyczyna wyniku wycofanego jako F-26.
- **F-29** · `BENCH_PRELIMINARY` — Podzial pasm korony ROWNY PO ZWISIE bije podzial rowny po szerokosci przy tym samym koszcie: przy 9 rzedach i zwisie 0.08 m odchylka zbudowanego przekroju od zamowionego luku spada 18.1 -> 9.0 mm, a przebieg fizyczny przestaje skakac (strata przy 3/5/7/9 rzedach: 973.8 / 916.9 / 879.3 / 869.9 W monotonicznie, zamiast 788.9 / 895.6 / 845.5 / 878.5 W). Jednoczesnie ZWIS przy stalej liczbie rzedow NIE jest monotonicznym pokretlem miekkosci: na kamieniu lezacym tuz za srodkiem biezni (z 0.15 m) a_rms idzie 1.541 / 0.956 / 1.112 / 1.312 / 1.489 m/s2 dla zwisu 0 / 0.04 / 0.08 / 0.12 / 0.16 m. Kontrola przy 2.5x mniejszym promieniu barku (crown_r 0.02, N=96) daje ten sam ksztalt krzywej: 1.491 / 0.912 / 1.224 / 1.439 / 1.541.
- **F-30** · `DESIGN_INFERENCE` — Rozdzielczosc POPRZECZNA obwiedni `torus-N` jest ograniczona przez promien barku, nie przez liczbe rzedow. Czasza kapsuly o promieniu crownR trzyma sie w granicy h od pelnego promienia az do sqrt(2*crownR*h) w bok: dla crownR 50 mm i h 30 mm to 55 mm, dla crownR 20 mm - 35 mm. Zeby rozroznic w poprzek przeszkode o wysokosci h, promien barku musi byc znaczaco mniejszy niz h. Szczelnosc pierscienia wymaga wtedy N >= pi/asin(crownR/ringR): przy crownR 10 mm stend podaje minimum 159 kapsul, czyli z 9 rzedami 1431 ksztaltow na jedno kolo.
- **F-31** · `BENCH_PRELIMINARY` — Sztywna bryla w tym rigu NIE daje odcisku: caly ciezar stoi na 1-3 punktach efektywnych, a rekord calej rodziny to ~5. Miara: `nios` = (suma p)^2 / suma p^2 po impulsach normalnych punktow, `max%` = udzial najwiekszego punktu. Kontrola miary przechodzi - `sphere` daje dokladnie nios 1.00 / max 100.0%, czyli tyle, ile ma z geometrii. Na plycie przy 4 podkrokach: prism-Nmax nios 1.66 / max 71.4%, prism-32 1.53 / 76.8%, torus-32 1.73 / 72.1%, torus-64 2.03 / 61.3%. Przy 13 m/s torus-32 przekracza prog degeneracji falsyfikatora R1 (nios 1.19, max 92.2%). Liczba punktow i liczba podkrokow dzialaja MULTIPLIKATYWNIE, obie sa konieczne: 64 ksztalty / 4 podkroki -> nios 1.56; 576 ksztaltow / 4 podkroki -> 2.87; 64 ksztalty / 32 podkroki -> 2.83; 576 ksztaltow / 32 podkroki -> 5.23 (max% odpowiednio 76.3 / 55.8 / 52.8 / 39.0).
- **F-32** · `BENCH_PRELIMINARY` — Podloga szumu z F-27 (sama liczba ksztaltow przesuwa strate o 5.3% przy identycznej obwiedni) jest efektem NIEDOZBIEZNOSCI solvera, a nie wlasnoscia liczby ksztaltow. Ten sam kontrast 64 vs 576 ksztaltow na tej samej obwiedni: przy 4 podkrokach 775.0 -> 816.1 W (5.3%), przy 32 podkrokach 636.0 -> 634.4 W (0.25%, czyli ponizej progu rozroznialnosci). Rownolegle rosnie rozklad obciazenia: nios 1.56 -> 2.87 przy 4 podkrokach i 2.83 -> 5.23 przy 32, wiec dodatkowe ksztalty naprawde przejmuja czesc nacisku, tylko przy 4 podkrokach solver nie zdazy tego rozdzielic i placi za to strata.
- **F-33** · `BENCH_PRELIMINARY` — Styk z ziemia na PRAWDZIWEJ SZEROKOSCI wzbudza szarpanie ukladu kierowniczego w POJEZDZIE - i nie jest to wina liczby ksztaltow. Walidator produktowy, ta sama konfiguracja, zmieniana wylacznie obwiednia kola: sfera 0/18 czerwonych i 0.31 st oscylacji po puszczeniu kierownicy, mieszana sfera+walec (domyslna) 0/18 i 0.31 st, torus-64 3/18 i 4.66 st, walec 8/18 i 6.71 st. JEDEN ksztalt (walec) wypada gorzej niz piecdziesiat kilka (pierscien kapsul). Amplituda idzie za szerokoscia plaskiej biezni: 337 / 238 / 137 / 38 mm biezni daje 6 / 6 / 5 / 3 czerwonych sond. Obwiednia domyslna dotyka terenu SFERA, czyli jednym punktem na plaszczyznie symetrii kola.
- **F-34** · `BENCH_PRELIMINARY` — Koszt CPU pierscienia kapsul w PELNYM pojezdzie na terenie MESHOWYM jest maly w liczbach bezwzglednych. Cztery kola, mapa produktu, jazda pod JOZZ_M6_AUTODRIVE, Release /O2, mediana z 3 przebiegow po 600 krokach, ms na krok fizyki: sama sfera 0.099, mieszana (domyslna) 0.105, opona 16 kapsul 0.143, opona 32 kapsuly 0.197, opona 64 kapsuly 0.403. Liczba kontaktow w swiecie rosnie 4 -> 15 -> 28 -> 60. Wzgledem dzisiejszego kola to 1.4x / 1.9x / 3.8x, ale najdrozszy wariant bierze 2.4% budzetu klatki 16.7 ms.
- **F-35** · `VEHICLE_MEASURED` — Domyslne kolo produktu jest w jezdzie fizycznie SFERA. Sonda jazdy porownala obwiednie SPHERE i SPLIT_SPHERE_SIDEWALL w dziesieciu warunkach (3 predkosci na wprost + zakret, oraz sweep podkrokow) i WSZYSTKIE liczby sa identyczne co do ostatniej cyfry: a_rms 0.018 / 0.061 / 0.088 na wprost, 0.861 w zakrecie, pkt/kolo 1.00, swieze 0.0%. Boczny walec obwiedni dzielonej nigdy nie wchodzi w kontakt z gruntem.
- **F-36** · `VEHICLE_MEASURED` — Na IDEALNIE PLASKIEJ plycie, przy predkosci, kazda obwiednia o powierzchni innej niz kulista szarpie 25-40x mocniej od sfery, i NIE zalezy to od liczby ksztaltow. a_rms przy 16 m/s na wprost: sfera 0.061, walec 1 ksztalt 2.434, union 3 warstwy 2.382, opona 16 kapsul 2.757, 32 kapsuly 2.233, 64 kapsuly 2.244. Roznica miedzy 1 a 64 ksztaltami (2.434 vs 2.244) jest mniejsza niz roznica miedzy dowolnym z nich a sfera (37x). Kolo traci styk z ziemia: pkt/kolo 0.35-1.31 wobec dokladnie 1.00 dla sfery. Zbior punktow styku jest budowany od nowa w kazdym kroku (swieze 100.0% wobec 0.0% dla sfery). W zakrecie dystans zostaje (sfera 0.861 wobec 3.6-4.7; a_max 2.7 wobec 11.2-16.8). Predkosc maksymalna: sfera 17.3 m/s, reszta scina sie na 12.4-14.0.
- **F-37** · `VEHICLE_MEASURED` — Podkroki solvera NIE lecza szarpania obwiedni fasetowanej - pogarszaja je. Przy 16 m/s na wprost, podkroki 4/8/16/32: sfera 0.061 -> 0.025 -> 0.009 -> 0.004 (monotonicznie w dol), opona-32 2.233 -> 2.095 -> 2.576 -> 3.233 (w gore). Ruch zawieszenia opony rosnie razem z tym: 9.79 -> 10.52 -> 13.06 -> 14.65 mm/krok.
- **F-38** · `ENGINE_FACT` — WHEEL-RIGID-01: kontakt b3Wheel z płaszczyzną ma ścisłą topologię supportu. Unikalny support vertex daje 1 punkt, rzeczywisty segment równoległy do płaszczyzny daje 2 końce; głębszy overlap nie dodaje próbek profilu, a B3_SPECULATIVE_DISTANCE wyłącznie bramkuje istnienie kontaktu.
- **F-39** · `VEHICLE_MEASURED` — Po usunięciu speculative-candidate bias wszystkie warianty crown mają 1.00 wszystkich punktów manifoldu/kolo i 1.00 punktów z dodatnim impulsem/kolo. Mimo tego 3 mm crown nadal obniża a_rms w zakręcie z 0.571 do 0.436 m/s2, podczas gdy na prostej podnosi je z 0.053 do 0.061 m/s2. Warianty 10 i 30 mm dają w tym protokole 0.471 w zakręcie i 0.061 na prostej.
- **F-40** · `ENGINE_FACT` — WHEEL-SEAM-02A: b3Wheel vs triangle jest jednostronny i nie gubi kontaktu tylko dlatego, ze strict face support wypada poza barycentre. W takim przypadku finite boundary fallback wybiera najglebsza rzeczywista krawedz lub wierzcholek, a feature pair rozroznia TriangleFace / Edge1-3 / Vertex1-3 oraz support profilu kola.
- **F-41** · `ENGINE_MEASURED` — WHEEL-SEAM-02A: obciazone kolo przechodzi przez wspolplaszczyznowy szew bez luki i z dokladnie jednym solver pointem w obu kierunkach oraz trzech fazach obrotu. Na zalamaniu ok. 1.15 stopnia dopuszczalne 1-2 punkty utrzymuja ciagla normalna i sume impulsu w oknie 20% w obu kierunkach i dwoch fazach. Wheel-only wybor normalnej najglebszego zaakceptowanego manifoldu usunal przed-poprawkowy, zalezny od fazy skok impulsu +36.4%.
- **F-42** · `ENGINE_FACT` — WHEEL-HULL-02B: b3Wheel vs hull nie używa już nieskończonej płaszczyzny ściany. Punkty face są clipowane do polygonu, pełna projekcja koła może uruchomić wyłącznie konserwatywny face-prism fast path, a pozostałe przypadki przechodzą deterministyczny feature walk z rozłącznymi face/edge/vertex feature IDs.
- **F-43** · `ENGINE_MEASURED` — WHEEL-HULL-02B: obciążone koło przechodzi face→edge→vertex bez luki w obu kierunkach i przy fazach 0.00/0.73 rad; każda topologia przenosi impuls >0.1 N*s, a kolejne normalne mają dot >0.85. Finalny audit 2000 orientowanych boxów i 60 nieortogonalnych hullów utrzymał separację nie gorszą o więcej niż 3 mm od gęstego szukania 8192 kierunków.
- **F-44** · `ENGINE_MEASURED` — WHEEL-SOFT-03 Q2: wheel-local contact Hertz działa jako czysty parametr podatności normalnej. Przy skalach 1.00/0.75/0.50/0.25 statyczna kompresja wynosi odpowiednio 0.155/0.276/0.622/2.488 mm, przy zachowanych 2 punktach, zerowym topology drift, zerowych lukach i persisted ratio 1.0.
- **F-45** · `ENGINE_MEASURED` — WHEEL-SOFT-03 Q2 nie rozstrzyga hipotezy komfortu. W kalibratorze z impulsem przyłożonym do koła niższy Hertz zwiększał zarówno kompresję, jak i RMS/peak przyspieszenia nadwozia; decyzja runu ma status INCONCLUSIVE i nie otwiera Q3.
- **P-01** · `ENGINE_FACT` — Limit formatu hulla.
- **P-02** · `ENGINE_FACT` — Masa z objetosci collidera.
- **P-03** · `MODEL_INFERENCE` — Churn rosnie z liczba scianek, amplituda tarki maleje jak 1/N^2.
- **P-04** · `MODEL_FACT` — Strata energii wielokata.
- **P-05** · `BENCH_PRELIMINARY` — Globalny contactHertz nie zastepuje geometrii.
- **P-06** · `MATH_FACT` — Bryla obrotowa ma krzywizne obwodu R; hull ma tam narozek.
- **P-07** · `MATH_FACT` — Elipsoida: rho korony = (W/2)^2/R = 0.093 m. Zero swobodnych parametrow profilu.
- **P-08** · `MATH_FACT` — Revolved Lame nie ma czlonka o koronie posredniej: p=2 daje rho=0.093, kazde p>2 daje krzywizne 0 w apeksie.
- **P-09** · `MATH_FACT` — Plaska korona powoduje, ze pochylone kolo robi sie WIEKSZE (prism-42: 0.514 -> 0.558 przy 20 st.).
- **P-10** · `DESIGN_INFERENCE` — Asymetria kierunkow.
- **P-11** · `ENGINE_FACT` — Tarcie liniowe jest CENTRALNE na manifold - jeden frictionImpulse niezaleznie od liczby punktow; b3ManifoldPoint NIE ma per-point tangentialImpulse.
- **P-12** · `ENGINE_FACT` — b3_compoundShape nie ma zarejestrowanej pary z mesh ani height.
- **P-13** · `ENGINE_FACT` — Telemetria manifoldu (featureId, persisted, impulsy) jest dostepna publicznym API; zaden patch core nie jest do tego potrzebny.
- **P-14** · `ENGINE_FACT` — Promien oporu toczenia: sphere -> sphere.radius, capsule -> capsule.radius, hull -> 0.25 * hull->innerRadius, pozostale -> 0.
- **P-15** · `ENGINE_FACT` — Efektywny contactHertz = min(zadany, 0.125 * inv_h), gdzie inv_h = subStepCount * inv_dt.
- **P-16** · `ENGINE_FACT` — Tarcie pary jest srednia geometryczna: sqrtf(frictionA * frictionB).
- **P-17** · `MODEL_FACT` — Predkosc graniczna ciaglego styku sztywnego wielokata: v_kryt = sqrt((load/m)*R), NIEZALEZNA od liczby scianek. Dla kontraktu Q2A 4.71 m/s przy celu 13 m/s, czyli 2.76x powyzej granicy.
- **P-18** · `ENGINE_FACT` — Manifold Box3D miesci NAJWYZEJ 4 punkty kontaktu (`B3_MAX_MANIFOLD_POINTS` = 4, include/box3d/constants.h:114; `b3Manifold.pointCount` jest udokumentowane jako 0..4). Tarcie liniowe (`frictionImpulse`), tarcie skretne (`twistImpulse`) i opor toczenia (`rollingImpulse`) sa polami MANIFOLDU, nie punktu - czyli jedna para ksztaltow ma jedna kotwice tarcia niezaleznie od tego, iloma punktami sie styka (to samo stwierdzenie co P-11, tu z jawnym limitem punktow).

<!-- FINDINGS_CATALOG:END -->
