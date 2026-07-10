# PLAN WYKONAWCZY — stabilność zawieszenia i polish prowadzenia (P1–P6 → dalej)

> ## ✅ ZAMKNIĘTE (2026-07-09) — TO JEST HISTORIA, NIE AKTYWNY PLAN
> P1–P6 wykonane i zweryfikowane audytem zero-trust; Bramki 1–2 (fix ściągania,
> model tarcia P4b) i deterministyczny fix presetu — **zaakceptowane przez
> Jozza w teście ręcznym**. Aktualny stan: `CHECKPOINTS_PL.md` (najnowsze u
> góry) + `README_FOR_AGENTS.md §2`. Aktywny plan porządków:
> `PLAN_PORZADKI_FUNDAMENT_2026_07_09_PL.md`. Ten dokument zostaje jako zapis
> JAK to zrobiono (metoda: sonda-przed-fixem, bramka-po-etapie, STOP-gate) —
> wzorzec do powielania, nie lista do wykonania.

Operacyjna mapa naprawy wynikająca z audytu
`AUDIT_PHYSICS_STEERING_2026_07_08_PL.md` (tam: diagnozy i dowody; tutaj:
JAK to wykonać krok po kroku). Pisany tak, żeby wykonać go mógł tańszy/słabszy
agent **bez zgadywania**. Taski w harness: #40–45.

---

## 0. Zasady dla wykonawcy (przeczytaj ZANIM zaczniesz)

1. **Jeden etap = jedna sesja.** Nie łącz etapów. Po zielonej bramce: commit +
   push na `jozz-vehicle-sandbox-m0`, wpis w `CHECKPOINTS_PL.md`, koniec sesji.
2. **Numery linii w tym planie DRYFUJĄ** (kod żyje). Zawsze szukaj po nazwie
   funkcji/stałej (Grep), numer linii traktuj jako wskazówkę.
3. **Bramka po każdym etapie** (§9 — skopiuj checklistę). Walidator: czytaj
   WYDRUKOWANE LICZBY, nie sam `OK` (asertuje luźno — README §3).
4. **Reprodukcja przed naprawą.** Tam gdzie plan każe najpierw napisać sondę i
   zobaczyć jak OBECNY kod failuje — zrób to i zapisz zmierzone liczby w
   checkpoincie. Jeśli sonda NIE failuje na starym kodzie, nie brnij: zapisz
   to i wykonaj fix mimo wszystko (uzasadnienie statyczne stoi), sondę zostaw
   jako regresję.
5. **Warunki STOP** przy każdym etapie: gdy zajdą — przerwij, zapisz stan w
   checkpoincie, zapytaj Jozza. Nie improwizuj obejść.
6. **Zakres = tylko to, co w etapie.** Żadnych refaktorów przy okazji, żadnych
   zmian w `src/`/`include/` (rdzeń box3d nietykalny).
7. Buduj przez `cmake --build --preset windows-debug --target <t>` (PowerShell,
   z katalogu repo). Przed buildem ubij `samples.exe` (blokuje linker).

---

## 1. Kolejność i zależności

```
P2 (fix rackTravel w Apply)      — 1 linia + tripwire; usuwa czynnik zakłócający
 └→ P1 (twist-fence)             — usuwa ŁAMANIE skrętu; rdzeń całego planu
     └→ P3 (prześwit przód/tył)  — dopiero na stabilnym rigu ma sens pomiar pozy
     └→ P4 (powrót kierownicy)   — zależy od P1 (odbicia testują płoty)
         └→ P5 (suwaki + opisy)  — suwak max skrętu wymaga P1 (płot z konfigu) i P2
             └→ P6 (sanity)      — porządki na końcu, gdy zachowanie już stabilne
```

**Dlaczego tak:** P2 przed P1, bo jest trywialny (rozgrzewka + nauka bramki) i
usuwa confound — bez niego testy udarowe P1 mogłyby trafiać na rozstrojony
limit maglownicy. P1 przed wszystkim dalszym, bo **strojenie łamliwego układu
stroi się do bugów**. P3/P4 równoległe koncepcyjnie, ale rób sekwencyjnie.
P5 na końcu strojenia (suwak max skrętu przelicza płoty z P1 i limit z P2).
P6 ostatni — zmienia defaulty, więc wymaga stabilnego punktu odniesienia.

---

## 2. ETAP P2 — `RecomputeRackTravel()` w Apply (task #41) — ZACZNIJ OD TEGO

**Cel:** po „Zastosuj" limit maglownicy zawsze odpowiada aktualnej geometrii.

**Pliki:** `samples/jozz_vehicle_m6_rig_lab.cpp`,
`samples/jozz_vehicle_m6_suspension_rig.cpp`, `samples/jozz_vehicle_validation.cpp`.

**Kroki:**
1. W `ApplyPendingStructuralSetup()` (rig_lab, ~l. 345; szukaj po nazwie)
   dodaj `RecomputeRackTravel();` PO skopiowaniu wszystkich pól edit→config,
   a PRZED `CreateVehicle();`. Kolejność jest istotna: funkcja czyta
   `m_config.wishbone/axleHalfSpacing/trackHalfWidth/rackHalfWidth/maxSteeringAngleDegrees`.
2. Tripwire na przyszłość: w `CreateJozzVehicleM6()`
   (jozz_vehicle_m6_suspension_rig.cpp, ~l. 890, blok tworzenia racka ~l. 919)
   dodaj porównanie `config.rackTravel` z wartością przeliczoną na miejscu z
   `ComputeJozzVehicleM6RackStroke(config.wishbone, 2*axleHalfSpacing,
   trackHalfWidth, rackHalfWidth, maxSteeringAngleDegrees*DEG)`. Przy różnicy
   > 1e-4 m: `std::printf("jozz m6 WARNING: stale rackTravel %.4f vs %.4f\n",...)`.
   NIE assert (printf wystarczy — walidator i tak czyta stdout). To łapie KAŻDĄ
   przyszłą ścieżkę, która zapomni przeliczyć.
3. W walidatorze dodaj mini-regresję (wzoruj się na istniejących sondach M7,
   np. „m7 landing integrity probe", ~l. 847): zbuduj config domyślny, zmień
   `wishbone.steeringArmBack` z 0.17 na 0.22, przelicz rackTravel wzorem z
   kroku 2, zbuduj pojazd — na stdout NIE może pojawić się „stale rackTravel".
   Wydrukuj obie wartości rackTravel (stare/nowe geometrie) — mają się różnić
   (to dowód, że przeliczanie w ogóle coś zmienia).

**Weryfikacja:** build 3 targetów + walidator (czytaj: brak WARNING, wydruk
dwóch różnych rackTravel) + test.exe + boot smoke 300 klatek.

**Wynik etapu:** commit z fixem + tripwire + regresją; checkpoint z liczbami
(rackTravel przed/po dla steeringArmBack 0.17→0.22).

**Kryterium poprawności:** WARNING nie występuje w żadnym przebiegu walidatora;
zmiana geometrii kierownicy suwakiem + Zastosuj nie powoduje przestrzału
(sprawdzisz to w pełni w P1 sondą udarową — tu wystarczy regresja).

**Ryzyka/ostrożność:** żadnych znanych. Zmiana nie dotyka fizyki istniejących
przebiegów (domyślna geometria daje identyczny rackTravel jak dziś).

**STOP gdy:** tripwire strzela na CZYSTYM defaultowym configu (znaczy, że
istnieje jeszcze inna rozjechana ścieżka — zapisz którą i pytaj).

---

## 3. ETAP P1 — twist-fence z konfiguracji (task #40) — RDZEŃ PLANU

> **P1 ZAAKCEPTOWANE przez Jozza 2026-07-08.** Płot z §3c wdrożony (przód
> maxSteer+10°, tył 15°, zamiast hardcoded ±70°). Sonda z §3b/3d ujawniła
> osobny efekt (kółko klinuje się w statycznym syntetycznym teście udarowym,
> niezależnie od płotu) — ale Jozz przetestował ręcznie ~10 min ekstremalnej
> jazdy i ani razu nie odtworzył pierwotnie zgłoszonego zerwania kierownicy,
> które wcześniej było łatwe do wywołania. **Decyzja: sukces, kontynuować
> plan.** Znalezisko zostaje jako watch-item (`TECH_DEBT_PL.md` #9, obniżone
> do 🟡, nie blokuje) — syntetyczna sonda pozostaje w walidatorze
> (diagnostyczna, nie gating).



**Cel:** koło fizycznie NIE MOŻE przeskoczyć martwego punktu drążka →
„złamany skręt" przestaje istnieć; po udarze układ zawsze wraca.

**Pliki:** `samples/jozz_vehicle_m6_suspension_rig.cpp` (+ walidator).

### 3a. Przygotowanie — policz martwy punkt (bez zmian w rigu)

1. W walidatorze napisz pomocnika (lokalnie, w validation.cpp):
   pętla po kącie α od 1° do 89° co 0.5°, licz
   `stroke = ComputeJozzVehicleM6RackStroke(geometry, wheelbase, track, rackHalfWidth, α)`.
   **Martwy punkt = pierwszy α, przy którym stroke przestaje rosnąć**
   (stroke(α+0.5°) <= stroke(α)) — za nim linkage nie ma już przełożenia.
2. Wydrukuj go dla domyślnej geometrii. Oczekiwanie: **~46–60°** (przy
   ackermannFraction 0.6 wyżej niż zmierzone 46° przy 1.0). Zapisz liczbę.

### 3b. Reprodukcja — sonda udarowa NA OBECNYM kodzie

3. Nowa sonda w walidatorze (wzór: „m7 landing integrity probe" — tam jest
   gotowy setup świata z ziemią; kopiuj strukturę):
   - zbuduj domyślny pojazd, ustabilizuj 120 kroków (60 Hz, hands-off:
     `UpdateJozzVehicleM6Drive` z input {0,0,false} co krok);
   - udar: `b3Body_SetLinearVelocity` na ciele KOŁA przednio-lewego
     (`vehicle.corners[JOZZ_M6_FRONT_LEFT].wheelId`) — prędkość boczna
     `{0, 0, +V}` dodana do aktualnej; przebiegi dla V = 6, 10, 14 m/s
     (trzy osobne buildy pojazdu, nie jeden po drugim);
   - po udarze 300 kroków hands-off; co 60 kroków drukuj
     `GetJozzVehicleM6WheelTelemetry(...).steeringAngle` (w stopniach!) dla
     FL i FR.
4. **Zapisz zachowanie:** złamanie = kąt po 300 krokach > 45° (koło
   zaklinowane). Jeśli przy żadnym V nie łamie — patrz zasada §0 pkt 4
   (fix i tak wchodzi; sonda zostaje regresją).

### 3c. Implementacja płotu

5. `CreateControlArm(...)` dostaje nowy parametr `float twistLimitRadians`
   (przekazywany z `CreateWishboneCorner`). W bloku spherical jointu
   (szukaj `enableTwistLimit`, ~l. 592) zamień hardcode ±70°:
   `def.lowerTwistAngle = -twistLimitRadians; def.upperTwistAngle = +twistLimitRadians;`
6. W `CreateWishboneCorner` policz limit per narożnik:
   ```
   float twistFence = IsFrontCorner(corner)
       ? (config.maxSteeringAngleDegrees + 10.0f) * DEGREES_TO_RADIANS   // skręt + margines
       : 15.0f * DEGREES_TO_RADIANS;                                     // tył nie skręca
   ```
   Stałe 10°/15° zostają w kodzie z komentarzem (celowo NIE w konfigu — to
   płot bezpieczeństwa, nie parametr strojenia).
7. Cone limit (80°) w P1 **NIE ruszaj** (to P6). Jedna zmiana na raz.
8. W walidatorze dodaj asercję: `frontFenceDeg <= deadPointDeg - 3.0f`
   (martwy punkt z kroku 1–2, liczony dla AKTUALNEJ geometrii configu).

### 3d. Weryfikacja (najważniejsza część etapu)

9. Sonda udarowa z 3b na NOWYM kodzie: dla każdego V —
   - w trakcie całego przebiegu `|steeringAngle| < fence + 2°`;
   - po 300 krokach `|steeringAngle| < 12°` (układ wrócił do prostej);
   - wartości WYDRUKOWANE (przed/po fixie — porównanie do checkpointu).
10. **Test nie-przeszkadzania:** płot nie może ciąć NORMALNEGO ruchu:
    - istniejące sondy M7 (landing 2.0/3.5 m, hands-off align, trailing)
      muszą przejść bez zmian liczb (porównaj wydruki z main);
    - pełny skręt w miejscu: hands-on steer=1.0 przez 120 kroków → kąt
      osiąga ≥ 30° (limit komendy 32°) — płot 42° nie przeszkadza.
11. Jazda ręczna (jeśli sesja ma dostęp): najedź na washboard pod kątem
    ~30° przy 5–8 m/s — koło nie zostaje wykrzywione. Zrzut/telemetria.

**Wynik etapu:** commit (rig + walidator z 2 nowymi sondami), checkpoint z
liczbami: martwy punkt, kąty przy udarach przed/po.

**Kryterium poprawności:** wszystkie punkty z 3d; ŻADNA istniejąca sonda nie
zmieniła wyników.

**Ryzyka/ostrożność:**
- **Crosstalk skoku na twist:** oś twist = sworzeń (kingpin), więc czysty skok
  zawieszenia powinien dawać znikomy twist — ale ZWERYFIKUJ: w sondzie
  lądowania 2.0 m wydrukuj max |steeringAngle| przednich kół podczas
  kompresji; jeśli > ~6°, margines 10° może być za mały → podnieś do 15°
  i powtórz 3d.
- Limity twist w box3d mogą być „miękkie" przy ogromnych impulsach (solver
  iteracyjny) — dlatego kryterium to `fence + 2°` tolerancji, nie równość.

**STOP gdy:** (a) sondy M7 zmieniają wyniki po fixie; (b) płot tnie normalny
skręt (pkt 10 failuje); (c) po udarze koło nadal klinuje się > 20° — wtedy
mechanizm jest inny niż zdiagnozowany, wróć do audytu §2 i pytaj Jozza.

---

## 4. ETAP P3 — prześwit przód/tył, rozprzęgnięty z twardością (task #42)

**Cel:** dwa suwaki „Prześwit przód/tył" działające niezależnie od twardości.

**Pliki:** rig (.h/.cpp), rig_lab, config_io, 3 presety JSON, walidator.

**Kroki:**
1. **NAJPIERW POMIAR** (dowód kierunku błędu, na obecnym kodzie): sonda w
   walidatorze — zbuduj pojazd 3× z `frontSuspensionScale` = 0.5 / 1.0 / 2.0,
   po 300 krokach hands-off wydrukuj `b3Body_GetPosition(chassisId).y` oraz
   `suspensionTravel` telemetrii przednich kół. Oczekiwanie z audytu §4:
   wysokość ROŚNIE ze scale (błędny kierunek). Zapisz liczby.
2. W `JozzVehicleM6Config` zamień `float suspensionPreload` na
   `float suspensionPreloadFront; float suspensionPreloadRear;`
   (defaulty oba 0.07). Kompilator wskaże wszystkie użycia — przejdź po
   KAŻDYM (grep `suspensionPreload`): tworzenie coilovera wishbone
   (~l. 647), trailing arm (preloadedLength), `ApplySuspensionTuning`
   (rig_lab ~l. 677), env hook `JOZZ_M6_PRELOAD` (ustawia oba).
3. W każdym użyciu: wybierz front/rear wg narożnika i **USUŃ mnożenie przez
   scale** (`* scale` przy preload — zostaje przy hertz/damping!).
4. `config_io`: writer zapisuje dwa nowe klucze; reader — kompatybilność
   wstecz: stary klucz `suspensionPreload` (jeśli obecny) ładuje się do OBU
   pól. Zaktualizuj 3 presety w `assets/vehicle_presets/` na nowe klucze.
5. UI (zakładka Zawieszenie, sekcja Postawa): suwak „Prześwit" → dwa suwaki
   „Prześwit przód" / „Prześwit tył" (zakres jak dziś: −0.08..0.20 m, live,
   wywołują `ApplySuspensionTuning`). Tooltip: „Podnosi/opuszcza oś przez
   napięcie wstępne sprężyny. Nie zmienia twardości."
6. Powtórz pomiar z kroku 1 po zmianie: wysokość przy scale 0.5/1.0/2.0 ma
   się zmieniać ZNACZNIE mniej (zostaje tylko naturalny wpływ ugięcia);
   dodatkowo pomiar niezależności: preloadFront 0.12 / preloadRear 0.07 →
   przód wyżej, tył bez zmian (liczby!).

**Wynik:** commit + checkpoint z tabelką wysokości przed/po.

**Kryterium:** sesja/presety ładują się (stare i nowe klucze); niezależność
osi potwierdzona liczbami; walidator/test zielone; zrzut UI z dwoma suwakami.

**Ryzyka:** trailing arm ma motionRatio — preload tam już jest mapowany przez
motionRatio, zostaw to mapowanie, usuń tylko `* scale`. Sesja
`build/jozz_vehicle_m6_session.json` ze starym kluczem musi się wczytać
(krok 4 to gwarantuje) — przetestuj ręcznie jednym uruchomieniem.

**STOP gdy:** pomiar z kroku 1 pokaże kierunek ODWROTNY niż audyt przewiduje
(wtedy diagnoza K3 jest błędna — nie zmieniaj kodu, zapisz liczby, pytaj).

---

## 5. ETAP P4 — naturalny powrót kierownicy (task #43)

**Cel:** puszczona kierownica wraca płynnie; przy mocnym odbiciu lekko
przeciąga w drugą stronę; parkowanie trzyma koła.

**Pliki:** rig (.h/.cpp), rig_lab (suwaki), config_io, walidator.

**Kroki:**
1. Config: `rackFrictionForce` → dwa pola:
   `rackStaticFrictionForce` (default **120 N**) i
   `rackKineticFrictionForce` (default **60 N**). Kompatybilność w config_io
   jak w P3 (stary klucz → oba, static = wartość, kinetic = 0.5×wartość).
2. W `UpdateJozzVehicleM6Drive`, gałąź hands-off (szukaj
   `b3PrismaticJoint_EnableSpring( vehicle.rackJointId, false )`):
   ```
   float rackSpeed = b3PrismaticJoint_GetSpeed( vehicle.rackJointId );  // API istnieje, box3d.h:1377
   float cap = std::fabs(rackSpeed) < 0.01f ? config.rackStaticFrictionForce
                                            : config.rackKineticFrictionForce;
   b3PrismaticJoint_SetMaxMotorForce( vehicle.rackJointId, cap );
   ```
   (motorSpeed zostaje 0 — motor dążący do v=0 z capem = tarcie Coulomba).
3. UI Kierownica: suwak „Tarcie zębatki" → dwa: „Tarcie statyczne (trzymanie)"
   0–400 N i „Tarcie kinetyczne (ruch)" 0–300 N, live. Tooltipy: statyczne =
   ile trzeba, żeby koła w ogóle ruszyły (parkowanie/szarpnięcia); kinetyczne =
   opór podczas powrotu (mniejsze → żywszy powrót i przestrzał).
4. Sonda w walidatorze „steering return":
   - pojazd jedzie prosto do ~12 m/s (drive=1 przez ~180 kroków);
   - steer=1.0 przez 60 kroków (pełny skręt w lewo);
   - steer=0 (hands-off), 240 kroków; co krok śledź `steeringAngle` FL;
   - drukuj: kąt w momencie puszczenia, **minimum przebiegu** (przestrzał
     na minus = przeciągnięcie w prawo), kąt końcowy.
   - Kryteria PO fixie: przestrzał < 0° wystąpił (choć raz ujemny, co najmniej
     −1°), kąt końcowy |α| < 3°, brak trwałej oscylacji (ostatnie 60 kroków:
     amplituda < 1°). PRZED fixem (uruchom raz na starym kodzie): przestrzał
     ~0 (nigdy ujemny) — zapisz do porównania.
5. Sonda „parking hold": auto stoi, hands-off, 180 kroków → dryf kąta < 1°
   (tarcie statyczne trzyma). Ta asercja częściowo istnieje w sondach M7
   (parking-hold) — upewnij się, że nadal przechodzi z nowymi defaultami.

**Wynik:** commit + checkpoint z liczbami (przestrzał przed/po, defaulty).

**Kryterium:** kryteria sondy z kroku 4 i 5; istniejące sondy M7 hands-off
align bez pogorszenia (self-align nadal działa: sprawdź wydruki).

**Ryzyka/ostrożność:** za niskie tarcie kinetyczne → shimmy (oscylacja racka).
Kryterium anty-oscylacyjne w kroku 4 to łapie. Jeśli shimmy: podnoś kinetic
co 20 N aż zniknie, zapisz próg. Threshold 0.01 m/s w kroku 2 jest zgrubny —
jeśli parking dryfuje, zmniejsz do 0.005.

**STOP gdy:** nie da się jednocześnie spełnić „przestrzał istnieje" i „brak
shimmy" w zakresie 40–150 N kinetic — wtedy model tarcia wymaga członu
zależnego od prędkości (decyzja Jozza, nie improwizuj).

---

## 6. ETAP P5 — brakujące suwaki + pełne opisy (task #44)

**Cel:** kompletna, samoopisująca się konsola strojenia.

**Kroki (kolejność wewnątrz etapu):**
1. **Suwak „Maksymalny skręt kół"** (Kierownica, 20–45°): pole
   `maxSteeringAngleDegrees` jest STRUKTURALNE (płoty P1 i rackTravel P2 baką
   się przy budowie) → wprowadź przez wzorzec pending-edit (`m_editMaxSteer` +
   kopiowanie w `ApplyPendingStructuralSetup` + `SyncEditFromConfig`) — wzoruj
   się na `m_editWishbone`. Etykieta z dopiskiem „(wymaga Zastosuj)".
2. **Toe statyczne przód/tył** (`frontToeDeg`/`rearToeDeg`, default 0,
   zakres −3..+3°, strukturalne): implementacja przez zmianę DŁUGOŚCI drążka/
   toe-linku w `CreateWishboneCorner`:
   `def.length = DistanceBetween(...) - config.wishbone.steeringArmBack * std::tan(toeRad) * (IsLeftCorner ? +1 : -1 ...)` —
   UWAGA: znak wyprowadź STARANNIE (toe-in = przody kół do środka; dodatni
   toe-in = kąt zbieżności). Weryfikacja w walidatorze: po zbudowaniu z
   toe=+1° telemetria `steeringAngle` w spoczynku = ±1° ±0.3° (lewe/prawe
   przeciwne znaki). Jeśli znak wychodzi odwrotnie — odwróć w JEDNYM miejscu
   i skomentuj dlaczego.
3. **Opisy wszystkich suwaków** wg szablonu Jozza — każdy `HelpMarker`:
   co kontroluje / na co wpływa w jeździe / efekt zwiększenia / efekt
   zmniejszenia / kiedy używać / typ: [FIZYCZNY]/[WIZUALNY]/[DEBUG].
   Przejdź zakładka po zakładce (tabela audytu §6 = checklista).
4. W nagłówku sekcji „Geometria wahaczy (zaawansowane)" dodaj jedną stałą
   linię: „Zmiany działają na fizykę i linie debug — model 3D auta NIE
   przeskalowuje się (rysowany z socketów oryginalnego modelu)."
5. Zaktualizuj `docs/SUBSYSTEM_UI_PRESETS_PL.md` (nowe suwaki, nowe klucze
   configu z P3/P4/P5).

**Weryfikacja:** build+walidator (toe-sonda!)+test; zrzut KAŻDEJ zmienionej
zakładki (`JOZZ_M6_TAB=0..5` + `--screenshot`); polskie znaki renderują się
(font Segoe — sprawdź na zrzucie, nie w kodzie).

**Kryterium:** toe-sonda w granicach ±0.3°; max-skręt 40° + Zastosuj →
pełny lock osiąga ~40° i płot P1 podąża (wydruk fence w walidatorze).

**Ryzyka:** znak toe (krok 2) — najczęstszy błąd; weryfikuj liczbą, nie okiem.
Max skręt 45° + ackermannFraction 1.0 może zbliżyć się do martwego punktu —
asercja z P1 (`fence <= deadPoint - 3°`) MUSI zostać w walidatorze; jeśli
strzela przy 45°, zaciśnij górny zakres suwaka do wartości bezpiecznej i
odnotuj w checkpoincie.

**STOP gdy:** asercja martwego punktu nie daje się spełnić w całym zakresie
suwaka — wymagana decyzja Jozza o zakresie.

---

## 7. ETAP P6 — sanity limitów i mas (task #45)

**Cel:** domyślne wartości i strażnicy przestają być fikcją.

**Kroki:**
1. **Masa koła:** w walidatorze wydrukuj `b3Body_GetMass(wheelId)` dla split
   envelope. Policz oczekiwaną masę samej sfery: `80 * (4/3)π r³` (r z
   configu). Jeśli zmierzona ≈ sfera+cylinder (podwójna) — fix: gęstość
   TYLKO na sferze, cylinder z `density = 0.001` (szukaj `CreateJozzVehicleM6WheelEnvelope`);
   po fixie wydrukuj nową masę i sprawdź, że sondy M7 nadal przechodzą
   (masa koła wpływa na wszystko — spodziewaj się DROBNYCH zmian liczb;
   duże zmiany zachowania = STOP).
2. **Skok domyślny:** zmień w `JozzVehicleM6DefaultConfig` hint-fallback z
   0.70 na 0.45 (→ compression 0.27, rebound 0.18) ALBO — jeśli Jozz woli
   zostawić — tylko ostrzeżenie: w UI przy suwakach skoku pokaż żółty tekst
   gdy `max(compression,rebound) / lowerArmLength > 0.85`; w walidatorze
   wydrukuj stosunek. **Zapytaj Jozza którą opcję wybrać PRZED implementacją**
   (to zmiana feelu defaultowego auta).
   > **ROZSTRZYGNIĘTE (2026-07-09):** zostawić jak jest, tylko ostrzeżenie
   > (wdrożone w P6, commit 56af66e). Jozz: niedługo dojdą nowe modele
   > zawieszenia ze znacznie dłuższymi wahaczami — przy dłuższym ramieniu ten
   > sam skok w cm odpowiada MNIEJSZEMU kątowi, więc saturacja zniknie sama
   > dla tych modeli. Nie zmieniać defaultu pod obecny (krótki) wahacz.
3. **Cone limit:** zamień hardcode 80° na
   `HingeSwingLimit(compression, rebound, armLength) + 15°` (spójny z
   zawiasem + margines). Weryfikacja: sondy lądowania bez zmian wyników.
4. **Sztywność limitów (bump-stopy):** sprawdź `include/box3d/box3d.h` czy
   distance joint ma API miękkiego limitu (szukaj `LowerSpring`/`limit.*hertz`
   — w Box2D v3 prismatic ma sprężyny limitów). Jeśli JEST → wystaw jako
   „Twardość odbojów" (zaawansowane); jeśli NIE MA → wpisz do TECH_DEBT jako
   granicę silnika i NIE dotykaj `src/`.

**Kryterium:** liczby mas w checkpoincie; sondy M7 stabilne; decyzja Jozza
z kroku 2 odnotowana.

---

## 8. Dalsze etapy (kierunek — ogólniej, każdy wymaga osobnego planu)

- **P7 — model opony (M7.4):** krzywa poślizgu (siła vs slip angle), load
  sensitivity, combined slip. Fundament: telemetria slipAngle już istnieje.
  ZACZYNAĆ dopiero po P1–P6 (opona na łamliwym rigu stroi się do bugów).
  Wymaga osobnego dokumentu projektowego + decyzji Jozza o modelu
  (brush model vs uproszczony Pacejka).
- **P8 — wizual ↔ hardpointy (research):** model 3D odzwierciedla zmiany
  geometrii (skalowanie części do hardpointów). Dotyka importera i filozofii
  „części z Blockbencha" — projektować razem z Jozzem.
- **P9 — drivetrain (M7.3):** dyfry, podział momentu, engine braking.
  Po P7 (siły opon definiują, co dyfr ma rozdzielać).
- **P10 — przeprojektowanie Ackermanna pod droop >16°:** odłożone
  (TECH_DEBT #5); po P1 płoty czynią to bezpieczniejszym do eksperymentów.
- **Boczne dampery + polish wizualnego rigu:** wg
  `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` P1–P4 (osobna ścieżka, niezależna od
  fizyki — może iść równolegle w oddzielnych sesjach).

---

## 9. Checklista bramki (kopiuj do każdej sesji)

Kroki 1–4 to teraz **jedno polecenie** `.\tools\gate.ps1 -Numbers` (build 3
targety + walidator + test + boot-smoke, jednoliniowe PASS/FAIL). Reszta
zostaje ręczna:

```
[ ] .\tools\gate.ps1 -Numbers      → BRAMKA: ...OK + PRZECZYTANE liczby sond
[ ] zmiana wizualna? → --screenshot + obejrzyj PNG (render is the gate)
[ ] wpis w docs/CHECKPOINTS_PL.md (co/czemu/efekt-LICZBY/dalej)
[ ] git add <jawne pliki> && commit && push -q origin jozz-vehicle-sandbox-m0
[ ] TaskUpdate: oznacz task etapu jako completed
```
