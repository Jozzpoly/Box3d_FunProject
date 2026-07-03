# Project direction — Jozz Vehicle Box3D Native

## Werdykt

Startujemy nową grę/sandbox pojazdów na fundamencie Box3D, ale robimy to małymi, twardymi krokami. Nie zaczynamy od pełnego buildera, pełnego renderera glTF ani fizycznie dokładnego zawieszenia wielowahaczowego.

Najpierw ma powstać **Jozz Vehicle Lab**: własny mały executable, okno, kamera, debug/primitives, ImGui i jeden testowy narożnik zawieszenia.

## Feedback Jozza z 2026-07-03

Jozz przyjął, że orientację modeli w Blockbenchu dostosuje później, dopiero kiedy modele będą widoczne w grze. To jest ważna decyzja projektowa.

Konsekwencja:

- nie blokujemy M0/M1 na finalnym ustawianiu osi w Blockbenchu;
- importer/kontrakt assetu musi pozwalać na tymczasowe korekty orientacji per asset;
- finalna orientacja authoringowa stanie się osobną pracą po uruchomieniu podglądu modeli w grze;
- kod fizyki nie może mieć porozrzucanych magicznych obrotów per model.

## Kierunek techniczny

Trzy warstwy muszą pozostać rozdzielone:

```text
Authoring asset  -> glTF + sidecar metadata
Game asset       -> render mesh + visual rig + material data
Physics prefab   -> bodies + shapes + joints + tuning parameters
```

Nie wolno traktować jednego glTF jako naraz:

- finalnego render mesha;
- fizycznej kolizji;
- rigu proceduralnego;
- prefab systemu;
- gameplayowego kontraktu montażu.

## Najważniejsze ryzyka

### 1. Zbyt szybkie wejście w renderer glTF

Renderer modeli jest potrzebny, ale może łatwo zjeść kilka dni zanim powstanie jakikolwiek feel jazdy. Dlatego pierwszy lab fizyczny powinien używać primitive renderingu/debug draw.

### 2. Zbyt skomplikowane zawieszenie za wcześnie

Multi-body suspension brzmi kusząco, bo pasuje do modelu, ale na start grozi niestabilnością, trudnym tuningiem i brakiem szybkiego feedbacku. V0 powinno użyć jednego wheel jointa na narożnik.

### 3. Zgadywanie po nazwach node'ów

Aktualne glTF-y mają duplicate root names. Importer musi używać node index/path/parent chain, a nazwa jest tylko semantycznym hintem.

### 4. Skala i orientacja

Skala musi być jawna w sidecar `.asset.json`. Orientacja authoringowa pozostaje tymczasowo "not final", ale runtime/game-space musi mieć jedną spójną konwencję.

## Aktualna rekomendowana kolejność

1. Seed branch z dokumentami, assetami, kontraktami i audytem.
2. Minimalny `jozz_vehicle_lab` executable.
3. Primitive physics smoke test.
4. Single wheel-corner physics lab.
5. Asset audit/validator rozszerzony o ostrzeżenia i błędy.
6. Minimalny glTF mesh renderer.
7. Visual rig wheel/suspension/damper/cardan.
8. Dopiero potem pełny vehicle assembly.
