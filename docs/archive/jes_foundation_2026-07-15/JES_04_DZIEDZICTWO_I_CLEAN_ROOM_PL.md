> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# JES_04 — Dziedzictwo i protokół clean-room

Warstwa: **DZIEDZICTWO**. Wersja: 1.0-kandydat (2026-07-15; destylat
protokołu Sol `02_HERITAGE_EXTRACTION...` + `03_VAW_JV_CAPABILITY_LEDGER`
scalony z prawami inżynierskimi pakietu Claude). „Clean-room" = dyscyplina
inżynieryjna, nie opinia prawna.

Cel: **odziedziczyć dowody, zachowania, scenariusze i porażki — a od nowa
zaprojektować dane, kontrakty, moduły i implementację.** Zwykły rewrite
nie wystarczy: można skopiować stare błędy bez jednej linii kodu (te same
złe granice modułów, magiczne liczby, testy utrwalające przypadkowe
zachowanie, workflow wynikający z ograniczeń starego UI).

---

## 1. Cztery klasy materiału dziedziczonego

Nic nie przechodzi do nowego programu bez nadania klasy:

| Klasa | Znaczenie |
|---|---|
| `ACCEPTED_LESSON` | potwierdzone zachowanie/decyzja — kandydat do clean-room próby (lekcja, NIE kod) |
| `CHARACTERIZATION` | obserwacja aktualnego zachowania; niekoniecznie pożądane |
| `FAILED_LESSON` | błąd/false-green — ma stać się testem negatywnym nowego projektu |
| `UNVERIFIED_CLAIM` | dokument/test twierdzący więcej, niż dowodzi |

Do zamknięcia P-1 wszystko dodatkowo nosi status `PROVISIONAL`.

## 2. Rdzeń metody: zachowanie ≠ mechanizm ≠ ograniczenie hosta

Dla każdej badanej zdolności trzy listy — dalej przechodzi domyślnie
tylko pierwsza:

1. **zachowanie wartościowe** (co system robi dobrze i czemu to cenne);
2. **mechanizm starej implementacji** (jak to osiągnięto — do oceny);
3. **przypadkowe ograniczenie hosta** (co wynikało tylko ze starego
   środowiska — do odrzucenia).

Pipeline per zdolność (uruchamiany TUŻ PRZED jej labem, nie hurtowo):
`SELECT → OBSERVE → SEPARATE → SPECIFY (kontrakt zachowania z falsifierem)
→ CHALLENGE (granice/workload/dusza/dowód) → REIMPLEMENT (od zera)
→ COMPARE (MATCH / IMPROVE / świadome BREAK) → PROMOTE (JES_03 §6)`.

Najczystszy wariant: implementer nie czyta starego kodu po przyjęciu
specyfikacji; pytania wracają przez autora speca; podobieństwo nazw
i struktury wymaga uzasadnienia domenowego (ryzyko R8 — mechaniczna
translacja).

## 3. Co wolno dziedziczyć

**Bezpośrednio jako wiedzę:** scenariusze użytkownika; przypadki awarii
i ich reprodukcje; ręczne uwagi o feelu; fixture'y mechaniczne; oczekiwane
łańcuchy przyczynowe; wymagania asset-workflow; konwencje jednostek/osi PO
ponownej walidacji; potrzeby diagnostyczne; politykę ochrony danych.

**Po osobnej decyzji (ADR/PDR):** algorytm; zewnętrzna biblioteka; format
sidecar assetu; model jointa; sposób replay; framework testowy.

**Materiały właściciela, po jawnym wpisie provenance (D14):** grafiki,
modele i tekstury Jozza; screenshoty porównawcze; nagrania telemetrii;
presety opisane jako INTENCJE (nie defaulty).

**Domyślnie NIE:** kod; foldery i nazwy klas; schematy JSON; magiczne
liczby tuningu (klasa `REFERENCE_ONLY` — historyczny punkt odniesienia,
nigdy nowy default); build system; ukryte fallbacki; uchwyty runtime;
monolityczne sample; testy sprawdzające tylko „finite"; obejścia starego
hosta; zależność build/runtime od starych repo; benchmark przeciw
niezamrożonemu dirty stanowi.

## 4. Klasyfikacja dziedziczonych testów (przyjęta z Sol — obowiązująca)

| Typ | Znaczenie | Co robimy |
|---|---|---|
| Invariant | chroni prawdziwe prawo domeny | reimplementacja z nową specyfikacją |
| Characterization | opisuje stare zachowanie | punkt porównania, nie wymaganie |
| Regression | chroni konkretną naprawę | sprawdzić, czy problem wciąż istnieje |
| Feel scenario | scenariusz ręcznego odbioru | odtworzyć z RUN-ID i pytaniami |
| Integration smoke | dowodzi uruchamialności | nie rozszerzać znaczenia |
| Loose guard | „finite"/szeroki próg | wzmocnić albo odrzucić |
| Obsolete | chroni odrzucony model | archiwum z powodem |
| **False green** | nie mierzy deklarowanej ścieżki | **zamienić w Failure Card + test negatywny** |

Stary test nie staje się nowym wymaganiem tylko dlatego, że istnieje.

## 5. Prawa inżynierskie L1–L12

Skodyfikowane lekcje kodu obu dem — pełna treść: **JES_02 §8** (L1
rusztowanie-nie-nośne; L2 gałąź lustrzana prętów; L3 ciała strukturalne
bez shape'ów + jawna masa; L4 categoryBits obu stron; L5 masa efektywna
na ramieniu; L6 walidacja buduje realny świat + testy negatywne +
timestampy; L7 render is the gate; L8 persystencja od razu; L9 ścieżki
niezależne od CWD; L10 zakaz cichych fallbacków i luzowania progów;
L11 pipeline wizualny dowodzony in-game; L12 model zastępczy z jednym
źródłem geometrii). Status: `ACCEPTED_LESSON` — wchodzą do nowego projektu
jako prawa, nie hipotezy; każde ma za sobą realny, udokumentowany incydent.

## 6. Rejestr zdolności (Capability Ledger)

Źródłem szczegółowym jest ledger Sol-a:
`<repo VAW>/docs/jes_pre_foundation_2026_07_15/03_VAW_JV_CAPABILITY_LEDGER.md`
— 29 kart `PROVISIONAL`: **CAP-VAW-01…17** (m.in. czysty wersjonowany
dokument konstrukcji; atomowe transakcje edycji; kompilator jako granica
authoring→runtime; stabilna tożsamość mechaniczna; wymienny assembly
builder; renderer-only asset pack; visual truth; oddzielne grafy znaczeń;
local assembly spaces; versioned persistence z recovery) oraz
**CAP-JV-01…12** (feel przed kompletnością; zachowanie z konstrukcji
i sił; semantyczne przestrzenie współrzędnych i design pose; one/two-corner
rig jako instrument; post-abuse function; factory+overrides; semantic ID
zamiast index-hintów; rig truth; falsyfikacja mechanizmu; monolit M6 jako
FAILED_LESSON; box3d = punkt startowy nie rozwiązanie marzenia; stare
liczby = REFERENCE_ONLY).

Najkrótsza synteza wspólnego rdzenia wiedzy (za Sol, potwierdzona):

```text
intencja autorska → wersjonowany dokument → walidacja + czysta kompilacja
→ neutralny plan runtime → wymienne adaptery fizyki/renderu/audio
→ oprzyrządowany run + dowód wizualny/ręczny → save/replay/diagnoza → powrót
```

VAW wnosi granice danych, transakcje, kompilację, art pipeline i dyscyplinę
dowodów; JV wnosi mechaniczny feel, izolowane rigi, testy-interwencje,
przypadki abuse i bezlitosną prawdę obrazu.

## 7. Karty (szablony skrócone)

**Capability Card:** `heritage_id / źródło+baseline / obserwowane
zachowanie / dowód+limit dowodu / co udowodniło, a czego NIE / wiedza do
przeniesienia / zakazane dziedziczenie / pytanie clean-room / falsifier /
lab docelowy / status`.

**Failure Card:** `failure_id / źródło / objaw / deklarowany status
w chwili incydentu / rzeczywista przyczyna / czemu bramki nie wykryły /
reprodukcja / zakazany wzorzec przyszły / wymagana nowa bramka`.

Failure Cards są równie ważne jak Capability Cards: stare projekty mają
dostarczyć przede wszystkim negatywną wiedzę, której nie trzeba kupować
drugi raz.

## 8. Warunki STOP ekstrakcji

Przerwij, gdy: nie widać konkretnej wartości dla nowego projektu; źródła
sprzeczne bez runtime evidence; fixture koduje tylko starą implementację;
spec wymaga skopiowania schematu; abstrakcja ma jednego hipotetycznego
konsumenta; performance claim bez workloadu; implementer musi czytać stare
prywatne szczegóły; nowa implementacja pełni kilka ról naraz; brak
jasnego ADOPT/REJECT/DEFER.

> Nie pytamy: „jak przepisać VAW albo JV?". Pytamy: „jakie prawa,
> zachowania, błędy i scenariusze te projekty odkryły — i jak najprościej
> udowodnić je ponownie w systemie, którego granice należą już do nowego
> projektu?"
