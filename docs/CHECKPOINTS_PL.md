# Checkpoints / handoff — Jozz Vehicle Box3D Native

Dziennik decyzji technicznych i handoffów. **To jest domyślny sposób
dokumentowania zmian** — zamiast pełnego `docs/*.md` na każdy drobiazg.

**Format wpisu (≤5 linii, najnowsze u góry):**
```
## YYYY-MM-DD · Tytuł · <commit lub „docs">
- CO:     jedno zdanie, co zmieniono
- CZEMU:  jedno zdanie, po co
- EFEKT:  weryfikowalny rezultat (walidator/render/liczby)
- DALEJ:  następny krok albo „—"
```

Zasady: pełne raporty (analizy, plany) zostają w dedykowanych `docs/*.md`; tutaj
tylko skrót + link. Gdy przekroczy ~30 wpisów — najstarsze usuń (są w gicie).

---

## 2026-07-08 · Analiza rig/dumper/mount + workflow + checkpointy · docs
- CO:     nowy `SUBSYSTEM_RIG_DAMPER_MOUNT_PL.md` (stan wizualnego rigu) + ten dziennik; README §5 token-economy.
- CZEMU:  przygotowanie pod dalszą pracę; wizual dumpera był odłączony od fizyki i nigdzie zwięźle nieopisany.
- EFEKT:  wiedza o podsystemie zapisana w repo (nie tylko w pamięci agenta); handoff ma stały format.
- DALEJ:  polish rigu/dumpera wg planu P1–P4 w subsystem-doc — po zgodzie Jozza, mały krok = render.

## 2026-07-08 · Zasady oszczędności tokenów · b02a5df
- CO:     README §5 — zwięzłe outputy narzędzi, quiet git flags, grep>Read, batchowanie, 1 milestone/sesja.
- CZEMU:  Jozz: minimalizacja kosztu bez utraty jakości/rygoru.
- EFEKT:  bramka (build+walidator+test) i dyscyplina commitów bez zmian; skraca się tylko narracja w czacie.
- DALEJ:  stosować co sesję; TECH_DEBT #1 zamknięty (M7/M8 zacommitowane).

## 2026-07-08 · Commit + push całego M7+M8 · 1446c9d, d2da267
- CO:     ~tydzień niezacommitowanej pracy pogrupowany w 2 commity i wypchnięty na `jozz-vehicle-sandbox-m0`.
- CZEMU:  ryzyko #1 z przeglądu — praca poza historią gita, jeden zły reset ją kasuje.
- EFEKT:  branch zdalny kompletny; ustanowiona zasada autonomicznego commit/push agentów (main = tylko Jozz).
- DALEJ:  po każdym zielonym etapie — commit, nie czekać na prośbę.

## 2026-07-08 · Przegląd techniczny / porządkowanie · d2da267
- CO:     README_FOR_AGENTS przepisany na 1 front (M8); nowy TECH_DEBT_PL; CURRENT_STATE_INDEX odchudzony.
- CZEMU:  fronty były na M6/M7, sprzeczne z kodem (usunięty self-align), rozrost docs.
- EFEKT:  jedno repo-widoczne źródło prawdy dla Claude i Codex.
- DALEJ:  trzymać dyscyplinę: po realnej zmianie aktualizować README §2 + ten dziennik.

## 2026-07-08 · UI PL + presety + auto-sesja · 1446c9d
- CO:     6 zakładek PL, font Segoe UI+/utf-8, presety (`config_io`, `assets/vehicle_presets`), auto-zapis sesji, fix ID zakładek.
- CZEMU:  UI chaotyczne/po angielsku; „R" kasowało strojenie; zakładki skakały przy Apply.
- EFEKT:  strojenie przeżywa restart; zakładki stabilne; render zweryfikowany.
- DALEJ:  brak własnego raportu UI (TECH_DEBT #3) — zamknąć przy okazji.

## 2026-07-07 · M8 rig + opadająca poza · 1446c9d
- CO:     wahacze wpięte w authored-sockety, opadanie (`restArmDroopDeg`+`suspensionPreload`), kompensacja bump-steer.
- CZEMU:  fundament pod drift/offroad/ciężarówki; wahacze wyginały się do góry, wchodziły w oponę.
- EFEKT:  walidator OK; render potwierdzony; droop klamrowany na 16° (over-center Ackermanna).
- DALEJ:  droop >16° wymaga przeprojektowania kierownicy (TECH_DEBT #5, odłożone).

## 2026-07-06 · M7 realne siły · 1446c9d (rozwój przed commitem)
- CO:     wahacze jako ciała na zawiasach, back-drivable rack, napęd momentem, trailing-arm tył; usunięty skryptowy self-align.
- CZEMU:  kierunek BeamNG — zachowanie ma wynikać z konstrukcji, nie skryptu.
- EFEKT:  landing integrity, hands-off align, wheelspin — w walidatorze.
- DALEJ:  drivetrain (dyfry), model opony — roadmapa README §8.
