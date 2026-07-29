# JP-02 — kontrakt telemetrii granic faz

Status: **kontrakt zapisany przed implementacją.** Jednostka instrumentuje
istniejący eksperyment; nie zmienia go i nie rozstrzyga żadnego sporu.

## Pytanie jednostki

**W jakim stanie ruchu i energii każdy wariant rozpoczyna oraz kończy obecne
okno pomiarowe sekcji E?**

Powód: `RunRoll` wykonuje 120 kroków rozbiegu *przed* 240 krokami pomiaru
(`wheel_bench.c`, pętle przy `120` i `steps = 240`). Rozbieg nie jest nigdzie
raportowany, więc nie wiadomo, czy wariant wchodzi w okno pomiarowe tocząc się,
ślizgając, czy prawie stojąc. Bez tego żadna metryka toczenia z §7 nie ma
znanej dziedziny.

## Język, którego jednostka używa o energii

JP-02 mierzy energię kinetyczną w trzech punktach. **Nie mierzy pełnego bilansu
pracy**, więc wolno mówić wyłącznie o **zmianie lub spadku energii kinetycznej**.
Słowa „strata" i „dysypacja" są zakazane — sugerowałyby zamknięty bilans, którego
ta jednostka nie liczy.

## Obserwacja `cylinder-32` — status nierozstrzygnięty

Początkowe `vx` różni się od 13 m/s o około **−1.9e−6 m/s**, jako jedyny z pięciu
wariantów. **Mechanizm przyczynowy pozostaje nierozstrzygnięty.** Zapisujemy samą
obserwację; żadnej hipotezy nie utrwalamy jako dowiedzionej.

## Czego jednostka NIE rozstrzyga

Czy §7 jest ważne czy nieważne; który backend wygrywa; czy potrzebny jest nowy
shape; jak ma działać Q2; jak zmienić prawo opony; jak naprawić opór toczenia.
Żadnego progu „ważności danych" nie wprowadzamy — JP-02 podaje liczby, nie werdykt.

## Granice faz

| granica | krok globalny | czas | znaczenie |
|---|---|---|---|
| `INITIAL` | 0 | 0 s | stan zadany, przed pierwszym `b3World_Step` |
| `MEASURE_START` | 120 | 2 s | dokładnie stan, od którego startuje obecna pętla pomiarowa |
| `MEASURE_END` | 360 | 6 s | koniec okna pomiarowego |

`dt = 1/60`, 4 podkroki. Instrumentowana **wyłącznie sekcja E**: dwa przypadki
obciążenia × pięć wariantów = 10 przebiegów × 3 granice = **30 rekordów**.
Sekcje D i F nie emitują telemetrii.

## Nie zmieniamy niczego z eksperymentu

120 kroków rozbiegu, 240 kroków pomiaru, `dt`, podkroki, geometria wariantów,
masa, bezwładność, prędkość początkowa i kątowa, tarcie, contact settings,
continuous collision, przyłożona siła, kolejność wariantów, `src/`, `include/`.

## Obciążenie — rejestrowane, nie poprawiane

Stend liczy docisk z `9.81`, a świat ma grawitację `-10.0`
(`src/types.c:16`, zweryfikowane). Różnicy **nie naprawiamy w JP-02**;
zapisujemy ją jako cztery obserwowalne pola:

| pole | corner case | v1 case |
|---|---|---|
| `nominal_load_N` | 1900.00 | 431.64 |
| `external_downforce_N` = `nominal − m·9.81` | 1468.36 | 0.00 |
| `gravity_load_N` = `m·10.0` | 440.00 | 440.00 |
| `effective_static_load_N` | **1908.36** | **440.00** |

Przebieg nazywany w §7 „432 N" ma więc faktycznie 440 N.

## Konwencje, których nie wolno zgadywać

- **Oś koła nie jest zakładana jako światowe Z.** Lokalna oś obrotu to `(0,1,0)`
  (`FreezeMass` stawia `iSpin` na `inertia.cy`); oś w świecie wyznaczana jako
  `R·(0,1,0)` z bieżącej rotacji ciała.
- **Kierunek jazdy** `forward = normalize(up × axle)`, `up = (0,1,0)`.
- **Znaki.** `omega_spin = ω_world · axle`. Przy stanie zadanym `ω = (0,0,−v/R)`
  i osi `+ẑ` daje to `omega_spin ≈ −25.29 rad/s` — **ujemne**.
  `reference_rim_speed = −(ω × r)·forward` gdzie `r = −R·up`, co przy stanie
  zadanym daje **+13 m/s**. `reference_slip_speed = v·forward −
  reference_rim_speed`, czyli **0 przy czystym toczeniu**. Dodatni ruch pojazdu
  i ujemna prędkość kątowa nie dają odwróconej interpretacji.
- **Pola `reference_*` nie mierzą rzeczywistego punktu kontaktu.** Liczą się
  z **nominalnego** `reference_radius_m = WHEEL_R` i z punktu odniesienia `R`
  pod środkiem masy. Rzeczywisty punkt kontaktu z manifoldu leży gdzie indziej,
  a przy fasetowanym obwodzie zmienia się w trakcie obrotu. Slip z manifoldu
  **nie jest** w JP-02 implementowany; przedrostek `reference_` istnieje po to,
  by nie dało się pomylić jednego z drugim przy późniejszym czytaniu CSV.
- **Energia obrotowa** z pełnego tensora: `ω_local = R⁻¹·ω_world`,
  `E = ½ ω_local · (I_local · ω_local)`. Nie z jednego elementu tensora.
- **Slip ratio** nie jest wyprowadzane przy prędkości bliskiej zeru. Surowe
  `slip_speed` jest obowiązkowe; ratio jest polem dodatkowym z jawną regułą
  dziedziny (`domain_valid`).
- `cumulative_spin_angle_rad` jest **ze znakiem** (może maleć);
  `cumulative_revolutions` całkuje `|ω_spin|` i jest **niemalejące**.

## Kryteria poprawności instrumentacji

1. Bez flagi `--phase-telemetry` zachowanie i wszystkie dotychczasowe sekcje
   outputu pozostają niezmienione.
2. Istniejący plik telemetrii nie jest po cichu nadpisywany.
3. Stały nagłówek, stabilna kolejność rekordów, kropka dziesiętna, brak
   `NaN`/`Inf`, dokładnie 30 rekordów danych.
4. Błąd zapisu telemetrii nie jest ignorowany — kończy przebieg kodem != 0.
5. `MEASURE_START` jest dokładnie tym stanem, od którego rusza obecna pętla
   pomiarowa (rekord powstaje po 120. kroku, przed pierwszym krokiem pomiaru).
6. Bramka braku zmiany fizyki: build przed i po, ta sama `box3d.lib`,
   porównanie sekcji A–G po odfiltrowaniu wyłącznie pól provenance. Sekcje
   deterministyczne muszą być identyczne co do znaku. Sekcja D mierzy czas
   zegarowy i jest niedeterministyczna z natury — jej rozrzut porównujemy
   z rozrzutem dwóch przebiegów **tego samego** binarium, zmierzonym osobno.
   „Dodaliśmy tylko gettery" nie jest dowodem.

## Zakres plików

Zmieniane: `wheel_bench.c`, `build.bat` (jeśli konieczne), ten kontrakt,
mały walidator w `tools/jozz_wheel_bench/`. Nietykane: `tools/evidence/*`,
`RAW_MANIFEST.json`, `summary.json`, stare raw logi, `docs/KOLA_*`,
`KOLA_FINDINGS.json`, `src/`, `include/`.

Przebieg diagnostyczny trafia do katalogu scratch/review — **nie** do trwałych
evidence, **nie** do `RAW_MANIFEST.json`, bez renderowania dokumentacji
i bez promocji jakiegokolwiek claimu.
