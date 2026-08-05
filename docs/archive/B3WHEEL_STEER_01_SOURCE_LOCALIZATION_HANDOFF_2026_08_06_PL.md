# Handoff — B3WHEEL-STEER-01: lokalizacja źródła antycentrowania

Data: 2026-08-06
Projekt: natywny JV / `Jozzpoly/Box3d_FunProject`
Branch rozwojowy: `recovery/jv-reconstruction`
Status etapu: reprodukcja i pierwsza lokalizacja źródła zakończone; brak poprawki fizyki

## 1. Misja projektu

JV ma rozwijać fizycznie wiarygodny pojazd i własny model koła/opony w Box3D.
Powrót kierownicy nie może być efektem ukrytej sprężyny, serva do zera, momentu
zależnego od puszczenia wejścia ani innego „arcade assist”. Ma wynikać z geometrii
zwrotnicy, caster/trail/KPI/scrub, kontaktu opony, ruchu pojazdu, obciążenia,
podatności i rzeczywistych oporów układu.

Ręczna ocena feelu należy do Jozza. Telemetria ma wyjaśniać mechanizm i blokować
fałszywe poprawki, nie zastępować jazdy.

## 2. Obowiązkowa kolejność czytania

1. `README_FOR_AGENTS.md`
2. `docs/CURRENT_STATE_INDEX_PL.md`
3. `docs/CHECKPOINTS_PL.md`
4. `docs/TECH_DEBT_PL.md`
5. `docs/KOLA_00_INDEX_PL.md`
6. ten dokument
7. `tools/research/experiments/B3WHEEL-STEER-01C.json`
8. `tools/research/native/steering_release_probe.cpp`
9. najnowszy manifest pod `tools/research/evidence/B3WHEEL-STEER-01/`

Nie odtwarzaj bieżącego stanu ze starych raportów. Ten dokument jest raportem
zamkniętego etapu; aktywny następny krok pozostaje w `CURRENT_STATE_INDEX_PL.md`.

## 3. Stan zabezpieczony przed tym etapem

Opublikowany branch zawierał:

```text
4b6eb95 test(vehicle): add physical steering release evidence probe
5101971 evidence(vehicle): lock steering release defect traces
```

Pierwszy run udowodnił deterministycznie, że:

- po jednoczesnym puszczeniu gazu i kierownicy `b3Wheel` wraca przy `±0,40`,
  lecz nie wraca przy `±0,50`;
- sfera jest asymetryczna kierunkowo;
- torus wraca obustronnie, ale nie jest przez to kandydatem shippingowym;
- wynik jest known-defect lockiem, nie poprawką.

## 4. Co dodano w etapie source localization

Commit instrumentu:

```text
380378ef52df980c62d2db87641dec2aa07e97ae
```

Sonda v2 nadal kompiluje ten sam pełny headless M6 i nie zmienia fizyki. Dodaje:

- gęsty sweep amplitudy `0,40–0,50` co `0,01` w obu kierunkach;
- rozdzielenie krótkiego outward overshoot od trwałego odejścia od środka;
- snapshot pierwszego potwierdzonego trwałego przejścia;
- prędkość obrotu obu zwrotnic wokół ich rzeczywistych osi;
- slip angle i obciążenie każdego przedniego koła;
- reakcje drążków, racka, spin-joint i ograniczeń przegubów kulowych;
- normalno-impulsowy środek kontaktu, jego geometryczny trail i scrub względem
  bieżącej osi zwrotnicy;
- jawne zastrzeżenie, że kanały nie są niezależne ani bezpośrednio addytywne.

API Box3D raportuje reakcję jointu działającą na body B. Instrument odwraca znak,
gdy żądanym ciałem jest body A, oraz liczy moment reakcji względem osi zwrotnicy.

## 5. Finalny run

Źródło:

```text
commit 380378ef52df980c62d2db87641dec2aa07e97ae
tree   eddb5ac0e1ca1da57df5dd279a3e1cf0acbea2f9
dirty  false
preset linux-research / Release
```

Run i surowe trace’y:

```text
tools/research/evidence/B3WHEEL-STEER-01/20260805T231329Z-380378e/
```

Dwa niezależne przebiegi gęstego sweepu dały bajtowo identyczne podsumowanie i
wszystkie 28 trace’ów. Sonda odrzuca niepusty katalog wyjściowy.

## 6. Twarde wyniki

### 6.1 Osobne przedziały kierunkowe

```text
skręt dodatni:  +0,44 wraca; +0,45 nie wraca
skręt ujemny:   -0,41 wraca; -0,42 nie wraca
```

Przejścia są monotoniczne w zbadanej siatce, ale są wyraźnie asymetryczne.
To przedziały tego rigu i protokołu, nie uniwersalne progi opony.

### 6.2 Ogranicznik przegubu nie inicjuje przejścia granicznego

```text
+0,45: trwałe przejście 0,5000 s; limit twist 0,7333 s
-0,42: trwałe przejście 0,4167 s; limit twist 0,6333 s
```

W chwili przejścia pozostaje około `19,9°` i `18,5°` zapasu twist na dominującej
stronie. Limit może później utrwalać pełny skręt, ale nie rozpoczyna granicznej
niestabilności.

### 6.3 Podpis transmisji na dominująco obciążonym kole

W obu wadliwych przypadkach granicznych:

- reakcja spin-joint na zwrotnicę ma znak zgodny z dalszym skrętem;
- motor racka i moment dominującego drążka działają przeciwnie;
- normalno-impulsowy środek kontaktu jest bardziej przesunięty przed oś skrętu
  niż w sąsiednim przypadku wracającym;
- release speed sąsiednich przypadków różni się o mniej niż `0,04 m/s`.

Najsilniejszy kierunek roboczy to geometria/transmisja
`kontakt → koło → spin-joint → zwrotnica`, a nie rack assist lub inicjacja przez
limit kulowy. To korelacja i lokalizacja kanału, nie jeszcze dowód konkretnej
wady równania kontaktu.

## 7. Czego nadal nie wiadomo

Nie udowodniono:

- czy źródłem jest położenie punktu/normalnej, sztywny Coulomb, brak bocznej
  podatności, brak relaxation length czy ich interakcja;
- czy asymetria pochodzi z Ackermanna/load transfer, kolejności solvera,
  geometrii lewo/prawo czy błędu znaku;
- Windows parity i zgodności z manualnym feel w nowym buildzie;
- zachowania przy innych prędkościach i obciążeniach;
- poprawnego modelu shippingowego.

`contact_trail_positive_behind` jest geometrią środka impulsu normalnego. Nie
wolno nazywać go pneumatic trail ani modelem opony.

## 8. Następny etap — B3WHEEL-STEER-01C: interwencje przyczynowe

Nie zaczynaj od poprawki solvera. Najpierw rozszerz sondę o **probe-only fork at
release**: każda odmiana odtwarza identyczny deterministyczny pre-release, a
jedna zmienna jest zmieniana dokładnie przy puszczeniu wejść.

Minimalna macierz dla przypadków `+0,45` i `-0,42`, z `+0,44/-0,41` jako
kontrolą:

1. baseline;
2. wyłączone limity twist przegubów kulowych;
3. `coastTorque = 0`;
4. `rackFrictionLoadCoeff = 0` przy niezmienionym base friction;
5. front wheel friction = 0 od chwili release;
6. front contact disabled/no-contact od chwili release.

Każdy wariant osobno, bez kombinacji i bez zmiany shippingowych defaultów.

Interpretacja:

- wada bez kontaktu oznacza źródło wewnątrz rigu/constraintów;
- zniknięcie wyłącznie po usunięciu tarcia wskazuje kanał styczny/slip;
- przetrwanie przy zerowym tarciu, lecz zniknięcie bez normalnego kontaktu,
  wzmacnia hipotezę normalnej geometrii rezultanty;
- brak wpływu wyłączenia limitów potwierdza ich rolę wtórną;
- rack/coast są kontrolami wykluczającymi, nie kandydatami na „naprawę”.

Dopiero po tej macierzy wolno wybrać minimalny eksperyment w rdzeniu Box3D.

Research OS został ustawiony tak, aby ten interlock był wykonywalnym źródłem
prawdy, a nie tylko opisem. Kontrola wejścia:

```text
python tools/jv_lab.py next
→ B3WHEEL-STEER-01C
```

Spec pozostaje `blocked`, dopóki nie istnieją: prawdziwy fork identycznego stanu
pre-release, adapter weryfikujący receipt, izolowane interwencje oraz czerwone
testy zakazujące kombinacji, zapisu shippingowych defaultów i sztucznego
centrowania. `WHEEL-SOFT-03R` zależy teraz również od zamknięcia tego etapu.

## 9. Twarde zakazy dla następnego agenta

- nie dodawaj `rackCenteringHertz` ani ukrytego serva;
- nie dodawaj momentu zależnego od puszczenia sterowania;
- nie obniżaj globalnie gripu, aby zazielenić test;
- nie uznawaj torusa za rozwiązanie tylko dlatego, że wraca;
- nie sumuj bezpośrednio kanałów kontakt/spin-joint/drążek/gravity;
- nie zmieniaj jednocześnie geometrii zawieszenia i modelu kontaktu;
- nie dotykaj `main` ani aktywnego lokalnego brancha Jozza;
- nie uruchamiaj GitHub Actions;
- nie wracaj do `WHEEL-SOFT-03R`, dopóki interlock kierownicy nie ma fizycznie
  uzasadnionej decyzji.

## 10. Kryterium zakończenia B3WHEEL-STEER-01C

Etap C kończy się tylko wtedy, gdy:

- wszystkie interwencje mają identyczny pre-release receipt;
- dwa świeże przebiegi są bajtowo identyczne;
- źródło zostaje zawężone co najmniej do normalnego kontaktu, stycznego kontaktu
  albo wewnętrznych constraintów;
- wynik zawiera negatywne warianty i jawne nie-wnioski;
- żadna zmiana shippingowej fizyki nie została jeszcze przemycona;
- następny minimalny eksperyment jest uzasadniony danymi, nie wyglądem wykresu.
