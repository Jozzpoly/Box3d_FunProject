# JES_01 — Wizja i konstytucja

Warstwa: **MARZENIE / PRAWA ŚWIATA**. Wersja: 1.0-kandydat (2026-07-15;
destylat wizji v0.1 + poprawki Jozza + zasady 13–16 scalone z kandydatów
konstytucyjnych pakietu Sol CC-02/03/04/05/07/08). Zmiany tego dokumentu
wymagają jawnej decyzji Jozza. Pełne źródło marzenia (nieedytowane):
`WIZJA_JOZZ_ENGINEERING_SANDBOX_V0_1.md`.

Ten dokument celowo NIE zawiera technologii, bibliotek ani planu — od tego
są JES_02 (hipotezy wykonania) i JES_03 (program). Tu jest tylko to, co ma
przetrwać lata: tożsamość i prawa.

---

## 1. Marzenie

> Zbuduj dowolną maszynę, przygotuj dla niej świat i sprawdź, czy cały
> system rzeczywiście zadziała.

Otwarty sandbox inżynieryjny: gracz konstruuje maszyny lądowe, robocze
i latające, przekształca teren, buduje infrastrukturę, transportuje
materiały i tworzy automatyzację. Świat nie narzuca jednego rozwiązania —
każdą konstrukcję i operację ocenia wspólna fizyka, ograniczenia energii,
zachowanie materii i mierzalny rezultat pracy.

Samochody, koparki, spychacze, walce, samoloty, helikoptery, balony
i rakiety **nie są trybami gry** — są różnymi sposobami wykonywania pracy
w tym samym świecie. Głębia powstaje ze sprzężeń między systemami, nie
z liczby funkcji. Absurdalne konstrukcje (latająca koparka, rakietowy
spychacz) są częścią wizji: *możesz to zbudować — fizyka pokaże, czy
działa*.

Uzasadnieniem edytowalnego terenu jest **praca maszyn** (wykopy, nasypy,
drogi, pasy startowe, fundamenty) — nie dekoracyjna destrukcja.

## 1b. Rejestr dyrektyw właściciela (scalony, 2026-07-15)

Wpisy `OWNER_DIRECTIVE` są wiążące w swoim zakresie; `PARAFRAZA` = wierna
parafraza intencji (nie cytat). Interpretacje agentów NIE dziedziczą
autorytetu Jozza.

| ID | Dyrektywa (parafraza) | Status |
|---|---|---|
| OD-01 | Całkowicie nowy projekt i nowe repo; stare VAW/JV nie są produktem docelowym | OWNER_DIRECTIVE |
| OD-02 | Bez kopiowania kodu: głębokie studium VAW/JV → projekt i implementacja od zera | OWNER_DIRECTIVE |
| OD-03 | Dema zamrożone; poligon drobnych eksperymentów; dawcy pomysłów i „organów" | OWNER_DIRECTIVE |
| OD-04 | Stack: NIE Godot/Unity/Unreal (agenci źle pracują w tych środowiskach, Jozz ich nie zna); budować samemu z małych bibliotek open-source klasy box3d | OWNER_DIRECTIVE (2026-07-15, sesja Claude; d. „D1") |
| OD-05 | UI/nawigacja wzorem Blender + Unreal Engine; z Godota brać to, co najlepsze | OWNER_DIRECTIVE (d. „D9") |
| OD-06 | Lab-first: trudne systemy żyją najpierw w osobnych labach (wzór sampli box3d), składanie później, z ciągłą optymalizacją | OWNER_DIRECTIVE |
| OD-07 | Brak presji casualowego gameplayu; mała/symboliczna mapa na start; game-loop produktowy NA KOŃCU | OWNER_DIRECTIVE |
| OD-08 | Feel kieruje rozwojem; ręczny odbiór Jozza jest osobnym rodzajem dowodu | OWNER_DIRECTIVE |
| OD-09 | Własna grafika i efekty wcześnie — art loop jest zdolnością fundamentu | OWNER_DIRECTIVE |
| OD-10 | System gracza wynika z gameplayu, planowany „na żywo w chaosie"; na starcie brak/szczątkowy; marzenie: fizyczna interakcja (chwyt/obrót/przyczep) — osobny przyszły dokument | OWNER_DIRECTIVE |
| OD-11 | Droga nie musi być pamiętana ani „magicznie" cofalna (na razie; może się zmienić) | OWNER_DIRECTIVE |
| OD-12 | Workflow projektujemy później; na teraz tylko minimum bezpieczeństwa | OWNER_DIRECTIVE |
| OD-13 | System pracy odporny na nagłe zmiany, wielkie refactory i kilka kierunków naraz | OWNER_DIRECTIVE |
| OD-14 | Krytyczność ponad potwierdzanie: każda atrakcyjna idea ma mieć kontrargument i falsyfikację | PARAFRAZA |
| OD-15 | Finalna synteza pakietów agentowych przed nowym repo (wykonana: ten pakiet 1.0) | OWNER_DIRECTIVE (zrealizowana) |

## 2. Prawa świata — realizm kontraktowy

| Prawo | Znaczenie |
|---|---|
| Przyczynowość | Geometria, masa, moc, materiał i sposób połączenia wpływają na rezultat. |
| Bilans | Masa, objętość, paliwo, energia i ładunek nie znikają ani nie powstają bez jawnej operacji. |
| Ograniczenia | Silnik, siłownik, skrzydło, opona i narzędzie mają skończoną zdolność wykonywania pracy. |
| Modele zastępcze | Uproszczenia są dozwolone, jeżeli zachowują kluczowe konsekwencje i są diagnostycznie czytelne. |

Hierarchia prawdy: **semantyka** (części, połączenia, materiały, warstwy,
stan ładunku) → **mechanika** (ciała, jointy, kontakty, siły) → **cache**
(meshe renderu i kolizji, LOD). Zakaz odtwarzania znaczenia z mesha.

Wynik pracy musi być **mierzalny** (profil, zagęszczenie, bilans wykopu/
nasypu, zachowanie pojazdu testowego) — diagnostyka jest narzędziem gracza,
nie tylko dewelopera.

Wierność jest **stopniowana per domena** (grunt/opona/gąsienica/aero/
wirnik/destrukcja/ładunek mają poziom początkowy i możliwe rozszerzenia) —
kontrakt modelu pozostaje spójny przy zmianie wierności.

## 3. Konstytucja (zasady nienegocjowalne)

1. **Maszyna i świat są jednym systemem.** Żadnych odseparowanych minigier
   ani biernej scenografii.
2. **Semantyka jest ważniejsza od mesha.** Grafy konstrukcji, materiał
   i warstwy są źródłem prawdy; meshe są cachem.
3. **Materia, energia i ładunek mają bilans.** Uproszczenia tak, magiczne
   znikanie nie.
4. **Ograniczenia mają znaczenie.** Moc, siła, masa, geometria
   i przyczepność realnie ograniczają działanie.
5. **Wspólna przyczynowość, specjalistyczne modele.** Ani jeden solver
   wszystkiego, ani osobna gra dla każdej maszyny.
6. **Każdy etap udowadnia sprzężenie.** Funkcja jest wartościowa dopiero,
   gdy wpływa na istniejące systemy („dodaliśmy koparkę" ≠ „opór gruntu
   przechodzi przez łyżkę i hydraulikę, a materiał ma masę").
7. **Diagnostyka jest częścią gameplayu.** Gracz może zrozumieć i porównać
   rezultat.
8. **Absurdalne konstrukcje są dozwolone.** Ocenia je fizyka, nie lista
   dozwolonych klas.
9. **Wierność jest skalowana.** Modele upraszczamy świadomie, kontrakty
   zostają spójne.
10. **Zakres pierwszej wersji jest brutalnie ograniczony.** Ograniczamy
    domeny — nie usuwamy rdzenia wizji.
11. **Feel jest kryterium odbioru w każdym laboratorium.** *(poprawka
    2026-07-15, brzmienie po feedbacku Jozza)* Kompletny game-loop
    i produktowy gameplay przychodzą NA KOŃCU (decyzja Jozza) — ale każda
    mechanika przechodzi ręczny test odczucia Jozza w swoim labie, w chwili
    powstania (przyjemność nabrania pełnej łyżki jest kryterium na równi
    z bilansem masy). To rozróżnienie jest celowe: odkładamy SKŁADANIE gry,
    nie odkładamy CZUCIA mechanik.
12. **Stan symulacji zmieniają wyłącznie commity na granicy ticku,
    w deterministycznej kolejności.** *(przyjęte TYMCZASOWO jako default
    inżynierski, 2026-07-15 — Jozz nie zajął stanowiska; do obalenia
    dowodem, nie opinią; docelowy poziom determinizmu/replay = otwarte
    PDR)* Asynchroniczność służy przygotowaniu danych, nigdy ich
    aplikacji. Chroni replay, testowalność i multi-agentową diagnozę;
    nie kosztuje nic w gameplayu.
13. **Trwały dokument autorski jest niezależny od backendu fizyki,
    renderera i ECS; zapis i user art NIE są cache'em.** *(scalone z
    CC-02+CC-07 pakietu Sol)* Save Jozza przeżywa wymianę silnika;
    uchwyty runtime (`bodyId`, entity, node) nigdy nie trafiają do
    zapisu; utrata lub nadpisanie user art to incydent krytyczny
    z polityką backupu.
14. **Wygląd nie jest źródłem fizyki ani znaczenia.** *(CC-03; lekcja
    obu dem)* Manifest assetu opisuje wygląd, role wizualne i transform
    importu; masa, collidery, siły i sterowanie żyją w domenie. Sockety
    wizualne nie definiują frame'ów jointów.
15. **Każda trudna zdolność przechodzi lab → falsyfikację → promocję;
    kod labu jest z założenia usuwalny.** *(CC-04+CC-05)* Wiedza żyje
    w kontraktach, fixture'ach i dowodach — musi przeżyć wyrzucenie
    implementacji labu. Produkt/workbench nigdy nie importuje kodu
    labów (lekcja monolitu M6).
16. **Przyszłościowość = bezpieczna zmiana, nie przewidzenie
    wszystkiego.** *(CC-08)* Wersjonowane kontrakty, wymienne backendy
    za granicą adaptera, exit strategy dla zależności; wspólna
    abstrakcja dopiero przy DRUGIM realnym konsumencie albo twardym
    invariancie.

## 4. Kierunki architektonicznie odrzucone

- wszystko jako jeden typ blokowego voxela; wszystko jako SDF;
- voxel = collider; urządzenia mechaniczne spłaszczone do materiału terenu;
- droga jako wyłącznie tekstura/mesh/typ materiału;
- jeden uniwersalny solver dla gruntu, opon, skrzydeł i hydrauliki;
- klasy `Car`/`Plane`/`Excavator` jako fundament (presety — tak, fundament — nie);
- pełna destrukcja jako warunek pierwszej gry;
- wielki proceduralny świat przed udowodnieniem lokalnego ekosystemu;
- mechaniczne sklejenie repozytoriów VAW/JozzVehicle bez nowej konstytucji.

## 5. System gracza — stanowisko (decyzja Jozza, 2026-07-15)

System gracza i jego mechanika mają **wynikać z gameplayu** i będą
projektowane „na żywo, w chaosie" — przez początkowy etap produkcji może
go nie być wcale albo w formie szczątkowej. Docelowe marzenie: gracz
fizycznie reagujący ze światem — podnoszenie (jak w demach box3d), ale też
obracanie, przybliżanie/oddalanie trzymanego obiektu, łapanie się,
przyczepianie. To temat na **osobny przyszły dokument**.

Konsekwencja architektoniczna (minimalna, przyjęta by uniknąć przyszłego
rewrite'u): od pierwszego dnia istnieje jawny, nazwany **model operatora
v0** = swobodna kamera konstruktora + „wsiąście" w maszynę (przejęcie
sterowania). To JEST system gracza w wersji zerowej — szczątkowy, zgodnie
z decyzją — a warstwa interakcji fizycznej (chwyt/obrót/przyczep) dostaje
zarezerwowane miejsce w API świata (uchwyt interakcji), nawet jeśli latami
będzie pusta. Rezerwacja slotu jest tania; doklejanie gracza do powłoki,
która nigdy go nie przewidziała, było błędem klasy „host sampli" (JES_02
§8, lekcja L1).

## 6. Drogi — stanowisko (decyzja Jozza, 2026-07-15)

Po wykonaniu drogi gra **nie musi** pamiętać jej jako edytowalnego,
parametrycznego obiektu ani pozwalać „magicznie" ją cofnąć — przynajmniej
na razie (Jozz dopuszcza zmianę zdania). Przyjęte stanowisko v1:

- świat zmieniają **wyłącznie maszyny** (żaden graf drogi nie edytuje
  terenu autorytatywnie);
- „droga" = fizyczny rezultat pracy (nasyp, profil, warstwy, zagęszczenie);
- projekt drogi istnieje jako **wytyczenie i pomiar** (tyczki/korytarz
  pomiarowy + porównanie stanu faktycznego z projektem) — czyli
  diagnostyka z zasady 7, nie magia;
- autorytatywne „zleć wykonanie drogi" może wrócić w przyszłości jako
  AUTOMATYZACJA (maszyny wykonują projekt), nie jako edycja świata.

## 7. Czego NIE wymagamy na starcie

Pełna topologia 3D terenu (jaskinie/nawisy); ciągła destrukcja; pełna
hydraulika płynowa; ziarna materiału sypkiego; fizyczne ogniwa gąsienic;
CFD i model łopat wirnika; mechanika orbitalna i planety; multiplayer;
ekonomia/survival; pełna wierność symulacji całego świata; **kompletny
game-loop produktowy** (decyzja Jozza: na końcu); **system gracza** (§5).
