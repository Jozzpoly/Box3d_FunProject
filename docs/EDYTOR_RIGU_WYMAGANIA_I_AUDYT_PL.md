# Edytor rigu — zachowane wymagania po realnym warm-up

**Źródło:** owner-driven import nowego steering-suspension rigu na żywy pojazd M6.  
**Status obecny:** `PARKED_DESIGN_AUTHORITY / OWNER_DECISION_REQUIRED`  
**Nie jest:** aktywną kampanią, gotowym formatem ani zgodą na zmianę fizyki.

Historyczny przebieg badawczy:

```text
docs/PLAN_EDYTOR_RIGU_ROZGRZEWKA_2026_07_11_PL.md
```

Granica z przyszłym JES musi zostać rozstrzygnięta przed rozpoczęciem pełnego editora.
Box3d_FunProject może pozostać laboratorium proofów i konkretnych kontraktów, ale nie
powinien automatycznie stać się docelowym repozytorium całego narzędzia.

## 1. Audyt języka rigu istniejącego dziś

Bieżące implementacje używają kilku prymitywów:

| Prymityw | Znaczenie |
|---|---|
| local binding frame | pozycja/orientacja części względem konkretnego żywego ciała |
| parent per part | część podąża za chassis/knuckle/arm/wheel/rack |
| stretch between anchors | ramię, drążek, damper lub cardan łączy dwa żywe punkty |
| mirror L/P | jeden authored asset obsługuje obie strony |
| authored socket role | semantyczna kotwica z asset contractu |
| visual correction | jawna korekta orientacji/środka modelu bez zmiany fizyki |

Realna topologia M6 obejmuje między innymi:

```text
chassis
upperArm / lowerArm
knuckle
wheel
rack
```

Uproszczony bench M9 miał dodatkową rolę „carrier”, której realny pojazd nie ma.
Dlatego binding nie może być kopiowany z izolowanego bencha bez mapowania na realne
ciała.

## 2. Wymagania kanoniczne

### W1 — rodzic per część

Edytor musi pozwolić przypisać każdą część do realnego body/semantic parenta.
To decyzja kinematyczna:

- knuckle: jedzie i skręca;
- arm: jedzie z zawieszeniem, nie skręca jak knuckle;
- wheel: obraca się;
- rack: przesuwa się poprzecznie;
- chassis: jest bazą pojazdu.

### W2 — jawne typy bindingu

Minimalny zestaw:

```text
RIGID_TO_PARENT
STRETCH_BETWEEN_ANCHORS
MIRRORED_COUNTERPART
ANCHOR_AT_SUBPOSITION_ON_BODY
SOCKET_CARRIED_BY_STRETCH_PART_POSE
```

Edytor nie może redukować każdego elementu do „node ma jednego parenta”.

### W3 — pivot i local binding transform

Per część potrzebne są:

- translation;
- rotation;
- opcjonalna jawna scale warstwy wizualnej;
- pivot/origin;
- live preview;
- reset do authored/accepted value.

Pivot jest częścią semantyki ruchu, nie kosmetyką.

### W4 — gizma i edycja socketów

Gizmo musi odróżniać:

```text
model root placement
part local binding frame
authored socket position
runtime anchor derived from a body/joint
```

Przesunięcie socketu nie może po cichu mutować geometrii fizycznej, chyba że użytkownik
jest w osobnym, jawnie fizycznym trybie edycji.

### W5 — jedno źródło prawdy

Dziś część danych występuje w asset contractach i hardkodowanych fallbackach. Docelowo
binding ma być zapisany w jednym wersjonowanym, walidowanym miejscu:

- rozszerzony asset contract;
- albo osobny rig document związany z exact asset revision.

Nie używać node indexu jako trwałej semantyki. Binding powinien działać po rolach i
stabilnych authored IDs.

### W6 — determinizm i walidacja

Dla zmian czysto wizualnych:

- headless/contract validation;
- fixed-camera screenshots;
- diff accepted baseline;
- brak zmian validator physics numbers.

Dla zmian fizycznych:

- osobny capability;
- osobne shapes/bodies evidence;
- full vehicle gate;
- owner feel/drive gate.

### W7 — topology override

Pole typu `ridesBody` w asset contract może być jedynie authored defaultem. Realny
pojazd może wymagać jawnego override’u wynikającego z jego topologii.

Override musi być widoczny, zapisany i audytowalny — nie ukryty w magicznym switchu.

### W8 — visual layer i collision layer są osobne

Owner wymaga przyszłego in-game importu modeli, w tym możliwości testowania modelu z
Blockbench jako źródła collision body. To tworzy dwie niezależne warstwy:

```text
visual model/bindings
collision source/cook/shapes
```

Model renderowany nie staje się automatycznie kolizją. Collision import wymaga:

- jawnej reprezentacji wejściowej;
- limitów convex/mesh/shape;
- cook/validation report;
- podglądu warstwy kolizji;
- bezpiecznego replacement/revert;
- osobnej owner approval;
- zakazu patchowania Box3D core jako skrótu.

## 3. Odkrycia z realnego warm-up

### O1 — parent ma znaczenie fizyczne

`WheelCenter` musi skręcać z knucklem, a `ChassisMount_b` nie może automatycznie robić
tego samego. Realne mapowanie wymaga różnych parentów.

### O2 — root placement i parent binding są niezależne

Jedna poza importowanego modelu może zostać następnie rozłożona na kilka local frames
względem różnych ciał. Edytor powinien pokazywać te dwie warstwy osobno.

### O3 — node index jest kruchy

Twarde indeksy węzłów mogą po cichu zmienić znaczenie po reeksporcie. Role/IDs i closed
schema są wymagane.

### O4 — część kotwic pochodzi spoza modelu

Rack endpoint, real body point, joint frame lub gameplay anchor mogą być poprawnym
źródłem końca bindingu. Nie wszystkie kotwice są authored nodes w jednym GLTF.

### O5 — mirror convention działa, ale musi być jawna

Jednostronny authored asset może generować L/P przez mirror-X. Edytor może proponować
to jako default, lecz musi pokazywać determinant/orientation i umożliwiać świadome
nadpisanie.

### O6 — wybór ciała nie wystarcza

Kotwica na racku może oznaczać środek, lewy koniec, prawy koniec albo konkretny socket.
Potrzebna jest sub-pozycja/rola na wybranym obiekcie.

### O7 — socket może jechać z pozą części rozciąganej

Dolne oko dampera może być punktem niesionym przez żywą pozę ramienia, a nie bezpośrednio
przez body transform. Edytor potrzebuje referencji do semantycznego punktu na części.

### O8 — collision importer jest osobnym systemem

Podgląd ukrywanej bryły chassis został wykonany. Przyszły import modelu jako collision
source nie został wykonany i nie jest naturalnym rozszerzeniem samego render editor.

## 4. Zachowane owner decisions

```text
front visual rig: nowy steering rig
rear visual rig:  dotychczasowy mount
steering rod:     center of real rack → knuckle
cardan:           optional future element
collision view:   opt-in visible/hidden debug layer
```

Visualny rozjazd części przy skręcie pozostaje kandydatem na pierwszy praktyczny test
pivota/parent bindingu.

## 5. Reactivation gate

Pełny edytor może rozpocząć się dopiero po:

1. jawnej decyzji, czy rozwija się w Box3d_FunProject, JES czy w dwóch warstwach;
2. exact accepted vehicle baseline;
3. oddzieleniu visual editing od physics/collision authoring;
4. wyborze minimalnego document schema i migration path;
5. owner-approved first vertical slice;
6. planie import/revert/validation;
7. braku konfliktu z aktywną scan/textured-terrain campaign.

Pierwszy slice powinien rozwiązać jeden realny przypadek — np. pivot/parent korektę
przedniego rigu — a nie budować od razu uniwersalny DCC.
