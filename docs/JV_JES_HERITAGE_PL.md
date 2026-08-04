# JV → JES: dziedzictwo i granica clean-room

Status: bieżąca polityka transferu wiedzy.
JV i JES są osobnymi projektami. Celem nie jest „przenieść repo JV”, lecz
promować sprawdzone zdolności bez historycznych założeń hosta i architektury.

## 1. Co przenosimy

### Wiedza i metoda

- jedna zmienna główna na eksperyment;
- drabina transferu: mikrotest → rig → pojazd → scena produktu;
- negatywny wynik jako trwały produkt;
- jawny rejestr confoundów;
- generated evidence zamiast ręcznie przepisywanych tabel;
- rozdział pomiaru od werdyktu o feelingu;
- Zero-Delta-Off dla eksperymentalnych rozszerzeń backendu;
- małe, odwracalne patche z lokalną bramką.

### Kontrakty, które mają wartość ponad JV

- authoring asset ≠ visual model ≠ physics prefab ≠ contact model;
- stabilna identyfikacja cech kontaktu;
- jawna semantyka exact / conservative / unsupported dla zapytań geometrii;
- konfiguracja eksperymentu jako plik z prowieniencją;
- zapis masy, bezwładności, solvera i sceny razem z wynikiem;
- render i headless używające tego samego rigu.

### Dowody

- surowe logi stendu i jazdy;
- findingi z aktualnym statusem;
- minimalne reproduktory bugów;
- porażki walca, split-sphere i wielokształtnych pierścieni;
- wyniki pokazujące, że ranking stendu nie musi przetrwać transferu do pojazdu.

## 2. Czego nie przenosimy automatycznie

- struktury `samples` jako docelowego hosta JES;
- nazw milestone M0–M9;
- całego configu M6 i UI jego zakładek;
- historycznych obejść ograniczeń konkretnego rigu;
- mapy JV jako architektury świata JES;
- kodu `b3Wheel` bez osobnego kontraktu backendu JES;
- domyślnych parametrów pojazdu jako „praw fizyki”.

## 3. Klasy materiału

| Klasa | Przykład | Działanie w JES |
|---|---|---|
| `EVIDENCE` | surowy run + config + SHA | kopiuj lub zamroź z manifestem |
| `CAPABILITY` | stabilny manifold feature ID | opisz zachowanie, zbuduj clean-room implementację |
| `METHOD` | drabina Q0–Q4 | przenieś zasadę, dopasuj narzędzie |
| `ASSET` | model Jozza | przenieś tylko z jawną własnością i kontraktem |
| `MECHANISM` | M6 rack friction | nie kopiuj bez nowego wymagania JES |
| `HISTORY` | plan M7/M8 | zachowaj jako kontekst, nie implementuj |

## 4. Procedura promocji capability

1. Nazwij zachowanie bez odwołania do plików JV.
2. Wskaż dowód i znane confoundy.
3. Zapisz minimalny kontrakt wejścia/wyjścia.
4. Zbuduj clean-room wariant w JES.
5. Porównaj na tym samym scenariuszu: MATCH / IMPROVE / świadome BREAK.
6. Dopiero po wyniku włącz capability do fundamentu JES.

## 5. Priorytetowe dziedzictwo z programu koła

Najcenniejszy wkład JV do JES nie brzmi „użyj `b3Wheel`”. Brzmi:

- gładkość obwiedni, liczba kształtów, topologia manifoldu i podatność to osobne
  zmienne;
- szeroki kontakt ujawnia problemy kierownicy niewidoczne dla sfery;
- sztywny manifold nie jest fizycznym odciskiem opony;
- lokalna compliance musi być testowana przy identycznej geometrii i feature IDs;
- wielokształtny collider może generować wynik przez liczbę constraintów, a nie
  przez lepszy model materiału.

To są prawa eksperymentalne do ponownej weryfikacji w JES, nie gotowa recepta.

## 6. Status starego pakietu JES

Pakiet z 2026-07-15 zachowano w
`archive/jes_foundation_2026-07-15/`. Ma wartość założycielską, ale nie jest
bieżącą instrukcją JV ani automatycznie aktualną konstytucją JES. Ten dokument
jest jedyną bieżącą granicą transferu zapisaną w repo JV.
