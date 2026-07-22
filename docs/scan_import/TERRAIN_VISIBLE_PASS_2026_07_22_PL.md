# TERRAIN_VISIBLE_PASS — zamknięcie geometry-only preview

**Data dowodu:** 2026-07-22  
**Domena:** scan import / native preview  
**Rodzaj dokumentu:** redacted milestone evidence  
**Prywatne źródła:** pozostają wyłącznie owner-local

## 1. Werdykt

```text
TERRAIN_VISIBLE_PASS
SAME_REVISION_RESTART_PASS
KNOWN_SOURCE_LIMITATIONS_ACCEPTED
WORLD_SCALE_VALIDATED = false
TEXTURED_PREVIEW_REQUIRED_BEFORE_SCALE_OR_COLLISION
```

Pierwszy rzeczywisty, prywatny zestaw siedmiu kafli został odnaleziony, dopasowany do
zweryfikowanego bundle'a, przetworzony do exact render-only preview, niezależnie
zweryfikowany oraz dwukrotnie załadowany w natywnym hoście na tej samej rewizji kodu.

Właściciel obejrzał rzeczywisty teren, rozpoznał miejsce i zaakceptował geometry-only
preview jako wystarczający dowód kierunku.

## 2. Publiczna rewizja dowodu

```text
code revision: 33099413bf8f44adbe1d635f9e10bdf2d0b5c321
branch at proof time: agent/scan-terrain-r1b-consolidated-integration
product PR: #13
```

Exact mutable authority nadal pochodzi z GitHub Control Issue. Powyższy SHA jest
historyczną rewizją milestone'u, nie ponadczasowym current headem.

## 3. Łańcuch dowodu

```text
clean isolated checkout
→ exact remote SHA verification
→ detached HEAD
→ governance/control validation
→ full scan contract suite
→ Windows CMake/project gate
→ owner-private source discovery
→ exact bundle/receipt binding
→ fourteen source assets resolved by identity
→ seven-tile preview pack
→ independent verifier
→ native first launch
→ owner visual review
→ close process
→ same-revision verify and restart
```

Żaden wcześniejszy etap nie został zaliczony na podstawie późniejszego rezultatu.

## 4. Wykonane bramki techniczne

Na rewizji milestone'u potwierdzono:

```text
CONTROL_VALID
REPOSITORY_AUDIT_PASS
governance tests: PASS
automation tests: PASS
synthetic automation scenarios: PASS
scan contracts: PASS
scan test run: 136 tests OK, 1 intentional skip
Windows configure: PASS
samples build: PASS
jozz_vehicle_validation build and run: PASS
Box3D test build and run: PASS
300-frame boot smoke: 0 Sokol errors
```

Repozytorium pozostało czyste w detached HEAD po testach i owner-local flow.

## 5. Realny pack i native runtime

Redacted runtime facts:

```text
tiles:      7
vertices:   1 409 687
triangles:  1 775 775
first native load: PASS
same-revision restart: PASS
restart runtime: 949 frames, 0 Sokol errors
```

Nie zapisujemy tutaj:

- prywatnych ścieżek;
- lokalizacji ani współrzędnych;
- source hashes;
- receipt contents;
- raw GLB/PLY;
- pełnej prywatnej tożsamości packa.

## 6. Owner visual evidence

Właściciel potwierdził na realnym renderze:

- siedem kafli jest obecnych;
- geometria tworzy rozpoznawalny, spójny centralny teren;
- budynki, drzewa, droga i ukształtowanie stoją we właściwej globalnej orientacji;
- per-tile visibility, geometry, bounds, metre grid i lab axes działają;
- po zbliżeniu widoczna jest użyteczna geometria drogi i otoczenia;
- ponowne uruchomienie wybiera ten sam przygotowany zestaw i odtwarza działający
  native preview.

Właściciel świadomie zaakceptował jako non-blocking dla tego proofu:

- wiszące fragmenty i wyspy na obrzeżach;
- pionowe ściany rekonstrukcji;
- odległe lub rozciągnięte artefakty coverage;
- niedoskonałe globalne kadrowanie pełnych bounds;
- brak tekstur;
- brak kolizji.

## 7. Co ten PASS udowadnia

```text
REAL_PRIVATE_SOURCE_RESOLVED
EXACT_PREVIEW_PACK_READY
NATIVE_SOURCE_GEOMETRY_VISIBLE
SEVEN_TILE_LAYOUT_VISIBLE
OWNER_RECOGNIZES_REAL_PLACE
SAME_REVISION_RESTART_REPEATABLE
```

Oznacza to, że realny skan może przejść przez obecny prywatnościowo bezpieczny
pipeline i pojawić się jako spójna geometria w natywnym silniku.

## 8. Czego ten PASS nie udowadnia

```text
TEXTURES_VISIBLE = false
WORLD_SCALE_VALIDATED = false
GLB_PLY_INTERIOR_CORRESPONDENCE = false
ACCEPTED_WORLD_PATCH_READY = false
COLLISION_PROJECTION_READY = false
SURFACE_MATERIALS_READY = false
DRIVE_TEST_READY = false
FUN_DRIVE_PASS = false
```

Szczególnie ważne: metre grid i poprawna transformacja nie są jeszcze finalnym
dowodem skali świata. Finalna walidacja skali wymaga samochodu ustawionego na drodze
lub obok znanego obiektu — domu, drzewa albo innego czytelnego punktu odniesienia.

## 9. Dlaczego tekstury są następnym gate'em

Geometry-only preview wystarczył do dowodu importu i globalnej orientacji, lecz nie
do końcowej oceny świata przed fizyką.

Tekstury muszą pojawić się przed kolizją, ponieważ pozwolą:

- rozpoznać rzeczywistą drogę, pobocze, trawę i zabudowę;
- odróżnić geometrię użyteczną od artefaktów rekonstrukcji;
- ustawić samochód w znaczącym miejscu;
- wykonać percepcyjną walidację skali;
- wybrać pierwszy ograniczony drive region;
- później porównać renderowaną nawierzchnię z pochodną kolizją.

Nowa obowiązująca kolejność:

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
```

## 10. Następna decyzja produktowa

Najbliższy produktowy milestone nie brzmi „zbuduj kolizję”. Brzmi:

> Pokaż ten sam zweryfikowany teren w rzeczywistych kolorach, a następnie ustaw na
> nim zaakceptowany samochód jako wzorzec skali i kontekstu.

Dopiero owner visual sign-off tekstur oraz sceny referencyjnej odblokuje wybór ROI i
badanie kolizji.
