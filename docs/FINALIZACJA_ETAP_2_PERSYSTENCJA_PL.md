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

- Commit: …
- Checklisty §5/§6: …
- Rozbieżności ze stanem zastanym: …
