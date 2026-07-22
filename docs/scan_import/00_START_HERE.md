# Photogrammetry Import V2 — current start here

**Rola:** router domeny scan; nie jest checkpointem ani samodzielną zgodą na implementację.  
**Najwyższa udowodniona capability:** `TERRAIN_VISIBLE_PASS`  
**Następny product gate:** `TEXTURED_SOURCE_PREVIEW`  
**Finalna skala:** `WORLD_SCALE_VALIDATED = false`

## 1. Kolejność odczytu

```text
AGENTS.md
→ GitHub Control Issue #11
→ AI_PROJECT_MEMORY.md
→ docs/scan_import/CURRENT_STATE.md
→ aktywny PR i jego exact remote head
→ docs/PROJECT_OPERATING_PLAN_PL.md
→ docs/PROJECT_CHARTER_PL.md
→ ten router i właściwy kontrakt domenowy
```

Nie wybieraj pracy z historycznego checkpointu, zamkniętego PR-a ani nazwy brancha.
Mutable authority oraz exact head pochodzą z Control Issue.

## 2. Co jest naprawdę zakończone

Na authoritative revision właściciel wykonał prywatny seven-tile flow i potwierdził:

```text
exact private source resolution  PASS
seven-tile geometry pack         PASS
independent verification         PASS
native first load                PASS
owner geometry review            PASS
same-revision restart            PASS
runtime                           949 frames / 0 Sokol errors
highest honest capability        TERRAIN_VISIBLE_PASS
```

Znane artefakty peryferyjne rekonstrukcji są świadomie zaakceptowane dla pierwszego
geometry proof. Szczegółowy redacted zapis:

```text
docs/scan_import/TERRAIN_VISIBLE_PASS_2026_07_22_PL.md
```

Nie publikuj prywatnych ścieżek, lokalizacji, współrzędnych, source hashes, receipts ani
raw GLB/PLY/tekstur.

## 3. Czego ten milestone nie dowodzi

```text
TEXTURED_SOURCE_PREVIEW_READY        false
WORLD_SCALE_VALIDATED                false
GOLDEN_DRIVE_REGION_SELECTED         false
GLB_PLY_INTERIOR_CORRESPONDENCE      false
ACCEPTED_WORLD_PATCH_READY           false
COLLISION_PROJECTION_READY           false
FIRST_REAL_SCAN_DRIVE_PASS           false
OWNER_FUN_VERDICT                    false
```

`TERRAIN_VISIBLE_PASS` oznacza powtarzalną widoczność dokładnej geometrii źródłowej.
Nie oznacza poprawnej finalnej skali, powierzchni fizycznej ani gotowej mapy.

## 4. Obowiązująca kolejność produktu

```text
TERRAIN_VISIBLE_PASS
→ TEXTURED_SOURCE_PREVIEW
→ VEHICLE_SCALE_REFERENCE_SCENE
→ GOLDEN_DRIVE_REGION_OWNER_SELECTION
→ COLLISION_REPRESENTATION_RESEARCH
→ FIRST_REAL_SCAN_DRIVE
→ OWNER_FUN_VERDICT
```

Tekstury nie są kosmetyką po kolizji. Są potrzebne, aby Jozz rozpoznał drogę, trawę,
domy i pierwszy sensowny region jazdy. Dopiero wtedy zaakceptowany samochód zostanie
pokazany na drodze albo obok znanego obiektu i można uczciwie zwalidować skalę.

## 5. Zamknięty kontrakt geometry preview v1

Geometry-only preview v1 pozostaje zamknięty:

```text
purpose                              SOURCE_VISUAL_PREVIEW_ONLY
privacyClass                         PRIVATE_LOCAL_ONLY
sourceGeometryVisible                true
texturesIncluded                     false
internalGeometryCorrespondencePassed false
acceptedWorld                        false
collisionReady                       false
```

Nie dopisuj tekstur do v1, zachowując stare `texturesIncluded = false`. Następny etap
wymaga nowego jawnego capability/manifestu albo sąsiedniego packa, który wiąże się z tą
samą source revision bez reinterpretacji starego evidence.

## 6. Trwałe kontrakty domeny

- `ARCHITECTURE.md` — evidence → proposal → authored truth → rebuildable projections;
- `P2A_SOURCE_VISUAL_PREVIEW.md` — dokładny geometry-only format i native consumer;
- `CURRENT_STATE.md` — bieżący evidence-scoped status;
- `STATUS.md` — krótki pointer, nie drugi current state;
- `P1B_CHARTER.md`, `P1B_BUNDLE_CHARTER.md`, `P1B_OWNER_GATE_HARDENING.md` —
  historyczne kontrakty źródła, bundle i owner gate, nadal ważne jako lineage;
- `TERRAIN_VISIBLE_PASS_2026_07_22_PL.md` — zamknięty milestone evidence.

Stare komendy P1/P1B pozostają w historii Git i checkpointach. Nie są current task
queue i nie powinny być automatycznie odtwarzane na nowej source revision.

## 7. Następna kampania: wymogi przed implementacją

Przed utworzeniem teksturowanego brancha potrzebny jest bounded campaign brief, który
ustala:

1. source texture evidence: embedded images, material/primitive bindings,
   `TEXCOORD_0`, samplers i color-space;
2. jawny textured-pack identity oraz powiązanie z geometry pack/source revision;
3. 1K baseline i 2K porównanie, bez uznawania BC7 za blocker pierwszego renderu;
4. etapowy, cache'owalny cook oraz pomiar decoded RAM/GPU upload;
5. fixed-camera screenshot matrix, w tym no-texture fallback;
6. realny visibility/render-distance problem ujawniony w geometry preview;
7. scenę zaakceptowanego samochodu jako późniejszy scale reference;
8. zakaz kolizji i surface promotion do czasu owner texture/scale/ROI gate.

Historyczny raport eksperymentów tekstur jest evidence inputem:

```text
docs/PHOTOGRAMMETRY_ROADMAP_EXPERIMENT_REPORT_2026_07_15_PL.md
```

Nie jest automatycznie finalnym formatem seven-tile textured preview.

## 8. Surface i collision pozostają zaparkowane

Issue #14 przechowuje dokładny head divergent PR #7. Zawarte tam surface evidence,
query API i derivative graph mogą być porównane po teksturach, skali i wyborze Golden
Drive Region. Nie wolno:

- merge'ować całego PR #7 z rozpędu;
- uznać lowest-observed PLY grid za ground truth;
- promować kinematycznego probe PASS do accepted surface;
- uruchamiać Box3D collision przed comparative representation review;
- traktować source render mesh i physics surface jako tę samą warstwę.

## 9. Prywatny owner flow

Istniejący resumable runner pozostaje narzędziem odtwarzania zamkniętego geometry
milestone'u:

```powershell
.\tools\scan_pipeline\run_real_terrain_flow.ps1
```

Jego użycie na nowej source revision nie przyznaje automatycznie starego PASS. Każda
nowa rewizja wymaga własnego verified packa i odpowiednich owner gate'ów.

## 10. STOP conditions

Zatrzymaj implementację, gdy:

- exact branch/head albo authority są niejasne;
- prywatne dane miałyby trafić do GitHub/PR/Issue;
- tekstury miałyby zmienić semantykę zamkniętego manifestu v1;
- scale jest zgadywane bez zaakceptowanego samochodu i rozpoznawalnego obiektu;
- kolizja zaczyna się przed texture, scale-reference i owner ROI review;
- scan evidence jest przedstawiane jako authored-world truth;
- zmiana dotyka zaakceptowanej fizyki vehicle albo Box3D `src/`/`include/`;
- duży cleanup, branch deletion lub integration odbywa się bez osobnej weryfikacji.
