# Przekazanie: kolider koła w silniku

Data: 2026-08-03 | Stan repo: `4c36a3d` na `jozz-scan-terrain-f0`, wypchnięty

Ten dokument jest po to, żeby ktokolwiek — Ty, inna osoba, inne narzędzie —
mógł zacząć budować kolider koła **od razu**, bez czytania czterech dokumentów
programu i bez powtarzania drogi, którą ja przeszedłem niepotrzebnie. Piszę go
bez kodów typu `F-31`; tam, gdzie coś twierdzę, podaję plik i linię, żeby dało
się sprawdzić, a nie uwierzyć.

Wszystko, co niżej, jest **sprawdzone w kodzie albo zmierzone**. Rzeczy, których
nie zdążyłem sprawdzić, są zebrane osobno na końcu i wyraźnie oznaczone.

---

## 1. Co ma powstać

Nowy typ kształtu w silniku: **koło obrotowe**. Walec o promieniu `R`
i szerokości `W`, z zaokrąglonym barkiem o promieniu `c`:

- `c = 0` → walec o ostrej krawędzi (slick, drift),
- `0 < c < W/2` → bieżnia płaska z zaokrąglonym barkiem (zwykła opona),
- `c = W/2` → pełny torus, przekrój okrągły (balonówa offroadowa).

Jednym kształtem obejmujesz cały zakres profili opon w trzech światach.

**Sedno konstrukcji, od którego zależy powodzenie:** kontakt musi być liczony
**analitycznie z osi koła**, a nie z wierzchołków i ścianek. Punkt styku z
płaszczyzną to rzut osi na płaszczyznę, przesunięty o `R−c` i o `c`. Taki
kontakt **nie zmienia się przy obrocie koła wokół własnej osi** — a to jest
dokładnie ta własność, która czyni dzisiejszą sferę gładką. Różnica jest taka,
że kształt obrotowy ma szerokość, a sfera nie.

---

## 2. Dlaczego nic ze stockowego słownika nie zadziała

Trzy liczby z pomiaru w **pełnym aucie na płaskiej płycie** (dowód:
`tools/jozz_wheel_bench/evidence/run_m6_jazda_na_plaskim_2026_08_03.txt`).
Płyta jest płaska, więc koło idealnie okrągłe **musi** pokazać zero; wszystko
powyżej zera produkuje sam kształt koła.

Trzęsienie nadwozia przy 58 km/h, jazda na wprost (m/s²):

| kształt koła | trzęsie | ma styk z ziemią |
|---|---|---|
| sfera | **0,061** | zawsze |
| walec — **jeden** kształt | 2,434 | w 41% kroków |
| pierścień **64** kapsuł | 2,244 | w 79% kroków |

1. **Nie chodzi o liczbę kształtów.** Jeden walec jest tak samo zły jak 64
   kapsuły; obie rzeczy dzieli od sfery ~37×.
2. **Koło traci kontakt z ziemią** na płaskim betonie — odbija się od własnej
   powierzchni. Sfera ma styk w każdym kroku.
3. **Więcej mocy obliczeniowej pogarsza.** 4/8/16/32 podkroki solvera: sfera
   0,061 → 0,004 (coraz gładziej, kontrola poprawności pomiaru), opona z 32
   kapsuł 2,233 → **3,233**. Nie ma czego kupić za procesor.

Do tego drugi, wcześniejszy pomiar: sztywna bryła złożona z wielu kształtów
**nie daje odcisku** — 1–3 punkty naprawdę niosące obciążenie, rekord ~5 przy
576 kształtach na jedno koło.

Wniosek: rodzina „koło złożone ze stockowych klocków" nie daje **ani** odcisku,
**ani** gładkiego toczenia, i nie jest to kwestia budżetu.

---

## 3. Fakty o box3d, które przesądzają o konstrukcji

Sprawdzone w kodzie, nie z pamięci:

- **Słownik kształtów** (`include/box3d/types.h:430–452`): kapsuła, compound,
  height field, hull, mesh, sfera. **Nie ma nic obrotowo symetrycznego względem
  osi koła i szerszego niż punkt.** Sfera jest jedyną bryłą bez cech
  przechodzących przez punkt styku przy obrocie — i dlatego jedyną gładką.
  Kapsuła o osi wzdłuż osi koła wyglądałaby idealnie (przekrój w płaszczyźnie
  toczenia to okrąg), ale jej czasze mają promień równy promieniowi koła, więc
  najwęższe takie koło ma szerokość `2R` — dla `R` 0,51 m to 1,03 m zamiast
  0,44 m. To jest powód, dla którego tego nie da się obejść.

- **Kontakt jest przycięty do czterech punktów**
  (`include/box3d/constants.h:114`: `#define B3_MAX_MANIFOLD_POINTS 4`).

- **Tarcie jest wspólne dla całego kontaktu, nie per punkt.**
  W `b3Manifold` (`include/box3d/types.h:2599–2614`) polami struktury są
  `frictionImpulse`, `twistImpulse`, `rollingImpulse`, obok tablicy
  `points[B3_MAX_MANIFOLD_POINTS]` i `pointCount`. Czyli jeden kontakt = jedna
  kotwica tarcia, niezależnie od liczby punktów. To ma bezpośrednie znaczenie
  dla odcisku: żeby opona miała rozłożony docisk **i** rozłożone tarcie, sam
  nowy kształt może nie wystarczyć.

- **Limit hulla**: 255 półkrawędzi (uint8) → pryzmat maksymalnie 42 ścianki.
  Dlatego „zagęśćmy walec" nie jest drogą wyjścia nawet teoretycznie.

- **Świat pojazdu jedzie bez ciągłej detekcji kolizji** i jest to świadoma
  decyzja z uzasadnieniem w `samples/validation/jozz_validation_helpers.cpp:58–69`
  (toczące się koło startuje zamiatanie w kontakcie z ziemią i wywala walidację
  w kompilacji Debug). Nowy kolider musi działać w tym reżimie.

- **Teren produktu jest siatką trójkątów** (`b3_meshShape`), nie pudłem. Cały
  program mierzył wcześniej na pudełkach — to była osobna, prawdziwa dziura.

---

## 4. Gdzie dokładnie dołożyć

- **Typ kształtu**: `include/box3d/types.h:430` (`b3ShapeType`) — nowa pozycja
  przed `b3_shapeTypeCount`. Uwaga: enum jest używany jako indeks tablicy
  rejestracji, więc dopisanie na końcu jest bezpieczniejsze niż w środku.
- **Rejestracja par kolizji**: `src/contact.c:104–137`. Tablica
  `s_registers[b3_shapeTypeCount][b3_shapeTypeCount]`, wypełniana przez
  `b3AddType( typ1, typ2 )`. Dla koła potrzebne są pary z: `b3_meshShape`
  (najważniejsza — teren), `b3_hullShape`, `b3_sphereShape`, `b3_capsuleShape`,
  a docelowo `b3_heightShape` i `b3_compoundShape`.
- **Generowanie kontaktu**: `src/manifold.c`, `src/convex_manifold.c`,
  `src/triangle_manifold.c` (kontakt z pojedynczym trójkątem siatki),
  `src/mesh_contact.c` (pętla po trójkątach).
- **Obrys, masa, promień, raycast**: `src/shape.c`, `src/aabb.c`,
  `src/geometry.c` — wszędzie tam, gdzie `switch` po `b3ShapeType` wymieni
  istniejące typy; kompilator pokaże listę miejsc po dodaniu enuma, jeśli
  ostrzeżenia o niepełnym `switch` są włączone.

**Warunek właściciela z 2026-07-24, nadal obowiązujący:** łatka ma zostać
osobnym, przeglądalnym kawałkiem, żeby dało się wciągać zmiany z upstreamu.
Polityka: `docs/KOLA_03_POLITYKA_BOX3D_PL.md`.

---

## 5. Egzamin już istnieje — nie trzeba go budować

`samples/validation/jozz_probes_ride.cpp`, uruchamiany razem z walidatorem.
**Zawsze z katalogu głównego repo:**

```bash
build/bin/Debug/jozz_vehicle_validation.exe
```

Drukuje tabelę „ride quality diagnosis probe". Nowy kolider wpina się jako
kolejny wiersz w liście obwiedni w tym pliku (`RideEnvelope`) i porównanie jest
natychmiastowe.

**Próg powodzenia — trzy warunki naraz:**

1. trzęsienie przy 16 m/s blisko **0,061** (poziom sfery), a nie 2,2–2,8;
2. `pkt/koło` co najmniej **1,00** w każdym kroku (koło nie traci ziemi);
3. pełna szerokość opony, a nie styk punktowy.

Jeśli któryś warunek nie wychodzi — wiadomo w jedno uruchomienie, nie po
dwóch tygodniach. Punkt 3 sprawdzisz też sondą jednego koła:
`tools/jozz_wheel_bench/` (kolumny `nios` = ile punktów naprawdę niesie
i `max%` = udział największego).

---

## 6. Czego nie powtarzać

Wyniki zamknięte, negatywne, każdy z surowym przebiegiem w
`tools/jozz_wheel_bench/evidence/`:

- pryzmaty i pierścienie kapsuł jako koło — **nie toczą się** (§2);
- zagęszczanie ścianek/kapsuł — nie pomaga, a limit hulla to 42 ścianki;
- podkroki solvera jako lekarstwo — **pogarszają**;
- dzisiejsza obwiednia „sfera + boczny walec": walec **nigdy nie dotyka
  gruntu**, więc dzisiejsze koło jest w jeździe fizycznie sferą. Jeśli
  porównujesz coś ze „sferą" i z „dzisiejszym kołem", porównujesz z tym samym;
- profil poprzeczny (wysklepienie korony) badany na płaskiej płycie — płyta
  tego nie widzi, potrzebny wąski próg.

---

## 7. Czego nie sprawdziłem, a ma znaczenie

Uczciwie, żeby nikt nie zaczął od fałszywego założenia:

1. **Czy trzęsienie zachowuje się tak samo na siatce terenu.** Cały pomiar z §2
   jest na płaskiej płycie — celowo, żeby oddzielić koło od gruntu. Na meshu
   dochodzą krawędzie trójkątów. Nie zmieni to werdyktu o sferze (0,061 to
   podłoga), może zmienić dystanse między kandydatami.
2. **Czy sam nowy kształt wystarczy do odcisku.** Cztery punkty na kontakt
   i jedna kotwica tarcia (§3) to ograniczenia niezależne od kształtu. Może się
   okazać, że kolider daje gładkie toczenie i szerokość, ale odcisk wymaga
   drugiej łatki — podniesienia limitu punktów albo tarcia per punkt. To osobna
   zmiana i osobna decyzja, **po** pierwszym egzaminie, nie przed.
3. **Kierunek przyczynowy** między „koło traci styk" a „kontakt liczony od
   nowa w każdym kroku". To dwa odczyty tego samego zjawiska i dane ich nie
   rozdzielają. Dla budowy kolidera to nie ma znaczenia, dla ewentualnej
   diagnozy czegoś innego — może mieć.
4. **Strojenie kierownicy pod prawdziwą szerokość styku.** Geometria układu
   kierowniczego była strojona, gdy koło dotykało ziemi jednym punktem.
   Szeroki styk wzbudza szarpanie kierownicy — zmierzone. Nowy kolider to
   wywoła i będzie trzeba to przestroić; to zmienia prowadzenie auta, więc
   należy do właściciela.

---

## 8. Stan repo w chwili przekazania

- gałąź `jozz-scan-terrain-f0`, `4c36a3d`, wypchnięta, drzewo czyste
  (poza `tools/jozz_wheel_bench/jozz_qc_rig.obj` — artefakt kompilacji);
- walidator: 19 sond + 2 mapowe, **OK**, uruchamiany z katalogu głównego;
- cztery bramki kół: zielone; kontrola spójności dowodów: 0 ostrzeżeń;
- Debug i Release zbudowane;
- **domyślne koło auta jest nietknięte** — wszystkie badane kształty były i są
  opcją do wyboru, nic w grze nie zmieniło zachowania.
