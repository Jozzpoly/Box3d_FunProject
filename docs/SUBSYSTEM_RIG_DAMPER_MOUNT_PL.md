# Podsystem rigu wizualnego, damperów i mocowań

Status: bieżący kontrakt, zweryfikowany z kodem 2026-08-04.
Dawny snapshot i kolejka polishu: `archive/vehicle_legacy_2026-07/SUBSYSTEM_RIG_DAMPER_MOUNT_SNAPSHOT_2026-07-11_PL.md`.

## 1. Najważniejsza granica

**Wizualny damper nie jest sprężyną, która niesie samochód.**

Fizykę zawieszenia tworzą ciała i jointy w
`jozz_vehicle_m6_suspension_rig.cpp`. Modele glTF, sockety oraz
`DrawTelescopingDamper` tylko odtwarzają obraz na podstawie żywej geometrii.
Zmiana socketu wizualnego nie może po cichu zmienić jointu; zmiana jointu nie
może być uznana za poprawny wizual bez obejrzenia renderu.

## 2. Warstwy i właściciele

| Warstwa | Właściciel | Kontrakt |
|---|---|---|
| fizyczny double wishbone | `CreateWishboneCorner` | `b3DistanceJoint` od chassis do punktu na knuckle; coilover niesie obciążenie |
| fizyczny trailing arm | `CreateTrailingArmCorner` | coilover między punktami authored, z motion ratio |
| klasyczny mount wizualny | `LoadMountVisual`, `SetupMountRig` | rigowany model mocowania na żywych ciałach narożnika |
| przedni rig kierowniczy | `LoadSteeringRig`, `UseSteeringRig` | alternatywny wizual przedniej osi, wybierany z configu |
| teleskopowy model dampera | `JozzVehicleRiggedMesh::DrawTelescopingDamper` | skaluje/ustawia części Upper–Stretch–Lower między dwoma punktami świata |
| linie diagnostyczne | `DrawRigDiagnostics` | pokazują prawdziwe hardpointy i jointy, nie asset |

## 3. Które narożniki dostają wizual

`SetupMountRig` ustawia `m_cornerHasMount` tylko dla narożnika typu
`JOZZ_M6_RIG_DOUBLE_WISHBONE` z poprawnym `knuckleId`.

- przód może rysować `rig_kierowniczy` zamiast klasycznego mountu;
- tył używa klasycznego mountu, gdy jest double wishbone;
- trailing arm ma fizyczny coilover, ale nie korzysta z klasycznego mountu;
- checkbox `Model 3D zawieszenia` steruje wizualem, nie fizyką.

Nie wyciągaj z braku modelu wniosku, że narożnik nie ma sprężyny.

## 4. Sockety i fallbacki

Sockety damperów są czytane z kontraktu assetu w `LoadMountVisual`:

```text
suspension.visual.damper_upper_l
suspension.visual.damper_upper_r
suspension.visual.damper_lower_l
suspension.visual.damper_lower_r
```

Kod nadal ma wartości fallback zarówno w inicjalizacji pól, jak i przy odczycie
kontraktu. To kontrolowany dług: asset jest preferowanym źródłem, fallback ma
utrzymać czytelny model przy niepełnym kontrakcie. Nie dodawaj trzeciej kopii
tych liczb.

## 5. Jak weryfikować zmianę

Zmiana wizualna wymaga trzech oddzielnych dowodów:

1. kod buduje się i nie zmienia fizycznych parametrów jointu;
2. linie diagnostyczne nadal wskazują prawdziwe hardpointy;
3. render lewego i prawego narożnika przy skręcie oraz skoku nie ma odbicia,
   rozciągnięcia w złej osi ani odłączenia końców.

Zielony walidator liczbowy nie zastępuje punktu 3. Z kolei ładny render nie
dowodzi punktu 1.

## 6. Otwarte decyzje, nie zadania „przy okazji”

- czy dwa boczne dampery mają pozostać dekoracją, czy wizual ma wskazywać jeden
  prawdziwy coilover;
- czy trailing arm dostaje osobny model wizualny;
- czy fallbacki socketów redukujemy do jednego miejsca;
- czy rig kierowniczy staje się jedynym wizualem przedniej osi.

Każda z tych decyzji zmienia język wizualny pojazdu i wymaga oceny Jozza. Nie
łączyć ich z pracą nad kołem, manifoldem ani podatnością kontaktu.

## 7. Pliki wejściowe

- `jozz_vehicle_m6_rig_lab_mount_visual.cpp` — klasyczny mount i sockety;
- `jozz_vehicle_m6_rig_lab_steering_visual.cpp` — przedni rig kierowniczy;
- `jozz_vehicle_visual_mesh_draw.cpp` — teleskopowy damper;
- `jozz_vehicle_m6_suspension_rig.cpp` — fizyka, hardpointy i telemetria;
- `SUSPENSION_RIG_SPACE_CONVENTIONS_PL.md` — osie i przestrzenie.
