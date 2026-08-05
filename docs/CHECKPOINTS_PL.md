# Checkpoints / handoff — Jozz Vehicle

## 2026-08-05 — Preflight ręcznej walidacji softness

- CTest uruchamia realny `box3d_unit` zamiast kończyć pustym sukcesem.
- README i audit wskazują aktywny `WHEEL-SOFT-03R`, nie zamknięty kalibrator 03.
- M6 lab ma runtime-only lokalny Hertz `b3Wheel` (0 = dokładny baseline; nie zapisuje się w presetach).
- Kontrakt 03R wymaga flat-road parity, mesh/time convergence i kalibracji impulsu.

## 2026-08-05 · WHEEL-SOFT-03 Q2 — immutable kalibrator opublikowany (RECONSTRUCTED)
- CO: wykonano i opublikowano czterowariantowy run Q2 dla skal Hertz 1.00/0.75/0.50/0.25; decyzja append-only ma status `INCONCLUSIVE`.
- CZEMU: sam hook solvera nie wystarczał; potrzebny był trwały dowód mechanizmu oraz jawne rozdzielenie kalibracji compliance od komfortu drogowego.
- EFEKT: kompresja rośnie monotonicznie 0.155→2.488 mm przy 2 punktach, 100% persistence, zero luk i zero topology drift; evidence: `tools/research/evidence/WHEEL-SOFT-03/20260805T100714313031Z-bf1264ed-b6de28ae/PUBLISH_MANIFEST.json`.
- DALEJ: osobny `WHEEL-SOFT-03R` ze statycznym bump mesh; Q3 pozostaje zablokowane.

Krótki dziennik najnowszego stanu. Wpis ma maksymalnie: CO / CZEMU / EFEKT / DALEJ.
Starsze wpisy: `archive/ledgers/CHECKPOINTS_2026-07_PL.md`.

## 2026-08-05 · WHEEL-SOFT-03A-2 — headless kalibrator Q2 (RECONSTRUCTED)
- CO: dodano niezależny target `jv_wheel_soft_q2`, adapter Research OS, presety `windows-research`/`linux-research` oraz kontrakt `metrics.json` + `trace.csv`.
- CZEMU: sam hook solvera nie dostarczał odtwarzalnego pomiaru kompresji, impulsu i trwałości manifoldu.
- EFEKT: baseline i trzy skale Hertz dają historycznie zgodne metryki; 2 punkty, zero luk/driftu i 100% persistence. Rig kalibruje mechanizm podatności, ale nie dowodzi poprawy komfortu jazdy.
- DALEJ: wykonać immutable run czterech wariantów, zapieczętować wynik i podjąć jawną decyzję przed awansem do bodźca drogowego.

## 2026-08-05 · WHEEL-SOFT-03A-1 — lokalna normalna softness koła (RECONSTRUCTED)
- CO: dodano wheel-local `contactHertz`/`contactDampingRatio`, zapis shape/recording oraz wspólny selektor używany przez convex i mesh prepare paths.
- CZEMU: globalne `contactHertz` zmieniało wszystkie kontakty świata i nie pozwalało na czyste A/B opony.
- EFEKT: `0/0` zwraca dokładnie precomputed world softness; niższy lokalny Hertz zwiększa kompresję na hull i mesh bez zmiany dwóch support points ani feature IDs; non-wheel override jest ignorowany.
- DALEJ: osobny headless Q2 runner i `metrics.json`; ten checkpoint nie wybiera jeszcze wartości softness ani nie zmienia pojazdu.

## 2026-08-05 · JV Research OS — wykonywalne eksperymenty
- CO: dodano `jv_lab.py`, kontrakty `WHEEL-SOFT-03`/`VEHICLE-FLEET-STRESS-04`, immutable runy, resume po tokenie proposal, wymagane artefakty i jawne decyzje parent-run.
- CZEMU: bramki jakości i evidence nie prowadziły aktywnego A/B ani nie wymuszały jednej zmiennej i awansu rigów.
- EFEKT: 10 regresji runnera; brak `metrics.json` jest FAIL, zmiana proposal blokuje resume, a Q3/Q4 wymagają zapieczętowanego i zatwierdzonego wcześniejszego rigu.
- DALEJ: `WHEEL-SOFT-03A` — lokalny hook softness + Q2 metrics; spec pozostaje `blocked` do czasu zero-delta parity.

## 2026-08-04 · WHEEL-HULL-02B — finite hully i narożniki
- CO: face clipping do polygonu, konserwatywny face-prism fast path, feature walk oraz numeryczne osie edge/vertex-cone ze stabilnymi IDs.
- CZEMU: nieskończona płaszczyzna dawała phantom corners, a search ograniczony do najlepszej ściany gubił odległy support vertex i wnętrze stożka wierzchołka.
- EFEKT: 32 testy koła; 2000 boxów + 60 nieortogonalnych hullów w progu 3 mm; obciążone face→edge→vertex w 2 kierunkach × 2 fazy; Debug, ASan, scalar UBSan i validator `19 + 2` zielone.
- DALEJ: `WHEEL-SOFT-03` — lokalna normalna softness A/B przy identycznej geometrii i manifoldzie; bez dalszego strojenia crown.

## 2026-08-04 · WHEEL-SEAM-02A — szwy trójkątów i mesha
- CO: dodano jednostronny wheel–triangle, finite edge/vertex fallback, stabilne feature IDs oraz wheel-only wybór normalnej najgłębszego manifoldu w klastrze mesha.
- CZEMU: samo odrzucenie barycentryczne gubiło kontakt na granicy, a normalna pierwszego trójkąta dawała zależny od fazy skok impulsu `+36,4%` na łagodnym załamaniu.
- EFEKT: 16 testów koła; płaski obciążony szew ma dokładnie 1 constraint w obu kierunkach i 3 fazach, załamanie `~1,15°` przechodzi w obu kierunkach i 2 fazach; pełne unity i ASan są zielone, wheel UBSan jest zielony, a 2 pełne walidacje produktu są bajtowo identyczne.
- DALEJ: `WHEEL-HULL-02B` — clipping do face polygon oraz komplet osi edge/axis; podatność nadal czeka.

## 2026-08-04 · WHEEL-RIGID-01 — strict support manifold
- CO: plane/triangle baseline raportuje 1 support vertex albo 2 końce rzeczywistego support segment; speculative distance tylko bramkuje istnienie kontaktu.
- CZEMU: poprzedni manifold mieszał geometrię crown z liczbą próbek wpadających w speculative band.
- EFEKT: 7 testów koła, ASan/UBSan i 2 bajtowo identyczne pełne walidacje są zielone; crown sweep ma stale 1,00 wszystkich i 1,00 obciążonych punktów/kolo.
- DALEJ: `WHEEL-SEAM-02` — ciągłość face/edge/vertex na szwach mesha, bez strojenia komfortu.

## 2026-08-04 · Domknięcie wznawialnej bramki
- CO: dodano fingerprint `HEAD:index-tree`, obowiązkowy token wznowienia i ograniczenie `--stop-after`.
- CZEMU: samo `--start-at` nie dowodziło niezmienności propozycji i mogło ponownie przekroczyć limit pojedynczego uruchomienia.
- EFEKT: wznowienie nie omija starych gate’ów po zmianie staged tree; częściowy przebieg jawnie kończy się `PARTIAL OK`.
- DALEJ: zamknąć eksport tej rekurencji, potem wrócić do `WHEEL-RIGID-01`.

## 2026-08-04 · Rekurencja konkursowa dokumentacji
- CO: scalono kontrakty assetów, osie rigu z dokumentem rigu, hotkeys z UI oraz zastąpiono przed-`b3Wheel` architekturę i protokół v2.1 bieżącymi kontraktami.
- CZEMU: sześć aktywnych plików miało nakładających się właścicieli albo historyczny status wyglądający jak instrukcja.
- EFEKT: aktywne dokumenty projektu spadły z 18 do 15 i z 3627 do 2673 linii; `quick`, 11/11 testów higieny i 43/43 testy evidence przechodzą w 11 wznawialnych shardach.
- DALEJ: domknąć bezpieczne wznowienia bramki; późniejszy audyt `KOLA_01` pozostaje osobną paczką.

## 2026-08-04 · Fundament dokumentacji i archiwum
- CO: jedna hierarchia źródeł prawdy, archiwum, bridge JV→JES, aktualne kontrakty subsystemów oraz cztery lokalne bramki higieny.
- CZEMU: front door zatrzymał się na M8, stare plany udawały aktywne, rejestr miał osierocone findings, a eksport katalogu niósł setki MiB artefaktów.
- EFEKT: `docs_audit`, `repo_hygiene` i evidence są zielone bez ostrzeżeń, 7 testów higieny przechodzi, core ma jawny manifest, a deterministyczny eksport czyta wyłącznie commitowane drzewo `HEAD`.
- DALEJ: rygorystyczny baseline manifoldu koła, potem seam probe i A/B podatności.

## 2026-08-03 · Profil bieżni w `b3Wheel` · 5b92e9c
- CO: koło dostało normalizowany profil poprzeczny, wysklepienie, stabilne IDs i debug draw wynikający z tej samej geometrii.
- CZEMU: walec nie reprezentował opony przechodzącej płynnie po szerokości bieżni.
- EFEKT: zapisane sweepy prosta/zakręt oraz komplet 19 sond; wynik crown pozostaje do rozdzielenia od topologii manifoldu.
- DALEJ: nie stroić dalej crown przed testem strict-manifold.

## 2026-08-03 · Domknięcie integracji nowego typu koła · 130b24c…c6f44ef
- CO: kontakty z kapsułą/sferą/hullem, mesh/heightfield, raycast oraz debug render.
- CZEMU: typ działający tylko na płaszczyźnie nie był produktem ani użytecznym instrumentem.
- EFEKT: nowe koło jest widoczne, wybieralne i działa na mapie oraz bumperach.
- DALEJ: testy szwów, krawędzi i dokładności query API.

## 2026-08-03 · Pierwszy natywny kolider koła w Box3D · 9800af9
- CO: dodano `b3Wheel` jako obrotowo symetryczny typ bryły z dedykowanym manifoldem.
- CZEMU: sfera była gładka, ale bez szerokości; walec i pierścienie wielokształtne młotkowały pojazd.
- EFEKT: recorded ride probe pokazuje spokojniejsze toczenie niż stockowe alternatywy.
- DALEJ: udowodnić, która część poprawy pochodzi z powierzchni, a która z manifoldu.

## 2026-07-30 · Wheel Scope · jozz-scan-terrain-f0
- CO: wspólny rig dla stendu i okna, zapisywalna konstrukcja, visual equivalence i behaviour lock.
- CZEMU: ręczna obserwacja musi używać dokładnie tego samego eksperymentu co headless.
- EFEKT: Q2A/telemetria były bajtowo zgodne; negatywny wynik prismu ujawnił brak wcześniejszej miary pracy na metr.
- DALEJ: zachować instrument jako dziedzictwo, ale nie traktować jego rankingu jako wyniku pojazdu.

## 2026-07-24 · Skan terenu i spawny per fragment · jozz-scan-terrain-f0
- CO: teksturowany mesh skanu, cache BVH, ładowanie paczki, spawny session/persistent i fundament wielu skanów.
- CZEMU: mapa miała przyjmować realne dane terenu bez utraty checkpointu po restarcie.
- EFEKT: ładowanie cache spadło z raportowanych ~14 s do ~0,6 s; stan przeżywa `R`.
- DALEJ: manualny odbiór mapy pozostaje otwarty, ale nie blokuje programu koła.
