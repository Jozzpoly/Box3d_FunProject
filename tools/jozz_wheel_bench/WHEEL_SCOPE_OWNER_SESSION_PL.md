# Wheel Scope — sesja obserwacyjna dla Jozza

Okno na to samo koło, które mierzymy bez okna. Nie trzeba nic konfigurować
i nie trzeba wiedzieć nic o strukturze repo.

## Uruchomienie

```bash
build\bin\Release\samples.exe --sample-name "Wheel Scope"
```

Jeśli plik nie istnieje:

```bash
cmake --build build --target samples --config Release
```

## Sterowanie

| Klawisz | Co robi |
|---|---|
| `1` | koło **sphere** (kontrola) |
| `2` | koło **prism-42** (fasetowane) |
| `R` | reset — od nowa, ten sam stan początkowy |
| `P` | pauza / dalej |
| `O` | jeden krok fizyki (Shift+`O` = pięć) |
| `,` `.` | spowolnienie: 1:1 → 1:2 → 1:4 → 1:8 → 1:16 |
| `W` | zeruj licznik pracy (definiuje okno pomiaru) |
| `V` | kamera przestaje / zaczyna śledzić koło |
| `G` | **zapisz obserwację** do pliku |
| `S` | **zapisz konstrukcję** na półkę (patrz niżej) |
| `M` | szuflada metryk (czas kroku silnika) |
| `Tab` | schowaj / pokaż panele |
| kółko, LPM | zoom i obrót kamery |

Zaburzenia — **zmieniają fizykę**:

| Klawisz | Co robi |
|---|---|
| `C` | wyłącz / włącz regulator prędkości |
| `K` | kopniak w bok |
| `J` | zrzut w dół |
| `B` | impuls hamowania |
| `Ctrl` + LPM | **chwyć koło myszą** i pociągnij |

Po pierwszym zaburzeniu ekran przechodzi na `SESJA: EXPLORATION` i mówi wprost,
że to nie jest przebieg dowodowy. `R` wraca do czystej obserwacji. Chwyt myszą
liczy się tak samo jak kopniak — to sprężyna ciągnąca koło, więc podnosi tę samą
flagę.

## Półka konstrukcji — żeby ciekawy przypadek nie przepadł

Panel „Konstrukcja (przebudowa)" zmienia **z czego** koło jest zbudowane:
liczbę ścianek i prędkość startową. To nie jest zaburzenie biegnącego przebiegu
— to nowy przebieg od zera. Przebudowa kosztuje 0.1 ms, więc suwak można ciągnąć
swobodnie; przebudowa następuje przy puszczeniu.

Gdy trafisz na coś ciekawego: wpisz nazwę w sekcji „Półka konstrukcji" i naciśnij
`S`. Powstaje plik `wheel_scope_bench/<nazwa>.rig` — zwykły tekst, do przeczytania
i do poprawienia w notatniku:

```
format 1
# 2026-07-31 12:40:03 | prism zaczyna mlotkowac | zapisane w kroku 412 przy v=3.0012 | sesja=OBSERVATION | v_kryt=3.7439 v/v_kryt=0.802

# geometria i masa
variant prism-Nmax
prism_sides 17
wheel_r 0.514100015
...

# instrument
dt 0.016666666666666666
substeps 4
```

Wczytanie: przycisk „wczytaj" przy nazwie. Nic nie jest nadpisywane po cichu —
zapis pod istniejącą nazwą dokłada `_2`, `_3`.

**Ten sam plik uruchamia stend bez okna.** To jest cała różnica między „widziałem
coś dziwnego" a zadaniem do policzenia:

```bash
tools\jozz_wheel_bench\wheel_bench.exe --rig-trace wynik.csv --rig-config wheel_scope_bench\nazwa.rig
```

Plik możesz też napisać sam od zera — wystarczy `format 1` i te klucze, które
chcesz zmienić; reszta bierze wartości kontraktowe. Pusty szablon z kompletem
kluczy i ich wartościami kontraktowymi:

```bash
tools\jozz_wheel_bench\wheel_bench.exe --rig-config-template szablon.rig
```

Klucz z literówką **nie jest po cichu pomijany**: wczytanie zostaje odrzucone
z komunikatem. Plik zawierający samo `format 1` odtwarza kontrakt Q2A co do bitu.

## Pasek reżimu — kiedy pomiar w ogóle ma sens

Linia `rezim` mówi, czy koło jest w stanie utrzymać ciągły kontakt z podłożem:

- `v/v_kryt < 1` → kontakt ciągły, jest model odniesienia;
- `v/v_kryt > 1` → sztywne koło **odrywa się** przy obtaczaniu wokół wierzchołka;
- `faset` > 1 → między dwiema aktualizacjami kontaktu mija cała ścianka.
  Więcej podkroków tego **nie naprawia** (`KOLA_01` §8.1).

Domyślna konfiguracja (13 m/s) jedzie 2.76× powyżej `v_kryt`. To nie jest błąd —
to jest reżim, w którym mierzyliśmy do tej pory, i teraz widać to na ekranie.

## Co widać na ekranie

- **bryła** — to prawdziwy collider, nie zastępczy model. Fasetowanie prism-42
  jest widoczne na obwodzie;
- **niebieska linia przez koło** — oś obrotu. Przerywana tam, gdzie schowana
  w bryle;
- **zielona strzałka** — kierunek jazdy wyliczony z osi;
- **pomarańczowa strzałka** — siła regulatora, pełna długość = limit 1900 N;
- **białe punkty** — punkty kontaktu niosące obciążenie; **żółte** — dotykające,
  ale bez impulsu. Krótkie kreski to normalne kontaktu;
- **fioletowy krzyżyk** — punkt odniesienia R pod środkiem masy. To z niego
  liczą się liczby „odniesienia" w telemetrii. **Nie jest** punktem kontaktu —
  dlatego oba są na ekranie naraz;
- **złota linia za kołem** — ślad wysokości środka. Szara linia obok to wysokość
  nominalna. Gładkie toczenie = obie linie się kryją. Suwak „przewyższenie
  śladu" pozwala podbić pion, żeby zobaczyć kierunek zjawiska — wtedy **nie jest
  to już skala 1:1** i ekran to pisze.

## Scenariusz obserwacji

Nie test zaliczeniowy. Kolejność jest po to, żeby porównanie było uczciwe.

1. Uruchom. Startuje `sphere`. Popatrz minutę w normalnym tempie.
2. Przyjrzyj się kontaktowi: ile punktów, gdzie siedzą, jak się zmieniają.
3. `,` `,` `,` — zwolnij do 1:8. Potem `P` i kilka razy `O`, krok po kroku.
4. `2` — przełącz na prism-42. Znowu normalne tempo, potem znowu spowolnienie
   i pojedynczy krok.
5. Porównaj trzy rzeczy: ruch koła, zachowanie kontaktu i pracę regulatora
   (linijka „regulator" i „praca od zerowania"). Wciśnij `W` po ustabilizowaniu,
   żeby licznik pracy nie mieszał rozbiegu z jazdą.
6. Opcjonalnie jedna perturbacja: `K` albo `J`. Zobacz, jak każde koło wraca
   (albo nie wraca) do toczenia.
7. `G` — zapisz obserwację. Plik: `jozz_wheel_scope_observations.txt` w katalogu
   uruchomienia. Zapisuje się dokładny kontekst techniczny tej chwili.

## Pytania, na które odpowiedź ma tylko Jozz

1. Co wygląda **najbardziej nienaturalnie**?
2. Czy to przypomina regularne toczenie, uderzanie, podskakiwanie czy ślizganie?
3. Które różnice są wizualnie istotne, chociaż średnia prędkość jest podobna?
4. Jakiego następnego testu chciałbyś zobaczyć?

Nie pytamy o architekturę kodu ani o to, jak coś zaimplementować.

## Czego ta sesja NIE ustala

Obserwacja i eksploracja **nie są** wynikiem silnikowym. To, co zobaczysz,
zamienia się w fakt dopiero tak:

```
obserwacja wizualna → jawna hipoteza → poprawka kontraktu PRZED przebiegiem
                    → zamrożony eksperyment headless
```

Okno nie dopisuje niczego do łańcucha dowodowego. Jedyne pliki, jakie tworzy,
to zaklaki obserwacji i odcisk stanu dla testu równoważności.

## Dlaczego można wierzyć, że to ta sama fizyka

Nie na słowo — jest to sprawdzane komendą:

```bash
python tools\jozz_wheel_bench\check_visual_equivalence.py
```

Test porównuje 600 kroków stanu ciała bajt w bajt między trybem bez okna
i z oknem, dla obu wariantów, a dodatkowo sprawdza, że tempo rysowania,
ruch kamery i overlay nie zmieniają ani jednej cyfry.
