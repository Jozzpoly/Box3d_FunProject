# Podsystem rigu wizualnego, damperów i mocowań

Status: bieżący kontrakt, zweryfikowany z kodem 2026-08-04.
Historia: `archive/vehicle_legacy_2026-07/SUBSYSTEM_RIG_DAMPER_MOUNT_SNAPSHOT_2026-07-11_PL.md` oraz poprzedni dokument przestrzeni w `archive/consolidated_2026-08/`.

## 1. Najważniejsza granica

**Wizualny damper nie jest sprężyną, która niesie samochód.**

Fizykę zawieszenia tworzą ciała i jointy w
`jozz_vehicle_m6_suspension_rig.cpp`. glTF, sockety i
`DrawTelescopingDamper` odtwarzają obraz na podstawie żywej geometrii. Zmiana
socketu nie może po cichu zmienić jointu; zielona fizyka nie zastępuje odbioru
renderu.

## 2. Przestrzenie i osie

```text
physics world
  transformaty ciał Box3D i ruch jointów

chassis/root frame
  układ nadwozia, względem którego authored są hardpointy

rest wheel-center frame
  zamierzony środek koła przy zerowej translacji zawieszenia

live wheel/knuckle frames
  bieżące ciała dynamiczne, skręt i skok

authoring asset space
  Blockbench/glTF po złożeniu transformacji node'ów

game visual correction
  tymczasowa korekta renderu opisana przez kontrakt assetu
```

Konwencja gry to `+X` przód, `+Y` góra, `+Z` bok. Strona lewa/prawa wynika z
montażu narożnika. Korekta authoringu pozostaje w imporcie/renderze, nie w
jointach.

Dla historycznego prostego corner-rigu authority fizyki stanowiły chassis,
dynamiczne koło, `b3WheelJoint`, frame A w rest wheel center, frame B w środku
koła i jawny `restDrop`. W bieżącym M6 authority stanowią rzeczywiste ciała,
hardpointy i jointy tworzone przez wybrany typ narożnika. Socket zawieszenia
nadal nie jest frame'em fizycznym.

## 3. Warstwy i właściciele

| Warstwa | Właściciel | Kontrakt |
|---|---|---|
| double wishbone | `CreateWishboneCorner` | ciała/jointy i coilover niosą obciążenie |
| trailing arm | `CreateTrailingArmCorner` | authored pivot, motion ratio i coilover |
| klasyczny mount | `LoadMountVisual`, `SetupMountRig` | wizual na żywych ciałach narożnika |
| przedni rig kierowniczy | `LoadSteeringRig`, `UseSteeringRig` | alternatywny wizual przedniej osi |
| damper teleskopowy | `DrawTelescopingDamper` | Upper–Stretch–Lower pomiędzy dwoma punktami świata |
| diagnostyka | `DrawRigDiagnostics` | prawdziwe hardpointy i jointy, nie asset |

## 4. Które narożniki dostają wizual

`SetupMountRig` ustawia `m_cornerHasMount` tylko dla poprawnego narożnika double
wishbone z `knuckleId`.

- przód może użyć `rig_kierowniczy` zamiast klasycznego mountu;
- tył double wishbone używa klasycznego mountu;
- trailing arm ma fizyczny coilover, lecz nie ten sam model mountu;
- checkbox `Model 3D zawieszenia` steruje wyłącznie wizualem.

Brak modelu nie dowodzi braku sprężyny ani jointu.

## 5. Sockety i fallbacki

Bieżące role dampera:

```text
suspension.visual.damper_upper_l
suspension.visual.damper_upper_r
suspension.visual.damper_lower_l
suspension.visual.damper_lower_r
```

Kontrakt assetu jest preferowanym źródłem, a fallback w kodzie utrzymuje czytelny
wizual przy niepełnym sidecarze. Nie dodawać trzeciej kopii współrzędnych.

Gdy punkty diagnostyczne nie pokrywają się, kolejność naprawy to:

1. zweryfikuj binding i złożoną transformację glTF;
2. zweryfikuj korektę visual-space;
3. porównaj z prawdziwym hardpointem;
4. dopiero potem oceniaj model.

Nie przesuwaj jointu, żeby zamaskować błąd renderu.

## 6. Walidacja

Zmiana wymaga trzech oddzielnych dowodów:

1. build/sonda nie pokazują zmiany parametrów fizycznych;
2. diagnostyka wskazuje prawdziwe hardpointy;
3. render lewego i prawego narożnika przy skręcie i skoku nie ma odbicia,
   złej osi rozciągania ani odłączonych końców.

## 7. Otwarte decyzje właściciela

- jeden prawdziwy coilover czy dwa dekoracyjne boczne dampery;
- osobny wizual trailing arm;
- redukcja fallbacków socketów do jednego miejsca;
- czy rig kierowniczy zastępuje klasyczny wizual przedniej osi.

Nie łączyć tych decyzji z manifoldem koła ani lokalną podatnością kontaktu.

## 8. Pliki wejściowe

- `samples/jozz_vehicle_m6_rig_lab_mount_visual.cpp`;
- `samples/jozz_vehicle_m6_rig_lab_steering_visual.cpp`;
- `samples/jozz_vehicle_visual_mesh_draw.cpp`;
- `samples/jozz_vehicle_m6_suspension_rig.cpp`;
- `docs/ASSET_CONTRACT_PL.md`.
