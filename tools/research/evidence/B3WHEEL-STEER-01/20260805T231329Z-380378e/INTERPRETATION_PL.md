# B3WHEEL-STEER-01 — interpretacja source localization v2

## Co zmierzono

Gęsty sweep przy niezmienionej fizyce M6 wyznaczył dwa osobne, monotoniczne
przedziały przejścia:

```text
skręt dodatni:  +0,44 wraca; +0,45 nie wraca
skręt ujemny:   -0,41 wraca; -0,42 nie wraca
```

Prędkości w chwili release różnią się między sąsiednimi przypadkami o mniej niż
`0,04 m/s`, więc wynik nie jest prostym skutkiem dużej różnicy prędkości startu.

W przypadkach granicznych wada staje się trwała przy około `0,50 s` dla `+0,45`
i `0,4167 s` dla `-0,42`. Ogranicznik twist odpowiedniego przegubu kulowego
wchodzi dopiero później: `0,7333 s` oraz `0,6333 s`. Ma zatem zdolność utrwalania
pełnego skrętu, ale nie inicjuje przejścia granicznego.

W chwili potwierdzonego przejścia reakcja spin-joint na zwrotnicę koła niosącego
dominujące obciążenie ma znak zgodny z dalszym skrętem. Motor racka i moment
dominującego drążka działają przeciwnie. Jednocześnie normalno-impulsowy środek
kontaktu leży bardziej z przodu osi skrętu niż w sąsiednim przypadku wracającym.

## Wniosek roboczy

Najsilniejszym bieżącym kierunkiem jest geometria i transmisja obciążenia
koło–kontakt–spin-joint–zwrotnica, a nie sztuczne centrowanie racka ani inicjacja
przez limit przegubu. To nadal lokalizacja podpisu źródła, nie dowód konkretnego
błędu w równaniu kolizji lub solvera.

## Czego ten run nie dowodzi

- Kanały kontaktu, spin-joint, drążka i grawitacji nie są niezależną sumą sił;
  mierzą ten sam układ na różnych poziomach transmisji.
- `contact_trail_positive_behind` jest geometrią środka impulsu normalnego, nie
  pneumatic trail ani modelem odkształcenia opony.
- Nie wiadomo jeszcze, czy przyczyną jest położenie punktu/normalnej, sztywny
  model Coulomba, brak relaxation length, brak podatności bocznej czy interakcja
  kilku z tych elementów.
- Nie ma poprawki, wartości shippingowej ani akceptacji feelu.
- Nie udowodniono jeszcze Windows parity, zależności od prędkości ani zachowania
  bez kontaktu z podłożem.
