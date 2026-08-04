# Koła i opony — front door

Data założenia: 2026-07-25
Ostatnia korekta front dooru: 2026-08-04
Branch bazowy: `jozz-scan-terrain-f0` @ `5b92e9c`
Status programu: **własny `b3Wheel` jest zaimplementowany i zintegrowany; najbliższa
bramka to oddzielenie geometrii, topologii manifoldu i lokalnej podatności.**

> **Rewizja 2026-08-04 — implementacja wyprzedziła stary front door.**
> Commity `9800af9…5b92e9c` dodały do Box3D własny typ `b3Wheel`, kontakt z
> płaszczyzną/hullem/trójkątem/kapsułą/sferą, mesh/heightfield, raycast, debug
> draw i profil bieżni. Historyczny handoff implementacyjny trafił do
> `archive/wheels/KOLA_PRZEKAZANIE_KOLIDER_KOLA_PL.md`.
>
> **Krytyczna korekta interpretacji:** obecny manifold płaszczyzny wybiera
> wierzchołki profilu znajdujące się w `B3_SPECULATIVE_DISTANCE`. Zwiększenie
> liczby punktów przy 3 mm crown może więc być efektem speculative candidates,
> nie fizycznej deformacji. Do czasu strict-manifold wynik „ślad rośnie z
> obciążeniem” ma status hipotezy/surrogate, nie prawa.
>
> **Następna kolejność:** strict rigid support → seam/edge correctness → A/B
> lokalnej softness przy identycznych punktach → dopiero decyzja o strukturze
> opony. Bieżące streszczenie: `CURRENT_STATE_INDEX_PL.md` §3.

Historyczny dziennik rewizji z okresu budowy stendu, torusa i pierwszego `b3Wheel` został zachowany w
`archive/wheels/KOLA_00_DZIENNIK_REWIZJI_2026-07-26_2026-08-03_PL.md`.
Nie jest częścią bieżącej instrukcji.

To jest jedyne wejście do prac nad kołami i oponami. Nie zaczynaj od żadnego innego pliku.

## Zaczynasz tu

1. Przeczytaj `CURRENT_STATE_INDEX_PL.md` §1–4 — to bieżący stan kodu i
   najbliższy eksperyment.
2. Następnie wróć tutaj do reguł twardych.
3. Przy zmianie rdzenia przeczytaj `KOLA_03_POLITYKA_BOX3D_PL.md`.
4. Przy pomiarze użyj `KOLA_05_PROTOKOL_STENDU_V21_PL.md` i maszynowego
   `KOLA_FINDINGS.json`.

Handoff, który doprowadził do implementacji pierwszego `b3Wheel`, jest historią:
`archive/wheels/KOLA_PRZEKAZANIE_KOLIDER_KOLA_PL.md`. Nie jest już planem do
wykonania.

## Kolejność czytania

| Kiedy | Plik | Czego jest jedynym właścicielem |
|---|---|---|
| zawsze, pierwsze | `KOLA_01_DOWODY_PL.md` | zmierzony stan, fakty o silniku, obalone hipotezy |
| przed projektowaniem | `KOLA_02_ARCHITEKTURA_PL.md` | kontrakt `WheelSpec`, warstwy, drabina zdolności F0–F4 |
| przed dotknięciem `src/` | `KOLA_03_POLITYKA_BOX3D_PL.md` | zasady forka, klasy patchy, reguła Zero-Delta-Off |
| przed każdą iteracją | `KOLA_04_PETLA_BADAWCZA_PL.md` | cykl K0–K7, poziomy walidacji, bieżąca kolejka eksperymentów |
| przed każdym pomiarem | `KOLA_05_PROTOKOL_STENDU_V21_PL.md` | rigi Q0–Q4, słownik metryk, rejestr confoundów, manifest dowodowy |
| przed cytowaniem `F-xx`/`P-xx` | `KOLA_FINDINGS.json` | **jedyny** aktualny status każdego findingu (maszynowo sprawdzany) |

Zewnętrzny pakiet metodyczny („Wheel & Tire Experimental Proving Ground v1.2"
oraz „Second Brain") jest **źródłem metody, nie planem**. Rozliczenie z nim:
archiwalny `KOLA_04` §7. Nie odtwarzamy jego rytuału adaptacyjnego BA0–BA8 —
jest odrobiony i udokumentowany w `KOLA_01`.

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
8. **Świeży wynik idzie do `PROVISIONAL`, nie do praw.** Audyt przeciwny i
   awans statusu opisuje `KOLA_04` §2 (`K5–K6`).
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
  jozz_wheel_rig.c/.h       WSPÓLNY rig: świat, ciało, materiał, masa, regulator,
                            JozzRigConfig i format pliku konstrukcji.
                            Ten sam plik kompilują stend i target `samples`.
  build.bat                 kompilacja MSVC, wymaga zbudowanego build/src/Release/box3d.lib
  golden/                   WZORZEC ZACHOWANIA rigu, 600 kroków, oba warianty.
                            Wygenerowany i zweryfikowany wobec stendu zbudowanego
                            ze źródeł `f9576c3` — zapis zaakceptowanego zachowania,
                            nie zdjęcie przypadkowego stanu drzewa.
  evidence/                 surowe wydruki przebiegów (nie kasować)
```

Okno wizualne na ten sam rig (poziom `V1v`, `KOLA_04` §2 `K4`):

```bash
build\bin\Release\samples.exe --sample-name "Wheel Scope"
```

Instrukcja dla właściciela: `tools/jozz_wheel_bench/WHEEL_SCOPE_OWNER_SESSION_PL.md`.

Uruchomienie stendu:

```bash
tools/jozz_wheel_bench/build.bat && tools/jozz_wheel_bench/wheel_bench.exe
```

### Konstrukcja jako plik

Okno zapisuje bieżącą konstrukcję (klawisz `S` lub panel „Półka konstrukcji")
do `wheel_scope_bench/<nazwa>.rig` — czytelny tekst `klucz wartość`, komentarz
z datą, krokiem i klasą sesji. **Ten sam plik uruchamia stend:**

```bash
tools/jozz_wheel_bench/wheel_bench.exe --rig-trace out.csv --rig-config plik.rig
```

Brakujący klucz = wartość domyślna; **nieznany klucz = błąd** (literówka nie może
uchodzić za brak). Plik zawierający samo `format 1` odtwarza kontrakt Q2A co do
bitu — sprawdzone wobec wzorca. Szablon z kompletem kluczy:

```bash
tools/jozz_wheel_bench/wheel_bench.exe --rig-config-template szablon.rig
```

### Bramki — jedno wejście

```bash
python tools/jozz_wheel_bench/check_all.py
```

Uruchamia po kolei: format konstrukcji → blokada zachowania → ekwiwalencja
okno/stend (w tym: ten sam plik `.rig` po obu stronach) → integralność findings.
Nie buduje: bramki same odmówią pracy, gdy binarka jest starsza od swoich źródeł.

Zielone znaczy **„nic się nie zmieniło i dane są spójne"** — nie „fizyka jest
poprawna". Czerwona blokada zachowania rozróżnia **zmianę opisu** przebiegu od
**zmiany trajektorii** i nazywa pierwszy rozbieżny krok.

Świadoma zmiana eksperymentu aktualizuje wzorzec osobno i **nigdy w jednym
commicie z funkcją**, żeby diff wzorca dał się czytać jako zmiana eksperymentu:

```bash
python tools/jozz_wheel_bench/check_behaviour_lock.py --update
```

Sam łańcuch dowodowy (dokumenty i przebiegi) osobno:

```bash
python tools/evidence/evidence.py check
```

### Reguły pracy — dlaczego każda z nich istnieje

1. **`check_all.py` przed sesją i po sesji.** Przed — żeby wiedzieć, od czego
   startujesz; po — żeby wiedzieć, co zmieniłeś. Bez „przed" każdy czerwony
   wynik jest podejrzany o to, że był tam wcześniej.
2. **Aktualizacja wzorca (`--update`) nigdy w jednym commicie z funkcją.**
   Diff wzorca ma się czytać jako *zmiana eksperymentu*. Wmieszany w zmianę
   kodu przestaje cokolwiek znaczyć.
3. **Bramka, której nie widziałeś na czerwono, nie jest bramką.** Każda nowa
   bramka w tym katalogu została celowo zepsuta i przywrócona, a sposób psucia
   jest opisany w jej nagłówku. Bramka bez udowodnionej porażki to deklaracja.
4. **Zielone znaczy „nic się nie zmieniło", nie „jest dobrze".** Bramki mierzą
   niezmienność i spójność. Poprawności fizyki nie sprawdza żadna z nich
   i sprawdzić nie może — ten osąd należy do `GATE-A` i do Jozza.
5. **Liczba, która nie ma pliku, z którego powstała, nie jest wynikiem.**
   Każdy przebieg niesie `JozzRig_ConfigDigest`; każda konstrukcja da się
   zapisać jako `.rig`. Diagnostyka z uprzęży spoza repo jest **hipotezą**,
   nie dowodem — nawet jeśli liczby wyglądają solidnie (`KOLA_04` §2 `K4–K6`).
6. **Jedna misja na sesję.** Wykryty przy okazji problem zapisuje się jako
   pytanie w rejestrze, a nie naprawia w biegu — inaczej diff przestaje
   odpowiadać na pytanie „co ta zmiana robi".
7. **Katalog tymczasowy nie jest pamięcią.** Na koniec sesji każdy artefakt
   albo trafia do repo, albo ginie jawnie. Trzeciej opcji nie ma.

`extract` (surowy log → `summary.json`), `render` (→ bloki w dokumentach),
`check` (weryfikacja: bloki == regeneracja, hash surowych logów, rejestr
statusów, tabele danych poza blokiem). `check` zwraca kod ≠ 0 przy błędzie.

Walidator produktowy (ZAWSZE z roota repo):

```bash
build/bin/Debug/jozz_vehicle_validation.exe
```

## Stan rejestrów

- statusy findings: `KOLA_FINDINGS.json`;
- surowe dowody: `tools/jozz_wheel_bench/evidence/`;
- pytania i dług techniczny: `TECH_DEBT_PL.md`;
- patche rdzenia: `JOZZ_CORE_PATCHES.json`;
- historia iteracji i zamkniętych pytań: `archive/wheels/`.

## Otwarte decyzje właściciela

Żadna decyzja właściciela nie blokuje cykli `WHEEL-RIGID-01` ani
`WHEEL-SEAM-02`. Jozz rozstrzyga feeling na V3 oraz decyzję o wejściu w oponę
strukturalną po wynikach `WHEEL-SOFT-03`; agent odpowiada za przygotowanie
uczciwego eksperymentu, nie za przerzucanie tej odpowiedzialności na właściciela.
