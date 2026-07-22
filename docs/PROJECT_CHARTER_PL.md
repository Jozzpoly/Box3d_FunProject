# Project Charter — Box3d_FunProject

**Rola:** trwała karta intencji produktu i właściciela  
**Właściciel kierunku:** Jozz  
**Status:** obowiązujący kompas projektowy po owner review  
**Nie jest:** mutable current state, roadmapa, polityka Git, work-item queue ani zgodą na implementację

## 1. Czym jest ten projekt

Box3d_FunProject jest natywnym warsztatem inżynieryjnym i zalążkiem gry, w której
Jozz ożywia własne modele pojazdów oraz prawdziwe miejsca.

Projekt łączy trzy źródła wartości:

1. **uczciwą fizykę pojazdu** — zachowanie wynika z konstrukcji, geometrii, jointów,
   momentu, kontaktu, przyczepności i masy;
2. **autorski proces twórczy** — modele, rig, świat i narzędzia pozostają pod realną
   kontrolą Jozza;
3. **doświadczenie jazdy po znaczącym świecie** — od syntetycznych laboratoriów po
   prawdziwe miejsca pozyskane ze skanów i później świadomie opracowane.

Docelowy produkt nie ma być pokazem importera ani samą demonstracją Box3D. Ma dawać
radość z budowania, obserwowania, diagnozowania, strojenia i prowadzenia maszyn w
świecie, który jest jednocześnie fizycznie uczciwy i osobiście ważny dla autora.

## 2. Dewiza

> **Drążymy skałę kropla po kropli — ale każda kropla ma zostawić działający,
> powtarzalny fundament.**

Znaczenie operacyjne:

- duże marzenie rozbijamy na małe eksperymenty;
- eksperyment ma odpowiadać na jedno prawdziwe pytanie produktu;
- nie rozbudowujemy systemu przed udowodnieniem jego wartości w grze;
- nie mylimy rozległej dokumentacji z postępem produktu;
- po każdym kamieniu zostaje dowód, granica capability oraz bezpieczny punkt powrotu.

## 3. Uczciwa fizyka przed iluzją

Zachowanie pojazdu ma emergować z fizycznej konstrukcji. Nie wolno zastępować
zaakceptowanej dynamiki animacją, skryptowanym prowadzeniem ani ukrytym
samocentrowaniem tylko dlatego, że daje łatwiejszy obraz.

Dozwolone wspomagania arcade muszą być:

- jawne;
- opt-in;
- odseparowane od realistycznego defaultu;
- nazwane tak, aby nie udawały naturalnej fizyki.

Render, rig i authoring mogą przedstawiać fizykę, ale nie mogą po cichu stać się jej
autorytetem.

## 4. Proof before production

Przed kosztowną kampanią produkcyjną należy uzyskać mały, grywalny dowód.

Przykłady:

- przed budową kompletnego rigu — jeden realny narożnik działający na żywych ciałach;
- przed wielką mapą testową — mały układ, po którym Jozz rzeczywiście chce jeździć;
- przed kampanią wysokiej jakości skanów — jeden prawdziwy skan, na którym samochód
  może jechać i dawać frajdę;
- przed streamingiem świata — jeden ograniczony sektor o sprawdzonych warstwach
  renderu, kolizji i materiału.

Najbardziej rozbudowane rozwiązanie nie jest automatycznie najlepsze. Wygrać ma
najmniejszy fundament, który uczciwie odpowiada na pytanie etapu i nie zamyka
przyszłej drogi.

## 5. Fun jest prawdziwą bramką właściciela

Build, testy, telemetry i benchmarki są konieczne, ale nie dowodzą, że gra daje
frajdę.

Gdy etap dotyczy:

- odczucia pojazdu;
- czytelności świata;
- jakości jazdy;
- wyboru miejsca;
- skali percepcyjnej;
- realizmu kontra arcade;

ostateczny werdykt należy do Jozza po realnym renderze albo jeździe.

Agent nie może wywnioskować `FUN_PASS`, `FEEL_PASS`, visual acceptance ani owner
acceptance z CI.

## 6. Wystarczająco dobry rdzeń przed idealną całością

Pierwsze dane i pierwsze implementacje mogą mieć świadomie zaakceptowane wady, jeżeli
ich dobry rdzeń wystarcza do odpowiedzi na najbliższe pytanie.

Dla pierwszego skanu akceptowane są między innymi artefakty obrzeży, wiszące fragmenty,
pionowe ściany rekonstrukcji i brak produkcyjnego oczyszczenia, ponieważ centralny
obszar jest wystarczający do testu kierunku.

Ta zasada nie oznacza ignorowania wad. Każda wada musi mieć jawny status:

```text
KNOWN_LIMITATION
NON_BLOCKING_FOR_CURRENT_PROOF
BLOCKING_BEFORE_<NAMED_FUTURE_GATE>
```

Nie wolno zostawiać problemu jako bezimiennego „później”.

## 7. Kontekst wizualny przed promocją do fizyki

Gdy skala, znaczenie miejsca lub wybór powierzchni zależy od rozpoznania realnego
świata, geometria bez koloru nie wystarcza do finalnej walidacji.

Dla aktualnej ścieżki skanów obowiązuje kolejność:

```text
SOURCE_GEOMETRY_VISIBLE
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ DRIVE_REGION_SELECTION
→ COLLISION_REPRESENTATION
→ REAL_SCAN_DRIVE
```

Tekstury nie są w tym kroku ozdobą. Dostarczają kontekstu potrzebnego do:

- rozpoznania drogi, pobocza, trawy i zabudowy;
- ustawienia samochodu przy znanym obiekcie;
- finalnej oceny skali;
- świadomego wyboru pierwszego obszaru jazdy;
- późniejszego porównania renderu i kolizji.

Kolizja nie może być rozpoczęta tylko dlatego, że geometry-only preview jest widoczny.

## 8. Warstwy prawdy świata

Poniższe warstwy muszą pozostać jawnie oddzielone:

```text
PRIVATE_SOURCE_EVIDENCE
SOURCE_VISUAL_PREVIEW
AUTHORED_WORLD_ASSETS
RENDER_DERIVATIVES
PHYSICS_SURFACE
SURFACE_MATERIAL_MAP
GAMEPLAY_SEMANTICS
WORLD_COMPOSITION_AND_STREAMING
```

Konsekwencje:

- surowy skan nie jest automatycznie authored world;
- render mesh nie jest automatycznie kolizją;
- kolizja nie definiuje automatycznie asfaltu, trawy ani błota;
- materiał fizyczny nie definiuje automatycznie gameplayu;
- source evidence pozostaje zachowane nawet po ręcznym authoringu.

## 9. Dwa rodzaje świata, dwa różne zastosowania

Projekt zachowuje oba:

### Syntetyczny engineering world

Służy do:

- deterministycznych sond;
- kontrolowanych przeszkód;
- porównań before/after;
- strojenia i benchmarków;
- izolowania problemów fizyki.

### Real-scan world

Służy do:

- autentycznego doświadczenia jazdy;
- naturalnych, nieprojektowanych nierówności;
- walidacji pipeline'u świata;
- emocjonalnego związku z realnym miejscem;
- przygotowania przyszłego authoringu dużej mapy.

Jeden świat nie zastępuje drugiego. Syntetyczne laboratorium jest przyrządem
pomiarowym, a skan — materiałem rzeczywistego doświadczenia.

## 10. Docelowy authoring świata

Surowe skany są początkiem, nie końcem procesu. Długoterminowa ścieżka może obejmować:

```text
wysokiej jakości skany w dobrych warunkach światła
→ ręczne czyszczenie i organizację w Blenderze
→ oddzielenie dróg, gruntu, budynków, lasu i roślinności
→ low-poly budynki, drzewa i obiekty
→ osobne render LOD-y i collision proxy
→ mapy asfaltu, trawy, błota i innych właściwości
→ fizykę, audio, VFX i ślady zależne od nawierzchni
→ łączenie i streaming wielu sektorów
→ duży naturalny, żywy świat
```

Automatyzacja może wspierać authoring, ale nie może odbierać Jozzowi kontroli nad tym,
co jest drogą, gruntem, budynkiem ani częścią świata.

## 11. Narzędzia twórcy są częścią produktu

Importer, edytor rigu, przyszły edytor świata, diagnostyka i workflow Blender → gra nie
są wyłącznie zapleczem technicznym. Są częścią wizji produktu: Jozz ma móc tworzyć,
wiązać, testować i poprawiać świat oraz maszyny bez ciągłego przepisywania C++.

Nie oznacza to budowy wielkiego edytora przed dowodem potrzeby. Każda funkcja edytora
ma wynikać z problemu odkrytego w realnym authoringu.

## 12. Relacja z JES

Technologia świata, skanów i authoringu może później wejść do JES albo pozostać
osobnym projektem współdzielącym część pipeline'u.

Obecnie:

- nie sprzęgamy architektury przedwcześnie z JES;
- zachowujemy czyste kontrakty i przenośne formaty;
- decyzję integracyjną podejmujemy po realnych dowodach jazdy, authoringu i kosztów.

## 13. Czego projekt nie robi

Projekt nie powinien:

- fałszować fizyki dla szybkiego efektu;
- budować całego kilometra świata przed jednym dobrym przejazdem;
- traktować surowych skanów jako bezpośredniej prawdy kolizji;
- automatycznie klasyfikować całego świata przed ręcznym proofem;
- podporządkowywać manualnej pracy właściciela biurokracji schedulera;
- tworzyć nowego dokumentu dla każdego drobiazgu;
- uznawać zielonego CI za dowód renderu, skali, feelu albo frajdy;
- odkładać niejawnych problemów bez statusu i warunku powrotu.

## 14. Jak używać tej karty

Przy sporze między dwiema technicznie poprawnymi drogami zapytaj:

1. Która szybciej daje uczciwy, grywalny dowód?
2. Która lepiej zachowuje prawdziwą fizykę i kontrolę twórcy?
3. Która pozostawia czystszy fundament dla kolejnego kroku?
4. Jakie ograniczenia świadomie akceptujemy i do którego gate'u?
5. Czy decyzja należy do testu, czy do Jozza po renderze/jazdzie?

Ta karta pomaga interpretować cel. Nie nadpisuje `AGENTS.md`, control plane, exact
Git authority, prywatności, testów ani owner gate'ów.
