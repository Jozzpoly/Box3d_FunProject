# M5.1 Feel Tuning — Raport implementacji

Data: 2026-07-05
Branch: `jozz-vehicle-sandbox-m0`
Status: Kroki 1-4 z planu zaimplementowane i zwalidowane headless + boot-smoke. Krok 5 (niestabilność przy prędkości) świadomie NIE ruszony — wymaga oczu Jozza na poprawionym buildzie.

Plan źródłowy: `docs/M5_1_FEEL_TUNING_HANDOFF_2026_07_05_PL.md`. Ten dokument zapisuje co faktycznie zrobiono i jakie dowody to potwierdzają.

## Metodologia: najpierw reprodukcja, potem naprawa

Zanim ruszono jakąkolwiek wartość konfiguracji, dodano do `RunM5DriveSmoke`
(w `jozz_vehicle_validation.cpp`) sub-test "steer while stationary" i
uruchomiono go na **niezmienionym** kodzie z poprzedniej sesji. Wynik:

```text
m5 stationary steer: left -0.0 deg, right -0.0 deg, target 32.0 deg
```

Koła w ogóle się nie ruszały (0.0° z 32° celu) przy `maxSteeringTorque=80`.
To twarde, liczbowe potwierdzenie hipotezy z handoffu (opór skrętu
nieruchomej opony o skończonej powierzchni styku, nie promień scrubu — patrz
sekcja 3.3 handoffu), zanim zdecydowano się na konkretną wartość naprawy.

## Co zostało zrobione

### Krok 1a — zwężenie chassis (potwierdzone liczbowo w handoffie)

`chassisHalfExtents.z`: 0.80 → **0.55** (m). Wcześniej wewnętrzna ściana opony
(≈0.83m) miała tylko 3cm prześwitu do krawędzi nadwozia; teraz ~0.28m.
Live-tunable w Kroku 3 poniżej (pending/Apply), więc Jozz może dalej
dostroić.

### Krok 1b — kamera i sterowanie A/D

**Ważne odkrycie zmieniające plan z handoffu:** matematycznie zweryfikowałem
znak skrętu przeciwko własnej konwencji kodu `right = up × forward`
([camera.cpp:52](../samples/host/camera.cpp)) i okazało się, że
`input.steer=+1` (klawisz A) faktycznie daje skręt w `+Z`, czyli w LEWO wg tej
konwencji — **znak w `UpdateJozzVehicleM5Drive` jest poprawny**, nie zmieniono
go.

Zamiast tego zidentyfikowano bardziej prawdopodobną przyczynę: domyślna
kamera (`SetView(38°, 16°, 15°, ...)`) patrzyła na auto niemal od przodu
(oko kamery leżało po tej samej stronie +X co kierunek jazdy pojazdu), co
lustrzanie odwraca poczucie lewo/prawo względem kierowcy — dokładnie jak
patrzenie na nadjeżdżający samochód z chodnika. Zmieniono domyślną kamerę na
właściwy chase-cam zza pojazdu (`SetView(-135°, 14°, 13°, ...)`) — auto
oddala się od kamery, jak w typowej grze wyścigowej. Dodano też jawny reset
`m_camera->m_thirdPerson = false` w konstruktorze (wcześniej nie był
ustawiany explicit, więc mógł dziedziczyć stan po poprzednim sample'u).

**Nie mogłem tego zweryfikować wzrokiem** (brak dostępu do podglądu
renderowanego okna w tej sesji) — dlatego dodano checkbox **"Invert steering
(if A/D feels backwards)"** jako natychmiastowy wentyl bezpieczeństwa, gdyby
poprawka kamery nie wystarczyła. Jozz: to pierwsza rzecz do sprawdzenia w
następnym ręcznym teście.

### Krok 3 — moment/sztywność skrętu (Opcja A z handoffu, potwierdzona empirycznie)

```text
maxSteeringTorque: 80 -> 700 N·m
steeringHertz:     8 -> 14 Hz
```

Po zmianie: `left -30.7°, right -30.7°` z celu 32° w 1.5s, symetrycznie w
granicach 0.1°. Reszta smoke testu (osiadanie, jazda prosta, jazda+skręt,
hamowanie) nadal przechodzi bez regresji.

Dodano też suwaki `Steering hertz` / `Steering damping ratio` do UI — istniały
w configu i były aplikowane przez `ApplySteeringTuning()`, ale nie miały
kontrolek (luka z poprzedniej sesji, teraz zamknięta).

### Krok 3 (dodatkowo) — pending/Apply dla geometrii + szersze zakresy

Przeniesiono `chassisHalfExtents`, `chassisDensity`, `axleHalfSpacing`,
`trackHalfWidth`, `restDrop`, `wheelDensity` na wzorzec pending/Apply
(`m_editX` + `m_structuralSetupDirty` + przycisk "Apply rig rebuild"),
kopiując sprawdzony wzorzec z Lab M2 Primitive Corner zamiast wymyślać nowy.
Promień/szerokość koła NIE są tu edytowalne — zostają asset-derived zgodnie z
M3A. Dodano "Reset rig to asset defaults".

Wszystkie wcześniej live-tunable suwaki poszerzone zgodnie z wyraźną prośbą
Jozza o stress-testy:

```text
drive torque      1500 -> 6000
drive speed         60 -> 150
brake torque      2000 -> 6000
suspension hertz    12 -> 30 (dolna granica 0.5 -> 0.2)
damping ratio        3 -> 6
steering angle      45 -> 60 deg (dolna 5 -> 1)
steering torque    300 -> 3000
```

### Krok 2 — poszerzony plac zabaw

Wydzielony do nowego modułu `samples/jozz_vehicle_m5_test_course.h/.cpp`
(zgodnie z lekcją "nie przeciążaj pliku sample'a" z
`PROJECT_STABILIZATION_AUDIT_2026_07_03_PL.md`, Problem A):

```text
grunt: half-extent 60 -> 120 (2x, zgodnie z prośbą)
rampy: 2 -> 4, rozstawione po większej mapie
tarka (washboard): 1 -> 2 pasy, różny rozstaw progów
strefa nierównego terenu: b3CreateWave (wzorzec ze stockowego Driving),
  umieszczona nad płaskim gruntem żeby nie dublować kontaktu
14 rozrzuconych dynamicznych propsów (skrzynie + kule, różne
  rozmiary/masy) + przycisk "Reset props"
```

## Czego świadomie NIE ruszono

**Krok 5 — niestabilność/"teleportowanie" kół przy prędkości.** Zgodnie z
planem z handoffu (sekcja 3.4), nie zgadywano naprawy bez diagnostyki. Ważne
odkrycie z tej sesji: panel "Solver" z suwakiem Sub-steps (1-50) i Hertz
(5-240) jest **już dostępny domyślnie** w obu samplach Jozza
(`HasSolverControls()` zwraca `true` z klasy bazowej, nikt tego nie
nadpisał) — nie trzeba nic dodawać, żeby zacząć diagnozować. Rekomendowany
pierwszy krok dla Jozza: podnieść Sub-steps (np. do 8-16) i sprawdzić czy
efekt znika — jeśli tak, to problem konwergencji solvera pod dużym
obciążeniem; jeśli nie, bardziej prawdopodobny jest artefakt
renderowania/interpolacji. Użyć wbudowanego nagrywania/replay do złapania
deterministycznego repro przed dalszym dochodzeniem.

## Walidacja wykonana w tej sesji

```text
jozz_vehicle_validation.exe -> m5 stationary steer OK, cała reszta OK
test.exe -> All Box3D tests passed (11.2s)
samples.exe --sample 95 --frames 300 -> 0 sokol errors (Lab M2, indeks bez zmian)
samples.exe --sample 96 --frames 300 -> 0 sokol errors (M5, indeks bez zmian)
```

Commity: `6b5f7d0` (naprawa torque + reprodukcja w smoke), `b4dce4f` (UX/tor/pending-apply).

## Następny krok dla Jozza

Ręczny test skupiony na:
```text
czy A/D teraz czuje się poprawnie z nową kamerą (jeśli nie: checkbox "Invert steering")
czy skręt na postoju/niskiej prędkości jest teraz OK (był 0 -> powinien być pełny)
czy oba przednie koła skręcają symetrycznie
czy chassis wygląda lepiej proporcjonalnie (można dalej zwężać suwakiem "Chassis half width")
zabawa na większym terenie/propsach
PRÓBA DIAGNOSTYCZNA: podnieść Sub-steps w panelu Solver przy dużej prędkości - czy "teleportowanie" znika?
```
