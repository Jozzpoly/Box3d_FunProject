# Kontrakt assetów JV

Status: **bieżący kontrakt authoring → runtime**, zweryfikowany z kodem
`samples/jozz_vehicle_asset_contract.cpp` i sidecarami w `assets/contracts/`.
Historia pierwszego runtime V1 oraz draftu V2: `archive/consolidated_2026-08/`.

## 1. Jedna granica odpowiedzialności

```text
assets/source/*.gltf
  geometria, hierarchia node'ów i transformaty autora

assets/contracts/*.asset.json
  stabilne role, kategorie, wskazówki identyfikacji, skala i status orientacji

kod fizyki / prefab pojazdu
  jedyne źródło ciał, jointów, limitów i tuningu

lokalne raporty audytowe
  diagnostyka; nigdy runtime authority
```

Mesh ani nazwa node'a nie stają się fizyką przez sam fakt istnienia. Sidecar
wiąże asset z runtime, ale każde pole ma jawne `roleCategory` i
`physicsAuthority`.

## 2. Format faktycznie obsługiwany

`LoadJozzVehicleAssetContract` odczytuje:

- `assetId`, `assetType`;
- `source.gltf`, `source.contractVersion`;
- `units.metersPerBlockbenchUnit`;
- status orientacji i tymczasowej korekty importera;
- obiekty bindingów zagnieżdżone pod `semantics`.

Binding używany przez runtime powinien zawierać:

```json
{
  "role": "suspension.visual.wheel_center",
  "roleCategory": "visual_endpoint",
  "nameHint": "Socket_WheelCenter",
  "nodeIndexHint": 1,
  "nodePathHint": "1:Socket_WheelCenter",
  "space": "authoring_world_after_composed_transforms",
  "required": true,
  "physicsAuthority": false
}
```

`nodeIndexHint` jest podstawowym wskazaniem. `nameHint` i `nodePathHint` służą do
walidacji, ponieważ eksport glTF może mieć powtarzające się nazwy. Loader składa
transformaty rodziców przed wyliczeniem pozycji i nie ukrywa ostrzeżeń o
duplikatach.

## 3. Kategorie ról

| Kategoria | Znaczenie | Może zmieniać fizykę |
|---|---|---|
| `visual_endpoint` | punkt dla proceduralnego rigu | nie |
| `visual_part` | część mesha do ustawienia lub rozciągania | nie |
| `physics_hint` | wymiar lub kierunek wymagający osobnej walidacji | nie automatycznie |
| `physics_authority` | jawnie zatwierdzona dana prefabu fizycznego | tak, tylko po osobnej decyzji |
| `diagnostic_marker` | debug i pomiary authoringu | nie |

Domyślna zasada to:

```text
visual_endpoint != physics_authority
```

Obecne kontrakty zawieszenia mają `physicsAuthority: false`. Sockety wizualne
nie mogą przepisywać frame'ów `b3WheelJoint`, `restDrop`, limitów ani geometrii
kolizji.

## 4. Przestrzeń i orientacja

Docelowa konwencja gry:

```text
+X = przód pojazdu
+Y = góra
+Z = oś boczna; strona wynika z montażu narożnika
```

Bindingi są dziś rozwiązywane w `authoring_world_after_composed_transforms`.
Tymczasowa korekta orientacji jest dozwolona wyłącznie w warstwie importu lub
renderu i musi być opisana w sidecarze. Nie wolno ukrywać korekty per modelem w
kodzie fizyki.

Skala prototypowa `0.35 m / Blockbench unit` jest zapisana w sidecarach. Nie
kopiować jej do kolejnych stałych. Ostateczna skala assetu jest częścią
kontraktu, nie globalnym prawem projektu.

## 5. Bieżące kontrakty

`assets/contracts/` zawiera między innymi:

- `offroad_big_wheel.asset.json` — semantyka koła i markery wymiarów;
- `one_sided_wheel_mount.asset.json` — wizual narożnika zawieszenia;
- `one_sided_steering_suspension.asset.json` — wizual przedniego rigu;
- `asset_dumper.asset.json` — części teleskopowego dampera;
- `cardan_shaft.asset.json` — obecnie ograniczony wizual bez pełnej semantyki.

Dla koła bezpieczne są zwalidowane wymiary z markerów: środek/mount, promień,
szerokość, skala i status orientacji. Nie są bezpieczne jako automatyczne dane:
steering pivot, `restDrop`, limity zawieszenia ani prawo opony.

## 6. Walidacja zmiany

Zmiana sidecaru lub loadera musi udowodnić:

1. plik źródłowy istnieje i JSON się parsuje;
2. każdy binding `required` rozwiązuje się do właściwego node'a;
3. indeks, nazwa i ścieżka nie przeczą sobie;
4. skala jest dodatnia, a przestrzeń jawna;
5. rola ma oczekiwaną kategorię;
6. `physicsAuthority` nie awansował przypadkiem;
7. wizual został obejrzany po obu stronach pojazdu.

Główni właściciele kodu:

- `samples/jozz_vehicle_asset_contract.h` i `samples/jozz_vehicle_asset_contract.cpp`;
- `samples/jozz_vehicle_steering_suspension_contract.h` i `samples/jozz_vehicle_steering_suspension_contract.cpp`;
- `samples/validation/` — sondy kontraktów;
- `assets/contracts/*.asset.json` — dane runtime.

## 7. Otwarty dług

- JSON nie ma formalnego schema file ani migratora wersji;
- część ról ma wartości fallback w kodzie;
- cardan nie ma kompletnego zestawu node'ów semantycznych;
- finalna polityka orientacji assetów wymaga odbioru wizualnego Jozza;
- `physics_authority` istnieje jako kategoria, ale nie wolno jej użyć bez
  osobnego ADR i sondy regresji fizyki.
