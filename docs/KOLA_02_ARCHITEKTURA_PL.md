# Koła i opony — architektura bieżąca

Status: **kontrakt architektoniczny po wdrożeniu `b3Wheel`**.
Poprzedni dokument kandydatów S4–S6 zachowano w
`archive/consolidated_2026-08/KOLA_02_ARCHITEKTURA_PRE_B3WHEEL_PL.md`.

## 1. Cel

JV potrzebuje koła, które jednocześnie:

- toczy się bez periodycznego młotkowania od dyskretnych brył;
- ma prawdziwą szerokość i profil poprzeczny;
- zachowuje stabilną tożsamość kontaktu;
- pozwala osobno badać geometrię, topologię manifoldu i podatność;
- pozostaje pojedynczym, jawnie posiadanym rozszerzeniem Box3D.

Obecny `b3Wheel` jest właściwym fundamentem powierzchni. Nie jest jeszcze pełną
oponą i nie wolno przypisywać mu deformacji, której kod nie liczy.

## 2. Pięć warstw

```text
Wheel definition
  profil 2D + oś + cornerRadius; normalizacja i upper convex hull

Surface/query geometry
  bryła obrotowa, AABB, support, ray/query policy

Contact generation
  wheel-plane / triangle / hull / capsule / sphere

Manifold identity and topology
  liczba punktów, feature IDs, persistence i clipping

Solver/material response
  normal softness, friction, rolling resistance, impulses
```

Integracja pojazdu siedzi nad tymi warstwami i dostarcza masę, zawieszenie,
napęd oraz warunki eksperymentu. Zmiana w jednej warstwie nie może być opisywana
jako wynik innej.

## 3. Kontrakt `b3Wheel`

`b3Wheel` opisuje solid of revolution wokół osi. Profil rdzenia ma maksymalnie
`B3_MAX_WHEEL_PROFILE_POINTS`, jest sortowany, deduplikowany i redukowany do
górnej otoczki wypukłej. `cornerRadius` wykonuje ball sweep narożników.

Własności wymagane:

- profil po normalizacji jest deterministyczny;
- `radius` i `halfWidth` wynikają z profilu;
- debug draw korzysta z tej samej znormalizowanej geometrii;
- liczba próbek profilu kształtuje powierzchnię, ale nie może sama definiować
  „odcisku” przez speculative distance;
- masa pojazdu pozostaje zamrożona podczas porównań reprezentacji.

## 4. Macierz dokładności API

| Ścieżka | Stan | Kontrakt |
|---|---|---|
| AABB / swept AABB | dokładna dla obwiedni profilu | broad phase |
| wheel–plane | dedykowana, lecz topologia do naprawy | `WHEEL-RIGID-01` |
| wheel–triangle | dedykowana, brak pełnego seam fallback | `WHEEL-SEAM-02` |
| wheel–hull | dedykowana, niepełny convex–convex | po seam |
| wheel–capsule/sphere | dedykowana projekcja | bumpery i proste przeszkody |
| raycast | konserwatywny walec obwiedniowy | picking/probe, nie dokładny profil |
| generic proxy / overlap / cast | konserwatywna sfera | jawnie conservative |
| mass | przybliżony walec obwiedniowy | caller może nadpisać masę |

Każda publiczna ścieżka ma być oznaczona jako `exact`, `conservative` albo
`unsupported`. Konserwatywne query nie mogą być przedstawiane jako dokładne.

## 5. Topologia sztywnego manifoldu

Dla ciągłej, sztywnej powierzchni i płaszczyzny:

- unikalny support vertex daje 1 punkt;
- rzeczywisty support segment równoległy do płaszczyzny daje 2 końce;
- dodatkowe wierzchołki nie stają się stopą tylko dlatego, że mieszczą się w
  `B3_SPECULATIVE_DISTANCE`.

Speculative distance odpowiada za istnienie kontaktu, nie za model deformacji.
Feature ID ma identyfikować faktyczny support feature i przeżywać obrót koła.
To jest baseline, od którego mierzymy każdą późniejszą podatność.

## 6. Teren i krawędzie

Triangle contact musi rozróżniać face, edge i vertex oraz utrzymywać kontakt na
szwie współpłaszczyznowych trójkątów. Odrzucenie punktu poza barycentrą bez
fallbacku jest błędem ciągłości, nie właściwością opony.

Wheel–hull docelowo potrzebuje ograniczenia punktów do face polygon i kompletu
istotnych osi rozdzielających. Nie naprawiamy tego przez powiększanie dystansu
spekulacyjnego.

## 7. Lokalna podatność

Podatność normalna jest parametrem odpowiedzi solvera, nie zmianą profilu.
Eksperyment A/B musi zachować identyczne:

- punkty manifoldu i feature IDs;
- profil, masę i bezwładność;
- tarcie i rolling resistance;
- zawieszenie, podkroki, drogę i sterowanie.

A używa bazowej odpowiedzi kontaktu, B lokalnego override wheel–ground. Dopiero
różnica tych przebiegów jest dowodem wartości podatności.

## 8. Możliwa opona strukturalna

Struktura jest etapem późniejszym, nie zamiennikiem brakującego baseline'u.
Jeżeli lokalna podatność daje wartość, kandydat docelowy może mieć lokalne stany
radialne/osiowe bieżni i coupling sąsiadów, podczas gdy analityczna obwiednia
pozostaje powierzchnią broad phase i zapytań.

Nie wracamy do wielu niezależnych stockowych colliderów jako substytutu
ciągłej opony. Liczba kształtów i kontaktów wprowadza osobny bias i koszt.

## 9. Granice zmian rdzenia

Każda delta `src/`/`include/` podlega `KOLA_03_POLITYKA_BOX3D_PL.md` i
`docs/JOZZ_CORE_PATCHES.json`. Architektura nie daje zgody na refaktor sąsiednich
systemów ani zmianę stockowego zachowania przy nieaktywnej funkcji.
