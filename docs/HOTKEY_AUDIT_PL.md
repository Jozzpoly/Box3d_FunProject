# Hotkey contract — shared sample host, Jozz Vehicle i scan preview

**Status:** `CURRENT_CONFLICT_AVOIDANCE_CONTRACT`  
**Ostatni audyt kodu:** 2026-07-22  
**Źródła:** `samples/main.cpp`, `samples/jozz_vehicle_m6_rig_lab.cpp`,
`samples/jozz_scan_preview_lab.cpp`, `samples/gfx/keycodes.h`.

Ten dokument chroni przed przejęciem klawisza używanego już przez shared host albo
aktywny lab. Nie wybiera aktywnej kampanii i nie jest zgodą na dodanie skrótu.

## 1. Reguła przed każdą nową akcją

Przed dodaniem skrótu:

1. sprawdź `samples/main.cpp`;
2. sprawdź `samples/gfx/keycodes.h`;
3. sprawdź `Keyboard(...)` i `IsKeyDown(...)` w konkretnym sample;
4. sprawdź konflikt z modyfikatorami oraz zachowanie, gdy UI przechwytuje input;
5. dodaj lub zaktualizuj test/kontrakt;
6. uruchom pełny samples build i realny input review, gdy akcja zmienia UX.

Nie zakładaj, że lista z historycznego M2.5/M5 nadal jest kompletna.

## 2. Globalne skróty shared hosta

Obsługiwane przed sample-specific `Keyboard(...)`:

```text
Tab        pokaż/ukryj UI
Esc        zamknij controls window albo wyczyść selection
Ctrl+Q     wyjście z aplikacji
Ctrl+O     fuzzy sample picker
O          single step
Shift+O    większy single step
P          pauza
M          metrics drawer
R          restart bieżącego sample
[          poprzedni sample
]          następny sample
F          frame selection albo whole-scene FocusBounds/FocusHome
?          pokaż/ukryj controls window
```

Konsekwencje:

- `[` i `]` nie są dostępne dla Jozz Vehicle;
- `R` jest globalnym restartem, nie lokalnym przyciskiem pojazdu;
- `Space` celowo nie jest globalną pauzą, aby sample mogły używać go jako brake/jump;
- `Q` bez Ctrl może trafić do sample, ale `Ctrl+Q` zawsze oznacza wyjście;
- UI-captured event nie trafia do sample; release/focus-loss nadal dociera do kamery.

## 3. Aktualny M6 Suspension Rig Lab

Input ciągły:

```text
W      drive +1
S      drive -1
A      steer +1
D      steer -1
Space  brake
```

Input pojedynczy:

```text
T      toggle third-person camera
```

`R` odtwarza sample przez shared host. M6 zachowuje third-person mode, tuning session,
debug/view state, terrain seed i ostatni checkpoint zgodnie z własnym persistence
contractem.

Wszystkie pozostałe ustawienia fizyki, rigów, świata, presetów i debug view są
sterowane przez ImGui albo jawne env-hooks dla testów. Nie dodawaj skrótów dla suwaków
bez konkretnego owner UX problemu.

## 4. M5 i wcześniejsze laby

M5 używa tego samego podstawowego zestawu jazdy:

```text
W/S  napęd
A/D  skręt
Space hamulec
T    kamera trzeciej osoby
```

M2.5 używał historycznie `Q/E` do przesuwania root debug rig. To nie jest wzorzec dla
nowych globalnych skrótów i nie powinno być kopiowane do nowych sample’i.

## 5. P2A Source Visual Preview

Preview nie definiuje sample-specific keyboard handlera. Kontrole są w ImGui:

```text
Show geometry
Show tile bounds
Show metre grid
Show lab axes
Frame whole preview
per-tile visibility
```

Globalne `F` kadruje cały załadowany pack przez `FocusBounds()`. `R` restartuje preview
na tej samej ogólnej zasadzie shared hosta. Tekstury ani collision nie są kontrolowane
hotkeyem, ponieważ nie istnieją w geometry-only preview v1.

## 6. Kamera i mysz

Shared camera otrzymuje mouse release oraz focus loss nawet wtedy, gdy UI przejęło
zdarzenie. Chroni to przed utknięciem orbit/pan po puszczeniu przycisku nad panelem.

Third-person vehicle mode zmienia routing kamery i powinien być testowany realnym
inputem; automatyzacja pulpitu może nie odtworzyć raw-input zachowania wiarygodnie.

## 7. Zakazane skróty i antywzorce

- nowe znaczenie dla `[` lub `]`;
- sample-specific przejęcie globalnego `R`, `F`, `P`, `M`, `Tab`, `Esc` bez zmiany
  shared-host contractu;
- ukryty shortcut zmieniający accepted physics/defaulty;
- skrót dostępny tylko w kodzie, bez controls/UI dokumentacji;
- poleganie na samym key-down bez poprawnego release/focus-loss lifecycle;
- dodanie skrótu podczas niezwiązanej kampanii scan/governance.

## 8. Walidacja zmiany

Minimalnie:

```text
compile
→ full project gate
→ sprawdzenie konfliktów host/sample
→ controls text update
→ real interactive confirmation dla istotnej akcji
```

Zmiana shared hosta wymaga sprawdzenia unrelated samples. Zmiana prowadzenia lub
kamery pojazdu wymaga owner review.
