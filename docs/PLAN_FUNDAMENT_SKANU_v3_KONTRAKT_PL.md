# Fundament importu skanu — v3 (kontrakt ról), 2026-07-24

> **Zastępuje** `PLAN_FUNDAMENT_TERENU_ZE_SKANU_2026_07_24_PL.md` (v2) jako **plan
> wykonawczy**. v2 zostaje żywy tylko jako **rejestr pomiarów** (heightfield 0,5 m
> vs mesh, gęstość siatki, chropowatość w skali koła) — te liczby są nadal
> prawdziwe i tu się do nich odwołuję, nie powielam ich.

---

## §0. Co się zmieniło i dlaczego (pivot Jozza, 2026-07-24)

Jozz przetestował skan z **pełną kolizją** (całość jako jeden mesh, łącznie z
koronami drzew). **Działa** — auto jeździ, zaczepia się często, ale to jest
**brudny, nieprzerobiony skan**. Werdykt Jozza, dosłownie:

> „zrobienie automatu którego nie będziemy używać w przyszłości nie jest dobry
> pomysłem. Potrzebujemy jak najsolidniej zaimportować ten, zbudować fundamenty
> z których będziemy korzystać przy lepszej jakości wyczyszczonych skanach."

Konsekwencja dla architektury — **inwersja osi planu v2**:

- **v2 stawiał w centrum AUTO‑KLASYFIKATOR** (flood‑fill najazdowy, segmentacja na
  6 klas, reguły roślinności). **To jest właśnie „automat, którego nie użyjemy."**
  Bo przyszłe skany **Jozz rozdzieli sam, zewnętrznie** (jego słowa z wcześniej:
  „będę oddzielał teren od dróg, domów, lasów i roślinności"). Silnik dostanie
  **gotowo rozdzielone meshe**, nie surową breję do posegmentowania.
- **v3 stawia w centrum KONTRAKT**: silnik **konsumuje role**, nie je nadaje.
  Rola przychodzi z zewnątrz (osobny plik / nazwa mesha / materiał). Dla **tego**
  skanu jest jedna rola: `Teren`. Dla przyszłych — wiele. **Kod silnika jest
  identyczny w obu przypadkach.** To jest fundament.

Zaczepianie o korony w tym skanie **zostaje zaakceptowane** — nie budujemy pod nie
maszynerii, bo czysty skan tego problemu nie będzie miał (korony wytnie Jozz
przed importem, albo dostaną rolę `Roślinność` z uproszczoną/zerową kolizją).

---

## §1. Kilka osobnych podejść do importu (i werdykt)

Wymóg Jozza: „podejdź do sprawy jeszcze z kilku osobnych stron". Cztery realne
architektury, każda z falsyfikatorem:

| # | Podejście | Co daje | Werdykt |
|---|-----------|---------|---------|
| A | **In‑engine auto‑klasyfikator** (v2) | Segmentacja bez pracy Jozza | **ODRZUCONE** — to „automat, którego nie użyjemy". Czysty skan przyjdzie już rozdzielony. |
| B | **Surowy pojedynczy mesh, bez ról** | Najszybciej wjeżdża TEN skan | Za płytkie na fundament, ale **poprawne dla brudnego skanu**. Zostaje jako **instancja M0** podejścia C. |
| C | **Kontrakt ról: import wielu meshy otagowanych rolą** | Silnik konsumuje zewnętrzną separację; TEN skan = 1×`Teren`, przyszłe = N×role; **ten sam kod** | **WYBRANE — to jest fundament (§2).** |
| D | **Ugotowany, serializowalny blob** (`b3MeshData`, BVH w środku, hash treści) | Reimport natychmiastowy, deterministyczny; „wpisanie w duszę box3d" | **ODROCZONE do M3** — ortogonalne do ról; wchodzi, gdy plaster C już jeździ. Nie blokuje. |

**Synteza:** fundament = kontrakt **C**. Pierwsza jego instancja (M0) to **B** —
ten skan jako jedno ciało roli `Teren`. **D** dokłada trwałość/wydajność później,
nie zmieniając kontraktu. **A** znika z planu wykonawczego.

Dlaczego C jest właściwym szwem, a nie A: **rola to jedyne miejsce, gdzie „co jest
gruntem" wchodzi do fizyki.** box3d już ma na to hak — split kół (`m6_suspension_rig.h`)
maskuje toczącą się kulę do `JOZZ_M6_TERRAIN_CATEGORY (0x2)`, a bok opony do
`~teren`. Czyli **rola → kategoria kolizji** i to wszystko, czego fizyka potrzebuje.
Klasyfikacja wewnątrz silnika (A) tego haka nie poprawia — tylko dokłada kod, który
i tak wyrzucimy.

---

## §2. Kontrakt (szew fundamentu) — `jozz_vehicle_scan_import`

Jeden nagłówek definiuje trwałą granicę. Wszystko powyżej (skąd wziąć rolę) i
poniżej (jak zbudować kolider) może się zmieniać niezależnie.

```
enum ScanRole {
    Teren,        // b3CreateMesh, identifyEdges=TRUE, kategoria = TEREN (0x2).
                  //   Dokladny mesh. To po tym jezdza kola. TYLKO ta rola dostaje 0x2.
    Konstrukcja,  // dom/mur/budynek: b3CreateHull / b3CreateCompound (uproszczone),
                  //   kategoria = OBJECT (0x1). Auto sie odbija, nie wjezdza.
    Roslinnosc,   // drzewo/krzew: hull pnia LUB zero kolizji (rola niesie flage),
                  //   kategoria = OBJECT (0x1).
    Dekoracja,    // tylko render, kolizji brak.
};

struct ScanMeshInput {
    // surowe trojkaty jednego mesha (juz w ukladzie swiata kafla)
    const float* positions;  int vertexCount;   // xyz
    const uint32_t* indices; int indexCount;
    ScanRole role;
    bool collide;            // dla Roslinnosc: pien(true) vs nic(false)
};

struct ScanTilePlacement {
    float originX, originZ;  // gdzie w swiecie stoi kafel (polnoc, z>200)
    float originY;           // pion (dno skanu na tej wysokosci)
    // krawedzie kafla = urwisko (wyspa + teleport, decyzja D5)
};

// Jedyne wejscie do fizyki. Buduje kolidery wg rol i zwraca uchwyty.
ScanTileBodies BuildScanTile( b3WorldId, const ScanMeshInput*, int meshCount,
                              const ScanTilePlacement& );
```

**Dla TEGO skanu** wywołanie to: `meshCount = 1`, `role = Teren`, `collide = true`.
**Dla czystego skanu** Jozz poda `meshCount = N` z różnymi rolami — **bez zmiany
`BuildScanTile`.**

Reguły niezmienne kontraktu:
1. **Tylko `Teren` dostaje `0x2`.** To pilnuje split kół (rolka po gruncie, bok
   opony po reszcie). Nie wolno tagować całego skanu jako teren (to był defekt D1
   PoC — łamał split kół).
2. **`identifyEdges = TRUE` wyłącznie w obrębie jednego mesha `Teren`.** Krawędzie
   wklęsłe działają tylko wewnątrz jednej siatki — dlatego teren MUSI być jednym
   spójnym meshem, nie sklejką per‑kafel×per‑materiał (defekt D2b PoC: 25 grup →
   sąsiedztwo trójkątów pękało na każdej granicy).
3. Placement jest **world‑space i trwały** — reimport lepszego skanu w to samo
   miejsce nie ruszy teleportów ani ręcznej pracy Jozza.

---

## §3. Ten skan konkretnie (M0 — brudny, jednorazowy)

- Źródło: lokalna paczka `JSPREV2` (7 kafli, ~107 MB) — **na razie**, bo to jest
  to, co istnieje. Reader tej paczki istnieje na gałęzi
  `agent/project-refoundation-audit-v1` (`jozz_vehicle_scan_geometry.*`).
- Cały skan → **jeden `ScanMeshInput` roli `Teren`**, `collide=true`.
- **Grunt = mesh, nie heightfield.** Dowód z v2 (nie powielam liczb): siatka skanu
  ma krawędzie ~kilka cm; heightfield 0,5 m niszczy teksturę w skali koła (3–6 cm),
  którą koło REALNIE czuje. Wymóg Jozza „teren musi być dokładny" → mesh.
- Zaczepianie o korony: **zaakceptowane, nie naprawiane** (§0).
- Placement: **północ, `z>200`**, wyspa z urwiskiem, dojazd **tylko teleportem**
  (`kWorldAnchors` w `world_layout.h`). Footprint ~1,2 km nie seamuje z płytą —
  i dobrze, wyspa jest niezależna.

---

## §4. Pytanie readera (jedyna realna decyzja F0)

Dwa formaty wejścia meshy:

- **Teraz:** `JSPREV2` (bespoke preview‑pack) — bo to leży lokalnie i jest
  zmierzone. Ryzyko: to format PODGLĄDU renderu, nie kanoniczny mesh.
- **Docelowo:** **GLB/glTF** — bo tak wychodzi z ContextCapture i tak Jozz
  wyeksportuje rozdzielone role z Blendera. `cgltf` to jedna zależność header‑only.

**Rekomendacja:** reader jest **za kontraktem `ScanMeshInput`**, więc oba formaty
dają to samo wejście. M0 czyta `JSPREV2` (żeby ruszyć na tym, co jest); reader GLB
dochodzi, gdy przyjdzie pierwszy czysty rozdzielony skan. Kontrakt się nie zmienia.
**To nie blokuje M0.**

---

## §5. Milestone'y (fundament najpierw, małe bramki)

| M | Cel | Bramka / dowód |
|---|-----|----------------|
| **M0** | Gałąź `jozz-scan-terrain-f0` od `36f3e79`; nagłówek kontraktu `scan_import.h`; port readera `JSPREV2` z gałęzi audytu (czysty czytnik, bez fizyki) | kompiluje się reader jako lib; **ten dokument** = replan od zera |
| **M1** | `BuildScanTile` dla 1×`Teren`: `b3CreateMesh`+`identifyEdges`, kategoria `0x2`, placement północ `z>200` | **render + jazda** (render is the gate): auto stoi na skanie, split kół działa |
| **M2** | Teleport „Skan (wyspa)" w `kWorldAnchors`; urwisko na krawędzi | „R"/teleport przenosi na skan i z powrotem |
| **M3** | Ugotowany blob `b3MeshData` (hash, wersja) — reimport bez rebuildu BVH | ten sam skan ładuje się z bloba, byte‑identycznie |
| **M4+** | Reader GLB; wiele ról (`Konstrukcja` hull, `Roślinność`) — **dopiero gdy Jozz da czysty rozdzielony skan** | pierwszy realny wieloskan |

Jeden milestone na sesję (reguła Jozza). M0 zamyka się TĄ sesją.

---

## §6. Co jawnie ODRZUCONE / ODROCZONE (żeby nie wróciło)

- **ODRZUCONE na stałe:** auto‑klasyfikator w silniku (flood‑fill najazdowy D8),
  segmentacja na 6 klas, reguły kolizji roślinności D7. Powód: „automat, którego
  nie użyjemy" — separację robi Jozz zewnętrznie. Reguły D7/D8 z v2 **nie są
  fundamentem**; były rozwiązaniem nie tego problemu.
- **ODROCZONE (nie porzucone):** blob D (M3), reader GLB (M4), materiały
  per‑trójkąt/tarcie (po M4), streaming wielu skanów (dużo później).

---

## §7. Co przeżywa z v2

Tylko pomiary — bo są prawdziwe i load‑bearing dla decyzji „mesh, nie heightfield"
(§3). Reszta v2 (§4.1 flood‑fill, §4.2 roślinność, D7–D9) jest **martwa** — patrz
§6. Nagłówek v2 dostaje wskaźnik tutaj.
