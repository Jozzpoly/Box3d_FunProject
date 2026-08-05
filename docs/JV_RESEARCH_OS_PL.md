# JV Research OS — wykonywalny system badań projektu

Status: aktywny kontrakt infrastruktury badawczej.
Wejście CLI: `python tools/jv_lab.py`.
Specyfikacje: `tools/research/experiments/*.json`.
Lokalne runy: `build/research_runs/` — ignorowane przez Git do czasu świadomej
promocji surowego dowodu.

## 1. Po co istnieje

JV ma już bramki jakości (`jv_gate.py`) oraz łańcuch zaakceptowanego evidence
(`tools/evidence/evidence.py`). Brakowało warstwy pomiędzy nimi: systemu, który
prowadzi aktywny eksperyment od pytania do decyzji i nie pozwala po drodze
zmienić kodu, macierzy wariantów ani poziomu rigu bez pozostawienia śladu.

Research OS nie jest kolejnym benchmarkiem koła. Jest wspólnym protokołem dla:

- `WHEEL-SOFT-03`;
- wielopojazdowego `VEHICLE-FLEET-STRESS-04`;
- późniejszych badań zawieszenia, kierownicy, mapy i dziedzictwa JES.

## 2. Rozdział odpowiedzialności

```text
jv_lab.py       planuje i wykonuje aktywny eksperyment
jv_gate.py      sprawdza jakość dokładnej staged proposal
evidence.py     chroni surowe, świadomie opublikowane dowody
KOLA_FINDINGS   przechowuje zaakceptowany status wiedzy
Jozz            odbiera feeling, obraz i decyzje produktowe
```

Runner nie nadaje statusu `SUPPORTED`. Zielony proces oznacza tylko, że case
wykonał się zgodnie z kontraktem i dostarczył wymagane artefakty.

## 3. Wykonywalny kontrakt eksperymentu

Każdy plik `jv-experiment/v1` deklaruje:

- jedno pytanie i jedną hipotezę;
- dokładnie jedną zmienną główną;
- baseline i jawne warianty;
- zamrożone czynniki oraz confoundy;
- drabinę rigów i zależności między poziomami;
- metryki, reguły awansu i manualne bramki;
- blokady implementacyjne;
- dopiero w stanie `ready`: command jako tablicę argumentów, timeout i wymagane
  artefakty.

Walidator odrzuca wariant, który próbuje zmienić drugi parametr razem ze zmienną
główną. Roadmapa ma jawne pole `order`; `next` nie wynika z alfabetu nazw plików.

## 4. Immutable run

`start` wymaga, aby worktree odpowiadał staged proposal. Run zapisuje:

```text
HEAD:index-tree
hash specyfikacji
branch i poziom rigu
baseline oraz warianty
pełne command arrays i środowisko
```

Każdy case ma osobny katalog, `case.json`, `stdout.log`, `stderr.log` i
`result.json`. Kod wyjścia `0` nie wystarcza: brak deklarowanego `metrics.json`
lub innego artefaktu oznacza `FAILED`.

`resume` działa tylko przy identycznym tokenie proposal. Zmieniony kod lub index
nie może kontynuować starego runu pod tą samą nazwą.

## 5. Awans między rigami

Poziom zależny, np. Q3, wymaga parent runu z wcześniejszego poziomu. Parent musi:

1. należeć do tego samego eksperymentu i tej samej wersji specyfikacji;
2. mieć kompletny `decision_packet.json`;
3. mieć jawną `human_decision.json`;
4. mieć status `SUPPORTED` albo `STRONGLY_SUPPORTED`.

`PROVISIONAL`, `REFUTED` i `INCONCLUSIVE` nie otwierają następnego rigu. Dzięki
temu „stend wyszedł zielono” nie staje się automatycznie wynikiem pojazdu.

## 6. Decision packet i decyzja człowieka

`seal` zamyka wykonanie w `decision_packet.json/.md`. Packet zawiera pełną
macierz, wyniki procesów, artefakty, confoundy i manualne bramki, ale pole
`automatic_verdict` zawsze pozostaje `null`.

Dopiero `decide` zapisuje osobny, jawny status wraz z autorem i uzasadnieniem.
Decyzja jest związana hashem z konkretnym packetem i proposal tokenem.

## 7. Podstawowe komendy

```text
python tools/jv_lab.py list
python tools/jv_lab.py next
python tools/jv_lab.py validate tools/research/experiments/WHEEL-SOFT-03.json
python tools/jv_lab.py plan tools/research/experiments/WHEEL-SOFT-03.json

# Headless Q2 build bez Samples/GUI:
cmake --preset linux-research
cmake --build --preset linux-research
# Windows: odpowiednio `windows-research`.

python tools/jv_lab.py start <spec> --level Q2
python tools/jv_lab.py status <run-dir>
python tools/jv_lab.py resume <run-dir> [--retry-failed]
python tools/jv_lab.py seal <run-dir>
python tools/jv_lab.py decide <run-dir> --status SUPPORTED --decided-by Jozz --note "..."
python tools/jv_lab.py publish <run-dir>
```

Poziom zależny otrzymuje wcześniejsze runy przez powtarzalne `--parent-run`.

## 8. Relacja do repo i evidence

`build/research_runs/` jest warsztatem, nie automatycznie publikowanym dowodem.
Po decyzji wartościowy run przechodzi świadomą promocję poleceniem `publish`.
Publikacja jest kuratorowana: zachowuje metryki, trace, case/result, packet, token
i append-only historię decyzji; pomija puste logi, osadza snapshot specyfikacji
w manifeście i odmawia nadpisania istniejącego evidence. Nie kopiujemy buildów
ani cache.

Niezmienność aktywnego runu chroni `jv_lab`; integralność opublikowanego wyniku
chroni `evidence.py`; kompletność propozycji przed checkpointem chroni
`jv_gate.py`.

## 9. Aktualna kolejka

1. `WHEEL-SOFT-03` — aktywny eksperyment `ready`; lokalny hook i headless kalibrator Q2 istnieją, a następny krok to immutable sweep oraz jawna decyzja;
2. `VEHICLE-FLEET-STRESS-04` — zaplanowany system skali i zabawy, zależny od
   jawnego modelu kontaktu po 03A oraz headless CMake bez GUI.

## 10. Plan rozwoju systemu

Kolejne rozszerzenia mają być adapterami, nie osobnymi runnerami:

- parsery metryk i porównania parowane, nadal bez automatycznego werdyktu;
- replay/input trace jako deterministyczny bodziec wielu pojazdów;
- adapter Windows/Linux do tej samej specyfikacji;
- dashboard czytający runy bez modyfikowania ich zawartości;
- kolejka workerów dla długich sweepów z jednym wspólnym proposal tokenem;
- eksport minimalnego decision packetu do clean-room dziedzictwa JES.

Nowa funkcja trafia do Research OS tylko wtedy, gdy obsługuje więcej niż jeden
konkretny eksperyment albo zamyka powtarzalną klasę błędów procesu.
