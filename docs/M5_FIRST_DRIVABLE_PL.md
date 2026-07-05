# M5 First Drivable — pierwszy jeżdżący pojazd

Data: 2026-07-05  
Branch: `jozz-vehicle-sandbox-m0`  
Status: zaimplementowane; walidacja headless zielona; boot smoke zielony; czeka na ręczny smoke Jozza

## Po co M5 i dlaczego przed M4C

ADR-0003 mówi wprost: pierwszym kryterium sukcesu projektu jest *feel and
controllability*, nie mechaniczna kompletność. Od M3B.2 do M4B projekt budował
kolejne wizualne proofy przy statycznym chassis — najbardziej ryzykowne pytanie
projektu („czy `b3WheelJoint` daje dobry feel przy dynamicznym nadwoziu z
realną masą?") pozostawało bez odpowiedzi.

M5 odpowiada na to pytanie minimalnym kosztem: dynamiczne chassis + 4 rogi na
zwalidowanym modelu M2.4/M2.5 + steering natywny z silnika. M4C (proceduralny
damper/cardan visual) nie jest skasowane — jest odroczone i po M5 będzie mogło
celować w jeżdżący pojazd zamiast statycznego rigu. Decyzja zapisana w
ADR-0005.

## Co powstało

### 1. Moduł fizyki pojazdu (warstwa physics prefab)

```text
samples/jozz_vehicle_m5_vehicle.h
samples/jozz_vehicle_m5_vehicle.cpp
```

Pierwszy element warstwy *physics prefab* z `PROJECT_DIRECTION_PL.md`: bodies +
shapes + joints + tuning, zero zależności od renderera i inputu. Ten sam moduł
konsumują sample host i headless walidator.

Model fizyki (niezmienione święte reguły):

```text
b3WheelJoint spring rest = translation 0
Frame A = rest wheel-center anchor na chassis (chassis-local)
Frame B = centrum koła / origin body koła
Rest drop = jawny offset środek-chassis -> rest wheel center
Kolizja koła = wyśrodkowany prymitywny cylinder, nie mesh glTF
```

Układ: przód = +X, lewo = +Z. Rogi: FL/FR/RL/RR. Przednia oś ma steering
(enableSteering + limity kąta), spin motor jest na wszystkich rogach (hamulec
działa wszędzie), napęd AWD/RWD to flaga configu. Opcjonalny „upright assist"
to miękki `b3ParallelJoint` do groundu, zapożyczony ze stockowego sample'a
`Driving`; można go wyłączyć, żeby czuć surowe zachowanie.

Wymiary koła (promień/szerokość) przychodzą z zewnątrz — źródłem pozostają
asset-derived defaults M3A. Moduł ich nie wylicza i nie czyta plików.

### 2. Headless drive smoke w walidatorze

`jozz_vehicle_validation.exe` po dotychczasowych checkach metadanych i
kontraktu tworzy prawdziwy świat Box3D bez GUI i przegania pojazd przez
scenariusz: osiadanie → pełny gaz na wprost → skręt → hamowanie do zera.

Asercje:

```text
stan skończony (bez NaN) po każdej fazie
osiadanie < 80% skoku kompresji (sprężyny niosą chassis, nie limit)
jazda do przodu: dx > 4 m, tor prosty (|dz| < 0.5*|dx|), prędkość > 2 m/s
skręt zmienia heading o > 0.15 rad
hamulec zatrzymuje pojazd (|v| < 0.8 m/s)
chassis pozostaje pionowe w każdej fazie
```

### 3. Sample „Jozz Vehicle / M5 First Drivable"

```text
samples/jozz_vehicle_m5_drivable_lab.cpp / .h
rejestracja w samples/sample_jozz_vehicle_lab.cpp
```

Sterowanie: `W/S` gaz/wstecz, `A/D` skręt, `Space` hamulec, `T` kamera
trzecioosobowa (konwencja stockowego Driving), `R` restart (globalny).
Hotkeys zaudytowane w `docs/HOTKEY_AUDIT_PL.md` (A/D/T były na liście
kandydatów).

Panel: suwaki drive torque/speed/brake, spring hertz/damping (live na 4
jointy), max kąt skrętu i steering torque (live na przednie jointy), AWD/RWD,
upright assist (rebuild), reset pojazdu.

Wizualia: mesh `Offroad_Big_Wheels.gltf` (ścieżka M3B.3, visual-only) na
wszystkich czterech kołach przez współdzielony helper
`ComputeJozzVehicleWheelVisualCorrection(...)`; prymitywne pomarańczowe koła
domyślnie ukryte (fizyka aktywna — to tylko debug shape), przełączalne.
Mały tor testowy: dwie rampy + tarka (washboard) do czucia zawieszenia.

## Czego M5 świadomie NIE robi

```text
nie zmienia Box3D internals
nie dodaje mesh collision
nie robi multi-body suspension (wahacze/dampery to nadal visual-only)
nie robi pełnego importera glTF
nie zmienia corner lab M2.5 ani jego fizyki
nie jest finalnym tuningiem feel — to punkt startowy do tuningu
```

## Czego nauczył nas headless smoke (ważne!)

Dwa realne defekty złapane przed pierwszym uruchomieniem GUI:

1. **Zapadnięte zawieszenie.** Sztywność sprężyny wheel jointa w Box3D podąża
   za masą efektywną więzu, którą dominuje lekkie koło — nie za udziałem masy
   chassis. Przy 2.5 Hz sprężyny zapadały się na limit kompresji pod ~700 kg
   nadwoziem (osiadanie 0.47 m). Domyślne `suspensionHertz` w M5 to dlatego
   6.0 przy kołach o gęstości 80. Wniosek na przyszłość: częstotliwości
   sprężyn w Box3D czyta się „na masę koła", nie „na masę auta".

2. **Odwrócony znak skrętu.** Dodatni kąt steering jointa obraca +X w stronę
   -Z (skręt w prawo) wokół osi zawieszenia (+Y). Moduł neguje input, żeby
   `steer = +1` znaczyło „w lewo" zgodnie z kontraktem nagłówka.

Do tego lekcja procesowa: wrapper buildowy `cmd /c "set PATH=& ..."` z
dokumentacji **nie działa w środowisku Git Bash** (cmd startuje interaktywnie
i nic nie wykonuje, wychodząc kodem 0). W Git Bash należy wołać
`cmake --build --preset windows-debug --target ...` bezpośrednio — problem
zduplikowanego PATH nie występuje. Wrapper pozostaje potrzebny tylko w
środowiskach, gdzie MSBuild faktycznie wywala się na duplikacie `Path/PATH`.

## Walidacja

```text
cmake --build --preset windows-debug --target samples
cmake --build --preset windows-debug --target test
cmake --build --preset windows-debug --target jozz_vehicle_validation
build\bin\Debug\test.exe                      -> All Box3D tests passed
build\bin\Debug\jozz_vehicle_validation.exe   -> ... m5 drive smoke: ok ... OK
build\bin\Debug\samples.exe --sample 96 --frames 300  -> 0 sokol errors
```

Wyniki smoke'a z 2026-07-05:

```text
osiadanie 0.105 m z 0.420 m skoku kompresji
jazda: dx 50.97 m w 6 s, dz 2.56 m, prędkość 12.75 m/s (~46 km/h)
skręt: heading delta +1.232 rad (steer=+1 -> w lewo)
hamowanie: zatrzymany
```

## Ręczny smoke dla Jozza

1. Otwórz `Jozz Vehicle / M5 First Drivable`.
2. `W` — pojazd rusza do przodu, koła glTF się kręcą.
3. `A/D` — skręt w lewo/prawo zgodny z intuicją.
4. `Space` — hamuje do zera.
5. `T` — kamera podąża za pojazdem; mysz kręci widokiem.
6. Wjedź na rampę i tarkę — zawieszenie pracuje, nic nie eksploduje.
7. Wyłącz „Upright assist" — pojazd robi się bardziej „żywy" na dachowanie.
8. Suwaki hertz/damping/torque działają na żywo.
9. `Jozz Vehicle / Lab M2 Primitive Corner` nadal działa jak dotychczas.

## Następny krok po M5

Rekomendowana kolejność:

```text
M5.1  feel tuning pass (masy, torque, hertz, damping) na bazie jazdy Jozza
M4C   proceduralny damper/cardan visual — teraz na jeżdżącym pojeździe
```
