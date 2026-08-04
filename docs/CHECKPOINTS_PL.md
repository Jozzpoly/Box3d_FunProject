# Checkpoints / handoff — Jozz Vehicle

Krótki dziennik najnowszego stanu. Wpis ma maksymalnie: CO / CZEMU / EFEKT / DALEJ.
Starsze wpisy: `archive/ledgers/CHECKPOINTS_2026-07_PL.md`.

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
