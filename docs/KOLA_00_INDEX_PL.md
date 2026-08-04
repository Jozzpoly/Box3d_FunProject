# Koła i opony — front door

Status: własny `b3Wheel` jest zintegrowany; `WHEEL-RIGID-01` zamknął
baseline plane, a `WHEEL-SEAM-02A` kontrolowane szwy triangle/mesh. Najbliższa
bramka to pełna poprawność wheel–hull przy ścianach, krawędziach i narożnikach.
Historyczny baseline przed porządkami: `5b92e9c`; rewizję roboczą odczytaj z Git.

> Sztywny manifold nie udaje już rosnącego śladu. Crown 3 mm zachował poprawę
> w zakręcie przy stałym `1,00 all/kolo` i `1,00 nios/kolo`, więc wcześniejszy
> efekt nie był skutkiem speculative candidates. Nadal nie jest to deformacja.

## 1. Zacznij tutaj

1. `CURRENT_STATE_INDEX_PL.md` — stan kodu i priorytet projektu;
2. ten dokument — granice programu;
3. `KOLA_02_ARCHITEKTURA_PL.md` — faktyczna architektura `b3Wheel`;
4. `KOLA_04_PETLA_BADAWCZA_PL.md` — cykl K0–K7 i kolejka;
5. `KOLA_05_PROTOKOL_EKSPERYMENTU_PL.md` — jak powstaje dowód;
6. `KOLA_03_POLITYKA_BOX3D_PL.md` — obowiązkowo przed `src/`/`include/`;
7. `KOLA_FINDINGS.json` — jedyny status findingów.

`KOLA_01_DOWODY_PL.md` jest dużym ledgerem dowodowym. Czytaj konkretną sekcję,
nie odtwarzaj z niego aktualnego planu. Dziennik budowy pierwszego `b3Wheel` i
stare protokoły są w `archive/wheels/` oraz `archive/consolidated_2026-08/`.

## 2. Co już istnieje

- pojedynczy typ `b3Wheel` — bryła obrotowa z profilem i `cornerRadius`;
- normalizacja profilu, upper convex hull i stabilne indeksy feature;
- dedykowane kontakty z plane, triangle, hull, capsule i sphere;
- integracja mesh/heightfield, AABB, raycast i debug draw;
- Wheel Scope, quarter-car, pełny pojazd i zachowane surowe pomiary;
- manifest jawnie posiadanej delty rdzenia Box3D.

To jest wartościowy fundament powierzchni i instrumentu. Nie jest jeszcze
modelem strukturalnej opony.

## 3. Bieżąca kolejka

```text
WHEEL-RIGID-01 — DONE
  strict support: 1 vertex albo 2 końce prawdziwego support segment

WHEEL-SEAM-02A — DONE
  finite triangle face/edge/vertex oraz obciążone szwy mesha

WHEEL-HULL-02B — NEXT
  face polygon clipping, edge/axis SAT i brak phantom corner contacts

WHEEL-SOFT-03
  lokalna podatność A/B przy identycznej geometrii i manifoldzie

WHEEL-STRUCT-04
  decyzja o lokalnych stanach strukturalnych dopiero po wyniku A/B
```

Szczegółowe STOP-gates: `KOLA_04_PETLA_BADAWCZA_PL.md`.

## 4. Reguły twarde

1. Jedna główna zmienna na eksperyment.
2. Masa i bezwładność są zamrożone przy porównaniu kształtów.
3. Liczba punktów kontaktu nie jest automatycznie powierzchnią odcisku.
4. Speculative distance nie jest modelem deformacji.
5. Wynik stendu nie awansuje na pojazd bez transferu co najmniej poziom wyżej.
6. Status findingu zmienia się wyłącznie w `KOLA_FINDINGS.json`.
7. Tabele liczb powstają z zachowanych logów, nie z ręcznego przepisywania.
8. Negatywny i niejednoznaczny wynik zostaje w repo.
9. Feeling i obraz odbiera Jozz; telemetria wyjaśnia mechanizm.
10. Nie wracamy do wielu stockowych colliderów jako substytutu ciągłej opony.
11. Zmiana rdzenia wymaga manifestu, właściciela i polityki Zero-Delta-Off.
12. Nie stroimy progu ani parametrów pobocznych po to, by uzyskać zielony wynik.

## 5. Jedna paczka pracy

Przed kodem zapisz:

```text
ID
hipoteza
jedna badana zmienna
zamrożone parametry
zakres plików
rig i metryki
PASS / FAIL / INCONCLUSIVE
warunek STOP
```

Po pracy zachowaj manifest, raw logs, krótkie podsumowanie, checkpoint i commit.
Nie zaczynaj kolejnej rekurencji przed zamknięciem poprzedniej.

## 6. Narzędzia

- `tools/jozz_wheel_bench/` — wspólny rig headless/visual;
- `tools/evidence/evidence.py` — rejestracja, ekstrakcja, render i kontrola raw;
- `tools/jozz_wheel_bench/check_all.py` — istniejące bramki instrumentu;
- `tools/jv_gate.py` — jedno wejście do profili jakości;
- `tools/jozz_core_delta.py` — własność i zakres zmian rdzenia;
- `samples/validation/` — sondy pojazdu i mapy.

Normalny checkpoint:

```text
python tools/jv_gate.py quick
```

Zamknięcie dokumentacji/infrastruktury:

```text
python tools/jv_gate.py deep
```

Zmiana programu koła z gotowym lokalnym buildem:

```text
python tools/jv_gate.py wheel
```

## 7. Czego ten program nie robi przy okazji

- nie przebudowuje kierownicy, żeby ukryć problem kontaktu;
- nie zmienia zawieszenia razem z oponą;
- nie przenosi kodu JV do JES mechanicznie;
- nie traktuje debug mesha jako authority kolizji;
- nie rozpoczyna strukturalnej opony przed rigid/seam/soft.

## 8. Decyzje Jozza

Zatrzymaj się, gdy wynik wymaga wyboru feelingu, domyślnego zachowania,
realistic/arcade, akceptacji obrazu albo awansu mechanizmu do JES. Problemy
techniczne wewnątrz zatwierdzonego eksperymentu rozwiązuj samodzielnie i
zapisuj jako dowód.
