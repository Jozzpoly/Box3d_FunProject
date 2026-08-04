# README_FOR_AGENTS — Jozz Vehicle (JV)

**To jest jedyne wejście dla agenta pracującego w tym repo.**
Przeczytaj ten plik, potem `docs/CURRENT_STATE_INDEX_PL.md`, a przy pracy nad
kołami także `docs/KOLA_00_INDEX_PL.md`. Nie odtwarzaj bieżącego stanu ze starych
raportów — historia jest w `docs/archive/` i w Git.

- Właściciel i dyrektor kreatywny: **Jozz**. Odpowiadaj po polsku.
- Baseline fizyki przed porządkami: `jozz-scan-terrain-f0` @ `5b92e9c`.
  Bieżącą rewizję zawsze odczytaj przez `git rev-parse HEAD`; nie kopiuj jej do
  dokumentów jako ruchomego „latest”.
- Aktualny kierunek: własny kolider koła/opony w Box3D oraz uczciwa izolacja
  geometrii, topologii kontaktu i podatności.
- JV jest aktywnym laboratorium i źródłem dziedzictwa dla **JES**, ale JV i JES
  nie są tym samym projektem. Zasady transferu: `docs/JV_JES_HERITAGE_PL.md`.
- Mapa dokumentacji: `docs/JV_DOCS_INDEX_PL.md`.

## 1. Co jest dziś prawdą

JV nie jest już „nietykalną nakładką na stockowy Box3D”. Ta reguła została
przekroczona świadomie i udokumentowanie, ponieważ stockowe bryły nie potrafiły
jednocześnie reprezentować szerokiej, gładko toczącej się opony. Repo zawiera
własny typ `b3Wheel` i jego ścieżki kolizji w `src/` oraz `include/`.

Zmiany rdzenia są dozwolone wyłącznie wtedy, gdy spełniają politykę z
`docs/KOLA_03_POLITYKA_BOX3D_PL.md`: mały zakres, jawny kontrakt, dowód potrzeby,
testy oraz brak wpływu przy wyłączonej funkcji. „Nie dotykaj rdzenia nigdy” jest
historyczną zasadą, nie aktualnym zakazem.

Najbliższa praca nie polega na dalszym strojeniu `wheelCrownDrop` ani ponownym
otwieraniu topologii. `WHEEL-RIGID-01`, `WHEEL-SEAM-02A` i `WHEEL-HULL-02B`
zamknęły kontrolowany baseline sztywny dla plane, triangle/mesh oraz finite hull.

Następny pakiet to `WHEEL-SOFT-03`: A/B lokalnej podatności przy identycznej
geometrii, feature IDs i punktach manifoldu. Dopiero jego wynik może uzasadnić
strukturalną oponę. Feeling i domyślne parametry nadal wymagają decyzji Jozza.

## 2. Źródła prawdy

Czytaj w tej kolejności:

1. `README_FOR_AGENTS.md` — reguły pracy i wejście;
2. `docs/CURRENT_STATE_INDEX_PL.md` — bieżący stan kodu, ryzyka i następny krok;
3. `docs/CHECKPOINTS_PL.md` — krótki dziennik najnowszych zmian;
4. `docs/TECH_DEBT_PL.md` — wyłącznie otwarty dług;
5. `docs/KOLA_00_INDEX_PL.md` — program badawczy koła/opony;
6. `docs/JV_RESEARCH_OS_PL.md` — wykonywalny cykl spec → run → decyzja → awans;
7. dokument subsystemu, którego dotykasz;
8. `docs/archive/` — tylko po historię i uzasadnienie dawnych decyzji.

Gdy dokumenty się różnią, wygrywa kod + świeży pomiar + powyższa hierarchia.
Dokument z archiwum nigdy nie jest instrukcją bieżącej pracy.

## 3. Twarde reguły pracy

- **Dowód przed poprawką.** Najpierw reprodukcja, telemetria albo minimalny test.
- **Jedna główna zmienna na eksperyment.** Zamroź geometrię, masę, tarcie,
  podkroki i warunki, których nie testujesz.
- **Stend nie jest pojazdem.** Wynik awansuje dopiero po transferze na wyższy
  poziom rigu.
- **Render jest bramką dla zmian wizualnych.** Zielony build nie dowodzi obrazu.
- **Werdykt o feelingu należy do Jozza.** Telemetria nie zastępuje jazdy.
- **Nie kasuj negatywnych wyników.** Surowy dowód jest produktem badawczym.
- **Nie stroisz progu, żeby test przeszedł.** Nieosiągalna bramka = STOP i raport.
- **Nie uruchamiaj GitHub Actions.** Lokalne bramki są wystarczające; limity CI
  są zasobem właściciela.
- **Bez szerokich destrukcyjnych komend Git.** Jawne ścieżki, małe commity,
  żadnego force-push, rewrite historii ani push do `main`.
- **Nie zakładaj ścieżek lokalnych Jozza.** Polecenia mają być literalne i
  oparte na sprawdzonym katalogu.

## 4. Granice odpowiedzialności

Rozdzielaj:

```text
authoring asset  -> glTF + sidecar
visual model     -> render i rig wizualny
physics rig      -> bodies, joints, parametry
contact model    -> geometria + manifold + solver
research evidence-> konfiguracja + surowy log + wersja kodu
```

Nie zamieniaj render mesha w kolizję tylko dlatego, że już istnieje. Nie dokładaj
wielu colliderów jako substytutu deformacji bez osobnego eksperymentu kosztu i
biasu liczby kontaktów.

## 5. Lokalna bramka jakości

Jedno wejście, profile będące uporządkowanymi supersetami:

```text
python tools/jv_gate.py quick  # normalny checkpoint
python tools/jv_gate.py deep   # dokumentacja, infrastruktura i regresje danych
python tools/jv_gate.py wheel  # dodatkowo lokalny Wheel Scope z gotowym buildem
python tools/jv_gate.py full   # pełna bramka produktu na Windows

python tools/jv_lab.py next    # najbliższy wykonywalny/odblokowywany eksperyment
python tools/jv_lab.py plan tools/research/experiments/WHEEL-SOFT-03.json
```

Przed checkpointem stage'uj kompletną propozycję. Bramka odmawia pracy, gdy
zostają konflikty, unstaged tracked files albo nieignorowane pliki untracked;
dzięki temu worktree i index opisują te same bajty.

`quick` sprawdza routing dokumentacji, higienę commitowanego drzewa, jawność
delty rdzenia, integralność evidence i whitespace worktree/index. `deep` dodaje
negatywne testy regresji narzędzi oraz małe, wznawialne shardy łańcucha
dowodowego. Bramka drukuje token `HEAD:index-tree`. Po przerwaniu wznowienie od
`--start-at N` wymaga podania tego samego `--proposal-token`; zmieniona propozycja
nie może ominąć wcześniejszych kontroli. `--stop-after N` wykonuje ograniczony
zakres i kończy komunikatem `PARTIAL OK`, nigdy fałszywym sukcesem całego profilu.
Pojedyncze „OK” składnika nie zastępuje pełnego profilu. `deep` uruchamia też
regresje JV Research OS, więc zmiana runnera, specyfikacji lub reguł awansu nie
może ominąć checkpointu.

Czystą paczkę źródłową twórz przez `python tools/export_source.py`; skrypt czyta
bajty wyłącznie z commitowanego drzewa `HEAD`, więc nie pakuje brudnego worktree,
`build/`, cache ani sesji.

Dla zmian shippingowego JV na Windows profil `full` uruchamia istniejący
`tools/gate.ps1`. Zawsze czytaj pełny log pod kątem ostrzeżeń, nawet gdy kod
wyjścia jest zerowy.

## 6. Dokumentowanie bez ponownego bałaganu

Pracuj jak przy oddaniu konkursowym: jedna paczka ma jawny cel, baseline, zakres,
zmienną główną, bramkę i artefakty końcowe. Zanim zaczniesz następną paczkę,
poprzednia musi mieć commit/patch, wynik testów oraz krótki checkpoint. Odkrycia
spoza zakresu trafiają do długu lub findings, nie do implementacji „przy okazji”.

- Realna zmiana stanu: wpis ≤5 linii w `docs/CHECKPOINTS_PL.md`.
- Zmiana otwartego ryzyka: aktualizacja `docs/TECH_DEBT_PL.md`.
- Zmiana bieżącego kierunku: aktualizacja `docs/CURRENT_STATE_INDEX_PL.md`.
- Nowy pełny dokument powstaje tylko wtedy, gdy ma trwałego właściciela treści,
  własny cykl życia i nie duplikuje istniejącego źródła.
- Raport zakończonego etapu trafia do `docs/archive/`, nie zostaje obok bieżących
  instrukcji.
- Po zmianie nazwy pola, hooka lub modelu uruchom grep po kodzie i dokumentacji
  w tym samym commicie.

## 7. STOP-gate

Zatrzymaj się i przedstaw Jozzowi dokładną decyzję, gdy:

- zmiana wpływa na feeling, domyślne zachowanie lub filozofię realistic/arcade;
- trzeba poluzować próg akceptacji;
- eksperyment nie izoluje badanej zmiennej;
- manualna jazda lub ocena obrazu jest jedyną prawdziwą akceptacją;
- plan wymaga przeniesienia mechanizmu JV do JES zamiast samej wiedzy i dowodu.

Nie zatrzymuj się z powodu problemu, który da się rozwiązać inną lokalną drogą.
Awaria jednego narzędzia nie jest powodem porzucenia zadania.

## 8. Czego nie robić „przy okazji”

- nie przebudowuj całego rigu zawieszenia podczas pracy nad manifoldem;
- nie zmieniaj geometrii kierownicy razem z kształtem opony;
- nie portuj JV do JES mechanicznie;
- nie przywracaj starych wielokształtnych opon jako kierunku tylko dlatego, że
  ich raporty są obszerne;
- nie traktuj komentarza w UI ani starego raportu jako dowodu fizycznego.
