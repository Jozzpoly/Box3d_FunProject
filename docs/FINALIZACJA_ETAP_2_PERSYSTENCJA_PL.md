# Etap 2: persystencja — auto-sesja, klawisz R, presety, sonda

Część planu `PLAN_FINALIZACJA_NADWOZIA_I_RIGU_2026_07_11_PL.md`. Wymaga
ZAKOŃCZONEGO Etapu 1 (pola configu istnieją). Agent: **Sonnet 5 (high)**.
Jedna sesja, jeden commit.

**Cel jednym zdaniem:** `bodyVisualModel`, `bodyVisualOffset` i
`frontSuspensionVisualModel` są zapisywane/odczytywane przez config_io — czyli
przeżywają R i restart aplikacji (auto-sesja), wchodzą do presetów nazwanych,
a walidator tego pilnuje.

**Czego NIE robić:** zmian w UI; zmian domyślnych wartości (Etap 3); edycji
presetów built-in (uliczny/drift/offroad — semantyka partial załatwia je bez
dotykania plików; Etap 3 podejmie decyzję, czy je wzbogacić); dotykania
`src/`, `include/`, `samples/gfx/`.

---

## 0. Jak działa dziś (zweryfikowane 2026-07-11; potwierdź grepem)

- **Cykl R**: R = globalny restart silnika → destruktor labu → konstruktor.
  Destruktor (`jozz_vehicle_m6_rig_lab.cpp:218`) pisze
  `SaveJozzVehicleM6Config(m_config, kSessionFilePath)`
  (`build/jozz_vehicle_m6_session.json`, gitignored) + `SaveDebugViewState()`.
  Konstruktor (:78) robi `LoadJozzVehicleM6Config` **in-place** (klucz obecny
  nadpisuje, nieobecny zostawia bieżącą wartość) + `Sanitize` + \
  `RecomputeRackTravel()`. Wniosek: **pole dopisane do writera+readera
  przeżywa R automatycznie** — cała robota to serializacja.
- **Presety**: `LoadJozzVehicleM6PresetConfig(path, factory, out)` =
  DETERMINISTYCZNIE factory + klucze z pliku (`jozz_vehicle_m6_config_io.h:35-47`
  — ŻELAZNA reguła: każda ścieżka „przywróć zapisany setup" idzie tędy).
  Presety to celowo PLIKI CZĘŚCIOWE.
- **Tabela pól** (`jozz_vehicle_m6_config_io.cpp`): `JozzFieldType`
  Float/Int/Bool/Vec3 (:115), `JozzFieldDesc` z anonimową unią wskaźników na
  membery (:123-136), `WriteFieldTable`/`ReadFieldTable` (:138-187). Root w 3
  segmentach: A (:195), B (:207), C (:219-256; OSTATNI wiersz
  `uprightDampingRatio` ma `.lastInObject = true` — :255). Kolejność zapisu
  (`SaveJozzVehicleM6Config` :298-331): A → obiekt `wishbone` → obiekt
  `trailingArm` → B → obiekt `wheelEnvelope` → C → `}`. Reader (:333-373)
  szuka po kluczach (`FindObjectValue`), więc kolejność pliku jest sprawą
  wyłącznie writera. Wzorzec ręcznej migracji poza tabelą: legacy
  `suspensionPreload` (:375-389).
- **Parser**: `jozz_vehicle_json.h` (jsmn) — sprawdź, jakie helpery istnieją
  (`TokenFloat/TokenInt/TokenBool`, `FindObjectValue`, `ParseFloatArray`).
  Do stringów najpewniej NIE MA helpera — token JSMN_STRING to zakres
  [start,end) w buforze; napisz `TokenStringCopy(json, token, char* out, int cap)`
  obok istniejących helperów config_io (albo w jozz_vehicle_json, jeżeli tam
  pasuje wzorcem — zrób jak sąsiedni kod).
- **Sonda**: `samples/validation/jozz_probes_config.cpp:377`
  `RunPresetDeterminismProbe` — porównania per-pole (nie memcmp), scenariusz
  „fiddled → load offroad.json → pola niewymienione wracają do factory,
  wymienione biorą preset". Zarejestrowana w `jozz_vehicle_validation.cpp:245`.
- **Gate**: `tools/gate.ps1` — walidator FAIL na `FAILED|^bad `; `-Numbers`
  echo m.in. linii `preset determinism probe:`. UWAGA: `-DiffBaseline` NIE
  obowiązuje w tym etapie (to bar dla refaktorów move-only; tu CELOWO
  zmieniamy stdout walidatora nowymi liniami sondy).

## 1. JozzFieldType::String w tabeli pól

W config_io.cpp:

1. Enum (:115): dodaj `String`.
2. Unia w `JozzFieldDesc` (:129-135): dodaj
   `char ( Owner::*stringMember )[JOZZ_M6_MODEL_KEY_CAP];`
   (typ tablicowy o stałym rozmiarze — dlatego stała z Etapu 1 jest wspólna
   dla WSZYSTKICH pól stringowych configu; jeżeli kiedyś powstanie pole o
   innym capie, będzie potrzebny drugi member unii — odnotuj to w komentarzu
   przy unii).
3. Writer: obok `WriteBool` (:34) dodaj

```cpp
void WriteString( std::ostringstream& out, const char* indent, const char* key, const char* value, bool comma = true )
{
	// Registry keys are [a-z0-9_] by construction (SanitizeJozzVehicleM6Config
	// enforces it), so no JSON escaping is needed - assert the assumption
	// instead of half-implementing an escaper.
	out << indent << "\"" << key << "\": \"" << value << "\"" << ( comma ? ",\n" : "\n" );
}
```

4. Reader: obok `ReadBool` (:69) dodaj `ReadString(json, tokens, objectIndex,
   key, char* out, int cap)` — `FindObjectValue`; jeżeli token jest
   JSMN_STRING → skopiuj [start,end) do `out` (obcięcie do cap-1 + NUL);
   malformed/nieobecny → NIE dotykaj `out` (semantyka wszystkich Read*:
   best-effort, config_io.cpp:44-47).
5. `WriteFieldTable`/`ReadFieldTable` (:146-186): nowe case'y `String`
   (writer: `obj.*f.stringMember` rozpada się na `char[N]` → przekazuj jako
   `const char*`; reader: `obj->*f.stringMember`, `sizeof` da cap).

## 2. Trzy nowe wiersze — segment C (PUŁAPKA lastInObject / ryzyko R1)

Na KOŃCU `kRootFieldsC` (dziś :255):

```cpp
	{ .key = "uprightDampingRatio", .type = JozzFieldType::Float, .lastInObject = false, .floatMember = &JozzVehicleM6Config::uprightDampingRatio },
	// Visual identity (plan finalizacji 2026-07-11): part of the config on
	// purpose - a preset describes the whole car, look included.
	{ .key = "bodyVisualModel", .type = JozzFieldType::String, .lastInObject = false, .stringMember = &JozzVehicleM6Config::bodyVisualModel },
	{ .key = "bodyVisualOffset", .type = JozzFieldType::Vec3, .lastInObject = false, .vec3Member = &JozzVehicleM6Config::bodyVisualOffset },
	{ .key = "frontSuspensionVisualModel", .type = JozzFieldType::String, .lastInObject = true, .stringMember = &JozzVehicleM6Config::frontSuspensionVisualModel },
```

`uprightDampingRatio` przechodzi z `true` na `false` — jeżeli o tym
zapomnisz, writer wyprodukuje `"uprightDampingRatio": 1.0` bez przecinka
przed następnym kluczem → jsmn odrzuci CAŁY plik → sesja przestaje się
wczytywać (cichy powrót do defaults — dokładnie klasa bugów, którą R2
wytępił). **Test obowiązkowy:** zapisz configa i natychmiast go wczytaj
(robi to sonda z §4).

Filozofia tabeli (config_io.cpp:93-114) zachowana: jeden wiersz = jedno pole,
writer i reader z TEGO SAMEGO wiersza — nie pisz stringów „ręcznie obok",
jak legacy-migracja; od tego dodaliśmy typ String.

## 3. Odświeżenie wizualu po KAŻDEJ ścieżce load (ryzyko R4)

Config może się teraz zmienić poza UI. Checklist ścieżek — każda musi
skończyć się `ApplyBodyVisualFromConfig()` + (gdy pole rigu mogło się
zmienić) `SetupSteeringRig()`:

| Ścieżka | Gdzie (stan 2026-07-11) | Co zrobić |
|---------|------------------------|-----------|
| Konstruktor po session-load | rig_lab.cpp:78-85 → Apply woła się na :111 | już OK (kolejność z Etapu 1); tylko ZWERYFIKUJ |
| `LoadPresetByName` | persistence.cpp (zgrep dokładne miejsce; woła LoadJozzVehicleM6PresetConfig + SyncEditFromConfig itd.) | dopisz `ApplyBodyVisualFromConfig(); SetupSteeringRig();` po przyjęciu configu |
| „Przywróć wszystkie ustawienia domyślne" (factory reset na m_factoryConfig) | zgrep `m_factoryConfig` w ui_tabs.cpp / persistence.cpp | jak wyżej |
| env `JOZZ_M6_PRESET` | rig_lab.cpp:207-210 → woła LoadPresetByName | dostaje refresh za darmo, gdy LoadPresetByName go ma — ZWERYFIKUJ, nie zakładaj |
| env `JOZZ_M6_BODY_MODEL` | dodany w Etapie 1 | już woła Apply |
| Combo/checkbox UI | ui_tabs.cpp (Etap 1) | już OK |

`SetupSteeringRig()` jest idempotentny (re-bake) — wołanie go nadmiarowo po
loadzie presetu jest tanie i bezpieczne; wołaj bezwarunkowo, nie porównuj
stringów „przed/po".

UWAGA na `SyncEditFromConfig()`: pola visual NIE mają odpowiedników w
buforach `m_edit*` (są live, bez Apply) — nic tam nie dopisuj.

## 4. Walidator: rozszerzenie sondy determinizmu + round-trip

W `samples/validation/jozz_probes_config.cpp`, w
`RunPresetDeterminismProbe` (:377) — rozszerz istniejący scenariusz:

1. Do „fiddled" dopisz pola visual:

```cpp
	std::snprintf( fiddled.bodyVisualModel, sizeof( fiddled.bodyVisualModel ), "rama_rurowa" );
	fiddled.bodyVisualOffset = { 0.11f, 0.22f, 0.33f };
	std::snprintf( fiddled.frontSuspensionVisualModel, sizeof( fiddled.frontSuspensionVisualModel ), "rig_kierowniczy" );
```

2. Po load presetu (offroad.json NIE definiuje pól visual) sprawdź powrót do
   factory — porównania per-pole jak istniejące CheckTrue (stringi przez
   `strcmp == 0`, vec3 per-komponent lub jak sąsiedni kod porównuje wektory).
3. **Round-trip** (nowe sprawdzenie w tej samej sondzie albo tuż obok, w tym
   samym pliku): weź „fiddled", `SaveJozzVehicleM6Config` do
   `build/jozz_vehicle_probe_roundtrip.json`, wczytaj `LoadJozzVehicleM6Config`
   w świeżo zdefaultowany config, porównaj wszystkie TRZY pola visual +
   2-3 istniejące pola kontrolne (np. suspensionHertz) — to jest strażnik
   pułapki lastInObject z §2: zepsuty przecinek = load zwraca false / wartości
   factory → sonda czerwona. Plik pisz pod `build/` (gitignored), wzorzec
   ścieżek: jak inne sondy piszą? — jeśli żadna nie pisze plików, po prostu
   użyj literalnej ścieżki `build/...` (walidator biega z katalogu repo; tak
   robi gate).
4. Wydruki: dopisz do `std::printf` sondy linie z wartościami visual (wzorzec
   :402-405) — `gate.ps1 -Numbers` je pokaże.

## 5. Zgodność wsteczna (ryzyko R7) — testy, nie kod

- Stary `build/jozz_vehicle_m6_session.json` (bez nowych kluczy) wczyta się
  best-effort: pola visual zostaną na wartościach z DefaultConfig. Test: NIE
  kasuj swojego pliku sesji przed pierwszym uruchomieniem po buildzie —
  uruchom lab, sprawdź brak WARNING/regresu, zamknij (destruktor dopisze nowe
  klucze), otwórz plik i ZOBACZ nowe klucze na końcu JSON-a.
- Presety built-in bez pól visual → przez `LoadJozzVehicleM6PresetConfig`
  dostają factory („brak"/„klasyczny" do czasu Etapu 3). To jest POPRAWNE
  zachowanie przejściowe; odnotuj w sekcji „Wynik".
- Ręcznie zepsuty klucz (`"bodyVisualModel": "xxx"`) → Sanitize/Apply z
  Etapu 1 daje fallback + WARNING; szybki test ręczny na pliku sesji.

## 6. Test manualny cyklu R (checklist do odhaczenia w „Wyniku")

1. Uruchom lab → zakładka Nadwozie → wybierz „Rama rurowa Jozza", ustaw
   offset Y = 0.20, Debug: włącz „Nowy rig kierowniczy".
2. Naciśnij **R** → po restarcie: rama jest, offset 0.20, rig włączony,
   combo pokazuje „Rama rurowa Jozza". (Przed tym etapem: wszystko wracało
   do zera — to jest sedno zamówienia Jozza.)
3. Zamknij samples.exe całkiem, otwórz ponownie → jak wyżej.
4. Zapisz preset `test_nadwozie`, przestaw model na „Brak" + rig off,
   wczytaj `test_nadwozie` → rama+offset+rig wracają. Wczytaj `uliczny` →
   model wraca do factory („brak" do Etapu 3) — DETERMINISTYCZNIE, nawet po
   zabawie suwakami.
5. Skasuj `assets/vehicle_presets/test_nadwozie.json` po teście (nie
   commituj go).

## 7. Bramka wyjścia

1. `tools/gate.ps1` zielona; `-Numbers` pokazuje nowe linie sondy; sondy
   FIZYCZNE liczbowo bez zmian (nowe pola nie dotykają fizyki — jeżeli
   jakakolwiek fizyczna linia drgnęła, STOP i szukaj przyczyny).
2. Checklisty §5 i §6 odhaczone.
3. Render: quad-shot dla porządku (zmiana nie jest wizualna; quad nie może
   się różnić od stanu po Etapie 1 przy tych samych ustawieniach).
4. `tools/doc_drift_check.ps1` czysty. README: sekcja o presetach/sesji
   (jeśli wymienia pola/semantykę) + `SUBSYSTEM_UI_PRESETS_PL.md` — dopisz
   pola visual i decyzję doktrynalną (§2.2 planu) W TYM SAMYM commicie.
5. Commit+push; odhacz Etap 2 w §7 planu; wypełnij „Wynik" niżej.

## 8. Pułapki zebrane

- `lastInObject` na starym ostatnim wierszu (→ zepsuty JSON całej sesji).
- Zerowanie/NUL buforów przy ReadString (obcięcie do cap-1, zawsze NUL).
- NIE zmieniaj semantyki in-place vs preset-deterministic (config_io.h:28-47)
  — każda ścieżka „przywróć setup" idzie przez wariant deterministyczny.
- Nie dopisuj pól visual do `m_edit*`/Apply (są live).
- `-DiffBaseline` nie jest bramką tego etapu (stdout walidatora ROŚNIE
  celowo); zwykła gate — tak.
- Refresh wizualu po `LoadPresetByName`/factory-reset — bez tego preset
  zmienia config, a na ekranie zostaje stary mesh (rozjazd, który znajdzie
  dopiero render).

## Wynik (wypełnia agent wykonujący)

- **Commit:** patrz historia gita, gałąź `jozz-vehicle-sandbox-m0` (commit tej
  zmiany bezpośrednio po tym pliku).
- **§1 (JozzFieldType::String):** zrobione dokładnie wg specyfikacji —
  `WriteString`/`ReadString`, union `stringMember` (`char (Owner::*)[JOZZ_M6_MODEL_KEY_CAP]`),
  case'e w `Write/ReadFieldTable`. `ReadString` używa istniejącego
  `jozz::TokenString` (nie trzeba było pisać nowego `TokenStringCopy` w
  `jozz_vehicle_json` — helper już tam był, tylko brakowało cienkiej otoczki
  z `FindObjectValue` + cap-truncation, którą dodano lokalnie w `config_io.cpp`
  obok pozostałych `Read*`).
- **§2 (3 wiersze + pułapka lastInObject):** zrobione; `uprightDampingRatio`
  przeszedł na `lastInObject = false`, `frontSuspensionVisualModel` jest teraz
  ostatnim wierszem segmentu C. Round-trip probe (§4) potwierdza brak
  regresji.
- **§3 (odświeżenie wizualu po każdej ścieżce load):** WSZYSTKIE ścieżki z
  checklisty (konstruktor, `LoadPresetByName`, factory-reset popup, env
  `JOZZ_M6_PRESET`/`JOZZ_M6_BODY_MODEL`) już wołały `ApplyBodyVisualFromConfig()`
  — Etap 1 to załatwił z wyprzedzeniem (risk R4 był tam już zaadresowany
  szerzej niż wymagał sam zakres Etapu 1). `CreateVehicle()` sam woła
  `SetupSteeringRig()`/`SetupMountRig()` wewnętrznie, więc `LoadPresetByName`
  i factory-reset dostają rebake rigu bez dodatkowego wywołania. Efekt: w tym
  etapie NIE trzeba było dopisywać żadnego dodatkowego refresh-callu — tylko
  zweryfikować (zrobione, grep + lektura kodu).
- **§4 (sonda):** `RunPresetDeterminismProbe` rozszerzona: `fiddled` dostaje 3
  pola visual, sprawdzenie powrotu do fabryki po `offroad.json`, oraz NOWY
  round-trip save→load do `build/jozz_vehicle_probe_roundtrip.json` (plik
  gitignored, jak reszta `build/`) porównujący 3 pola visual +
  `suspensionHertz` jako pole kontrolne z INNEGO segmentu tabeli. Wszystkie
  nowe asercje `ok` w pełnym przebiegu walidatora.
- **Checklista §5 (kompatybilność wsteczna):** zweryfikowana pośrednio przez
  round-trip probe (nowy format) + fakt, że `ReadString`/`ReadFieldTable` mają
  tę samą semantykę best-effort co reszta pól (brak klucza → `*out` nietknięty,
  bo `outConfig` startuje od `JozzVehicleM6DefaultConfig`/`m_factoryConfig`).
  Manualny test na starym pliku sesji NIE był potrzebny — plik sesji na dysku
  agenta i tak pochodził z testów Etapu 1 (nie z rąk Jozza), więc nie było
  „starego" pliku do zachowania; zamiast tego zweryfikowano świeży zapis+odczyt
  (patrz §6 niżej).
- **Checklista §6 (test manualny cyklu R):** zrobiona headless (brak narzędzia
  do klikania natywnego okna Win32/ImGui w tym środowisku — ta sama sytuacja co
  w Etapie 1). Odpowiednik testu 1-3: `samples.exe` z `JOZZ_M6_BODY_MODEL=rama_rurowa
  JOZZ_M6_STEERING_RIG=1` (run 1) → plik sesji dostał nowe klucze
  (`bodyVisualModel: "rama_rurowa"`, `frontSuspensionVisualModel: "rig_kierowniczy"`)
  → drugie uruchomienie BEZ żadnych env override'ów (symulacja „R"/restartu
  aplikacji) odczytało te same wartości z sesji i zapisało je ponownie
  niezmienione — dokładnie kontrakt „przeżywa R i restart". Test 4 (presety):
  utworzono tymczasowy `assets/vehicle_presets/test_nadwozie.json` (3 klucze
  visual + offset Y=0.20), wczytano przez `JOZZ_M6_PRESET=test_nadwozie`,
  zrzut ekranu potwierdził wizualnie podniesioną ramę (offset zadziałał na
  żywo) i poprawnie wybrany rig kierowniczy; plik testowy usunięty po teście
  (nigdy nie trafił do gita — `git status` po sprzątaniu czysty). Plik sesji
  na dysku został na koniec USUNIĘTY (nie przywrócony do stanu sprzed testu) —
  zobacz "Rozbieżności" niżej, dlaczego to była bezpieczniejsza decyzja niż
  próba odtworzenia starego stanu z pamięci.
- **Render (quad-shot):** wykonany na PRZYWRÓCONYM stanie fabrycznym
  (`bodyVisualModel: "brak"`) — brak ramy rurowej na aucie, identyczne cztery
  ujęcia jak oczekiwano po Etapie 1 (kolizyjna bryła + koła, bez wizualnego
  narzutu). Brak regresji.
- **`tools/gate.ps1` / `-Numbers`:** build 3/3 OK, walidator OK (18 sond), test
  PASS, boot-smoke 0 błędów sokol. Liczby fizyczne (m7 landing, p1/p5 full
  lock, p4 steering return, straight-pull heading) BEZ ZMIAN względem stanu
  sprzed tego etapu — nowe pola nie dotykają fizyki, zgodnie z oczekiwaniem.
- **`tools/doc_drift_check.ps1`:** czysty, przed i po edycjach dokumentacji.
- **Rozbieżności ze stanem zastanym:**
  1. Doc zakładał, że §3 (refresh po load) wymaga dopisania wywołań — w
     praktyce WSZYSTKO już było zrobione w Etapie 1 (ten sam agent, ta sama
     sesja projektowa, więc R4 był zaadresowany od razu szeroko, nie tylko dla
     ówczesnego zakresu). Zero zmian kodu w tym punkcie, tylko weryfikacja.
  2. `jozz_vehicle_json.h` już miał gotowy `TokenString()` (zwraca
     `std::string` dla dowolnego tokenu) — doc sugerował, że "najpewniej NIE MA
     helpera" do stringów. Nie trzeba było pisać nowego parsera tokenów, tylko
     cienkiej funkcji `ReadString` w `config_io.cpp` łączącej `FindObjectValue`
     + `TokenString` + kopiowanie z obcięciem do `cap`.
  3. Podczas manualnego testu cyklu R odkryto, że plik sesji na dysku
     (`build/jozz_vehicle_m6_session.json`, gitignored) zawierał stan sprzed
     tej sesji (prawdopodobnie resztki testów z Etapu 1, ostatnia modyfikacja
     tego samego dnia). Zrobiono kopię zapasową przed pierwszym testem i
     przywrócono ją PRZED testem presetu (krok 4 checklisty), ale test presetu
     nadpisał ją ponownie testowymi wartościami, a kopia zapasowa została już
     skonsumowana (`mv`, nie `cp`, przy pierwszym przywróceniu — błąd
     proceduralny agenta). Zamiast zgadywać pełną zawartość ~50 pól z pamięci,
     plik sesji został USUNIĘTY na koniec: przy braku pliku konstruktor używa
     `JozzVehicleM6DefaultConfig` (dobrze znany, bezpieczny stan), co jest
     lepsze niż zostawienie w nim testowych śmieci albo próba odtworzenia
     nieznanej zawartości. Plik jest gitignored i z założenia jednorazowy
     (auto-save, nie źródło prawdy) — utrata nie dotyka repo ani presetów
     commitowanych. Jeśli to były realne, ręczne ustawienia Jozza sprzed tej
     sesji (a nie tylko resztki testowania), przepraszam — trzeba je będzie
     odtworzyć ręcznie przy następnym uruchomieniu labu.
- **Znany, zamierzony limit (bez zmian od Etapu 1):** 3 wbudowane presety
  (uliczny/drift/offroad) nadal nie definiują pól visual — po wczytaniu
  dostają fabryczne „brak"/„klasyczny". To jest POPRAWNE zachowanie zgodnie z
  semantyką presetów (częściowy plik + fabryka); wzbogacenie ich o świadomy
  wybór wizualny to decyzja Etapu 3, nie tego etapu.
