# Podsystem: rig · dumpery · mocowania — stan aktualny

Krótka mapa „co jest solidne / co tymczasowe / co do polishu" dla warstwy
wizualnego rigu (mocowanie + amortyzatory). Przeczytaj PRZED pracą w tym
obszarze — inaczej łatwo pomylić wizual z fizyką. Stan: 2026-07-08 (M8).

Pełna historia decyzji: `M8_SUSPENSION_RIG_REPAIR_PLAN_PL.md` §6f/§8–11.

---

## 1. Werdykt w jednym zdaniu

Wizualne amortyzatory są **dekoracją odłączoną od fizyki** — wyglądają dobrze i
podążają za żywą geometrią, ale nie są tą samą sprężyną, która niesie auto.
To świadomy stan v0; polish tej warstwy jest odłożony.

## 2. Kto czym jest (rozdział odpowiedzialności)

| Warstwa | Gdzie | Co robi |
|---|---|---|
| **Sprężyna fizyczna (wishbone)** | `jozz_vehicle_m6_suspension_rig.cpp:205,645` | `b3DistanceJoint` coilover: `coiloverChassis` (chassis) → `coiloverKnuckle` = **dolny ball-joint**. To ona trzyma masę. |
| **Sprężyna fizyczna (trailing-arm)** | `..._rig.cpp:790–861` | coilover między authored `damperArmOffset`/`damperChassisOffset`, z motion-ratio + kompensacją masy efektywnej. Tu fizyka JEST zgodna z markerami assetu. |
| **Wizualne mocowanie (mount)** | `jozz_vehicle_m6_rig_lab_mount_visual.cpp` (`LoadMountVisual`,`SetupMountRig` — po podziale R3) | model `One_Sided_wheel_mount.gltf` zrigowany per-kość na żywe ciała; wahacze `Chassis_Top/Bottom` rysowane bracket→hub (`DrawPartBetween`). |
| **Wizualne dumpery** | `rig_lab.cpp:1655–1687` + `visual_mesh_draw.cpp` (`DrawTelescopingDamper` — po podziale R4) | 2 teleskopowe amortyzatory/narożnik z `Asset_Dumper.gltf` (kości Upper/Stretch/Lower), pięte do authored `Socket_DamperUpper/Lower_L/R`, rysowane bracket→hub. |

## 3. Trzy fakty, które MUSISZ znać przed dotknięciem

1. **Wizual ≠ fizyka.** Dwa rysowane amortyzatory (straddling wahacz) NIE są
   fizycznym coiloverem — ten siedzi na dolnym ball-joincie. Zmiana wizualnych
   socketów nie zmienia jazdy; zmiana coilovera nie przesuwa wizualu.
2. **Tylko double-wishbone dostaje wizual.** `m_cornerHasMount` = true tylko dla
   `JOZZ_M6_RIG_DOUBLE_WISHBONE` z knuckle (`rig_lab.cpp:545`). Domyślnie 4/4 są
   wishbone → 4/4 mają mount+dumpery. Wariant trailing-arm ma fizyczny coilover
   zgodny z assetem, ale **zero wizualu** mocowania/dumpera.
3. **Sockety dumpera są zdublowane.** Wartości `m_damperUpper/LowerL/R` są
   hardkodowane w `rig_lab.cpp:1754` JAKO fallback i jednocześnie czytane z
   kontraktu (`readDamperSocket`, `rig_lab.cpp:501`). Jeśli zmienisz asset,
   fallbacki się rozjadą — źródłem prawdy ma być kontrakt.

## 4. Co solidne / tymczasowe / do polishu

- **Solidne (nie ruszać bez powodu):** fizyka coilovera obu rigów, per-kość rig
  mocowania, `DrawPartBetween`/`DrawTelescopingDamper` (geometria śledzenia),
  symetria L/R (naprawiona flat-frame rotation).
- **Tymczasowe (świadomie):** wizualne dumpery jako czysta dekoracja; hardkodowane
  fallbacki socketów; brak wizualu na trailing-arm.
- **Do polishu (przyszłość, po zgodzie Jozza):** (a) sprząc wizualny dumper z
  realnym coiloverem albo świadomie zostawić 2 dekoracyjne + osobno pokazać
  fizyczny; (b) dwa boczne dampery „na bok" (Jozz prosił, odłożone); (c) wizual
  dla trailing-arm; (d) usunąć duplikację socketów (kontrakt = jedyne źródło).

## 5. Plan etapowy polishu (gdy wrócimy — mały krok = obejrzany render)

Każdy etap: zmiana → build → walidator (czytaj liczby) → **zrzut ekranu** → checkpoint.

- **P1** Usuń duplikację socketów dumpera (kontrakt jako źródło, fallback tylko
  gdy brak bindingu). Render bez zmian = OK. *Niskie ryzyko, dobry pierwszy krok.*
- **P2** Wizual trailing-arm (mount+dumper) — najpierw stół 1 narożnik, potem auto.
- **P3** Decyzja Jozza: dumper wizualny sprzężony z coiloverem czy dekoracja +
  osobny znacznik fizyczny. Dopiero po decyzji — implementacja.
- **P4** Dwa boczne dampery (osobna geometria) — po P3.

Nie łącz P1–P4 w jedną zmianę. Stabilność po każdym kroku.
