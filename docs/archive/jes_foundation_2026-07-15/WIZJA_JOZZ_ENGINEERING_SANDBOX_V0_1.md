> **ARCHIWUM — nie jest bieżącą instrukcją.** Plik zachowano jako historię decyzji i materiał dziedzictwa. Aktualny start: [`docs/JV_DOCS_INDEX_PL.md`](../../JV_DOCS_INDEX_PL.md).

# JOZZ ENGINEERING SANDBOX


## Dokument wizji, wspólnych zasad i krytycznych wniosków


**Wersja:** 0.1  
**Data:** 15 lipca 2026  
**Status:** dokument założycielski do dalszej krytycznej analizy; nie jest roadmapem ani specyfikacją implementacyjną.


> Zbuduj dowolną maszynę, przygotuj dla niej świat i sprawdź, czy cały system rzeczywiście zadziała.


Synteza koncepcji powstałej z Voxel Aeronautics Workshop, Vehicle Lab, edytowalnego świata oraz maszyn wykonujących realną pracę

> Zbuduj dowolną maszynę, przygotuj dla niej świat i sprawdź, czy cały system rzeczywiście zadziała.

Wersja 0.1 — dokument założycielski do dalszej analizy

Autor wizji: Przemek „Jozz”  |  Synteza i krytyczna redakcja: GPT‑5.6 Thinking

Data: 15 lipca 2026

> STATUS DOKUMENTU To nie jest roadmap, specyfikacja implementacyjna ani obietnica ukończenia wszystkich opisanych systemów. Dokument definiuje wizję produktu, wspólne prawa świata, hipotezy architektoniczne, ryzyka i pytania, które muszą zostać poddane dalszej analizie przed rozpoczęciem nowego projektu.


# 0. Jak czytać ten dokument


Dokument zbiera najważniejsze wnioski wypracowane podczas rozmowy o połączeniu Voxel Aeronautics Workshop, eksperymentalnego Vehicle Lab, destruktywnego terenu, robót ziemnych, infrastruktury, lotnictwa, logistyki oraz automatyzacji. Największy nacisk położono na końcową, uzgodnioną wizję gry — nie na techniczne szczegóły pierwszych rozważań.

Jego zadaniem jest zachowanie tożsamości marzenia przed kolejną fazą analizy. Następny agent powinien krytycznie podważyć założenia, zweryfikować architekturę i dopiero potem zaproponować wykonalny program eksperymentów oraz plan projektu.

> Najważniejsze rozróżnienie Wizja docelowa określa kierunek i prawa świata. Pierwszy wykonalny produkt ma udowodnić tylko najważniejsze sprzężenia systemowe. Nie wolno oceniać wizji przez pryzmat małego prototypu ani projektować prototypu tak, jakby od razu musiał zawierać pełną wizję.


## 0.1. Zakres dokumentu


- Definiuje: tożsamość gry, nadrzędną fantazję gracza, wspólne prawa i filary systemowe.

- Porządkuje: relacje między pojazdami, maszynami roboczymi, lotnictwem, terenem, infrastrukturą, logistyką i automatyzacją.

- Rozdziela: wspólny rdzeń symulacji od wyspecjalizowanych modeli domenowych.

- Identyfikuje: największe ryzyka techniczne, produktowe i organizacyjne.

- Utrwala: decyzje robocze, hipotezy, odrzucone kierunki i pytania otwarte.

- Przygotowuje: materiał wejściowy do krytycznej analizy GPT‑5.6 Sol na wysokim poziomie rozumowania.


## 0.2. Czego dokument nie robi


- Nie wybiera ostatecznego silnika, języka, biblioteki voxelowej ani modelu danych.

- Nie ustala harmonogramu, kosztu ani wielkości zespołu.

- Nie obiecuje pełnej geotechniki, CFD, orbital mechanics, multiplayera ani ciągłej destrukcji.

- Nie traktuje wszystkich pomysłów jako równorzędnych wymagań pierwszej wersji.

- Nie zastępuje eksperymentów i benchmarków realnym kodem.


## 0.3. Spis głównych części


1. Rdzeń marzenia i definicja produktu.

2. Wspólny ekosystem zależności mechanik.

3. Prawa świata i realizm kontraktowy.

4. Architektura wspólnego rdzenia i modeli specjalistycznych.

5. Materia, teren, roboty ziemne i infrastruktura.

6. Maszyny lądowe, latające i absurdalne konstrukcje.

7. Skala przestrzeni, czasu i aktywnej symulacji.

8. Krytyczne ryzyka, granice oraz decyzje odłożone.

9. Pytania i zadania dla następnej głębokiej analizy.


# CZĘŚĆ I. TOŻSAMOŚĆ WYMARZONEJ GRY



## 1. Ostateczna definicja wizji


> Otwarty sandbox inżynieryjny, w którym gracz konstruuje maszyny lądowe, robocze i latające, przekształca teren, buduje infrastrukturę, transportuje materiały oraz tworzy systemy automatyzacji. Świat nie narzuca jednego rozwiązania — każda konstrukcja i każda operacja są oceniane przez wspólną fizykę, ograniczenia energii, zachowanie materii i mierzalny rezultat pracy.

Ta definicja jest ważniejsza niż lista funkcji. Samochody, koparki, spychacze, samoloty, balony, helikoptery i rakiety nie są osobnymi trybami gry. Są różnymi sposobami wykonywania pracy w tym samym świecie.

Projekt nie powinien być opisywany jako „VAW z samochodami w voxelowym świecie”. Taki opis zbyt mocno eksponuje technologię reprezentacji i sugeruje mechaniczne sklejenie istniejących projektów. Właściwszy opis to: świat i maszyna tworzą jeden system inżynieryjny.


## 1.1. Nadrzędna obietnica dla gracza


> Zbuduj dowolną maszynę, przygotuj dla niej świat i sprawdź, czy cały system rzeczywiście zadziała.

Obietnica obejmuje trzy równorzędne obszary odpowiedzialności gracza:

- Maszyna: jej geometria, masa, układ napędowy, konstrukcja, narzędzia, sterowanie i ograniczenia.

- Świat: teren, nawierzchnia, droga, most, pas startowy, rampa, zaplecze i dostępne materiały.

- Wykonanie: faktyczny proces kopania, transportu, rozkładania, zagęszczania, montażu, testu i poprawy.

Gra nie powinna gwarantować sukcesu tylko dlatego, że gracz wybrał prawidłowy typ obiektu. Powinna pozwolić zbudować coś absurdalnego, a następnie uczciwie pokazać, dlaczego działa albo dlaczego się rozpada.


## 1.2. Maszyna jako uniwersalny sposób wykonywania pracy


| Kategoria | Podstawowa praca | Przykładowe konsekwencje systemowe |

| --- | --- | --- |

| Samochód / ciężarówka | Transportuje ludzi, ładunek i energię po powierzchni. | Wymusza drogi, mosty, przyczepność, zawieszenie, nośność podłoża i logistykę. |

| Koparka | Odspaja, nabiera, przenosi i odkłada materiał. | Łączy hydraulikę, geometrię narzędzia, opór gruntu, masę ładunku i aktualizację terenu. |

| Spychacz / równiarka | Przemieszcza, rozprowadza i profiluje materiał. | Wymusza zachowanie objętości, przepływ gruntu, trakcję, profil docelowy i pomiar jakości. |

| Walec / ubijarka | Zmienia stan i nośność ułożonego materiału. | Łączy nacisk, liczbę przejazdów, wilgotność, zagęszczenie i późniejsze zachowanie pojazdów. |

| Samolot | Pokonuje odległość i przenosi ładunek w atmosferze. | Wymusza aerodynamikę, bilans masy, energię, przygotowanie pasa i logistykę lotniczą. |

| Helikopter / wirnikowiec | Unosi i precyzyjnie przenosi ładunek bez drogi. | Wymusza model wirnika, moc, moment reakcyjny, sterowanie i lokalne warunki powietrza. |

| Balon / sterowiec | Zapewnia wypór i tani transport powietrzny. | Wymusza objętość, gaz, wiatr, balast i zarządzanie masą. |

| Rakieta | Generuje ekstremalny ciąg i osiąga trudno dostępne obszary. | Wymusza paliwo, zbiorniki, konstrukcję, stabilność, atmosferę i decyzję o skali świata. |


## 1.3. Wspólny mianownik VAW, Vehicle Lab i nowego świata


> konstrukcja + energia + sterowanie + generatory sił + narzędzia i ładunek + interakcja ze środowiskiem + mierzalny rezultat

Wspólnym mianownikiem nie jest typ pojazdu ani voxelowa grafika. Jest nim maszyna jako semantyczna konstrukcja, która przetwarza energię i działa w świecie przez siły, kontakty, narzędzia oraz przepływ materiałów.


## 1.4. Dlaczego edytowalny świat ma prawdziwe uzasadnienie


Edytowalny teren nie powinien być atrakcją do kopania ani dekoracyjną destrukcją. Jego uzasadnieniem jest fizyczna praca maszyn:

- karczowanie lasu i przygotowanie placu;

- wykopy, nasypy, rowy, rampy, skarpy i fundamenty;

- wydobywanie, ładowanie, transport i wysypywanie materiału;

- budowa warstw drogi, profilowanie i zagęszczanie;

- przygotowanie pasa startowego, platformy rakietowej lub lądowiska;

- tworzenie infrastruktury umożliwiającej następną generację maszyn.

> Wniosek Voxele lub inne struktury przestrzenne są uzasadnione tylko jako reprezentacja materii podlegającej pracy. Nie mogą stać się celem samym w sobie ani narzucić blokowej postaci wszystkim elementom świata.


# CZĘŚĆ II. EKOSYSTEM ZALEŻNOŚCI I GAMEPLAY



## 2. Głębia powstaje ze sprzężeń, nie z liczby funkcji


Lista pojazdów i systemów nie tworzy głębokiej gry. Głębia pojawia się wtedy, gdy decyzja w jednym systemie zmienia warunki w kilku kolejnych, a gracz może odpowiedzieć inną maszyną, lepszą infrastrukturą albo automatyzacją.


## 2.1. Przykładowy łańcuch zależności: ciężki transport


> większy ładunek   → cięższy pojazd   → większy nacisk na grunt   → koleiny i większy opór toczenia   → potrzeba lepszej podbudowy   → potrzeba kruszywa i zagęszczania   → koparka + wywrotka + równiarka + walec   → paliwo, serwis i logistyka


## 2.2. Przykładowy łańcuch zależności: transport lotniczy


> większy samolot transportowy   → większa masa i prędkość startowa   → dłuższy i równiejszy pas   → karczowanie lasu i roboty ziemne   → podbudowa, asfalt i zagęszczanie   → dostawy materiałów i maszyn   → infrastruktura paliwowa i serwis


## 2.3. Przykładowy łańcuch zależności: rakieta


> rakieta   → zbiorniki, paliwo i silnik   → duża masa konstrukcji   → transport elementów   → dźwigi i specjalna droga   → stabilna platforma startowa   → odporność na drgania i obciążenia   → systemy sterowania i bezpieczeństwa


## 2.4. Naturalna progresja systemowa


Postęp nie musi opierać się na arbitralnym odblokowywaniu technologii. Może wynikać z realnych możliwości produkcyjnych i infrastrukturalnych:

> proste narzędzia   → pierwsza maszyna   → wydajniejsze pozyskanie materiału   → większe konstrukcje   → lepsza infrastruktura   → dalszy transport i eksploracja   → nowe problemy inżynieryjne   → automatyzacja i skala

Samolot nie musi zostać „odblokowany na poziomie 20”. Staje się praktyczny, kiedy gracz potrafi wytworzyć odpowiedni napęd, zbudować lekką konstrukcję, przygotować pas i potrzebuje transportu na większą odległość.


## 2.5. Rozwiązania emergentne zamiast klas misji


Problem „dostarcz generator na drugą stronę gór” może zostać rozwiązany przez:

- ciężarówkę i nową drogę;

- pojazd terenowy;

- most lub tunel;

- kolej linową;

- dźwig pod sterowcem;

- helikopter transportowy;

- samolot i przygotowany pas;

- absurdalną konstrukcję rakietową;

- maszynę kroczącą lub autonomiczny konwój.

System nie powinien rozpoznawać „poprawnego rozwiązania”. Powinien rozpoznawać masę, siły, energię, geometrię, nośność, materiał i rezultat.


## 2.6. Absurdalne konstrukcje są częścią wizji


Latające koparki, rakietowe spychacze, ciężarówki z wirnikami, balonowe dźwigi i wieloczłonowe maszyny nie są błędem projektowym. Są sprawdzianem, czy fundament naprawdę opisuje części i prawa, zamiast zamkniętych klas pojazdów.

> Możesz to zbudować. Fizyka pokaże, czy to działa.


# CZĘŚĆ III. PRAWA ŚWIATA I REALIZM KONTRAKTOWY



## 3. Uczciwa fizyka nie oznacza symulowania wszystkiego


Pełna symulacja ziaren piasku, przepływu powietrza wokół każdej śruby, szczegółowej geotechniki i każdego ogniwa gąsienicy jest niewykonalna jako wspólny fundament. Gra potrzebuje modeli zastępczych, ale nie może arbitralnie łamać najważniejszych zależności przyczynowych.


## 3.1. Cztery zasady realizmu kontraktowego


| Zasada | Znaczenie |

| --- | --- |

| Przyczynowość | Geometria, masa, położenie, moc, materiał i sposób połączenia muszą wpływać na rezultat. |

| Bilans | Masa, objętość, paliwo, energia i ładunek nie mogą magicznie znikać ani pojawiać się bez jawnej operacji. |

| Ograniczenia | Silnik, siłownik, skrzydło, opona, gąsienica i narzędzie mają skończoną zdolność generowania siły i wykonywania pracy. |

| Modele zastępcze | Dopuszczalne są uproszczenia zbiorcze, jeżeli zachowują kluczowe konsekwencje i są diagnostycznie czytelne. |


## 3.2. Hierarchia prawdy


- Źródło prawdy semantycznej: części, połączenia, materiały, warstwy, przebieg drogi, stan ładunku i operacje świata.

- Źródło prawdy mechanicznej: masa, bezwładność, ciała, jointy, kontakty, siły i momenty.

- Cache wynikowy: render mesh, collision mesh, LOD, instancing, uproszczone collidery i dane diagnostyczne.

- Zakaz: odtwarzania znaczenia drogi, maszyny albo materiału z gotowego triangle mesha.


## 3.3. Materiał powinien być zachowywany i przenoszony


Wydobyta ziemia nie może tylko znikać z terenu. Powinna przejść do łyżki, skrzyni, przenośnika albo bufora narzędzia, zwiększyć masę maszyny, zostać przewieziona i ponownie zdeponowana. System może agregować materiał, ale musi zachować co najmniej:

- rodzaj materiału;

- masę;

- objętość;

- przybliżony stopień zagęszczenia;

- aktualnego właściciela lub miejsce składowania.


## 3.4. Wynik pracy musi być mierzalny


Droga nie jest ukończona dlatego, że została pomalowana na czarno. Można porównać jej stan z projektem:

- błąd wysokości i profilu podłużnego;

- spadek poprzeczny;

- falistość i zmienność normalnych;

- grubość warstw;

- zagęszczenie i nośność;

- objętość wykopu i nasypu;

- zachowanie pojazdu testowego.

Diagnostyka nie jest dodatkiem dla dewelopera. Jest podstawowym narzędziem gracza do zrozumienia, dlaczego system działa lub zawodzi.


## 3.5. Stopnie wierności zamiast jednego poziomu symulacji


| Domena | Poziom początkowy | Możliwy poziom późniejszy |

| --- | --- | --- |

| Grunt | Objętość, typ, zagęszczenie, opór i nośność. | Wilgotność, osiadanie, przepływ, stabilność skarp i procesy długoterminowe. |

| Opona | Fizyczne koło + ograniczony model sił kontaktowych. | Rozszerzony model slipu, deformacji, temperatury i wieloskalowej nawierzchni. |

| Gąsienica | Ciągły obszar kontaktu i rozkład nacisku. | Ogniwa fizyczne lub bardziej szczegółowy model pasa. |

| Samolot | Powierzchnie aerodynamiczne i lokalny przepływ. | Interakcje powierzchni, indukcja, bardziej złożony stall i uszkodzenia. |

| Helikopter | Model dysku wirnika. | Model łopat, autorotacja, zjawiska asymetryczne i zaawansowana aerodynamika. |

| Destrukcja | Odłączanie komponentów i uproszczone fragmenty. | Naprężenia, pęknięcia, dalsza fragmentacja i zmęczenie. |

| Ładunek sypki | Agregat masy, objętości i środka ciężkości. | Powierzchnia swobodna, przepływ, mieszanie i lokalna segregacja. |


# CZĘŚĆ IV. WSPÓLNY RDZEŃ I MODELE SPECJALISTYCZNE



## 4. Nie należy budować klas Car, Plane i Excavator jako fundamentu


Szablony pojazdów mogą istnieć dla wygody, ale nie mogą definiować architektury. Fundament powinien składać się z komponentów i kontraktów, które można łączyć w nieprzewidziane konstrukcje.

> RigidPart      Joint          Wheel TrackContact   AeroSurface    Rotor Thruster       Engine         Gearbox Pump           Valve          Actuator Tool           Container      Sensor Controller     EnergyStore    ResourcePort

> Zasada konstrukcyjna Klasy wysokiego poziomu mogą być presetami, analizatorami lub interfejsami budowania. Nie mogą ograniczać tego, jakie kombinacje części są fizycznie dozwolone.


## 4.1. CraftGraph jako semantyczny rdzeń maszyny


CraftGraph powinien opisywać znaczenie konstrukcji niezależnie od renderera i fizyki. Minimalnie obejmuje:

- części strukturalne i ich właściwości masowe;

- hardpointy, sockety i połączenia;

- jointy oraz ograniczenia ruchu;

- urządzenia generujące lub przetwarzające energię;

- aktuatory, powierzchnie siłowe i narzędzia;

- magazyny paliwa, energii, gazu i materiału;

- sensory, kontrolery, sygnały i sterowanie;

- stan uszkodzeń, temperatury i zużycia — w zakresie przyjętym dla etapu.


## 4.2. Wspólny kontrakt wszystkich maszyn


> konstrukcja   + energia i zasoby   + sterowanie   + generatory sił i momentów   + narzędzia lub ładunek   + interakcja ze środowiskiem   = zachowanie maszyny

Samochód, koparka i samolot nie potrzebują identycznego algorytmu sił. Potrzebują wspólnych jednostek, przestrzeni, czasu, masy, energii, sygnałów, diagnostyki i kontraktu przekazywania sił do mechanicznego integratora.


## 4.3. Box3D jako integrator mechanicznego rezultatu


Box3D jest kandydatem na rdzeń ruchu ciał sztywnych, kontaktów, jointów, impulsów i integracji mechanicznej. Nie powinien jednak definiować, czym jest droga, grunt, skrzydło, hydraulika albo materiał w łyżce.

> TireSystem ───────────────┐ TrackSystem ──────────────┤ AerodynamicsSystem ───────┤ RotorSystem ──────────────┤ HydraulicsSystem ─────────┤ TerrainMaterialSystem ────┼──→ siły / momenty / ograniczenia → integrator rigid-body BuoyancySystem ───────────┤ PropulsionSystem ─────────┤ ControlSystem ────────────┘


## 4.4. Sześć głównych filarów


| Filar | Odpowiedzialność |

| --- | --- |

| Craft System | Budowanie, semantyka części, połączenia, masa, uszkodzenia i konfiguracja maszyny. |

| World Matter System | Teren lity, materiał luźny, warstwy powierzchniowe, roślinność i operacje materiałowe. |

| Physics & Force Systems | Rigid bodies oraz modele opon, gąsienic, aerodynamiki, wirników, wyporu, narzędzi i kontaktu z gruntem. |

| Energy & Resource Networks | Paliwo, elektryczność, hydraulika, gazy, przepływy, magazyny i ograniczenia mocy. |

| Control & Automation | Sterowanie ręczne, sensory, logika, autopiloty, sekwencje i autonomiczne maszyny. |

| Diagnostics & Experimentation | Replay, wykresy, pomiary, porównania, debug overlays, scenariusze i testy regresji. |


## 4.5. System automatyzacji jako spoiwo skali


Przy większych inwestycjach gracz nie może ręcznie prowadzić każdej wywrotki i każdego walca. Automatyzacja nie jest opcjonalnym dodatkiem końcowym. Jest mechanizmem umożliwiającym przejście od pojedynczej maszyny do ekosystemu logistycznego.

> sensor wysokości   → kontroler   → zawór hydrauliczny   → lemiesz równiarki   → utrzymanie profilu drogi

> waypoint / sensor   → autopilot   → przepustnica + powierzchnie sterowe   → autonomiczny lot transportowy


# CZĘŚĆ V. MATERIA, TEREN I ROBOTY ZIEMNE



## 5. Teren jest materiałem roboczym, nie dekoracją


Najważniejszy problem technologiczny nie brzmi „jak renderować voxele”, lecz: jak przechowywać i aktualizować materiał, który reaguje na narzędzia, zachowuje masę oraz produkuje stabilną geometrię dla fizyki i renderingu.


## 5.1. Wieloreprezentacyjny świat


| Domena | Źródło prawdy | Przeznaczenie |

| --- | --- | --- |

| Solid Terrain | Pole gęstości, kolumny lub inna objętościowa reprezentacja. | Grunt skonsolidowany, skała, wykopy, skarpy i podłoże. |

| Loose Material | Pole objętości/masy, warstwa lub kolumny materiału luźnego. | Ziemia po wykopaniu, piasek, żwir, kruszywo, nasypy i zwały. |

| Surface Layers | Warstwy o grubości i stanie technologicznym. | Asfalt, podbudowa, błoto, śnieg i parametry kontaktu. |

| Structural World | Bloki, belki, płyty, graf konstrukcyjny i zakotwienia. | Mosty, platformy, warsztaty, mury i konstrukcje nośne. |

| Road / Alignment Graph | Parametryczny przebieg, profil, szerokość, przekrój i typ konstrukcji. | Projekt drogi, pasa, rampy, wykopu, nasypu i infrastruktury liniowej. |

| CraftGraph | Semantyczne części i systemy maszyny. | Pojazdy, narzędzia, urządzenia, ładunki i automatyka. |


## 5.2. Voxel nie oznacza widocznego sześcianu


„Voxel” powinien oznaczać dane przestrzenne, nie obowiązkową blokową geometrię renderowaną. Twarda konstrukcja może być dyskretna, naturalny teren gładki, droga parametryczna, a maszyna ciągła geometrycznie i semantycznie opisana.

> Zakaz architektoniczny Nie wolno voxelizować urządzeń mechanicznych ani sprowadzać drogi do materiału „asfalt”. Koło, silnik, siłownik, skrzydło i droga zachowują własną semantykę.


## 5.3. Początkowy model robót ziemnych może być 2.5D


Fantazja koparek, spychaczy, równiarek, wywrotek i walców nie wymaga od pierwszego dnia pełnych tuneli, jaskiń i wiszących mas ziemi. Otwarta powierzchnia robót może być reprezentowana przez kolumny zawierające lity substrat, objętość luźnego materiału i warstwy powierzchniowe.

> TerrainColumn {     solidHeight;     looseVolume;     solidMaterial;     looseMaterial;     compaction;     moisture; }

Model 2.5D jest ograniczeniem świadomym, nie ostateczną definicją świata. Może udowodnić najważniejszą pętlę: odspojenie → transport → rozłożenie → profilowanie → zagęszczenie → test pojazdem.


## 5.4. Operacje materiałowe zamiast bezpośredniej edycji komórek


> MaterialOperation {     operationType;     toolGeometry;     relativeVelocity;     appliedForce;     availablePower;     duration;     sourceMachine; }

Gameplay nie powinien wywoływać dowolnego „removeSphere”. Narzędzie inicjuje operację, a domena materiałowa interpretuje ją według rodzaju gruntu, geometrii krawędzi, kierunku ruchu, siły, prędkości, wilgotności i zagęszczenia.


## 5.5. Kontrakt narzędzie–materiał–maszyna


| Narzędzie | Semantyka robocza | Wymagany feedback do maszyny |

| --- | --- | --- |

| Łyżka | Krawędź tnąca, zęby, pojemność, obszar przechowywania i kąt wysypu. | Opór cięcia, masa nabranego materiału, moment na ramieniu, możliwość wysypu. |

| Lemiesz | Szerokość, wysokość, krzywizna, kąt i możliwość kierowania materiału na bok. | Siła oporu zależna od pryzmy materiału, poślizg i ograniczenie przez trakcję. |

| Zrywak | Zęby, głębokość i zdolność odspajania twardego gruntu. | Silny opór, przeciążenie konstrukcji, ograniczenie mocą i przyczepnością. |

| Walec | Szerokość, średnica, masa, nacisk, wibracja i prędkość przejazdu. | Zmiana zagęszczenia, osiadanie powierzchni i reakcja podłoża. |

| Rozkładarka | Pojemność, przepływ materiału, stół profilujący i grubość warstwy. | Zmiana masy, opór technologiczny i jakość ułożonej powierzchni. |


## 5.6. Koparka jako test integracyjny fundamentu


1. Łyżka wchodzi w grunt; model materiału oblicza opór.

2. Opór wraca przez ramię, jointy i aktuator do całej maszyny.

3. Układ napędowy lub hydrauliczny ogranicza dostępne siły i prędkości.

4. Materiał przechodzi ze świata do agregatu ładunku w łyżce.

5. Masa, środek ciężkości i stabilność maszyny ulegają zmianie.

6. Materiał zostaje przeniesiony i zdeponowany w nowym miejscu.

7. Powierzchnia relaksuje się zgodnie z uproszczonym zachowaniem materiału.

8. Collider zostaje bezpiecznie zaktualizowany na granicy kroku fizyki.

Jeżeli prototyp nie potrafi zachować tego łańcucha przyczynowego, nie udowadnia wizji maszyn wykonujących realną pracę.


## 5.7. Spychacz i równiarka jako odrębne testy


- Spychacz: testuje przepływ materiału przed lemieszem, ucieczkę bokami, opór zależny od objętości oraz ograniczenie przez trakcję.

- Równiarka: testuje celowy profil, drobną korektę wysokości, spadek poprzeczny, pomiar jakości i sterowanie narzędziem.

- Walec: testuje zmianę stanu materiału w czasie, malejący efekt kolejnych przejazdów i wpływ jakości podłoża na późniejszy ruch pojazdów.


## 5.8. Asfalt jako proces technologiczny


Asfalt nie powinien być typem voxela. Jest warstwą o grubości, temperaturze, gęstości, zagęszczeniu, równości i połączeniu z podbudową. Minimalny proces może obejmować przygotowanie podłoża, rozłożenie warstwy, profilowanie, zagęszczenie i schłodzenie.

> Granica zakresu Pełna technologia drogowa nie jest warunkiem pierwszego prototypu. Ważne jest udowodnienie, że stan warstwy wpływa na fizykę pojazdu i może zostać zmieniony przez maszynę.


## 5.9. Bilans cut/fill jako mechanika


System powinien móc policzyć objętość wykopu, nasypu, materiał przewieziony, brakujący i nadmiarowy. Umożliwia to zadania, w których gracz optymalizuje trasę, wykorzystuje ziemię z wykopu do nasypu oraz projektuje logistykę kruszywa.


# CZĘŚĆ VI. DROGI, INFRASTRUKTURA I ŚWIAT



## 6. Droga pozostaje obiektem semantycznym


RoadGraph powinien przechowywać przebieg, profil podłużny, przekrój, szerokość, przechyłkę, pobocza, warstwy i połączenia. Deformacja terenu, render mesh, collider i mapa właściwości nawierzchni są wynikami, które można przebudować.


## 6.1. Dwa typy drogi


| Typ | Charakter | Konsekwencje |

| --- | --- | --- |

| Droga terenowa | Wykop, nasyp i warstwy spoczywające na gruncie. | Podkopanie i brak nośności powinny wpływać na stan drogi. |

| Droga konstrukcyjna | Most, estakada, pomost, stalowy deck lub platforma. | Nośność wynika z konstrukcji, podpór i połączeń, a nie z samego terenu. |


## 6.2. Fundamentalny problem edycji proceduralnej


Po wygenerowaniu drogi gracz może ręcznie zmienić teren. Należy rozstrzygnąć, czy droga stale narzuca swój kształt, staje się jednorazowym stemplem, czy pozostaje warstwą operacji. Każdy model ma koszt:

| Model | Zaleta | Ryzyko |

| --- | --- | --- |

| Autorytatywna droga | Łatwa późniejsza edycja projektu. | Kasuje ręczne zmiany i może wyglądać magicznie. |

| Jednorazowy stempel | Prosty stan świata po wykonaniu. | Traci semantykę i odwracalność projektu. |

| Warstwy operacji | Największa elastyczność i historia zmian. | Największa złożoność zapisu, undo, konfliktów i wydajności. |


## 6.3. Wieloskalowa nawierzchnia


| Skala | Reprezentacja | Przykłady |

| --- | --- | --- |

| Makro | Rzeczywisty collider. | Zbocza, rowy, krawężniki, duże dziury, głazy i uskoki. |

| Mezo | Profil powierzchni i model kontaktowy. | Koleiny, tarki, drobne nierówności, żwir i falistość. |

| Mikro | Właściwości materiałowe. | Tarcie, mokrość, opór toczenia, ścieranie i chropowatość. |

Jedna trójkątna geometria nie powinna reprezentować każdej skali nierówności. Collider ma zawierać cechy istotne geometrycznie, a mniejsze skale mogą wpływać na model kontaktu opony lub gąsienicy.


## 6.4. Granica block–smooth–road


Najtrudniejsze błędy mogą pojawić się nie wewnątrz poszczególnych systemów, lecz na ich styku. W jednym miejscu może istnieć blokowa konstrukcja, gładki grunt, warstwa asfaltu i część maszyny. System potrzebuje jednoznacznej własności powierzchni fizycznej.

> przykładowy priorytet własności powierzchni: construction > road surface > smooth terrain > empty

Nie może istnieć kilka nachodzących colliderów twierdzących, że są tą samą powierzchnią. Każdy punkt kontaktu powinien mieć jednego fizycznego właściciela, nawet jeżeli dane źródłowe pochodzą z kilku domen.


## 6.5. Karczowanie lasu jako systemowa operacja


Drzewa lepiej traktować jako semantyczne obiekty z pniem, masą i zakotwieniem korzeniowym niż jako zwykłe voxele. Maszyna może je ściąć, wyrwać, przewrócić, przetransportować i pozostawić zagłębienie wymagające zasypania. Nie wymaga to symulowania każdego liścia.


# CZĘŚĆ VII. POJAZDY LĄDOWE I MASZYNY ROBOCZE



## 7. Fizyczne pojazdy muszą pozostać uczciwe wobec świata


Pojazdy nie powinny być odseparowaną minigrą. Masa ładunku, stan drogi, zagęszczenie gruntu, geometria zawieszenia, model opony i dostępna moc mają wspólnie decydować o zachowaniu.


## 7.1. Koło i teren


Blokowy collider może być celowo szorstki dla stalowych schodów lub skał, ale nie dla asfaltu i profilowanej drogi. Możliwe rozwiązanie docelowe łączy:

- fizyczną geometrię makro;

- specjalistyczny model sił opony;

- profil mezoskopowy nawierzchni;

- mikroskopowe właściwości materiału.

Nie należy automatycznie zastępować fizycznych kół raycastami. Vehicle Lab ma wartość właśnie dlatego, że koła i zawieszenie są częścią rzeczywistej konstrukcji mechanicznej. Uproszczenia powinny zachować tę prawdę.


## 7.2. Gąsienice


Pełna gąsienica z setek ogniw może być niepotrzebnie kosztowna. Dla ciężkich maszyn bardziej użyteczny może być ciągły model pasa kontaktowego, który oblicza rozkład nacisku, trakcję, poślizg i zapadanie na miękkim gruncie.


## 7.3. Hydraulika i aktuatory


Koparki i spychacze wymuszają model aktuatorów o ograniczonej sile, prędkości i mocy. Początkowo może to być prosty kontrakt siła–prędkość–limit mocy. Docelowo można rozwijać pompę, przepływ, zawory, ciśnienie, przecieki, temperaturę i zawór przelewowy.

> duża wymagana siła   → mniejsza dostępna prędkość  duża prędkość ruchu   → mniejsza siła przy ograniczonej mocy


## 7.4. Ładunek jako pełnoprawny stan fizyczny


Materiał w łyżce lub skrzyni wpływa na masę, środek ciężkości, bezwładność, obciążenie osi, stabilność, drogę hamowania i pracę zawieszenia. Podczas wysypywania właściwości masowe zmieniają się stopniowo, a nie skokowo.


## 7.5. Sprzężenie narzędzia z napędem


Opór gruntu musi przechodzić przez mechanizm do źródła energii. Zbyt słaby siłownik, pompa, silnik, rama albo przyczepność powinny ograniczyć pracę. Właściwa geometria narzędzia może być równie ważna jak większa moc.


# CZĘŚĆ VIII. LOTNICTWO, BALONY I RAKIETY



## 8. Maszyny latające pasują do wspólnego ekosystemu


Samoloty, helikoptery, balony i rakiety nie są przypadkowym rozszerzeniem. Zmieniają sposób transportu, eksploracji, budowy i logistyki, a jednocześnie korzystają z tego samego CraftGraphu, energii, masy, sterowania, uszkodzeń i diagnostyki.


## 8.1. Samoloty


Pierwszy model nie wymaga CFD. Semantyczne powierzchnie aerodynamiczne mogą obliczać lokalny przepływ, kąt natarcia, nośność, opór i moment. Projekt skrzydeł, środek masy, moc, podwozie i jakość pasa powinny wspólnie decydować o locie.


## 8.2. Helikoptery


Helikopter jest znacznie trudniejszy i nie powinien rozpoczynać od pełnego modelu łopat. Pierwszy poziom może traktować wirnik jako dysk generujący siłę i moment na podstawie obrotów, skoku, orientacji i lokalnego przepływu. Architektura powinna umożliwić późniejsze zwiększanie wierności.


## 8.3. Balony i sterowce


Wypór jest relatywnie prostym systemem, który tworzy nowe rozwiązania logistyczne: dźwigi powietrzne, platformy obserwacyjne, transport wielkogabarytowy i absurdalne hybrydy. Wymaga objętości gazu, gęstości atmosfery, wiatru, balastu i oporu.


## 8.4. Rakiety i granica skali


Rakiety lokalne, pionowy start, eksperymentalne pojazdy i loty w obrębie mapy pasują do wspólnego sandboxa. Pełna orbita, planety i ogromne odległości wymagają innej skali współrzędnych, propagacji i czasu. Nie mogą być domyślnym wymaganiem pierwszego fundamentu.

> Decyzja odłożona Wizja pozostawia miejsce na rakiety i loty orbitalne, ale pierwszy projekt musi świadomie rozdzielić rakietę jako maszynę lokalną od pełnego symulatora kosmicznego.


## 8.5. Wspólny test systemowy


Samolot transportowy może wymagać pasa, paliwa i części dowożonych ciężarówkami. Helikopter może przenosić elementy mostu. Sterowiec może pełnić rolę dźwigu. Rakieta może wymagać platformy, dźwigów i specjalistycznej drogi. Właśnie te zależności uzasadniają obecność lotnictwa.


# CZĘŚĆ IX. SKALA ŚWIATA, CZASU I SYMULACJI



## 9. Jedna rozdzielczość nie wystarczy


| Domena | Przykładowa wymagana skala |

| --- | --- |

| Precyzyjne mechanizmy | Geometria ciągła i dokładność centymetrowa lub lepsza. |

| Roboty ziemne | Centymetry do dziesiątek centymetrów lokalnie. |

| Drogi i nawierzchnie | Gładka geometria makro plus dokładniejszy profil powierzchni. |

| Krajobraz | Dziesiątki centymetrów do metrów. |

| Transport lądowy | Kilometry aktywnego świata i streaming. |

| Lotnictwo | Dziesiątki kilometrów i większa prędkość przemieszczania. |

| Orbita | Potencjalnie zupełnie inny układ skali i propagacji. |

Świat nie powinien mieć jednej uniwersalnej siatki. Wspólny może być system regionów i współrzędnych, ale poszczególne domeny potrzebują własnej reprezentacji i lokalnej rozdzielczości.


## 9.1. Aktywna symulacja regionalna


> aktywny region wokół gracza   → pełna fizyka, dokładny teren i bieżące operacje  bliskie otoczenie   → pełna geometria, ograniczona aktywność  daleki świat   → streaming i uproszczony stan  bardzo dalekie obiekty   → symulacja dyskretna lub propagacja analityczna

Pełna fizyka wszystkich maszyn na całej mapie jest nierealistyczna. Trzeba zachować stan i przyczynowość, ale obniżać częstotliwość oraz wierność odległych systemów.


## 9.2. Różne skale czasu


| Proces | Charakterystyczna skala |

| --- | --- |

| Kontakt koła i jointy | Milisekundy i częsty krok fizyki. |

| Praca narzędzia | Dziesiąte części sekundy do sekund. |

| Budowa drogi | Minuty lub godziny czasu gry. |

| Chłodzenie asfaltu | Minuty. |

| Osiadanie gruntu | Godziny lub dni. |

| Logistyka poza ekranem | Zdarzenia i uproszczone kroki. |

| Lot orbitalny | Godziny lub dni z możliwością przyspieszania czasu. |

Jeden timestep nie może sterować wszystkimi procesami. Fizyka mechaniczna, materiał, termika, logistyka i procesy długoterminowe powinny mieć osobne harmonogramy aktualizacji oraz jawne punkty synchronizacji.


## 9.3. Aktualizacja collidera pod maszyną


Roboty ziemne zmieniają geometrię bardzo blisko aktywnych kontaktów. Niezbędny jest pipeline z dirty regions, asynchroniczną generacją danych i commitowaniem na bezpiecznej granicy kroku fizyki. Natychmiastowy feedback siłowy narzędzia może być oddzielony od nieco opóźnionej przebudowy collidera.

> operacja narzędzia   → zmiana stanu materiału   → oznaczenie regionu dirty   → generacja render/collision cache   → commit na granicy physics tick   → późniejsze zwolnienie starego cache


# CZĘŚĆ X. DESTRUKCJA I KONWERSJA STATYCZNO-DYNAMICZNA



## 10. Destrukcja nie może przejąć całego projektu


Destruktywny teren i maszyny naturalnie prowadzą do odrywających się fragmentów, zawalania i gruzu. Pełna ciągła destrukcja materiałowa jest jednak osobnym ogromnym projektem. Wizja wymaga kontrolowanej hierarchii.


## 10.1. Poziomy destrukcji


| Poziom | Zakres |

| --- | --- |

| 1. Edycja bez zawalania | Kopanie, dodawanie i usuwanie materiału bez pełnej analizy struktur. |

| 2. Odłączane komponenty | Region bez kotwicy staje się uproszczonym dynamicznym ciałem. |

| 3. Zaawansowane pękanie | Naprężenia, propagacja pęknięć, dalsza fragmentacja i zmęczenie. |

Pierwszy fundament może zatrzymać się na poziomie 1 lub ograniczonej części poziomu 2. Poziom 3 nie jest warunkiem udowodnienia robót ziemnych, budowy dróg ani wspólnego CraftGraphu.


## 10.2. Konwersja statycznego świata w dynamiczny fragment


1. Wykrycie komponentu odłączonego od kotwicy.

2. Usunięcie go ze statycznej reprezentacji regionu.

3. Przebudowa collidera pozostałego świata.

4. Utworzenie uproszczonego dynamicznego compoundu.

5. Obliczenie masy, środka masy i bezwładności.

6. Przeniesienie materiałów i stanu uszkodzeń.

7. Ograniczenie dalszej fragmentacji według poziomu wierności.


## 10.3. Eksplozja jako operacja wielodomenowa


Jedna eksplozja może trafić teren lity, luźny materiał, asfalt, stalową konstrukcję i część maszyny. Nie powinna wykonywać jednego generycznego „removeSphere”. Powinna zostać opisana jako operacja świata, którą każda domena interpretuje zgodnie ze swoją semantyką.


# CZĘŚĆ XI. DIAGNOSTYKA, TESTY I INTERFEJS



## 11. Koszt testowania może przewyższyć koszt implementacji


Liczba kombinacji materiałów, pojazdów, narzędzi, warunków i uszkodzeń rośnie wykładniczo. Projekt wymaga od początku infrastruktury testowej, a nie dopiero po pojawieniu się problemów.

- deterministyczne scenariusze;

- replay i możliwość odtworzenia błędu;

- automatyczne benchmarki;

- porównywanie wyników między wersjami;

- testy invariants: masa, energia, stabilność i brak utraty danych;

- wizualizacja sił, momentów, kontaktów, przepływów i stanu materiału;

- laboratoria dla pojedynczych sprzężeń systemowych.


## 11.1. Każdy etap ma udowodnić sprzężenie


| Słabe osiągnięcie | Właściwy dowód systemowy |

| --- | --- |

| „Dodaliśmy koparkę.” | Opór gruntu przechodzi przez łyżkę i hydraulikę, materiał zwiększa masę i może zostać przeniesiony. |

| „Dodaliśmy drogę.” | Profil, warstwy i zagęszczenie zmieniają zachowanie fizycznego pojazdu. |

| „Dodaliśmy samolot.” | Skrzydła, środek masy, napęd, ładunek i wykonany pas wspólnie decydują o locie. |

| „Dodaliśmy automatykę.” | Sensor i kontroler realnie sterują istniejącym aktuatorom bez specjalnego skryptu dla danego pojazdu. |


## 11.2. Interfejs może być trudniejszy niż fizyka


Gra może technicznie działać, a mimo to być niegrywalna. Użytkownik nie może walczyć z kamerą, łączeniem części, setkami parametrów i monotonną obsługą konwoju. Potrzebne są poziomy interakcji:

- Bezpośredni: sterowanie maszyną i narzędziem.

- Konstrukcyjny: budowa i konfiguracja mechanizmów.

- Planistyczny: projekt drogi, robót i zadań.

- Automatyzacyjny: sensory, kontrolery, trasy i sekwencje.

- Diagnostyczny: pomiary, replay, porównania i wyjaśnienie awarii.


## 11.3. Diagnostyka dla gracza


- siły i momenty na częściach oraz jointach;

- przepływ mocy, paliwa, ciśnienia i sygnałów;

- obciążenie osi, zawieszenia i narzędzi;

- slip, trakcja, zagłębienie i nacisk na grunt;

- bilans masy materiału;

- mapa zagęszczenia, nośności i jakości drogi;

- porównanie projektu z wykonanym rezultatem;

- historia zdarzeń prowadzących do awarii.


# CZĘŚĆ XII. KRYTYCZNE RYZYKA I GRANICE



## 12. Brutalna ocena skali przedsięwzięcia


> Ocena Pełna literalna wersja wizji — wysokiej jakości pojazdy, grunt, hydraulika, aerodynamika, wirniki, rakiety, destrukcja, automatyzacja i wielki świat — jest większa niż wiele niezależnych gier. Jako jednorazowy cel implementacyjny byłaby nieracjonalna. Jako wieloletni program rozwijany przez warstwowe, grywalne eksperymenty pozostaje ekstremalnie ambitna, lecz możliwa do badania.


## 12.1. Największe ryzyka techniczne


| Ryzyko | Dlaczego jest groźne | Podstawowa obrona |

| --- | --- | --- |

| Granice reprezentacji | Błędy block–smooth–road–structure mogą generować nachodzące collidery i niespójny materiał. | Jawna własność powierzchni i kontrakty domen. |

| Aktualizacja świata pod pojazdem | Przeskoki, utrata kontaktów, impulsy i koszt broadphase. | Dirty regions, asynchroniczne cache i bezpieczny commit. |

| Model materiału luźnego | Pełna fizyka granularna jest za droga, zbyt proste heightmapy mogą oszukiwać. | Agregaty i modele 2.5D z zachowaniem masy oraz testami zachowania. |

| Model kontaktu opon i gąsienic | Standardowe tarcie może być najsłabszym ogniwem całego środowiska. | Oddzielne modele siłowe, wieloskalowa nawierzchnia i laboratoria. |

| Eksplozja kombinatoryczna | Każdy nowy materiał i system mnoży liczbę interakcji. | Automatyczne scenariusze, invariants i ograniczanie domen. |

| Skala przestrzenna i czasowa | Koparka, samolot i rakieta wymagają skrajnie różnych rozdzielczości. | Regiony, LOD symulacji i osobne harmonogramy aktualizacji. |

| Destrukcja | Może pochłonąć cały projekt i destabilizować fizykę. | Stopnie wierności i odłożenie ciągłego pękania. |

| Interfejs | Bogaty system może stać się niedostępny i monotonny. | Warstwowe narzędzia, automatyzacja i diagnostyka użytkowa. |


## 12.2. Największe ryzyka produktowe


- Brak jednej obietnicy: technologia zaczyna rozwijać się bez gry i bez mierzalnego doświadczenia.

- Pięć gier naraz: pojazdy, budownictwo, lotnictwo, logistyka i destrukcja powstają jako odrębne minigry.

- Realizm bez czytelności: system jest złożony, ale gracz nie rozumie przyczyn.

- Monotonia pracy: realistyczne roboty ziemne stają się długą, ręczną czynnością bez automatyzacji i celów.

- Fałszywa uniwersalność: jeden monstrualny solver próbuje obsłużyć wszystkie domeny i staje się niestabilny.

- Zbyt wczesny ogromny świat: streaming i LOD przesłaniają najważniejsze sprzężenia mechanik.

- Przypadkowe dziedziczenie: nowy projekt przejmuje techniczny dług i założenia VAW lub Vehicle Lab bez ponownej walidacji.


## 12.3. Ochrona marzenia przez ograniczenie pierwszego produktu


Ograniczanie pierwszego produktu nie powinno usuwać wspólnej przyczynowości. Należy ograniczać liczbę domen, materiałów i skal, ale zachować pełny łańcuch wybranego doświadczenia.

> Mały prototyp ma być pionowym przekrojem ekosystemu, nie poziomą kolekcją niepołączonych funkcji.


## 12.4. Rzeczy, których nie wolno uznać za obowiązkowe na starcie


- pełna topologia 3D terenu z jaskiniami i nawisami;

- ciągła destrukcja wszystkich materiałów;

- pełna hydraulika płynowa;

- indywidualne ziarna materiału sypkiego;

- pełne fizyczne ogniwa każdej gąsienicy;

- CFD i pełny model łopat helikoptera;

- orbital mechanics i planety;

- multiplayer;

- ekonomia, survival i rynek;

- symulacja całego świata w pełnej wierności.


# CZĘŚĆ XIII. KONSTYTUCJA PROJEKTU



## 13. Zasady nienegocjowalne


| Zasada | Interpretacja |

| --- | --- |

| 1. Maszyna i świat są jednym systemem. | Nie wolno projektować pojazdów jako odseparowanej minigry ani świata jako biernej scenografii. |

| 2. Semantyka jest ważniejsza od mesha. | CraftGraph, RoadGraph, materiał i warstwy są źródłem prawdy; meshe są cachem. |

| 3. Materia, energia i ładunek mają bilans. | Uproszczenia są dozwolone, magiczne znikanie nie. |

| 4. Ograniczenia mają znaczenie. | Moc, siła, masa, geometria i przyczepność muszą ograniczać działanie. |

| 5. Wspólna przyczynowość, specjalistyczne modele. | Nie tworzymy jednego solvera dla wszystkiego ani osobnych gier dla każdej maszyny. |

| 6. Każdy etap udowadnia sprzężenie. | Nowa funkcja jest wartościowa dopiero, gdy wpływa na istniejące systemy. |

| 7. Diagnostyka jest częścią gameplayu. | Gracz musi móc zrozumieć i porównać rezultat. |

| 8. Absurdalne konstrukcje są dozwolone. | System ocenia je prawami świata, nie listą dozwolonych klas. |

| 9. Wierność jest skalowana. | Modele mogą być upraszczane, ale ich kontrakt musi pozostać spójny. |

| 10. Zakres pierwszej wersji jest brutalnie ograniczony. | Ograniczamy domeny, nie usuwamy rdzenia wizji. |


## 13.1. Kierunki architektonicznie odrzucone


- wszystko jako jeden typ blokowego voxela;

- wszystko jako SDF;

- pojedynczy voxel jako pojedynczy collider;

- spłaszczenie urządzeń mechanicznych do materiału terenu;

- droga jako wyłącznie tekstura, mesh lub typ materiału;

- jeden uniwersalny solver fizyczny dla gruntu, opon, skrzydeł i hydrauliki;

- osobne, specjalnie oskryptowane klasy Car, Plane, Excavator jako fundament;

- pełna destrukcja jako warunek pierwszej gry;

- duży proceduralny świat przed udowodnieniem lokalnego ekosystemu;

- bezpośrednie sklejenie istniejących repozytoriów bez nowej konstytucji i kontraktów.


## 13.2. Hipotezy silne


- Nowy projekt powinien być osobnym rdzeniem, nawet jeżeli ponownie wykorzysta wiedzę i wybrane moduły VAW oraz Vehicle Lab.

- CraftGraph musi pozostać semantyczny i niezależny od renderera.

- Świat potrzebuje wielu reprezentacji materii, a nie jednego uniwersalnego voxela.

- Maszyny robocze są naturalnym pomostem między budową pojazdów i edytowalnym światem.

- Materiał luźny powinien zachowywać masę i objętość, ale nie wymaga pełnej fizyki granularnej.

- RoadGraph i warstwy nawierzchni są potrzebne, jeżeli budowa dróg ma mieć znaczenie techniczne.

- Box3D może integrować rezultat mechaniczny, ale potrzebuje systemów domenowych.

- Automatyzacja i diagnostyka są centralne dla skalowania gameplayu.

- Pierwsze eksperymenty muszą być lokalne, mierzalne i deterministyczne.


## 13.3. Hipotezy prawdopodobne, ale nieweryfikowane


- Pierwszy wartościowy model robót ziemnych może być 2.5D.

- Fizyczne koła powinny zostać zachowane, a model opony rozszerzony o siły domenowe.

- Gąsienice lepiej zacząć od ciągłego obszaru kontaktowego niż setek ogniw.

- Powierzchnie aerodynamiczne wystarczą do pierwszego modelu samolotu.

- Model dysku wirnika wystarczy do pierwszego modelu helikoptera.

- Statyczne regiony i dynamiczne fragmenty powinny używać różnych reprezentacji kolizyjnych.

- Droga powinna zachować projekt parametryczny, lecz sposób łączenia go z ręczną edycją terenu wymaga eksperymentów.


# CZĘŚĆ XIV. PYTANIA OTWARTE I PROGRAM DALSZEJ ANALIZY



## 14. Pytania o tożsamość pierwszego produktu


1. Jaki jeden scenariusz najlepiej udowadnia wspólny ekosystem maszyna–materiał–infrastruktura?

2. Czy pierwsza gra jest przede wszystkim laboratorium, otwartym sandboxem, kampanią techniczną czy połączeniem tych form?

3. Czy gracz działa jako postać, operator wielu maszyn, swobodna kamera konstruktora czy hybryda?

4. Jaka skala świata jest potrzebna do pierwszego pełnego doświadczenia?

5. Które formy lotnictwa są częścią pierwszego produktu, a które jedynie wymaganiem architektonicznym na przyszłość?


## 14.1. Pytania o CraftGraph


1. Jakie minimalne typy części wystarczą do zbudowania samochodu, koparki, samolotu, helikoptera, balonu i rakiety?

2. Jak rozdzielić część strukturalną, urządzenie, port zasobów, actuator, sensor i powierzchnię siłową?

3. Jak reprezentować połączenia, które jednocześnie przenoszą siły, energię, płyny i sygnały?

4. Jak zmiana ładunku aktualizuje masę i bezwładność bez destabilizacji solvera?

5. Jak zachować dane urządzeń po odłączeniu lub zniszczeniu części?


## 14.2. Pytania o materiał i teren


1. Czy model kolumnowy 2.5D wystarczy do realistycznego spychacza, koparki i równiarki?

2. Jak dokładnie materiał przechodzi ze świata do narzędzia i z powrotem?

3. Jak obliczać opór cięcia i pchania bez pełnego solvera geotechnicznego?

4. Jak modelować relaksację zwału i kąt naturalnego zsypu?

5. Jak mieszać materiały i warstwy bez eksplozji pamięci?

6. Jak często można przebudowywać collider pod aktywną maszyną?

7. Kiedy potrzebny jest pełny SDF 3D, a kiedy wystarczy powierzchnia lub kolumny?


## 14.3. Pytania o drogi i nawierzchnie


1. Czy RoadGraph jest zawsze autorytatywny, czy może zostać „wykonany” i częściowo utracić parametryczność?

2. Jak zachować ręczne poprawki po zmianie projektu drogi?

3. Jak reprezentować warstwy podbudowy i asfaltu?

4. Jak wyznaczyć fizycznego właściciela powierzchni na granicy drogi, terenu i konstrukcji?

5. Jakie metryki jakości drogi są potrzebne do gameplayu?

6. Jak wieloskalowy profil nawierzchni powinien wpływać na fizyczne koło?


## 14.4. Pytania o skalę i fizykę


1. Jaki jest kontrakt między systemami generującymi siły a integratorem rigid-body?

2. Jakie strefy wierności i częstotliwości aktualizacji są potrzebne?

3. Jak przełączać odległe maszyny między pełną i uproszczoną symulacją?

4. Jak zapewnić deterministyczne replay przy asynchronicznym meshingu i streamingu?

5. Jak rozwiązać precyzję współrzędnych dla lotnictwa bez projektowania od razu planet?

6. Jak odseparować lokalne rakiety od przyszłej mechaniki orbitalnej?


## 14.5. Pytania o interfejs i doświadczenie


1. Jak budować złożone mechanizmy bez zmuszania gracza do pracy jak w profesjonalnym CAD?

2. Jak tłumaczyć porażkę konstrukcji bez odbierania satysfakcji z odkrywania?

3. Które czynności powinny być ręczne, a które delegowane automatyzacji?

4. Jak projekt drogi, wykonanie i test pokazywać w jednym spójnym interfejsie?

5. Jak umożliwić absurdalne konstrukcje bez utraty czytelności i wydajności?


## 14.6. Oczekiwany rezultat następnej analizy


- krytyka i ewentualna korekta przedstawionej konstytucji;

- porównanie kilku konkurencyjnych architektur rdzenia;

- minimalny model domen i kontraktów, który nie zamyka przyszłej wizji;

- seria pionowych eksperymentów o jasnych kryteriach sukcesu i porażki;

- mapa ryzyk technicznych z kolejnością ich redukcji;

- propozycja pierwszego realnego produktu, który sam w sobie jest grywalny;

- jawna lista rzeczy świadomie odłożonych;

- plan migracji wiedzy lub kodu z VAW i Vehicle Lab bez mechanicznego łączenia repozytoriów.


# CZĘŚĆ XV. BRIEF DLA GPT‑5.6 SOL



## 15. Rola następnego agenta


GPT‑5.6 Sol powinien działać jako krytyczny architekt systemów, badacz technologii symulacyjnych, projektant sandboxów emergentnych oraz przeciwnik własnych założeń. Jego zadaniem nie jest natychmiastowe potwierdzenie wizji ani stworzenie efektownego roadmapu.


## 15.1. Polecenie nadrzędne


> Podważ tę wizję tak mocno, jak to konieczne, ale nie redukuj jej tylko dlatego, że pełna wersja jest ogromna. Znajdź najmniejszy zestaw wspólnych praw i pionowych eksperymentów, który pozwoli rozwijać marzenie bez oszustwa architektonicznego.


## 15.2. Czego Sol nie powinien robić


- Nie traktować dokumentu jako zatwierdzonej specyfikacji technicznej.

- Nie proponować od razu wieloletniego harmonogramu bez redukcji największych niewiadomych.

- Nie wybierać bibliotek przed zdefiniowaniem kontraktów danych i eksperymentów.

- Nie upraszczać wizji do zwykłego vehicle sandboxa ani voxelowego survivalu.

- Nie obiecywać pełnej geotechniki, lotów orbitalnych lub multiplayera bez osobnego uzasadnienia.

- Nie scalać automatycznie kodu VAW i Vehicle Lab.


## 15.3. Co Sol powinien zakwestionować


- czy CraftGraph może rzeczywiście obsłużyć wszystkie klasy maszyn bez stania się monolitem;

- czy model 2.5D nie zablokuje za wcześnie wizji świata 3D;

- czy Box3D jest odpowiednim mechanicznym rdzeniem dla skali i liczby systemów;

- czy zachowanie masy materiału można osiągnąć bez kosztownej granularności;

- czy RoadGraph i ręcznie wykonana droga mogą współistnieć bez chaosu danych;

- czy automatyzacja może być wspólna dla pojazdów, maszyn i infrastruktury;

- czy pierwsza obietnica gry jest wystarczająco konkretna i grywalna;

- które elementy wizji są architektonicznie kompatybilne, a które tylko tematycznie podobne.


## 15.4. Minimalny standard jakości odpowiedzi


| Wymaganie | Oczekiwane zachowanie |

| --- | --- |

| Krytyczność | Wskazuje sprzeczności, koszty i warunki porażki, nie tylko zalety. |

| Alternatywy | Porównuje co najmniej kilka architektur i modeli danych. |

| Weryfikowalność | Każdą główną hipotezę łączy z eksperymentem lub benchmarkiem. |

| Zakres | Rozdziela wizję docelową, platformę badawczą i pierwszy produkt. |

| Dziedziczenie | Określa, co można przenieść z istniejących projektów, a co należy zaprojektować od nowa. |

| Uczciwość | Jawnie oznacza niewiadome i nie udaje, że nieistniejąca technologia jest rozwiązana. |


# 16. Konkluzja


Rozmowa doprowadziła do wizji znacznie bardziej spójnej niż początkowe połączenie dwóch projektów. Jej rdzeniem nie jest voxelowy teren ani długa lista pojazdów. Rdzeniem jest wspólny ekosystem, w którym maszyny wykonują realną pracę, materia i energia mają konsekwencje, a infrastruktura może być zarówno rozwiązaniem problemu, jak i źródłem nowych ograniczeń.

Samochody, koparki, spychacze, walce, samoloty, helikoptery, balony i rakiety pasują do jednego świata wtedy, gdy nie są osobnymi minigrami. Muszą korzystać ze wspólnego CraftGraphu, wspólnych praw masy i energii, wspólnego sterowania oraz wyspecjalizowanych modeli sił, których wynik integruje jeden mechaniczny rdzeń.

Największa innowacja nie polega na stworzeniu jednej nowej technologii. Polega na połączeniu wielu reprezentacji materii, mechaniki maszyn, procesu wykonywania pracy, pomiaru rezultatu i swobodnych rozwiązań gracza w jeden przyczynowy system.

> Ostateczny wniosek Nie budujemy gry zawierającej wiele pojazdów. Budujemy świat, w którym maszyna jest uniwersalnym sposobem wykonywania pracy — a gracz może projektować zarówno maszynę, jak i warunki, w których ma ona zadziałać.

KONIEC DOKUMENTU WIZJI v0.1

Następny krok: niezależna, głęboka i krytyczna analiza architektury oraz programu eksperymentów.