# Koła i opony — front door

Data założenia: 2026-07-25
Branch: `jozz-scan-terrain-f0`, HEAD `959aefb`
Status programu: **R0.5 — kalibracja instrumentu. Stend v2 istotnie ulepszony,
ale nadal kalibrowany; wyniki `PROVISIONAL`; geometria pozostaje pytaniem
otwartym; implementacja produktowa NIE rozpoczęta**

> **Rewizja 2026-07-28 (pełny audyt zewnętrzny stendu v2, zweryfikowany wobec
> kodu i przyjęty).** Poprzednia rewizja z 2026-07-27 była przedwczesna i
> zostaje **osłabiona**:
> - „instrument naprawiony" → **v2 istotnie ulepszony, nadal kalibrowany**
>   (dwanaście udokumentowanych wad, `KOLA_01` §7.9);
> - **żadna rodzina nie została odrzucona pomiarem.** `P-04` i `P-05` schodzą
>   do `MODEL FACT + BENCH WARNING` i `obalony suwak, nie rodzina`;
> - test nazwany „realnym obciążeniem narożnika" był **wolnym kołem pod stałym
>   dociskiem** o poziomej bezwładności 44 kg — nie quarter-carem;
> - okno pomiarowe poprzedzały **2 s ukrytego rozbiegu**, więc metryki toczenia
>   opisują koło, które już prawie stoi;
> - `P-10` (asymetria kierunków) → `STRONG DESIGN INFERENCE` z mierzalną
>   podstawą: częstotliwość przejść między cechami różni się o ~2 rzędy
>   wielkości.
>
> Nowy dokument: **`KOLA_05`** — protokół stendu v2.1 (rigi Q0–Q4, słownik
> metryk, manifest dowodowy). Obowiązuje bramka `GATE-A` (`KOLA_04` §0.1):
> świeży wynik idzie do `PROVISIONAL FINDINGS`, nie do praw.
> Żadna rodzina nie jest wybrana ani zamknięta.

> **Rewizja 2026-07-26 (decyzja właściciela + audyt własny).** Statusy kilku
> wniosków z pierwszej rundy zostały **osłabione**, a elipsoida **cofnięta**
> z roli kandydata wiodącego do jednego z kilku badanych kandydatów.
> Szczegóły: `KOLA_01` §5.6 (wady protokołu pomiarowego), `KOLA_01` §6
> (zawężony zakres wniosku), `KOLA_02` §4.1 (limit geometryczny elipsoidy),
> `KOLA_03` §5.1 (korekta klasy patcha z `I` na `X`).
> Żadna rodzina rozwiązań nie jest wybrana ani odrzucona architektonicznie.

To jest jedyne wejście do prac nad kołami i oponami. Nie zaczynaj od żadnego innego pliku.

## Kolejność czytania

| Kiedy | Plik | Czego jest jedynym właścicielem |
|---|---|---|
| zawsze, pierwsze | `KOLA_01_DOWODY_PL.md` | zmierzony stan, fakty o silniku, obalone hipotezy |
| przed projektowaniem | `KOLA_02_ARCHITEKTURA_PL.md` | kontrakt `WheelSpec`, warstwy, drabina zdolności F0–F4 |
| przed dotknięciem `src/` | `KOLA_03_POLITYKA_BOX3D_PL.md` | zasady forka, klasy patchy, reguła Zero-Delta-Off |
| przed każdą iteracją | `KOLA_04_PETLA_BADAWCZA_PL.md` | poziomy L0–L5, reguła powrotu, iteracje R0–R8, bramka `GATE-A`, rejestry |
| przed każdym pomiarem | `KOLA_05_PROTOKOL_STENDU_V21_PL.md` | rigi Q0–Q4, słownik metryk, rejestr confoundów, manifest dowodowy |
| przed cytowaniem `F-xx`/`P-xx` | `KOLA_FINDINGS.json` | **jedyny** aktualny status każdego findingu (maszynowo sprawdzany) |

Zewnętrzny pakiet metodyczny („Wheel & Tire Experimental Proving Ground v1.2"
oraz „Second Brain") jest **źródłem metody, nie planem**. Rozliczenie z nim:
`KOLA_04` §7. Nie odtwarzamy jego rytuału adaptacyjnego BA0–BA8 — jest odrobiony
i udokumentowany w `KOLA_01`.

## Reguły twarde tego programu

1. **Nie porównuj kształtów kolizji bez zamrożonej masy i bezwładności.**
   Dowód, że to unieważnia wyniki: `KOLA_01` §3.
2. **Wynik z izolowanego stendu nie jest wynikiem dla pojazdu.** Ranking
   reprezentacji zmienia się między stendem a autem — zmierzone, `KOLA_01` §5.3.
3. **Jedna zmienna główna na eksperyment.** Reszta zamrożona i wypisana.
4. **Każdy patch w `src/`/`include/` musi być bit-identyczny ze stockiem przy
   wyłączonej funkcji** (Zero-Delta-Off, `KOLA_03` §4).
5. **Werdykt o feelingu należy wyłącznie do Jozza.** Telemetria mówi *dlaczego*,
   nie *czy dobrze*.
6. **Nie kasuj wyników negatywnych.** Są trwałym produktem.
7. **Reguła „stend ≠ pojazd" dotyczy także nagłówka tego pliku.** Wynik z jednego
   rigu nie awansuje na prawo bez transferu o poziom wyżej (`KOLA_05` §1).
8. **Świeży wynik idzie do `PROVISIONAL`, nie do praw.** Bramka `GATE-A`,
   `KOLA_04` §0.1.
9. **Liczb pomiarowych się nie przepisuje — generuje się je.** Każda tabela
   danych w `KOLA_01` stoi w bloku `<!-- EVIDENCE:BEGIN … -->` wypełnianym
   z surowego logu. Ręczna edycja wewnątrz bloku wywala `check`.
   Powód: 2026-07-29 wykryto, że tabela CPU zawierała dziesięć liczb, z których
   **żadna** nie występowała w zachowanym przebiegu (`W-16`).
10. **Status findingu żyje w `KOLA_FINDINGS.json`, nie w prozie.** Dokumenty
    opisują finding; statusu mu nie nadają. Jeden finding = jeden status.
11. **Fakt o silniku cytujemy z pliku implementacji, nigdy z komentarza
    nagłówka.** Komentarz `types.h:407` o `rollingResistance` jest nieaktualny
    i przez cztery dni był w tym programie „faktem" (`W-15`, `P-14`).

## Narzędzia

```
tools/jozz_wheel_bench/     izolowany stend badawczy (linkuje box3d.lib, nie dotyka repo)
  wheel_bench.c             PROTOKOŁY eksperymentów A-G i Q2A, opisane w KOLA_01 (v2)
  jozz_wheel_rig.c/.h       WSPÓLNY rig: świat, ciało, materiał, masa, regulator.
                            Ten sam plik kompilują stend i target `samples`.
  build.bat                 kompilacja MSVC, wymaga zbudowanego build/src/Release/box3d.lib
  evidence/                 surowe wydruki przebiegów (nie kasować)
```

Okno wizualne na ten sam rig (poziom `V1v`, `KOLA_04` §3):

```bash
build\bin\Debug\samples.exe --sample-name "Wheel Scope"
```

Instrukcja dla właściciela: `tools/jozz_wheel_bench/WHEEL_SCOPE_OWNER_SESSION_PL.md`.
Dowód, że okno pokazuje tę samą fizykę (600 kroków bajt w bajt):

```bash
python tools\jozz_wheel_bench\check_visual_equivalence.py
```

Stend v2 ma znane wady (`KOLA_01` §7.9). Protokół v2.1: `KOLA_05`.

Uruchomienie stendu:

```bash
tools/jozz_wheel_bench/build.bat && tools/jozz_wheel_bench/wheel_bench.exe
```

Łańcuch dowodowy — **uruchamiać po każdej zmianie dokumentów lub przebiegu**:

```bash
python tools/evidence/evidence.py check
```

`extract` (surowy log → `summary.json`), `render` (→ bloki w dokumentach),
`check` (weryfikacja: bloki == regeneracja, hash surowych logów, rejestr
statusów, tabele danych poza blokiem). `check` zwraca kod ≠ 0 przy błędzie.

Walidator produktowy (ZAWSZE z roota repo):

```bash
build/bin/Debug/jozz_vehicle_validation.exe
```

## Stan rejestrów

Rejestry (pytania, findings, failures, patche Box3D) żyją w `KOLA_04` §5–§6.
Nie zakładamy osobnych plików CSV, dopóki liczba wpisów nie przekroczy czytelności.

## Otwarte decyzje właściciela

Zapisane w `KOLA_04` §8. Nic w tym programie nie rusza `src/` przed decyzją
`D-CORE-01`.
