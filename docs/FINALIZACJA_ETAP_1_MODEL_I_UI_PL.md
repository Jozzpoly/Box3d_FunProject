# Etap 1: model danych nadwozia + rejestr modeli + UI wyboru

Część planu `PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md`. Przeczytaj
najpierw §2 i §4 planu. Agent: **Sonnet 5 (high)**. Jedna sesja, jeden commit.

**Cel jednym zdaniem:** w zakładce Nadwozie pojawia się wybór modelu nadwozia
(Brak / Rama rurowa Jozza) + suwaki przesunięcia pozycji, wybór modelu
zawieszenia przodu staje się polem configu — wszystko działa live, bez
persystencji (ta wchodzi w Etapie 2).

**Czego NIE robić w tym etapie:** żadnych zmian w `jozz_vehicle_m6_config_io.*`
(serializacja = Etap 2); żadnych zmian domyślnych wartości na „docelowe"
(Etap 3); żadnej rotacji/skali w UI; żadnego dotykania `src/`, `include/`,
`samples/gfx/`.

---

## 0. Stan zastany (zweryfikowany 2026-07-11 — sprawdź grepem, linie mogły się przesunąć)

- `samples/jozz_vehicle_m6_rig_lab_internal.h`
  - :146 `bool m_useSteeringRig = false;` — toggle nowego rigu przodu
    (env `JOZZ_M6_STEERING_RIG` + checkbox Debug).
  - :152-154 `m_bodyVisual` / `m_bodyChassisLocal` / `bool m_showBodyVisual = false;`
  - :52/:53/:65 ścieżki: sesja configu, katalog presetów, sesja debug-view.
  - :55-64 komentarz-doktryna „view toggles poza configiem" — ZAKTUALIZUJ go
    w tym etapie (patrz §6 pkt 8).
- `samples/jozz_vehicle_m6_rig_lab_mount_visual.cpp` — `LoadBodyVisual()`:
  hardkod `Nadwozie.gltf`, yaw −90° (`b3MakeQuatFromAxisAngle(b3Vec3_axisY, -0.5f*B3_PI)`),
  `p = {0, -0.60f, 0}`, skala `m_metersPerBlockbenchUnit` (0.35).
  `DrawBodyVisual()`: `DrawAtTransform(chassisLive ∘ m_bodyChassisLocal)`.
- `samples/jozz_vehicle_m6_rig_lab.cpp` — konstruktor: stash factory :69,
  session-load :78, `LoadDebugViewState()` :97, `LoadSteeringRig()` :110,
  `LoadBodyVisual()` :111, `CreateVehicle()` :112, rejestr env :115-135,
  hooki env :136-210 (`JOZZ_M6_STEERING_RIG` :160, `JOZZ_M6_BODY` :164).
  Destruktor :213-231.
- `samples/jozz_vehicle_m6_rig_lab_ui_tabs.cpp` — `DrawChassisTab()` :398
  (wzorzec: `edited |= …` → `m_structuralSetupDirty`), `DrawDebugTab()` :550
  (checkbox rigu :555 z żywym `SetupSteeringRig()`, checkbox nadwozia :564),
  tab bar :696, komentarz o bugu tożsamości zakładek :698-707 (`###TabChassis`).
- `samples/jozz_vehicle_m6_rig_lab_persistence.cpp` — `SaveDebugViewState()` :9
  / `LoadDebugViewState()` :30 (plik key=value, wzorzec do skopiowania).
- `samples/jozz_vehicle_m6_suspension_rig.h` — `struct JozzVehicleM6Config`
  :260-427 (ostatnie pole `int filterGroupIndex;` :426 — runtime-only),
  `JozzVehicleM6DefaultConfig(...)` :429, `SanitizeJozzVehicleM6Config` :442.
- Definicja defaults i sanitize żyją w `samples/jozz_vehicle_m6_suspension_rig.cpp`
  (znajdź `JozzVehicleM6DefaultConfig` grepem).
- Asset: `assets/source/Nadwozie.gltf` (samowystarczalny, data-URI). Zmierzony
  po skali 0.35: 3.28 m (Z) × 2.73 m (X) × 1.23 m (Y); pojazd: rozstaw osi
  2.50 m (przód +X), rozstaw kół 2.10 m, origin chassis 0.60 m nad linią osi.

## 1. Nowe pola w JozzVehicleM6Config

W `jozz_vehicle_m6_suspension_rig.h`, tuż PRZED `int filterGroupIndex;`
(filterGroupIndex ma zostać ostatni — jest runtime-only i celowo poza plikami):

```cpp
	// --- Visual identity (lab-only; the physics rig ignores these) ----------
	// Which body skin and which front-suspension visual model this vehicle
	// wears. Part of the config ON PURPOSE (Jozz, 2026-07-11): a preset
	// describes the WHOLE car (BeamNG pattern), so swapping presets swaps the
	// look too. Kept as fixed char buffers, not std::string, so the config
	// stays trivially copyable (probes copy it by value; see
	// jozz_probes_config.cpp). Serialized in Etap 2.
	char bodyVisualModel[JOZZ_M6_MODEL_KEY_CAP]; // registry key, e.g. "brak", "rama_rurowa"
	b3Vec3 bodyVisualOffset;					 // meters, chassis-local axes, added to the registry base position
	char frontSuspensionVisualModel[JOZZ_M6_MODEL_KEY_CAP]; // "klasyczny" | "rig_kierowniczy"
```

Stała nad structem (obok innych stałych M6, np. `JOZZ_M6_CORNER_COUNT`):

```cpp
// Cap for visual-model registry keys stored inline in the config (ASCII,
// NUL-terminated). 32 is comfortably above the longest key and keeps the
// config a plain aggregate.
#define JOZZ_M6_MODEL_KEY_CAP 32
```

W `JozzVehicleM6DefaultConfig(...)` (suspension_rig.cpp) — Etap 1 zachowuje
DZISIEJSZE zachowanie labu (nadwozie brak, rig klasyczny; Etap 3 zmieni):

```cpp
	// Visual identity defaults - Etap 1 keeps today's look (no body skin,
	// classic mount visuals); Etap 3 flips these to the validated defaults.
	std::snprintf( config.bodyVisualModel, sizeof( config.bodyVisualModel ), "brak" );
	config.bodyVisualOffset = { 0.0f, 0.0f, 0.0f };
	std::snprintf( config.frontSuspensionVisualModel, sizeof( config.frontSuspensionVisualModel ), "klasyczny" );
```

**PUŁAPKA (padding):** jeżeli funkcja defaults NIE zaczyna się od
`JozzVehicleM6Config config = {};` (zero-init całości), ogony buforów `char[32]`
zostaną śmieciowe — porównania i ewentualne memcmp w sondach będą
niedeterministyczne. Sprawdź początek funkcji; jeśli pola są wypełniane po
kolei bez zero-initu, dodaj `memset` obu buforów przed `snprintf` albo (lepiej,
jeśli nie zmienia zachowania) zero-init całego structa. Zweryfikuj, że
walidator po tej zmianie dalej PASS.

W `SanitizeJozzVehicleM6Config(...)` (suspension_rig.cpp) dopisz na końcu:

- `bodyVisualModel`: wymuś NUL na `[JOZZ_M6_MODEL_KEY_CAP-1]`; jeżeli klucza
  nie ma w rejestrze (patrz §2, `FindJozzVehicleBodyModelByKey`) → ustaw
  `"brak"` + jedna linia `WARNING` (wzorzec istniejących warningów w tej
  funkcji). UWAGA na zależność źródeł: sanitize żyje w suspension_rig.cpp —
  jeżeli include rejestru tworzyłby cykl albo zaciągał zależności labu do
  walidatora, przenieś walidację klucza do `ApplyBodyVisualFromConfig()` w
  labie, a w sanitize zostaw tylko NUL-guard + filtr znaków (dozwolone:
  `[a-z0-9_]`). Wybierz wariant, który NIE dodaje zależności do walidatora.
- `frontSuspensionVisualModel`: jak wyżej, dozwolone wartości
  `"klasyczny"`/`"rig_kierowniczy"`, fallback `"klasyczny"`.
- `bodyVisualOffset`: nie-skończone → 0; clamp każdej osi do ±2.0 m.

## 2. Rejestr modeli nadwozia

Nowy plik `samples/jozz_vehicle_body_registry.h` (+ `.cpp`). **Pamiętaj o
dopisaniu .cpp do listy źródeł sampli** — znajdź, jak inne `jozz_vehicle_*.cpp`
są zarejestrowane (grep nazwy istniejącego pliku w `samples/CMakeLists.txt`)
i zrób identycznie. Walidator NIE linkuje tego pliku (rejestr jest lab-only),
chyba że wybrałeś wariant sanitize z §1 wymagający rejestru — wtedy dodaj też
do źródeł walidatora i trzymaj rejestr wolny od zależności GL/ImGui (sam
rejestr to czyste dane — MUSI tak zostać).

```cpp
// jozz_vehicle_body_registry.h
#pragma once
#include "box3d/math_functions.h"

// One row = one selectable body skin. Curated BY HAND on purpose - a folder
// scan would happily offer Cardan_shaft.gltf as a "body". Adding a body =
// adding a row (+ the asset under assets/source/).
struct JozzVehicleBodyModelDef
{
	const char* key;	   // stable ID stored in configs/presets ([a-z0-9_])
	const char* label;	   // Polish UI label for the combo
	const char* assetPath; // repo-relative; nullptr = no mesh (the "brak" row)
	float baseYawDeg;	   // authored base rotation around chassis Y
	b3Vec3 basePos;		   // authored base position in the chassis body's local frame
};

const JozzVehicleBodyModelDef* GetJozzVehicleBodyModels( int* outCount );
// nullptr when the key is unknown - caller decides the fallback.
const JozzVehicleBodyModelDef* FindJozzVehicleBodyModelByKey( const char* key );
```

Wiersze na start (`.cpp`):

```cpp
static const JozzVehicleBodyModelDef s_bodyModels[] = {
	{ "brak", "Brak (sama bryła fizyczna)", nullptr, 0.0f, { 0.0f, 0.0f, 0.0f } },
	// Base pose solved from measured geometry, not eyeballed (commit a275947):
	// yaw -90 maps the model's rear (+Z) onto the car's rear (-X); anchoring at
	// chassis-local (0, -0.60, 0) centres the frame on wheelbase/track and
	// drops its floor to axle height.
	{ "rama_rurowa", "Rama rurowa Jozza (Nadwozie)", "assets/source/Nadwozie.gltf", -90.0f, { 0.0f, -0.60f, 0.0f } },
};
```

## 3. ApplyBodyVisualFromConfig — jeden punkt wejścia (ryzyko R4 planu)

W `mount_visual.cpp` przerób `LoadBodyVisual()` na:

```cpp
// Loads (or clears) the body skin the CONFIG names. The ONLY function allowed
// to (re)load m_bodyVisual - every path that can change
// m_config.bodyVisualModel (constructor after session load, preset load, the
// Nadwozie-tab combo, factory reset, env overrides) must funnel through here,
// or the mesh on screen silently diverges from the config (plan risk R4).
void JozzVehicleM6RigLab::ApplyBodyVisualFromConfig()
```

Logika: `FindJozzVehicleBodyModelByKey(m_config.bodyVisualModel)`; brak wpisu
lub `assetPath == nullptr` → `m_bodyVisual.Destroy()` i return; inaczej
`Destroy()` + `FindJozzVehicleAssetFile(assetPath, &path)` +
`LoadStaticGltf(path.c_str(), m_metersPerBlockbenchUnit)`; na koniec
`m_bodyChassisLocal.q = b3MakeQuatFromAxisAngle(b3Vec3_axisY, baseYawDeg * B3_PI/180)`,
`m_bodyChassisLocal.p = basePos`. **Offset NIE wchodzi do
`m_bodyChassisLocal`** — dokładany jest przy rysowaniu, żeby suwak działał
live bez przeładowania mesha:

```cpp
// DrawBodyVisual():
b3WorldTransform local = m_bodyChassisLocal;
local.p = b3Add( local.p, m_config.bodyVisualOffset );
b3WorldTransform world = b3MulWorldTransforms( chassisLive, local );
```

(Sprawdź istniejące helpery wektorowe — jeśli w tym TU dodaje się wektory
inaczej, np. operatorem albo ręcznie per-komponent, rób jak sąsiedni kod.)

Wczesny return w `DrawBodyVisual()` przez `IsLoaded()` zostaje — „brak" po
prostu nic nie rysuje. Deklaracje w internal.h: `LoadBodyVisual` →
`ApplyBodyVisualFromConfig` (zaktualizuj też komentarz przy deklaracji);
wywołanie w konstruktorze (rig_lab.cpp:111) analogicznie. Wywołanie MUSI
zostać PO session-load (config już wczytany — w Etapie 2 zacznie przynosić
klucz) i PRZED `CreateVehicle()`.

Ładowanie glTF z callbacku UI (combo) jest bezpieczne: UI i GL działają na
głównym wątku — dokładnie tak samo konstruktor ładuje mesh dziś. Rozmiar
Nadwozie.gltf (~270 KB, data-URI) = przeładowanie niezauważalne.

## 4. Migracja m_useSteeringRig → config.frontSuspensionVisualModel

Zgrepuj WSZYSTKIE użycia `m_useSteeringRig` (stan na dziś: internal.h:146
deklaracja; rig_lab.cpp:162 env hook; ui_tabs.cpp:555 checkbox;
`SetupSteeringRig`/`DrawSteeringRig`/ścieżki render w
`jozz_vehicle_m6_rig_lab_steering_visual.cpp` i `rig_lab.cpp` — zweryfikuj
grepem, mogło dojść użycie). Zamień na helper (internal.h, inline):

```cpp
	bool UseSteeringRig() const
	{
		return std::strcmp( m_config.frontSuspensionVisualModel, "rig_kierowniczy" ) == 0;
	}
```

(`<cstring>` jest już pośrednio dostępny; jeśli nie — dodaj include w
internal.h obok `<cstdio>`.) Member `m_useSteeringRig` USUŃ — kompilator
znajdzie każde przeoczone użycie. Checkbox w Debug (ui_tabs.cpp:555) zostaje
wizualnie checkboxem:

```cpp
	bool useRig = UseSteeringRig();
	if ( ImGui::Checkbox( "Nowy rig kierowniczy — przód (rozgrzewka)", &useRig ) )
	{
		std::snprintf( m_config.frontSuspensionVisualModel, sizeof( m_config.frontSuspensionVisualModel ),
					   useRig ? "rig_kierowniczy" : "klasyczny" );
		SetupSteeringRig(); // re-bake, jak dziś - toggle działa bez rebuildu
	}
```

Env hook `JOZZ_M6_STEERING_RIG` (rig_lab.cpp:160-163): zamiast
`m_useSteeringRig = atoi(v)!=0` ustawia analogicznie pole configu (snprintf
jak wyżej) **+ `SetupSteeringRig()` nie jest potrzebny w tym miejscu** —
hooki env biegną PRZED pierwszym Step/Render, a `CreateVehicle()` (:112)
już zawołał `SetupSteeringRig()`… ALE hook biegnie PO CreateVehicle, więc
re-bake JEST potrzebny: dodaj `SetupSteeringRig()` po ustawieniu pola.
Zweryfikuj kolejność w aktualnym kodzie zanim uwierzysz temu akapitowi.

## 5. UI: sekcja „Model nadwozia (wygląd)" w DrawChassisTab

Na SAMEJ GÓRZE `DrawChassisTab()` (ui_tabs.cpp:398), PRZED
`SectionHeader("Wymiary nadwozia")` i POZA łańcuchem `edited |= …` (wybór
skina jest live i NIE MOŻE ustawiać `m_structuralSetupDirty` — nic nie
wymaga Apply/rebuildu):

```cpp
	SectionHeader( "Model nadwozia (wygląd)" );
	{
		int count = 0;
		const JozzVehicleBodyModelDef* models = GetJozzVehicleBodyModels( &count );
		int current = 0; // fallback to "brak" when the key is unknown
		for ( int i = 0; i < count; ++i )
		{
			if ( std::strcmp( models[i].key, m_config.bodyVisualModel ) == 0 )
			{
				current = i;
			}
		}
		ImGui::SetNextItemWidth( 14.0f * ImGui::GetFontSize() );
		if ( ImGui::BeginCombo( "##BodyModelSelect", models[current].label ) )
		{
			for ( int i = 0; i < count; ++i )
			{
				if ( ImGui::Selectable( models[i].label, i == current ) && i != current )
				{
					std::snprintf( m_config.bodyVisualModel, sizeof( m_config.bodyVisualModel ), "%s", models[i].key );
					ApplyBodyVisualFromConfig();
				}
			}
			ImGui::EndCombo();
		}
		HelpMarker( "Wygląd nadwozia - czysto wizualna skóra na bryle fizycznej. Nie zmienia fizyki: "
					"bryła kolizyjna i jej wymiary (sekcje niżej) działają jak dotąd. "
					"Wybór wejdzie do presetów i przeżyje R (Etap 2)." );

		bool offsetEdited = false;
		offsetEdited |= ImGui::SliderFloat( "Przesunięcie przód/tył", &m_config.bodyVisualOffset.x, -0.50f, 0.50f, "%.2f m" );
		offsetEdited |= ImGui::SliderFloat( "Przesunięcie góra/dół", &m_config.bodyVisualOffset.y, -0.50f, 0.50f, "%.2f m" );
		offsetEdited |= ImGui::SliderFloat( "Przesunięcie lewo/prawo", &m_config.bodyVisualOffset.z, -0.50f, 0.50f, "%.2f m" );
		(void)offsetEdited; // live - DrawBodyVisual reads the offset every frame
		HelpMarker( "Dostrojenie pozycji modelu względem bryły fizycznej, w osiach nadwozia "
					"(X przód, Y góra, Z lewo). Baza per model siedzi w rejestrze - to jest korekta." );
		if ( ImGui::Button( "Wyzeruj przesunięcie" ) )
		{
			m_config.bodyVisualOffset = { 0.0f, 0.0f, 0.0f };
		}
	}
	ImGui::Separator();
```

Uzasadnienie miejsca: flow Jozza to „wybierz nadwozie → dopasuj do niego
zawieszenie/wymiary", więc wybór stoi PIERWSZY; sekcje wymiarów (z Apply
i gwiazdką `Nadwozie *`) zostają pod spodem bez zmian. **Nie dotykaj
identyfikatora zakładki `###TabChassis`** (bug tożsamości — komentarz
ui_tabs.cpp:698-707).

Suwaki: zakres ±0.50 m to korekta, nie teleport (preferencja ciasnych
zakresów). Sanitize clampuje do ±2.0 m, więc ręcznie edytowany plik nie
wystrzeli modelu w kosmos, a suwak poza swój zakres nie wyjdzie.

Checkbox „Nadwozie 3D (rama Jozza) na chassis" w Debug (ui_tabs.cpp:564):
zmień etykietę na `"Pokaż nadwozie 3D"` i tooltip na krótszy: to już tylko
przełącznik WIDOKU (model wybiera zakładka Nadwozie); wspomnij w tooltipie,
że wyłączenie przydaje się w tym labie do oglądania zawieszenia.

## 6. m_showBodyVisual: default ON + zapis do debug-session

1. internal.h:154 → `bool m_showBodyVisual = true;` (przy defaultowym
   `bodyVisualModel="brak"` i tak nic się nie rysuje — zmiana defaultu jest
   bezpieczna wizualnie w Etapie 1, a od Etapu 3 będzie oczekiwana).
2. Konstruktor rig_lab.cpp:88-92 — jawne defaulty view-flag przed
   `LoadDebugViewState()`: dopisz `m_showBodyVisual = true;` do tego bloku
   (spójność wzorca).
3. `SaveDebugViewState()` (persistence.cpp:19-27): dopisz linię
   `file << "showBodyVisual=" << ( m_showBodyVisual ? 1 : 0 ) << "\n";`
4. `LoadDebugViewState()` (persistence.cpp:48-71): dopisz gałąź
   `else if ( key == "showBodyVisual" ) { m_showBodyVisual = value; }`
5. Env `JOZZ_M6_BODY` (rig_lab.cpp:164-167) zostaje jak jest (steruje
   `m_showBodyVisual` — widocznością).
6. NOWY env `JOZZ_M6_BODY_MODEL` (do screenshotów headless): po bloku
   `JOZZ_M6_BODY` dodaj hook, który snprintf-uje wartość do
   `m_config.bodyVisualModel` i woła `ApplyBodyVisualFromConfig()`.
7. Rejestr env w komentarzu (rig_lab.cpp:115-135): dopisz
   `//   BODY_MODEL  key  select the body skin by registry key (e.g. rama_rurowa)`
   oraz zaktualizuj README_FOR_AGENTS.md (lista env + licznik hooków 15→16;
   zgrep `getenv( "JOZZ_M6` musi się zgadzać z komentarzem — to tripwire).
8. Komentarz-doktryna internal.h:55-64: dopisz zdanie, że TOŻSAMOŚĆ pojazdu
   (bodyVisualModel/offset, frontSuspensionVisualModel) jest od 2026-07-11
   świadomie W configu (decyzja Jozza, plan finalizacji §2.2), a plik
   debug-session trzyma wyłącznie widok.

## 7. Bramka wyjścia (wszystko, po kolei)

1. `tools/gate.ps1` → `BRAMKA: build 3/3 OK - walidator OK - test PASS -
   smoke 0 err`. Wyniki sond fizycznych bez zmian (nowe pola nikt fizyczny
   nie czyta).
2. **Render is the gate** — trzy zrzuty (PowerShell; wzorzec env+exe jak w
   `tools/quad_shot.ps1`):
   - zakładka: `JOZZ_M6_TAB=1` + `--sample-name "M6 Suspension Rig Lab"
     --frames 90 --screenshot <scratchpad>\etap1_tab.png` → combo + 3 suwaki
     + przycisk widoczne, sekcja NAD „Wymiary nadwozia".
   - ciało: `JOZZ_M6_BODY_MODEL=rama_rurowa` + kamera profilowa
     (`JOZZ_M6_CAM="180,14,9,0,1,0"`, `JOZZ_M6_DIAG=0`) → rama na aucie,
     orientacja jak w commicie a275947 (spojler nad TYLNYMI kołami).
   - offset: to samo + tymczasowo wpisany offset Y (np. przez suwak ręcznie
     albo krótki test z configiem) → rama wyraźnie wyżej. OBEJRZYJ PNG.
   Po zrzutach POSPRZĄTAJ zmienne env (wzorzec: gate.ps1 New-GateQuad).
3. Test manualny combo: Brak→Rama→Brak bez wycieku/crasha (Destroy między
   loadami), suwaki działają live, „Wyzeruj" zeruje.
4. Checkbox Debug „Pokaż nadwozie 3D" ukrywa/pokazuje niezależnie od combo;
   przeżywa R (debug-session). Wybór MODELU po R wraca do „brak" — to jest
   ZNANY, zapisany limit Etapu 1 (persystencja = Etap 2); zaznacz go w
   sekcji „Wynik" na dole tego pliku.
5. `tools/doc_drift_check.ps1` czysty (README zaktualizowany w tym samym
   commicie; jeśli tripwire zażąda innych doków — zaktualizuj je, nie omijaj).
6. Commit (message wg wzorca repo: co+dlaczego+bramka) + push na
   `jozz-vehicle-sandbox-m0`. Do commita wchodzi też sekcja „Wynik" poniżej
   i odhaczenie Etapu 1 w §7 planu.

## 8. Pułapki zebrane (przeczytaj dwa razy)

- `lastInObject`/serializacja — NIE w tym etapie. Jeśli dotykasz config_io —
  zabłądziłeś.
- Zero-init buforów char[] w defaults (padding → niedeterminizm sond) — §1.
- Sekcja UI poza `edited |=` — inaczej wybór skina zapali gwiazdkę „Nadwozie *"
  i zażąda Apply, którego nie potrzebuje (i odpali bug-wzorzec tożsamości
  zakładek, przed którym chroni ###TabChassis).
- `ApplyBodyVisualFromConfig` PO session-load, PRZED CreateVehicle; env-hooki
  PO CreateVehicle wymagają własnego Apply/re-bake (§4, §6.6).
- Nowy .cpp → samples/CMakeLists.txt, inaczej linker error dopiero na gate.
- Walidator nie może zassać zależności labu (rejestr = czyste dane).
- Kompilacyjnie wymuszona migracja: usuń member `m_useSteeringRig`, nie
  zostawiaj go „na wszelki wypadek".

## Wynik (wypełnia agent wykonujący)

- Commit: (patrz commit tego pliku - "Etap 1: model nadwozia + UI wyboru").
- Zrzuty obejrzane: TAK, wszystkie trzy.
  - Zakładka Nadwozie: combo "Brak (sama bryła fizyczna)" + 3 suwaki przesunięcia
    + przycisk "Wyzeruj przesunięcie", sekcja NAD "Wymiary nadwozia", bez
    gwiazdki dirty. Zgodne z projektem.
  - Ciało na aucie (`JOZZ_M6_BODY_MODEL=rama_rurowa`, kamera profilowa): rama
    widoczna, spojler nad TYLNYMI kołami (zgodnie z zweryfikowaną orientacją z
    a275947). W większości zasłonięta przez tan bryłę kolizyjną - to ZNANY,
    udokumentowany artefakt (occlusion problem, plan §2.3 R5), NIE regresja
    tej sesji; naprawa odłożona do Etapu 3 (warianty B/A/C).
  - Offset: tymczasowy env-hook testowy (`JOZZ_M6_BODY_OFFSET_Y_TEMP_TEST`,
    USUNIĘTY z kodu przed commitem) z Y=+0.35 pokazał ramę wyraźnie wyżej,
    nogi/spód ramy widoczne NAD bryłą kolizyjną - potwierdza, że offset
    dokładany jest przy rysowaniu (live, bez przeładowania mesha).
- Manualny test combo: NIE wykonany jako dosłowne kliknięcie myszą w UI -
  środowisko wykonawcze tej sesji nie ma narzędzia do automatyzacji natywnego
  okna Win32/ImGui. Zamiast tego zweryfikowano RÓWNOWAŻNĄ ścieżkę kodu: combo
  i env-hook `JOZZ_M6_BODY_MODEL` wywołują dokładnie tę samą funkcję
  `ApplyBodyVisualFromConfig()` (jeden punkt wejścia, §3), która bezwarunkowo
  woła `Destroy()` przed każdym (re)ładowaniem - więc cykl Brak→Rama→Brak nie
  może wyciekać zasobów niezależnie od wyzwalacza. Zrzuty z różnych wartości
  `JOZZ_M6_BODY_MODEL` (brak w zrzucie 1, rama_rurowa w zrzutach 2-3) w osobnych
  uruchomieniach procesu potwierdzają obie gałęzie się renderują poprawnie.
  Rekomendacja dla Jozza: przy pierwszej okazji z klawiaturą/myszą przy
  aplikacji, ręcznie poklikaj combo raz - powinno być formalnością.
- Znane limity: wybór modelu (i offset, i model zawieszenia przodu) NIE
  przeżywa R ani presetów - to jest ZNANY, zapisany limit Etapu 1;
  persystencja wchodzi w Etapie 2 (pułapka `lastInObject` już opisana tam).
- Rozbieżności ze stanem zastanym opisanym wyżej:
  - `JozzVehicleM6DefaultConfig`/`SanitizeJozzVehicleM6Config` NIE żyją w
    `jozz_vehicle_m6_suspension_rig.cpp` jak zakładał ten dokument w §0/§1 -
    faktycznie są w `jozz_vehicle_m6_geometry.cpp` (zweryfikowane grepem przed
    edycją). Edycje poszły tam; zachowanie i miejsce w strukturze pliku bez
    zmian.
  - Walidacja klucza `frontSuspensionVisualModel` w `SanitizeJozzVehicleM6Config`
    NIE wymagała przenoszenia do laba (§1 przewidywał taką możliwość) - to pole
    ma tylko dwie literalne wartości (brak osobnego rejestru), więc pełna
    walidacja z fallbackiem "klasyczny" siedzi bezpiecznie w tej samej funkcji
    co reszta sanitize, bez zależności do rejestru nadwozi. `bodyVisualModel`
    poszedł zgodnie z planem: tylko filtr znaków w sanitize, pełna walidacja
    klucza (z fallbackiem "brak") w `ApplyBodyVisualFromConfig()` w labie.
  - Wszystkie pozostałe założenia §0 (linie, sygnatury, wzorce) potwierdzone
    zgodne ze stanem faktycznym.
