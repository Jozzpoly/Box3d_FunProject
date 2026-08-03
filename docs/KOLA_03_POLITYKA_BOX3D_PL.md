# Polityka modyfikacji Box3D

Data: 2026-07-25 | Podstawa: decyzja właściciela z 2026-07-24 („możemy modyfikować
Box3D dowolnie, ale fork musi dać się aktualizować")
Status: **OBOWIĄZUJE** od 2026-08-03 (`D-CORE-01` rozstrzygnięte, `O-6`).

> Ten dokument stał dziesięć dni jako „propozycja do zatwierdzenia", mimo że
> zgoda właściciela z 2026-07-24 (cytat dwie linijki wyżej) była już udzielona.
> Nikt na nic nie czekał poza mną. `D-CORE-01` domyka się **własnym warunkiem
> wyjątku z §8**: „gdy zdolność jest dowiedzenie nieosiągalna inaczej
> (warunek C1), utrzymywalność ustępuje". `F-31` (sztywna bryła nie daje
> odcisku) i `F-36` (żadna bryła poza sferą nie potrafi się toczyć) są tym
> dowodem — zdolności nie da się uzyskać ze słownika kształtów stockowego
> Box3D. Patch klasy `X` idzie za jawną zgodą właściciela, która jest.

Właściciel jawnie odmówił podawania arbitralnych liczb („10 patchy, 2 invasive").
Miał rację — to była zła prośba z mojej strony. Poniżej polityka techniczna,
którą proponuję ja, wraz z tym, co właściciel faktycznie ma rozstrzygnąć (§8).

---

## 1. Punkt wyjścia jest idealny i nie powtórzy się

```
git diff <merge-base upstream/main>..HEAD -- src include   ->   PUSTE
```

Zero linii delty w silniku po 200 commitach projektu. Dyscyplinę forka ustawiamy
**przed pierwszym patchem**, nie po dziesiątym.

---

## 2. Reguła wejścia w core (kiedy patch jest w ogóle uprawniony)

Patch w `src/` lub `include/` jest uprawniony **wyłącznie wtedy**, gdy spełnione
są jednocześnie wszystkie cztery warunki:

```
C1  DOWIEDZIONA NIEOSIAGALNOSC KONKRETNEJ ZDOLNOSCI
    Nazwana, waska zdolnosc jest niemozliwa przez publiczne API - z pomiarem
    lub argumentem z kodu, nie z przypuszczenia.
    POPRAWNIE: "publiczne API nie pozwala zarejestrowac nowej pary
               shape/manifold".
    BLEDNIE:   "publiczne API nie potrafi zrobic opony".
C2  NAJTANSZY SPOSROD JAWNIE PORÓWNANYCH WARIANTOW
    Wybrany wariant jest najtanszy sposrod PORÓWNANYCH kandydatow
    spelniajacych jawnie zapisany kontrakt. NIE wymaga dowodu o wszystkich
    wyobrazalnych rozwiazaniach; wymaga listy tego, co porownano.
C3  ZERO-DELTA-OFF
    Przy wylaczonej funkcji zachowanie jest identyczne ze stockiem.
    ZDO-S (semantyka) jest obowiazkowe. ZDO-B (bitowo) - patrz §4
    i otwarta decyzja D-CORE-02.
C4  ZMIERZONY KOSZT AKTUALIZACJI
    Wykonana proba aktualizacji (§6) z wynikiem w ledgerze, na SHA
    upstreamu sprawdzonym W MOMENCIE proby - nie na SHA z dokumentu.
```

> **Uwaga (korekta 2026-07-28).** Wcześniejszy przykład „S4, elipsoida: C1
> spełnione" został **wycofany**. Nie wpisujemy kandydatów na kształt jako
> sugerowanych identyfikatorów patcha przed przejściem laboratorium manifoldu
> (V1b). Pierwszym poprawnie sformułowanym C1 w tym programie będzie zdanie
> o konkretnym braku w API, a nie o rodzinie brył.

---

## 3. Klasy patchy — po KOSZCIE MERGE, nie po temacie

Klasyfikacja pakietu (`U`/`E`/`J`) mówi o **przeznaczeniu**. Ta oś mówi o
**cenie utrzymania** i jest ważniejsza przy planowaniu.

| Klasa | Co to jest | Koszt aktualizacji | Reguła |
|---|---|---|---|
| **A — ADDITIVE** | nowy plik `src/b3_jozz_*.c/h`, którego upstream nigdy nie dotknie | zerowy **dopóki** nie zależy od zmiennych struktur upstreamu | preferowana domyślnie |
| **H — HOOK** | 1–3 linie wywołania w funkcji upstreamu do naszego pliku A | minimalny mechanicznie, **ale koszt semantyczny zależy od miejsca** | dozwolona po wskazaniu ścieżki i wpływu |
| **I — INLINE** | logika wewnątrz funkcji upstreamu | przegląd semantyczny przy każdym update | wymaga uzasadnienia w ledgerze |
| **X — INVASIVE** | zmiana układu danych, enumu, formatu serializacji, tożsamości kontaktu | przeprojektowanie patcha | wymaga jawnej decyzji właściciela |

**Reguła projektowa:** każdy patch zaczyna życie jako próba bycia `A`. Zejście
o klasę niżej wymaga zdania w ledgerze, dlaczego wyższa klasa nie wystarczyła.

**Klasy `A` i `H` nie są darmowe z definicji** (korekta 2026-07-28). Osobny plik
może zależeć od struktur, które upstream zmienia; jedna linia w gorącej ścieżce
może mieć ogromny koszt semantyczny. Każda klasyfikacja musi wymienić: zakres
danych, czy to gorąca ścieżka, wątkowanie, determinizm, ABI, serializację
i własność pamięci.

Znane pułapki podnoszące klasę do `X` w naszym obszarze:

- dodanie wartości do `b3ShapeType` (enum alfabetyczny, przesuwa indeksy nagrań);
- podniesienie limitu hulla z `uint8` na `uint16` — pociąga za sobą `b3SATCache`,
  `b3FeaturePair` i pakowanie `featureId`, czyli **tożsamość kontaktu i warm start**;
- każda zmiana układu `b3Manifold` lub kolejności operacji w `contact_solver.c`.

---

## 4. Zero-Delta-Off — twardy strażnik semantyki

Najlepszy pojedynczy pomysł z krytyki „Second Brain": utrzymywalność zależy nie
od sentineli, tylko od **semantycznego sprzężenia patcha z upstreamem**.
Formalizacja:

Rozdzielone na dwa poziomy (korekta 2026-07-26 — wcześniej mylone):

```
ZDO-S  SEMANTYCZNY: przy wylaczonej funkcji patch nie zmienia SWIADOMIE
       zachowania stockowego. To jest INTENCJA projektowa.
ZDO-B  BITOWY: przy kontrolowanym buildzie, tych samych flagach, tej samej
       liczbie workerow i tym samym seedzie hashe stanu sa identyczne.
       To jest TEST, ktory ZDO-S musi przejsc - nie wlasnosc "z definicji".
```

> Każdy patch core musi mieć jawny wyłącznik. **ZDO-S jest obowiązkowe.**
> Czy **ZDO-B** jest warunkiem koniecznym każdego patcha — to otwarta decyzja
> `D-CORE-02` (§8), a nie fakt zapisany w niezmiennikach.

**Dlaczego to nie może być zapisane jako niezmiennik, dopóki właściciel nie
zdecyduje.** Dla patcha klasy `X` sama obecność nieużywanego pola, enumu albo
zmiana układu danych może zmienić pamięć, snapshot albo ABI **nawet przy
funkcji OFF**. Nie każdy wartościowy patch przejdzie dosłowne porównanie binarne
całego programu. Dlatego ZDO-B rozbijamy na cztery osobne pytania i wymagamy
odpowiedzi na każde:

```
1. identycznosc ZACHOWANIA stockowych scen   (zawsze wymagane)
2. identycznosc SNAPSHOTOW                   (wymagane, jesli ruszamy serializacje)
3. identycznosc ABI                          (wymagane, jesli ruszamy uklad danych)
4. identycznosc PLIKU WYKONYWALNEGO          (najostrzejsze; czesto nieosiagalne)
```

Środowisko testu ZDO-B musi być jawnie zamrożone: kompilator i flagi, liczba
workerów, seed, scenariusz, platforma, układ danych.

Żaden patch nie jest „bit-identyczny z definicji". Zmiana rozmiaru struktury
przenoszonej przez `memcpy`/`sizeof` może zmienić układ pamięci, wyrównanie
i kolejność operacji nawet przy neutralnej wartości pola — dlatego ZDO-B jest
pomiarem, nie argumentem.

Maszynowa weryfikacja (do zbudowania razem z pierwszym patchem):

```
1. zbuduj wariant STOCK  (patch fizycznie usunięty / gałąź bez patcha)
2. zbuduj wariant PATCHED (patch obecny, funkcja OFF)
3. przepuść zestaw scen determinizmu przez oba
4. porównaj hasze stanu świata krok po kroku
5. dowolna różnica = patch odrzucony, nie „do zbadania"
```

Repo ma już `test_determinism.c` i haszowanie stanu w `world_snapshot.c` —
infrastruktura istnieje. To jest tania i bardzo mocna gwarancja: sprawia, że
włączenie naszych funkcji jest zawsze **opcją**, a nie rozwidleniem silnika.

---

## 5. Sentinele i ledger

Każda zmiana w `src/`/`include/` opakowana:

```c
// >>> JOZZ-PATCH B3X-XXXXXX-001 (klasa <A|H|I|X>, <U|E|J>) : <jedno zdanie>
//     powod: <konkretny brak w publicznym API - warunek C1>
//     wylacznik: <warunek, ktory przywraca sciezke stockowa>
//     ledger: docs/KOLA_03_POLITYKA_BOX3D_PL.md §5.1
... nasze linie ...
// <<< JOZZ-PATCH B3X-XXXXXX-001
```

> Szablon jest celowo bez nazwy kandydata. Wcześniejsza wersja pokazywała tu
> `B3X-ELLIPSOID-001 (klasa I)`, co było **sprzeczne z ledgerem §5.1**, gdzie
> ten sam patch ma klasę `X`. Przykład jest częścią polityki i musi być zgodny,
> a nazwy kandydatów nie trafiają do polityki przed przejściem V1b.

Do walidatora dopinamy strażnika (`tools/jozz_core_delta`), który:

```
1. liczy `git diff <upstream_base>..HEAD -- src include`
2. FAIL, jeśli jakikolwiek hunk leży poza parą sentineli
3. FAIL, jeśli jakikolwiek patch_id nie ma wiersza w ledgerze
4. FAIL, jeśli wiersz ledgeru nie ma wypełnionego update_dry_run
5. wypisuje AKTUALNY KOSZT FORKA: liczba patchy per klasa + linie per plik
```

Bez maszynowego strażnika dyscyplina forka rozpada się w trzy tygodnie — to jest
ta sama lekcja, która stoi w programie JES („każda reguła potrzebuje strażnika").

### 5.1 Ledger (pusty — pierwszy wiersz powstanie dopiero po `D-CORE-01`)

| patch_id | klasa | pytanie | upstream_base | pliki | wyłącznik | Zero-Delta-Off | update_dry_run | status |
|---|---|---|---|---|---|---|---|---|
| _(brak)_ | | | | | | | | |

Kandydaci zgłoszeni, niezatwierdzeni (klasy SKORYGOWANE 2026-07-26):

```
B3X-ELLIPSOID-001  klasa X (bylo bledne "I")  niejednorodna skala sfery
     powod korekty: src/world_snapshot.c:489 zapisuje `b3Sphere` BAJTOWO
     (`b3SnapW_Bytes(buf, &src->sphere, sizeof(b3Sphere))`). Dodanie pola do
     tej struktury zmienia publiczne ABI ORAZ format snapshotu -> wedlug
     wlasnej definicji z §3 to jest klasa X, nie I.
B3X-SWEPTDISK-001  klasa X   analityczny profil disk+odcinek+promien (S4b)
B3X-TIREFRAME-001  klasa I   wlasna baza styczna + elipsa tarcia (T1)
B3X-CONTACTOBS-001 WYCOFANY  telemetria manifoldu NIE wymaga patcha:
     `b3Body_GetContactData` (body.c:464) zwraca pelne `contact->manifolds`
     z featureId, persisted, normalImpulse i totalNormalImpulse.
```

**Reguła dodana po audycie:** zanim jakikolwiek kandydat trafi do ledgeru,
trzeba nazwać **konkretne brakujące pole albo zdarzenie** publicznego API.
„Przydałoby się" nie jest uzasadnieniem patcha.

---

## 6. Rytuał aktualizacji upstreamu

Aktualizacja jest **eksperymentem utrzymaniowym**, nie sprzątaniem. Ma własny
pakiet dowodowy i własny wynik.

```
1. osobny worktree (nigdy nie na branchu roboczym)
2. merge/rebase na dokladny SHA upstreamu; zapisz SHA
3. build + test.exe + jozz_vehicle_validation.exe
4. stend tools/jozz_wheel_bench - PRZED i PO, ten sam wydruk
5. przejazd Jozza - werdykt feelingowy
6. wynik: CLEAN | MECHANICAL_CONFLICT | SEMANTIC_REVIEW | PATCH_REDESIGN
7. wpis do ledgeru przy KAZDYM patchu, ktorego dotyczylo
```

**Zastrzeżenie (Second Brain, przyjęte):** brak konfliktu tekstowego **nie
dowodzi** braku zmiany zachowania. Punkty 4 i 5 nie są opcjonalne. Zmiany
upstreamu w `triangle_manifold.c`, `convex_manifold.c` i `contact_solver.c`
mogą zmienić zachowanie koła bez jednego konfliktu w merge'u.

---

## 7. Raportowanie kosztu (co właściciel dostaje zamiast liczb, które miałby zgadywać)

Po każdej iteracji, jedną tabelką:

```
KOSZT FORKA na dziś
  patche:        A=n  H=n  I=n  X=n
  linie delty:   src/ n  include/ n
  pliki dotknięte: lista
  ostatnia próba aktualizacji: SHA, wynik, czas trwania
  prognoza kosztu następnej aktualizacji: LOW / MEDIUM / HIGH + dlaczego
```

To zamienia „czy fork nam się nie rozjeżdża?" z niepokoju w liczbę.

---

## 8. Co faktycznie rozstrzyga właściciel

Nie liczby patchy. Trzy pytania kierunkowe:

**`D-CORE-01` — Czy utrzymywalność upstreamu ma pierwszeństwo przed szybszym,
lecz bardziej inwazyjnym rozwiązaniem, o ile nie ogranicza to fundamentalnie
wizji?**
Moja rekomendacja: **TAK**, z jednym wyjątkiem — gdy zdolność jest dowiedzenie
nieosiągalna inaczej (warunek C1), utrzymywalność ustępuje, ale patch musi
spełnić C2–C4. W praktyce: preferujemy klasę `A`/`H`, `I` za uzasadnieniem,
`X` tylko za jawną zgodą.

**`D-CORE-02` — Czy przyjmujemy Zero-Delta-Off jako warunek konieczny?**
Rekomendacja: **TAK dla ZDO-S** (semantyka, bezwarunkowo) i **TAK dla ZDO-B
w zakresie punktów 1–3 z §4** (zachowanie, snapshoty, ABI). Punkt 4
(identyczność pliku wykonywalnego) traktujemy jako cel, nie jako warunek
odrzucający — inaczej wykluczymy z góry każdy patch klasy `X`, którego
właściciel może chcieć świadomie.

**`D-CORE-03` — Czy aktualizujemy się do `upstream/main` teraz?**
Rekomendacja: **TAK, jako pierwszy eksperyment programu** — dziś kosztuje zero
delty, a upstream zmienił dokładnie te pliki, które będziemy patchować.
Bramka: walidator + stend przed/po + przejazd Jozza.
**SHA celu (`d421e45` w wersji z 2026-07-25) trzeba sprawdzić ponownie
w momencie wykonywania próby** — nie traktować go jako wiecznie aktualnego.
