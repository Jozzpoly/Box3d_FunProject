# B3WHEEL-STEER-01 — interpretacja evidence lock

## Werdykt

Ten run **nie naprawia** fizyki i nie jest akceptacją produktu. Blokuje znaną wadę w deterministycznym, headless pełnym M6: po jednoczesnym puszczeniu gazu i kierownicy `b3Wheel` zmienia zachowanie pomiędzy zbadanymi amplitudami `0.40` i `0.50`, a przy pełnym wejściu nie wraca podczas dalszego toczenia.

## Twarde obserwacje

- torus wraca poniżej `3°` w obu kierunkach;
- `b3Wheel` wraca dla `±0.40`, lecz dla `±0.50` przechodzi w `ANTI_CENTERING_OR_HOLD`;
- `b3Wheel ±1.00` pozostaje przy dużym skręcie po 4 s, nadal przy prędkości około 4 m/s;
- sfera jest asymetryczna: `+1.00` wraca, `-1.00` nie wraca;
- dwa świeże przebiegi dały bajtowo identyczne summary i wszystkie trace CSV.

## Granice dowodu

To nie lokalizuje dokładnego progu pomiędzy `0.40` i `0.50`. Nie dowodzi też, że dominującą przyczyną jest normalna kontaktu, tarcie, manifold, caster albo brak podatności opony. Zapisane impulsy względem osi skrętu są diagnostyką bieżącego solvera; nie są jeszcze fizycznym modelem opony ani pełnym rozkładem momentów.

## Ograniczenie projektowe

Naprawa nie może użyć ukrytej sprężyny/serva do zera, momentu zależnego od puszczenia wejścia ani globalnego obniżenia przyczepności. Kolejny etap ma analizować fazę przejścia w trace i ustalić, jakie fizyczne składniki modelu kontaktu/koła muszą zostać rozwinięte.
