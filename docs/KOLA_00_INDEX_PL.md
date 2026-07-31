# Koła i opony — front door

Data założenia: 2026-07-25
Branch: `jozz-scan-terrain-f0`
Status programu: **R0.5 — kalibracja instrumentu. Stend v2 istotnie ulepszony,
ale nadal kalibrowany; wyniki `PROVISIONAL`; geometria pozostaje pytaniem
otwartym; implementacja produktowa NIE rozpoczęta**

> **Etap następny przyjęty 2026-07-31: `Q3 — Quarter Car Lab`.** Koło na piaście
> i zawieszeniu, pod masą resorowaną, **napędzane momentem**. Plan wykonawczy
> `Q3-0…Q3-6`: `KOLA_04` §4 (R0.5). Specyfikacja rigu: `KOLA_05` §1.2.
> - nazwa **`Q3`**, wewnątrz drabiny `Q0–Q4` — propozycja „W2" odrzucona,
>   bo `W2` jest już warstwą architektury (`KOLA_02` §4);
> - odstępstwo od drabiny zapisane jawnie: spec mówił „brak napędu",
>   właściciel zdecydował o momencie napędowym; wersja bierna zostaje kontrolą;
> - **`KOLA_05` §1.3 — `suspensionHertz` nie jest częstotliwością resorowania.**
>   Sztywność podąża za masą efektywną więzu (150+44 kg → **34,02 kg**), więc ta
>   sama liczba na dwóch kandydatach o różnej masie nieresorowanej to dwie różne
>   sprężyny. Nowe pytania `U-24…U-26` rozstrzyga sonda `Q3-1` **przed** budową
>   rigu, bo jej wynik może zmienić jego projekt.

> **Rewizja 2026-07-31 (druga) — POWSTAŁ PIERWSZY NOWY SYSTEM KOŁA.**
> Właściciel uchylił zapis kontraktu Q3 „żadnych nowych obwiedni w tym etapie"
> i polecił skończyć działającym kandydatem, nie kolejnym planem.
> - **`torus-N`** — trzecia obwiednia w programie: pierścień N kapsuł o osi
>   równoległej do osi koła, płaska bieżnia i zaokrąglone barki. Powód jest
>   zmierzony: pryzmat nie przejdzie ~42 ścianek (`F-01`) i ma ostre krawędzie,
>   na których skacze normalna kontaktu. Kapsuł można dać 64.
> - **Q3 działa w obu trybach**: `wheel_bench.exe --qc-compare` (macierz
>   kandydat × droga × prędkość) i okno `Quarter Car Scope` na TYM SAMYM rigu.
> - **`F-21`** — `torus-64` wobec dzisiejszego `prism-42` przy 4 m/s: strata
>   **−33%**, `sprung_accel_rms` **−83%**, contact churn **0% wobec 90%**.
> - **`F-22`** — i robi to **mimo o 57% większego tętnienia promienia**. Nie
>   rządzi amplituda odchyłki, tylko obecność ostrych krawędzi.
> - **`F-23`** — cena: **13× CPU** (0,094 ms/krok wobec 0,007). Realny handel.
> - **`F-24`** — przy 13 m/s pojedynczy przebieg **nie jest powtarzalny** na
>   poziomie progu ważności; każda komórka porównania idzie 3× z przesuniętym
>   startem i podaje rozrzut.
> - Werdykt o feelu należy do Jozza i **nie został wydany** (reguła twarda 5).
>   Otwarte: `Q3-5` (wzorzec zachowania) — dopiero po akceptacji w oknie.

> **Rewizja 2026-07-31 (trzecia) — `Quarter Car Scope` staje się WARSZTATEM.**
> Właściciel: kierunek dobry, ale okno „bardzo surowe i niewygodne, część
> kontrolek nie działa poprawnie, zbyt mały wpływ na rig". Przebudowane.
> - **Protokół pomiaru przeniesiony do `jozz_qc_rig.c`** — rozgrzewka, okno,
>   3 powtórzenia i lista kandydatów uruchamiają teraz OBA frontendy. Okno ma
>   przycisk „zmierz jak stend": rig budowany od nowa, liczby identyczne
>   z `--qc-compare` (sprawdzone: churn 89,7 / 70,1 / 8,6 / 0,0 po obu stronach).
> - **Konstrukcja narożnika jako plik `.qc`** — półka w oknie, `--qc-config`
>   i `--qc-config-template` w stendzie, ten sam potrójny strażnik formatu co
>   `.rig` (bramka „format konstrukcji" pilnuje obu).
> - **Strojenie na żywo**: sprężyna, tłumienie, skok, tryb napędu i prędkość
>   zadana zmieniają biegnący rig bez przebudowy. Reszta przebudowuje —
>   i od razu wykonuje rozgrzewkę, więc obraz startuje tam, gdzie stend liczy.
> - **Nie da się już zbudować ślepego zaułka**: N i promień korony poza zakresem
>   budowalnym są wciskane w zakres z jawnym komunikatem, a nieudana budowa
>   zostawia przycisk powrotu zamiast martwego okna.
> - **Przemiatanie jednego parametru** — w oknie i w stendzie
>   (`--qc-sweep`). Pierwszy wynik, który dało: **`F-25`**.
> - **`F-25`** — promień korony **NIE jest kompromisem** na płaskiej płycie:
>   0,04 → 0,19 m daje stratę **795 → 457 W (−43%)**, churn **26,9% → 0%**,
>   przy zwężeniu bieżni z 358 do 58 mm. Obala zapis, który sam wcześniej
>   umieściłem w `jozz_wheel_rig.h`. Cena leży w CPU (**1,7×**), nie w bieżni;
>   a po co jest płaska bieżnia, Q3 zmierzyć nie może — nie stawia siły bocznej.

> **Rewizja 2026-07-31 — instrument dostał pamięć i punkt odniesienia.**
> Nie zmienia to statusu programu ani żadnego wyniku. Zmienia to, co da się
> o instrumencie stwierdzić maszynowo:
> - **`KOLA_01` §8** — trzy nowe ustalenia: `F-15` (narrowphase raz na krok
>   świata), `F-16` (liczba podkroków zmienia twardość kontaktu, nie tylko
>   dokładność), `P-17` (`v_kryt` — kontrakt Q2A jedzie **2.76×** powyżej
>   granicy ciągłego styku i przez cały program nic tego nie sygnalizowało);
> - **blokada zachowania** (`golden/`) — pierwszy w tym programie zapis
>   *zaakceptowanego zachowania*. Do 2026-07-30 istniały wyłącznie bramki
>   spójności; żadna nie potrafiła zauważyć zmiany fizyki, bo zmieniała się
>   po obu stronach porównania naraz;
> - **konstrukcja jest plikiem** (`.rig`) — ten sam plik wczytuje okno i
>   uruchamia stend (`--rig-config`), więc przypadek znaleziony ręcznie da się
>   oddać agentowi bez przepisywania suwaków.

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

Okno wizualne na ten sam rig (poziom `V1v`, `KOLA_04` §3):

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
   nie dowodem — nawet jeśli liczby wyglądają solidnie (`KOLA_04` §5, `U-20`).
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

Rejestry (pytania, findings, failures, patche Box3D) żyją w `KOLA_04` §5–§6.
Nie zakładamy osobnych plików CSV, dopóki liczba wpisów nie przekroczy czytelności.

## Otwarte decyzje właściciela

Zapisane w `KOLA_04` §8. Nic w tym programie nie rusza `src/` przed decyzją
`D-CORE-01`.
