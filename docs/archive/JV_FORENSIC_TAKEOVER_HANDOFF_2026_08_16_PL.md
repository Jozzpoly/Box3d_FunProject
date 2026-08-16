# JV native / Box3D — forensic takeover handoff

**Data:** 2026-08-16  
**Cel:** bezpieczne przekazanie do nowej rozmowy przed utratą kontekstu  
**Projekt:** natywny Jozz Vehicle / Box3D  
**Repo:** `Jozzpoly/Box3d_FunProject`  
**Charakter dokumentu:** jednorazowy forensic checkpoint w `archive/`; **NIE jest nowym źródłem prawdy o produkcie i NIE zastępuje audytu live**.

---

## 0. Najważniejsza instrukcja dla nowego orchestratora

Nie kontynuuj automatycznie starego planu badawczego, dokumentacyjnego ani `B3WHEEL-STEER-01C` tylko dlatego, że aktualne docs tak routują.

Najpierw wykonaj **cold takeover / forensic truth audit** całego obszaru, który ma być później portowany do JV-Web:

1. rzeczywista topologia i semantyka front-corner riga;
2. authored asset i jego markery/osi;
3. realny runtime native JV;
4. pochodzenie ostatniego owner-validated driving feel;
5. wszystkie warstwy stanu wpływające na ten feel;
6. rozbieżności `docs / sidecar / config / validator / executable dataflow`.

**Nie implementuj ani nie sprzątaj niczego, dopóki ten model prawdy nie zostanie odtworzony.**

Główny powód powrotu do JV-Core: w JV-Web przez wiele tygodni nie udało się ani razu wiarygodnie przenieść aktualnego riga i feelu. Agenci regularnie wracali do starszych, zepsutych wersji mimo zielonych testów i deklaracji zgodności. W sierpniu 2026 znaleziono konkretne mechanizmy, które mogły to powodować.

---

## 1. Live identity lock zweryfikowany 2026-08-16

### `recovery/jv-reconstruction`

```text
HEAD:
edbc9d73ae612e51184b4a438775418d4d1b7e3c

tree:
954c3c65597da516065c2005bf65f8925cbe0635
```

Commit `edbc9d73...` to `revert: remove accidental noop file`.

**Ważne:** tree `954c3c65...` jest identyczny z tree starego checkpointu/handoffu `b756f091...` z 2026-08-06. Późniejsze dwa commity zmieniły historię HEAD, ale nie zawartość drzewa projektu.

### `main`

```text
959aefb78587ce60cf2b8eb03ff82797a4165142
```

To stan mapy/skanu z 2026-07-24, nie pełny recovery head.

### `jozz-scan-terrain-f0`

```text
241fe10a9056836332c21d9614471d32d749ce3d
```

Remote ref nadal odpowiada staremu chronionemu worktree użytkownika z handoffu 2026-08-06.

### Chronione lokalne środowisko użytkownika

Historyczny handoff podaje:

```text
C:\Pliki_Joza\Gamo_devovo\Box3d_FunProject\box3d
branch: jozz-scan-terrain-f0
HEAD: 241fe10a9056836332c21d9614471d32d749ce3d
```

Connector nie widzi aktualnego lokalnego `git status`, więc **nie zakładaj, że lokalny worktree jest dziś clean tylko dlatego, że był clean w handoffie**. Nie przełączaj go, nie resetuj i nie używaj jako disposable workspace bez jawnego potwierdzenia.

### `AI_PROJECT_MEMORY.md`

Na `recovery/jv-reconstruction` nie istnieje (sprawdzone 2026-08-16). Nie czekaj na ten plik.

---

## 2. Status pracy tej rozmowy

W tej fazie nie wykonano product implementation, cleanupu ani zmian fizyki.

Do momentu tego handoffu praca była **read-only forensic audit**. Jedyny write wykonany przy przekazaniu to dodanie tego archiwalnego dokumentu.

Nie uznawaj wcześniejszych wniosków agenta za zamknięte tylko dlatego, że są tu zapisane. Każdy ważny punkt poniżej jest opisany jako:

- `VERIFIED` — potwierdzony live kodem/refem/danymi;
- `OWNER EVIDENCE` — bezpośredni feedback Jozza;
- `STRONG LEAD` — mocny trop wymagający domknięcia;
- `UNKNOWN` — nie wolno przedstawiać jako faktu.

---

## 3. VERIFIED — dokumentacja i executable runtime nie opisują tej samej prawdy

### 3.1 Steering sidecar zawiera błędną semantykę `ChassisMount_b`

Aktualny steering sidecar deklaruje w swojej warstwie opisowej:

```text
Socket_ChassisMount_b -> ridesBody: knuckle
Socket_WheelCenter    -> ridesBody: knuckle
```

Natomiast działający native M6 visual rig rozdziela te role:

```text
Socket_WheelCenter    -> knuckleWorld      (travel + steering)
Socket_ChassisMount_b -> armWorld/lowerArm (travel, NO steering)
```

To jest centralny smoking gun dla wcześniejszych nieudanych portów: agent ufający sidecarowi może wiernie zaimplementować **złą** wersję mimo poprawnego zachowania istniejącego w native code.

### 3.2 Validator nie waliduje tej semantyki

`JozzVehicleContractBinding` i parser nie przenoszą `ridesBody` do modelu używanego przez validator.

Validator sprawdza m.in. role, kategorie, node hints, glTF resolution, `physicsAuthority` i wybrane odległości, ale nie może sprawdzić semantyki body-parenting zapisanej wyłącznie w opisowym `ridesBody`.

Wniosek:

```text
CONTRACT VALIDATION PASS
!=
body-role / parenting semantic correctness proven
```

To jest przykład **validator scope overclaim**: test może być poprawny w swoim zakresie, a człowiek/agent może nadinterpretować jego zielony wynik.

### 3.3 Skażenie istnieje także w comments/docs

Nie tylko sidecar może być błędny. W części helper comments/headerów występuje stara narracja `ChassisMount_b + WheelCenter -> knuckle`, podczas gdy executed draw path rozdziela te role.

Dlatego prowizoryczna hierarchia:

```text
code > docs
```

jest za słaba.

Dla krytycznych twierdzeń trzeba zejść do:

```text
executed dataflow / actual body-joint graph / authored geometry / pomiar
>
komentarz
```

---

## 4. OWNER EVIDENCE — front corner nie może być traktowany jako jeden „knuckle”

Bezpośredni feedback Jozza z aktualnej kampanii JV-Web:

Front wheel-side mechanizm ma co najmniej logiczny rozdział:

```text
wahacze / suspension articulation
        ↓
[ suspension-side member ]   <- żółty w owner annotations
        ↓
relative STEERING DOF
        ↓
[ steerable member ]         <- czerwony w owner annotations
        ↓
independent WHEEL SPIN
        ↓
koło
```

Kluczowa semantyka:

- „żółty” nie jest statyczny względem świata — porusza się z zawieszeniem;
- jest **non-steering względem ruchu kierowniczego**;
- „czerwony” obraca się względem żółtego wokół osi skrętu;
- wheel spin jest osobnym DOF;
- wcześniejsze wizualizacje/porty, które sklejały wheel-side w jeden knuckle/upright frame, zostały przez ownera odrzucone.

Nie zakładaj jednak automatycznie, że repair oznacza dokładnie „dodaj dwa nowe physics bodies”. Najpierw ustal, czy poprawny split już istnieje w physics, a zepsuta jest tylko warstwa visual/binding, czy problem obejmuje również physical topology.

---

## 5. OWNER EVIDENCE + VERIFIED ASSET LEAD — authored axis ma pierwszeństwo przed wtórną narracją

W source rig istnieją jawne markery:

```text
Axis_SuspensionTravel_Top
Axis_SuspensionTravel_Bottom
Socket_WheelCenter
```

W poprzedniej analizie potwierdzono, że `Socket_WheelCenter` leży na prostej Top→Bottom i w połowie między markerami.

Owner wizualnie wskazał, że wcześniejsza niebieska oś była przesunięta w jednym rzucie i że poprawne położenie odpowiada authored axis z assetu.

Krytyczna korekta ownera:

> słuchamy się tego, co mówi asset; dokumentacja jest corrupted przez nieodpowiednią aktualizację.

Nie wolno więc automatycznie traktować historycznych `caster/KPI/kingpinOffset` wygenerowanych przez generic M6 geometry jako nadrzędnej prawdy o położeniu/ośce authored riga.

**UNKNOWN do rozstrzygnięcia:**

- czy authored Top↔Bottom definiuje wyłącznie registration/origin;
- pełną local steering axis;
- czy local axis, która następnie ma być mapowana na live physics frame;
- czy current M6 physical kingpin (`upperBall↔lowerBall`) jest świadomą adaptacją czy historyczną regresją względem assetu.

Nie rozwiązuj tego z dokumentacji. Porównaj raw authored model, actual native runtime oraz body/joint graph.

---

## 6. VERIFIED — M6 nie jest determinowany samym commitem

To jest krytyczne zarówno dla riga, jak i dla feelu.

Konstruktor M6 tworzy factory config, a następnie automatycznie nakłada:

```text
build/jozz_vehicle_m6_session.json
```

Pełny tuning ostatniej sesji jest zapisywany/odtwarzany poza Gitem.

Osobno istnieje:

```text
build/jozz_vehicle_m6_debug_session.txt
```

oraz niepersystowane globalne parametry contact solvera w labie.

Dodatkowo istnieją runtime/env hooks, które mogą zmieniać wybrane parametry eksperymentalnie.

Dlatego:

```text
M6 @ commit X
```

**NIE wystarcza do odtworzenia samochodu, którym Jozz faktycznie jeździł.**

W future truth model rozdziel co najmniej:

1. `factory specimen`;
2. `named preset specimen`;
3. `owner-session specimen`;
4. `research/headless specimen`;
5. global/sample solver state poza vehicle configiem.

---

## 7. STRONG LEAD — odzyskanie ostatniego owner feel

### 7.1 Istnieje zabezpieczony Git tag

```text
checkpoint/owner-feel-source-5b92e9c
→ 5b92e9c349ff2106d154c4b29dcc7a1428f5ae6a
```

Tag nadal istnieje na GitHubie.

### 7.2 W File Library istnieje pełna sesja

Nazwa:

```text
JV_OWNER_FEEL_SESSION_5b92e9c.json
```

To pełny `JozzVehicleM6Config` zachowany poza repo 2026-08-05.

Istotne pola obejmują m.in.:

```text
frontRigType = 1
rearRigType = 1
casterDeg = 5
kingpinInclinationDeg = 7
ackermannFraction = 0.6
wheelEnvelope.mode = 0
wheelFriction = 1.25
suspensionHertz = 6
suspensionDampingRatio = 0.7
preload F/R = 0.07 / 0.07
ARB F/R = 16000 / 10000
maxDriveTorque = 320
steeringHertz = 14
maxSteeringTorque = 700
rackFrictionBase = 40
rackFrictionLoadCoeff = 0.1
rackCenteringHertz = 0
uprightAssist = false
bodyVisualModel = rama_rurowa
frontSuspensionVisualModel = rig_kierowniczy
```

**Najważniejsza różnica:** zachowana owner-feel session ma:

```text
wheelEnvelope.mode = 0   // sphere
```

podczas gdy factory config na `5b92e9c` i obecnym recovery head nadal ustawia:

```text
JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL
```

To jest potencjalnie bardzo istotne dla feelu kontaktu/steering.

### 7.3 `uliczny.json` nie rozwiązuje problemu

Wersjonowany `assets/vehicle_presets/uliczny.json` jest bajtowo identyczny między:

```text
57ca2d2dd318b69c5784a52051fdda163f65ca56
```

(okolice manualnie zaakceptowanego Gate 2 / preset fix 2026-07-10)

a obecnym:

```text
edbc9d73ae612e51184b4a438775418d4d1b7e3c
```

To dowodzi, że ten preset nie został cicho przepisany.

Ale preset jest częściowy i nie jest pełnym owner-session snapshotem. Sam `uliczny.json` **nie może zostać nazwany latest validated feel**.

### 7.4 Poziom pewności

`checkpoint/owner-feel-source-5b92e9c + JV_OWNER_FEEL_SESSION_5b92e9c.json` to obecnie **najsilniejszy kandydat do forensic recovery owner feel**, ale **NIE został w tej rozmowie domknięty jako ostatni i finalnie zaakceptowany driving feel**.

Nowy agent ma znaleźć bezpośredni owner verdict / chronology i dopiero potem poprosić Jozza o kontrolowany A/B manual gate.

Nie wolno zrobić błędu:

```text
nazwa taga "owner-feel-source"
→ więc na pewno golden
```

To nadal hipoteza do falsyfikacji.

---

## 8. VERIFIED — starszy owner/manual acceptance istnieje, ale nie wolno się do niego cofnąć automatycznie

Historia zawiera m.in.:

### M5.2 — 2026-07-05

Commit:

```text
f84f8c32c7d5de17c3cf88f63029b242b115c9de
```

Zapisany owner re-test:

- A/D direction correct;
- at-speed wheel hopping fixed przez sphere wheels;
- tie-rod steering coupling `feels good`;
- fundamenty kierunku/koła/steering/contact/telemetry zaakceptowane jako „good enough for now”.

To jest twardy historyczny acceptance checkpoint, ale nie najnowszy automatycznie.

### M6 / Gate 2 — 2026-07-10

Historia zapisuje późniejszą manualną akceptację Gate 2 + deterministic preset fix.

To także nie wystarcza jako finalny golden specimen, ponieważ później istniały owner sessions i program koła.

### Reguła

Chronologia manualnych verdictów ma zostać odtworzona jako ledger:

```text
owner verdict
→ exact source ref
→ exact session/preset
→ runtime-only state
→ wheel/contact shape
→ późniejsze behavior-changing changes
```

Dopiero ten ledger może powiedzieć, co jest „najnowszym zwalidowanym feelem”.

---

## 9. VERIFIED — current factory nadal jest split-envelope, nie b3Wheel

Current factory M6 na recovery head używa:

```text
JOZZ_M6_ENVELOPE_SPLIT_SPHERE_SIDEWALL
```

Komentarz w kodzie mówi wprost, że true-width wheel contact (torus/cylinder/b3Wheel family) ujawnił steering shimmy, więc default nie został zmieniony.

To rozdziela trzy rzeczy:

- aktualny factory M6 baseline;
- eksperymentalny program `b3Wheel`;
- owner-session specimen, który może mieć inne `wheelEnvelope.mode`.

Nie mieszaj ich w jednym słowie „current”.

---

## 10. VERIFIED — nie wszystkie sidecary są równie zepsute

Nie wolno po znalezieniu steering-sidecar corruption przejść do tezy:

```text
sidecars = garbage
```

Przykład: `one_sided_wheel_mount.asset.json` jest realnie konsumowany przez trailing-arm physics importer; resolved positions z kontraktu zasilają rzeczywistą geometrię fizyczną.

Dlatego audyt ma być **field-by-field / consumer-by-consumer**, nie file-by-file.

Docelowa macierz dla każdego istotnego pola:

```text
producer
consumer
runtime effect
validator coverage
owner evidence
status: VERIFIED / CONTRADICTED / UNUSED / UNKNOWN
```

---

## 11. VERIFIED — executable front door nadal eksponuje historyczne milestone'y

Mimo że dokumentacja twierdzi, że M0–M9 są historią/archiwum, sample registry nadal pokazuje użytkownikowi równorzędnie m.in. M1, M2, M5, M6, M8 i M9.

To jest realny routing problem: agent/człowiek może łatwo wybrać historyczny instrument jako „kolejną wersję produktu”.

Jednocześnie nie wolno usuwać ich po nazwie:

- M6 nadal ma implementation dependencies z M5;
- M8/M9 mają wartość jako isolated research instruments;
- nazwa milestone'a nie dowodzi dead code.

Najpierw dependency/capability graph, dopiero później reduction.

---

## 12. Kluczowa diagnoza kampanii JV-Web

Owner wrócił do JV-Core głównie dlatego, że port do `Jozzpoly/JV-Box3D-Web-experiment` przez wiele tygodni nie potrafił skopiować aktualnego działającego riga i feelu.

W ostatniej kampanii Web odkryto wzorzec:

```text
working native behavior / authored asset
        ↓
secondary docs + contract + receipt + calibration
        ↓
agent reconstructs its own Web model
        ↓
tests validate that model against its own assumptions
        ↓
PASS
        ↓
owner sees obvious rig/axis/feel regression
```

To jest potencjalna **authority inversion**.

Nowa podstawowa zasada portu — do ponownej walidacji, ale traktowana poważnie:

> jeżeli istnieje działający system w core JV, najpierw odtwórz jego rzeczywiste zachowanie i provenance; nie rekonstruuj go z dokumentacji.

Jeżeli Web musi się różnić, różnica ma być jawna i uzasadniona dopiero po parity baseline.

---

## 13. Czego NIE uznawać przy takeover za prawdę

Nie przyjmuj bez ponownego sprawdzenia:

- że `CURRENT_STATE_INDEX_PL.md` poprawnie wskazuje najważniejszy obecny cel;
- że najnowszy commit = najnowszy owner-validated feel;
- że `uliczny.json` = owner feel;
- że `checkpoint/owner-feel-source-5b92e9c` = final golden bez owner verdictu;
- że `M6` jest jednym specimenem;
- że green validator dowodzi body-role semantics;
- że `physicsAuthority=false` oznacza „asset irrelevant”;
- że caster/KPI hardpoint generator ma pierwszeństwo przed authored axis;
- że wszystkie sidecary są błędne;
- że wszystkie M1–M9 należy usunąć;
- że nowy `b3Wheel` jest aktualnym shipping/default wheel;
- że stary handoff z 2026-08-06 jest bieżącym planem;
- że archive jest nieważne — może zawierać dowód historii, ale nie instrukcję bieżącą.

---

## 14. Zalecany cold-start nowej rozmowy

### H0 — identity + freeze

Read-only:

1. potwierdź `main`, `recovery/jv-reconstruction`, `jozz-scan-terrain-f0`;
2. potwierdź exact recovery HEAD + tree;
3. nie dotykaj chronionego lokalnego worktree;
4. nie uruchamiaj GitHub Actions;
5. nie implementuj physics/rig/docs cleanup;
6. przeczytaj ten forensic handoff jako mapę pytań, **nie authority**.

### H1 — reacquire owner evidence

Z File Library pobierz:

```text
JV_OWNER_FEEL_SESSION_5b92e9c.json
```

Jeżeli w nowej rozmowie dostępne są screeny ownera z yellow/red split + blue/green steering axis, użyj ich. Jeśli nie — poproś Jozza o te konkretne obrazy zamiast rekonstruować ich sens z pamięci.

### H2 — build `Truth / Contradiction Ledger`

Dla każdego krytycznego claimu zapisuj:

```text
claim
producer/source
consumer
actual executed behavior
validator coverage
owner verdict
confidence
```

Zacznij od:

- `ChassisMount_b`;
- `WheelCenter`;
- yellow/non-steering member;
- red/steering member;
- wheel-spin member;
- `Axis_SuspensionTravel_Top/Bottom`;
- upper/lower ball / generated kingpin;
- caster/KPI;
- rack/tie rod;
- wheel envelope;
- contact solver global state;
- owner session/presets.

### H3 — recover `Golden Specimens`, nie jeden „golden M6”

Minimum:

```text
G-RIG    = owner-accepted rig semantics/geometry
G-FEEL   = owner-accepted driving behavior + full runtime state
G-WHEEL  = currently accepted wheel/contact state for driving baseline
G-MAP    = current map/scan state (osobny zakres)
G-RESEARCH = instrument configurations, not product defaults
```

Nie wymuszaj, żeby jeden commit był golden dla wszystkiego.

### H4 — temporal owner-feel reconstruction

Porównaj co najmniej:

```text
f84f8c3...  M5.2 manual accepted
57ca2d2...  M6 Gate2/preset accepted vicinity
5b92e9c...  owner-feel source candidate
241fe10...  later wheel/research branch
edbc9d73... current recovery tree
```

Do każdego dołącz session/preset/wheel/contact state.

### H5 — manual owner A/B gate

Dopiero gdy kandydaci są odtwarzalni, przygotuj minimalne A/B dla Jozza.

Nie pytaj „który commit jest lepszy”. Pokaż 2–3 jednoznacznie oznaczone specimens i poproś o ocenę:

- steering feel;
- return/self-align;
- stabilność;
- contact smoothness;
- ogólna kontrolowalność;
- czy to jest „ten stan”, który pamięta jako dobry.

### H6 — rig parity gate

Dopiero po truth reconstruction zrób owner-facing visualization:

- yellow suspension-side member;
- red steerable member;
- wheel separate;
- authored steering axis;
- proposed live mapping;
- dwa czytelne rzuty odpowiadające wcześniejszym annotations.

### H7 — dopiero potem dokumentacja/cleanup

Po zamknięciu truth ledger:

1. wskaż active docs do poprawy;
2. oznacz historyczne claims;
3. usuń lub zdegraduj mylące entrypointy;
4. rozszerz validators dokładnie o odkryte blind spots;
5. dopiero na końcu wykonaj reduction/deletion.

Nie twórz kolejnego „fundamental cleanup” na niezwalidowanej prawdzie.

---

## 15. STOP conditions

Natychmiast zatrzymaj implementację, jeśli:

- nie potrafisz powiązać owner verdictu z exact config/session;
- dwa źródła prawdy przeczą sobie i nie ma executable evidence;
- test jest self-referential (generator i oracle bazują na tej samej założonej geometrii);
- plan wymaga kasowania historycznych sample'ów przed dependency graph;
- repair riga wymaga zgadywania authored axis;
- port do Web opiera się na prose contract zamiast rzeczywistym native behavior;
- próbujesz użyć `main` jako pełnego recovery state;
- potrzebny jest lokalny plik z `build/`, a nie masz do niego dostępu.

W ostatnim przypadku od razu poproś Jozza o dokładnie nazwany plik. Nie obchodź długo ograniczeń środowiska.

---

## 16. Istotny historyczny handoff z 2026-08-06

Istnieje `OSTATECZNY HANDOFF — Jozz Vehicle / Box3D — B3WHEEL-STEER-01C`.

Jest wartościowy jako provenance stanu badawczego i zawiera m.in.:

- `b756f091...`;
- tree `954c3c65...`;
- protected local worktree `241fe10...`;
- wynik source-localization steering;
- blokadę sztucznego centrowania;
- wymaganie wiarygodnego pre-release fork.

**Nie używaj jego „next immediate package C1a” jako automatycznego obecnego priorytetu.** Od 2026-08-16 nadrzędnym problemem jest najpierw truth recovery riga + feelu, ponieważ bez tego dalsze badania mogą mierzyć nie ten specimen, który owner uważa za właściwy produkt.

---

## 17. Ostateczny stan tego checkpointu

```text
product code modified in this forensic session: NO
physics modified: NO
main modified: NO
protected local worktree touched: NO
GitHub Actions run: NO

live recovery HEAD before handoff write:
edbc9d73ae612e51184b4a438775418d4d1b7e3c
live recovery tree:
954c3c65597da516065c2005bf65f8925cbe0635

rig truth audit: PARTIAL, critical contradictions proven
owner feel recovery: PARTIAL, strong 5b92e9c + session candidate found
latest validated feel: UNKNOWN
final golden rig: UNKNOWN
safe documentation deletion set: UNKNOWN
JV-Web parity-ready foundation: NOT YET
```

### Misja następnej rozmowy

Nie „kontynuować od miejsca, gdzie skończyliśmy” mechanicznie.

**Najpierw jeszcze raz, poważniej i od zera zweryfikować projekt jako system dowodów.** Jeżeli obecny forensic model wytrzyma falsyfikację — kontynuować. Jeżeli nie — wyrzucić nasze hipotezy i zbudować poprawniejszy model.

Celem jest przygotowanie native JV tak, aby następna próba portu do JV-Web mogła przenieść **rzeczywisty aktualny rig i rzeczywiście zaakceptowany driving feel**, a nie kolejną wewnętrznie spójną rekonstrukcję starego błędu.
