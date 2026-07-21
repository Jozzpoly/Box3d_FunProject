# Photogrammetry Import V2 — architecture boundary

**Status:** `PROVISIONAL / P1B / TEST_BEFORE_JES`  
**Zasada nadrzędna:** skan jest źródłem dowodowym, importer tworzy propozycję, ręcznie przyjęty patch jest authored truth, a render/kolizja są odbudowywalnymi projekcjami.

## 1. Dlaczego ta granica istnieje

Dotychczasowy inspector poprawnie odpowiada na pytania o integralność plików, bounds, statystyki i podstawową zgodność par. Nie powinien jednak automatycznie definiować świata. Takie przejście utrwaliłoby przypadkowe założenia pierwszego skanu i utrudniło przyszły reimport oraz transfer wiedzy do JES.

## 2. Przepływ prawdy

```text
PrivateScanSource
  GLB / PLY / hashes / source metadata
        │
        ▼
InspectionEvidence
  parser truth / bounds / warnings / diagnostic grids
        │
        ▼
WorldImportProposal
  generated / disposable / UNREVIEWED
        │ explicit future review commands
        ▼
AcceptedWorldPatch
  stable authored IDs / accepted masks / holes / proxies / materials
        │ validation
        ▼
ValidatedWorldPatchView
        ├── RenderProjection
        └── CollisionProjection
                 │
                 ▼
            RuntimeSession
```

Runtime, Box3D, GPU i UI nigdy nie cofają się do authored truth jako autorytet.

## 3. Klasy danych

### `ScanSourcePackage`

Trwała tożsamość prywatnego zestawu źródłowego i jedna content-derived rewizja.

- `packageId` jest stabilne przez kolejne eksporty/reimporty;
- `revisionId` zmienia się razem z hashem plików lub frame contract;
- tile ma stable ID niezależne od filename i native handle;
- status prywatności v1 to zawsze `PRIVATE_LOCAL_ONLY`;
- source package nie jest asset library ani world document.

### `WorldImportProposal`

Odbudowywalny wynik inspectora.

- status v1: `UNREVIEWED`;
- pair evidence z P1A jest jawnie oznaczone `BOUNDS_ONLY`;
- historyczne `strong-match` jest w proposal normalizowane do `bounds-strong-match`;
- brak ręcznych decyzji, ground truth, collision data i runtime handles;
- proposal ma własny content hash i wskazuje dokładną source revision.

### Przyszły `WorldPatchReview`

Oddzielny authored dokument decyzji:

- potwierdzony frame i skala;
- Golden Drive Region;
- include/exclude/force-ground/interpolation;
- holes i obstacle proxies;
- decyzje world-space, nie indeksy konkretnego gridu;
- referencja do proposal hash.

### Przyszły `AcceptedWorldPatch`

Najmniejsza authored prawda testowego fragmentu świata. Nie jest finalnym formatem JES.

### Projekcje

Quantized heightfield, Box3D shapes, GPU buffers, texture uploads, debug meshes i BVH są cache. Muszą być usuwalne i odbudowywalne z accepted patch.

## 4. Kontrakt ramy współrzędnych

Konwencja numeryczna:

```text
lab_meters = axisMatrix × ((source_point - localOriginSource) / unitsPerMeter)
```

Kontrakt zapisuje:

- source units per meter;
- semantic roles `right`, `forward`, `up` jako signed axes;
- handedness w jawnej kolejności bazowej `right/forward/up`;
- analogiczne role przestrzeni lab;
- signed-permutation `axisMatrix`;
- determinant;
- `preserve | mirror`;
- jawne `mirrorApproved`;
- local source origin;
- confirmation state.

Macierz musi zgadzać się z zadeklarowanymi rolami. Nie może być drugiej ukrytej konwersji w rendererze lub cookerze.

## 5. Tożsamość i rewizja

```text
packageId     = stabilne znaczenie zbioru
revisionId    = hash source hashes + normalized frame contract
stableTileId  = packageId/tile/N
proposalId    = stabilna tożsamość konkretnej linii propozycji
proposal hash = dokładna zawartość wygenerowanego proposal
```

Nie używamy:

- SHA jako jedynego stable ID;
- filename jako długowiecznej semantyki;
- GLB node index jako authored object ID;
- Box3D/GPU/UI handle jako persistent ID.

## 6. Drabina capabilities

```text
G0 SOURCE_INSPECTION_PASSED
G1 DIAGNOSTIC_PREVIEW_ALLOWED
G2 SOURCE_FRAME_CONFIRMED
G3 PAIRING_SEMANTICS_PASSED
G4 WORLD_IMPORT_PROPOSAL_READY
G5 ACCEPTED_WORLD_PATCH_READY
G6 COLLISION_PROJECTION_READY
G7 DRIVE_TEST_READY
G8 JES_TRANSFER_CANDIDATE
```

P1B nie twierdzi, że osiągnięto G3–G8.

## 7. Relacja do JES

Box3d_FunProject jest laboratorium technicznym i może używać istniejącego sample hosta, Box3D oraz prywatnego skanu. Do JES mogą później przejść:

- behavior contracts;
- source-frame rules;
- fixture’y syntetyczne;
- reimport semantics;
- failure lessons;
- neutralne schema po ponownej implementacji lub jawnej adopcji.

Nie przechodzą automatycznie:

- kod sample hosta;
- prywatny skan;
- magiczne progi;
- Box3D-specific cooker;
- struktura repozytorium laboratorium.

## 8. Falsyfikator P1B

P1B jest nieudane, jeżeli którakolwiek z sytuacji jest możliwa:

- dwa różne source revisions otrzymują ten sam revision ID;
- mirror przechodzi bez jawnej akceptacji;
- matrix nie odpowiada semantic axis roles;
- proposal zawiera manual approval;
- zmiana native handle zmienia authored identity;
- kontrakt wymaga importu renderera, Box3D albo UI;
- output jest prezentowany jako gotowy world patch.
