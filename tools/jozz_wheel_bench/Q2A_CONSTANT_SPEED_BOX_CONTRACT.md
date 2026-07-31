# Q2A — kontrakt eksperymentu przy utrzymywanej prędkości

Status: **wszystkie definicje zapisane przed pierwszym wynikiem.**

> **Gdzie ten kontrakt żyje w kodzie (2026-07-30).** Wszystkie liczby z tabeli
> „Warunki" są od teraz polami `JozzRigConfig`, a ich wartości kontraktowe zwraca
> **`JozzRig_DefaultConfig()`** (`jozz_wheel_rig.c`). Dokument pozostaje źródłem
> *uzasadnień*; kod jest źródłem *wartości*. Rozjazd między nimi jest wykrywalny
> maszynowo:
>
> - **`check_behaviour_lock.py`** porównuje przebieg domyślnej konfiguracji z
>   wzorcem w `golden/`, bajt w bajt, 600 kroków, oba warianty. Wzorzec został
>   wygenerowany i zweryfikowany wobec stendu zbudowanego ze źródeł z `f9576c3`.
> - Każdy przebieg (trace, zakładka obserwacji) niesie **`JozzRig_ConfigDigest()`**,
>   więc żadna liczba nie krąży bez opisu, z czego powstała.
>
> Konfiguracja **niedomyślna** to inny eksperyment i nie jest objęta tym
> kontraktem — nawet jeśli używa tego samego rigu.

## Pytanie

**Przy tej samej utrzymywanej prędkości liniowej, tym samym dystansie, tym samym
efektywnym obciążeniu i płaskim podłożu typu box: ile pracy zewnętrznego napędu
wymaga `sphere`, a ile jeden prosty wariant fasetowany?**

JP-02 pokazał, że `corner_1900N` jest transientem — warianty przebywają w oknie
od 2.1 do 51.7 m. Q2A wyrównuje drogę kontaktu, żeby porównanie miało sens.

## Czego Q2A nie robi

Nie wybiera finalnej reprezentacji opony, nie porównuje mesha, nie analizuje
upstreamu, nie testuje `rollingResistance` Box3D, nie implementuje prawa opony,
nie rozszerza core, nie uruchamia macierzy pięciu wariantów.

## Warianty

| rola | wariant |
|---|---|
| kontrola | `sphere` — gładki obwód analitycznie |
| fasetowany | **`prism-Nmax`** (40 ścianek) |

Uzasadnienie wyboru `prism-Nmax`: jeden shape i jeden hull, więc liczba ciał
kontaktowych jest ta sama co u sfery; ostre barki, więc **fasetowanie obwodu
jest jedyną różnicą geometryczną** wobec gładkiej kontroli. `phased union-4` ma
4 shape'y (zmienia liczbę manifoldów), `tire profile` ma zaokrąglone barki (dwie
zmienne naraz), a `cylinder-32` to ten sam mechanizm przy grubszym podziale.
Wybór najdrobniejszego podziału czyni test **konserwatywnym**: jeśli 40 ścianek
nadal kosztuje pracę, nie jest to artefakt małego N.

## Warunki — zapisane przed pomiarem

| parametr | wartość | uzasadnienie |
|---|---|---|
| podłoże | płaski box, `b3MakeBoxHull(400,1,60)` @ y=−1 | jak w sekcji E |
| `rollingResistance` | **jawnie `0.0`** na kole i na podłożu | usuwa confound; `types.h:407` twierdzi „only spheres and capsules", ale `src/contact.c:646-684` nadaje promień także hullowi — bez jawnego zera sfera i pryzmat dostałyby różne formuły |
| tarcie | **1.0 na obu**, efektywne `sqrt(1·1) = 1.0` | domyślne to 0.6, a sekcja E ma 1.2 tylko na podłożu; tu jawnie i symetrycznie |
| masa / bezwładność | `FreezeMass(44 kg)`, `I_spin = 0.70·m·R²`, `I_tr = 0.55·m·R²` | identycznie jak E |
| `dt` / podkroki | `1/60` / `4` | identycznie jak E |
| target prędkości | **13.0 m/s** | ciągłość z sekcją E |
| efektywne obciążenie | **1900.0 N** | grawitacja świata `−10.0` → `440 N`; docisk zewnętrzny `1460 N`. Liczone z **faktycznej** grawitacji, nie z `9.81` |
| napęd | `b3Body_ApplyForceToCenter` wzdłuż światowego `+X` | przez środek masy, bez zamierzonego momentu |
| `omega` początkowe | `−v_target/R = −25.28691 rad/s` wokół osi koła | nominalne toczenie |
| continuous collision | wyłączone | identycznie jak E |

## Regulator — PI z anti-windup

```
err   = v_target − v·x̂
F_raw = Kp·err + Ki·I
F     = clamp(F_raw, −Fmax, +Fmax)
I    += err·dt        tylko gdy |F_raw| ≤ Fmax   (anti-windup)
```

Regulator P nie wystarcza: przy oporze rzędu 110 N (oszacowanie z JP-02:
prism traci ~5 m/s w 2 s) proporcjonalny regulator zostawiłby **stały** błąd
`R/Kp`, a pytanie Q2A wymaga utrzymanej prędkości, nie zbieżnej do innej.

| nastawa | wartość | wyprowadzenie |
|---|---|---|
| `Kp` | 440 N/(m/s) | `m/τ` przy `τ = 0.1 s` |
| `Ki` | 1100 N/(m/s)/s | `ω_n = √(Ki/m) = 5 rad/s`, `ζ = Kp/(2√(Ki·m)) = 1.0` — krytycznie tłumiony |
| `Fmax` | 1900.0 N | równe efektywnemu obciążeniu, czyli limit przyczepności przy `μ = 1.0`; powyżej eksperyment przestaje dotyczyć toczenia |

`ω_n·dt = 0.083 ≪ 1`, więc regulator jest daleko od granicy stabilności kroku.
Pasmo regulatora `0.8 Hz` jest **dwa rzędy** poniżej częstotliwości przejść
między ściankami `N·v/(2πR) ≈ 161 Hz` — regulator **nie może** wygładzić tętnienia
fasetowania. To celowe: tętnienie ma być widoczne w danych, nie wytłumione.

## Kwalifikacja i okno pomiarowe

Stały warm-up **nie jest** bramką. Pomiar startuje dopiero po spełnieniu warunku:

| kryterium | wartość | uzasadnienie |
|---|---|---|
| warunek stabilizacji | `|err| ≤ 0.05 m/s` przez **60 kolejnych kroków** (1 s) | 0.05 m/s = 0.38% targetu, ~5 rzędów nad szumem float; 60 kroków = 5 stałych czasowych `1/ω_n` |
| timeout | **900 kroków** (15 s) | 15× czas ustalania; brak kwalifikacji do tego czasu = `failed_to_qualify` |
| okno pomiarowe | **stały dystans 50.0 m** przemieszczenia w `+X` | dystans wyrównuje **drogę kontaktu** bezpośrednio, co jest sednem pytania; 50 m przy 13 m/s ≈ 231 kroków, czyli rząd okna z sekcji E (240) — oba eksperymenty pozostają współmierne |

Reguła jest **identyczna dla obu wariantów**.

### Kryteria przyjęcia okna — zadeklarowane przed zobaczeniem wyniku

| metryka | bramka |
|---|---|
| średni błąd bezwzględny prędkości | ≤ 0.05 m/s |
| RMS błędu prędkości | ≤ 0.10 m/s |
| udział kroków w saturacji (`|F| ≥ 0.999·Fmax`) | ≤ 5% |
| **maksymalny błąd** | **raportowany, NIE bramkowany** |

Maksymalny błąd jest zdominowany przez pojedyncze uderzenia ścianek — czyli
przez **badane zjawisko**, nie przez wadę regulatora. Bramkowanie na nim
odrzucałoby wariant za to, co mamy zmierzyć. Zostaje w raporcie.

## Dwuetapowa kwalifikacja

**Etap A** — regulator konfigurowany wyłącznie na `sphere`; wszystkie nastawy
i kryteria powyżej zapisane w tym pliku **przed** jakimkolwiek przebiegiem.

**Etap B** — zamrożone `Kp`, `Ki`, `Fmax`, prawo aktualizacji, okno stabilizacji,
warunek startu i dystans pomiaru. **Dokładnie te same** dla wariantu fasetowanego.
Brak dostrajania per wariant. Jeśli fasetowany nie kwalifikuje się, stale saturuje
albo nie utrzymuje prędkości — jego przebieg kończy się wynikiem **negatywnym**.
Limitów nie podnosimy po zobaczeniu rezultatu.

## Bilans pracy i energii

Całkowanie: **reguła prostokąta po stanie POkrokowym**, `dt = 1/60`. Residual
zależy od reguły całkowania, więc reguła jest częścią kontraktu.

```
W_drive_signed   = Σ (F_drive · v) dt
W_drive_positive = Σ max(P_drive, 0) dt
W_drive_negative = Σ min(P_drive, 0) dt
W_drive_absolute = Σ |P_drive| dt
W_downforce      = Σ (F_down · v) dt
W_gravity        = Σ (m·g · v) dt
energy_residual  = W_drive_signed + W_downforce + W_gravity − ΔKE_total
```

`W_drive_positive/negative/absolute` są **obowiązkowe**: praca netto ukryłaby
regulator, który na przemian napędza i hamuje.

`energy_residual` **nie jest nazywany „stratą opony"**. Jest to energia
niewyjaśniona przez jawnie mierzony bilans w tym modelu.

## Wyjście

`q2a_manifest.json` (provenance, SHA źródła/EXE/`box3d.lib`, komenda, dirty
state, nastawy regulatora, parametry fizyki, kryteria), `q2a_samples.csv`
(krok po kroku), `q2a_summary.csv` (jedna linia na przebieg),
`human_readable_stdout.txt`.

Bez flagi `--q2a` sekcje A–G pozostają niezmienione. Żaden plik nie jest cicho
nadpisywany; błąd zapisu jest fatalny. **Nie rejestrujemy Q2A w trwałym
`RAW_MANIFEST.json`.**

Powtórzenia: ≥1 kwalifikacyjny `sphere`, ≥3 właściwe `sphere`, ≥3 fasetowane.
Symulacja jest deterministyczna — jeśli powtórzenia się różnią, rozrzut jest
mierzony i zgłaszany. **Nie tworzymy jednego zbiorczego „score".**
